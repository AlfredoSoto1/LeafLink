#include "Tasks.hpp"
#include <stdio.h>
#include "pico/stdlib.h"

void Tasks::load_config_from_flash(AppContext &ctx) {
  // Implementation for loading config from flash
  printf("Loading config...\n");
}

void Tasks::request_config_from_master(AppContext &ctx) {
  // Implementation for requesting config from master
  printf("Requesting config from master...\n");
}

void Tasks::apply_config_to_sensors(AppContext &ctx) {
  // Implementation for applying config to sensors
  printf("Applying config to sensors...\n");
}

void Tasks::read_sensors(AppContext &ctx) {
  // Implementation for reading sensors
  printf("Reading sensors...\n");

  printf("Reading sensors...\n");

  auto moistureReading = ctx.moisture.read();
  auto uvReading = ctx.uv.read();

  printf("Moisture: raw=%u percent=%.2f needs_water=%s\n",
         moistureReading.raw,
         moistureReading.percent,
         moistureReading.needs_water ? "yes" : "no");

  printf("UV: raw=%u uv_index=%.2f alert=%s\n",
         uvReading.raw,
         uvReading.uv_index,
         uvReading.is_alert ? "yes" : "no");

  // if (moistureReading.needs_water) {
  //   ctx.scheduler->schedule(Tasks::control_pump);
  // }
  // ctx.scheduler->schedule(Tasks::send_plant_status);
}

void Tasks::control_pump(AppContext &ctx) {
  // Implementation for controlling the pump
  printf("Controlling pump...\n");
}

void Tasks::send_plant_status(AppContext &ctx) {
  // Implementation for WiFi communication
  printf("Sending plant status...\n");
}



  // // Task A — check flash for saved config; if missing, fetch it from master.
  // context.scheduler.schedule([&]() {
  //   if (context.config.load()) {
  //     printf("[Config] Loaded from flash.\n");

  //     // Task B (fast path) — apply config immediately.
  //     context.scheduler.schedule([&]() {
  //       context.moisture.set_config(context.config.get());
  //       context.uv.set_config(context.config.get());
  //       context.pump.set_config(context.config.get());
  //       printf("[Config] Applied to sensors.\n");
  //     });

  //     return;
  //   }

  //   // No valid config — initialise Wi-Fi and request it from the master.
  //   printf("[Config] Not found in flash. Connecting to master...\n");

  //   if (!context.wifi.init()) {
  //     printf("[Wifi] Module init failed.\n");
  //     return;
  //   }

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
