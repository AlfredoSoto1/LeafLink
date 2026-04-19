#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SystemConfig.hpp"
#include "ADCController.hpp"

// ---------------------------------------------------------------------------
// UVSensor — ML8511 analog UV sensor on ADC
// ---------------------------------------------------------------------------

class UVSensor {
public:
  static constexpr uint POWER_PIN = 3;
  static constexpr uint ADC_SELECT = 3;

  struct Reading {
    uint16_t raw;
    float    uv_index;
    bool     is_alert;
    bool     error = false;
  };

public:
  UVSensor(uint sample_count, uint32_t warmup_ms, float alert_threshold);
  ~UVSensor() = default;

  void init();

  Reading read(ADCController &adc);
  void set_config(const SystemConfig &cfg);

  uint16_t get_raw()        const;
  float    get_uv_index()   const;
  bool     is_alert()       const;

private:
  float raw_to_uv_index(uint16_t raw) const;
  void  ensure_initialized() const;

private:
  uint     m_sample_count;
  uint32_t m_warmup_ms;
  float    m_alert_threshold;
  uint16_t m_lastRaw;
  float    m_lastUvIndex;
  bool     m_lastAlert;
  bool     m_initialized;
};

