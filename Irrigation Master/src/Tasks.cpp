#include "Tasks.hpp"
#include "AppContext.hpp"
#include "EventQueue.hpp"
#include "SystemConfig.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

// Reference to the global context defined in main.cpp
extern AppContext gCtx;
extern const char DASHBOARD_HTML[];

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Serialize a PlantStatus to a JSON string for WebSocket broadcast.
String statusToJson(const IrrigationNodeStatus &s, bool picoConnected) {
    StaticJsonDocument<512> doc;
    doc["connected"]        = picoConnected;

    JsonObject moisture     = doc.createNestedObject("moisture");
    moisture["raw"]         = s.moisture_raw;
    moisture["percent"]     = s.moisture_percent;
    moisture["needs_water"] = s.needs_water;

    JsonObject uv           = doc.createNestedObject("uv");
    uv["raw"]               = s.uv_raw;
    uv["uv_index"]          = s.uv_index;
    uv["alert"]             = s.uv_alert;

    JsonObject water        = doc.createNestedObject("water");
    water["raw"]            = s.water_raw;
    water["percent"]        = s.water_percent;
    water["oz"]             = s.water_oz;

    JsonObject power        = doc.createNestedObject("power");
    power["raw"]            = s.power_raw;
    power["voltage"]        = s.battery_voltage;
    power["percent"]        = s.battery_percent;

    doc["pump_active"]      = s.pump_active;

    String out;
    serializeJson(doc, out);
    return out;
}

// Serialize IrrigationNodeConfig to JSON for the /config HTTP endpoint.
String configToJson(const IrrigationNodeConfig &c) {
    StaticJsonDocument<256> doc;
    doc["moisture_dry_cal"]       = c.moisture_dry_cal;
    doc["moisture_wet_cal"]       = c.moisture_wet_cal;
    doc["moisture_threshold_pct"] = c.moisture_threshold_pct;
    doc["uv_alert_threshold"]     = c.uv_alert_threshold;
    doc["pump_run_duration_ms"]   = c.pump_run_duration_ms;
    doc["water_tank_oz"]          = c.water_tank_oz;
    doc["battery_low_pct"]        = c.battery_low_pct;

    String out;
    serializeJson(doc, out);
    return out;
}

} // anonymous namespace

void Tasks::start(AppContext &ctx) {
    Serial.println("[System] Starting up...");
    ctx.scheduler->schedule(Tasks::init_access_point);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks::init_access_point
// Start the ESP32 as a Wi-Fi soft-AP so the Pico can connect.
// ─────────────────────────────────────────────────────────────────────────────
void Tasks::init_access_point(AppContext &ctx) {
    Serial.println("[AP] Starting soft-AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ctx.apSSID, ctx.apPassword);
    delay(100);   // let the AP settle

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[AP] SSID=%s  IP=%s\n", ctx.apSSID, ip.toString().c_str());

    // Schedule web server startup immediately after AP is up.
    ctx.scheduler->schedule(Tasks::init_web_server);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks::init_web_server
// Register every HTTP route and the WebSocket handler.
// ─────────────────────────────────────────────────────────────────────────────
void Tasks::init_web_server(AppContext &ctx) {
    Serial.println("[WS] Registering routes...");

    // ── WebSocket ────────────────────────────────────────────────────────────
    ctx.ws->onEvent([](AsyncWebSocket *srv, AsyncWebSocketClient *client,
                        AwsEventType type, void *arg, uint8_t *data, size_t len) {
        (void)srv; (void)arg; (void)data; (void)len;
        if (type == WS_EVT_CONNECT) {
            Serial.printf("[WS] Browser client #%u connected\n", client->id());
        } else if (type == WS_EVT_DISCONNECT) {
            Serial.printf("[WS] Browser client #%u disconnected\n", client->id());
        }
    });
    ctx.server->addHandler(ctx.ws);

    // ── GET /  → dashboard SPA ───────────────────────────────────────────────
    ctx.server->on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send_P(200, "text/html", DASHBOARD_HTML);
    });

    // ── GET /config  → Pico polls this to receive IrrigationNodeConfig ───────
    ctx.server->on("/config", HTTP_GET, [](AsyncWebServerRequest *req) {
        Serial.println("[HTTP] GET /config  — Pico requesting config");
        req->send(200, "application/json", configToJson(gCtx.config));
    });

    // ── POST /status  → Pico posts IrrigationNodeStatus JSON ────────────────
    ctx.server->on("/status", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                Serial.printf("[HTTP] POST /status parse error: %s\n", err.c_str());
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }

            // Map JSON fields → IrrigationNodeStatus
            IrrigationNodeStatus s{};
            s.moisture_raw     = doc["moisture"]["raw"]         | 0;
            s.moisture_percent = doc["moisture"]["percent"]     | 0.0f;
            s.needs_water      = doc["moisture"]["needs_water"] | false;

            s.uv_raw           = doc["uv"]["raw"]               | 0;
            s.uv_index         = doc["uv"]["uv_index"]          | 0.0f;
            s.uv_alert         = doc["uv"]["alert"]             | false;

            s.water_raw        = doc["water"]["raw"]            | 0;
            s.water_percent    = doc["water"]["percent"]        | 0.0f;
            s.water_oz         = doc["water"]["oz"]             | 0.0f;

            s.power_raw        = doc["power"]["raw"]            | 0;
            s.battery_voltage  = doc["power"]["voltage"]        | 0.0f;
            s.battery_percent  = doc["power"]["percent"]        | 0.0f;

            s.pump_active      = doc["pump_active"]             | false;

            // Push into event queue — main loop will broadcast to browsers
            if (!gCtx.eventDispatcher->push(Event::fromStatus(s, millis()))) {
                Serial.println("[EventQueue] FULL – status event dropped!");
            }

            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // ── GET /api/status  → REST snapshot for browsers ──────────────────────
    ctx.server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json",
                  statusToJson(gCtx.lastStatus, gCtx.picoConnected));
    });

    // ── 404 ──────────────────────────────────────────────────────────────────
    ctx.server->onNotFound([](AsyncWebServerRequest *req) {
        req->send(404, "text/plain", "Not found");
    });

    ctx.server->begin();
    Serial.println("[HTTP] Server started on port 80");

    // Schedule default config build right after the server is up
    ctx.scheduler->schedule(Tasks::build_default_config);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks::build_default_config
// Populate ctx.config with sensible defaults.
// ─────────────────────────────────────────────────────────────────────────────
void Tasks::build_default_config(AppContext &ctx) {
    ctx.configReady = true;
    Serial.println("[Config] Default config ready.");
    ctx.eventDispatcher->push(Event::fromConfig(ctx.config, millis()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks::send_config_to_pico
// Called after a Pico GET /config — just logs and marks the flag.
// ─────────────────────────────────────────────────────────────────────────────
void Tasks::send_config_to_pico(AppContext &ctx) {
    Serial.println("[Config] Config served to Pico.");
    ctx.eventDispatcher->push(Event::make(EventType::ConfigSentToPico, millis()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks::await_pico_connection
// Periodic check: log connected Pico count.
// ─────────────────────────────────────────────────────────────────────────────
void Tasks::await_pico_connection(AppContext &ctx) {
    uint8_t n = WiFi.softAPgetStationNum();
    Serial.printf("[AP] Clients connected: %u\n", n);
    ctx.picoConnected = (n > 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks::process_events
// Drain the EventQueue and dispatch each event to the right handler.
// ─────────────────────────────────────────────────────────────────────────────
void Tasks::process_events(AppContext &ctx) {
    Event evt;
    while (ctx.eventDispatcher->pop(evt)) {
        switch (evt.type) {

        case EventType::PicoConnected:
            ctx.picoConnected = true;
            Serial.printf("[Event] PicoConnected (ip=%s)\n",
                          evt.payload.client.ip);
            ctx.scheduler->schedule(Tasks::broadcast_status);
            break;

        case EventType::PicoDisconnected:
            ctx.picoConnected = false;
            Serial.println("[Event] PicoDisconnected");
            ctx.scheduler->schedule(Tasks::broadcast_status);
            break;

        case EventType::PlantStatusReceived:
            ctx.lastStatus   = evt.payload.status;
            ctx.lastStatusMs = millis();

            Serial.printf("[Event] PlantStatusReceived  "
                          "moisture=%.1f%%  uv=%.2f  water=%.1f%%  batt=%.1f%%\n",
                          ctx.lastStatus.moisture_percent,
                          ctx.lastStatus.uv_index,
                          ctx.lastStatus.water_percent,
                          ctx.lastStatus.battery_percent);

            if (ctx.lastStatus.battery_percent < ctx.config.battery_low_pct) {
                Event battEvt          = Event::make(EventType::BatteryLow, millis());
                battEvt.payload.value  = ctx.lastStatus.battery_percent;
                ctx.eventDispatcher->push(battEvt);
            }
            if (ctx.lastStatus.uv_alert) {
                ctx.eventDispatcher->push(Event::fromAlert("UV index high!", millis()));
            }
            if (ctx.lastStatus.pump_active) {
                ctx.eventDispatcher->push(Event::make(EventType::PumpAlert, millis()));
            }

            ctx.scheduler->schedule(Tasks::broadcast_status);
            break;

        case EventType::SensorDataStale:
            Serial.printf("[Event] SensorDataStale (%.0f s without update)\n",
                          evt.payload.value / 1000.0f);
            ctx.eventDispatcher->push(Event::fromAlert("⚠️ No data from Pico!", millis()));
            ctx.scheduler->schedule(Tasks::broadcast_alert);
            break;

        case EventType::BatteryLow:
            Serial.printf("[Event] BatteryLow  %.1f%%\n", evt.payload.value);
            ctx.eventDispatcher->push(
                Event::fromAlert("🔋 Battery low!", millis()));
            ctx.scheduler->schedule(Tasks::broadcast_alert);
            break;

        case EventType::PumpAlert:
            Serial.println("[Event] PumpAlert — Pico ran the pump");
            ctx.eventDispatcher->push(Event::fromAlert("💧 Pump activated!", millis()));
            ctx.scheduler->schedule(Tasks::broadcast_alert);
            break;

        case EventType::BroadcastAlert:
            ctx.scheduler->schedule(Tasks::broadcast_alert);
            break;

        case EventType::WsClientConnected:
            Serial.printf("[Event] Browser WS client connected\n");
            ctx.scheduler->schedule(Tasks::broadcast_status);
            break;

        case EventType::WsClientDisconnected:
            Serial.printf("[Event] Browser WS client disconnected\n");
            break;

        case EventType::BroadcastStatus:
            ctx.scheduler->schedule(Tasks::broadcast_status);
            break;

        default:
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks::broadcast_status
// Push the latest PlantStatus JSON to all connected browser WebSocket clients.
// ─────────────────────────────────────────────────────────────────────────────
void Tasks::broadcast_status(AppContext &ctx) {
    if (ctx.ws->count() == 0) return;

    String json = statusToJson(ctx.lastStatus, ctx.picoConnected);
    ctx.ws->textAll(json);
    Serial.printf("[WS] Broadcast status (%u bytes) to %u client(s)\n",
                  json.length(), ctx.ws->count());
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks::broadcast_alert
// Pop the next BroadcastAlert event and send it to browser clients.
// ─────────────────────────────────────────────────────────────────────────────
void Tasks::broadcast_alert(AppContext &ctx) {
    if (ctx.ws->count() == 0) return;

    Event evt;
    if (!ctx.eventDispatcher->peek(evt)) return;
    if (evt.type != EventType::BroadcastAlert) return;
    ctx.eventDispatcher->pop(evt);

    StaticJsonDocument<128> doc;
    doc["alert"] = evt.payload.alert;
    String json;
    serializeJson(doc, json);
    ctx.ws->textAll(json);
    Serial.printf("[WS] Broadcast alert: %s\n", evt.payload.alert);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks::check_stale_data
// Called periodically. Enqueues a SensorDataStale event if the Pico hasn't
// posted a status update within ctx.staleThresholdMs.
// ─────────────────────────────────────────────────────────────────────────────
void Tasks::check_stale_data(AppContext &ctx) {
    if (!ctx.picoConnected) return;
    if (ctx.lastStatusMs == 0)       return;

    uint32_t age = millis() - ctx.lastStatusMs;
    if (age > ctx.staleThresholdMs) {
        Event evt         = Event::make(EventType::SensorDataStale, millis());
        evt.payload.value = static_cast<float>(age);
        ctx.eventDispatcher->push(evt);
        ctx.lastStatusMs  = millis();
        Serial.printf("[Watchdog] Stale data detected (%u ms)\n", age);
    }
}
