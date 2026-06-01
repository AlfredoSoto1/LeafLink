#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#include "pico/stdlib.h"
#include "hardware/uart.h"

/**
 * @brief WifiController manages the ESP8266 WiFi module, handling initialization, 
 *        sending data, and listening for incoming data. It abstracts the UART 
 *        communication and power control for the WiFi module, providing a simple 
 *        interface for the rest of the application to use.
 * 
 */
class WifiController {
public:
  struct Config {
    const char* ap_ssid;
    const char* ap_password;
    uint16_t tcp_port;
  };

  struct State {
    // Future state variables can be added here, such as:
    // - Current IP address
    // - Connection status
    // - Error codes
  };

public:
  Config config = {
    .ap_ssid = "PICO_PAIR_DEVICE",
    .ap_password = "12345678",
    .tcp_port = 5000
  };

public:
  /**
   * @brief Initializes the WiFi controller, setting up GPIO pins and UART 
   *        communication.
   * 
   */
  void init();

  /**
   * @brief Enters the WiFi module into pairing mode, allowing it to connect
   *        to a new network.
   * 
   */
  void enter_pairing_mode();
  
private:
  static constexpr uint     PAIR_PIN   = 21;
  static constexpr uint     ENABLE_PIN = 22;
  static constexpr uint     TX_PIN     = 0;
  static constexpr uint     RX_PIN     = 1;
  static constexpr uint32_t BAUD_RATE  = 115200;

private:
  void power_cycle();
  void wifi_enable(bool enabled);
  void uart_send_raw(const std::string& data);
  void uart_send_line(const std::string& command);
  std::string uart_read_for(uint32_t timeout_ms);
  std::string send_at(const std::string& command, uint32_t timeout_ms = 1000);
  bool wait_for_ok(const std::string& command, uint32_t timeout_ms = 1000);
  int extract_connection_id(const std::string& data);
  void send_tcp(int connection_id, const std::string& message);
  void handle_wifi_data(const std::string& data);
};
