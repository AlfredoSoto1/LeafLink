#include "Tasks.hpp"
#include "TaskScheduler.hpp"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>

// Maximum watering cycles per wakeup to prevent infinite loops if the
// soil moisture sensor never reads as satisfied.
static constexpr int MAX_PUMP_CYCLES = 3;

// ---------------------------------------------------------------------------
// Timer callback — fires every 60 seconds on the timer IRQ
// Sets a flag so the main loop knows to wake and drain the queue
// ---------------------------------------------------------------------------
static volatile bool g_timer_fired = false;

static bool timer_callback(repeating_timer_t *rt) {
  g_timer_fired = true;
  return true;
}

// ---------------------------------------------------------------------------
// boot_os — initialize all hardware, load config, choose startup path
// ---------------------------------------------------------------------------
void Tasks::boot_os(AppContext &ctx) {
  // Initialize all controllers
  ctx.storage.init();
  ctx.sensor.init();
  ctx.wifi.init();
  ctx.pump.init();
  printf("[Boot] All controllers initialized.\n");
  
  // Initialize all modules
  ctx.uv.init();
  ctx.power.init();
  ctx.water.init();
  ctx.moisture.init();
  printf("[Boot] All modules initialized.\n");

  switch (ctx.storage.state) {
    case StorageController::State::OK:
      // Load configs from flash into the respective modules.
      ctx.wifi.config     = ctx.storage.flash.config.wifi_config;
      ctx.pump.config     = ctx.storage.flash.config.pump_config;
      ctx.uv.config       = ctx.storage.flash.config.uv_config;
      ctx.power.config    = ctx.storage.flash.config.power_config;
      ctx.water.config    = ctx.storage.flash.config.water_config;
      ctx.moisture.config = ctx.storage.flash.config.soil_moisture_config;
      printf("[Boot] Config loaded from flash.\n");
      ctx.scheduler->schedule(Tasks::wakeup_os);
      break;

    case StorageController::State::NO_DATA:
      printf("[Boot] No config in flash. Requesting from master.\n");
      ctx.scheduler->schedule(Tasks::request_config_from_master);
      ctx.scheduler->schedule(Tasks::configure_timer);
      ctx.scheduler->schedule(Tasks::wakeup_os);
      break;

    case StorageController::State::ERROR:
    default:
      ctx.report.set_error("Flash read error: cannot load configuration.");
      ctx.scheduler->schedule(Tasks::transmit_report);
      break;
  }
}

void Tasks::finish(AppContext &ctx) {
  ctx.scheduler->schedule(Tasks::wakeup_os);

  if (ctx.report.has_fatal_error) {
    ctx.scheduler->clear();
    panic("[Fatal] %s", ctx.report.fatal_error_msg);
    return;
  }

  // On a clean finish with no fatal errors, the system can enter sleep mode if configured.
  if (!ctx.storage.flash.config.sleep_config.active) {
    printf("[System] Sleep mode disabled. Staying awake.\n");
    return;
  }

  do {
    printf("[System] Entering sleep mode.\n");
    __wfi();
    
    // Interrupt was triggered by timer
    if (g_timer_fired) {
      g_timer_fired = false;
      return;
    }

    // Interrupt was triggered by pairing button
    // Force restart to pairing mode on next loop iteration
    if (WifiController::pairing_requested) {
      WifiController::pairing_requested = false;
      Tasks::restart(ctx);
      return;
    }
  } while (true);
}

void Tasks::wakeup_os(AppContext &ctx) {
  ctx.report.clear();
  ctx.scheduler->schedule(Tasks::read_power);
}

void Tasks::configure_timer(AppContext &ctx) {
  auto& sleep_config = ctx.storage.flash.config.sleep_config;

  if (sleep_config.active) {
    cancel_repeating_timer(&sleep_config.timer);
    sleep_config.active = false;
  }

  // Add a repeating timer to wake up the system every sleep_interval_ms milliseconds.
  sleep_config.active = add_repeating_timer_ms(
    -static_cast<int64_t>(sleep_config.sleep_interval_ms), 
    timer_callback, 
    nullptr, 
    &sleep_config.timer);

  if (!sleep_config.active) {
    ctx.report.set_error("Failed to configure sleep timer.");
    ctx.scheduler->schedule(Tasks::finish);
    return;
  }

  printf("[Config] Sleep interrupt initialized with interval %u ms.\n", sleep_config.sleep_interval_ms);
}

void Tasks::restart(AppContext &ctx) {
  printf("[System] Restarting OS and clearing config...\n");
  ctx.storage.flash = {};
  ctx.storage.state = StorageController::State::NO_DATA;
  ctx.storage.save();
  ctx.scheduler->clear();
  ctx.report.clear();
  ctx.scheduler->schedule(Tasks::boot_os);
}

void Tasks::request_config_from_master(AppContext &ctx) {
  printf("[Config] Entering WiFi pairing mode...\n");
  ctx.wifi.enter_pairing_mode();

  printf("[Config] Waiting for config payload from master (60 s)...\n");
  std::string payload = ctx.wifi.receive_config_payload(60000);

  if (payload.empty()) {
    printf("[Config] No config received within timeout. Halting configuration.\n");
    ctx.scheduler->schedule(Tasks::finish);
    return;
  }

  if (payload.size() != sizeof(StorageController::SystemConfig)) {
    printf("[Config] Invalid config size (%u bytes, expected %u). Halting.\n",
           static_cast<unsigned>(payload.size()),
           static_cast<unsigned>(sizeof(StorageController::SystemConfig)));
    ctx.scheduler->schedule(Tasks::finish);
    return;
  }

  // Cast the raw bytes directly into the flash config record.
  memcpy(&ctx.storage.flash.config, payload.data(), sizeof(StorageController::SystemConfig));
  ctx.storage.flash.magic = StorageController::MAGIC;

  // Apply the received config to all live modules.
  ctx.wifi.config     = ctx.storage.flash.config.wifi_config;
  ctx.pump.config     = ctx.storage.flash.config.pump_config;
  ctx.uv.config       = ctx.storage.flash.config.uv_config;
  ctx.power.config    = ctx.storage.flash.config.power_config;
  ctx.water.config    = ctx.storage.flash.config.water_config;
  ctx.moisture.config = ctx.storage.flash.config.soil_moisture_config;

  ctx.storage.save();
  printf("[Config] Config received, applied, and saved.\n");
}

void Tasks::read_power(AppContext &ctx) {
  ctx.sensor.acquire(&ctx.power.sensor);
  ctx.sensor.start();
  ctx.sensor.read_raw();
  ctx.sensor.release();
  ctx.power.sinthesize();

  if (ctx.power.state.error) {
    ctx.report.set_error("Power sensor read failed (voltage critically low or disconnected).");
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }
  
  if (ctx.power.state.warning) {
    ctx.report.add_warning("Low battery warning.");
    printf("[Sensors] Low battery warning: %.2f V (%.1f%%)\n",
      ctx.power.state.voltage, ctx.power.state.percentage);
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }
    
  printf("[Sensors] Power: %.2f V %.1f%%\n",
    ctx.power.state.voltage, ctx.power.state.percentage);
    
  ctx.scheduler->schedule(Tasks::read_moisture);
}

// ---------------------------------------------------------------------------
// read_moisture — read soil moisture; halt on sensor error
// ---------------------------------------------------------------------------
void Tasks::read_moisture(AppContext &ctx) {
  ctx.sensor.acquire(&ctx.moisture.sensor);
  ctx.sensor.start();
  ctx.sensor.read_raw();
  ctx.sensor.release();
  ctx.moisture.sinthesize();

  if (ctx.moisture.state.error) {
    ctx.report.set_error("Moisture sensor read failed (disconnected or malfunctioning).");
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }

  printf("[Sensors] Moisture: raw=%u  %.1f%%  dry=%s\n",
         ctx.moisture.sensor.last_value,
         ctx.moisture.state.moisture_percent,
         ctx.moisture.state.is_dry ? "YES" : "NO");

  ctx.scheduler->schedule(Tasks::read_uv);
}

// ---------------------------------------------------------------------------
// read_uv — read UV sensor; warn on alert, halt on sensor error
// ---------------------------------------------------------------------------
void Tasks::read_uv(AppContext &ctx) {
  ctx.sensor.acquire(&ctx.uv.sensor);
  ctx.sensor.start();
  ctx.sensor.read_raw();
  ctx.sensor.release();
  ctx.uv.sinthesize();

  if (ctx.uv.state.error) {
    ctx.report.set_error("UV sensor read failed (disconnected or malfunctioning).");
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }

  if (ctx.uv.state.is_alert) {
    ctx.report.add_warning("High UV index alert.");
  }

  printf("[Sensors] UV:       raw=%u  index=%.2f  alert=%s\n",
         ctx.uv.sensor.last_value,
         ctx.uv.state.uv_index,
         ctx.uv.state.is_alert ? "YES" : "NO");

  ctx.scheduler->schedule(Tasks::read_water_level);
}

// ---------------------------------------------------------------------------
// read_water_level — read tank level; halt on sensor error
// ---------------------------------------------------------------------------
void Tasks::read_water_level(AppContext &ctx) {
  ctx.sensor.acquire(&ctx.water.sensor);
  ctx.sensor.start();
  ctx.sensor.read_raw();
  ctx.sensor.release();
  ctx.water.sinthesize();

  if (ctx.water.state.error) {
    ctx.report.set_error("Water level sensor read failed (disconnected or malfunctioning).");
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }

  printf("[Sensors] Water:    raw=%u  %.1f oz remaining\n",
         ctx.water.sensor.last_value,
         ctx.water.state.ounces_remaining);

  ctx.scheduler->schedule(Tasks::check_plant_conditions);
}

// ---------------------------------------------------------------------------
// check_plant_conditions — decide whether watering is needed and feasible
// ---------------------------------------------------------------------------
void Tasks::check_plant_conditions(AppContext &ctx) {
  if (!ctx.moisture.state.is_dry) {
    printf("[Plant] Moisture OK (%.1f%%). No watering needed.\n",
           ctx.moisture.state.moisture_percent);
    ctx.scheduler->schedule(Tasks::save_states);
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }

  printf("[Plant] Soil dry (%.1f%%). Checking water supply...\n",
         ctx.moisture.state.moisture_percent);
  
  float tank_threshold_oz = ctx.water.config.tank_capacity_oz * (ctx.water.config.tank_min_threshold_percent / 100.0f);

  if (ctx.water.state.ounces_remaining <= tank_threshold_oz) {
    ctx.report.add_warning("Plant needs water but the tank is below threshold.");
    ctx.scheduler->schedule(Tasks::save_states);
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }

  if (ctx.pump.config.flow_rate_oz_per_sec <= 0.0f) {
    ctx.report.add_warning("Plant needs water but pump flow rate is not configured.");
    ctx.scheduler->schedule(Tasks::save_states);
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }

  ctx.scheduler->schedule(Tasks::control_pump);
}

// ---------------------------------------------------------------------------
// control_pump — dispense one dose of water, then re-check moisture.
// Cycles up to MAX_PUMP_CYCLES times before giving up.
// ---------------------------------------------------------------------------
void Tasks::control_pump(AppContext &ctx) {
  static int pump_cycles = 0;

  if (pump_cycles >= MAX_PUMP_CYCLES) {
    pump_cycles = 0;
    ctx.report.add_warning("Max pump cycles reached; moisture target not satisfied.");
    ctx.scheduler->schedule(Tasks::save_states);
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }

  // Dispense up to one daily target dose, capped by available water.
  float oz_to_dispense = ctx.pump.config.target_oz_per_day;
  if (oz_to_dispense > ctx.water.state.ounces_remaining) {
    oz_to_dispense = ctx.water.state.ounces_remaining;
  }

  printf("[Pump] Cycle %d/%d: dispensing %.2f oz...\n",
         pump_cycles + 1, MAX_PUMP_CYCLES, oz_to_dispense);

  ctx.pump.start(oz_to_dispense);

  // Block until the pump finishes its timed run.
  while (ctx.pump.state.running) {
    ctx.pump.update();
    sleep_ms(100);
  }

  printf("[Pump] Done. Total dispensed: %.2f oz\n",
         ctx.pump.state.total_oz_dispensed);

  // Re-read moisture to decide whether another cycle is needed.
  ctx.sensor.acquire(&ctx.moisture.sensor);
  ctx.sensor.start();
  ctx.sensor.read_raw();
  ctx.sensor.release();
  ctx.moisture.sinthesize();

  if (ctx.moisture.state.error) {
    pump_cycles = 0;
    ctx.report.set_error("Moisture sensor failed after pump cycle.");
    ctx.scheduler->schedule(Tasks::transmit_report);
    return;
  }

  // Re-read water level to get an updated remaining volume.
  ctx.sensor.acquire(&ctx.water.sensor);
  ctx.sensor.start();
  ctx.sensor.read_raw();
  ctx.sensor.release();
  ctx.water.sinthesize();

  // Calculate the tank threshold in ounces for the warning check.
  float tank_threshold_oz = ctx.water.config.tank_capacity_oz * (ctx.water.config.tank_min_threshold_percent / 100.0f);

  if (ctx.moisture.state.is_dry && ctx.water.state.ounces_remaining > tank_threshold_oz) {
    pump_cycles++;
    printf("[Pump] Moisture still low (%.1f%%). Scheduling next cycle.\n",
           ctx.moisture.state.moisture_percent);
    ctx.scheduler->schedule(Tasks::control_pump);
  } else {
    pump_cycles = 0;
    printf("[Pump] Moisture satisfied (%.1f%%) or tank now below threshold (%.2f oz).\n",
           ctx.moisture.state.moisture_percent, ctx.water.state.ounces_remaining);
    ctx.scheduler->schedule(Tasks::save_states);
    ctx.scheduler->schedule(Tasks::transmit_report);
  }
}

// ---------------------------------------------------------------------------
// save_states — persist the current module states to flash
// ---------------------------------------------------------------------------
void Tasks::save_states(AppContext &ctx) {
  ctx.storage.flash.state.soil_moisture_state = ctx.moisture.state;
  ctx.storage.flash.state.uv_state            = ctx.uv.state;
  ctx.storage.flash.state.water_state         = ctx.water.state;
  ctx.storage.flash.state.power_state         = ctx.power.state;
  ctx.storage.flash.state.pump_state          = ctx.pump.state;
  ctx.storage.save();
  printf("[Storage] States saved to flash.\n");
}

// ---------------------------------------------------------------------------
// transmit_report — build a key=value payload and send it to master over WiFi.
// On a fatal error the device halts after transmission so it does not silently
// continue with a broken configuration.
// ---------------------------------------------------------------------------
void Tasks::transmit_report(AppContext &ctx) {
  // Text header: status, optional error message, and any warnings.
  char header[256];
  int  offset = 0;

  if (ctx.report.has_fatal_error) {
    offset += snprintf(header + offset, sizeof(header) - offset,
                       "STATUS=ERROR\nMSG=%s\n",
                       ctx.report.fatal_error_msg);
  } else {
    offset += snprintf(header + offset, sizeof(header) - offset, "STATUS=OK\n");
    for (int i = 0; i < ctx.report.warning_count; ++i) {
      offset += snprintf(header + offset, sizeof(header) - offset,
                         "WARN=%s\n", ctx.report.warnings[i]);
    }
  }

  // Build the final packet: text header followed by the raw SystemStates struct.
  std::string packet(header, offset);
  packet.append(reinterpret_cast<const char*>(&ctx.storage.flash.state),
                sizeof(StorageController::SystemStates));

  printf("[Report] Header:\n%.*s\n", offset, header);

  if (ctx.wifi.connect_to_master()) {
    ctx.wifi.send_states_payload(packet);
    printf("[Report] Transmitted to master.\n");
  } else {
    printf("[Report] WiFi connection failed. Could not reach master.\n");
  }

  ctx.scheduler->schedule(Tasks::finish);
}

