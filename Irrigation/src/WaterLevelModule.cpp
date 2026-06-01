#include "WaterLevelModule.hpp"

void WaterLevelModule::init() {
  // Initialize the sensor pin
  gpio_init(sensor.power_pin);
  gpio_set_dir(sensor.power_pin, GPIO_OUT);
  gpio_put(sensor.power_pin, 0); // Start with sensor powered off
}

void WaterLevelModule::sinthesize() {
  // From the current sensor's last_value, calculate the water level
  // percentage and ounces remaining.
  state.ounces_remaining = raw_to_percent(sensor.last_value) * config.tank_capacity_oz / 100.0f;

  // If the raw value is 0, it's likely a sensor error 
  // (disconnected or malfunctioning)
  state.error = (sensor.last_value == 0);
}

float WaterLevelModule::raw_to_percent(uint16_t raw) const {
  return raw * 100.0f / 4095.0f;
}
