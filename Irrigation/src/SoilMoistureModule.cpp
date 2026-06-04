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

  // Only flag an error if raw==0 but wet_cal is above 0, meaning 0 is not a
  // valid wet-soil reading for this calibration.
  state.error = (sensor.last_value == 0 && config.wet_cal > 0);
}

float SoilMoistureModule::raw_to_percent(uint16_t raw) const {
  if (raw >= config.dry_cal) return 0.0f;
  if (raw <= config.wet_cal) return 100.0f;

  return ((static_cast<float>(config.dry_cal) - raw) /
          (config.dry_cal - config.wet_cal)) * 100.0f;
}
