#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SystemConfig.hpp"

// ---------------------------------------------------------------------------
// Pump — motor driver controlled via GPIO
// ---------------------------------------------------------------------------

class PumpController {
public:
  static constexpr uint POWER_PIN = 15;

public:
  PumpController() = default;

  void init();
  
  void run();
  bool is_running() const;
  void set_config(const SystemConfig &cfg);
  
private:
  void power_on();
  void power_off();
  void ensure_initialized() const;

private:
  bool m_running             = false;
  bool m_initialized         = false;
  uint32_t m_default_duration_ms = 5000;
};
