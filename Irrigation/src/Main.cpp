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

  // ADC channels define the enable/power GPIO for each sensor slot. 
  // For this the ADC input pin is shared.
  const ADCEnableChannel adc_enable_channels[] = { 
    PowerSensor::POWER_PIN,         // ADC_SELECT = 0
    SoilMoistureSensor::POWER_PIN,  // ADC_SELECT = 1
    WaterLevelSensor::POWER_PIN,    // ADC_SELECT = 3
    UVSensor::POWER_PIN,            // ADC_SELECT = 2
  };

  AppContext context = {
    .moisture  = SoilMoistureSensor(16, 500, 30.0f),
    .uv        = UVSensor(16, 500, 6.0f),
    .water     = WaterLevelSensor(8, 500, 128.0f),  // 128 oz = 1 gallon default
    .pump      = PumpController(),
    .wifi      = WifiModule(uart0),
    // .power     = PowerSensor(8, 500, 0.5f, 3.0f, 4.2f),
    .power     = PowerSensor(8, 500, 0.5f, 0.0f, 3.3f),
    .config    = ConfigManager(),
    .adc       = ADCController(adc_enable_channels, 4),
    .scheduler = &scheduler
  };

  // -------------------------------------------------------------------------
  // 2 — Calibrate and initialize all hardware
  // -------------------------------------------------------------------------
  context.adc.init();
  
  context.moisture.calibrate(3000, 1500);
  context.moisture.init();
  context.uv.init();
  context.water.calibrate(0, 3500);
  context.water.init();
  context.power.init();
  // context.pump.init();

  // -------------------------------------------------------------------------
  // 3 — Schedule startup task chain
  // -------------------------------------------------------------------------
  // context.scheduler->schedule(Tasks::load_config_from_flash);
  context.scheduler->schedule(Tasks::read_sensors);
  // context.scheduler->schedule(Tasks::read_power);

  // -------------------------------------------------------------------------
  // 4 — Set up LED and repeating 60-second timer
  // -------------------------------------------------------------------------
  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  // repeating_timer_t timer;
  // Negative value = period between END of callback and next call.
  // Use -60000 ms so the timer fires every 60 seconds regardless of
  // callback duration. Swap to 60000 if you want wall-clock alignment.
  // add_repeating_timer_ms(-60000, timer_callback, nullptr, &timer);

  // -------------------------------------------------------------------------
  // Main loop — sleep until the timer fires, then drain the task queue
  // -------------------------------------------------------------------------
  while (true) {
    // Deep sleep: CPU halts, only wakes on interrupt (timer IRQ, etc.)
    // __wfi();

    // go back to sleep if the timer wasn't the reason we woke up (spurious wake, or other IRQ)
    // if (!g_timer_fired) {
    //   continue;
    // }
    // g_timer_fired = false;

    // Drain the entire task queue before returning to sleep
    while (!context.scheduler->empty()) {
      auto task = context.scheduler->pop();
      if (task != nullptr) {
        gpio_put(LED_PIN, 1);
        task(context);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
      }
    }

    // If the queue is empty, we can go back to sleep immediately. 
    // If not, we'll process remaining tasks on the next timer tick.
    // if (context.scheduler->empty()) {
    //   context.scheduler->schedule(Tasks::read_sensors);
    //   context.scheduler->schedule(Tasks::read_power);
    //   printf("[Main] All tasks complete. Going back to sleep...\n");
    // } else {
    //   printf("[Main] Tasks still pending. Will process on next timer tick.\n");
    // }
  }

  return 0;
}

