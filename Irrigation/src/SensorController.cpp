#include "SensorController.hpp"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>

SensorController::SensorController()
    : acquired_sensor(nullptr)
{
}

void SensorController::init() {
  adc_init();
  adc_gpio_init(ADC_PIN);
  adc_select_input(0);
}

void SensorController::start() {
  if (!acquired_sensor) {
    printf("[SensorController] Warning: start() called with no sensor acquired.\n");
    return;
  }

  // Enable the sensor's power pin and wait for warmup time
  gpio_put(acquired_sensor->power_pin, 1);
  sleep_ms(acquired_sensor->warmup_ms);
}

void SensorController::stop() {
  if (!acquired_sensor) {
    printf("[SensorController] Warning: stop() called with no sensor acquired.\n");
    return;
  }
  // Disable the sensor's power pin to save power
  gpio_put(acquired_sensor->power_pin, 0);
}

void SensorController::acquire(Sensor* sensor) {
  if (acquired_sensor) {
    printf("[SensorController] Warning: acquire() called while another sensor is already acquired. Releasing previous sensor.\n");
    release();
  }
  // Set the new sensor as acquired. The caller should then call start() 
  // to enable it.
  acquired_sensor = sensor;
}

void SensorController::release() {
  if (!acquired_sensor) {
    printf("[SensorController] Warning: release() called with no sensor acquired.\n");
    return;
  }
  // Disable all power pins to ensure everything is off, then clear the 
  // acquired sensor state.
  stop();
  acquired_sensor = nullptr;
}

uint16_t SensorController::read_raw() {
  if (!acquired_sensor) {
    printf("[SensorController] Warning: read_raw() called with no sensor acquired.\n");
    return 0;
  }
  // Read the raw ADC value for the currently acquired sensor. This assumes
  // that the sensor's power pin is already enabled and warmed up.
  adc_select_input(0);
  uint16_t raw_value = adc_read();
  acquired_sensor->last_value = raw_value;
  return raw_value;
}
