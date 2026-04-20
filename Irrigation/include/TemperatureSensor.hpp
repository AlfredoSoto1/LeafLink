#pragma once

#include <cstdint>

#include "ADCController.hpp"
#include "pico/stdlib.h"

class TemperatureSensor {
public:
  struct Reading {
    uint16_t raw;
    float    voltage;
    float    celsius;
    bool     error = false;
  };

public:
  explicit TemperatureSensor(uint sample_count);
  ~TemperatureSensor() = default;

  void init();

  Reading read(ADCController &adc);

  uint16_t get_raw() const;
  float    get_voltage() const;
  float    get_celsius() const;

private:
  float raw_to_voltage(uint16_t raw) const;
  float voltage_to_celsius(float voltage) const;
  void  ensure_initialized() const;

private:
  uint     m_sample_count;
  uint16_t m_last_raw = 0;
  float    m_last_voltage = 0.0f;
  float    m_last_celsius = 0.0f;
  bool     m_initialized = false;
};