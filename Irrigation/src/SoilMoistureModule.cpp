#include "SoilMoistureModule.hpp"

void SoilMoistureModule::init() {
  // Initialize the sensor's power pin
  gpio_init(sensor.power_pin);
  gpio_set_dir(sensor.power_pin, GPIO_OUT);
  gpio_put(sensor.power_pin, 0);
}

void SoilMoistureModule::sinthesize() {
  // From the current sensor's last_value, calculate the moisture percentage
  // and dry/wet state.
  state.moisture_percent = raw_to_percent(sensor.last_value);
  state.is_dry = state.moisture_percent < config.threshold_percent;

  // If the raw value is 0, it's likely a sensor error 
  // (disconnected or malfunctioning)
  state.error = (sensor.last_value == 0);
}

float SoilMoistureModule::raw_to_percent(uint16_t raw) const {
  if (raw >= config.dry_cal) return 0.0f;
  if (raw <= config.wet_cal) return 100.0f;

  return ((static_cast<float>(raw) - config.wet_cal) / 
          (config.dry_cal - config.wet_cal)) * 100.0f;
}
