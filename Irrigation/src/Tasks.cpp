#include "Tasks.hpp"
#include "TaskScheduler.hpp"
#include "pico/stdlib.h"

void Tasks::load_config_from_flash(AppContext &ctx) {
  if (ctx.config.load()) {
    printf("[Config] Loaded from flash.\n");
    ctx.scheduler->schedule(Tasks::apply_config_to_sensors);
  } else {
    printf("[Config] Not found in flash. Requesting from master...\n");
    ctx.scheduler->schedule(Tasks::request_config_from_master);
  }
}

void Tasks::request_config_from_master(AppContext &ctx) {
  if (!ctx.wifi.init()) {
    printf("[Wifi] Module init failed.\n");
    return;
  }

  ctx.wifi.power_on();

  if (!ctx.wifi.connect(Defaults::WIFI_SSID, Defaults::WIFI_PASSWORD)) {
    printf("[Wifi] Connect failed.\n");
    return;
  }

  SystemConfig received = {};
  while (!ctx.wifi.request_config(Defaults::MASTER_HOST, Defaults::MASTER_PORT, received)) {
    printf("[Config] Waiting for config from master...\n");
    sleep_ms(2000);
  }

  printf("[Config] Received from master.\n");

  if (ctx.config.save(received)) {
    printf("[Config] Saved to flash.\n");
  } else {
    printf("[Config] Flash write failed!\n");
  }

  ctx.scheduler->schedule(Tasks::apply_config_to_sensors);
}

void Tasks::apply_config_to_sensors(AppContext &ctx) {
  const SystemConfig &cfg = ctx.config.get();
  ctx.moisture.set_config(cfg);
  ctx.uv.set_config(cfg);
  ctx.water.set_config(cfg);
  ctx.pump.set_config(cfg);
  ctx.power.set_config(cfg);
  printf("[Config] Applied to all sensors.\n");
}

void Tasks::read_power(AppContext &ctx) {
  auto power = ctx.power.read(ctx.adc);
  if (power.error) {
    ctx.plant_status.write_message(ErrorType::SensorReadFailed, "Error: Power sensor read failed.");
    ctx.scheduler->schedule(Tasks::notify_error);
    return;
  }

  // Update the shared plant status with the latest power readings
  PlantStatus::StatusData& status = ctx.plant_status.write_status();
  status.power_voltage = power.voltage;
  status.power_percent = power.percent;
  
  printf("[Sensors] Power:    raw=%u  ratio=%.2f  voltage=%.2fV  battery=%.1f%%\n",
         power.raw, (static_cast<float>(power.raw) / 4095.0f) * 3.3f, power.voltage, power.percent);
}

/**
 * This task reads all sensors and updates the shared plant status. It also checks
 * for any alert conditions (like low moisture or high UV) and schedules further tasks as needed.
 */
void Tasks::read_sensors(AppContext &ctx) {
  PlantStatus::StatusData& status = ctx.plant_status.write_status();

  auto moisture = ctx.moisture.read(ctx.adc);
  if (moisture.error) {
    ctx.plant_status.write_message(ErrorType::SensorReadFailed, "Moisture sensor read failed.");
    ctx.scheduler->schedule(Tasks::notify_error);
    return;
  }

  auto uv = ctx.uv.read(ctx.adc);
  if (uv.error) {
    ctx.plant_status.write_message(ErrorType::SensorReadFailed, "UV sensor read failed.");
    ctx.scheduler->schedule(Tasks::notify_error);
    return;
  }

  auto water = ctx.water.read(ctx.adc);
  if (water.error) {
    ctx.plant_status.write_message(ErrorType::SensorReadFailed, "Water level sensor read failed.");
    ctx.scheduler->schedule(Tasks::notify_error);
    return;
  }

  // Update the shared plant status with the latest sensor readings
  status.sampled_at_ms = to_ms_since_boot(get_absolute_time());
  status.moisture_percent = moisture.percent;
  status.moisture_needs_water = moisture.needs_water;
  status.uv_index = uv.uv_index;
  status.uv_alert = uv.is_alert;
  status.water_percent = water.percent;
  status.water_ounces_remaining = water.ounces_remaining;

  printf("[Sensors] Moisture: raw=%u  percent=%.1f%%\n", moisture.raw, moisture.percent);
  printf("[Sensors] UV:       raw=%u  index=%.2f  alert=%s\n", uv.raw, uv.uv_index, uv.is_alert ? "YES" : "NO");
  printf("[Sensors] Water:    raw=%u  percent=%.1f%%  ounces remaining=%.1f\n", water.raw, water.percent, water.ounces_remaining);

  // After reading sensors, check plant conditions to determine if watering is needed
  ctx.scheduler->schedule(Tasks::check_plant_conditions);
}

/**
 * This task checks the current sensor readings and determines 
 * if the plant needs watering. If the soil moisture is below the 
 * configured threshold, it schedules the pump control task.
 */
void Tasks::check_plant_conditions(AppContext &ctx) {
  PlantStatus::StatusData& status = ctx.plant_status.write_status();
  
  // Check if watering is needed based on soil moisture
  if (status.moisture_needs_water) {
    printf("[Plant] Soil moisture low (%.1f%%). Scheduling pump control task.\n", status.moisture_percent);
    ctx.scheduler->schedule(Tasks::control_pump);
  }

  // If UV index is above alert threshold, schedule a notification task
  if (status.uv_alert) {
    printf("[Plant] UV index high (%.2f). Scheduling status update task.\n", status.uv_index);
    ctx.scheduler->schedule(Tasks::notify_status);
  }
}

void Tasks::control_pump(AppContext &ctx) {
  printf("[Pump] Running for configured duration.\n");
  ctx.pump.run_for();
}

void Tasks::notify_error(AppContext &ctx) {
  // Read the latest message from plant status and send it over WiFi or log it
  const PlantStatus::MessageData &message = ctx.plant_status.message();
  printf("[Notification] Error: %s\n", message.text);

  // Clear the message after notifying
  ctx.plant_status.clear(); 
}

void Tasks::notify_status(AppContext &ctx) {
  // Read the latest status from plant status and send it over WiFi or log it
  const PlantStatus::StatusData &status = ctx.plant_status.status();
  printf("[Notification] Status update: Moisture=%.1f%%, UV Index=%.2f, Water Remaining=%.1f oz\n",
         status.moisture_percent, status.uv_index, status.water_ounces_remaining);

  // Clear the status after notifying
  ctx.plant_status.clear();
}

  //   context.wifi.power_on();

  //   if (!context.wifi.connect(Defaults::WIFI_SSID, Defaults::WIFI_PASSWORD)) {
  //     printf("[Wifi] Connect failed.\n");
  //     return;
  //   }

  //   SystemConfig received = {};
  //   while (!context.wifi.request_config(Defaults::MASTER_HOST, Defaults::MASTER_PORT, received)) {
  //     printf("[Config] Waiting for config from master...\n");
  //     sleep_ms(2000);
  //   }

  //   printf("[Config] Received from master.\n");

  //   // Task B (slow path) — persist the received config to flash.
  //   context.scheduler.schedule([&, received]() {
  //     if (!context.config.save(received)) {
  //       printf("[Config] Flash write failed!\n");
  //       return;
  //     }

  //     printf("[Config] Saved to flash.\n");

  //     // Task C — apply config to all sensors.
  //     context.scheduler.schedule([&]() {
  //       context.moisture.set_config(context.config.get());
  //       context.uv.set_config(context.config.get());
  //       context.pump.set_config(context.config.get());
  //       printf("[Config] Applied to sensors.\n");
  //     });
  //   });
  // });
