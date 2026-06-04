#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "SensorController.hpp"

/**
 * @brief WaterLevelModule is responsible for interfacing with the water level sensor, 
 *        processing raw readings, and managing the water level state.
 * 
 * Sensor: Generic resistive water level sensor on ADC
 */
class WaterLevelModule {
public:
  SensorController::Sensor sensor = {
    .power_pin = 21,
    .warmup_ms = 100
  };

  struct Config {
    uint warmup_ms;
    float tank_capacity_oz;
    float tank_min_threshold_percent; // Below this percentage, the tank is considered empty
  };

  struct State {
    float ounces_remaining; 
    bool error;
  };

public:
  Config config = {
    .warmup_ms = 100,
    .tank_capacity_oz = 128.0f,
    .tank_min_threshold_percent = 10.0f
  };

  State state = {
    .ounces_remaining = 0.0f,
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
   * @brief From the current sensor's last_value, calculate the water level
   *        percentage and ounces remaining.
   * 
   */
  void sinthesize();

private:
  float raw_to_percent(uint16_t raw) const;
};
