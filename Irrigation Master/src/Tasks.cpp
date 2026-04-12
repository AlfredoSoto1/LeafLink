// #include "Tasks.hpp"
// #include "AppContext.hpp"
// #include "EventQueue.hpp"
// #include "SystemConfig.hpp"

// #include <Arduino.h>
// #include <WiFi.h>
// #include <ArduinoJson.h>
// #include <ESPAsyncWebServer.h>

// // ─────────────────────────────────────────────────────────────────────────────
// // Helpers
// // ─────────────────────────────────────────────────────────────────────────────
// namespace {

// // Serialize a PlantStatus to a JSON string for WebSocket broadcast.
// String statusToJson(const PlantStatus &s, bool picoConnected) {
//     StaticJsonDocument<512> doc;
//     doc["connected"]        = picoConnected;
//     doc["timestamp_ms"]     = s.timestamp_ms;

//     JsonObject moisture     = doc.createNestedObject("moisture");
//     moisture["raw"]         = s.moisture_raw;
//     moisture["percent"]     = s.moisture_percent;
//     moisture["needs_water"] = s.needs_water;

//     JsonObject uv           = doc.createNestedObject("uv");
//     uv["raw"]               = s.uv_raw;
//     uv["uv_index"]          = s.uv_index;
//     uv["alert"]             = s.uv_alert;

//     JsonObject water        = doc.createNestedObject("water");
//     water["raw"]            = s.water_raw;
//     water["percent"]        = s.water_percent;
//     water["oz"]             = s.water_oz;

//     JsonObject power        = doc.createNestedObject("power");
//     power["raw"]            = s.power_raw;
//     power["voltage"]        = s.battery_voltage;
//     power["percent"]        = s.battery_percent;

//     doc["pump_active"]      = s.pump_active;

//     String out;
//     serializeJson(doc, out);
//     return out;
// }

// // Serialize SystemConfig to JSON for the /config HTTP endpoint.
// String configToJson(const SystemConfig &c) {
//     StaticJsonDocument<256> doc;
//     doc["plant_name"]         = c.plant_name;
//     doc["moisture_low"]       = c.moisture_low;
//     doc["moisture_high"]      = c.moisture_high;
//     doc["uv_alert"]           = c.uv_alert;
//     doc["tank_capacity_oz"]   = c.tank_capacity_oz;
//     doc["pump_duration_ms"]   = c.pump_duration_ms;
//     doc["sensor_interval_ms"] = c.sensor_interval_ms;
//     doc["battery_low"]        = c.battery_low;

//     String out;
//     serializeJson(doc, out);
//     return out;
// }

// } // anonymous namespace

// void Tasks::start(AppContext &ctx) {
//     Serial.println("[System] Starting up...");
//     ctx.scheduler->schedule(Tasks::init_access_point);
// }

// // ─────────────────────────────────────────────────────────────────────────────
// // Tasks::init_access_point
// // Start the ESP32 as a Wi-Fi soft-AP so the Pico can connect.
// // ─────────────────────────────────────────────────────────────────────────────
// void Tasks::init_access_point(AppContext &ctx) {
//     Serial.println("[AP] Starting soft-AP...");
//     WiFi.mode(WIFI_AP);
//     WiFi.softAP(ctx.apSSID, ctx.apPassword);
//     delay(100);   // let the AP settle

//     IPAddress ip = WiFi.softAPIP();
//     Serial.printf("[AP] SSID=%s  IP=%s\n", ctx.apSSID, ip.toString().c_str());

//     // Schedule web server startup immediately after AP is up.
//     ctx.scheduler->schedule(Tasks::init_web_server);
// }

// // ─────────────────────────────────────────────────────────────────────────────
// // Tasks::init_web_server
// // Register every HTTP route and the WebSocket handler.
// // ─────────────────────────────────────────────────────────────────────────────
// void Tasks::init_web_server(AppContext &ctx) {
//     Serial.println("[WS] Registering routes...");

//     // ── WebSocket ────────────────────────────────────────────────────────────
//     ctx.ws->onEvent([](AsyncWebSocket *srv, AsyncWebSocketClient *client,
//                         AwsEventType type, void *arg, uint8_t *data, size_t len) {
//         // We need the AppContext but the lambda can't capture it here because
//         // ESPAsyncWebServer uses C-style callbacks. We store it as a global
//         // pointer (see main.cpp) and access it from the event handler there.
//         // The handler below just signals our EventQueue so the main loop does
//         // the actual work.
//         (void)srv; (void)arg; (void)data; (void)len;
//         if (type == WS_EVT_CONNECT) {
//             Serial.printf("[WS] Browser client #%u connected\n", client->id());
//         } else if (type == WS_EVT_DISCONNECT) {
//             Serial.printf("[WS] Browser client #%u disconnected\n", client->id());
//         }
//     });
//     ctx.server->addHandler(ctx.ws);

//     // ── GET /  → dashboard SPA ───────────────────────────────────────────────
//     // (HTML defined in main.cpp as a PROGMEM string)
//     extern const char DASHBOARD_HTML[];
//     ctx.server->on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
//         extern const char DASHBOARD_HTML[];
//         req->send_P(200, "text/html", DASHBOARD_HTML);
//     });

//     // ── GET /config  → Pico polls this to receive SystemConfig ───────────────
//     ctx.server->on("/config", HTTP_GET, [&ctx](AsyncWebServerRequest *req) {
//         Serial.println("[HTTP] GET /config  — Pico requesting config");

//         // Enqueue PicoConnected event (idempotent; handler checks flag)
//         Event evt = Event::make(EventType::PicoConnected, millis());
//         strncpy(evt.payload.client.ip,
//                 req->client()->remoteIP().toString().c_str(),
//                 sizeof(evt.payload.client.ip) - 1);
//         ctx.events->push(evt);

//         // Respond immediately with the current config JSON
//         req->send(200, "application/json", configToJson(ctx.config));

//         // Schedule the logging task
//         ctx.scheduler->schedule(Tasks::send_config_to_pico);
//     });

//     // ── POST /status  → Pico posts PlantStatus JSON ──────────────────────────
//     ctx.server->on("/status", HTTP_POST,
//         [](AsyncWebServerRequest *req) {},   // onRequest (unused for body)
//         nullptr,                             // onUpload
//         [&ctx](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
//             StaticJsonDocument<512> doc;
//             DeserializationError err = deserializeJson(doc, data, len);
//             if (err) {
//                 Serial.printf("[HTTP] POST /status parse error: %s\n", err.c_str());
//                 req->send(400, "application/json", "{\"error\":\"bad json\"}");
//                 return;
//             }

//             // Map JSON fields → IrrigationNodeStatus
//             IrrigationNodeStatus s{};
//             s.moisture_raw     = doc["moisture"]["raw"]         | 0;
//             s.moisture_percent = doc["moisture"]["percent"]     | 0.0f;
//             s.needs_water      = doc["moisture"]["needs_water"] | false;

//             s.uv_raw           = doc["uv"]["raw"]               | 0;
//             s.uv_index         = doc["uv"]["uv_index"]          | 0.0f;
//             s.uv_alert         = doc["uv"]["alert"]             | false;

//             s.water_raw        = doc["water"]["raw"]            | 0;
//             s.water_percent    = doc["water"]["percent"]        | 0.0f;
//             s.water_oz         = doc["water"]["oz"]             | 0.0f;

//             s.power_raw        = doc["power"]["raw"]            | 0;
//             s.battery_voltage  = doc["power"]["voltage"]        | 0.0f;
//             s.battery_percent  = doc["power"]["percent"]        | 0.0f;

//             s.pump_active      = doc["pump_active"]             | false;
//             s.timestamp_ms     = doc["timestamp_ms"]            | (uint32_t)millis();

//             // Push into event queue — main loop will broadcast to browsers
//             if (!ctx.eventDispatcher->push(Event::fromStatus(s, millis()))) {
//                 Serial.println("[EventQueue] FULL – status event dropped!");
//             }

//             req->send(200, "application/json", "{\"ok\":true}");
//         }
//     );

//     // ── GET /api/status  → REST snapshot for browsers ─────────────────────
//     ctx.server->on("/api/status", HTTP_GET, [&ctx](AsyncWebServerRequest *req) {
//         req->send(200, "application/json",
//                   statusToJson(ctx.lastStatus, ctx.picoConnected));
//     });

//     // ── 404 ──────────────────────────────────────────────────────────────────
//     ctx.server->onNotFound([](AsyncWebServerRequest *req) {
//         req->send(404, "text/plain", "Not found");
//     });

//     ctx.server->begin();
//     Serial.println("[HTTP] Server started on port 80");

//     // Schedule default config build right after the server is up
//     ctx.scheduler->schedule(Tasks::build_default_config);
// }

// // ─────────────────────────────────────────────────────────────────────────────
// // Tasks::build_default_config
// // Populate ctx.config with sensible defaults (or load from NVS in the future).
// // ─────────────────────────────────────────────────────────────────────────────
// void Tasks::build_default_config(AppContext &ctx) {
//     // Defaults already set in the SystemConfig constructor.
//     // Override here if you want non-default values.
//     // strncpy(ctx.config.plant_name, "LeafLink Plant", sizeof(ctx.config.plant_name) - 1);
//     // ctx.config.moisture_low       = 30.0f;
//     // ctx.config.moisture_high      = 70.0f;
//     // ctx.config.uv_alert           = 6.0f;
//     // ctx.config.tank_capacity_oz   = 128.0f;
//     // ctx.config.pump_duration_ms   = 5000;
//     // ctx.config.sensor_interval_ms = 10000;
//     // ctx.config.battery_low_pct    = 20.0f;

//     ctx.configReady = true;
//     Serial.println("[Config] Default config ready.");

//     // Enqueue event so the scheduler knows config is available
//     ctx.eventDispatcher->push(Event::fromConfig(ctx.config, millis()));
// }

// // ─────────────────────────────────────────────────────────────────────────────
// // Tasks::send_config_to_pico
// // Called after a Pico GET /config — just logs and marks the flag.
// // The actual HTTP response is sent inline in the route handler above.
// // ─────────────────────────────────────────────────────────────────────────────
// void Tasks::send_config_to_pico(AppContext &ctx) {
//     Serial.println("[Config] Config served to Pico.");
//     ctx.eventDispatcher->push(Event::make(EventType::ConfigSentToPico, millis()));
// }

// // ─────────────────────────────────────────────────────────────────────────────
// // Tasks::await_pico_connection
// // Periodic check: log connected Pico count.
// // ─────────────────────────────────────────────────────────────────────────────
// void Tasks::await_pico_connection(AppContext &ctx) {
//     uint8_t n = WiFi.softAPgetStationNum();
//     Serial.printf("[AP] Clients connected: %u\n", n);
//     ctx.picoConnected = (n > 0);
// }

// // ─────────────────────────────────────────────────────────────────────────────
// // Tasks::process_events
// // Drain the EventQueue and dispatch each event to the right handler.
// // This is the heart of the system — called every loop tick.
// // ─────────────────────────────────────────────────────────────────────────────
// void Tasks::process_events(AppContext &ctx) {
//     Event evt;
//     while (ctx.eventDispatcher->pop(evt)) {
//         switch (evt.type) {

//         case EventType::PicoConnected:
//             ctx.picoConnected = true;
//             Serial.printf("[Event] PicoConnected (ip=%s)\n",
//                           evt.payload.client.ip);
//             // Schedule immediate broadcast so the browser sees "Pico online"
//             ctx.scheduler->schedule(Tasks::broadcast_status);
//             break;

//         case EventType::PicoDisconnected:
//             ctx.picoConnected = false;
//             Serial.println("[Event] PicoDisconnected");
//             ctx.scheduler->schedule(Tasks::broadcast_status);
//             break;

//         case EventType::PlantStatusReceived:
//             ctx.lastStatus   = evt.payload.status;
//             ctx.lastStatusMs = millis();

//             Serial.printf("[Event] PlantStatusReceived  "
//                           "moisture=%.1f%%  uv=%.2f  water=%.1f%%  batt=%.1f%%\n",
//                           ctx.lastStatus.moisture_percent,
//                           ctx.lastStatus.uv_index,
//                           ctx.lastStatus.water_percent,
//                           ctx.lastStatus.battery_percent);

//             // Push derivative events if thresholds are breached
//             if (ctx.lastStatus.battery_percent < ctx.config.battery_low_pct) {
//                 Event battEvt          = Event::make(EventType::BatteryLow, millis());
//                 battEvt.payload.value  = ctx.lastStatus.battery_percent;
//                 ctx.eventDispatcher->push(battEvt);
//             }
//             if (ctx.lastStatus.uv_alert) {
//                 ctx.eventDispatcher->push(Event::fromAlert("UV index high!", millis()));
//             }
//             if (ctx.lastStatus.pump_active) {
//                 ctx.eventDispatcher->push(Event::make(EventType::PumpAlert, millis()));
//             }

//             ctx.scheduler->schedule(Tasks::broadcast_status);
//             break;

//         case EventType::ConfigReady:
//             Serial.println("[Event] ConfigReady");
//             break;

//         case EventType::ConfigSentToPico:
//             Serial.println("[Event] ConfigSentToPico");
//             break;

//         case EventType::SensorDataStale:
//             Serial.printf("[Event] SensorDataStale (%.0f s without update)\n",
//                           evt.payload.value / 1000.0f);
//             ctx.eventDispatcher->push(Event::fromAlert("⚠️ No data from Pico!", millis()));
//             ctx.scheduler->schedule(Tasks::broadcast_alert);
//             break;

//         case EventType::BatteryLow:
//             Serial.printf("[Event] BatteryLow  %.1f%%\n", evt.payload.value);
//             ctx.eventDispatcher->push(
//                 Event::fromAlert("🔋 Battery low!", millis()));
//             ctx.scheduler->schedule(Tasks::broadcast_alert);
//             break;

//         case EventType::PumpAlert:
//             Serial.println("[Event] PumpAlert — Pico ran the pump");
//             ctx.eventDispatcher->push(Event::fromAlert("💧 Pump activated!", millis()));
//             ctx.scheduler->schedule(Tasks::broadcast_alert);
//             break;

//         case EventType::BroadcastAlert:
//             // Already enqueued from another event — let broadcast_alert handle it
//             ctx.scheduler->schedule(Tasks::broadcast_alert);
//             break;

//         case EventType::WsClientConnected:
//             Serial.printf("[Event] Browser WS client #%u connected\n",
//                           evt.payload.client.client_id);
//             // Push current state to the newly connected browser immediately
//             ctx.scheduler->schedule(Tasks::broadcast_status);
//             break;

//         case EventType::WsClientDisconnected:
//             Serial.printf("[Event] Browser WS client #%u disconnected\n",
//                           evt.payload.client.client_id);
//             break;

//         case EventType::BroadcastStatus:
//             ctx.scheduler->schedule(Tasks::broadcast_status);
//             break;

//         default:
//             break;
//         }
//     }
// }

// // ─────────────────────────────────────────────────────────────────────────────
// // Tasks::broadcast_status
// // Push the latest PlantStatus JSON to all connected browser WebSocket clients.
// // ─────────────────────────────────────────────────────────────────────────────
// void Tasks::broadcast_status(AppContext &ctx) {
//     if (ctx.ws->count() == 0) return;

//     String json = statusToJson(ctx.lastStatus, ctx.picoConnected);
//     ctx.ws->textAll(json);
//     Serial.printf("[WS] Broadcast status (%u bytes) to %u client(s)\n",
//                   json.length(), ctx.ws->count());
// }

// // ─────────────────────────────────────────────────────────────────────────────
// // Tasks::broadcast_alert
// // Pop the next BroadcastAlert event and send it to browser clients.
// // ─────────────────────────────────────────────────────────────────────────────
// void Tasks::broadcast_alert(AppContext &ctx) {
//     if (ctx.ws->count() == 0) return;

//     // Peek into queue for any BroadcastAlert events
//     Event evt;
//     if (!ctx.eventDispatcher->peek(evt)) return;
//     if (evt.type != EventType::BroadcastAlert) return;
//     ctx.eventDispatcher->pop(evt); // consume it

//     StaticJsonDocument<128> doc;
//     doc["alert"] = evt.payload.alert;
//     String json;
//     serializeJson(doc, json);
//     ctx.ws->textAll(json);
//     Serial.printf("[WS] Broadcast alert: %s\n", evt.payload.alert);
// }

// // ─────────────────────────────────────────────────────────────────────────────
// // Tasks::check_stale_data
// // Called periodically. Enqueues a SensorDataStale event if the Pico hasn't
// // posted a status update within ctx.staleThresholdMs.
// // ─────────────────────────────────────────────────────────────────────────────
// void Tasks::check_stale_data(AppContext &ctx) {
//     if (!ctx.picoConnected) return;  // Not connected → no expectation of data
//     if (ctx.lastStatusMs == 0)       return;  // Never received anything yet

//     uint32_t age = millis() - ctx.lastStatusMs;
//     if (age > ctx.staleThresholdMs) {
//         Event evt         = Event::make(EventType::SensorDataStale, millis());
//         evt.payload.value = static_cast<float>(age);
//         ctx.eventDispatcher->push(evt);
//         ctx.lastStatusMs  = millis(); // reset so we don't flood the queue
//         Serial.printf("[Watchdog] Stale data detected (%u ms)\n", age);
//     }
// }
