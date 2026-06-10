#pragma once

#include <cstdint>
#include "pico/stdlib.h"


class PumpController {
public:
  struct Config {
    float target_oz_per_day;
    float flow_rate_oz_per_sec;
  };

  struct State {
    bool running;
    uint32_t duration_ms;
    uint32_t started_at_ms;
    float total_oz_dispensed;
  };

public:
  Config config = {
    .target_oz_per_day = 1.0f,
    .flow_rate_oz_per_sec = 1.0f
  };

  State state = {
    .running = false,
    .duration_ms = 0,
    .started_at_ms = 0,
    .total_oz_dispensed = 0.0f
  };

public:
  /**
   * @brief Initializes the pump controller, setting up GPIO and internal state. 
   *        Must be called before using other methods.
   * 
   */
  void init();

  /**
   * @brief Powers on the pump. If the pump is already running, this is a no-op.
   * 
   */
  void start(float ounces);
  
  /**
   * @brief Powers off the pump. If the pump is already stopped, this is a no-op.
   * 
   */
  void stop();

  /**
   * @brief Updates the pump controller state. Should be called periodically.
   * 
   */
  void update();

private:
  uint power_pin = 9;
};
