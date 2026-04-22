/*
 * LeafLink — ESP32-S3 Master Node
 *
 * Responsibilities:
 *   1. Host a Wi-Fi soft-AP that the Pico joins.
 *   2. Serve SystemConfig to the Pico on GET /config.
 *   3. Receive PlantStatus from the Pico on POST /status.
 *   4. Queue every event through EventQueue<>.
 *   5. Run a TaskScheduler loop (mirrors the Pico's main loop).
 *   6. Broadcast live plant data to browser clients via WebSocket.
 *
 * Board   : ESP32-S3-DevKitC-1
 * Platform: PlatformIO / Arduino framework
 *
 * Required libs (platformio.ini):
 *   me-no-dev/AsyncTCP
 *   me-no-dev/ESPAsyncWebServer
 *   bblanchon/ArduinoJson @ ^6
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>


#include "AppContext.hpp"
#include "EventQueue.hpp"
#include "TaskScheduler.hpp"
#include "Tasks.hpp"
#include "SystemConfig.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Dashboard SPA — embedded in flash (PROGMEM)
// For larger apps, store in LittleFS and use server.serveStatic().
// ─────────────────────────────────────────────────────────────────────────────
extern const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
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
    --r:12px; --shadow:0 4px 12px rgba(0,0,0,0.4);
  }
  *{box-sizing:border-box;margin:0;padding:0}
  html,body{height:100%}
  body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);
       color:var(--text);display:flex;flex-direction:column}
  .container{flex:1;display:flex;flex-direction:column;align-items:center;
             padding:1.5rem 1rem;overflow-y:auto}
  h1{color:var(--green);font-size:1.8rem;letter-spacing:1px;margin-bottom:4px;
     display:flex;align-items:center;gap:8px}
  .subtitle{color:var(--muted);font-size:.85rem;margin-bottom:1.5rem;
            display:flex;align-items:center;gap:8px}
  .status-bar{display:flex;align-items:center;gap:12px;justify-content:center;
              background:var(--surface);border:1px solid var(--border);
              border-radius:999px;padding:.5rem 1.5rem;margin-bottom:1.5rem;
              font-size:.82rem;flex-wrap:wrap}
  .dot{width:10px;height:10px;border-radius:50%;background:var(--red);
       transition:background .4s;flex-shrink:0}
  .dot.ok{background:var(--green);box-shadow:0 0 8px var(--green)}
  .separator{width:1px;height:16px;background:var(--border)}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));
        gap:1rem;width:100%;max-width:1000px;margin-bottom:1.5rem}
  .card{background:var(--surface);border:1px solid var(--border);
        border-radius:var(--r);padding:1.4rem;box-shadow:var(--shadow);
        transition:transform .2s,box-shadow .2s}
  .card:hover{transform:translateY(-2px);box-shadow:0 6px 16px rgba(0,0,0,0.6)}
  .card .lbl{font-size:.7rem;text-transform:uppercase;letter-spacing:1px;
             color:var(--muted);margin-bottom:8px;font-weight:600}
  .card .val{font-size:2rem;font-weight:700;color:var(--text)}
  .card .sub{font-size:.78rem;color:var(--muted);margin-top:4px}
  .val.good{color:var(--green)} .val.warn{color:var(--yellow)} .val.bad{color:var(--red)}
  .bar-bg{height:8px;background:#0f1411;border-radius:4px;margin-top:10px;overflow:hidden;
          border:1px solid var(--border)}
  .bar{height:100%;border-radius:3px;transition:width .6s ease;min-width:0}
  .bar.green{background:linear-gradient(90deg,#22c55e,#4ade80)}
  .bar.yellow{background:linear-gradient(90deg,#eab308,#facc15)}
  .bar.red{background:linear-gradient(90deg,#dc2626,#f87171)}
  .alert-zone{width:100%;max-width:1000px;margin-bottom:1rem}
  .alerts{display:flex;flex-direction:column;gap:.5rem}
  .alert-item{background:rgba(28,16,0,0.8);border-left:4px solid var(--yellow);
              border-radius:6px;padding:.8rem 1rem;font-size:.85rem;
              animation:fadein .3s ease;display:flex;align-items:center;gap:8px}
  .alert-item.error{border-left-color:var(--red);background:rgba(30,10,10,0.8)}
  .alert-item.success{border-left-color:var(--green);background:rgba(10,30,10,0.8)}
  .alert-icon{font-size:1rem;flex-shrink:0}
  .alert-text{flex:1}
  .alert-close{cursor:pointer;color:var(--muted);font-size:1.2rem;line-height:1}
  .alert-close:hover{color:var(--text)}
  @keyframes fadein{from{opacity:0;transform:translateX(-12px)}to{opacity:1;transform:none}}
  .controls{width:100%;max-width:1000px;display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));
            gap:1rem;margin-bottom:1.5rem}
  .btn{background:var(--blue);color:var(--text);border:none;border-radius:var(--r);
       padding:.6rem 1.2rem;cursor:pointer;font-size:.9rem;font-weight:600;
       transition:all .2s;display:flex;align-items:center;justify-content:center;gap:6px}
  .btn:hover{background:#3b82f6;transform:translateY(-2px)}
  .btn:active{transform:translateY(0)}
  .btn.danger{background:var(--red)}
  .btn.danger:hover{background:#dc2626}
  .btn.success{background:var(--green)}
  .btn.success:hover{background:#22c55e}
  .btn:disabled{opacity:0.5;cursor:not-allowed}
  .log-panel{background:var(--surface);border:1px solid var(--border);border-radius:var(--r);
             padding:1rem;width:100%;max-width:1000px;font-family:monospace;
             font-size:.75rem;color:var(--muted);height:200px;overflow-y:auto;
             margin-bottom:1rem;box-shadow:var(--shadow)}
  .log-line{display:flex;gap:8px;padding:.3rem 0;border-bottom:1px solid var(--border)}
  .log-line:last-child{border:none}
  .log-time{color:var(--blue);flex-shrink:0;min-width:100px}
  .log-type{color:var(--yellow);flex-shrink:0;min-width:60px;text-transform:uppercase}
  .log-msg{color:var(--muted);flex:1;word-break:break-all}
  .status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));
               gap:1rem;width:100%;max-width:1000px;margin-bottom:1.5rem}
  .stat-box{background:var(--surface);border:1px solid var(--border);border-radius:var(--r);
            padding:1rem;text-align:center;box-shadow:var(--shadow)}
  .stat-label{font-size:.7rem;color:var(--muted);text-transform:uppercase;margin-bottom:6px}
  .stat-value{font-size:1.8rem;font-weight:700;color:var(--green)}
  .stat-value.warn{color:var(--yellow)}
  .stat-value.bad{color:var(--red)}
  .footer{text-align:center;color:var(--muted);font-size:.75rem;margin-top:auto;
          padding:1rem;border-top:1px solid var(--border);width:100%}
  @media(max-width:768px){
    .container{padding:1rem .5rem}
    h1{font-size:1.4rem}
    .grid{grid-template-columns:1fr}
    .card{padding:1rem}
    .card .val{font-size:1.5rem}
  }
</style>
</head>
<body>
<div class="container">
  <h1>🌿 LeafLink 🌿</h1>
  <p class="subtitle">Real-time plant monitoring & control</p>

  <div class="status-bar">
    <div class="dot" id="pico-dot"></div>
    <span id="pico-label">Pico: <strong>disconnected</strong></span>
    <div class="separator"></div>
    <div class="dot" id="ws-dot"></div>
    <span id="ws-label">WebSocket: <strong>connecting…</strong></span>
    <div class="separator"></div>
    <span id="uptime-label">Uptime: <strong>0s</strong></span>
  </div>

  <div class="grid">
    <div class="card">
      <div class="lbl">🌱 Soil Moisture</div>
      <div class="val" id="moist-val">—</div>
      <div class="sub" id="moist-sub">—</div>
      <div class="bar-bg"><div class="bar green" id="moist-bar" style="width:0%"></div></div>
    </div>
    <div class="card">
      <div class="lbl">☀️ UV Index</div>
      <div class="val" id="uv-val">—</div>
      <div class="sub" id="uv-sub">—</div>
      <div class="bar-bg"><div class="bar green" id="uv-bar" style="width:0%"></div></div>
    </div>
    <div class="card">
      <div class="lbl">💧 Water Level</div>
      <div class="val" id="water-val">—</div>
      <div class="sub" id="water-sub">—</div>
      <div class="bar-bg"><div class="bar green" id="water-bar" style="width:0%"></div></div>
    </div>
    <div class="card">
      <div class="lbl">🔋 Battery</div>
      <div class="val" id="batt-val">—</div>
      <div class="sub" id="batt-sub">—</div>
      <div class="bar-bg"><div class="bar green" id="batt-bar" style="width:0%"></div></div>
    </div>
    <div class="card">
      <div class="lbl">💦 Pump Status</div>
      <div class="val" id="pump-val">⏸ Idle</div>
      <div class="sub">Auto-triggered by Pico</div>
    </div>
    <div class="card">
  <div class="lbl">🪴 Plant Library</div>

  <div style="margin-bottom:10px;">
    <label for="plant-library" style="display:block; margin-bottom:6px; color:var(--muted); font-size:.8rem;">
      Choose from plant library
    </label>
    <select id="plant-library" style="width:100%; padding:.6rem; border-radius:8px; border:1px solid var(--border); background:#0f1411; color:var(--text);">
      <option>Recao</option>
      <option>Oregano Brujo</option>
      <option>Oregano Regular</option>
      <option>Cilantro</option>
      <option>Romero</option>
      //JOSE KEEP ADDING HERE!
    </select>
  </div>

  <div style="margin-bottom:10px;">
    <label for="custom-plant" style="display:block; margin-bottom:6px; color:var(--muted); font-size:.8rem;">
      Add my own plant
    </label>
    <input
      id="custom-plant"
      type="text"
      placeholder="Enter plant name"
      style="width:100%; padding:.6rem; border-radius:8px; border:1px solid var(--border); background:#0f1411; color:var(--text);"
    />
  </div>

  <div style="display:flex; gap:.5rem; flex-wrap:wrap; margin-top:10px;">
    <button class="btn success" type="button">Select Plant</button>
    <button class="btn" type="button">Add Plant</button>
  </div>

  <div class="sub" style="margin-top:10px;">
    
  </div>
</div>
    <div class="card">
      <div class="lbl">📊 Last Update</div>
      <div class="val" id="update-val">—</div>
      <div class="sub" id="update-sub">—</div>
    </div>
  </div>

  <div class="alert-zone">
    <div class="alerts" id="alerts"></div>
  </div>

  <div class="log-panel" id="log"></div>

  <div class="footer">
    <div>ESP32-S3 Master Node | LeafLink v1.0</div>
    <div id="connection-info">192.168.4.1:80</div>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
let startTime = Date.now();
let messageLog = [];

const log = (type, msg) => {
  const ts = new Date().toLocaleTimeString('en-US', {hour12: false});
  const logEntry = {ts, type: type.toUpperCase(), msg};
  messageLog.unshift(logEntry);
  messageLog = messageLog.slice(0, 50);
  
  const logHtml = $('log').innerHTML;
  const newLine = `<div class="log-line"><span class="log-time">${ts}</span><span class="log-type">${type.toUpperCase()}</span><span class="log-msg">${msg}</span></div>`;
  $('log').innerHTML = newLine + logHtml;
};

function setBar(id, pct, low, high) {
  const bar = $(id);
  const clampedPct = Math.min(100, Math.max(0, pct));
  bar.style.width = clampedPct + '%';
  bar.className = 'bar ' + (clampedPct < low ? 'red' : clampedPct < high ? 'yellow' : 'green');
}

function valClass(pct, low, high) {
  return pct < low ? 'val bad' : pct < high ? 'val warn' : 'val good';
}

function addAlert(msg, type = 'warning') {
  const el = document.createElement('div');
  el.className = 'alert-item' + (type === 'error' ? ' error' : type === 'success' ? ' success' : '');
  const icons = {warning: '⚠️', error: '❌', success: '✅'};
  el.innerHTML = `
    <span class="alert-icon">${icons[type] || '❌'}</span>
    <span class="alert-text">${msg}</span>
    <span class="alert-close" onclick="this.parentElement.remove()">✕</span>
  `;
  $('alerts').prepend(el);
  log(type, msg);
  
  setTimeout(() => {
    if (el.parentElement) el.remove();
  }, 8000);
}

function updateUptime() {
  const elapsed = Math.floor((Date.now() - startTime) / 1000);
  const hours = Math.floor(elapsed / 3600);
  const mins = Math.floor((elapsed % 3600) / 60);
  const secs = elapsed % 60;
  let uptimeStr = '';
  if (hours > 0) uptimeStr += hours + 'h ';
  if (mins > 0 || hours > 0) uptimeStr += mins + 'm ';
  uptimeStr += secs + 's';
  $('uptime-label').innerHTML = `Uptime: <strong>${uptimeStr}</strong>`;
}

function renderStatus(d) {
  const picoConnected = d.connected || d.picoConnected;
  $('pico-dot').className = 'dot' + (picoConnected ? ' ok' : '');
  $('pico-label').innerHTML = `Pico: <strong>${picoConnected ? '✓ connected' : '✗ disconnected'}</strong>`;

  if (d.moisture) {
    const mp = d.moisture.percent ?? 0;
    $('moist-val').textContent = mp.toFixed(1) + '%';
    $('moist-val').className = valClass(mp, 30, 50);
    $('moist-sub').textContent = d.moisture.needs_water ? '⚠️ Needs water' : '✓ Optimal';
    setBar('moist-bar', mp, 30, 50);
  }

  if (d.uv) {
    const uv = d.uv.uv_index ?? 0;
    $('uv-val').textContent = uv.toFixed(1);
    $('uv-val').className = uv >= 6 ? 'val bad' : uv >= 3 ? 'val warn' : 'val good';
    $('uv-sub').textContent = d.uv.alert ? '⚠️ High UV alert' : '✓ Safe';
    setBar('uv-bar', (uv / 11) * 100, 27, 55);
  }

  if (d.water) {
    const wp = d.water.percent ?? 0;
    $('water-val').textContent = wp.toFixed(1) + '%';
    $('water-val').className = valClass(wp, 20, 40);
    $('water-sub').textContent = (d.water.oz ?? 0).toFixed(1) + ' oz remaining';
    setBar('water-bar', wp, 20, 40);
  }

  if (d.power) {
    const bp = d.power.percent ?? 0;
    $('batt-val').textContent = bp.toFixed(0) + '%';
    $('batt-val').className = valClass(bp, 20, 40);
    $('batt-sub').textContent = (d.power.voltage ?? 0).toFixed(2) + ' V';
    setBar('batt-bar', bp, 20, 40);
  }

  if (d.pump_active !== undefined) {
    $('pump-val').textContent = d.pump_active ? '💧 Running' : '⏸ Idle';
    $('pump-val').className = d.pump_active ? 'val warn' : 'val good';
  }

  const now = new Date();
  $('update-val').textContent = now.toLocaleTimeString('en-US', {hour12: false});
  $('update-sub').textContent = 'Just now';
}

let ws;
let reconnectAttempts = 0;
const MAX_RECONNECT_ATTEMPTS = 10;

function connect() {
  log('info', 'Connecting to WebSocket...');
  ws = new WebSocket('ws://' + location.hostname + '/ws');

  ws.onopen = () => {
    $('ws-dot').className = 'dot ok';
    $('ws-label').innerHTML = `WebSocket: <strong>✓ live</strong>`;
    log('success', 'WebSocket connected');
    reconnectAttempts = 0;
    addAlert('✓ Connected to ESP32-S3', 'success');
  };

  ws.onclose = () => {
    $('ws-dot').className = 'dot';
    $('ws-label').innerHTML = `WebSocket: <strong>✗ disconnected</strong>`;
    log('warn', 'WebSocket closed');
    
    if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
      reconnectAttempts++;
      const delay = Math.min(30000, 1000 * Math.pow(1.5, reconnectAttempts));
      $('ws-label').innerHTML = `WebSocket: <strong>reconnecting in ${Math.round(delay/1000)}s…</strong>`;
      setTimeout(connect, delay);
    } else {
      addAlert('⚠️ WebSocket connection lost. Check ESP32 connection.', 'error');
      log('error', 'Max reconnection attempts reached');
    }
  };

  ws.onerror = (err) => {
    log('error', 'WebSocket error: ' + (err.message || 'Unknown error'));
    addAlert('⚠️ WebSocket error occurred', 'error');
  };

  ws.onmessage = evt => {
    try {
      const d = JSON.parse(evt.data);
      
      if (d.alert) {
        const type = d.alert_type === 'error' ? 'error' : d.alert_type === 'success' ? 'success' : 'warning';
        addAlert(d.alert, type);
        return;
      }
      
      if (d.moisture || d.uv || d.water || d.power || d.pump_active !== undefined) {
        renderStatus(d);
      }
    } catch(e) {
      log('error', 'Parse error: ' + e.message);
    }
  };
}

setInterval(updateUptime, 1000);

log('info', 'LeafLink dashboard loaded');
updateUptime();
connect();
</script>
</body>
</html>
)rawliteral";

// ----------------------------------------------------------------------------
// Globals — kept minimal; everything lives in AppContext.
// ----------------------------------------------------------------------------
static AsyncWebServer  webServer(80);
static AsyncWebSocket  webSocket("/ws");
static TaskScheduler   scheduler;
static EventQueue      eventDispatcher;

// Global context accessible from Tasks.cpp
AppContext gCtx = {
  .server            = &webServer,
  .ws                = &webSocket,
  .scheduler         = &scheduler,
  .eventDispatcher   = &eventDispatcher,
  .config            = {},
  .lastStatus        = {},
  .configReady       = false,
  .picoConnected     = false,
  .lastStatusMs      = 0,
  .staleThresholdMs  = 30000,
  .apSSID            = "LeafLink-AP",
  .apPassword        = "leaflink123",
};

// ----------------------------------------------------------------------------
// Periodic ticker — runs check_stale_data every N ms without blocking loop()
// ----------------------------------------------------------------------------
static unsigned long lastStaleCheck  = 0;
static unsigned long lastApCheck     = 0;
constexpr unsigned long STALE_CHECK_INTERVAL = 5000;  // every 5 s
constexpr unsigned long AP_CHECK_INTERVAL    = 3000;  // every 3 s

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== LeafLink ESP32-S3 Master ===");

  // Schedule the first task to start the system.
  scheduler.schedule(Tasks::start);
}

void loop() {
  // Run the scheduled tasks, if any. Tasks can schedule other tasks, 
  // so this may run multiple per loop.
  if (!scheduler.empty()) {
    TaskScheduler::TaskFunc task = scheduler.pop();
    if (task) task(gCtx);
  }

  Tasks::process_events(gCtx);

  // Periodic: check for stale Pico data
  unsigned long now = millis();
  if (now - lastStaleCheck >= STALE_CHECK_INTERVAL) {
    lastStaleCheck = now;
    Tasks::check_stale_data(gCtx);
  }

  // Periodic: log AP client count
  if (now - lastApCheck >= AP_CHECK_INTERVAL) {
    lastApCheck = now;
    scheduler.schedule(Tasks::await_pico_connection);
  }

  // Clean up dead WebSocket connections
  webSocket.cleanupClients();

  // Small yield so the WiFi stack gets CPU time.
  delay(10);
}
