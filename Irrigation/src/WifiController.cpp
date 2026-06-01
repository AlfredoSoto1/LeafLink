#include "WifiController.hpp"

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include <cstring>
#include <cstdio>

#define WIFI_UART uart0

void WifiController::init() {
  // Initialize enable pin
  gpio_init(ENABLE_PIN);
  gpio_set_dir(ENABLE_PIN, GPIO_OUT);
  gpio_put(ENABLE_PIN, 0);

  // Initialize pairing pin
  gpio_init(PAIR_PIN);
  gpio_set_dir(PAIR_PIN, GPIO_IN);
  gpio_pull_up(PAIR_PIN);

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

int WifiController::extract_connection_id(const std::string& data) {
  size_t start = data.find("+IPD,");
  if (start == std::string::npos) {
    return -1;
  }

  start += 5;

  size_t comma = data.find(",", start);
  if (comma == std::string::npos) {
    return -1;
  }

  std::string id = data.substr(start, comma - start);

  for (char c : id) {
    if (c < '0' || c > '9') {
      return -1;
    }
  }
  if (id.empty()) {
    return -1;
  }

  return std::stoi(id);
}

void WifiController::send_tcp(int connection_id, const std::string& message) {
  std::string command =
    "AT+CIPSEND=" +
    std::to_string(connection_id) +
    "," +
    std::to_string(message.length());

  std::string response = send_at(command, 1000);

  if (response.find(">") != std::string::npos || response.find("OK") != std::string::npos) {
    uart_send_raw(message);
    sleep_ms(300);

    std::string sendResponse = uart_read_for(1000);
    printf("%s\n", sendResponse.c_str());
  } else {
    printf("ESP8266 not ready to send data\n");
  }
}

void WifiController::handle_wifi_data(const std::string& data) {
  printf("ESP8266 DATA:\n%s\n", data.c_str());

  if (data.find("CONNECT") != std::string::npos) {
    printf("MASTER CONNECTED\n");
  }

  if (data.find("CLOSED") != std::string::npos) {
    printf("MASTER DISCONNECTED\n");
  }

  if (data.find("+IPD,") != std::string::npos) {
    int connection_id = extract_connection_id(data);

    if (connection_id >= 0) {
      printf("Received data from master on connection %d\n", connection_id);
      send_tcp(connection_id, "ACK_FROM_PICO\n");
    }
  }
}