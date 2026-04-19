#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SystemConfig.hpp"
#include "ADCController.hpp"

// ---------------------------------------------------------------------------
// PowerModule — reads battery / supply voltage via an ADC-connected voltage
// divider and reports input voltage and charge percentage.
//
// Voltage divider:
//   V_in ──R1──┬──R2── GND
//              └── ADC pin
//
// V_adc = V_in * R2/(R1+R2)   →   V_in = V_adc / divider_ratio
// ---------------------------------------------------------------------------

class PowerModule {
public:
  static constexpr uint POWER_PIN = 20;
  static constexpr uint ADC_SELECT = 0;

  struct Reading {
    uint16_t raw;
    float    voltage;   // actual input voltage (after divider correction)
    float    percent;   // 0–100 % charge estimate
    bool     error = false;
  };

public:
  // divider_ratio = R2 / (R1 + R2)
  // v_min / v_max define the 0 % / 100 % voltage points
  PowerModule(uint sample_count, uint32_t warmup_ms,
              float divider_ratio, float v_min, float v_max);
  ~PowerModule() = default;

  void init();

  Reading read(ADCController &adc);
  void set_config(const SystemConfig &cfg);

  uint16_t get_raw()        const;
  float    get_voltage()    const;
  float    get_percent()    const;

private:
  float raw_to_voltage(uint16_t raw) const;
  void  ensure_initialized() const;

private:
  uint     m_sample_count;
  uint32_t m_warmup_ms;
  float    m_divider_ratio;
  float    m_v_min;
  float    m_v_max;

  uint16_t m_last_raw     = 0;
  float    m_last_voltage = 0.0f;
  float    m_last_percent = 0.0f;
  bool     m_initialized  = false;
};

