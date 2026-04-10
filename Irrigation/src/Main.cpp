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

  // -------------------------------------------------------------------------
  // Main loop — drain the task queue
  // -------------------------------------------------------------------------
  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  while (true) {
    if (!context.scheduler->empty()) {
      auto task = context.scheduler->pop();
      if (task != nullptr) {
        task(context);
      }
    } else {
      context.scheduler->schedule(Tasks::load_config_from_flash);
      context.scheduler->schedule(Tasks::read_sensors);
    }
    gpio_put(LED_PIN, 1);  // LED ON
    sleep_ms(500);
  
    gpio_put(LED_PIN, 0);  // LED OFF
    sleep_ms(500);
  }

  return 0;
}
