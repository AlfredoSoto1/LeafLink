
#include <Arduino.h>

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
  .alert-zone{position:fixed;top:1rem;right:1rem;width:320px;z-index:99;pointer-events:none;}
  .alerts{display:flex;flex-direction:column;gap:.5rem}
  .alert-item{pointer-events:all;background:rgba(28,16,0,0.8);border-left:4px solid var(--yellow);
            border-radius:6px;padding:.8rem 1rem;font-size:.85rem;
            animation:fadein .3s ease;display:flex;align-items:center;gap:8px}  .alert-item.error{border-left-color:var(--red);background:rgba(30,10,10,0.8)}
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

  
  /* ── Plant Library Section ── */
  .plant-section{width:100%;max-width:1000px;margin-bottom:1.5rem}
  .plant-section-header{display:flex;align-items:center;gap:10px;margin-bottom:1rem}
  .plant-section-title{font-size:.7rem;text-transform:uppercase;letter-spacing:2px;color:var(--muted);font-weight:600}
  .plant-section-line{flex:1;height:1px;background:var(--border)}
  .plant-nodes{display:flex;flex-wrap:wrap;gap:1rem;align-items:flex-start}
  .plant-node{background:var(--surface);border:1px solid var(--border);border-radius:var(--r);padding:1.2rem 1.4rem;min-width:180px;box-shadow:var(--shadow);display:flex;flex-direction:column;gap:8px;transition:transform .2s,box-shadow .2s}
  .plant-node:hover{transform:translateY(-2px);box-shadow:0 6px 16px rgba(0,0,0,0.6)}
  .plant-node-label{font-size:.68rem;text-transform:uppercase;letter-spacing:1px;color:var(--muted);font-weight:600}
  .plant-node-name{font-size:1.1rem;font-weight:700;color:var(--green);min-height:1.4rem}
  .plant-node-name.empty{color:var(--muted);font-style:italic;font-weight:400;font-size:.9rem}
  .plant-node-actions{display:flex;gap:6px;margin-top:4px}
  .node-btn{background:transparent;border:1px solid var(--border);border-radius:6px;color:var(--muted);font-size:.72rem;padding:.3rem .7rem;cursor:pointer;transition:all .2s}
  .node-btn:hover{border-color:var(--green);color:var(--green)}
  .node-btn.remove:hover{border-color:var(--red);color:var(--red)}
  .plant-node.selected{border-color:var(--green);box-shadow:0 0 0 2px rgba(74,222,128,0.3);}
  .node-info-bar{width:100%;max-width:1000px;background:var(--surface);border:1px solid var(--border);
                 border-radius:var(--r);padding:.8rem 1.2rem;margin-bottom:1rem;
                 font-size:.82rem;color:var(--muted);display:flex;align-items:center;gap:8px;}
  .node-info-bar strong{color:var(--green);}
  .add-node-btn{background:transparent;border:2px dashed var(--border);border-radius:var(--r);padding:1.2rem 1.8rem;min-width:140px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;cursor:pointer;transition:all .2s;color:var(--muted);font-size:.82rem}
  .add-node-btn:hover{border-color:var(--green);color:var(--green);background:rgba(74,222,128,0.04)}
  .add-node-btn .plus{font-size:1.6rem;line-height:1;font-weight:300}
  .modal-overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,0.75);backdrop-filter:blur(4px);z-index:100;align-items:center;justify-content:center;padding:1rem}
  .modal-overlay.open{display:flex;animation:fadein .2s ease}
  .modal{background:var(--surface);border:1px solid var(--border);border-radius:16px;padding:1.8rem;width:100%;max-width:420px;box-shadow:0 20px 60px rgba(0,0,0,0.6);display:flex;flex-direction:column;gap:1.2rem}
  .modal-header{display:flex;align-items:center;justify-content:space-between}
  .modal-title{font-size:1.1rem;font-weight:700;color:var(--green);display:flex;align-items:center;gap:8px}
  .modal-close{background:none;border:none;color:var(--muted);font-size:1.4rem;cursor:pointer;line-height:1;padding:4px;border-radius:4px;transition:color .2s}
  .modal-close:hover{color:var(--text)}
  .modal-divider{display:flex;align-items:center;gap:10px;color:var(--muted);font-size:.75rem;text-transform:uppercase;letter-spacing:1px}
  .modal-divider::before,.modal-divider::after{content:'';flex:1;height:1px;background:var(--border)}
  .modal label{display:block;font-size:.75rem;color:var(--muted);margin-bottom:6px;text-transform:uppercase;letter-spacing:.5px}
  .modal select,.modal input{width:100%;padding:.7rem .9rem;border-radius:8px;border:1px solid var(--border);background:#0a0f0d;color:var(--text);font-size:.9rem;outline:none;transition:border-color .2s}
  .modal select:focus,.modal input:focus{border-color:var(--green)}
  .modal-node-label{font-size:.78rem;color:var(--muted);background:rgba(74,222,128,0.06);border:1px solid var(--border);border-radius:8px;padding:.6rem .9rem;display:flex;align-items:center;gap:8px}
  .modal-node-label strong{color:var(--green)}
  .modal-footer{display:flex;gap:.7rem;justify-content:flex-end}
  .btn{border:none;border-radius:8px;padding:.6rem 1.2rem;cursor:pointer;font-size:.88rem;font-weight:600;transition:all .2s}
  .btn-cancel{background:transparent;border:1px solid var(--border);color:var(--muted)}
  .btn-cancel:hover{border-color:var(--muted);color:var(--text)}
  .btn-confirm{background:var(--green);color:#0a0f0d}
  .btn-confirm:hover{background:#22c55e;transform:translateY(-1px)}

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
    <div class="separator"></div>
    <span id="log-toggle" onclick="toggleLog()" style="cursor:pointer;user-select:none;" title="Toggle log">⚙️</span>
  </div>

  <div class="node-info-bar" id="node-info-bar">
    📊 Viewing: <strong id="viewing-node-label">No node selected — click a node below</strong>
  </div>

  <div id="log-wrapper" style="display:none;width:100%;max-width:1000px;margin-bottom:1.5rem;">
    <div class="log-panel" id="log"></div>
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
      <div class="lbl">📊 Last Update</div>
      <div class="val" id="update-val">—</div>
      <div class="sub" id="update-sub">—</div>
    </div>
  </div>


  <div class="plant-section">
    <div class="plant-section-header">
      <span class="plant-section-title">🪴 Plant Nodes</span>
      <div class="plant-section-line"></div>
    </div>
    <div class="plant-nodes" id="plant-nodes"></div>
  </div>

  <div class="footer">
    <div>ESP32-S3 Master Node | LeafLink v1.0</div>
    <div id="connection-info">192.168.4.1:80</div>
  </div>
</div>

<div class="alert-zone">
  <div class="alerts" id="alerts"></div>
</div>

<div class="modal-overlay" id="modal-overlay">
  <div class="modal">
    <div class="modal-header">
      <div class="modal-title">🌱 Add Plant</div>
      <button class="modal-close" onclick="closeModal()">✕</button>
    </div>
    <div class="modal-node-label">
      Assigning to: <strong id="modal-node-name">Node 1</strong>
    </div>
    <div>
      <label>Pick from library</label>
      <select id="modal-library">
        <option value="">— Choose a plant —</option>
        <option>Recao</option>
        <option>Oregano Brujo</option>
        <option>Oregano Regular</option>
        <option>Cilantro</option>
        <option>Romero</option>
        <option>Albahaca</option>
        <option>Ruda</option>
        <option>Yerba Buena</option>
        <option>Hoja de Menta</option>
        <option>Gandules</option>
        <option>Sávila</option>
        <option>Laurel</option>
      </select>
    </div>
    <div class="modal-divider">or</div>
    <div>
      <label>Add my own plant</label>
      <input id="modal-custom" type="text" placeholder="e.g. Tomate, Ají Caballero…"/>
    </div>

    <div id="custom-params" style="display:none; flex-direction:column; gap:.8rem;">
      <div style="display:grid; grid-template-columns:1fr 1fr; gap:.8rem;">
        <div>
          <label>Moisture min %</label>
          <input id="custom-moist-min" type="number" min="0" max="100" placeholder="e.g. 40"/>
        </div>
        <div>
          <label>Moisture max %</label>
          <input id="custom-moist-max" type="number" min="0" max="100" placeholder="e.g. 65"/>
        </div>
      </div>
      <div style="display:grid; grid-template-columns:1fr 1fr; gap:.8rem;">
        <div>
          <label>UV max (0-11)</label>
          <input id="custom-uv-max" type="number" min="0" max="11" placeholder="e.g. 5"/>
        </div>
        <div>
          <label>Water every (days)</label>
          <input id="custom-water-days" type="number" min="1" placeholder="e.g. 3"/>
        </div>
      </div>
    </div>
    <div class="modal-footer">
      <button class="btn btn-cancel" onclick="closeModal()">Cancel</button>
      <button class="btn btn-confirm" onclick="confirmPlant()">Add Plant</button>
    </div>
  </div>
</div>

<div class="modal-overlay" id="confirm-overlay">
  <div class="modal">
    <div class="modal-header">
      <div class="modal-title">⚠️ Remove Plant</div>
      <button class="modal-close" onclick="closeConfirm()">✕</button>
    </div>
    <div class="modal-node-label">
      Type the plant name to confirm removal: <strong id="confirm-plant-name"></strong>
    </div>
    <div>
      <label>Plant name</label>
      <input id="confirm-input" type="text" placeholder="Type plant name here…"/>
    </div>
    <div id="confirm-error" style="display:none; color:var(--red); font-size:.78rem;">
      ✕ Name doesn't match. Try again.
    </div>
    <div class="modal-footer">
      <button class="btn btn-cancel" onclick="closeConfirm()">Cancel</button>
      <button class="btn btn-confirm" style="background:var(--red);" onclick="confirmRemove()">Remove</button>
    </div>
  </div>
</div>
<script>
const PLANT_PROFILES = {
  "Recao":           { moisture: [50, 70], uvMax: 3,  waterDays: 2  },
  "Oregano Brujo":   { moisture: [30, 50], uvMax: 7,  waterDays: 5  },
  "Oregano Regular": { moisture: [25, 45], uvMax: 9,  waterDays: 5  },
  "Cilantro":        { moisture: [40, 60], uvMax: 4,  waterDays: 2  },
  "Romero":          { moisture: [20, 40], uvMax: 10, waterDays: 7  },
  "Albahaca":        { moisture: [40, 60], uvMax: 8,  waterDays: 2  },
  "Ruda":            { moisture: [20, 40], uvMax: 10, waterDays: 7  },
  "Yerba Buena":     { moisture: [50, 70], uvMax: 5,  waterDays: 2  },
  "Hoja de Menta":   { moisture: [50, 70], uvMax: 5,  waterDays: 2  },
  "Gandules":        { moisture: [30, 50], uvMax: 10, waterDays: 5  },
  "Sávila":          { moisture: [10, 25], uvMax: 10, waterDays: 14 },
  "Laurel":          { moisture: [35, 55], uvMax: 8,  waterDays: 5  },
};

let nodes = [];
let activeNodeId = null;
let selectedNodeId = null;

function toggleLog() {
  const wrapper = document.getElementById('log-wrapper');
  const isHidden = wrapper.style.display === 'none';
  wrapper.style.display = isHidden ? 'block' : 'none';
}

function selectNode(nodeId) {
  selectedNodeId = nodeId;
  const node = nodes.find(n => n.id === nodeId);

  // Update the info bar
  const label = node && node.plant
    ? `Node ${nodeId} — ${node.plant}`
    : `Node ${nodeId} — No plant assigned`;
  document.getElementById('viewing-node-label').textContent = label;

  // Update selected highlight on cards
  document.querySelectorAll('.plant-node').forEach(el => el.classList.remove('selected'));
  const cards = document.querySelectorAll('.plant-node');
  if (cards[nodeId - 1]) cards[nodeId - 1].classList.add('selected');
}

function renderNodes() {
  const container = document.getElementById('plant-nodes');
  container.innerHTML = '';
  nodes.forEach(node => {
    const profile = node.plant ? PLANT_PROFILES[node.plant] : null;
    const el = document.createElement('div');
    el.className = 'plant-node' + (node.id === selectedNodeId ? ' selected' : '');
    el.style.cursor = 'pointer';
    el.addEventListener('click', (e) => {
      if (!e.target.classList.contains('node-btn')) selectNode(node.id);
    });    
    el.innerHTML = `
      <div class="plant-node-label">Plant Node ${node.id}</div>
      <div class="plant-node-name ${node.plant ? '' : 'empty'}">
        ${node.plant || 'No plant assigned'}
      </div>
      ${profile ? `
        <div style="font-size:.72rem;color:var(--muted);display:flex;flex-direction:column;gap:3px;margin-top:2px;">
          <span>💧 Moisture: ${profile.moisture[0]}–${profile.moisture[1]}%</span>
          <span>☀️ UV max: ${profile.uvMax}</span>
          <span>🪣 Water every: ${profile.waterDays}d</span>
        </div>` : ''}
      <div class="plant-node-actions">
        ${node.plant
          ? `<button class="node-btn" onclick="openModal(${node.id})">Change</button>
             <button class="node-btn" onclick="editProfile(${node.id})">Edit</button>
             <button class="node-btn remove" onclick="removePlant(${node.id})">Remove</button>`          : `<button class="node-btn" onclick="openModal(${node.id})">Assign</button>`}
      </div>`;
    container.appendChild(el);
  });
  const addBtn = document.createElement('button');
  addBtn.className = 'add-node-btn';
  addBtn.innerHTML = `<span class="plus">+</span><span>Add Plant</span>`;
  addBtn.onclick = () => {
    const newId = nodes.length + 1;
    nodes.push({ id: newId, plant: null });
    renderNodes();
    openModal(newId);
  };
  container.appendChild(addBtn);
}

function openModal(nodeId) {
  activeNodeId = nodeId;
  document.getElementById('modal-node-name').textContent = `Node ${nodeId}`;
  document.getElementById('modal-library').value = '';
  document.getElementById('modal-custom').value = '';
  document.getElementById('modal-overlay').classList.add('open');
}

function closeModal() {
  document.getElementById('modal-overlay').classList.remove('open');
  const node = nodes.find(n => n.id === activeNodeId);
  if (node && !node.plant) {
    nodes = nodes.filter(n => n.id !== activeNodeId);
    renderNodes();
  }
  activeNodeId = null;
}

function editProfile(nodeId) {
  const node = nodes.find(n => n.id === nodeId);
  if (!node || !node.plant) return;
  const profile = PLANT_PROFILES[node.plant];
  if (!profile) return;

  document.getElementById('modal-node-name').textContent = `Node ${nodeId} — ${node.plant}`;
  document.getElementById('modal-library').value = '';
  document.getElementById('modal-custom').value = node.plant;
  document.getElementById('custom-moist-min').value = profile.moisture[0];
  document.getElementById('custom-moist-max').value = profile.moisture[1];
  document.getElementById('custom-uv-max').value = profile.uvMax;
  document.getElementById('custom-water-days').value = profile.waterDays;
  document.getElementById('custom-params').style.display = 'flex';

  activeNodeId = nodeId;
  document.getElementById('modal-overlay').classList.add('open');
}

function confirmPlant() {
  const libraryVal = document.getElementById('modal-library').value;
  const customVal  = document.getElementById('modal-custom').value.trim();
  const chosen     = customVal || libraryVal;
  if (!chosen) { document.getElementById('modal-library').focus(); return; }

  if (customVal) {
    const moistMin  = parseInt(document.getElementById('custom-moist-min').value) || 40;
    const moistMax  = parseInt(document.getElementById('custom-moist-max').value) || 60;
    const uvMax     = parseInt(document.getElementById('custom-uv-max').value)    || 6;
    const waterDays = parseInt(document.getElementById('custom-water-days').value) || 3;
    PLANT_PROFILES[customVal] = {
      moisture: [moistMin, moistMax],
      uvMax: uvMax,
      waterDays: waterDays
    };
    const select = document.getElementById('modal-library');
    if (![...select.options].some(o => o.value === customVal)) {
      const opt = document.createElement('option');
      opt.value = customVal;
      opt.textContent = customVal;
      select.appendChild(opt);
    }
  }

  const node = nodes.find(n => n.id === activeNodeId);
  if (node) {
    if (node.plant !== chosen) node.plant = chosen;
  }  document.getElementById('modal-overlay').classList.remove('open');
  document.getElementById('custom-params').style.display = 'none';
  renderNodes();
  activeNodeId = null;
}

let pendingRemoveNodeId = null;

function removePlant(nodeId) {
  const node = nodes.find(n => n.id === nodeId);
  if (!node || !node.plant) return;
  pendingRemoveNodeId = nodeId;
  document.getElementById('confirm-plant-name').textContent = node.plant;
  document.getElementById('confirm-input').value = '';
  document.getElementById('confirm-error').style.display = 'none';
  document.getElementById('confirm-overlay').classList.add('open');
  document.getElementById('confirm-overlay').addEventListener('click', function(e) {
    if (e.target === this) closeConfirm();
  });
}

function closeConfirm() {
  document.getElementById('confirm-overlay').classList.remove('open');
  document.getElementById('confirm-error').style.display = 'none';
  pendingRemoveNodeId = null;
}

function confirmRemove() {
  const node = nodes.find(n => n.id === pendingRemoveNodeId);
  const typed = document.getElementById('confirm-input').value.trim();
  if (!node) return;

  if (typed !== node.plant) {
    document.getElementById('confirm-error').style.display = 'block';
    return;
  }

  node.plant = null;
  renderNodes();
  closeConfirm();
}

document.getElementById('modal-overlay').addEventListener('click', function(e) {
  if (e.target === this) closeModal();
});
document.getElementById('modal-library').addEventListener('change', () => {
  if (document.getElementById('modal-library').value)
    document.getElementById('modal-custom').value = '';
});
document.getElementById('modal-custom').addEventListener('input', () => {
  const val = document.getElementById('modal-custom').value;
  if (val) {
    document.getElementById('modal-library').value = '';
    document.getElementById('custom-params').style.display = 'flex';
  } else {
    document.getElementById('custom-params').style.display = 'none';
  }
});

renderNodes();
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

