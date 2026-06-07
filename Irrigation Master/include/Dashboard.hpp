#pragma once

#include <Arduino.h>

extern const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>LeafLink Plant Dashboard</title>
<style>
  :root {
    --bg:#0a0f0d; --surface:#121a15; --border:#1e2d22;
    --green:#4ade80; --yellow:#facc15; --red:#f87171;
    --blue:#60a5fa; --text:#e2f0e6; --muted:#6b8f73;
    --r:12px; --shadow:0 4px 12px rgba(0,0,0,0.4);
  }
  *{box-sizing:border-box;margin:0;padding:0}
  html,body{height:100%}
  body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--text)}
  .container{min-height:100%;display:flex;flex-direction:column;align-items:center;padding:1.5rem 1rem}
  h1{color:var(--green);font-size:1.8rem;margin-bottom:4px;letter-spacing:1px}
  .subtitle{color:var(--muted);font-size:.85rem;margin-bottom:1.5rem}
  .status-bar{display:flex;align-items:center;gap:12px;justify-content:center;background:var(--surface);border:1px solid var(--border);border-radius:999px;padding:.5rem 1.5rem;margin-bottom:1.5rem;font-size:.82rem;flex-wrap:wrap}
  .dot{width:10px;height:10px;border-radius:50%;background:var(--red);transition:background .4s;flex-shrink:0}
  .dot.ok{background:var(--green);box-shadow:0 0 8px var(--green)}
  .separator{width:1px;height:16px;background:var(--border)}
  .section{width:100%;max-width:1000px;margin-bottom:1.5rem}
  .section-header{display:flex;align-items:center;gap:10px;margin-bottom:1rem}
  .section-title{font-size:.7rem;text-transform:uppercase;letter-spacing:2px;color:var(--muted);font-weight:600}
  .section-line{flex:1;height:1px;background:var(--border)}
  .plant-nodes{display:flex;flex-wrap:wrap;gap:1rem;align-items:stretch}
  .plant-node,.card,.empty-state{background:var(--surface);border:1px solid var(--border);border-radius:var(--r);box-shadow:var(--shadow)}
  .plant-node{padding:1.2rem 1.4rem;min-width:190px;display:flex;flex-direction:column;gap:8px;cursor:pointer;transition:transform .2s,box-shadow .2s}
  .plant-node:hover,.card:hover{transform:translateY(-2px);box-shadow:0 6px 16px rgba(0,0,0,0.6)}
  .plant-node.selected{border-color:var(--green);box-shadow:0 0 0 2px rgba(74,222,128,0.3)}
  .plant-node-label{font-size:.68rem;text-transform:uppercase;letter-spacing:1px;color:var(--muted);font-weight:600}
  .plant-node-name{font-size:1.1rem;font-weight:700;color:var(--green)}
  .plant-node-meta{font-size:.72rem;color:var(--muted);line-height:1.45}
  .node-actions{display:flex;gap:6px;margin-top:auto}
  .node-btn{background:transparent;border:1px solid var(--border);border-radius:6px;color:var(--muted);font-size:.72rem;padding:.3rem .7rem;cursor:pointer;transition:all .2s}
  .node-btn:hover{border-color:var(--green);color:var(--green)}
  .node-btn.remove:hover{border-color:var(--red);color:var(--red)}
  .add-node-btn,.btn{border:none;border-radius:8px;cursor:pointer;font-size:.88rem;font-weight:600;transition:all .2s}
  .add-node-btn{background:transparent;border:2px dashed var(--border);color:var(--muted);min-width:160px;padding:1.2rem 1.8rem}
  .add-node-btn:hover{border-color:var(--green);color:var(--green);background:rgba(74,222,128,0.04)}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:1rem;width:100%}
  .card{padding:1.4rem;transition:transform .2s,box-shadow .2s}
  .card .lbl{font-size:.7rem;text-transform:uppercase;letter-spacing:1px;color:var(--muted);margin-bottom:8px;font-weight:600}
  .card .val{font-size:2rem;font-weight:700;color:var(--text)}
  .card .sub{font-size:.78rem;color:var(--muted);margin-top:4px}
  .val.good{color:var(--green)} .val.warn{color:var(--yellow)} .val.bad{color:var(--red)}
  .bar-bg{height:8px;background:#0f1411;border-radius:4px;margin-top:10px;overflow:hidden;border:1px solid var(--border)}
  .bar{height:100%;border-radius:3px;transition:width .6s ease;min-width:0}
  .bar.green{background:linear-gradient(90deg,#22c55e,#4ade80)}
  .bar.yellow{background:linear-gradient(90deg,#eab308,#facc15)}
  .bar.red{background:linear-gradient(90deg,#dc2626,#f87171)}
  .empty-state{padding:1rem 1.2rem;color:var(--muted);font-size:.86rem;width:100%}
  .log-panel{display:none;background:var(--surface);border:1px solid var(--border);border-radius:var(--r);padding:1rem;width:100%;max-width:1000px;font-family:monospace;font-size:.75rem;color:var(--muted);height:180px;overflow-y:auto;margin-bottom:1rem;box-shadow:var(--shadow)}
  .log-line{display:flex;gap:8px;padding:.3rem 0;border-bottom:1px solid var(--border)}
  .log-time{color:var(--blue);flex-shrink:0;min-width:90px}
  .log-type{color:var(--yellow);flex-shrink:0;min-width:60px;text-transform:uppercase}
  .alert-zone{position:fixed;top:1rem;right:1rem;width:320px;z-index:99;pointer-events:none}
  .alerts{display:flex;flex-direction:column;gap:.5rem}
  .alert-item{pointer-events:all;background:rgba(28,16,0,0.85);border-left:4px solid var(--yellow);border-radius:6px;padding:.8rem 1rem;font-size:.85rem;display:flex;gap:8px;align-items:center}
  .alert-item.error{border-left-color:var(--red);background:rgba(30,10,10,0.85)}
  .alert-item.success{border-left-color:var(--green);background:rgba(10,30,10,0.85)}
  .alert-text{flex:1}
  .alert-close{cursor:pointer;color:var(--muted);font-size:1.2rem;line-height:1}
  .modal-overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,0.75);backdrop-filter:blur(4px);z-index:100;align-items:center;justify-content:center;padding:1rem}
  .modal-overlay.open{display:flex}
  .modal{background:var(--surface);border:1px solid var(--border);border-radius:16px;padding:1.4rem;width:100%;max-width:620px;max-height:92vh;overflow:auto;box-shadow:0 20px 60px rgba(0,0,0,0.6);display:flex;flex-direction:column;gap:1rem}
  .modal-header{display:flex;align-items:center;justify-content:space-between}
  .modal-title{font-size:1.1rem;font-weight:700;color:var(--green)}
  .modal-close{background:none;border:none;color:var(--muted);font-size:1.4rem;cursor:pointer;line-height:1;padding:4px}
  .modal-close:hover{color:var(--text)}
  label{display:block;font-size:.75rem;color:var(--muted);margin-bottom:6px;text-transform:uppercase;letter-spacing:.5px}
  select,input{width:100%;padding:.7rem .9rem;border-radius:8px;border:1px solid var(--border);background:#0a0f0d;color:var(--text);font-size:.9rem;outline:none}
  select:focus,input:focus{border-color:var(--green)}
  .scan-list{display:flex;flex-direction:column;gap:.5rem}
  .scan-row{background:#0a0f0d;border:1px solid var(--border);border-radius:8px;color:var(--text);padding:.75rem .9rem;text-align:left;cursor:pointer}
  .scan-row.selected{border-color:var(--green);box-shadow:0 0 0 2px rgba(74,222,128,.2)}
  .tabs{display:flex;border:1px solid var(--border);border-radius:8px;overflow:hidden}
  .tab{flex:1;background:#0a0f0d;color:var(--muted);border:none;padding:.65rem;cursor:pointer;font-weight:600}
  .tab.active{background:var(--green);color:#0a0f0d}
  .form-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:.8rem}
  .modal-footer{display:flex;gap:.7rem;justify-content:flex-end}
  .btn{background:var(--blue);color:var(--text);padding:.65rem 1.1rem}
  .btn:hover{background:#3b82f6;transform:translateY(-1px)}
  .btn-cancel{background:transparent;border:1px solid var(--border);color:var(--muted)}
  .btn-confirm{background:var(--green);color:#0a0f0d}
  .btn-confirm:hover{background:#22c55e}
  .btn:disabled{opacity:.45;cursor:not-allowed;transform:none}
  .footer{text-align:center;color:var(--muted);font-size:.75rem;margin-top:auto;padding:1rem;border-top:1px solid var(--border);width:100%}
  @media(max-width:768px){
    .container{padding:1rem .5rem}
    h1{font-size:1.4rem}
    .grid,.form-grid{grid-template-columns:1fr}
    .card{padding:1rem}
    .card .val{font-size:1.5rem}
    .alert-zone{left:.75rem;right:.75rem;width:auto}
  }
</style>
</head>
<body>
<div class="container">
  <h1>LeafLink</h1>
  <p class="subtitle">Real-time plant monitoring & control</p>

  <div class="status-bar">
    <div class="dot" id="pico-dot"></div>
    <span id="pico-label">Pico: <strong>waiting</strong></span>
    <div class="separator"></div>
    <div class="dot" id="ws-dot"></div>
    <span id="ws-label">WebSocket: <strong>connecting</strong></span>
    <div class="separator"></div>
    <span id="uptime-label">Uptime: <strong>0s</strong></span>
    <div class="separator"></div>
    <span onclick="toggleLog()" style="cursor:pointer;user-select:none" title="Toggle log">Log</span>
  </div>

  <section class="section">
    <div class="section-header">
      <span class="section-title">Plant Nodes</span>
      <div class="section-line"></div>
    </div>
    <div class="plant-nodes" id="plant-nodes"></div>
  </section>

  <section class="section" id="stats-section" style="display:none">
    <div class="section-header">
      <span class="section-title" id="stats-title">Sensor State</span>
      <div class="section-line"></div>
      <button class="node-btn" onclick="refreshStates()">Refresh States</button>
    </div>
    <div class="grid">
      <div class="card">
        <div class="lbl">Soil Moisture</div>
        <div class="val" id="moist-val">--</div>
        <div class="sub" id="moist-sub">Waiting for report</div>
        <div class="bar-bg"><div class="bar green" id="moist-bar" style="width:0%"></div></div>
      </div>
      <div class="card">
        <div class="lbl">UV Index</div>
        <div class="val" id="uv-val">--</div>
        <div class="sub" id="uv-sub">Waiting for report</div>
        <div class="bar-bg"><div class="bar green" id="uv-bar" style="width:0%"></div></div>
      </div>
      <div class="card">
        <div class="lbl">Water Level</div>
        <div class="val" id="water-val">--</div>
        <div class="sub" id="water-sub">Waiting for report</div>
        <div class="bar-bg"><div class="bar green" id="water-bar" style="width:0%"></div></div>
      </div>
      <div class="card">
        <div class="lbl">Battery</div>
        <div class="val" id="batt-val">--</div>
        <div class="sub" id="batt-sub">Waiting for report</div>
        <div class="bar-bg"><div class="bar green" id="batt-bar" style="width:0%"></div></div>
      </div>
      <div class="card">
        <div class="lbl">Pump Status</div>
        <div class="val" id="pump-val">Idle</div>
        <div class="sub" id="pump-sub">Auto-triggered by Pico</div>
      </div>
      <div class="card">
        <div class="lbl">Last Update</div>
        <div class="val" id="update-val">--</div>
        <div class="sub" id="update-sub">No report yet</div>
      </div>
    </div>
  </section>

  <div class="log-panel" id="log"></div>
  <div class="footer">
    <div>ESP32-S3 Master Node | LeafLink v1.0</div>
    <div>192.168.4.1:80</div>
  </div>
</div>

<div class="alert-zone"><div class="alerts" id="alerts"></div></div>

<div class="modal-overlay" id="modal-overlay">
  <div class="modal">
    <div class="modal-header">
      <div class="modal-title">Add Plant Node</div>
      <button class="modal-close" onclick="closeModal()">x</button>
    </div>

    <div>
      <label>Available Irrigation Nodes</label>
      <div class="scan-list" id="scan-list"></div>
      <div class="empty-state" id="pairing-status">No Pico selected</div>
    </div>

    <div class="tabs">
      <button class="tab active" id="tab-library" onclick="setTab('library')">Library</button>
      <button class="tab" id="tab-custom" onclick="setTab('custom')">My Own</button>
    </div>

    <div id="library-panel">
      <label>Plant Profile</label>
      <select id="library-select"></select>
    </div>

    <div id="custom-panel" style="display:none">
      <div class="form-grid">
        <div><label>Plant name</label><input id="custom-name" placeholder="e.g. Tomato"/></div>
        <div><label>Moisture threshold %</label><input id="custom-moisture" type="number" min="0" max="100" value="40"/></div>
        <div><label>Dry calibration</label><input id="custom-dry" type="number" min="0" max="4095" value="1023"/></div>
        <div><label>Wet calibration</label><input id="custom-wet" type="number" min="0" max="4095" value="0"/></div>
        <div><label>UV alert threshold</label><input id="custom-uv" type="number" min="0" step="0.1" value="6"/></div>
        <div><label>Pump target oz/day</label><input id="custom-pump-target" type="number" min="0" step="0.1" value="1"/></div>
        <div><label>Pump flow oz/sec</label><input id="custom-pump-flow" type="number" min="0" step="0.01" value="0"/></div>
        <div><label>Tank capacity oz</label><input id="custom-tank" type="number" min="0" step="1" value="128"/></div>
        <div><label>Tank minimum %</label><input id="custom-tank-min" type="number" min="0" max="100" value="10"/></div>
        <div><label>Wake interval seconds</label><input id="custom-sleep" type="number" min="1" step="1" value="600"/></div>
      </div>
    </div>

    <div class="modal-footer">
      <button class="btn btn-cancel" onclick="closeModal()">Cancel</button>
      <button class="btn btn-confirm" id="send-config-btn" onclick="saveNode()" disabled>Set Configuration</button>
    </div>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
const STORAGE_KEY = 'leaflink_nodes_v2';
const PAIRING_PASSWORD = '12345678';

const PLANT_PROFILES = {
  'Recao': {moisture:55, uv:3, pumpTarget:1.0, pumpFlow:0, tank:128, tankMin:10, sleepSec:600, dry:1023, wet:0},
  'Oregano Brujo': {moisture:35, uv:7, pumpTarget:.6, pumpFlow:0, tank:128, tankMin:10, sleepSec:1200, dry:1023, wet:0},
  'Oregano Regular': {moisture:30, uv:9, pumpTarget:.6, pumpFlow:0, tank:128, tankMin:10, sleepSec:1200, dry:1023, wet:0},
  'Cilantro': {moisture:45, uv:4, pumpTarget:1.1, pumpFlow:0, tank:128, tankMin:10, sleepSec:600, dry:1023, wet:0},
  'Romero': {moisture:25, uv:10, pumpTarget:.5, pumpFlow:0, tank:128, tankMin:10, sleepSec:1800, dry:1023, wet:0},
  'Albahaca': {moisture:45, uv:8, pumpTarget:1.0, pumpFlow:0, tank:128, tankMin:10, sleepSec:600, dry:1023, wet:0},
  'Yerba Buena': {moisture:55, uv:5, pumpTarget:1.1, pumpFlow:0, tank:128, tankMin:10, sleepSec:600, dry:1023, wet:0},
  'Gandules': {moisture:35, uv:10, pumpTarget:.8, pumpFlow:0, tank:128, tankMin:10, sleepSec:1200, dry:1023, wet:0},
  'Savila': {moisture:15, uv:10, pumpTarget:.3, pumpFlow:0, tank:128, tankMin:10, sleepSec:3600, dry:1023, wet:0},
  'Laurel': {moisture:40, uv:8, pumpTarget:.7, pumpFlow:0, tank:128, tankMin:10, sleepSec:1200, dry:1023, wet:0}
};

let nodes = JSON.parse(localStorage.getItem(STORAGE_KEY) || '[]');
let selectedNodeId = nodes[0]?.id || null;
let selectedPairingSsid = '';
let scanResults = [];
let scanPollTimer = null;
let activeTab = 'library';
let latestStatus = null;
let picoWifiReady = false;
let picoNeedsConfig = false;
let picoWifiStatus = 'No Pico selected';
let startTime = Date.now();
let ws;

function saveNodes(){ localStorage.setItem(STORAGE_KEY, JSON.stringify(nodes)); }
function log(type,msg){
  const ts = new Date().toLocaleTimeString('en-US',{hour12:false});
  $('log').innerHTML = `<div class="log-line"><span class="log-time">${ts}</span><span class="log-type">${type}</span><span>${msg}</span></div>` + $('log').innerHTML;
}
function addAlert(msg,type='warning'){
  const el = document.createElement('div');
  el.className = 'alert-item ' + (type === 'error' ? 'error' : type === 'success' ? 'success' : '');
  el.innerHTML = `<span class="alert-text">${msg}</span><span class="alert-close" onclick="this.parentElement.remove()">x</span>`;
  $('alerts').prepend(el);
  log(type,msg);
  setTimeout(()=>el.remove(),8000);
}
function toggleLog(){ $('log').style.display = $('log').style.display === 'block' ? 'none' : 'block'; }

function renderLibrary(){
  $('library-select').innerHTML = Object.keys(PLANT_PROFILES).map(name => `<option value="${name}">${name}</option>`).join('');
}
function renderNodes(){
  const wrap = $('plant-nodes');
  const cards = nodes.map(node => `
    <div class="plant-node ${node.id === selectedNodeId ? 'selected' : ''}" onclick="selectNode(${node.id})">
      <div class="plant-node-label">Irrigation Node ${node.id}</div>
      <div class="plant-node-name">${node.plant}</div>
      <div class="plant-node-meta">${node.ssid}<br/>Moisture ${node.config.soil_moisture.threshold_percent}% | UV ${node.config.uv.alert_threshold}</div>
      <div class="node-actions">
        <button class="node-btn" onclick="event.stopPropagation();openModal(${node.id})">Change</button>
        <button class="node-btn remove" onclick="event.stopPropagation();removeNode(${node.id})">Remove</button>
      </div>
    </div>`).join('');

  wrap.innerHTML = cards + `<button class="add-node-btn" onclick="openModal()">+<br/>Add Plant</button>`;
  if (nodes.length === 0) {
    wrap.insertAdjacentHTML('afterbegin','<div class="empty-state">No plant nodes configured. Add a plant to pair with an available Irrigation node.</div>');
  }
  updateStatsVisibility();
}
function selectNode(id){ selectedNodeId = id; renderNodes(); renderStatus(latestStatus); }
function removeNode(id){
  nodes = nodes.filter(node => node.id !== id);
  selectedNodeId = nodes[0]?.id || null;
  saveNodes();
  renderNodes();
}

function updateStatsVisibility(){
  const hasPlant = nodes.some(node => node.id === selectedNodeId);
  $('stats-section').style.display = hasPlant ? 'block' : 'none';
  if (hasPlant) $('stats-title').textContent = 'Sensor State - ' + nodes.find(node => node.id === selectedNodeId).plant;
}
function setTab(tab){
  activeTab = tab;
  $('tab-library').classList.toggle('active',tab === 'library');
  $('tab-custom').classList.toggle('active',tab === 'custom');
  $('library-panel').style.display = tab === 'library' ? 'block' : 'none';
  $('custom-panel').style.display = tab === 'custom' ? 'block' : 'none';
}
function openModal(existingId=null){
  const existing = nodes.find(node => node.id === existingId);
  selectedPairingSsid = existing?.ssid || '';
  $('modal-overlay').classList.add('open');
  setTab('library');
  loadPairingNodes(false);
}
function closeModal(){
  $('modal-overlay').classList.remove('open');
  clearTimeout(scanPollTimer);
}
function modalIsOpen(){ return $('modal-overlay').classList.contains('open'); }

async function fetchNodes(refresh=false){
  const res = await fetch('/api/nodes' + (refresh ? '?refresh=1' : ''));
  return await res.json();
}
function updateConfigButton(){
  const btn = $('send-config-btn');
  if (!btn) return;
  const ready = !!selectedPairingSsid && picoWifiReady && picoNeedsConfig;
  btn.disabled = !ready;
  btn.textContent = ready ? 'Set Configuration' :
    (selectedPairingSsid ? (picoWifiReady ? 'Waiting for Pico request...' : 'Connecting to Pico...') : 'Set Configuration');
  if ($('pairing-status')) $('pairing-status').textContent = picoWifiStatus || 'No Pico selected';
}
function applyScanData(data){
  scanResults = data.nodes || [];
  picoWifiReady = !!data.pico_wifi_connected;
  picoNeedsConfig = !!data.pico_needs_config || scanResults.some(node => node.selected && node.needs_config);
  picoWifiStatus = data.pico_wifi_status || (picoWifiReady ? 'Pico WiFi connected' : 'Waiting for Pico WiFi');
  if (data.selected_ssid) selectedPairingSsid = data.selected_ssid;
}
function autoSelectPairingNode(){
  if (scanResults.length === 0) {
    selectedPairingSsid = '';
    updateConfigButton();
    return;
  }
  const selectedStillVisible = scanResults.some(node => node.ssid === selectedPairingSsid);
  if (!selectedStillVisible) selectedPairingSsid = scanResults[0].ssid;
  updateConfigButton();
}
function scheduleScanPoll(){
  clearTimeout(scanPollTimer);
  scanPollTimer = setTimeout(async () => {
    try {
      const data = await fetchNodes(false);
      applyScanData(data);
      autoSelectPairingNode();
      renderScanResults(data.scanning);
      if (modalIsOpen()) scheduleScanPoll();
    } catch (err) {
      $('scan-list').innerHTML = '<div class="empty-state">Scan failed. Check ESP32 serial output.</div>';
      addAlert('Scan failed: ' + err.message, 'error');
    }
  }, 3000);
}
async function scanNodes(){
  clearTimeout(scanPollTimer);
  $('scan-list').innerHTML = '<div class="empty-state">Scanning for Irrigation nodes in pairing mode...</div>';
  try {
    const data = await fetchNodes(true);
    applyScanData(data);
    autoSelectPairingNode();
    renderScanResults(data.scanning);
    if (modalIsOpen()) scheduleScanPoll();
  } catch (err) {
    $('scan-list').innerHTML = '<div class="empty-state">Scan failed. Check ESP32 serial output.</div>';
    addAlert('Scan failed: ' + err.message, 'error');
  }
}
async function loadPairingNodes(refresh=false){
  clearTimeout(scanPollTimer);
  $('scan-list').innerHTML = '<div class="empty-state">Checking Pico connection...</div>';
  try {
    const data = await fetchNodes(refresh);
    applyScanData(data);
    autoSelectPairingNode();
    renderScanResults(data.scanning);
    if (modalIsOpen()) scheduleScanPoll();
  } catch (err) {
    $('scan-list').innerHTML = '<div class="empty-state">Could not read Pico connection state.</div>';
    addAlert('Could not read Pico state: ' + err.message, 'error');
  }
}
function renderScanResults(scanning=false){
  if (scanning && scanResults.length === 0) {
    $('scan-list').innerHTML = '<div class="empty-state">Scanning for Irrigation nodes in pairing mode...</div>';
    return;
  }
  if (scanResults.length === 0) {
    $('scan-list').innerHTML = '<div class="empty-state">No Irrigation nodes found. Put the Pico in pairing mode and refresh.</div>';
    return;
  }
  autoSelectPairingNode();
  const rows = scanResults.map((node,index) => `
    <button class="scan-row ${node.ssid === selectedPairingSsid ? 'selected' : ''}" onclick="selectPairingNode(${index})">
      <strong>${node.ssid}</strong><br/><span style="color:var(--muted)">RSSI ${node.rssi} dBm${node.ssid === selectedPairingSsid ? ' | ' + (picoWifiReady ? (picoNeedsConfig ? 'ready for config' : 'connected') : 'connecting') : ''}</span>
    </button>`).join('');
  const suffix = scanning ? '<div class="empty-state">Still scanning...</div>' : '';
  $('scan-list').innerHTML = rows + suffix;
  updateConfigButton();
}
async function selectPairingNode(index){
  selectedPairingSsid = scanResults[index]?.ssid || '';
  picoWifiReady = false;
  picoNeedsConfig = false;
  picoWifiStatus = 'ESP32 will connect to the selected Pico';
  renderScanResults();
  updateConfigButton();
  if (!selectedPairingSsid) return;
  try {
    const res = await fetch('/api/select-node?ssid=' + encodeURIComponent(selectedPairingSsid));
    if (!res.ok) throw new Error('HTTP ' + res.status);
    scheduleScanPoll();
  } catch (err) {
    addAlert('Could not select Pico: ' + err.message, 'error');
  }
}

function configFromProfile(profile){
  return {
    soil_moisture:{threshold_percent:profile.moisture,dry_cal:profile.dry,wet_cal:profile.wet},
    uv:{alert_threshold:profile.uv},
    pump:{target_oz_per_day:profile.pumpTarget,flow_rate_oz_per_sec:profile.pumpFlow},
    water:{tank_capacity_oz:profile.tank,tank_min_threshold_percent:profile.tankMin},
    sleep:{sleep_interval_ms:Math.min(Math.max(1, profile.sleepSec || 1) * 1000,4294967295)}
  };
}
function customProfile(){
  return {
    moisture:+$('custom-moisture').value, uv:+$('custom-uv').value,
    pumpTarget:+$('custom-pump-target').value, pumpFlow:+$('custom-pump-flow').value,
    tank:+$('custom-tank').value, tankMin:+$('custom-tank-min').value,
    sleepSec:+$('custom-sleep').value, dry:+$('custom-dry').value, wet:+$('custom-wet').value
  };
}
async function saveNode(){
  if (!selectedPairingSsid) { addAlert('Select an available Irrigation node first.','error'); return; }
  if (!picoWifiReady) { addAlert('Wait until the ESP32 is connected to the Pico before setting configuration.','error'); return; }
  if (!picoNeedsConfig) { addAlert('Wait until the Pico requests configuration before setting configuration.','error'); return; }

  const plant = activeTab === 'library' ? $('library-select').value : ($('custom-name').value.trim() || 'Custom Plant');
  const profile = activeTab === 'library' ? PLANT_PROFILES[plant] : customProfile();
  const config = configFromProfile(profile);
  const payload = {...config, pairing:{ssid:selectedPairingSsid,password:PAIRING_PASSWORD}};

  try {
    const res = await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
    if (!res.ok) {
      let detail = 'HTTP ' + res.status;
      try {
        const body = await res.json();
        if (body.error) detail = body.error;
      } catch (_) {}
      throw new Error(detail);
    }

    const id = selectedNodeId || 1;
    const existing = nodes.find(node => node.id === id);
    const nextNode = {id, plant, ssid:selectedPairingSsid, config};
    if (existing) Object.assign(existing,nextNode); else nodes.push(nextNode);
    selectedNodeId = id;
    saveNodes();
    renderNodes();
    closeModal();
    addAlert('Configuration queued. Waiting for Pico confirmation.','success');
  } catch (err) {
    addAlert('Could not queue config: ' + err.message,'error');
  }
}

function setBar(id,pct,low,high){
  const p = Math.min(100,Math.max(0,pct || 0));
  const bar = $(id);
  bar.style.width = p + '%';
  bar.className = 'bar ' + (p < low ? 'red' : p < high ? 'yellow' : 'green');
}
function valClass(pct,low,high){ return pct < low ? 'val bad' : pct < high ? 'val warn' : 'val good'; }
function renderStatus(d){
  latestStatus = d || latestStatus;
  if (latestStatus) {
    picoWifiReady = !!latestStatus.pico_wifi_connected;
    picoNeedsConfig = !!latestStatus.pico_needs_config;
    picoWifiStatus = latestStatus.pico_wifi_status || picoWifiStatus;
    if (latestStatus.selected_ssid) selectedPairingSsid = latestStatus.selected_ssid;
    updateConfigButton();
  }
  const reportConnected = !!(latestStatus && latestStatus.connected);
  const wifiConnected = !!(latestStatus && latestStatus.pico_wifi_connected);
  const wifiConnecting = !!(latestStatus && latestStatus.pico_wifi_connecting);
  $('pico-dot').className = 'dot' + ((reportConnected || wifiConnected) ? ' ok' : '');
  $('pico-label').innerHTML = `Pico: <strong>${reportConnected ? 'report received' : wifiConnected ? 'WiFi connected' : wifiConnecting ? 'connecting' : 'waiting'}</strong>`;
  if (!latestStatus || !nodes.some(node => node.id === selectedNodeId)) { updateStatsVisibility(); return; }

  updateStatsVisibility();
  const m = latestStatus.moisture || {}, uv = latestStatus.uv || {}, water = latestStatus.water || {}, power = latestStatus.power || {};
  const mp = m.percent ?? 0;
  $('moist-val').textContent = mp.toFixed(1) + '%';
  $('moist-val').className = valClass(mp,30,50);
  $('moist-sub').textContent = m.error ? 'Sensor error' : (m.needs_water ? 'Needs water' : 'Optimal');
  setBar('moist-bar',mp,30,50);

  const uvIndex = uv.uv_index ?? 0;
  $('uv-val').textContent = uvIndex.toFixed(1);
  $('uv-val').className = uvIndex >= 6 ? 'val bad' : uvIndex >= 3 ? 'val warn' : 'val good';
  $('uv-sub').textContent = uv.error ? 'Sensor error' : (uv.alert ? 'High UV alert' : 'Safe');
  setBar('uv-bar',(uvIndex / 11) * 100,27,55);

  const wp = water.percent ?? 0;
  $('water-val').textContent = wp.toFixed(1) + '%';
  $('water-val').className = valClass(wp,20,40);
  $('water-sub').textContent = (water.oz ?? 0).toFixed(1) + ' oz remaining';
  setBar('water-bar',wp,20,40);

  const bp = power.percent ?? 0;
  $('batt-val').textContent = bp.toFixed(0) + '%';
  $('batt-val').className = valClass(bp,20,40);
  $('batt-sub').textContent = (power.voltage ?? 0).toFixed(2) + ' V';
  setBar('batt-bar',bp,20,40);

  const running = latestStatus.pump_active || latestStatus.pump?.running;
  $('pump-val').textContent = running ? 'Running' : 'Idle';
  $('pump-val').className = running ? 'val warn' : 'val good';
  $('update-val').textContent = new Date().toLocaleTimeString('en-US',{hour12:false});
  $('update-sub').textContent = 'Latest Pico wakeup report';
}

function connectWs(){
  ws = new WebSocket('ws://' + location.hostname + '/ws');
  ws.onopen = () => { $('ws-dot').className='dot ok'; $('ws-label').innerHTML='WebSocket: <strong>live</strong>'; log('info','WebSocket connected'); };
  ws.onclose = () => { $('ws-dot').className='dot'; $('ws-label').innerHTML='WebSocket: <strong>reconnecting</strong>'; setTimeout(connectWs,2000); };
  ws.onerror = () => addAlert('WebSocket error','error');
  ws.onmessage = evt => {
    try {
      const data = JSON.parse(evt.data);
      if (data.alert) { addAlert(data.alert,data.alert_type || 'warning'); return; }
      renderStatus(data);
    } catch (err) { log('error','Bad message: ' + err.message); }
  };
}
async function loadStatus(){
  try { renderStatus(await (await fetch('/api/status')).json()); } catch (_) {}
}
async function refreshStates(){
  try {
    await loadStatus();
    addAlert('Loaded Pico states stored on ESP32.','success');
  } catch (err) {
    addAlert('Could not refresh states: ' + err.message,'error');
  }
}
function updateUptime(){
  const s = Math.floor((Date.now() - startTime) / 1000);
  $('uptime-label').innerHTML = `Uptime: <strong>${Math.floor(s/60)}m ${s%60}s</strong>`;
}

renderLibrary();
renderNodes();
loadStatus();
connectWs();
setInterval(updateUptime,1000);
</script>
</body>
</html>
)rawliteral";
