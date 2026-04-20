#include "PumpController.hpp"

#include "hardware/gpio.h"
#include "pico/platform.h"

void PumpController::init() {
  gpio_init(POWER_PIN);
  gpio_set_dir(POWER_PIN, GPIO_OUT);
  gpio_put(POWER_PIN, 0);

  m_running     = false;
  m_initialized = true;
}

void PumpController::power_on() {
  ensure_initialized();
  gpio_put(POWER_PIN, 1);
  m_running = true;
}

void PumpController::power_off() {
  ensure_initialized();
  gpio_put(POWER_PIN, 0);
  m_running = false;
}

void PumpController::run() {
  power_on();
  sleep_ms(m_default_duration_ms);
  power_off();
}

void PumpController::set_config(const SystemConfig &cfg) {
  m_default_duration_ms = cfg.pump_run_duration_ms;
}

bool PumpController::is_running() const {
  return m_running;
}

void PumpController::ensure_initialized() const {
  if (!m_initialized) {
    panic("Pump used before init()");
  }
}
