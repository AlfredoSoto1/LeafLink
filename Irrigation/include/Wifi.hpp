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
  static constexpr uint16_t PAIRING_PORT  = 8080;

  enum class State : uint8_t {
    DISCONNECTED = 0,
    PAIRING,
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

  // Puts the ESP8266 into soft-AP + TCP-server mode so the master can
  // discover and pair with this node.  node_id is a short string (e.g. "0001")
  // appended to the SSID: "LeafLink-Node-0001".
  bool start_pairing_beacon(const char *node_id);
 
  // Blocks until a PAIR:<ssid>:<password>\r\n message arrives on the TCP
  // server, or timeout_ms elapses.  Parsed credentials are written into the
  // output buffers.  Returns true on success.
  bool await_pair_command(char *ssid_out,  size_t ssid_len,
                          char *pass_out,  size_t pass_len,
                          uint32_t timeout_ms);
 
  // Tears down the TCP server and switches ESP8266 back to station mode.
  // Sets m_initialized = false; call init() before connect().
  void stop_pairing_beacon();

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
