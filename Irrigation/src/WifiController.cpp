#include "WifiController.hpp"

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

#define WIFI_UART uart0

static constexpr bool WIFI_DEBUG_AT = false;
static constexpr bool WIFI_DEBUG_UART = false;
static constexpr const char* PICO_AP_SSID = "PICO_PAIR_DEVICE";
static constexpr const char* PICO_AP_PASSWORD = "12345678";

static bool response_ok(const std::string& response) {
  return response.find("OK") != std::string::npos ||
         response.find("SEND OK") != std::string::npos;
}

static bool response_error(const std::string& response) {
  return response.find("ERROR") != std::string::npos ||
         response.find("FAIL") != std::string::npos;
}

// Fires on the falling edge of the pairing button (GPIO11 pulled high).
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
  printf("[WiFi] Entering pairing mode...\n");

  power_cycle();

  wait_for_ok("AT", 1000);
  wait_for_ok("ATE0", 1000);

  // Reset ESP8266
  printf("[WiFi] Resetting WiFi module...\n");
  uart_send_line("AT+RST");
  uart_read_for(3000);
  // AT+RST reboots the module, restoring echo to default (on).
  // Re-disable echo before sending further commands.
  wait_for_ok("ATE0", 1000);

  // Mode 2 = SoftAP mode
  wait_for_ok("AT+CWMODE_DEF=2", 1000);

  // Optional: set AP IP address
  wait_for_ok("AT+CIPAP_DEF=\"192.168.5.1\"", 1000);

  // Configure WiFi AP.
  // Format:
  // AT+CWSAP_DEF="ssid","password",channel,encryption
  //
  // encryption:
  // 0 = open
  // 3 = WPA2_PSK
  std::string apCommand =
    "AT+CWSAP_DEF=\"" +
    std::string(PICO_AP_SSID) +
    "\",\"" +
    std::string(PICO_AP_PASSWORD) +
    "\",5,3";

  wait_for_ok(apCommand, 1500);

  // Allow multiple TCP connections
  wait_for_ok("AT+CIPMUX=1", 1000);

  // Start TCP server
  wait_for_ok("AT+CIPSERVER=1," + std::to_string(config.tcp_port), 1000);

  printf("[WiFi] Pairing mode ready.\n");
  printf("[WiFi] SSID: %s\n", PICO_AP_SSID);
  printf("[WiFi] PASS: %s\n", PICO_AP_PASSWORD);
  printf("[WiFi] PORT: %d\n", config.tcp_port);

  printf("[Pairing] TCP server is listening for master config.\n");
}

void WifiController::enter_state_report_mode() {
  printf("[WiFi] Entering state report mode...\n");

  power_cycle();

  wait_for_ok("AT", 1000);
  wait_for_ok("ATE0", 1000);
  printf("[WiFi] Resetting WiFi module...\n");
  uart_send_line("AT+RST");
  uart_read_for(3000);
  wait_for_ok("ATE0", 1000);
  wait_for_ok("AT+CWMODE_DEF=2", 1000);
  wait_for_ok("AT+CIPAP_DEF=\"192.168.5.1\"", 1000);

  std::string apCommand =
    "AT+CWSAP_DEF=\"" +
    std::string(PICO_AP_SSID) +
    "\",\"" +
    std::string(PICO_AP_PASSWORD) +
    "\",5,3";

  wait_for_ok(apCommand, 1500);
  wait_for_ok("AT+CIPMUX=1", 1000);
  wait_for_ok("AT+CIPSERVER=1," + std::to_string(config.tcp_port), 1000);

  printf("[Report] Pico online for state transfer. SSID=%s PORT=%d\n",
         PICO_AP_SSID,
         config.tcp_port);
}

void WifiController::power_cycle() {
  printf("[WiFi] Power cycling WiFi module...\n");
  
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
    bool read_any = false;
    while (uart_is_readable(WIFI_UART)) {
      char c = uart_getc(WIFI_UART);
      response += c;
      read_any = true;
    }
    if (read_any) {
      tight_loop_contents();
    } else {
      sleep_us(250);
    }
  }

  return response;
}

std::string WifiController::send_at(const std::string& command, uint32_t timeout_ms) {
  if (WIFI_DEBUG_AT) {
    printf("[WiFi] >> %s\n", command.c_str());
  }
  uart_send_line(command);

  std::string response = uart_read_for(timeout_ms);
  if (WIFI_DEBUG_AT) {
    printf("[WiFi] << %s\n", response.c_str());
  } else if (response_error(response)) {
    printf("[WiFi] Command failed: %s\n", command.c_str());
  }

  return response;
}

bool WifiController::wait_for_ok(const std::string& command, uint32_t timeout_ms) {
  std::string response = send_at(command, timeout_ms);
  return response.find("OK") != std::string::npos;
}

bool WifiController::send_tcp_server_payload(uint8_t connection_id,
                                             const std::string& payload,
                                             std::string* captured_uart) {
  std::string cmd =
    "AT+CIPSEND=" + std::to_string(connection_id) + "," + std::to_string(payload.length());
  if (WIFI_DEBUG_AT) {
    printf("[WiFi] >> %s\n", cmd.c_str());
  }
  uart_send_line(cmd);

  std::string response = uart_read_for(1000);
  if (captured_uart != nullptr) {
    captured_uart->append(response);
  }
  if (WIFI_DEBUG_AT) {
    printf("[WiFi] << %s\n", response.c_str());
  }

  if (response.find(">") == std::string::npos) {
    if (response.find("link is not valid") == std::string::npos) {
      printf("[Pairing] Could not send TCP server payload.\n");
    }
    return false;
  }

  uart_send_raw(payload);
  std::string send_response = uart_read_for(250);
  if (captured_uart != nullptr) {
    captured_uart->append(send_response);
  }
  if (WIFI_DEBUG_AT && !send_response.empty()) {
    printf("[WiFi] << %s\n", send_response.c_str());
  }
  return true;
}

std::string WifiController::receive_config_payload(uint32_t timeout_ms, size_t expected_bytes) {
  std::string raw;
  std::string control;
  std::string payload;
  absolute_time_t end = make_timeout_time_ms(timeout_ms);
  bool master_connected = false;
  bool need_config_sent = false;
  bool ready_for_payload = false;
  uint8_t connection_id = 0;
  size_t parse_offset = 0;
  size_t frame_count = 0;

  while (!time_reached(end)) {
    std::string chunk = uart_read_for(100);

    if (WIFI_DEBUG_UART && !chunk.empty()) {
      printf("[UART][RX] ");
      for (char c : chunk) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 32 && uc <= 126) {
          printf("%c", c);
        } else {
          printf("\\x%02X", uc);
        }
      }
      printf("\n");
    }

    raw += chunk;

    if (!master_connected && raw.find("+STA_CONNECTED") != std::string::npos) {
      printf("[Pairing] ESP32 station joined Pico AP.\n");
      raw.erase(0, raw.find("+STA_CONNECTED") + 14);
    }

    if (!master_connected) {
      size_t connected = raw.find(",CONNECT");
      if (connected != std::string::npos &&
          connected > 0 &&
          raw[connected - 1] >= '0' &&
          raw[connected - 1] <= '4') {
        char id_char = raw[connected - 1];
        connection_id = static_cast<uint8_t>(id_char - '0');

        printf("[Pairing] Master connected on TCP id %u. Waiting for MASTER_QUERY...\n",
               connection_id);
        pairing_connection_id = connection_id;
        pairing_connection_active = true;
        master_connected = true;
      }
    }

    if (master_connected) {
      std::string closed_token = std::to_string(connection_id) + ",CLOSED";
      if (raw.find(closed_token) != std::string::npos) {
        printf("[Pairing] Master disconnected before config payload. Waiting for reconnect...\n");
        raw.clear();
        control.clear();
        payload.clear();
        parse_offset = 0;
        frame_count = 0;
        master_connected = false;
        need_config_sent = false;
        ready_for_payload = false;
        pairing_connection_active = false;
        continue;
      }
    }

    while (true) {
      size_t ipd = raw.find("+IPD,", parse_offset);
      if (ipd == std::string::npos) {
        if (raw.size() > 1024 && payload.empty()) {
          raw.erase(0, raw.size() - 128);
        }
        parse_offset = 0;
        break;
      }

      size_t colon = raw.find(':', ipd);
      if (colon == std::string::npos) {
        parse_offset = ipd;
        break;
      }

      size_t len_start = raw.rfind(',', colon);
      if (len_start == std::string::npos || len_start <= ipd) {
        parse_offset = ipd + 5;
        continue;
      }

      size_t id_start = ipd + 5;
      if (id_start < raw.size() && raw[id_start] >= '0' && raw[id_start] <= '4') {
        connection_id = static_cast<uint8_t>(raw[id_start] - '0');
      }

      std::string len_text = raw.substr(len_start + 1, colon - len_start - 1);
      size_t frame_len = static_cast<size_t>(std::strtoul(len_text.c_str(), nullptr, 10));
      size_t payload_start = colon + 1;

      if (raw.size() - payload_start < frame_len) {
        parse_offset = ipd;
        break;
      }

      std::string frame = raw.substr(payload_start, frame_len);

      raw.erase(0, payload_start + frame_len);
      parse_offset = 0;

      if (!need_config_sent) {
        control += frame;
        if (control.find("MASTER_QUERY") != std::string::npos) {
          printf("[Pairing] Master asked for Pico request. Replying NEED_CONFIG.\n");
          if (send_tcp_server_payload(connection_id, "PICO_CONNECTED\nNEED_CONFIG\n", &raw)) {
            need_config_sent = true;
            control.clear();
            printf("[Pairing] NEED_CONFIG sent. Waiting for READY_CONFIG...\n");
          }
        }
        continue;
      }

      if (!ready_for_payload) {
        control += frame;
        if (control.find("READY_CONFIG") != std::string::npos) {
          printf("[Pairing] Master is ready to send config text.\n");
          ready_for_payload = true;
          control.clear();
        }
        continue;
      }

      payload += frame;
      ++frame_count;
      if (frame_count == 1 || frame_count % 4 == 0) {
        printf("[Config] Receiving text config... %u bytes so far.\n",
               static_cast<unsigned>(payload.size()));
      }

      bool payload_complete =
        expected_bytes == 0 ? payload.find('\n') != std::string::npos
                            : payload.size() >= expected_bytes;

      if (payload_complete) {
        if (expected_bytes != 0 && payload.size() > expected_bytes) {
          printf("[Config] Warning: received %u extra byte(s); trimming to expected size.\n",
                 static_cast<unsigned>(payload.size() - expected_bytes));
          payload.resize(expected_bytes);
        }

        printf("[Config] Full config payload received: %u bytes.\n",
               static_cast<unsigned>(payload.size()));
        pairing_connection_id = connection_id;
        pairing_connection_active = true;
        return payload;
      }
    }
  }

  wifi_enable(false);
  pairing_connection_active = false;

  printf("[Config] Timed out waiting for config payload. Received %u/%u bytes.\n",
         static_cast<unsigned>(payload.size()),
         static_cast<unsigned>(expected_bytes));
  return "";
}

void WifiController::send_config_result(bool ok) {
  if (!pairing_connection_active) {
    printf("[Pairing] No active pairing connection for config result.\n");
    wifi_enable(false);
    return;
  }

  std::string payload = ok ? "CONFIG_OK\nPICO_DONE\n" : "CONFIG_ERR\nPICO_DONE\n";
  send_tcp_server_payload(pairing_connection_id, payload);
  uart_send_line("AT+CIPCLOSE=" + std::to_string(pairing_connection_id));
  uart_read_for(1000);
  pairing_connection_active = false;
  wifi_enable(false);
}

bool WifiController::connect_to_master() {
  state = State::CONNECTING;
  power_cycle();
  send_at("AT", 1000);
  send_at("ATE0", 1000);
  send_at("AT+RST", 3000);
  send_at("ATE0", 1000);

  // Station mode
  if (!wait_for_ok("AT+CWMODE_DEF=1", 2000)) {
    state = State::ERROR;
    return false;
  }

  // Connect to master's AP using stored credentials
  std::string join_cmd =
    "AT+CWJAP=\"" + std::string(config.ap_ssid) + "\",\"" + std::string(config.ap_password) + "\"";
  std::string resp = send_at(join_cmd, 15000);
  if (resp.find("WIFI CONNECTED") == std::string::npos &&
      resp.find("OK") == std::string::npos) {
    state = State::ERROR;
    return false;
  }

  // Open TCP connection to master
  std::string tcp_cmd =
    "AT+CIPSTART=\"TCP\",\"" + std::string(config.master_host) + "\"," +
    std::to_string(config.tcp_port);
  resp = send_at(tcp_cmd, 5000);
  bool connected = resp.find("CONNECT") != std::string::npos ||
                   resp.find("OK") != std::string::npos;
  state = connected ? State::CONNECTED : State::ERROR;
  return connected;
}

void WifiController::send_states_payload(const std::string& states_payload) {
  printf("[Report] Waiting for ESP32 master to collect states...\n");

  std::string raw;
  std::string incoming;
  absolute_time_t end = make_timeout_time_ms(60000);
  uint8_t connection_id = 0;
  bool master_connected = false;
  bool announced = false;
  bool sent_states = false;
  bool done_sent = false;
  size_t parse_offset = 0;

  while (!time_reached(end)) {
    raw += uart_read_for(100);

    if (!master_connected && raw.find("+STA_CONNECTED") != std::string::npos) {
      printf("[Report] ESP32 station joined Pico AP.\n");
      raw.erase(0, raw.find("+STA_CONNECTED") + 14);
    }

    if (!master_connected) {
      size_t connected = raw.find(",CONNECT");
      if (connected != std::string::npos &&
          connected > 0 &&
          raw[connected - 1] >= '0' &&
          raw[connected - 1] <= '4') {
        connection_id = static_cast<uint8_t>(raw[connected - 1] - '0');
        printf("[Report] Master connected on TCP id %u.\n", connection_id);
        master_connected = true;
      }
    }

    while (true) {
      size_t ipd = raw.find("+IPD,", parse_offset);
      if (ipd == std::string::npos) {
        parse_offset = 0;
        break;
      }

      size_t colon = raw.find(':', ipd);
      if (colon == std::string::npos) {
        parse_offset = ipd;
        break;
      }

      size_t len_start = raw.rfind(',', colon);
      if (len_start == std::string::npos || len_start <= ipd) {
        parse_offset = ipd + 5;
        continue;
      }

      size_t frame_len = static_cast<size_t>(
        std::strtoul(raw.substr(len_start + 1, colon - len_start - 1).c_str(), nullptr, 10));
      size_t payload_start = colon + 1;
      if (raw.size() - payload_start < frame_len) {
        parse_offset = ipd;
        break;
      }

      incoming.append(raw, payload_start, frame_len);
      raw.erase(0, payload_start + frame_len);
      parse_offset = 0;
    }

    if (master_connected && !announced && incoming.find("MASTER_QUERY") != std::string::npos) {
      printf("[Report] Master asked for Pico request. Replying SENDING_STATES.\n");
      if (send_tcp_server_payload(connection_id, "PICO_CONNECTED\nSENDING_STATES\n", &raw)) {
        printf("[Report] SENDING_STATES sent. Waiting for READY_STATES...\n");
        announced = true;
      }
    }

    if (announced && !sent_states && incoming.find("READY_STATES") != std::string::npos) {
      printf("[Report] Master is ready. Sending states text (%u bytes)...\n",
             static_cast<unsigned>(states_payload.size()));
      send_tcp_server_payload(connection_id, states_payload, &raw);
      sent_states = true;
    }

    if (sent_states && incoming.find("STATES_OK") != std::string::npos) {
      printf("[Report] Master acknowledged states.\n");
      if (!done_sent) {
        send_tcp_server_payload(connection_id, "PICO_DONE\n", &raw);
        done_sent = true;
      }
      uart_send_line("AT+CIPCLOSE=" + std::to_string(connection_id));
      uart_read_for(500);
      wifi_enable(false);
      state = State::DISCONNECTED;
      return;
    }

    if (master_connected) {
      std::string closed_token = std::to_string(connection_id) + ",CLOSED";
      if (raw.find(closed_token) != std::string::npos) {
        printf("[Report] Master disconnected before state ACK was received. Waiting for reconnect...\n");
        raw.clear();
        incoming.clear();
        parse_offset = 0;
        master_connected = false;
        announced = false;
        sent_states = false;
        done_sent = false;
        continue;
      }
    }
  }

  printf("[Report] Timed out waiting for master state ACK.\n");
  wifi_enable(false);
  state = State::ERROR;
}
