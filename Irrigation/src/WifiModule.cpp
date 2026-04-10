
// =============================================================================
// WifiModule.cpp — ESP8266 / ESP32 over UART  (Hayes AT command set)
// =============================================================================
#include "Irrigation.hpp"
#include <stdlib.h>

// =============================================================================
// Helpers — internal linkage
// =============================================================================

// Attempt to find `needle` anywhere inside the rolling buffer `buf[0..len)`.
static inline bool buf_contains(const char *buf, size_t len, const char *needle) {
    if (!needle || needle[0] == '\0') return true;
    size_t nlen = strlen(needle);
    if (nlen > len) return false;
    for (size_t i = 0; i <= len - nlen; i++) {
        if (memcmp(buf + i, needle, nlen) == 0) return true;
    }
    return false;
}

// =============================================================================
// Constructor
// =============================================================================
WifiModule::WifiModule(uart_inst_t *uart,
                       uint8_t tx_pin, uint8_t rx_pin,
                       uint32_t baud)
    : _uart(uart), _tx_pin(tx_pin), _rx_pin(rx_pin), _baud(baud),
      _state(DISCONNECTED), _server_port(0),
      _last_tx_ms(0), _sent_ok(false)
{
    memset(_ssid,       0, sizeof(_ssid));
    memset(_password,   0, sizeof(_password));
    memset(_server_url, 0, sizeof(_server_url));
}

// =============================================================================
// Initialisation
// =============================================================================
bool WifiModule::init() {
    uart_init(_uart, _baud);
    gpio_set_function(_tx_pin, GPIO_FUNC_UART);
    gpio_set_function(_rx_pin, GPIO_FUNC_UART);

    // Allow the module time to stabilise
    sleep_ms(500);
    _flush_rx();

    // Basic sanity check
    if (!_at("AT", "OK", 2000)) {
        // Try a reset and one more attempt
        reset();
        sleep_ms(2000);
        if (!_at("AT", "OK", 2000)) {
            _state = ERROR_STATE;
            return false;
        }
    }

    // Disable echo
    _at("ATE0", "OK", 1000);
    // Station mode
    _at("AT+CWMODE=1", "OK", 1000);

    _state = DISCONNECTED;
    return true;
}

// =============================================================================
// Reset (hardware RST not wired — use AT+RST instead)
// =============================================================================
void WifiModule::reset() {
    _flush_rx();
    // Send reset; wait for the "ready" banner
    uart_write_blocking(_uart, (const uint8_t *)"AT+RST\r\n", 8);
    sleep_ms(2000);
    _flush_rx();
    _state = DISCONNECTED;
}

// =============================================================================
// Connect to WiFi network
// =============================================================================
bool WifiModule::connect(const char *ssid, const char *password) {
    strncpy(_ssid,     ssid,     sizeof(_ssid)     - 1);
    strncpy(_password, password, sizeof(_password) - 1);

    // Build:  AT+CWJAP="ssid","password"
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", _ssid, _password);

    _state = CONNECTING;
    if (!_at(cmd, "WIFI GOT IP", 15000)) {
        _state = ERROR_STATE;
        return false;
    }

    _state = CONNECTED;
    return true;
}

void WifiModule::disconnect() {
    _at("AT+CWQAP", "OK", 2000);
    _state = DISCONNECTED;
}

void WifiModule::set_server(const char *url, uint16_t port) {
    strncpy(_server_url, url, sizeof(_server_url) - 1);
    _server_port = port;
}

// =============================================================================
// Send a payload string to the configured server via TCP
// =============================================================================
bool WifiModule::send(const char *payload) {
    if (!payload || payload[0] == '\0') return false;

    // Open TCP connection
    char start_cmd[192];
    snprintf(start_cmd, sizeof(start_cmd),
             "AT+CIPSTART=\"TCP\",\"%s\",%u", _server_url, _server_port);

    if (!_at(start_cmd, "CONNECT", 5000)) {
        // Connection may already be open — try CIPSEND anyway
    }

    size_t len = strlen(payload);
    char cipsend[32];
    snprintf(cipsend, sizeof(cipsend), "AT+CIPSEND=%u", (unsigned)len);

    // Send AT+CIPSEND command, wait for '>' prompt
    if (!_at(cipsend, ">", 3000)) {
        _at("AT+CIPCLOSE", "OK", 1000);
        return false;
    }

    // Send the raw payload
    uart_write_blocking(_uart, (const uint8_t *)payload, len);

    // Wait for "SEND OK"
    char buf[128] = {0};
    _at("", "SEND OK", 5000); // empty cmd — just waiting

    _at("AT+CIPCLOSE", "OK", 1000);
    _sent_ok      = true;
    _last_tx_ms   = to_ms_since_boot(get_absolute_time());
    return true;
}

// =============================================================================
// AP mode for pairing
// =============================================================================
bool WifiModule::start_ap_mode(const char *ap_name) {
    // Switch to AP mode
    if (!_at("AT+CWMODE=2", "OK", 2000)) return false;

    // Configure open AP:  AT+CWSAP="name","",channel,open
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"\",1,0", ap_name);
    if (!_at(cmd, "OK", 3000)) return false;

    // Enable multiple connections and start TCP server on port 8080
    if (!_at("AT+CIPMUX=1",        "OK", 1000)) return false;
    if (!_at("AT+CIPSERVER=1,8080","OK", 1000)) return false;

    return true;
}

// =============================================================================
// wait_for_config — block until master sends a line starting with "CFG:"
//
// Expected format (single line, LF-terminated):
//   CFG:ssid=<ssid>,pass=<pw>,server=<url>,port=<p>,interval=<s>,
//       moisture=<f>,uv=<f>,pump=<ms>,dry=<u>,wet=<u>,tank=<f>
// =============================================================================
bool WifiModule::wait_for_config(Config &out_cfg, uint32_t timeout_ms) {
    uint32_t start = to_ms_since_boot(get_absolute_time());

    char line[320];
    while (true) {
        if (timeout_ms != 0xFFFFFFFFu &&
            (to_ms_since_boot(get_absolute_time()) - start) >= timeout_ms) {
            return false;
        }

        int n = _read_line(line, sizeof(line), 500);
        if (n <= 0) continue;

        // Look for +IPD prefix (incoming TCP data):  +IPD,<id>,<len>:<data>
        const char *data = line;
        const char *ipd  = strstr(line, "+IPD,");
        if (ipd) {
            // Skip past the colon that separates header from payload
            const char *colon = strchr(ipd, ':');
            if (colon) data = colon + 1;
        }

        if (strncmp(data, "CFG:", 4) == 0) {
            if (_parse_config_line(data + 4, out_cfg)) {
                // Acknowledge back to master
                const char *ack = "OK_CFG\n";
                size_t alen = strlen(ack);
                char cipsend[32];
                snprintf(cipsend, sizeof(cipsend), "AT+CIPSEND=0,%u",
                         (unsigned)alen);
                if (_at(cipsend, ">", 2000)) {
                    uart_write_blocking(_uart,
                                        (const uint8_t *)ack, alen);
                    sleep_ms(200);
                }
                return true;
            }
        }
    }
}

// =============================================================================
// Poll for a command line from master (non-blocking with short timeout)
// Returns number of bytes written to buf, or 0 if nothing arrived.
//
// Expected command format arriving via +IPD:
//   PUMP_ON:<ms>
//   PUMP_OFF
//   READ_NOW
//   PAIR
//   RESET_CFG
// =============================================================================
int WifiModule::read_command(char *buf, size_t max_len, uint32_t timeout_ms) {
    char line[256];
    int n = _read_line(line, sizeof(line), timeout_ms);
    if (n <= 0) return 0;

    // Strip +IPD header if present
    const char *data = line;
    const char *ipd  = strstr(line, "+IPD,");
    if (ipd) {
        const char *colon = strchr(ipd, ':');
        if (colon) data = colon + 1;
    }

    int dlen = (int)strlen(data);
    if (dlen <= 0) return 0;

    // Reject CFG lines — they are handled by wait_for_config
    if (strncmp(data, "CFG:", 4) == 0) return 0;

    int copy = dlen < (int)max_len - 1 ? dlen : (int)max_len - 1;
    memcpy(buf, data, (size_t)copy);
    buf[copy] = '\0';
    return copy;
}

// =============================================================================
// Private: send an AT command and scan for `expected` response substring
// =============================================================================
bool WifiModule::_at(const char *cmd, const char *expected, uint32_t timeout_ms) {
    if (cmd && cmd[0] != '\0') {
        size_t clen = strlen(cmd);
        uart_write_blocking(_uart, (const uint8_t *)cmd,   clen);
        uart_write_blocking(_uart, (const uint8_t *)"\r\n", 2);
    }

    if (!expected || expected[0] == '\0') return true;

    // Rolling receive buffer (no heap allocation)
    char    rx[512];
    size_t  rx_idx    = 0;
    uint32_t start    = to_ms_since_boot(get_absolute_time());

    while ((to_ms_since_boot(get_absolute_time()) - start) < timeout_ms) {
        if (uart_is_readable(_uart)) {
            char c = (char)uart_getc(_uart);
            if (rx_idx < sizeof(rx) - 1) rx[rx_idx++] = c;
            rx[rx_idx] = '\0';

            if (buf_contains(rx, rx_idx, expected))  return true;
            if (buf_contains(rx, rx_idx, "ERROR"))   return false;
            if (buf_contains(rx, rx_idx, "FAIL"))    return false;
        }
    }
    return false;
}

// =============================================================================
// Private: read one newline-terminated line within timeout_ms
// =============================================================================
int WifiModule::_read_line(char *buf, size_t max, uint32_t timeout_ms) {
    size_t   idx   = 0;
    uint32_t start = to_ms_since_boot(get_absolute_time());

    while ((to_ms_since_boot(get_absolute_time()) - start) < timeout_ms && idx < max - 1) {
        if (uart_is_readable(_uart)) {
            char c = (char)uart_getc(_uart);
            if (c == '\n') break;
            if (c != '\r') buf[idx++] = c;
        }
    }
    buf[idx] = '\0';
    return (int)idx;
}

// =============================================================================
// Private: drain any bytes sitting in the UART RX FIFO
// =============================================================================
void WifiModule::_flush_rx() {
    while (uart_is_readable(_uart)) {
        (void)uart_getc(_uart);
    }
}

// =============================================================================
// Private: parse a "key=val,key=val,..." config string into a Config struct
// =============================================================================
bool WifiModule::_parse_config_line(const char *line, Config &out) {
    // Work on a mutable copy
    char buf[320];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Strip trailing whitespace / newline
    size_t L = strlen(buf);
    while (L > 0 && (buf[L-1] == '\n' || buf[L-1] == '\r' || buf[L-1] == ' '))
        buf[--L] = '\0';

    memset(&out, 0, sizeof(out));

    // Tokenise by comma
    char *saveptr = nullptr;
    char *token   = strtok_r(buf, ",", &saveptr);
    int   fields  = 0;

    while (token) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq        = '\0';
            const char *key = token;
            const char *val = eq + 1;

            if      (strcmp(key, "ssid")     == 0) { strncpy(out.ssid,       val, 32);  fields++; }
            else if (strcmp(key, "pass")     == 0) { strncpy(out.password,   val, 64);  fields++; }
            else if (strcmp(key, "server")   == 0) { strncpy(out.server_url, val, 128); fields++; }
            else if (strcmp(key, "port")     == 0) { out.server_port           = (uint16_t)atoi(val); fields++; }
            else if (strcmp(key, "interval") == 0) { out.sleep_interval_s      = (uint32_t)atoi(val); fields++; }
            else if (strcmp(key, "moisture") == 0) { out.moisture_threshold_pct = (float)atof(val);   fields++; }
            else if (strcmp(key, "uv")       == 0) { out.uv_alert_threshold     = (float)atof(val);   fields++; }
            else if (strcmp(key, "pump")     == 0) { out.pump_duration_ms       = (uint32_t)atoi(val);fields++; }
            else if (strcmp(key, "dry")      == 0) { out.moisture_dry_val       = (uint16_t)atoi(val);fields++; }
            else if (strcmp(key, "wet")      == 0) { out.moisture_wet_val       = (uint16_t)atoi(val);fields++; }
            else if (strcmp(key, "tank")     == 0) { out.tank_height_cm         = (float)atof(val);   fields++; }
        }
        token = strtok_r(nullptr, ",", &saveptr);
    }

    // Require at minimum ssid, pass, server, port
    return (fields >= 4 && out.ssid[0] != '\0' &&
            out.server_url[0] != '\0' && out.server_port != 0);
}
