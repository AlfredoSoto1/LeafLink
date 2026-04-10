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
  ctx.moisture.power_on();
  auto moisture = ctx.moisture.read();
  ctx.moisture.power_off();

  ctx.uv.power_on();
  auto uv = ctx.uv.read();
  ctx.uv.power_off();

  ctx.water.power_on();
  auto water = ctx.water.read();
  ctx.water.power_off();

  printf("[Moisture] raw=%u  percent=%.2f%%  needs_water=%s\n",
         moisture.raw, moisture.percent,
         moisture.needs_water ? "yes" : "no");

  printf("[UV]       raw=%u  uv_index=%.2f  alert=%s\n",
         uv.raw, uv.uv_index,
         uv.is_alert ? "yes" : "no");

  printf("[Water]    raw=%u  level=%.1f%%  ounces=%.1f oz\n",
         water.raw, water.percent, water.ounces_remaining);

  if (moisture.needs_water) {
    ctx.scheduler->schedule(Tasks::control_pump);
  }
}

void Tasks::read_power(AppContext &ctx) {
  ctx.power.power_on();
  auto pwr = ctx.power.read();
  ctx.power.power_off();

  printf("[Power]    raw=%u  voltage=%.2fV  battery=%.1f%%\n",
         pwr.raw, pwr.voltage, pwr.percent);
}

void Tasks::control_pump(AppContext &ctx) {
  printf("[Pump] Running for configured duration.\n");
  ctx.pump.run_for();
}

void Tasks::send_plant_status(AppContext &ctx) {
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
