#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SystemConfig.hpp"
#include "ADCController.hpp"

// ---------------------------------------------------------------------------
// SoilMoistureSensor — capacitive / resistive probe on ADC
// ---------------------------------------------------------------------------

class SoilMoistureSensor {
public:
  static constexpr uint POWER_PIN = 2;
  static constexpr uint ADC_SELECT = 1;

  struct Reading {
    uint16_t raw;
    float percent;
    bool needs_water;
    bool error = false;
  };

public:
  SoilMoistureSensor(uint sample_count, uint32_t warmup_ms, float threshold_percent);
  ~SoilMoistureSensor() = default;

  void init();

  Reading read(ADCController &adc);
  void calibrate(uint16_t dry_val, uint16_t wet_val);
  void set_config(const SystemConfig &cfg);

private:
  float raw_to_percent(uint16_t raw) const;
  void ensure_initialized() const;

private:
  uint m_sample_count;
  uint32_t m_warmup_ms;
  uint16_t m_dryCal;
  uint16_t m_wetCal;
  float m_threshold_percent;
  bool m_initialized;
};