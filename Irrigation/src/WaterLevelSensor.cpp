#include "WaterLevelSensor.hpp"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/platform.h"

WaterLevelSensor::WaterLevelSensor(uint sample_count, uint32_t warmup_ms,
                                   float tank_capacity_oz)
    : m_sample_count(sample_count),
      m_warmup_ms(warmup_ms),
      m_tank_capacity_oz(tank_capacity_oz),
      m_dry_cal(0),
      m_wet_cal(3500) {}

void WaterLevelSensor::init() {
  gpio_init(POWER_PIN);
  gpio_set_dir(POWER_PIN, GPIO_OUT);
  gpio_put(POWER_PIN, 0);

  adc_init();
  adc_gpio_init(ADC_PIN);

  m_initialized = true;
}

void WaterLevelSensor::power_on() {
  gpio_put(POWER_PIN, 1);
  sleep_ms(m_warmup_ms);
}

void WaterLevelSensor::power_off() {
  gpio_put(POWER_PIN, 0);
}

WaterLevelSensor::Reading WaterLevelSensor::read() {
  ensure_initialized();

  adc_select_input(ADC_INPUT);

  uint32_t sum = 0;
  for (uint i = 0; i < m_sample_count; ++i) {
    sum += adc_read();
  }

  const uint16_t raw    = static_cast<uint16_t>(sum / m_sample_count);
  const float    pct    = raw_to_percent(raw);
  const float    oz     = (pct / 100.0f) * m_tank_capacity_oz;

  m_last_raw     = raw;
  m_last_percent = pct;
  m_last_oz      = oz;

  return Reading{ .raw = raw, .percent = pct, .ounces_remaining = oz };
}

void WaterLevelSensor::calibrate(uint16_t dry_val, uint16_t wet_val) {
  // Sensor outputs LOW when dry and HIGH when wet — wet > dry
  if (wet_val >= dry_val) {
    m_dry_cal = dry_val;
    m_wet_cal = wet_val;
  } else {
    m_dry_cal = wet_val;
    m_wet_cal = dry_val;
  }
}

void WaterLevelSensor::set_config(const SystemConfig &cfg) {
  m_sample_count      = cfg.water_sample_count;
  m_warmup_ms         = cfg.water_warmup_ms;
  m_tank_capacity_oz  = static_cast<float>(cfg.water_tank_oz);
  calibrate(cfg.water_dry_cal, cfg.water_wet_cal);
}

uint16_t WaterLevelSensor::get_raw()              const { return m_last_raw;     }
float    WaterLevelSensor::get_percent()          const { return m_last_percent; }
float    WaterLevelSensor::get_ounces_remaining() const { return m_last_oz;      }

float WaterLevelSensor::raw_to_percent(uint16_t raw) const {
  if (m_wet_cal == m_dry_cal) return 0.0f;
  if (raw <= m_dry_cal)       return 0.0f;
  if (raw >= m_wet_cal)       return 100.0f;

  return (static_cast<float>(raw - m_dry_cal) /
          static_cast<float>(m_wet_cal - m_dry_cal)) * 100.0f;
}

void WaterLevelSensor::ensure_initialized() const {
  if (!m_initialized) {
    panic("WaterLevelSensor used before init()");
  }
}
