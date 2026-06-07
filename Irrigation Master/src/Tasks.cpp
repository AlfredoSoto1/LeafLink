#include "Tasks.hpp"

#include "AppContext.hpp"
#include "Dashboard.hpp"
#include "EventQueue.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <cstring>

extern AppContext gCtx;
extern const char DASHBOARD_HTML[] PROGMEM;

namespace {

enum class PicoServiceRequest {
  Unknown,
  NeedsConfig,
  SendingStates,
};

bool readClientLine(WiFiClient &client, String &out, uint32_t timeout_ms);
bool parseStateText(const String &text, IrrigationNodeState &state);
PicoServiceRequest askPicoService(WiFiClient &client, String *incoming, uint32_t timeout_ms);

void copyText(char *dst, size_t dst_len, const char *src) {
  if (dst_len == 0) return;
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dst_len - 1);
  dst[dst_len - 1] = '\0';
}

void setPicoStaStatus(AppContext &ctx, const char *status) {
  copyText(ctx.picoStaStatus, sizeof(ctx.picoStaStatus), status);
}

bool picoStaIsReady(const AppContext &ctx) {
  return ctx.picoStaConnected &&
         WiFi.status() == WL_CONNECTED &&
         WiFi.SSID() == String(ctx.picoPairSSID);
}

float clampPercent(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 100.0f) return 100.0f;
  return value;
}

float waterPercent(const IrrigationNode &node) {
  float capacity = node.config.water.tank_capacity_oz;
  if (capacity <= 0.0f) return 0.0f;
  return clampPercent((node.state.water.ounces_remaining / capacity) * 100.0f);
}

void syncMasterWifiConfig(AppContext &ctx) {
  copyText(ctx.node.config.wifi.ap_ssid, sizeof(ctx.node.config.wifi.ap_ssid), ctx.apSSID);
  copyText(ctx.node.config.wifi.ap_password, sizeof(ctx.node.config.wifi.ap_password), ctx.apPassword);
  copyText(ctx.node.config.wifi.master_host, sizeof(ctx.node.config.wifi.master_host), ctx.apIP);
  ctx.node.config.wifi.tcp_port = AppContext::PICO_TCP_PORT;
  ctx.node.config.sleep.active = false;
}

bool startAccessPoint(AppContext &ctx) {
  IPAddress local;
  IPAddress gateway;
  IPAddress subnet(255, 255, 255, 0);

  local.fromString(ctx.apIP);
  gateway.fromString(ctx.apIP);

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAPConfig(local, gateway, subnet);

  bool ok = WiFi.softAP(ctx.apSSID, ctx.apPassword);
  delay(150);

  Serial.printf("[AP] SSID=%s IP=%s %s\n",
                ctx.apSSID,
                WiFi.softAPIP().toString().c_str(),
                ok ? "ready" : "failed");
  Serial.printf("[AP] Open web app at http://%s\n", ctx.apIP);

  return ok;
}

void startPicoReportServer(AppContext &ctx) {
  ctx.picoServer.begin();
  ctx.picoServer.setNoDelay(true);
  ctx.picoServerStarted = true;
  Serial.printf("[PicoTCP] Listening for reports on port %u\n", AppContext::PICO_TCP_PORT);
}

String statusToJson(const AppContext &ctx) {
  StaticJsonDocument<1024> doc;
  const IrrigationNode &node = ctx.node;

  doc["connected"] = ctx.picoConnected;
  doc["last_update_ms"] = ctx.lastStateMs;
  doc["pico_wifi_connected"] = picoStaIsReady(ctx);
  doc["pico_wifi_connecting"] = ctx.picoStaConnecting;
  doc["pico_wifi_status"] = ctx.picoStaStatus;
  doc["pico_needs_config"] = ctx.picoNeedsConfig;
  doc["selected_ssid"] = ctx.picoPairSSID;

  JsonObject moisture = doc.createNestedObject("moisture");
  moisture["percent"] = node.state.soilMoisture.moisture_percent;
  moisture["needs_water"] = node.state.soilMoisture.is_dry;
  moisture["error"] = node.state.soilMoisture.error;

  JsonObject uv = doc.createNestedObject("uv");
  uv["uv_index"] = node.state.uv.uv_index;
  uv["alert"] = node.state.uv.is_alert;
  uv["error"] = node.state.uv.error;

  JsonObject water = doc.createNestedObject("water");
  water["oz"] = node.state.water.ounces_remaining;
  water["percent"] = waterPercent(node);
  water["error"] = node.state.water.error;

  JsonObject power = doc.createNestedObject("power");
  power["voltage"] = node.state.power.voltage;
  power["percent"] = node.state.power.percentage;
  power["warning"] = node.state.power.warning;
  power["error"] = node.state.power.error;

  JsonObject pump = doc.createNestedObject("pump");
  pump["running"] = node.state.pump.running;
  pump["duration_ms"] = node.state.pump.duration_ms;
  pump["started_at_ms"] = node.state.pump.started_at_ms;
  pump["total_oz_dispensed"] = node.state.pump.total_oz_dispensed;

  doc["pump_active"] = node.state.pump.running;

  String out;
  serializeJson(doc, out);
  return out;
}

String configToJson(const AppContext &ctx) {
  StaticJsonDocument<1024> doc;
  const IrrigationNodeConfig &config = ctx.node.config;

  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["ap_ssid"] = config.wifi.ap_ssid;
  wifi["master_host"] = config.wifi.master_host;
  wifi["tcp_port"] = config.wifi.tcp_port;

  JsonObject pairing = doc.createNestedObject("pairing");
  pairing["ssid"] = ctx.picoPairSSID;
  pairing["host"] = ctx.picoPairHost;
  pairing["tcp_port"] = AppContext::PICO_TCP_PORT;

  JsonObject pump = doc.createNestedObject("pump");
  pump["target_oz_per_day"] = config.pump.target_oz_per_day;
  pump["flow_rate_oz_per_sec"] = config.pump.flow_rate_oz_per_sec;

  JsonObject uv = doc.createNestedObject("uv");
  uv["alert_threshold"] = config.uv.alert_threshold;
  uv["min_voltage"] = config.uv.min_voltage;
  uv["max_voltage"] = config.uv.max_voltage;
  uv["max_uv_index"] = config.uv.max_uv_index;

  JsonObject power = doc.createNestedObject("power");
  power["warmup_ms"] = config.power.warmup_ms;
  power["v_max"] = config.power.v_max;
  power["v_min"] = config.power.v_min;
  power["divider_ratio"] = config.power.divider_ratio;

  JsonObject water = doc.createNestedObject("water");
  water["warmup_ms"] = config.water.warmup_ms;
  water["tank_capacity_oz"] = config.water.tank_capacity_oz;
  water["tank_min_threshold_percent"] = config.water.tank_min_threshold_percent;

  JsonObject soil = doc.createNestedObject("soil_moisture");
  soil["warmup_ms"] = config.soilMoisture.warmup_ms;
  soil["threshold_percent"] = config.soilMoisture.threshold_percent;
  soil["dry_cal"] = config.soilMoisture.dry_cal;
  soil["wet_cal"] = config.soilMoisture.wet_cal;

  JsonObject sleep = doc.createNestedObject("sleep");
  sleep["active"] = config.sleep.active;
  sleep["sleep_interval_ms"] = config.sleep.sleep_interval_ms;

  String out;
  serializeJson(doc, out);
  return out;
}

String configToWireText(const AppContext &ctx) {
  const IrrigationNodeConfig &config = ctx.node.config;
  String out;
  out.reserve(640);
  out += "wifi.ap_ssid=" + String(config.wifi.ap_ssid);
  out += ",wifi.ap_password=" + String(config.wifi.ap_password);
  out += ",wifi.master_host=" + String(config.wifi.master_host);
  out += ",wifi.tcp_port=" + String(config.wifi.tcp_port);
  out += ",pump.target_oz_per_day=" + String(config.pump.target_oz_per_day, 3);
  out += ",pump.flow_rate_oz_per_sec=" + String(config.pump.flow_rate_oz_per_sec, 3);
  out += ",uv.alert_threshold=" + String(config.uv.alert_threshold, 3);
  out += ",uv.min_voltage=" + String(config.uv.min_voltage, 3);
  out += ",uv.max_voltage=" + String(config.uv.max_voltage, 3);
  out += ",uv.max_uv_index=" + String(config.uv.max_uv_index, 3);
  out += ",power.warmup_ms=" + String(config.power.warmup_ms);
  out += ",power.v_max=" + String(config.power.v_max, 3);
  out += ",power.v_min=" + String(config.power.v_min, 3);
  out += ",power.divider_ratio=" + String(config.power.divider_ratio, 3);
  out += ",water.warmup_ms=" + String(config.water.warmup_ms);
  out += ",water.tank_capacity_oz=" + String(config.water.tank_capacity_oz, 3);
  out += ",water.tank_min_threshold_percent=" + String(config.water.tank_min_threshold_percent, 3);
  out += ",soil_moisture.warmup_ms=" + String(config.soilMoisture.warmup_ms);
  out += ",soil_moisture.threshold_percent=" + String(config.soilMoisture.threshold_percent, 3);
  out += ",soil_moisture.dry_cal=" + String(config.soilMoisture.dry_cal);
  out += ",soil_moisture.wet_cal=" + String(config.soilMoisture.wet_cal);
  out += ",sleep.active=" + String(config.sleep.active ? 1 : 0);
  out += ",sleep.sleep_interval_ms=" + String(config.sleep.sleep_interval_ms);
  out += "\n";
  return out;
}

void printConfigForSerial(const char *prefix, const IrrigationNodeConfig &config) {
  Serial.printf("%s wifi.ap_ssid=%s\n", prefix, config.wifi.ap_ssid);
  Serial.printf("%s wifi.ap_password=%s\n", prefix, config.wifi.ap_password);
  Serial.printf("%s wifi.master_host=%s\n", prefix, config.wifi.master_host);
  Serial.printf("%s wifi.tcp_port=%u\n", prefix, config.wifi.tcp_port);
  Serial.printf("%s pump.target_oz_per_day=%.3f\n", prefix, config.pump.target_oz_per_day);
  Serial.printf("%s pump.flow_rate_oz_per_sec=%.3f\n", prefix, config.pump.flow_rate_oz_per_sec);
  Serial.printf("%s uv.alert_threshold=%.3f\n", prefix, config.uv.alert_threshold);
  Serial.printf("%s uv.min_voltage=%.3f\n", prefix, config.uv.min_voltage);
  Serial.printf("%s uv.max_voltage=%.3f\n", prefix, config.uv.max_voltage);
  Serial.printf("%s uv.max_uv_index=%.3f\n", prefix, config.uv.max_uv_index);
  Serial.printf("%s power.warmup_ms=%u\n", prefix, config.power.warmup_ms);
  Serial.printf("%s power.v_max=%.3f\n", prefix, config.power.v_max);
  Serial.printf("%s power.v_min=%.3f\n", prefix, config.power.v_min);
  Serial.printf("%s power.divider_ratio=%.3f\n", prefix, config.power.divider_ratio);
  Serial.printf("%s water.warmup_ms=%u\n", prefix, config.water.warmup_ms);
  Serial.printf("%s water.tank_capacity_oz=%.3f\n", prefix, config.water.tank_capacity_oz);
  Serial.printf("%s water.tank_min_threshold_percent=%.3f\n",
                prefix,
                config.water.tank_min_threshold_percent);
  Serial.printf("%s soil_moisture.warmup_ms=%u\n", prefix, config.soilMoisture.warmup_ms);
  Serial.printf("%s soil_moisture.threshold_percent=%.3f\n",
                prefix,
                config.soilMoisture.threshold_percent);
  Serial.printf("%s soil_moisture.dry_cal=%u\n", prefix, config.soilMoisture.dry_cal);
  Serial.printf("%s soil_moisture.wet_cal=%u\n", prefix, config.soilMoisture.wet_cal);
  Serial.printf("%s sleep.active=%s\n", prefix, config.sleep.active ? "true" : "false");
  Serial.printf("%s sleep.sleep_interval_ms=%u\n", prefix, config.sleep.sleep_interval_ms);
}

template <typename T>
void updateNumber(JsonObject obj, const char *name, T &target) {
  if (!obj[name].isNull()) {
    target = static_cast<T>(obj[name].as<double>());
  }
}

void applyConfigJson(AppContext &ctx, JsonObject doc) {
  if (doc["pairing"].is<JsonObject>()) {
    JsonObject pairing = doc["pairing"].as<JsonObject>();
    if (!pairing["ssid"].isNull()) {
      copyText(ctx.picoPairSSID, sizeof(ctx.picoPairSSID), pairing["ssid"] | "PICO_PAIR_DEVICE");
    }
    if (!pairing["password"].isNull()) {
      copyText(ctx.picoPairPassword, sizeof(ctx.picoPairPassword), pairing["password"] | "12345678");
    }
  }

  if (doc["pump"].is<JsonObject>()) {
    JsonObject pump = doc["pump"].as<JsonObject>();
    updateNumber(pump, "target_oz_per_day", ctx.node.config.pump.target_oz_per_day);
    updateNumber(pump, "flow_rate_oz_per_sec", ctx.node.config.pump.flow_rate_oz_per_sec);
  }

  if (doc["uv"].is<JsonObject>()) {
    JsonObject uv = doc["uv"].as<JsonObject>();
    updateNumber(uv, "alert_threshold", ctx.node.config.uv.alert_threshold);
    updateNumber(uv, "min_voltage", ctx.node.config.uv.min_voltage);
    updateNumber(uv, "max_voltage", ctx.node.config.uv.max_voltage);
    updateNumber(uv, "max_uv_index", ctx.node.config.uv.max_uv_index);
  }

  if (doc["power"].is<JsonObject>()) {
    JsonObject power = doc["power"].as<JsonObject>();
    updateNumber(power, "warmup_ms", ctx.node.config.power.warmup_ms);
    updateNumber(power, "v_max", ctx.node.config.power.v_max);
    updateNumber(power, "v_min", ctx.node.config.power.v_min);
    updateNumber(power, "divider_ratio", ctx.node.config.power.divider_ratio);
  }

  if (doc["water"].is<JsonObject>()) {
    JsonObject water = doc["water"].as<JsonObject>();
    updateNumber(water, "warmup_ms", ctx.node.config.water.warmup_ms);
    updateNumber(water, "tank_capacity_oz", ctx.node.config.water.tank_capacity_oz);
    updateNumber(water, "tank_min_threshold_percent", ctx.node.config.water.tank_min_threshold_percent);
  }

  if (doc["soil_moisture"].is<JsonObject>()) {
    JsonObject soil = doc["soil_moisture"].as<JsonObject>();
    updateNumber(soil, "warmup_ms", ctx.node.config.soilMoisture.warmup_ms);
    updateNumber(soil, "threshold_percent", ctx.node.config.soilMoisture.threshold_percent);
    updateNumber(soil, "dry_cal", ctx.node.config.soilMoisture.dry_cal);
    updateNumber(soil, "wet_cal", ctx.node.config.soilMoisture.wet_cal);
  }

  if (doc["sleep"].is<JsonObject>()) {
    JsonObject sleep = doc["sleep"].as<JsonObject>();
    updateNumber(sleep, "sleep_interval_ms", ctx.node.config.sleep.sleep_interval_ms);
  }

  if (!doc["sleep_interval_ms"].isNull()) {
    ctx.node.config.sleep.sleep_interval_ms = doc["sleep_interval_ms"].as<uint32_t>();
  }

  syncMasterWifiConfig(ctx);
}

bool isPicoPairingSsid(const String &ssid) {
  return ssid == "PICO_PAIR_DEVICE" ||
         ssid.indexOf("PICO") >= 0 ||
         ssid.indexOf("Pico") >= 0 ||
         ssid.indexOf("pico") >= 0 ||
         ssid.indexOf("Irrigation") >= 0;
}

void finishPicoScan(AppContext &ctx, int network_count) {
  ctx.picoNetworkCount = 0;

  for (int i = 0; i < network_count && ctx.picoNetworkCount < AppContext::MAX_PICO_NETWORKS; ++i) {
    String ssid = WiFi.SSID(i);
    if (!isPicoPairingSsid(ssid)) continue;

    AppContext::PicoNetwork &network = ctx.picoNetworks[ctx.picoNetworkCount++];
    copyText(network.ssid, sizeof(network.ssid), ssid.c_str());
    network.rssi = WiFi.RSSI(i);
    network.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }

  WiFi.scanDelete();
  ctx.picoScanInProgress = false;
  ctx.picoScanCompletedMs = millis();

  Serial.printf("[Scan] Complete. Found %u Pico candidate(s).\n", ctx.picoNetworkCount);
  for (uint8_t i = 0; i < ctx.picoNetworkCount; ++i) {
    Serial.printf("[Scan]   %s RSSI=%ld secure=%s\n",
                  ctx.picoNetworks[i].ssid,
                  static_cast<long>(ctx.picoNetworks[i].rssi),
                  ctx.picoNetworks[i].secure ? "yes" : "no");
  }

  if (ctx.picoNetworkCount > 0) {
    bool selected_visible = false;
    for (uint8_t i = 0; i < ctx.picoNetworkCount; ++i) {
      if (strcmp(ctx.picoNetworks[i].ssid, ctx.picoPairSSID) == 0) {
        selected_visible = true;
        break;
      }
    }

    if (!selected_visible || ctx.picoPairSSID[0] == '\0') {
      copyText(ctx.picoPairSSID, sizeof(ctx.picoPairSSID), ctx.picoNetworks[0].ssid);
      Serial.printf("[Pairing] Auto-selected Pico SSID '%s'.\n", ctx.picoPairSSID);
    }

    ctx.picoStaWanted = true;
    if (!picoStaIsReady(ctx)) {
      setPicoStaStatus(ctx, "Pico found. Connecting...");
    }
  } else {
    ctx.picoStaWanted = false;
    ctx.picoStaConnecting = false;
    ctx.picoStaConnected = false;
    setPicoStaStatus(ctx, "No Pico found");
  }
}

void pollPicoScan(AppContext &ctx) {
  if (!ctx.picoScanInProgress) return;

  int result = WiFi.scanComplete();
  if (result >= 0) {
    finishPicoScan(ctx, result);
    return;
  }

  if (millis() - ctx.picoScanStartedMs > 12000) {
    WiFi.scanDelete();
    ctx.picoScanInProgress = false;
    Serial.println("[Scan] Timed out.");
  }
}

void startPicoScan(AppContext &ctx) {
  pollPicoScan(ctx);
  if (ctx.picoScanInProgress) return;
  if (ctx.picoStaConnecting || picoStaIsReady(ctx)) {
    return;
  }

  WiFi.scanDelete();
  int result = WiFi.scanNetworks(true, true);
  ctx.picoScanInProgress = true;
  ctx.picoScanStartedMs = millis();
  Serial.printf("[Scan] Started async Pico scan (result=%d).\n", result);
}

void beginPicoStaConnection(AppContext &ctx) {
  if (ctx.picoPairSSID[0] == '\0') return;

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.begin(ctx.picoPairSSID, ctx.picoPairPassword);

  ctx.picoStaConnecting = true;
  ctx.picoStaConnected = false;
  ctx.picoStaConnectStartedMs = millis();
  ctx.picoStaLastAttemptMs = millis();
  setPicoStaStatus(ctx, "Connecting to Pico AP...");

  Serial.printf("[Pairing] Connecting ESP32 STA to Pico AP '%s' while keeping web AP online.\n",
                ctx.picoPairSSID);
}

bool collectPicoStates(AppContext &ctx) {
  IPAddress host;
  host.fromString(ctx.picoPairHost);

  WiFiClient client;
  if (!client.connect(host, AppContext::PICO_TCP_PORT)) {
    return false;
  }

  client.setNoDelay(true);

  String greeting;
  PicoServiceRequest request = askPicoService(client, &greeting, 5000);
  if (request == PicoServiceRequest::NeedsConfig) {
    ctx.picoNeedsConfig = true;
    setPicoStaStatus(ctx, "Pico is waiting for configuration");
    Serial.println("[Pairing] Pico requested configuration. Waiting for user to send config.");
    client.stop();
    return false;
  }
  if (request != PicoServiceRequest::SendingStates) {
    Serial.printf("[Report] Pico service role was not understood: %s\n", greeting.c_str());
    client.stop();
    return false;
  }

  ctx.picoNeedsConfig = false;
  Serial.println("[Report] Pico is ready to send states.");
  client.print("READY_STATES\n");
  client.flush();
  delay(200);

  String states_text;
  if (!readClientLine(client, states_text, 10000)) {
    Serial.println("[Report] Timed out waiting for Pico states text.");
    client.stop();
    return false;
  }

  IrrigationNodeState next_state = ctx.node.state;
  if (!parseStateText(states_text, next_state)) {
    Serial.printf("[Report] Could not parse Pico states: %s\n", states_text.c_str());
    client.print("STATES_ERR\n");
    client.flush();
    client.stop();
    return false;
  }

  client.print("STATES_OK\n");
  client.flush();

  String done;
  if (readClientLine(client, done, 3000) && done.indexOf("PICO_DONE") >= 0) {
    Serial.println("[Report] Pico confirmed report transfer complete.");
  } else {
    Serial.printf("[Report] Pico did not send PICO_DONE before close. Received: %s\n",
                  done.c_str());
  }
  delay(100);
  client.stop();

  ctx.events.push(Event::fromState(next_state, nullptr, false, millis()));
  ctx.picoStaWanted = false;
  ctx.picoStaConnecting = false;
  ctx.picoStaConnected = false;
  setPicoStaStatus(ctx, "States received. Waiting for next Pico wake.");
  Serial.printf("[Report] States received from Pico: %s\n", states_text.c_str());
  return true;
}

String availableNodesToJson(AppContext &ctx, bool refresh) {
  if (refresh && !ctx.picoStaConnecting && !picoStaIsReady(ctx)) {
    startPicoScan(ctx);
  } else {
    pollPicoScan(ctx);
  }

  StaticJsonDocument<1536> doc;
  JsonArray nodes = doc.createNestedArray("nodes");

  for (uint8_t i = 0; i < ctx.picoNetworkCount; ++i) {
    JsonObject node = nodes.createNestedObject();
    bool selected = strcmp(ctx.picoNetworks[i].ssid, ctx.picoPairSSID) == 0;
    node["ssid"] = ctx.picoNetworks[i].ssid;
    node["rssi"] = ctx.picoNetworks[i].rssi;
    node["secure"] = ctx.picoNetworks[i].secure;
    node["selected"] = selected;
    node["connected"] = picoStaIsReady(ctx) && selected;
    node["connecting"] = ctx.picoStaConnecting && selected;
    node["needs_config"] = ctx.picoNeedsConfig && selected;
  }

  if (ctx.picoNetworkCount == 0 &&
      ctx.picoPairSSID[0] != '\0' &&
      (ctx.picoStaWanted || ctx.picoStaConnecting || picoStaIsReady(ctx))) {
    JsonObject node = nodes.createNestedObject();
    node["ssid"] = ctx.picoPairSSID;
    node["rssi"] = 0;
    node["secure"] = true;
    node["selected"] = true;
    node["connected"] = picoStaIsReady(ctx);
    node["connecting"] = ctx.picoStaConnecting;
    node["needs_config"] = ctx.picoNeedsConfig;
  }

  doc["count"] = nodes.size();
  doc["scanning"] = ctx.picoScanInProgress;
  doc["last_scan_ms"] = ctx.picoScanCompletedMs;
  doc["selected_ssid"] = ctx.picoPairSSID;
  doc["pico_wifi_connected"] = picoStaIsReady(ctx);
  doc["pico_wifi_connecting"] = ctx.picoStaConnecting;
  doc["pico_wifi_status"] = ctx.picoStaStatus;
  doc["pico_needs_config"] = ctx.picoNeedsConfig;

  String out;
  serializeJson(doc, out);
  return out;
}

bool extractHeaderLine(const String &header, const char *key, char *out, size_t out_len) {
  int start = header.indexOf(key);
  if (start < 0) return false;

  start += strlen(key);
  int end = header.indexOf('\n', start);
  if (end < 0) end = header.length();

  String value = header.substring(start, end);
  value.trim();
  copyText(out, out_len, value.c_str());
  return true;
}

void pushAlert(AppContext &ctx, const char *message, bool is_error = false) {
  ctx.events.push(Event::fromAlert(message, is_error, millis()));
}

void setupWebSocket(AppContext &ctx) {
  ctx.ws.onEvent([](AsyncWebSocket *server,
                    AsyncWebSocketClient *client,
                    AwsEventType type,
                    void *arg,
                    uint8_t *data,
                    size_t len) {
    (void)server;

    if (type == WS_EVT_CONNECT) {
      Event event = Event::make(EventType::WsClientConnected, millis());
      event.client_id = client->id();
      gCtx.events.push(event);
      return;
    }

    if (type == WS_EVT_DISCONNECT) {
      Event event = Event::make(EventType::WsClientDisconnected, millis());
      event.client_id = client->id();
      gCtx.events.push(event);
      return;
    }

    if (type != WS_EVT_DATA) return;

    AwsFrameInfo *info = reinterpret_cast<AwsFrameInfo *>(arg);
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) {
      pushAlert(gCtx, "Unsupported WebSocket message", true);
      return;
    }

    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
      pushAlert(gCtx, "Invalid WebSocket JSON", true);
      return;
    }

    const char *messageType = doc["type"] | "";
    if (strcmp(messageType, "pair") == 0) {
      if (!picoStaIsReady(gCtx) || !gCtx.picoNeedsConfig) {
        pushAlert(gCtx, "Pico is not ready for configuration yet", true);
        return;
      }
      gCtx.events.push(Event::make(EventType::SendConfigToPico, millis()));
      return;
    }

    if (strcmp(messageType, "config") == 0) {
      applyConfigJson(gCtx, doc.as<JsonObject>());
      if (!picoStaIsReady(gCtx) || !gCtx.picoNeedsConfig) {
        pushAlert(gCtx, "Pico is not ready for configuration yet", true);
        return;
      }
      gCtx.events.push(Event::make(EventType::SendConfigToPico, millis()));
      return;
    }
  });

  ctx.server.addHandler(&ctx.ws);
}

void setupRoutes(AppContext &ctx) {
  ctx.server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", DASHBOARD_HTML);
  });

  ctx.server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", DASHBOARD_HTML);
  });

  ctx.server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "LeafLink");
  });

  ctx.server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "LeafLink");
  });

  ctx.server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", statusToJson(gCtx));
  });

  ctx.server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", configToJson(gCtx));
  });

  ctx.server.on("/api/nodes", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool refresh = request->hasParam("refresh");
    request->send(200, "application/json", availableNodesToJson(gCtx, refresh));
  });

  ctx.server.on("/api/select-node", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid")) {
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"missing ssid\"}");
      return;
    }

    String ssid = request->getParam("ssid")->value();
    copyText(gCtx.picoPairSSID, sizeof(gCtx.picoPairSSID), ssid.c_str());
    gCtx.picoStaWanted = true;
    gCtx.picoStaConnected = false;
    gCtx.picoStaConnecting = false;
    gCtx.picoNeedsConfig = false;
    setPicoStaStatus(gCtx, "Selected Pico. Connecting...");
    request->send(200, "application/json", "{\"ok\":true}");
  });

  ctx.server.on("/api/pair", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!picoStaIsReady(gCtx)) {
      request->send(409, "application/json",
                    "{\"ok\":false,\"error\":\"pico wifi not connected\"}");
      return;
    }
    if (!gCtx.picoNeedsConfig) {
      request->send(409, "application/json",
                    "{\"ok\":false,\"error\":\"pico has not requested configuration yet\"}");
      return;
    }
    gCtx.events.push(Event::make(EventType::SendConfigToPico, millis()));
    request->send(202, "application/json", "{\"ok\":true,\"queued\":true}");
  });

  ctx.server.on(
    "/api/config",
    HTTP_POST,
    [](AsyncWebServerRequest *request) {
      (void)request;
    },
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index != 0 || len != total) {
        request->send(413, "application/json", "{\"ok\":false,\"error\":\"body too large\"}");
        return;
      }

      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error) {
        request->send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
        return;
      }

      applyConfigJson(gCtx, doc.as<JsonObject>());
      gCtx.picoStaWanted = true;

      if (!picoStaIsReady(gCtx)) {
        gCtx.picoStaConnected = false;
        gCtx.picoStaConnecting = false;
        setPicoStaStatus(gCtx, "Selected Pico is not connected yet");
        request->send(409, "application/json",
                      "{\"ok\":false,\"error\":\"ESP32 has not connected to the selected Pico yet\"}");
        return;
      }

      if (!gCtx.picoNeedsConfig) {
        setPicoStaStatus(gCtx, "Waiting for Pico to request configuration");
        request->send(409, "application/json",
                      "{\"ok\":false,\"error\":\"Pico has not requested configuration yet\"}");
        return;
      }

      gCtx.events.push(Event::make(EventType::SendConfigToPico, millis()));
      request->send(200, "application/json", "{\"ok\":true,\"queued\":true}");
    });

  ctx.server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(200, "text/html", DASHBOARD_HTML);
  });
}

bool waitForStationConnection(uint32_t timeout_ms) {
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout_ms) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool waitForPicoConfigAck(WiFiClient &client, uint32_t timeout_ms) {
  String incoming;
  uint32_t start = millis();

  while ((client.connected() || client.available()) && millis() - start < timeout_ms) {
    while (client.available()) {
      char c = static_cast<char>(client.read());
      incoming += c;
      if (incoming.indexOf("CONFIG_OK") >= 0) {
        Serial.printf("[Pairing] Pico config ACK received: %s\n", incoming.c_str());
        return true;
      }
      if (incoming.indexOf("CONFIG_ERR") >= 0) {
        Serial.printf("[Pairing] Pico config error ACK received: %s\n", incoming.c_str());
        return false;
      }
    }
    delay(10);
  }

  Serial.printf("[Pairing] Timed out waiting for Pico config ACK. Received: %s\n",
                incoming.c_str());
  return false;
}

PicoServiceRequest askPicoService(WiFiClient &client, String *incoming, uint32_t timeout_ms) {
  String buffer;
  uint32_t start = millis();

  client.print("MASTER_QUERY\n");
  client.flush();

  while ((client.connected() || client.available()) && millis() - start < timeout_ms) {
    while (client.available()) {
      char c = static_cast<char>(client.read());
      buffer += c;
      if (buffer.indexOf("NEED_CONFIG") >= 0 || buffer.indexOf("WAITING_CONFIG") >= 0) {
        if (incoming != nullptr) *incoming = buffer;
        Serial.printf("[Pairing] Pico service request: NEED_CONFIG (%s)\n", buffer.c_str());
        return PicoServiceRequest::NeedsConfig;
      }
      if (buffer.indexOf("SENDING_STATES") >= 0) {
        if (incoming != nullptr) *incoming = buffer;
        Serial.printf("[Report] Pico service request: SENDING_STATES (%s)\n", buffer.c_str());
        return PicoServiceRequest::SendingStates;
      }
    }
    delay(10);
  }

  if (incoming != nullptr) *incoming = buffer;
  Serial.printf("[Pairing] Timed out waiting for Pico service request. Received: %s\n",
                buffer.c_str());
  return PicoServiceRequest::Unknown;
}

bool waitForPicoWaitingConfig(WiFiClient &client, uint32_t timeout_ms) {
  String incoming;
  return askPicoService(client, &incoming, timeout_ms) == PicoServiceRequest::NeedsConfig;
}

bool readClientLine(WiFiClient &client, String &out, uint32_t timeout_ms) {
  uint32_t start = millis();
  uint32_t last_byte = millis();

  while ((client.connected() || client.available()) && millis() - start < timeout_ms) {
    while (client.available()) {
      char c = static_cast<char>(client.read());
      out += c;
      last_byte = millis();
      if (c == '\n') return true;
    }
    if (out.length() > 0 && millis() - last_byte > 1000) return true;
    delay(5);
  }

  return out.length() > 0;
}

bool textBool(const String &value) {
  return value == "1" || value == "true" || value == "TRUE" || value == "yes";
}

void applyStatePair(IrrigationNodeState &state, const String &key, const String &value) {
  if (key == "pump.running") state.pump.running = textBool(value);
  else if (key == "pump.duration_ms") state.pump.duration_ms = value.toInt();
  else if (key == "pump.started_at_ms") state.pump.started_at_ms = value.toInt();
  else if (key == "pump.total_oz_dispensed") state.pump.total_oz_dispensed = value.toFloat();
  else if (key == "uv.uv_index") state.uv.uv_index = value.toFloat();
  else if (key == "uv.is_alert") state.uv.is_alert = textBool(value);
  else if (key == "uv.error") state.uv.error = textBool(value);
  else if (key == "power.voltage") state.power.voltage = value.toFloat();
  else if (key == "power.percentage") state.power.percentage = value.toFloat();
  else if (key == "power.warning") state.power.warning = textBool(value);
  else if (key == "power.error") state.power.error = textBool(value);
  else if (key == "water.ounces_remaining") state.water.ounces_remaining = value.toFloat();
  else if (key == "water.error") state.water.error = textBool(value);
  else if (key == "soil_moisture.moisture_percent") state.soilMoisture.moisture_percent = value.toFloat();
  else if (key == "soil_moisture.is_dry") state.soilMoisture.is_dry = textBool(value);
  else if (key == "soil_moisture.error") state.soilMoisture.error = textBool(value);
}

bool parseStateText(const String &text, IrrigationNodeState &state) {
  size_t start = 0;
  bool saw_state = false;

  while (start < text.length()) {
    int end = text.indexOf(',', start);
    if (end < 0) end = text.length();

    String pair = text.substring(start, end);
    pair.trim();
    int equals = pair.indexOf('=');
    if (equals > 0) {
      String key = pair.substring(0, equals);
      String value = pair.substring(equals + 1);
      key.trim();
      value.trim();
      applyStatePair(state, key, value);
      if (key == "soil_moisture.moisture_percent" ||
          key == "water.ounces_remaining" ||
          key == "power.voltage") {
        saw_state = true;
      }
    }

    start = end + 1;
  }

  return saw_state;
}

size_t writeClientThrottled(WiFiClient &client, const char *data, size_t length) {
  static constexpr size_t CHUNK_SIZE = 48;
  size_t total = 0;

  while (total < length && client.connected()) {
    size_t chunk = length - total;
    if (chunk > CHUNK_SIZE) chunk = CHUNK_SIZE;

    size_t written = client.write(reinterpret_cast<const uint8_t *>(data + total), chunk);
    client.flush();
    total += written;

    if (written != chunk) break;
    delay(35);
  }

  return total;
}

} // namespace

void Tasks::init(AppContext &ctx) {
  syncMasterWifiConfig(ctx);

  Serial.printf("[Wire] Config bytes=%u State bytes=%u\n",
                static_cast<unsigned>(sizeof(IrrigationNodeConfig)),
                static_cast<unsigned>(sizeof(IrrigationNodeState)));

  startAccessPoint(ctx);
  startPicoReportServer(ctx);
  setupWebSocket(ctx);
  setupRoutes(ctx);

  ctx.server.begin();
  Serial.println("[HTTP] Server ready on port 80");
  setPicoStaStatus(ctx, "Scanning for Pico...");
  startPicoScan(ctx);
}

void Tasks::maintain_pico_connection(AppContext &ctx) {
  if (ctx.picoScanInProgress) {
    pollPicoScan(ctx);
    return;
  }

  if (!ctx.picoStaWanted && millis() - ctx.picoScanCompletedMs > 10000) {
    setPicoStaStatus(ctx, "Scanning for Pico...");
    startPicoScan(ctx);
    return;
  }

  if (!ctx.picoStaWanted || ctx.picoPairSSID[0] == '\0') {
    return;
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == String(ctx.picoPairSSID)) {
    if (!ctx.picoStaConnected) {
      Serial.printf("[Pairing] ESP32 STA connected to Pico AP '%s'. STA IP=%s\n",
                    ctx.picoPairSSID,
                    WiFi.localIP().toString().c_str());
    }
    ctx.picoStaConnected = true;
    ctx.picoStaConnecting = false;
    if (ctx.picoNeedsConfig) {
      setPicoStaStatus(ctx, "Pico is waiting for configuration");
      return;
    }

    setPicoStaStatus(ctx, "Pico WiFi connected");
    if (!ctx.picoConfigSendInProgress &&
        millis() - ctx.picoServiceLastAttemptMs > 3000) {
      ctx.picoServiceLastAttemptMs = millis();
      collectPicoStates(ctx);
    }
    return;
  }

  if (ctx.picoStaConnecting && millis() - ctx.picoStaConnectStartedMs > 15000) {
    Serial.printf("[Pairing] Timed out connecting to Pico AP '%s'.\n", ctx.picoPairSSID);
    ctx.picoStaConnecting = false;
    ctx.picoStaConnected = false;
    setPicoStaStatus(ctx, "Pico WiFi connect timed out");
  }

  if (!ctx.picoStaConnecting && millis() - ctx.picoStaLastAttemptMs > 5000) {
    beginPicoStaConnection(ctx);
  }
}

bool Tasks::send_config_to_pico(AppContext &ctx) {
  syncMasterWifiConfig(ctx);
  ctx.picoConfigSendInProgress = true;

  if (!picoStaIsReady(ctx)) {
    Serial.printf("[Pairing] Refusing config send: Pico WiFi is not connected. status=%s\n",
                  ctx.picoStaStatus);
    pushAlert(ctx, "Pico is not connected yet. Wait for Pico WiFi connected.", true);
    ctx.picoConfigSendInProgress = false;
    return false;
  }

  Serial.printf("[Pairing] Pico AP '%s' is connected. ESP32 STA IP=%s\n",
                ctx.picoPairSSID,
                WiFi.localIP().toString().c_str());
  ctx.picoNeedsConfig = false;

  IPAddress host;
  host.fromString(ctx.picoPairHost);

  WiFiClient client;
  Serial.printf("[Pairing] Connecting to Pico TCP %s:%u...\n",
                ctx.picoPairHost,
                AppContext::PICO_TCP_PORT);

  if (!client.connect(host, AppContext::PICO_TCP_PORT)) {
    Serial.println("[Pairing] Pico TCP connection failed.");
    pushAlert(ctx, "Pico TCP connection failed", true);
    ctx.picoConfigSendInProgress = false;
    return false;
  }

  client.setNoDelay(true);
  Serial.println("[Pairing] Pico TCP connected. Asking Pico what it needs...");
  if (!waitForPicoWaitingConfig(client, 10000)) {
    client.stop();
    pushAlert(ctx, "Pico did not request configuration", true);
    ctx.picoConfigSendInProgress = false;
    return false;
  }
  client.print("READY_CONFIG\n");
  client.flush();
  delay(300);

  String payload = configToWireText(ctx);
  size_t expected = payload.length();
  Serial.println("[Pairing] TEXT CONFIG DATA BEING SENT TO PICO:");
  printConfigForSerial("[Pairing][TX]", ctx.node.config);
  Serial.printf("[Pairing][TX] text=%s", payload.c_str());
  Serial.printf("[Pairing] Sending %u text config bytes to Pico...\n",
                static_cast<unsigned>(expected));
  size_t written = writeClientThrottled(client, payload.c_str(), expected);

  if (written != expected) {
    Serial.printf("[Pairing] Config write incomplete: %u/%u bytes\n",
                  static_cast<unsigned>(written),
                  static_cast<unsigned>(expected));
    client.stop();
    pushAlert(ctx, "Config write to Pico was incomplete", true);
    ctx.picoConfigSendInProgress = false;
    return false;
  }

  if (!waitForPicoConfigAck(client, 10000)) {
    client.stop();
    pushAlert(ctx, "Pico did not acknowledge config", true);
    ctx.picoConfigSendInProgress = false;
    return false;
  }

  delay(200);
  client.stop();

  Serial.printf("[Pairing] Sent %u config bytes to Pico.\n", static_cast<unsigned>(written));
  ctx.picoStaWanted = false;
  ctx.picoStaConnecting = false;
  ctx.picoStaConnected = false;
  ctx.picoNeedsConfig = false;
  ctx.picoConfigSendInProgress = false;
  setPicoStaStatus(ctx, "Config sent. Waiting for Pico report.");
  ctx.events.push(Event::make(EventType::ConfigSentToPico, millis()));
  return true;
}

void Tasks::handle_pico_report(AppContext &ctx) {
  WiFiClient client = ctx.picoServer.available();
  if (!client) return;

  Serial.printf("[PicoTCP] Report connection from %s\n", client.remoteIP().toString().c_str());

  uint8_t buffer[512];
  size_t length = 0;
  bool overflow = false;
  uint32_t start = millis();
  uint32_t last_byte = millis();

  while (millis() - start < 5000) {
    while (client.available()) {
      int value = client.read();
      if (value < 0) break;

      if (length < sizeof(buffer)) {
        buffer[length++] = static_cast<uint8_t>(value);
      } else {
        overflow = true;
      }
      last_byte = millis();
    }

    if (!client.connected()) break;
    if (length > 0 && millis() - last_byte > 500) break;
    delay(2);
  }

  client.stop();

  if (overflow) {
    pushAlert(ctx, "Pico report exceeded ESP32 buffer", true);
    return;
  }

  if (length < sizeof(IrrigationNodeState)) {
    pushAlert(ctx, "Pico report was too short", true);
    return;
  }

  size_t header_len = length - sizeof(IrrigationNodeState);
  IrrigationNodeState state{};
  memcpy(&state, buffer + header_len, sizeof(state));

  String header;
  header.reserve(header_len);
  for (size_t i = 0; i < header_len; ++i) {
    header += static_cast<char>(buffer[i]);
  }
  char message[96]{};
  bool is_error = header.indexOf("STATUS=ERROR") >= 0;

  if (is_error) {
    extractHeaderLine(header, "MSG=", message, sizeof(message));
  } else {
    extractHeaderLine(header, "WARN=", message, sizeof(message));
  }

  ctx.events.push(Event::fromState(state, message, is_error, millis()));
}

void Tasks::process_events(AppContext &ctx) {
  Event event;
  while (ctx.events.pop(event)) {
    switch (event.type) {
      case EventType::SendConfigToPico:
        send_config_to_pico(ctx);
        break;

      case EventType::ConfigSentToPico:
        broadcast_alert(ctx, "Configuration applied successfully on Pico", false);
        broadcast_status(ctx);
        break;

      case EventType::ConfigSendFailed:
        broadcast_alert(ctx, event.message[0] ? event.message : "Config send failed", true);
        break;

      case EventType::PicoReportReceived:
        ctx.node.state = event.state;
        ctx.picoConnected = true;
        ctx.lastStateMs = millis();

        Serial.printf("[PicoTCP] State received: moisture=%.1f%% uv=%.2f water=%.1foz battery=%.1f%%\n",
                      ctx.node.state.soilMoisture.moisture_percent,
                      ctx.node.state.uv.uv_index,
                      ctx.node.state.water.ounces_remaining,
                      ctx.node.state.power.percentage);

        if (event.message[0] != '\0') {
          broadcast_alert(ctx, event.message, event.is_error);
        }
        if (ctx.node.state.power.warning) {
          broadcast_alert(ctx, "Low battery warning", false);
        }
        if (ctx.node.state.power.error) {
          broadcast_alert(ctx, "Power sensor error", true);
        }
        if (ctx.node.state.uv.is_alert) {
          broadcast_alert(ctx, "High UV index alert", false);
        }
        if (ctx.node.state.soilMoisture.error ||
            ctx.node.state.uv.error ||
            ctx.node.state.water.error) {
          broadcast_alert(ctx, "One or more sensors reported an error", true);
        }

        broadcast_status(ctx);
        break;

      case EventType::WsClientConnected:
        Serial.printf("[WS] Client #%u connected\n", event.client_id);
        broadcast_status(ctx);
        break;

      case EventType::WsClientDisconnected:
        Serial.printf("[WS] Client #%u disconnected\n", event.client_id);
        break;

      case EventType::BroadcastStatus:
        broadcast_status(ctx);
        break;

      case EventType::BroadcastAlert:
        broadcast_alert(ctx, event.message, event.is_error);
        break;

      case EventType::None:
      default:
        break;
    }
  }
}

void Tasks::broadcast_status(AppContext &ctx) {
  String json = statusToJson(ctx);
  ctx.ws.textAll(json);
  Serial.printf("[WS] Broadcast status (%u bytes) to %u client(s)\n",
                static_cast<unsigned>(json.length()),
                static_cast<unsigned>(ctx.ws.count()));
}

void Tasks::broadcast_alert(AppContext &ctx, const char *message, bool is_error) {
  if (message == nullptr || message[0] == '\0') return;

  StaticJsonDocument<192> doc;
  doc["alert"] = message;
  doc["alert_type"] = is_error ? "error" : "warning";

  String json;
  serializeJson(doc, json);
  ctx.ws.textAll(json);
  Serial.printf("[Alert] %s%s\n", is_error ? "ERROR: " : "", message);
}

void Tasks::check_stale_data(AppContext &ctx) {
  if (!ctx.picoConnected || ctx.lastStateMs == 0) return;

  uint32_t age = millis() - ctx.lastStateMs;
  if (age <= ctx.staleThresholdMs) return;

  ctx.picoConnected = false;
  ctx.lastStateMs = 0;
  broadcast_alert(ctx, "No recent data from Pico", true);
  broadcast_status(ctx);
}
