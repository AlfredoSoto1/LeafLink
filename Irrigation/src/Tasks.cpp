#include "Tasks.hpp"
#include "TaskScheduler.hpp"
#include "pico/stdlib.h"
#include <cstdlib>
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

static void print_received_config(const StorageController::SystemConfig& config) {
  printf("[Config] Parsed summary:\n");
  printf("  master=%s:%u ssid=%s\n",
         config.wifi_config.master_host,
         config.wifi_config.tcp_port,
         config.wifi_config.ap_ssid);
  printf("  soil_threshold=%.1f dry_cal=%u wet_cal=%u\n",
         config.soil_moisture_config.threshold_percent,
         config.soil_moisture_config.dry_cal,
         config.soil_moisture_config.wet_cal);
  printf("  pump_target=%.2f oz/day flow=%.3f oz/s\n",
         config.pump_config.target_oz_per_day,
         config.pump_config.flow_rate_oz_per_sec);
  printf("  water_capacity=%.1f oz min=%.1f%% uv_alert=%.1f\n",
         config.water_config.tank_capacity_oz,
         config.water_config.tank_min_threshold_percent,
         config.uv_config.alert_threshold);
  printf("  sleep_active=%s interval_ms=%u\n",
         config.sleep_config.active ? "true" : "false",
         config.sleep_config.sleep_interval_ms);
}

static void copy_config_text(char* dst, size_t dst_len, const std::string& value) {
  if (dst_len == 0) return;
  std::strncpy(dst, value.c_str(), dst_len - 1);
  dst[dst_len - 1] = '\0';
}

static bool parse_bool_text(const std::string& value) {
  return value == "1" || value == "true" || value == "TRUE" || value == "yes";
}

static void apply_config_pair(StorageController::SystemConfig& config,
                              const std::string& key,
                              const std::string& value) {
  if (key == "wifi.ap_ssid") {
    copy_config_text(config.wifi_config.ap_ssid, sizeof(config.wifi_config.ap_ssid), value);
  } else if (key == "wifi.ap_password") {
    copy_config_text(config.wifi_config.ap_password, sizeof(config.wifi_config.ap_password), value);
  } else if (key == "wifi.master_host") {
    copy_config_text(config.wifi_config.master_host, sizeof(config.wifi_config.master_host), value);
  } else if (key == "wifi.tcp_port") {
    config.wifi_config.tcp_port = static_cast<uint16_t>(std::strtoul(value.c_str(), nullptr, 10));
  } else if (key == "pump.target_oz_per_day") {
    config.pump_config.target_oz_per_day = std::strtof(value.c_str(), nullptr);
  } else if (key == "pump.flow_rate_oz_per_sec") {
    config.pump_config.flow_rate_oz_per_sec = std::strtof(value.c_str(), nullptr);
  } else if (key == "uv.alert_threshold") {
    config.uv_config.alert_threshold = std::strtof(value.c_str(), nullptr);
  } else if (key == "uv.min_voltage") {
    config.uv_config.min_voltage = std::strtof(value.c_str(), nullptr);
  } else if (key == "uv.max_voltage") {
    config.uv_config.max_voltage = std::strtof(value.c_str(), nullptr);
  } else if (key == "uv.max_uv_index") {
    config.uv_config.max_uv_index = std::strtof(value.c_str(), nullptr);
  } else if (key == "power.warmup_ms") {
    config.power_config.warmup_ms = static_cast<uint>(std::strtoul(value.c_str(), nullptr, 10));
  } else if (key == "power.v_max") {
    config.power_config.v_max = std::strtof(value.c_str(), nullptr);
  } else if (key == "power.v_min") {
    config.power_config.v_min = std::strtof(value.c_str(), nullptr);
  } else if (key == "power.divider_ratio") {
    config.power_config.divider_ratio = std::strtof(value.c_str(), nullptr);
  } else if (key == "water.warmup_ms") {
    config.water_config.warmup_ms = static_cast<uint>(std::strtoul(value.c_str(), nullptr, 10));
  } else if (key == "water.tank_capacity_oz") {
    config.water_config.tank_capacity_oz = std::strtof(value.c_str(), nullptr);
  } else if (key == "water.tank_min_threshold_percent") {
    config.water_config.tank_min_threshold_percent = std::strtof(value.c_str(), nullptr);
  } else if (key == "soil_moisture.warmup_ms") {
    config.soil_moisture_config.warmup_ms = static_cast<uint>(std::strtoul(value.c_str(), nullptr, 10));
  } else if (key == "soil_moisture.threshold_percent") {
    config.soil_moisture_config.threshold_percent = std::strtof(value.c_str(), nullptr);
  } else if (key == "soil_moisture.dry_cal") {
    config.soil_moisture_config.dry_cal = static_cast<uint16_t>(std::strtoul(value.c_str(), nullptr, 10));
  } else if (key == "soil_moisture.wet_cal") {
    config.soil_moisture_config.wet_cal = static_cast<uint16_t>(std::strtoul(value.c_str(), nullptr, 10));
  } else if (key == "sleep.active") {
    config.sleep_config.active = parse_bool_text(value);
  } else if (key == "sleep.sleep_interval_ms") {
    config.sleep_config.sleep_interval_ms = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
  } else {
    printf("[Config] Ignoring unknown config key '%s'.\n", key.c_str());
  }
}

static bool parse_config_text(const std::string& text,
                              StorageController::SystemConfig& config) {
  size_t start = 0;
  size_t parsed = 0;
  bool has_wifi_ssid = false;
  bool has_wifi_password = false;
  bool has_master_host = false;
  bool has_tcp_port = false;
  bool has_soil_threshold = false;
  bool has_sleep_interval = false;

  while (start < text.size()) {
    size_t end = text.find(',', start);
    if (end == std::string::npos) end = text.size();

    std::string pair = text.substr(start, end - start);
    while (!pair.empty() && (pair.back() == '\n' || pair.back() == '\r' || pair.back() == ' ')) {
      pair.pop_back();
    }

    size_t equals = pair.find('=');
    if (equals != std::string::npos && equals > 0) {
      std::string key = pair.substr(0, equals);
      std::string value = pair.substr(equals + 1);
      apply_config_pair(config, key, value);
      if (key == "wifi.ap_ssid") has_wifi_ssid = true;
      if (key == "wifi.ap_password") has_wifi_password = true;
      if (key == "wifi.master_host") has_master_host = true;
      if (key == "wifi.tcp_port") has_tcp_port = true;
      if (key == "soil_moisture.threshold_percent") has_soil_threshold = true;
      if (key == "sleep.sleep_interval_ms") has_sleep_interval = true;
      ++parsed;
    }

    start = end + 1;
  }

  if (!(has_wifi_ssid && has_wifi_password && has_master_host &&
        has_tcp_port && has_soil_threshold && has_sleep_interval)) {
    printf("[Config] Missing required text config keys: ssid=%d pass=%d host=%d port=%d soil=%d sleep=%d\n",
           has_wifi_ssid,
           has_wifi_password,
           has_master_host,
           has_tcp_port,
           has_soil_threshold,
           has_sleep_interval);
    return false;
  }

  return parsed > 0;
}

static const char* wifi_state_text(WifiController::State state) {
  switch (state) {
    case WifiController::State::CONNECTING: return "connecting";
    case WifiController::State::CONNECTED: return "connected";
    case WifiController::State::ERROR: return "error";
    case WifiController::State::DISCONNECTED:
    default: return "disconnected";
  }
}

static std::string states_to_text(const StorageController::SystemStates& state) {
  char buffer[512];
  snprintf(buffer,
           sizeof(buffer),
           "wifi.state=%s,"
           "pump.running=%u,pump.duration_ms=%u,pump.started_at_ms=%u,pump.total_oz_dispensed=%.3f,"
           "uv.uv_index=%.3f,uv.is_alert=%u,uv.error=%u,"
           "power.voltage=%.3f,power.percentage=%.3f,power.warning=%u,power.error=%u,"
           "water.ounces_remaining=%.3f,water.error=%u,"
           "soil_moisture.moisture_percent=%.3f,soil_moisture.is_dry=%u,soil_moisture.error=%u\n",
           wifi_state_text(state.wifi_state),
           state.pump_state.running ? 1 : 0,
           state.pump_state.duration_ms,
           state.pump_state.started_at_ms,
           state.pump_state.total_oz_dispensed,
           state.uv_state.uv_index,
           state.uv_state.is_alert ? 1 : 0,
           state.uv_state.error ? 1 : 0,
           state.power_state.voltage,
           state.power_state.percentage,
           state.power_state.warning ? 1 : 0,
           state.power_state.error ? 1 : 0,
           state.water_state.ounces_remaining,
           state.water_state.error ? 1 : 0,
           state.soil_moisture_state.moisture_percent,
           state.soil_moisture_state.is_dry ? 1 : 0,
           state.soil_moisture_state.error ? 1 : 0);
  return std::string(buffer);
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

  ctx.scheduler->schedule(Tasks::wakeup_os);
  return;

  // 
  ctx.storage.state = StorageController::State::NO_DATA;

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
      ctx.scheduler->schedule(Tasks::configure_timer);
      ctx.scheduler->schedule(Tasks::wakeup_os);
      break;

    case StorageController::State::NO_DATA:
      printf("[Boot] No config in flash. Requesting from master.\n");
      ctx.scheduler->schedule(Tasks::request_config_from_master);
      break;

    case StorageController::State::ERROR:
    default:
      ctx.report.set_error("Flash read error: cannot load configuration.");
      ctx.scheduler->schedule(Tasks::finish);
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

  printf("[System] Entering sleep mode. Waking in %u ms.\n", ctx.storage.flash.config.sleep_config.sleep_interval_ms);
  do {
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
  // ctx.scheduler->schedule(Tasks::transmit_report);
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

  printf("[Config] Waiting for text config payload from master (120 s)...\n");
  std::string payload = ctx.wifi.receive_config_payload(120000, 0);

  if (payload.empty()) {
    printf("[Config] No config received within timeout. Halting configuration.\n");
    ctx.wifi.send_config_result(false);
    ctx.scheduler->schedule(Tasks::finish);
    return;
  }

  printf("[Config] Received text config (%u bytes):\n", static_cast<unsigned>(payload.size()));
  printf("----- CONFIG BEGIN -----\n%s----- CONFIG END -----\n", payload.c_str());

  StorageController::SystemConfig next_config = {};
  if (!parse_config_text(payload, next_config)) {
    printf("[Config] Could not parse text configuration. Halting.\n");
    ctx.wifi.send_config_result(false);
    ctx.scheduler->schedule(Tasks::finish);
    return;
  }

  ctx.storage.flash.config = next_config;
  ctx.storage.flash.magic = StorageController::MAGIC;
  print_received_config(ctx.storage.flash.config);

  // Apply the received config to all live modules.
  ctx.wifi.config     = ctx.storage.flash.config.wifi_config;
  ctx.pump.config     = ctx.storage.flash.config.pump_config;
  ctx.uv.config       = ctx.storage.flash.config.uv_config;
  ctx.power.config    = ctx.storage.flash.config.power_config;
  ctx.water.config    = ctx.storage.flash.config.water_config;
  ctx.moisture.config = ctx.storage.flash.config.soil_moisture_config;

  ctx.storage.save();
  printf("[Config] Config received, applied, and saved.\n");
  ctx.wifi.send_config_result(true);
  printf("[Config] Waiting briefly for master AP to return...\n");
  sleep_ms(2000);

  ctx.scheduler->schedule(Tasks::configure_timer);
  ctx.scheduler->schedule(Tasks::wakeup_os);
}

void Tasks::read_power(AppContext &ctx) {
  ctx.sensor.acquire(&ctx.power.sensor);
  ctx.sensor.start();
  ctx.sensor.read_raw();
  ctx.sensor.release();
  ctx.power.sinthesize();

  // if (ctx.power.state.error) {
  //   ctx.report.set_error("Power sensor read failed (voltage critically low or disconnected).");
  //   ctx.scheduler->schedule(Tasks::transmit_report);
  //   ctx.scheduler->schedule(Tasks::finish);
  //   return;
  // }
  
  // if (ctx.power.state.warning) {
  //   ctx.report.add_warning("Low battery warning.");
  //   printf("[Sensors] Low battery warning: %.2f V (%.1f%%)\n",
  //     ctx.power.state.voltage, ctx.power.state.percentage);
  //   ctx.scheduler->schedule(Tasks::transmit_report);
  //   ctx.scheduler->schedule(Tasks::finish);
  //   return;
  // }
    
  printf("[Sensors] Power: %.2f V %.1f%%\n",
    ctx.power.state.voltage, ctx.power.state.percentage);
    
  // ctx.scheduler->schedule(Tasks::read_moisture);
  ctx.scheduler->schedule(Tasks::read_water_level);
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
    ctx.scheduler->schedule(Tasks::finish);
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
    ctx.scheduler->schedule(Tasks::finish);
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

  // if (ctx.water.state.error) {
  //   ctx.report.set_error("Water level sensor read failed (disconnected or malfunctioning).");
  //   ctx.scheduler->schedule(Tasks::transmit_report);
  //   ctx.scheduler->schedule(Tasks::finish);
  //   return;
  // }

  printf("[Sensors] Water:    raw=%u  %.1f oz remaining\n",
         ctx.water.sensor.last_value,
         ctx.water.state.ounces_remaining);

  // ctx.scheduler->schedule(Tasks::check_plant_conditions);
  ctx.scheduler->schedule(Tasks::read_power);
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
    ctx.scheduler->schedule(Tasks::finish);
    return;
  }

  printf("[Plant] Soil dry (%.1f%%). Checking water supply...\n",
         ctx.moisture.state.moisture_percent);
  
  float tank_threshold_oz = ctx.water.config.tank_capacity_oz * (ctx.water.config.tank_min_threshold_percent / 100.0f);

  if (ctx.water.state.ounces_remaining <= tank_threshold_oz) {
    ctx.report.add_warning("Plant needs water but the tank is below threshold.");
    ctx.scheduler->schedule(Tasks::save_states);
    ctx.scheduler->schedule(Tasks::transmit_report);
    ctx.scheduler->schedule(Tasks::finish);
    return;
  }

  if (ctx.pump.config.flow_rate_oz_per_sec <= 0.0f) {
    ctx.report.add_warning("Plant needs water but pump flow rate is not configured.");
    ctx.scheduler->schedule(Tasks::save_states);
    ctx.scheduler->schedule(Tasks::transmit_report);
    ctx.scheduler->schedule(Tasks::finish);
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
    ctx.scheduler->schedule(Tasks::finish);
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
    ctx.scheduler->schedule(Tasks::finish);
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

  ctx.scheduler->schedule(Tasks::finish);
}

// ---------------------------------------------------------------------------
// save_states — persist the current module states to flash
// ---------------------------------------------------------------------------
void Tasks::save_states(AppContext &ctx) {
  ctx.storage.flash.state.wifi_state          = ctx.wifi.state;
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
  ctx.storage.flash.state.wifi_state           = ctx.wifi.state;
  ctx.storage.flash.state.soil_moisture_state  = ctx.moisture.state;
  ctx.storage.flash.state.uv_state             = ctx.uv.state;
  ctx.storage.flash.state.water_state          = ctx.water.state;
  ctx.storage.flash.state.power_state          = ctx.power.state;
  ctx.storage.flash.state.pump_state           = ctx.pump.state;

  std::string packet = states_to_text(ctx.storage.flash.state);
  if (ctx.report.has_fatal_error) {
    packet.insert(packet.size() - 1,
                  std::string(",report.status=error,report.message=") +
                  (ctx.report.fatal_error_msg ? ctx.report.fatal_error_msg : ""));
  } else {
    packet.insert(packet.size() - 1, ",report.status=ok");
  }

  printf("[Report] States text ready (%u bytes).\n", static_cast<unsigned>(packet.size()));
  ctx.wifi.enter_state_report_mode();
  ctx.wifi.send_states_payload(packet);
}

