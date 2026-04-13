#pragma once

#include "pico/stdlib.h"

struct RawResult {
  uint16_t value;
  bool     valid;
};

struct VoltageResult {
  float value;
  bool  valid;
};

// ---------------------------------------------------------------------------
// Represents a single sensor slot: one enable GPIO + its ADC input index
// ---------------------------------------------------------------------------
using ADCEnableChannel = uint;

// ---------------------------------------------------------------------------
// ADCController
//
// Manages up to MAX_CHANNELS sensors sharing a single ADC pin via individual
// enable GPIOs. Only one sensor is ever powered at a time.
//
// Usage:
//   static const ADCChannel channels[] = { {10}, {11}, {12} };
//   ADCController adc(channels, 3);
//   adc.init();
//
//   RawResult     r = adc.read_raw(0);
//   VoltageResult v = adc.read_voltage(1);
//   if (v.valid) { ... use v.value ... }
// ---------------------------------------------------------------------------
class ADCController {
public:
  static constexpr size_t ADC_PIN = 26;

  // settle_us — microseconds to wait after enabling a sensor before sampling
  ADCController(const ADCEnableChannel* channels,
                size_t            count,
                uint32_t          settle_us = 10000);

  // -----------------------------------------------------------------------
  // Initialise all enable GPIOs (output, default LOW) and ADC hardware
  // -----------------------------------------------------------------------
  void init();

  // -----------------------------------------------------------------------
  // Read raw 12-bit value (0-4095) from sensor at index idx
  // -----------------------------------------------------------------------
  RawResult read_raw(size_t idx);

  // -----------------------------------------------------------------------
  // Read calibrated voltage (0.0-3.3 V) from sensor at index idx
  // -----------------------------------------------------------------------
  VoltageResult read_voltage(size_t idx);

  size_t get_count() const;

private:
  size_t count;
  uint32_t settle_us;
  const ADCEnableChannel* channels;

  void enable_only(size_t idx);
  void disable_all();
};