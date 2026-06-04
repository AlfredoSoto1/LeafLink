#include "Tasks.hpp"
#include "TaskScheduler.hpp"
#include "AppContext.hpp"
#include <cstdio>

static void toggle_inboard_led() {
  gpio_put(PICO_DEFAULT_LED_PIN, 1);
  sleep_ms(500);
  gpio_put(PICO_DEFAULT_LED_PIN, 0);
}

int main() {
  stdio_init_all();
  sleep_ms(5000);

  // -------------------------------------------------------------------------
  // 1 — Construct the application context with all components
  // -------------------------------------------------------------------------
  TaskScheduler scheduler;

  AppContext context = {
    .sensor    = SensorController(),
    .scheduler = &scheduler
  };

  // -------------------------------------------------------------------------
  // 2 — Schedule the boot task to initialize everything and kick off the first cycle
  // -------------------------------------------------------------------------
  context.scheduler->schedule(Tasks::boot_os);

  // -------------------------------------------------------------------------
  // 3 — Set up LED and repeating 60-second timer
  // -------------------------------------------------------------------------
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  // -------------------------------------------------------------------------
  // Main loop — sleep until the timer fires, then drain the task queue
  // -------------------------------------------------------------------------
  do {
    // Process all tasks in the queue. Tasks can schedule more tasks,
    // so keep popping until it's empty.
    while (!context.scheduler->empty()) {
      // If the pairing button was pressed, break out of task processing to restart the system.
      if (WifiController::pairing_requested) {
        break;
      }
      auto task = context.scheduler->pop();
      if (task != nullptr) {
        task(context);
        toggle_inboard_led();
      }
    }

    if (context.scheduler->empty()) {
      printf("[Main] Task queue empty. Waiting for next timer tick or pairing button press...\n");
      sleep_ms(1000);
    }
  } while (true);

  return 0;
}
