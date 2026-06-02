#include "Tasks.hpp"
#include "TaskScheduler.hpp"
#include "AppContext.hpp"
#include <cstdio>

// ---------------------------------------------------------------------------
// Timer callback — fires every 60 seconds on the timer IRQ
// Sets a flag so the main loop knows to wake and drain the queue
// ---------------------------------------------------------------------------
static volatile bool g_timer_fired = false;
static volatile bool g_pair_button_pressed = false;

static bool timer_callback(repeating_timer_t *rt) {
  g_timer_fired = true;
  return true;
}

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

  repeating_timer_t timer;
  // add_repeating_timer_ms(-60000, timer_callback, nullptr, &timer);
  add_repeating_timer_ms(-10000, timer_callback, nullptr, &timer);

  // -------------------------------------------------------------------------
  // Main loop — sleep until the timer fires, then drain the task queue
  // -------------------------------------------------------------------------
  while (true) {
    // // Deep sleep: CPU halts, only wakes on interrupt (timer IRQ, GPIO IRQ, etc.)
    // __wfi();

    // // go back to sleep if the timer wasn't the reason we woke up (spurious wake, or other IRQ)
    // if (!g_timer_fired && !WifiController::pairing_requested) {
    //   continue;
    // }
    // g_timer_fired = false;

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

    // If the pairing button was pressed, restart the system and clear config.
    if (WifiController::pairing_requested) {
      WifiController::pairing_requested = false;
      Tasks::restart(context);
    } else {
      // After processing all tasks, schedule the next sensor read cycle
      // context.scheduler->schedule(Tasks::wakeup_os);
    }

    if (context.scheduler->empty()) {
      printf("[Main] Task queue empty. Waiting for next timer tick or pairing button press...\n");
      sleep_ms(1000);
    }
  }
  return 0;
}
