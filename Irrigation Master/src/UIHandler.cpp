#include "UIHandler.hpp"
#include <ArduinoJson.h>

// ─────────────────────────────────────────────────────────────────────────────
// Static member initialization
// ─────────────────────────────────────────────────────────────────────────────
AppContext *UIHandler::gCtx = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// Dashboard HTML — embedded in flash (PROGMEM)
// For larger apps, store in LittleFS and use server.serveStatic().
// ─────────────────────────────────────────────────────────────────────────────
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>LeafLink — Plant Dashboard</title>
<style>
  :root {
    --bg:#0a0f0d; --surface:#121a15; --border:#1e2d22;
    --green:#4ade80; --yellow:#facc15; --red:#f87171;
    --blue:#60a5fa; --text:#e2f0e6; --muted:#6b8f73;
    --r:12px;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);
       color:var(--text);min-height:100vh;padding:1.5rem 1rem;
       display:flex;flex-direction:column;align-items:center}
  h1{color:var(--green);font-size:1.6rem;letter-spacing:1px;margin-bottom:4px}
  .subtitle{color:var(--muted);font-size:.85rem;margin-bottom:1.5rem}
  .status-bar{display:flex;align-items:center;gap:8px;
              background:var(--surface);border:1px solid var(--border);
              border-radius:999px;padding:.35rem 1rem;margin-bottom:1.5rem;
              font-size:.82rem}
  .dot{width:8px;height:8px;border-radius:50%;background:var(--red);
       transition:background .4s}
  .dot.ok{background:var(--green);box-shadow:0 0 6px var(--green)}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));
        gap:1rem;width:100%;max-width:820px;margin-bottom:1rem}
  .card{background:var(--surface);border:1px solid var(--border);
        border-radius:var(--r);padding:1.1rem 1.4rem}
  .card .lbl{font-size:.7rem;text-transform:uppercase;letter-spacing:1px;
             color:var(--muted);margin-bottom:6px}
  .card .val{font-size:1.6rem;font-weight:700;color:var(--text)}
  .card .sub{font-size:.78rem;color:var(--muted);margin-top:3px}
  .val.good{color:var(--green)} .val.warn{color:var(--yellow)} .val.bad{color:var(--red)}
  .bar-bg{height:6px;background:#1e2d22;border-radius:3px;margin-top:8px;overflow:hidden}
  .bar{height:100%;border-radius:3px;transition:width .6s}
  .bar.green{background:var(--green)} .bar.yellow{background:var(--yellow)}
  .bar.red{background:var(--red)}
  .alerts{width:100%;max-width:820px;margin-bottom:1rem}
  .alert-item{background:#1c1000;border-left:3px solid var(--yellow);
              border-radius:6px;padding:.6rem 1rem;font-size:.85rem;
              margin-bottom:.5rem;animation:fadein .3s ease}
  @keyframes fadein{from{opacity:0;transform:translateY(-6px)}to{opacity:1;transform:none}}
  .log{background:var(--surface);border:1px solid var(--border);border-radius:var(--r);
       padding:.9rem 1.1rem;width:100%;max-width:820px;font-family:monospace;
       font-size:.78rem;color:var(--muted);height:130px;overflow-y:auto}
  .log .ts{color:var(--blue)}
</style>
</head>
<body>
<h1>🌿 LeafLink</h1>
<p class="subtitle">Real-time plant monitoring dashboard</p>

<div class="status-bar">
  <div class="dot" id="pico-dot"></div>
  <span id="pico-label">Pico: disconnected</span>
  &nbsp;|&nbsp;
  <div class="dot" id="ws-dot"></div>
  <span id="ws-label">WS: connecting…</span>
</div>

<div class="grid">
  <div class="card">
    <div class="lbl">Soil Moisture</div>
    <div class="val" id="moist-val">—</div>
    <div class="sub" id="moist-sub">—</div>
    <div class="bar-bg"><div class="bar green" id="moist-bar" style="width:0%"></div></div>
  </div>
  <div class="card">
    <div class="lbl">UV Index</div>
    <div class="val" id="uv-val">—</div>
    <div class="sub" id="uv-sub">—</div>
    <div class="bar-bg"><div class="bar green" id="uv-bar" style="width:0%"></div></div>
  </div>
  <div class="card">
    <div class="lbl">Water Level</div>
    <div class="val" id="water-val">—</div>
    <div class="sub" id="water-sub">—</div>
    <div class="bar-bg"><div class="bar green" id="water-bar" style="width:0%"></div></div>
  </div>
  <div class="card">
    <div class="lbl">Battery</div>
    <div class="val" id="batt-val">—</div>
    <div class="sub" id="batt-sub">—</div>
    <div class="bar-bg"><div class="bar green" id="batt-bar" style="width:0%"></div></div>
  </div>
  <div class="card">
    <div class="lbl">Pump Status</div>
    <div class="val" id="pump-val">—</div>
    <div class="sub">Auto-triggered by Pico</div>
  </div>
</div>

<div class="alerts" id="alerts"></div>

<div class="log" id="log"></div>

<script>
const $ = id => document.getElementById(id);
const log = msg => {
  const ts = new Date().toLocaleTimeString();
  $('log').innerHTML =
    `<div><span class="ts">[${ts}]</span> ${msg}</div>` + $('log').innerHTML;
};

function setBar(id, pct, low, high) {
  const bar = $(id);
  bar.style.width = Math.min(100, pct) + '%';
  bar.className = 'bar ' + (pct < low ? 'red' : pct < high ? 'yellow' : 'green');
}

function valClass(pct, low, high) {
  return pct < low ? 'val bad' : pct < high ? 'val warn' : 'val good';
}

function renderStatus(d) {
  // Pico connectivity
  $('pico-dot').className = 'dot' + (d.connected ? ' ok' : '');
  $('pico-label').textContent = 'Pico: ' + (d.connected ? 'connected' : 'disconnected');

  // Moisture
  const mp = d.moisture?.percent ?? 0;
  $('moist-val').textContent = mp.toFixed(1) + '%';
  $('moist-val').className   = valClass(mp, 30, 50);
  $('moist-sub').textContent = d.moisture?.needs_water ? '⚠ Needs water' : '✓ OK';
  setBar('moist-bar', mp, 30, 50);

  // UV
  const uv = d.uv?.uv_index ?? 0;
  $('uv-val').textContent = uv.toFixed(1);
  $('uv-val').className   = uv >= 6 ? 'val bad' : uv >= 3 ? 'val warn' : 'val good';
  $('uv-sub').textContent = d.uv?.alert ? '⚠ High UV alert' : '✓ Normal';
  setBar('uv-bar', (uv / 11) * 100, 27, 55);

  // Water
  const wp = d.water?.percent ?? 0;
  $('water-val').textContent = wp.toFixed(1) + '%';
  $('water-val').className   = valClass(wp, 20, 40);
  $('water-sub').textContent = (d.water?.oz ?? 0).toFixed(1) + ' oz remaining';
  setBar('water-bar', wp, 20, 40);

  // Battery
  const bp = d.power?.percent ?? 0;
  $('batt-val').textContent = bp.toFixed(1) + '%';
  $('batt-val').className   = valClass(bp, 20, 40);
  $('batt-sub').textContent = (d.power?.voltage ?? 0).toFixed(2) + ' V';
  setBar('batt-bar', bp, 20, 40);

  // Pump
  $('pump-val').textContent = d.pump_active ? '💧 Running' : '⏸ Idle';
  $('pump-val').className   = d.pump_active ? 'val warn' : 'val good';
}

function showAlert(msg) {
  const el = document.createElement('div');
  el.className = 'alert-item';
  el.textContent = msg;
  $('alerts').prepend(el);
  setTimeout(() => el.remove(), 8000);
  log('Alert: ' + msg);
}

// ── WebSocket ─────────────────────────────────────────────────────────────
let ws;
function connect() {
  ws = new WebSocket('ws://' + location.hostname + '/ws');

  ws.onopen = () => {
    $('ws-dot').className = 'dot ok';
    $('ws-label').textContent = 'WS: live';
    log('WebSocket connected');
  };
  ws.onclose = () => {
    $('ws-dot').className = 'dot';
    $('ws-label').textContent = 'WS: reconnecting…';
    log('WebSocket closed — retrying in 2 s');
    setTimeout(connect, 2000);
  };
  ws.onmessage = evt => {
    try {
      const d = JSON.parse(evt.data);
      if (d.alert) { showAlert(d.alert); return; }
      renderStatus(d);
    } catch(e) { log('Parse error: ' + e); }
  };
}
connect();
</script>
</body>
</html>
)rawliteral";

// ─────────────────────────────────────────────────────────────────────────────
// UIHandler::init — register all HTTP endpoints with the web server
// ─────────────────────────────────────────────────────────────────────────────
void UIHandler::init(AppContext &ctx) {
    gCtx = &ctx;

    // Root & dashboard
    ctx.server->on("/", HTTP_GET, handle_root);
    ctx.server->on("/dashboard", HTTP_GET, handle_dashboard);

    // API — status & config
    ctx.server->on("/api/status", HTTP_GET, handle_get_status);
    ctx.server->on("/api/config", HTTP_GET, handle_get_config);
    ctx.server->on("/api/config", HTTP_POST, handle_post_config);

    // System info
    ctx.server->on("/api/system", HTTP_GET, handle_system_info);

    // Start server
    ctx.server->begin();
    Serial.println("[UIHandler] Web server initialized with all endpoints");
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_root — serve dashboard HTML
// ─────────────────────────────────────────────────────────────────────────────
void UIHandler::handle_root(AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", DASHBOARD_HTML);
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_dashboard — alternative endpoint for dashboard
// ─────────────────────────────────────────────────────────────────────────────
void UIHandler::handle_dashboard(AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", DASHBOARD_HTML);
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_get_status — return current plant status as JSON
// ─────────────────────────────────────────────────────────────────────────────
void UIHandler::handle_get_status(AsyncWebServerRequest *request) {
    if (!gCtx) {
        request->send(500, "application/json", "{\"error\":\"No context\"}");
        return;
    }

    // Create JSON response with current status
    DynamicJsonDocument doc(1024);
    doc["connected"] = gCtx->picoConnected;
    doc["timestamp"] = gCtx->lastStatusMs;

    // Moisture
    doc["moisture"]["percent"] = gCtx->lastStatus.moisture_percent;
    doc["moisture"]["needs_water"] = gCtx->lastStatus.moisture_percent < 30;

    // UV
    doc["uv"]["uv_index"] = gCtx->lastStatus.uv_index;
    doc["uv"]["alert"] = gCtx->lastStatus.uv_index > 6;

    // Water level
    doc["water"]["percent"] = gCtx->lastStatus.water_percent;
    doc["water"]["oz"] = gCtx->lastStatus.water_oz;

    // Power
    doc["power"]["percent"] = gCtx->lastStatus.battery_percent;
    doc["power"]["voltage"] = gCtx->lastStatus.battery_voltage;

    // Pump
    doc["pump_active"] = gCtx->lastStatus.pump_active;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_get_config — return current system config as JSON
// ─────────────────────────────────────────────────────────────────────────────
void UIHandler::handle_get_config(AsyncWebServerRequest *request) {
    if (!gCtx) {
        request->send(500, "application/json", "{\"error\":\"No context\"}");
        return;
    }

    DynamicJsonDocument doc(512);
    doc["ssid"] = "LeafLink";
    doc["update_interval_ms"] = 5000;
    doc["moisture_threshold"] = gCtx->config.moisture_threshold;
    doc["uv_alert_threshold"] = gCtx->config.uv_alert_threshold;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_post_config — accept new config from client
// ─────────────────────────────────────────────────────────────────────────────
void UIHandler::handle_post_config(AsyncWebServerRequest *request) {
    if (!gCtx) {
        request->send(500, "application/json", "{\"error\":\"No context\"}");
        return;
    }

    if (request->hasParam("plain", true)) {
        String body = request->getParam("plain", true)->value();
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        // Update config if values provided
        if (doc.containsKey("moisture_threshold")) {
            gCtx->config.moisture_threshold = doc["moisture_threshold"];
        }
        if (doc.containsKey("uv_alert_threshold")) {
            gCtx->config.uv_alert_threshold = doc["uv_alert_threshold"];
        }

        request->send(200, "application/json", "{\"status\":\"Config updated\"}");
    } else {
        request->send(400, "application/json", "{\"error\":\"No body\"}");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_system_info — return system information
// ─────────────────────────────────────────────────────────────────────────────
void UIHandler::handle_system_info(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(512);
    doc["device"] = "LeafLink Master (ESP32-S3)";
    doc["ap_ssid"] = "LeafLink-AP";
    doc["ap_ip"] = "192.168.4.1";
    doc["uptime_ms"] = millis();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
