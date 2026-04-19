#include "UVSensor.hpp"

// ML8511 output voltage: ~1.0 V at 0 UV index, ~2.8 V at ~15 UV index
// ADC reference 3.3 V, 12-bit resolution (0–4095)
static constexpr float ADC_VREF     = 3.3f;
static constexpr float ADC_MAX      = 4095.0f;
static constexpr float SENSOR_V_MIN = 0.0f;  // 1.0f
static constexpr float SENSOR_V_MAX = 1.0f;  // 2.8f
static constexpr float UV_INDEX_MAX = 11.0f; // 15.0f

UVSensor::UVSensor(uint sample_count, uint32_t warmup_ms, float alert_threshold)
    : m_sample_count(sample_count),
      m_warmup_ms(warmup_ms),
      m_alert_threshold(alert_threshold),
      m_lastRaw(0),
      m_lastUvIndex(0.0f),
      m_lastAlert(false),
      m_initialized(false) {}

void UVSensor::init() {
  m_initialized = true;
}

UVSensor::Reading UVSensor::read(ADCController &adc) {
  ensure_initialized();

  uint32_t sum = 0;
  for (uint i = 0; i < m_sample_count; ++i) {
    auto result = adc.read_raw(ADC_SELECT, m_warmup_ms);
    if (result.valid) {
      sum += result.value;
    } else {
      return Reading{ .error = true };
    }
  }

  const uint16_t raw      = static_cast<uint16_t>(sum / m_sample_count);
  const float    uv_index = raw_to_uv_index(raw);
  const bool     alert    = uv_index >= m_alert_threshold;

  m_lastRaw     = raw;
  m_lastUvIndex = uv_index;
  m_lastAlert   = alert;

  return Reading{ 
    .raw = raw, 
    .uv_index = uv_index, 
    .is_alert = alert 
  };
}

uint16_t UVSensor::get_raw() const {
  return m_lastRaw;
}

float UVSensor::get_uv_index() const {
  return m_lastUvIndex;
}

bool UVSensor::is_alert() const {
  return m_lastAlert;
}

float UVSensor::raw_to_uv_index(uint16_t raw) const {
  const float voltage = (static_cast<float>(raw) / ADC_MAX) * ADC_VREF;

  if (voltage <= SENSOR_V_MIN) return 0.0f;
  if (voltage >= SENSOR_V_MAX) return UV_INDEX_MAX;

  return ((voltage - SENSOR_V_MIN) / (SENSOR_V_MAX - SENSOR_V_MIN)) * UV_INDEX_MAX;
}

void UVSensor::ensure_initialized() const {
  if (!m_initialized) {
    panic("UVSensor used before init()");
  }
}

void UVSensor::set_config(const SystemConfig &cfg) {
  m_sample_count    = cfg.uv_sample_count;
  m_warmup_ms       = cfg.uv_warmup_ms;
  m_alert_threshold = cfg.uv_alert_threshold;
}
