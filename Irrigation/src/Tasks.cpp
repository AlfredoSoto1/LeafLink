#include "Tasks.hpp"
#include "TaskScheduler.hpp"
#include <stdio.h>
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

void Tasks::read_sensors(AppContext &ctx) {
  // auto moisture = ctx.moisture.read(ctx.adc);
  // if (moisture.error) {
  //   printf("[Sensors] Moisture sensor read error!\n");
  //   ctx.scheduler->schedule(Tasks::read_sensors);
  //   return;
  // }

  // auto uv = ctx.uv.read(ctx.adc);
  // if (uv.error) {
  //   printf("[Sensors] UV sensor read error!\n");
  //   ctx.scheduler->schedule(Tasks::read_sensors);
  //   return;
  // }

  auto water = ctx.water.read(ctx.adc);
  if (water.error) {
    printf("[Sensors] Water sensor read error!\n");
    ctx.scheduler->schedule(Tasks::read_sensors);
    return;
  }

  auto power = ctx.power.read(ctx.adc);
  if (power.error) {
    printf("[Sensors] Power sensor read error!\n");
    ctx.scheduler->schedule(Tasks::read_sensors);
    return;
  }

  // printf("[Sensors] Moisture: raw=%u  percent=%.1f%%  needs_water=%s\n",
  //        moisture.raw, moisture.percent, moisture.needs_water ? "YES" : "NO");
  // printf("[Sensors] UV:       raw=%u  uv_index=%.2f  alert=%s\n",
  //        uv.raw, uv.uv_index, uv.is_alert ? "YES" : "NO");
  printf("[Sensors] Water:    raw=%u  percent=%.1f%%  oz_remaining=%.1f\n",
         water.raw, water.percent, water.ounces_remaining);
  printf("[Sensors] Power:    raw=%u  ratio=%.2f  voltage=%.2fV  battery=%.1f%%\n",
         power.raw, (static_cast<float>(power.raw) / 4095.0f) * 3.3f, power.voltage, power.percent);

  ctx.scheduler->schedule(Tasks::read_sensors);
}

void Tasks::read_power(AppContext &ctx) {
  auto power = ctx.power.read(ctx.adc);
  if (power.error) {
    printf("[Sensors] Power sensor read error!\n");
    ctx.scheduler->schedule(Tasks::notify_error);
    return;
  }

  printf("[Sensors] Power:    raw=%u  ratio=%.2f  voltage=%.2fV  battery=%.1f%%\n",
         power.raw, (static_cast<float>(power.raw) / 4095.0f) * 3.3f, power.voltage, power.percent);
}

/**
 * This task checks the current sensor readings and determines 
 * if the plant needs watering. If the soil moisture is below the 
 * configured threshold, it schedules the pump control task.
 */
void Tasks::check_plant_conditions(AppContext &ctx) {
  // Read current sensor values
  auto moisture = ctx.moisture.read(ctx.adc);
  if (moisture.error) {
    printf("[Sensors] Moisture sensor read error!\n");
    ctx.scheduler->schedule(Tasks::notify_error);
    return;
  }
  
  // Check if watering is needed based on soil moisture
  if (moisture.needs_water) {
    printf("[Plant] Soil moisture low (%.1f%%). Scheduling pump control task.\n", moisture.percent);
    ctx.scheduler->schedule(Tasks::control_pump);
  }

  // Check UV sensor for alerts
  auto uv = ctx.uv.read(ctx.adc);
  if (uv.error) {
    printf("[Sensors] UV sensor read error!\n");
    ctx.scheduler->schedule(Tasks::notify_error);
    return;
  }

  // If UV index is above alert threshold, schedule a notification task
  if (uv.is_alert) {
    printf("[Plant] UV index high (%.2f). Scheduling status update task.\n", uv.uv_index);
    ctx.scheduler->schedule(Tasks::notify_status);
  }
}

void Tasks::control_pump(AppContext &ctx) {
  printf("[Pump] Running for configured duration.\n");
  ctx.pump.run_for();
}

void Tasks::notify_error(AppContext &ctx) {
}

void Tasks::notify_status(AppContext &ctx) {
  printf("[Wifi] Sending plant status...\n");
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
