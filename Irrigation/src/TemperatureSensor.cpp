#include "TemperatureSensor.hpp"

TemperatureSensor::TemperatureSensor(uint sample_count)
    : m_sample_count(sample_count) {}

void TemperatureSensor::init() {
  m_initialized = true;
}

TemperatureSensor::Reading TemperatureSensor::read(ADCController &adc) {
  ensure_initialized();

  uint32_t sum = 0;
  for (uint i = 0; i < m_sample_count; ++i) {
    auto result = adc.read_temperature_raw();
    if (!result.valid) {
      return Reading{ .error = true };
    }

    sum += result.value;
  }

  const uint16_t raw = static_cast<uint16_t>(sum / m_sample_count);
  const float voltage = raw_to_voltage(raw);
  const float celsius = voltage_to_celsius(voltage);

  return Reading{ .raw = raw, .voltage = voltage, .celsius = celsius };
}

float TemperatureSensor::raw_to_voltage(uint16_t raw) const {
  return (static_cast<float>(raw) / 4095.0f) * 3.3f;
}

float TemperatureSensor::voltage_to_celsius(float voltage) const {
  return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void TemperatureSensor::ensure_initialized() const {
  if (!m_initialized) {
    panic("TemperatureSensor used before init()");
  }
}