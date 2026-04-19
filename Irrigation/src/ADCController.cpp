#include "ADCController.hpp"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

ADCController::ADCController(const ADCEnableChannel* channels, size_t count)
    : channels(channels), count(count)
{
}

void ADCController::init() {
  adc_init();

  for (size_t i = 0; i < count; ++i) {
    gpio_init(channels[i]);
    gpio_set_dir(channels[i], GPIO_OUT);
    gpio_put(channels[i], 0);
  }

  // ADC inputs are shared.
  adc_gpio_init(ADC_PIN);
}

RawResult ADCController::read_raw(size_t idx, uint32_t warmup_ms) {
  if (idx >= count) {
    return { 0, false };
  }

  enable_only(idx, warmup_ms);
  adc_select_input(0);
  const uint16_t raw = adc_read();
  disable_all();

  return { raw, true };
}

size_t ADCController::get_count() const { return count; }

void ADCController::enable_only(size_t idx, uint32_t warmup_ms) {
  disable_all();
  gpio_put(channels[idx], 1);
  sleep_us(warmup_ms);
}

void ADCController::disable_all() {
  for (size_t i = 0; i < count; ++i) {
    gpio_put(channels[i], 0);
  }
}