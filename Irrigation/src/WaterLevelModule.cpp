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

  // raw == 0 is a valid empty-tank reading; check_plant_conditions handles
  // the low-water case through the threshold comparison.
  state.error = false;
}

float WaterLevelModule::raw_to_percent(uint16_t raw) const {
  return raw * 100.0f / 4095.0f;
}
