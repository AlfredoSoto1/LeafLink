#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SensorController.hpp"

/**
 * @brief SoilMoistureModule is responsible for interfacing with the soil moisture sensor, 
 *        processing raw readings, and managing the soil moisture state.
 * 
 * Sensor: Generic resistive soil moisture sensor on ADC
 */
class SoilMoistureModule {
public:
  SensorController::Sensor sensor = {
    .power_pin = 2,
    .warmup_ms = 500
  };

  struct Config {
    uint warmup_ms;
    float threshold_percent;
    uint16_t dry_cal;
    uint16_t wet_cal;
  };

  struct State {
    float moisture_percent;
    bool is_dry;
    bool error;
  };

public:
  Config config = {
    .warmup_ms = 500,
    .threshold_percent = 30.0f,
    .dry_cal = 1023,
    .wet_cal = 0
  };

  State state = {
    .moisture_percent = 0.0f,
    .is_dry = false,
    .error = false
  };

public:
  /**
   * @brief Initializes the soil moisture module by configuring the sensor's power pin and 
   *        any necessary ADC settings.
   * 
   */
  void init();

  /**
   * @brief From the current sensor's last_value, calculate the moisture percentage
   *        and dry/wet state.
   * 
   */
  void sinthesize();

private:
  float raw_to_percent(uint16_t raw) const;
};