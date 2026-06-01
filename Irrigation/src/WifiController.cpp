#include "WifiController.hpp"

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include <cstring>
#include <cstdio>

#define WIFI_UART uart0

// Fires on the falling edge of the pairing button (GPIO21 pulled high).
// Safe to call from ISR context — only sets a flag.
static void pair_button_irq(uint gpio, uint32_t events) {
  WifiController::pairing_requested = true;
}

void WifiController::init() {
  // Initialize enable pin
  gpio_init(ENABLE_PIN);
  gpio_set_dir(ENABLE_PIN, GPIO_OUT);
  gpio_put(ENABLE_PIN, 0);

  // Initialize pairing pin & interrupt
  gpio_init(PAIR_PIN);
  gpio_set_dir(PAIR_PIN, GPIO_IN);
  gpio_pull_up(PAIR_PIN);
  gpio_set_irq_enabled_with_callback(PAIR_PIN, GPIO_IRQ_EDGE_FALL, true, &pair_button_irq);

  // Initialize UART
  uart_init(WIFI_UART, BAUD_RATE);
  gpio_set_function(TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(RX_PIN, GPIO_FUNC_UART);
}

void WifiController::enter_pairing_mode() {
  printf("Starting ESP8266 pairing mode...\n");

  power_cycle();

  send_at("AT", 1000);
  send_at("ATE0", 1000);

  // Reset ESP8266
  send_at("AT+RST", 3000);
  // AT+RST reboots the module, restoring echo to default (on).
  // Re-disable echo before sending further commands.
  send_at("ATE0", 1000);

  // Mode 2 = SoftAP mode
  send_at("AT+CWMODE_DEF=2", 1000);

  // Optional: set AP IP address
  send_at("AT+CIPAP_DEF=\"192.168.4.1\"", 1000);

  // Configure WiFi AP.
  // Format:
  // AT+CWSAP_DEF="ssid","password",channel,encryption
  //
  // encryption:
  // 0 = open
  // 3 = WPA2_PSK
  std::string apCommand =
    "AT+CWSAP_DEF=\"" +
    std::string(config.ap_ssid) +
    "\",\"" +
    std::string(config.ap_password) +
    "\",5,3";

  send_at(apCommand, 1500);

  // Allow multiple TCP connections
  send_at("AT+CIPMUX=1", 1000);

  // Start TCP server
  send_at("AT+CIPSERVER=1," + std::to_string(config.tcp_port), 1000);

  printf("PAIRING MODE READY\n");
  printf("SSID: %s\n", config.ap_ssid);
  printf("PASS: %s\n", config.ap_password);
  printf("PORT: %d\n", config.tcp_port);

  // Block until master connects to the TCP server. The ESP8266 sends
  // "0,CONNECT" (or just "CONNECT") when a client establishes the TCP link.
  printf("[Pairing] Waiting for master to connect...\n");
  std::string conn;
  while (conn.find("CONNECT") == std::string::npos) {
    conn += uart_read_for(1000);
  }
  printf("[Pairing] Master connected.\n");
}

void WifiController::power_cycle() {
  printf("Power cycling WiFi module...\n");
  
  wifi_enable(false);
  sleep_ms(1000);

  wifi_enable(true);
  sleep_ms(3000);
}

void WifiController::wifi_enable(bool enabled) {
  gpio_put(ENABLE_PIN, enabled ? 1 : 0);
}

void WifiController::uart_send_raw(const std::string& data) {
  uart_write_blocking(WIFI_UART, reinterpret_cast<const uint8_t*>(data.c_str()), data.length());
}

void WifiController::uart_send_line(const std::string& command) {
  uart_send_raw(command + "\r\n");
}

std::string WifiController::uart_read_for(uint32_t timeout_ms) {
  std::string response;
  absolute_time_t end = make_timeout_time_ms(timeout_ms);

  while (!time_reached(end)) {
    while (uart_is_readable(WIFI_UART)) {
      char c = uart_getc(WIFI_UART);
      response += c;
    }
    sleep_ms(5);
  }

  return response;
}

std::string WifiController::send_at(const std::string& command, uint32_t timeout_ms) {
  printf(">> %s\n", command.c_str());
  uart_send_line(command);

  std::string response = uart_read_for(timeout_ms);
  printf("%s\n", response.c_str());

  return response;
}

bool WifiController::wait_for_ok(const std::string& command, uint32_t timeout_ms) {
  std::string response = send_at(command, timeout_ms);
  return response.find("OK") != std::string::npos;
}

std::string WifiController::receive_config_payload(uint32_t timeout_ms) {
  std::string raw = uart_read_for(timeout_ms);
  wifi_enable(false);

  if (raw.empty()) return "";

  // ESP8266 frames incoming TCP data as: +IPD,<conn_id>,<len>:<payload>
  size_t ipd = raw.find("+IPD,");
  if (ipd == std::string::npos) return "";

  size_t colon = raw.find(':', ipd);
  if (colon == std::string::npos) return "";

  return raw.substr(colon + 1);
}

bool WifiController::connect_to_master() {
  power_cycle();
  send_at("AT", 1000);
  send_at("ATE0", 1000);
  send_at("AT+RST", 3000);
  send_at("ATE0", 1000);

  // Station mode
  if (!wait_for_ok("AT+CWMODE_DEF=1", 2000)) return false;

  // Connect to master's AP using stored credentials
  std::string join_cmd =
    "AT+CWJAP=\"" + std::string(config.ap_ssid) + "\",\"" + std::string(config.ap_password) + "\"";
  std::string resp = send_at(join_cmd, 15000);
  if (resp.find("WIFI CONNECTED") == std::string::npos &&
      resp.find("OK") == std::string::npos) {
    return false;
  }

  // Open TCP connection to master
  std::string tcp_cmd =
    "AT+CIPSTART=\"TCP\",\"" + std::string(config.master_host) + "\"," +
    std::to_string(config.tcp_port);
  resp = send_at(tcp_cmd, 5000);
  return resp.find("CONNECT") != std::string::npos ||
         resp.find("OK") != std::string::npos;
}

void WifiController::send_states_payload(const std::string& states_payload) {
  std::string cmd = "AT+CIPSEND=" + std::to_string(states_payload.length());
  std::string resp = send_at(cmd, 1000);
  if (resp.find(">") != std::string::npos) {
    uart_send_raw(states_payload);
    sleep_ms(300);
    uart_read_for(1000);
  } else {
    printf("[WiFi] send_states_payload: module not ready to send.\n");
  }
  send_at("AT+CIPCLOSE", 1000);
  wifi_enable(false);
}