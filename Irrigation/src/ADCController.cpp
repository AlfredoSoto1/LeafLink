#include "ADCController.hpp"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

ADCController::ADCController(const ADCChannel* channels, size_t count, uint32_t settle_us)
    : count_(count > MAX_CHANNELS ? MAX_CHANNELS : count), settle_us_(settle_us)
{
  for (size_t i = 0; i < count_; ++i) {
    channels_[i] = channels[i];
  }
}

void ADCController::init() {
  adc_init();

  for (size_t i = 0; i < count_; ++i) {
    gpio_init(channels_[i].enable_gpio);
    gpio_set_dir(channels_[i].enable_gpio, GPIO_OUT);
    gpio_put(channels_[i].enable_gpio, 0);

    adc_gpio_init(26 + channels_[i].adc_input); // ADC0=GP26, ADC1=GP27 …
  }
}

RawResult ADCController::read_raw(size_t idx) {
  if (idx >= count_) {
    return { 0, false };
  }

  enable_only(idx);
  adc_select_input(channels_[idx].adc_input);
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

size_t ADCController::count() const { return count_; }

void ADCController::enable_only(size_t idx) {
  disable_all();
  gpio_put(channels_[idx].enable_gpio, 1);
  sleep_us(settle_us_);
}

void ADCController::disable_all() {
  for (size_t i = 0; i < count_; ++i) {
    gpio_put(channels_[i].enable_gpio, 0);
  }
}