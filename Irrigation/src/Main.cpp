#include "Tasks.hpp"
#include "TaskScheduler.hpp"
#include "AppContext.hpp"
#include "hardware/uart.h"
#include <cstdio>

int main() {
  stdio_init_all();
  sleep_ms(2000);

  // -------------------------------------------------------------------------
  // 1 — Construct the application context with all components
  // -------------------------------------------------------------------------
  TaskScheduler scheduler;

  AppContext context = {
    .moisture = SoilMoistureSensor(16, 500, 30.0f),
    .uv       = UVSensor(16, 100, 6.0f),
    .pump     = Pump(),
    .wifi     = WifiModule(uart0),
    .config   = ConfigManager(),
    .scheduler = &scheduler
  };

  // -------------------------------------------------------------------------
  // 2 — Calibrate, and initialize sensors
  // -------------------------------------------------------------------------
  context.uv.init();
  context.pump.init();
  context.moisture.init();
  context.moisture.calibrate(3000, 1500);

  // -------------------------------------------------------------------------
  // 3 — Schedule task chain
  // -------------------------------------------------------------------------

  // Initial tasks to read config and sensors immediately on boot.
  context.scheduler->schedule(Tasks::load_config_from_flash);
  context.scheduler->schedule(Tasks::read_sensors);

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

  // -------------------------------------------------------------------------
  // Main loop — drain the task queue
  // -------------------------------------------------------------------------
  while (!context.scheduler->empty()) {
    auto task = context.scheduler->pop();
    if (task != nullptr) {
      task(context);
    }
    sleep_ms(1000);
  }

  return 0;
}
