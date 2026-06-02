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
    char     ap_ssid[32]     = "PICO_PAIR_DEVICE";
    char     ap_password[64] = "12345678";
    char     master_host[64] = "192.168.1.100";
    uint16_t tcp_port        = 5000;
  };

  enum class State {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
  };

public:
  Config config;
  State state = State::DISCONNECTED;

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

  /**
   * @brief Waits up to timeout_ms for the master to send the system
   *        configuration over the active TCP server (call after
   *        enter_pairing_mode). Extracts the +IPD data portion and returns it.
   *        Returns an empty string if nothing arrived in time.
   */
  std::string receive_config_payload(uint32_t timeout_ms);

  /**
   * @brief Resets the ESP8266 into station mode and connects to the master's
   *        WiFi AP and TCP endpoint using config credentials.
   *
   * @return true if the TCP connection was established.
   */
  bool connect_to_master();

  /**
   * @brief Sends the system states payload to the master over the open TCP
   *        connection (call after connect_to_master succeeds).
   */
  void send_states_payload(const std::string& states_payload);

public:
  inline static bool pairing_requested = false;

private:
  static constexpr uint     PAIR_PIN   = 11;
  static constexpr uint     ENABLE_PIN = 3;
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
};
