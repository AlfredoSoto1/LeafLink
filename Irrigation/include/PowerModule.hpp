#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SensorController.hpp"

class PowerModule {
public:
  SensorController::Sensor sensor = {
    .power_pin = 20,
    .warmup_ms = 100
  };

  struct Config {
    uint warmup_ms;
    float v_max;
    float v_min;
    float divider_ratio;
  };

  struct State {
    float voltage;
    float percentage;
    bool warning;
    bool error;
  };

public:

  Config config = {
    .warmup_ms = 100,
    .v_max = 4.2f,
    .v_min = 3.0f,
    .divider_ratio = 0.5f
  };

  State state = {
    .voltage = 0.0f,
    .percentage = 0.0f,
    .warning = false,
    .error = false
  };

public:
  /**
   * @brief Initializes the power module by configuring the sensor's power pin and 
   *        any necessary ADC settings.
   * 
   */
  void init();

  /**
   * @brief From the current sensor's last_value, calculate the voltage and percentage
   *        based on the voltage divider ratio and min/max voltage.
   * 
   */
  void sinthesize();

private:
  float raw_to_voltage(uint16_t raw) const;
};

