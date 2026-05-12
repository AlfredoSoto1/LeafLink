# LeafLink Dashboard UI

A modern, real-time web-based plant monitoring dashboard for the ESP32-S3 master node.

## Features

### 📊 Real-Time Monitoring
- **Soil Moisture**: Shows current moisture percentage with color-coded status (red/yellow/green)
- **UV Index**: Real-time UV intensity monitoring with high-UV alerts
- **Water Level**: Tank fill percentage and remaining volume in ounces
- **Battery Status**: Power level and voltage display for the Pico sensor node
- **Pump Control**: Visual indicator showing pump running/idle status
- **Uptime**: System uptime counter that updates continuously

### 🔔 Alert System
- Color-coded alerts (success ✅, warning ⚠️, error ❌)
- Auto-dismissing notifications (8-second display)
- Manual close button for immediate dismissal
- Alert history in system logs

### 📱 Responsive Design
- Optimized for desktop, tablet, and mobile devices
- Flexible grid layout that adapts to screen size
- Touch-friendly buttons and controls
- Dark theme for eye-friendly monitoring

### 🔌 Connectivity Status
- **Pico Status**: Shows if the sensor node is connected to the AP
- **WebSocket Status**: Displays real-time connection state
- **Auto-Reconnect**: Automatic reconnection with exponential backoff
- **Connection Info**: IP address and port information

### 📝 System Logs
- Real-time activity log with timestamps
- Color-coded log types (info, warning, error, success)
- Keeps last 50 entries for troubleshooting
- Monospace font for easy reading

## How to Use

### Accessing the Dashboard

1. **Connect to the LeafLink Wi-Fi Access Point:**
   - SSID: `LeafLink-AP`
   - Password: `leaflink123`

2. **Open in Web Browser:**
   - Navigate to: `http://192.168.4.1`
   - The dashboard will load automatically

### Understanding the Display

#### Status Bar
- **Pico**: Green dot = connected, red dot = disconnected
- **WebSocket**: Green dot = live connection, red dot = connecting/disconnected
- **Uptime**: How long the system has been running

#### Sensor Cards
Each card displays:
- **Label**: Sensor name with emoji icon
- **Value**: Current reading with color coding
  - 🟢 Green: Normal operation
  - 🟡 Yellow: Warning threshold
  - 🔴 Red: Critical/alert threshold
- **Subtitle**: Additional info or status
- **Progress Bar**: Visual percentage indicator

#### Alert Zone
- Shows recent system alerts and notifications
- Each alert has an icon and timestamp in the logs
- Click the X button to dismiss an alert

#### System Log
- Bottom panel shows recent activity
- Helps troubleshoot connection or sensor issues
- Displays timestamps for all events

## Data Format

The dashboard expects WebSocket messages in JSON format:

```json
{
  "connected": true,
  "moisture": {
    "percent": 45.5,
    "needs_water": false
  },
  "uv": {
    "uv_index": 3.2,
    "alert": false
  },
  "water": {
    "percent": 75.0,
    "oz": 96.0
  },
  "power": {
    "voltage": 4.15,
    "percent": 85
  },
  "pump_active": false
}
```

For alerts:
```json
{
  "alert": "Alert message here",
  "alert_type": "warning"
}
```

Alert types: `warning`, `error`, `success`

## Thresholds

### Soil Moisture
- 🔴 Critical: < 30%
- 🟡 Warning: 30-50%
- 🟢 Good: > 50%

### Water Level
- 🔴 Critical: < 20%
- 🟡 Warning: 20-40%
- 🟢 Good: > 40%

### Battery
- 🔴 Critical: < 20%
- 🟡 Warning: 20-40%
- 🟢 Good: > 40%

### UV Index
- 🟢 Safe: < 3
- 🟡 Moderate: 3-6
- 🔴 High: > 6

## Customization

### Colors
Edit the CSS variables in the `<style>` section:
```css
:root {
  --bg: #0a0f0d;          /* Dark background */
  --surface: #121a15;     /* Card/surface color */
  --green: #4ade80;       /* Good status */
  --yellow: #facc15;      /* Warning status */
  --red: #f87171;         /* Critical status */
  --blue: #60a5fa;        /* Accent color */
  --text: #e2f0e6;        /* Text color */
  --muted: #6b8f73;       /* Muted text */
}
```

### Thresholds
Modify the `valClass()` function parameters to change when thresholds trigger:
```javascript
function valClass(pct, low, high) {
  return pct < low ? 'val bad' : pct < high ? 'val warn' : 'val good';
}
// Example: valClass(mp, 25, 45) → different thresholds
```

## Troubleshooting

### Dashboard Won't Load
1. Check WiFi connection to `LeafLink-AP`
2. Verify you're navigating to `http://192.168.4.1` (not HTTPS)
3. Check that the ESP32 is powered and booted
4. Look for error messages in the browser console (F12)

### WebSocket Connection Failing
1. Check that the ESP32 has the web server initialized
2. Verify the WebSocket endpoint is at `/ws`
3. Ensure the browser supports WebSocket (most modern browsers do)
4. Check if a firewall is blocking the connection

### Sensor Data Not Updating
1. Verify the Pico is connected to the AP (should show green dot)
2. Check that sensor readings are being sent from the Pico
3. Look at the system logs for any error messages
4. Power cycle the entire system

### Mobile Display Issues
1. The dashboard is fully responsive—should work on any device
2. If text is too small, pinch to zoom (on mobile)
3. Rotate device to landscape for larger view
4. Consider using Firefox or Chrome for best compatibility

## Technical Details

- **Embedded**: Dashboard is embedded in ESP32 flash (PROGMEM)
- **Framework**: Vanilla JavaScript (no external dependencies)
- **Size**: Optimized for embedded systems (~40KB)
- **Updates**: Real-time via WebSocket (~100ms typical latency)
- **Compatibility**: Works on any modern browser (Chrome, Firefox, Safari, Edge)

## File Locations

- **Dashboard HTML**: `include/index.html`
- **Embedded in C++**: `src/main.cpp` (DASHBOARD_HTML constant)
- **WebSocket Handler**: Implementation in Tasks.cpp

## Future Enhancements

Potential features for future versions:
- Configuration panel to adjust thresholds
- Historical data graphs
- Manual pump control
- System settings interface
- Alert notification preferences
- Data export (CSV/JSON)
- Multi-plant support
