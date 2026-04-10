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
    .moisture  = SoilMoistureSensor(16, 500, 30.0f),
    .uv        = UVSensor(16, 100, 6.0f),
    .water     = WaterLevelSensor(8, 100, 128.0f),  // 128 oz = 1 gallon default
    .pump      = Pump(),
    .wifi      = WifiModule(uart0),
    .power     = PowerModule(8, 100, 0.5f, 3.0f, 4.2f),
    .config    = ConfigManager(),
    .scheduler = &scheduler
  };

  // -------------------------------------------------------------------------
  // 2 — Calibrate and initialize all hardware
  // -------------------------------------------------------------------------
  context.moisture.calibrate(3000, 1500);
  context.moisture.init();

  context.uv.init();

  context.water.calibrate(0, 3500);
  context.water.init();

  context.pump.init();
  context.power.init();

  // -------------------------------------------------------------------------
  // 3 — Schedule startup task chain
  // -------------------------------------------------------------------------
  context.scheduler->schedule(Tasks::load_config_from_flash);
  context.scheduler->schedule(Tasks::read_sensors);
  context.scheduler->schedule(Tasks::read_power);

  // -------------------------------------------------------------------------
  // Main loop — drain the task queue, then reschedule sensor reads
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
      context.scheduler->schedule(Tasks::read_sensors);
      context.scheduler->schedule(Tasks::read_power);
    }

    gpio_put(LED_PIN, 1);
    sleep_ms(500);
    gpio_put(LED_PIN, 0);
    sleep_ms(500);
  }

  return 0;
}

