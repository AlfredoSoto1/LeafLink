#include "Wifi.hpp"

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include <cstring>
#include <cstdio>

WifiModule::WifiModule(uart_inst_t *uart)
    : m_uart(uart),
      m_state(State::DISCONNECTED),
      m_initialized(false) {}

void WifiModule::power_on() {
  gpio_put(POWER_PIN, 1);
  sleep_ms(500);
}

void WifiModule::power_off() {
  gpio_put(POWER_PIN, 0);
}

bool WifiModule::init() {
  gpio_init(POWER_PIN);
  gpio_set_dir(POWER_PIN, GPIO_OUT);
  gpio_put(POWER_PIN, 0);

  uart_init(m_uart, BAUD_RATE);
  gpio_set_function(TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(RX_PIN, GPIO_FUNC_UART);

  sleep_ms(500);
  flush_rx();

  if (!send_at("AT", "OK", 2000)) {
    reset();
    sleep_ms(2000);
    if (!send_at("AT", "OK", 2000)) {
      m_state = State::ERROR_STATE;
      return false;
    }
  }

  send_at("ATE0", "OK", 1000);        // disable echo
  send_at("AT+CWMODE=1", "OK", 1000); // station mode

  m_initialized = true;
  m_state = State::DISCONNECTED;
  return true;
}

bool WifiModule::connect(const char *ssid, const char *password) {
  if (!m_initialized) return false;

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);

  if (!send_at(cmd, "WIFI CONNECTED", 10000)) {
    m_state = State::ERROR_STATE;
    return false;
  }

  m_state = State::CONNECTED;
  return true;
}

bool WifiModule::send(const char *host, uint16_t port, const char *payload) {
  if (m_state != State::CONNECTED) return false;

  char cmd[128];

  snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", host, (unsigned)port);
  if (!send_at(cmd, "CONNECT", 5000)) return false;

  const size_t len = strlen(payload);
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)len);
  if (!send_at(cmd, ">", 2000)) return false;

  uart_write_blocking(m_uart, reinterpret_cast<const uint8_t *>(payload), len);

  return send_at("", "SEND OK", 5000);
}

void WifiModule::reset() {
  flush_rx();
  uart_write_blocking(m_uart, reinterpret_cast<const uint8_t *>("AT+RST\r\n"), 8);
  sleep_ms(2000);
  flush_rx();
  m_state = State::DISCONNECTED;
}

WifiModule::State WifiModule::get_state() const {
  return m_state;
}

bool WifiModule::is_connected() const {
  return m_state == State::CONNECTED;
}

bool WifiModule::send_at(const char *cmd, const char *expect, uint32_t timeout_ms) {
  if (cmd[0] != '\0') {
    uart_write_blocking(m_uart, reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
    uart_write_blocking(m_uart, reinterpret_cast<const uint8_t *>("\r\n"), 2);
  }

  if (!expect || expect[0] == '\0') return true;

  char   buf[256] = {};
  size_t pos      = 0;
  const uint32_t deadline = to_ms_since_boot(get_absolute_time()) + timeout_ms;

  while (to_ms_since_boot(get_absolute_time()) < deadline) {
    if (uart_is_readable(m_uart)) {
      char c = static_cast<char>(uart_getc(m_uart));
      if (pos < sizeof(buf) - 1) buf[pos++] = c;
      if (strstr(buf, expect)) return true;
    }
  }

  return false;
}

void WifiModule::flush_rx() {
  while (uart_is_readable(m_uart)) {
    uart_getc(m_uart);
  }
}

// ---------------------------------------------------------------------------
// request_config — sends a CONFIG_REQUEST to the master over TCP and parses
// the response into a SystemConfig.
//
// Expected response wire format (all values unsigned integers):
//   CFG:<dry_cal>,<wet_cal>,<threshold*100>,<m_samples>,<m_warmup_ms>,
//       <uv_alert*100>,<uv_samples>,<uv_warmup_ms>,<pump_ms>\r\n
// ---------------------------------------------------------------------------
bool WifiModule::request_config(const char *host, uint16_t port, SystemConfig &out) {
  char cmd[128];

  snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", host, (unsigned)port);
  if (!send_at(cmd, "CONNECT", 5000)) return false;

  const char   *req     = "CONFIG_REQUEST\r\n";
  const size_t  req_len = strlen(req);
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)req_len);
  if (!send_at(cmd, ">", 2000)) {
    send_at("AT+CIPCLOSE", "OK", 1000);
    return false;
  }

  uart_write_blocking(m_uart, reinterpret_cast<const uint8_t *>(req), req_len);
  if (!send_at("", "SEND OK", 3000)) {
    send_at("AT+CIPCLOSE", "OK", 1000);
    return false;
  }

  char response[256] = {};
  const bool got = receive_ipd(response, sizeof(response), 10000);
  send_at("AT+CIPCLOSE", "OK", 1000);

  return got && parse_config(response, out);
}

bool WifiModule::receive_ipd(char *buf, size_t max_len, uint32_t timeout_ms) {
  char   raw[512] = {};
  size_t pos      = 0;

  const uint32_t deadline = to_ms_since_boot(get_absolute_time()) + timeout_ms;

  while (to_ms_since_boot(get_absolute_time()) < deadline) {
    if (!uart_is_readable(m_uart)) continue;

    const char c = static_cast<char>(uart_getc(m_uart));
    if (pos < sizeof(raw) - 1) raw[pos++] = c;

    // +IPD,<len>:<data>
    const char *colon = strstr(raw, "+IPD,");
    if (!colon) continue;

    const char *sep = strchr(colon, ':');
    if (!sep) continue;

    strncpy(buf, sep + 1, max_len - 1);
    buf[max_len - 1] = '\0';
    return true;
  }

  return false;
}

bool WifiModule::parse_config(const char *data, SystemConfig &out) {
  if (strncmp(data, "CFG:", 4) != 0) return false;

  uint16_t threshold_x100  = 0;
  uint16_t uv_alert_x100   = 0;
  uint16_t pwr_vmax_x100   = 0;
  uint16_t pwr_vmin_x100   = 0;
  uint16_t pwr_div_x1000   = 0;

  const int n = sscanf(data + 4,
    "%hu,%hu,%hu,%u,%u,"    // moisture (5)
    "%hu,%u,%u,%u,"         // uv + pump (4)
    "%hu,%hu,%u,%u,%u,"     // water level (5)
    "%hu,%hu,%hu,%u",       // power (4)
    &out.moisture_dry_cal,
    &out.moisture_wet_cal,
    &threshold_x100,
    &out.moisture_sample_count,
    &out.moisture_warmup_ms,
    &uv_alert_x100,
    &out.uv_sample_count,
    &out.uv_warmup_ms,
    &out.pump_run_duration_ms,
    &out.water_dry_cal,
    &out.water_wet_cal,
    &out.water_sample_count,
    &out.water_warmup_ms,
    &out.water_tank_oz,
    &pwr_vmax_x100,
    &pwr_vmin_x100,
    &pwr_div_x1000,
    &out.power_sample_count);

  if (n != 18) return false;

  out.moisture_threshold_pct = threshold_x100 / 100.0f;
  out.uv_alert_threshold     = uv_alert_x100  / 100.0f;
  out.power_v_max            = pwr_vmax_x100  / 100.0f;
  out.power_v_min            = pwr_vmin_x100  / 100.0f;
  out.power_divider_ratio    = pwr_div_x1000  / 1000.0f;
  return true;
}


