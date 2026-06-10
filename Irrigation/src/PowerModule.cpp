#include "PowerModule.hpp"

#include <stdio.h>

void PowerModule::init() {
  // Initialize the sensor's power pin
  gpio_init(sensor.power_pin);
  gpio_set_dir(sensor.power_pin, GPIO_OUT);
  gpio_put(sensor.power_pin, 0);
}

void PowerModule::sinthesize() {
  state.voltage = raw_to_voltage(sensor.last_value);

  // Calculate percentage based on v_min/v_max and clamp to 0-100 %
  if (state.voltage <= 3.0f) {
    state.percentage = 0.0f;
  } else if (state.voltage >= 5.0f) {
    state.percentage = 100.0f;
  } else {
    state.percentage = ((state.voltage - 3.0f) / (5.0f - 3.0f)) * 100.0f;
  }

  // Set warning if percentage is below 20 %, and error if below 5 %
  state.warning = (state.percentage <= 20.0f) ? true : false;
  state.error = (state.percentage <= 5.0f) ? true : false;
}

float PowerModule::raw_to_voltage(uint16_t raw) const {
  // Convert raw ADC value to voltage based on the voltage divider ratio
  float v_out = (raw / 4095.0f) * 3.3f; 
  return v_out / config.divider_ratio;
}
