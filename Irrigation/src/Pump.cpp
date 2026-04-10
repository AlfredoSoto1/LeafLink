#include "Pump.hpp"

#include "hardware/gpio.h"
#include "pico/platform.h"

void Pump::init() {
  gpio_init(POWER_PIN);
  gpio_set_dir(POWER_PIN, GPIO_OUT);
  gpio_put(POWER_PIN, 0);

  m_running     = false;
  m_initialized = true;
}

void Pump::on() {
  ensure_initialized();
  gpio_put(POWER_PIN, 1);
  m_running = true;
}

void Pump::off() {
  ensure_initialized();
  gpio_put(POWER_PIN, 0);
  m_running = false;
}

void Pump::run_for(uint32_t duration_ms) {
  on();
  sleep_ms(duration_ms);
  off();
}

void Pump::run_for() {
  run_for(m_default_duration_ms);
}

void Pump::set_config(const SystemConfig &cfg) {
  m_default_duration_ms = cfg.pump_run_duration_ms;
}

bool Pump::is_running() const {
  return m_running;
}

void Pump::ensure_initialized() const {
  if (!m_initialized) {
    panic("Pump used before init()");
  }
}
