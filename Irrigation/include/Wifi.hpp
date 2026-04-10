#pragma once

#include <cstdint>
#include <cstddef>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "SystemConfig.hpp"

// ---------------------------------------------------------------------------
// WifiModule — ESP8266 over UART (Hayes AT command set)
// ---------------------------------------------------------------------------

class WifiModule {
public:
  static constexpr uint     POWER_PIN = 21;
  static constexpr uint     TX_PIN    = 0;
  static constexpr uint     RX_PIN    = 1;
  static constexpr uint32_t BAUD_RATE = 115200;

  enum class State : uint8_t {
    DISCONNECTED = 0,
    CONNECTED,
    ERROR_STATE,
  };

public:
  explicit WifiModule(uart_inst_t *uart);
  ~WifiModule() = default;

  void power_on();
  void power_off();
  bool init();
  bool connect(const char *ssid, const char *password);
  bool send(const char *host, uint16_t port, const char *payload);
  bool request_config(const char *host, uint16_t port, SystemConfig &out);
  void reset();

  State get_state()    const;
  bool  is_connected() const;

private:
  bool send_at(const char *cmd, const char *expect, uint32_t timeout_ms);
  bool receive_ipd(char *buf, size_t max_len, uint32_t timeout_ms);
  bool parse_config(const char *data, SystemConfig &out);
  void flush_rx();

private:
  uart_inst_t *m_uart;
  State        m_state;
  bool         m_initialized;
};
