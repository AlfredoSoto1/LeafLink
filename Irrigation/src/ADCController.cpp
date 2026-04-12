#include "ADCController.hpp"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

ADCController::ADCController(const ADCChannel* channels, size_t count, uint32_t settle_us)
    : channels(channels), count(count), settle_us(settle_us)
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

RawResult ADCController::read_raw(size_t idx) {
  if (idx >= count) {
    return { 0, false };
  }

  enable_only(idx);
  adc_select_input(ADC_PIN);
  const uint16_t raw = adc_read();
  disable_all();

  return { raw, true };
}

VoltageResult ADCController::read_voltage(size_t idx) {
  RawResult r = read_raw(idx);
  if (!r.valid) {
    return { 0.0f, false };
  }

  constexpr float VREF    = 3.3f;
  constexpr float ADC_MAX = 4095.0f;
  return { (r.value / ADC_MAX) * VREF, true };
}

size_t ADCController::get_count() const { return count; }

void ADCController::enable_only(size_t idx) {
  disable_all();
  gpio_put(channels[idx], 1);
  sleep_us(settle_us);
}

void ADCController::disable_all() {
  for (size_t i = 0; i < count; ++i) {
    gpio_put(channels[i], 0);
  }
}