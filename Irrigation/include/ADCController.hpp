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
struct ADCChannel {
  uint enable_gpio;  // GPIO pin that powers/enables this sensor
  uint adc_input;    // RP2040 ADC input index (0-3 -> GPIO 26-29)
};

// ---------------------------------------------------------------------------
// ADCController
//
// Manages up to MAX_CHANNELS sensors sharing a single ADC pin via individual
// enable GPIOs. Only one sensor is ever powered at a time.
//
// Usage:
//   static const ADCChannel channels[] = { {10, 0}, {11, 0}, {12, 0} };
//   ADCController adc(channels, 3);
//   adc.init();
//
//   RawResult     r = adc.read_raw(0);
//   VoltageResult v = adc.read_voltage(1);
//   if (v.valid) { ... use v.value ... }
// ---------------------------------------------------------------------------
class ADCController {
public:
  static constexpr size_t MAX_CHANNELS = 8;

  // settle_us — microseconds to wait after enabling a sensor before sampling
  ADCController(const ADCChannel* channels,
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

  size_t count() const;

private:
  ADCChannel channels_[MAX_CHANNELS];
  size_t     count_;
  uint32_t   settle_us_;

  void enable_only(size_t idx);
  void disable_all();
};