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
  return true;
}

static void toggle_led() {
  gpio_put(PICO_DEFAULT_LED_PIN, 1);
  sleep_ms(500);
  gpio_put(PICO_DEFAULT_LED_PIN, 0);
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
    .temperature = TemperatureSensor(8),
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
  context.temperature.init();
  context.power.init();
  // context.pump.init();

  // -------------------------------------------------------------------------
  // 3 — Schedule startup task chain
  // -------------------------------------------------------------------------
  // context.scheduler->schedule(Tasks::load_config_from_flash);
  // context.scheduler->schedule(Tasks::read_sensors);
  context.scheduler->schedule(Tasks::read_power);

  // -------------------------------------------------------------------------
  // 4 — Set up LED and repeating 60-second timer
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
    // Deep sleep: CPU halts, only wakes on interrupt (timer IRQ, etc.)
    __wfi();

    // go back to sleep if the timer wasn't the reason we woke up (spurious wake, or other IRQ)
    if (!g_timer_fired) {
      continue;
    }
    g_timer_fired = false;

    // Drain the entire task queue before returning to sleep
    while (!context.scheduler->empty()) {
      auto task = context.scheduler->pop();
      if (task != nullptr) {
        task(context);
        toggle_led();
      }
    }

    // After processing all tasks, schedule the next sensor read for the next cycle
    // context.scheduler->schedule(Tasks::read_sensors);
    context.scheduler->schedule(Tasks::read_power);
  }

  return 0;
}

