#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SystemConfig.hpp"
#include "ADCController.hpp"

// ---------------------------------------------------------------------------
// WaterLevelSensor — DIYables resistive water level sensor (analog)
//
// The sensor exposes 10 conductive traces. The more traces are submerged,
// the lower the resistance and the higher the ADC output voltage.
// Calibration values (dry_val / wet_val) map the raw range to 0–100 %.
// Tank capacity in fluid ounces is used to compute ounces remaining.
// ---------------------------------------------------------------------------

class WaterLevelSensor {
public:
  static constexpr uint POWER_PIN = 21;
  static constexpr uint ADC_SELECT = 2;

  struct Reading {
    uint16_t raw;
    float    percent;         // 0–100 %
    float    ounces_remaining; // calculated from tank capacity
    bool     error = false;
  };

public:
  // tank_capacity_oz — total volume of the tank in fluid ounces
  WaterLevelSensor(uint sample_count, uint32_t warmup_ms, float tank_capacity_oz);
  ~WaterLevelSensor() = default;

  void init();

  Reading read(ADCController &adc);
  void calibrate(uint16_t dry_val, uint16_t wet_val);
  void set_config(const SystemConfig &cfg);

  uint16_t get_raw()              const;
  float    get_percent()          const;
  float    get_ounces_remaining() const;

private:
  float raw_to_percent(uint16_t raw) const;
  void  ensure_initialized() const;

private:
  uint     m_sample_count;
  uint32_t m_warmup_ms;
  float    m_tank_capacity_oz;
  uint16_t m_dry_cal;
  uint16_t m_wet_cal;

  uint16_t m_last_raw     = 0;
  float    m_last_percent = 0.0f;
  float    m_last_oz      = 0.0f;
  bool     m_initialized  = false;
};
