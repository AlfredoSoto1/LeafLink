#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SystemConfig.hpp"

// ---------------------------------------------------------------------------
// Pump — motor driver controlled via GPIO
// ---------------------------------------------------------------------------

class Pump {
public:
  static constexpr uint POWER_PIN = 15;

public:
  Pump() = default;
  ~Pump() = default;

  void init();
  void on();
  void off();
  void run_for(uint32_t duration_ms);
  void run_for();        // uses configured duration from set_config
  void set_config(const SystemConfig &cfg);

  bool is_running() const;

private:
  void ensure_initialized() const;

private:
  bool m_running             = false;
  bool m_initialized         = false;
  uint32_t m_default_duration_ms = 5000;
};
