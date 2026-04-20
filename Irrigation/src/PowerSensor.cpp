#include "PowerSensor.hpp"

#include <stdio.h>

PowerSensor::PowerSensor(uint sample_count, uint32_t warmup_ms,
                         float divider_ratio, float v_min, float v_max)
    : m_sample_count(sample_count),
      m_warmup_ms(warmup_ms),
      m_divider_ratio(divider_ratio),
      m_v_min(v_min),
      m_v_max(v_max) {}

void PowerSensor::init() {
  m_initialized = true;
}

PowerSensor::Reading PowerSensor::read(ADCController &adc) {
  ensure_initialized();
  adc.enable_only(ADC_SELECT, m_warmup_ms);

  uint16_t sum = 0;
  for (uint i = 0; i < m_sample_count; ++i) {
    auto result = adc.read_raw();
    if (result.valid) {
      sum += result.value;
    } else {
      adc.disable_all();
      return Reading{ .error = true };
    }
  }

  adc.disable_all();

  const uint16_t raw     = static_cast<uint16_t>(sum / m_sample_count);
  const float    voltage = raw_to_voltage(raw);

  float percent = 0.0f;
  if (voltage <= m_v_min) {
    percent = 0.0f;
  } else if (voltage >= m_v_max) {
    percent = 100.0f;
  } else {
    percent = ((voltage - m_v_min) / (m_v_max - m_v_min)) * 100.0f;
  }

  m_last_raw     = raw;
  m_last_voltage = voltage;
  m_last_percent = percent;

  return Reading{ .raw = raw, .voltage = voltage, .percent = percent };
}

void PowerSensor::set_config(const SystemConfig &cfg) {
  m_divider_ratio = cfg.power_divider_ratio;
  m_v_min         = cfg.power_v_min;
  m_v_max         = cfg.power_v_max;
  m_sample_count  = cfg.power_sample_count;
}

uint16_t PowerSensor::get_raw() const { 
  return m_last_raw;  
}

float PowerSensor::get_voltage() const { 
  return m_last_voltage; 
}

float PowerSensor::get_percent() const { 
  return m_last_percent; 
}

float PowerSensor::raw_to_voltage(uint16_t raw) const {
  const float v_adc = (static_cast<float>(raw) / 4095.0f) * 3.3f;
  return v_adc / m_divider_ratio;
}

void PowerSensor::ensure_initialized() const {
  if (!m_initialized) {
    panic("PowerModule used before init()");
  }
}

