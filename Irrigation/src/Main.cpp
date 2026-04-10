#include "TaskScheduler.hpp"
#include "SoilMoistureSensor.hpp"
#include "UVSensor.hpp"
#include "Pump.hpp"
#include "Wifi.hpp"
#include "SystemConfig.hpp"
#include "hardware/uart.h"
#include <cstdio>

int main() {
  stdio_init_all();
  sleep_ms(2000);

  // -------------------------------------------------------------------------
  // 1 — Create sensors, pump, wifi, and config manager
  // -------------------------------------------------------------------------
  SoilMoistureSensor moisture(16, 500, 30.0f);
  UVSensor           uv(16, 100, 6.0f);
  Pump               pump;
  WifiModule         wifi(uart0);
  ConfigManager      config;

  // -------------------------------------------------------------------------
  // 2 — Power on, calibrate, and initialize sensors
  // -------------------------------------------------------------------------
  moisture.calibrate(3000, 1500);
  moisture.init();

  uv.init();
  pump.init();

  // -------------------------------------------------------------------------
  // 3 — Schedule task chain
  // -------------------------------------------------------------------------
  TaskScheduler scheduler;

  // Task A — check flash for saved config; if missing, fetch it from master.
  scheduler.schedule([&]() {
    if (config.load()) {
      printf("[Config] Loaded from flash.\n");

      // Task B (fast path) — apply config immediately.
      scheduler.schedule([&]() {
        moisture.set_config(config.get());
        uv.set_config(config.get());
        pump.set_config(config.get());
        printf("[Config] Applied to sensors.\n");
      });

      return;
    }

    // No valid config — initialise Wi-Fi and request it from the master.
    printf("[Config] Not found in flash. Connecting to master...\n");

    if (!wifi.init()) {
      printf("[Wifi] Module init failed.\n");
      return;
    }

    wifi.power_on();

    if (!wifi.connect(Defaults::WIFI_SSID, Defaults::WIFI_PASSWORD)) {
      printf("[Wifi] Connect failed.\n");
      return;
    }

    SystemConfig received = {};
    while (!wifi.request_config(Defaults::MASTER_HOST, Defaults::MASTER_PORT, received)) {
      printf("[Config] Waiting for config from master...\n");
      sleep_ms(2000);
    }

    printf("[Config] Received from master.\n");

    // Task B (slow path) — persist the received config to flash.
    scheduler.schedule([&, received]() {
      if (!config.save(received)) {
        printf("[Config] Flash write failed!\n");
        return;
      }

      printf("[Config] Saved to flash.\n");

      // Task C — apply config to all sensors.
      scheduler.schedule([&]() {
        moisture.set_config(config.get());
        uv.set_config(config.get());
        pump.set_config(config.get());
        printf("[Config] Applied to sensors.\n");
      });
    });
  });

  // -------------------------------------------------------------------------
  // Main loop — drain the task queue
  // -------------------------------------------------------------------------
  while (!scheduler.empty()) {
    scheduler.execute();
  }

  return 0;
}
