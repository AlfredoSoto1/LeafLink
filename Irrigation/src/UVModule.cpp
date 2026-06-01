#include "UVModule.hpp"

void UVModule::init() {
  // Initialize the sensor pin
  gpio_init(sensor.power_pin);
  gpio_set_dir(sensor.power_pin, GPIO_OUT);
  gpio_put(sensor.power_pin, 0); // Start with sensor powered off
}

void UVModule::sinthesize() {
  // From the current sensor's last_value, calculate the UV 
  // index and alert state.
  state.uv_index = raw_to_uv_index(sensor.last_value);
  state.is_alert = state.uv_index >= config.alert_threshold;

  // If the raw value is 0, it's likely a sensor error 
  // (disconnected or malfunctioning)
  state.error = (sensor.last_value == 0);
}

float UVModule::raw_to_uv_index(uint16_t raw) const {
  const float voltage = (static_cast<float>(raw) / 4095.0f) * 3.3f;

  if (voltage <= config.min_uv_index) return 0.0f;
  if (voltage >= config.max_uv_index) return config.max_uv_index;

  return ((voltage - config.min_uv_index) / (config.max_uv_index - config.min_uv_index)) * config.max_uv_index;
}
