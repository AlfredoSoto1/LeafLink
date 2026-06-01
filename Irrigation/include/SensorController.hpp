#pragma once

#include "pico/stdlib.h"

class SensorController {
public:
  struct Sensor {
    uint power_pin;
    uint warmup_ms;
    uint16_t last_value;
  };

  static constexpr size_t ADC_PIN = 26;

  SensorController();

  /**
   * @brief Initializes the ADC and configures all power pins as outputs. 
   *        This should be called once at startup before any readings are taken.
   * 
   */
  void init();

  /**
   * @brief Starts the sensor reading process for the currently acquired sensor. 
   *        This will enable the sensor's power pin and wait for the specified 
   *        warmup time before allowing reads. If no sensor is currently acquired, 
   *        this will print a warning and do nothing.
   */
  void start();

  /**
   * @brief Stops all sensors by disabling all power pins. This should be 
   *        called after reading is done to save power.
   * 
   */
  void stop();

  /**
   * @brief Acquires a sensor for reading. This will set the internal state to track
   *        the specified sensor and prepare it for reading.
   * 
   * @param sensor The sensor to acquire.
   */
  void acquire(Sensor* sensor);

  /**
   * @brief Releases the currently acquired sensor, if any. This will disable all
   *        power pins and reset the internal state.
   */
  void release();

  /**
   * @brief Read raw 12-bit value (0-4095) from the currently acquired sensor.
   * 
   * @return uint16_t 
   */
  uint16_t read_raw();

private:
  Sensor* acquired_sensor;
};