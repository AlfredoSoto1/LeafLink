#include "SoilMoistureSensor.hpp"

SoilMoistureSensor::SoilMoistureSensor(uint sample_count, uint32_t warmup_ms, float threshold_percent)
    : m_sample_count(sample_count),
      m_warmup_ms(warmup_ms),
      m_threshold_percent(threshold_percent),
      m_dryCal(3000),
      m_wetCal(1500),
      m_lastRaw(0),
      m_lastPercent(0.0f),
      m_lastNeedsWater(true),
      m_initialized(false) {}

void SoilMoistureSensor::init() {
  m_initialized = true;
}

void SoilMoistureSensor::calibrate(uint16_t dry_val, uint16_t wet_val) {
  // Keep calibration internally consistent:
  // dry should be the larger ADC value, wet the smaller one.
  if (dry_val >= wet_val) {
    m_dryCal = dry_val;
    m_wetCal = wet_val;
  } else {
    m_dryCal = wet_val;
    m_wetCal = dry_val;
  }
}

SoilMoistureSensor::Reading SoilMoistureSensor::read(ADCController &adc) {
  ensure_initialized();

  uint32_t sum = 0;
  for (uint i = 0; i < m_sample_count; ++i) {
    auto result = adc.read_raw(ADC_SELECT, m_warmup_ms);
    if (result.valid) {
      sum += result.value;
    } else {
      // If any sample is invalid, return an error reading (0 % moisture, needs water)
      return Reading{ .error = true };        
    }
  }

  // Since SAMPLE_COUNT = 16, divide efficiently with a shift.
  const uint16_t raw = static_cast<uint16_t>(sum >> 4);
  const float percent = raw_to_percent(raw);
  const bool water = percent < m_threshold_percent;

  m_lastRaw = raw;
  m_lastPercent = percent;
  m_lastNeedsWater = water;

  return Reading{
    .raw = m_lastRaw,
    .percent = m_lastPercent,
    .needs_water = m_lastNeedsWater
  };
}

uint16_t SoilMoistureSensor::get_raw() const { 
  return m_lastRaw; 
}

float SoilMoistureSensor::get_percent() const { 
  return m_lastPercent; 
}

bool SoilMoistureSensor::needs_water() const { 
  return m_lastNeedsWater; 
}

float SoilMoistureSensor::raw_to_percent(uint16_t raw) const {
  if (m_dryCal == m_wetCal) {
    return 0.0f;
  }

  // Typical capacitive sensor behavior:
  // drier soil -> higher ADC value
  // wetter soil -> lower ADC value
  if (raw >= m_dryCal) {
    return 0.0f;
  }

  if (raw <= m_wetCal) {
    return 100.0f;
  }

  const float numerator = static_cast<float>(m_dryCal - raw);
  const float denominator = static_cast<float>(m_dryCal - m_wetCal);
  const float percent = (numerator / denominator) * 100.0f;

  return percent;
}

void SoilMoistureSensor::ensure_initialized() const {
  if (!m_initialized) {
    panic("SoilMoistureSensor used before init()");
  }
}

void SoilMoistureSensor::set_config(const SystemConfig &cfg) {
  m_sample_count      = cfg.moisture_sample_count;
  m_warmup_ms         = cfg.moisture_warmup_ms;
  m_threshold_percent = cfg.moisture_threshold_pct;
  calibrate(cfg.moisture_dry_cal, cfg.moisture_wet_cal);
}