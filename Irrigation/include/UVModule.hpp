#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SensorController.hpp"

/**
 * @brief UVModule is responsible for interfacing with the UV sensor, 
 *        processing raw readings, and managing the UV index state and alerts.
 * 
 * Sensor: ML8511 UV
 */
class UVModule {
public:
  SensorController::Sensor sensor = {
    .power_pin = 3,
    .warmup_ms = 100
  };

  struct Config {
    float alert_threshold;
    float min_uv_index;
    float max_uv_index;
  };

  struct State {
    float uv_index;
    bool is_alert;
    bool error;
  };

public:
  Config config = {
    .alert_threshold = 6.0f,
    .min_uv_index = 0.0f,
    .max_uv_index = 11.0f
  };

  State state = {
    .uv_index = 0.0f,
    .is_alert = false,
    .error = false
  };

public:
  /**
   * @brief Initializes the sensor pin and sets it to a known state (powered off).
   *        This should be called once at startup before any readings are taken.
   * 
   */
  void init();

  /**
   * @brief From the current sensor's last_value, calculate the UV index and 
   *        alert state.
   * 
   */
  void sinthesize();

private:
  float raw_to_uv_index(uint16_t raw) const;
};

