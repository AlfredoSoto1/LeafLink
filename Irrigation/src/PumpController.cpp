#include "PumpController.hpp"

#include "hardware/gpio.h"
#include "pico/platform.h"

void PumpController::init() {
  gpio_init(power_pin);
  gpio_set_dir(power_pin, GPIO_OUT);
  gpio_put(power_pin, 0);

  state.running = false;
  state.total_oz_dispensed = 0.0f;
  state.duration_ms = 0;
  state.started_at_ms = 0;
}

void PumpController::start(float ounces) {
  if (state.running || config.flow_rate_oz_per_sec <= 0.0f || config.target_oz_per_day <= 0.0f) {
    return;
  }

  // Calculate duration based on flow rate and desired ounces
  state.duration_ms = static_cast<uint32_t>((ounces / config.flow_rate_oz_per_sec) * 1000.0f);
  state.started_at_ms = to_ms_since_boot(get_absolute_time());
  state.running = true;
  gpio_put(power_pin, 1);
}

void PumpController::stop() {
  if (!state.running) {
    return;
  }

  gpio_put(power_pin, 0);
  state.running = false;

  // Update total ounces dispensed
  uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - state.started_at_ms;
  float oz_dispensed = (elapsed_ms / 1000.0f) * config.flow_rate_oz_per_sec;
  state.total_oz_dispensed += oz_dispensed;
}

void PumpController::update() {
  if (!state.running) {
    return;
  }

  uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - state.started_at_ms;
  if (elapsed_ms >= state.duration_ms) {
    stop();
  }
}
