#include "Tasks.hpp"
#include "TaskScheduler.hpp"
#include "AppContext.hpp"
#include "hardware/uart.h"
#include <cstdio>

// ---------------------------------------------------------------------------
// Timer callback — fires every 60 seconds on the timer IRQ
// Sets a flag so the main loop knows to wake and drain the queue
// ---------------------------------------------------------------------------
static volatile bool g_timer_fired = false;

static bool timer_callback(repeating_timer_t *rt) {
    g_timer_fired = true;
    return true; // keep repeating
}

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
  // 4 — Set up LED and repeating 60-second timer
  // -------------------------------------------------------------------------
  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  repeating_timer_t timer;
  // Negative value = period between END of callback and next call.
  // Use -60000 ms so the timer fires every 60 seconds regardless of
  // callback duration. Swap to 60000 if you want wall-clock alignment.
  add_repeating_timer_ms(-60000, timer_callback, nullptr, &timer);

  // -------------------------------------------------------------------------
  // Main loop — sleep until the timer fires, then drain the task queue
  // -------------------------------------------------------------------------
  while (true) {
    // Deep sleep: CPU halts, only wakes on interrupt (timer IRQ, etc.)
    __wfi();

    // go back to sleep if the timer wasn't the reason we woke up (spurious wake, or other IRQ)
    if (!g_timer_fired) {
      continue;
    }
    g_timer_fired = false;

    // Reschedule recurring sensor reads each minute
    context.scheduler->schedule(Tasks::read_sensors);
    context.scheduler->schedule(Tasks::read_power);

    // Drain the entire task queue before returning to sleep
    while (!context.scheduler->empty()) {
      auto task = context.scheduler->pop();
      if (task != nullptr) {
        gpio_put(LED_PIN, 1);
        task(context);
        gpio_put(LED_PIN, 0);
      }
    }
  }

  return 0;
}

