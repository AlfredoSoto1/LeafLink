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
    .power_pin = 12,
    .warmup_ms = 100
  };

  struct Config {
    float alert_threshold;
    float min_voltage;   // sensor output voltage at UV index 0   (ML8511: ~1.0 V)
    float max_voltage;   // sensor output voltage at max UV index (ML8511: ~2.8 V)
    float max_uv_index;  // UV index corresponding to max_voltage (ML8511: ~15.0)
  };

  struct State {
    float uv_index;
    bool is_alert;
    bool error;
  };

public:
  Config config = {
    .alert_threshold = 6.0f,
    .min_voltage     = 1.0f,
    .max_voltage     = 2.8f,
    .max_uv_index    = 15.0f
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

