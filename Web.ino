/**********************************************************************
 * 2025 (Copyleft) Anton Polishchuk
 * ESP32 Well Water Pump project with Web Interface and WiFi.
 * Authtor Contact: gmail account: antonvp
 * LICENSE:
 * This software is licensed under the MIT License.
 * Redistribution, modification, or forks must include this banner.
 * DISCLAIMER:
 * - This software is provided "as is," without warranty of any kind.
 * - The author is not responsible for any damages, data loss, or issues
 *   arising from its use, under any circumstances.
 * - For more information, refer to the full license terms.
 **********************************************************************/

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <pgmspace.h>
#include "PumpStates.h"
#include "WiFiStates.h"

// Forward declarations
void stopStateTimer();
float getPumpCurrent();
String getLogsForWeb();
void logStateTransition(PumpState state);

// Helper function to convert PumpState to string
String getStateString(PumpState state) {
  switch (state) {
    case WELL_RECOVERY: return "WELL_RECOVERY";
    case STARTING_PUMP: return "STARTING_PUMP";
    case PUMPING_WATER: return "PUMPING_WATER";
    case STOPPING_PUMP: return "STOPPING_PUMP";
    case ERROR_RED: return "ERROR_RED";
    case ERROR_PURPLE: return "ERROR_PURPLE";
    case ERROR_YELLOW: return "ERROR_YELLOW";
    case HALTED_PUMP: return "HALTED_PUMP";
    default: return "UNKNOWN";
  }
}

// Helper function to get state emoji
String getStateEmoji(PumpState state) {
  switch (state) {
    case WELL_RECOVERY: return "🟢";
    case STARTING_PUMP: return "⚪";
    case PUMPING_WATER: return "🔵";
    case STOPPING_PUMP: return "⚪";
    case ERROR_RED: return "🔴";
    case ERROR_PURPLE: return "🟣";
    case ERROR_YELLOW: return "🟡";
    case HALTED_PUMP: return "⚫";
    default: return "⭕";
  }
}

// Helper function to format epoch timestamp
String formatTimestamp(int64_t timestamp) {
  if (timestamp <= 0) return "Never";
  time_t t = (time_t)timestamp;
  struct tm *tm = localtime(&t);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d-%b-%y %H:%M:%S", tm);
  return String(buffer);
}


// CSS content in PROGMEM
const char css_content[] PROGMEM = R"=====(
body {
  background-color: #f3f4f6;
  color: #111827;
  font-family: 'Inter', 'Arial', sans-serif;
  margin: 0;
  padding: 0;
  line-height: 1.5;
}
@media (prefers-color-scheme: dark) {
  body {
    background-color: #111827;
    color: #9ca3af;
  }
}
@media (prefers-color-scheme: dark) {
  .bg-odd { background-color: #2d3748; }
  .bg-even { background-color: #1a202c; }
}
@media (prefers-color-scheme: light) {
  .bg-odd { background-color: #edf2f7; }
  .bg-even { background-color: #ffffff; }
}
.sensor-row {
  display: flex;
  justify-content: space-between;
  padding: 1rem;
  margin-bottom: 2px;
}
.sensor-row p {
  margin: 0;
  font-size: 1.125rem;
  color: #111827;
}
@media (prefers-color-scheme: dark) {
  .sensor-row p {
    color: #9ca3af;
  }
}
.sensor-row p:first-child {
  font-weight: 500;
}
nav {
  position: fixed;
  top: 0;
  width: 100%;
  background-color: #ffffff;
  box-shadow: 0 2px 5px rgba(0,0,0,0.1);
  z-index: 10;
}
.nav-container {
  max-width: 500px;
  margin: 0 auto;
  display: flex;
  justify-content: center;
  gap: 0.5rem;
  padding: 1.1rem;
}
.nav-btn {
  transition: all 0.3s ease;
  padding: 0.5rem 1rem;
  color: #111827;
  background-color: #e5e7eb;
  border: none;
  border-radius: 0.375rem;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
}
.nav-btn:hover, .nav-btn.active {
  background-color: #4a90e2;
  color: #e5e7eb;
}
@media (prefers-color-scheme: dark) {
  nav {
    background-color: #1f2937;
  }
  .nav-btn {
    color: #9ca3af;
    background-color: #4b5563;
  }
}
.control-btn {
  transition: all 0.3s ease;
  width: 100%;
  max-width: 200px;
  padding: 0.75rem;
  border-radius: 0.5rem;
  font-size: 1.125rem;
  font-weight: 500;
  text-align: center;
  border: none;
  cursor: pointer;
  color: #e5e7eb !important;
}
.control-btn.blue {
  background: #4a90e2 !important;
}
.control-btn.blue:hover {
  background: #1e88e5 !important;
}
.control-btn.blue.start:active,
.control-btn.blue.enable:active {
  background: #28a745 !important;
}
.control-btn.blue.stop:active {
  background: #dc3545 !important;
}
.control-btn.blue-btn {
  background: #6b7280 !important;
}
.control-btn.blue-btn:hover {
  background: #4b5563 !important;
}
.control-btn.blue-btn:active {
  background: #374151 !important;
}
.control-btn.dark-blue {
  background: #2563eb !important;
}
.control-btn.dark-blue:hover {
  background: #1e40af !important;
}
.control-btn.dark-blue.connect:active,
.control-btn.dark-blue.sensors:active {
  background: #28a745 !important;
}
.control-btn.dark-blue-btn {
  background: #1e40af !important;
}
.control-btn.dark-blue-btn:hover {
  background: #1e3a8a !important;
}
.control-btn.dark-blue-btn:active {
  background: #374151 !important;
}
.control-btn.purple {
  background: #6d28d9 !important;
}
.control-btn.purple:hover {
  background: #8b5cf6 !important;
}
.control-btn.purple:active {
  background: #28a745 !important;
}
.control-btn.red {
  background: #dc3545 !important;
}
.control-btn.red:hover {
  background: #b91c1c !important;
}
.control-btn.red:active {
  background: #6c757d !important;
}
.control-btn.green {
  background-color: #16a34a !important;
}
.control-btn.green:hover {
  background-color: #22c55e !important;
}
.control-btn.green:active {
  background-color: #15803d !important;
}
.control-btn:disabled {
  background: #d1d5db !important;
  cursor: not-allowed;
  opacity: 0.6;
}
h1 {
  font-family: 'Inter', 'Arial', sans-serif;
  font-weight: 400;
  letter-spacing: -0.5px;
  margin: 0.5rem 0;
  font-size: 1.4rem;
  color: #2596be;
  text-align: center;
}
h2 {
  font-family: 'Inter', 'Arial', sans-serif;
  font-size: 1.25rem;
  font-weight: 600;
  text-align: center;
  margin-bottom: 1rem;
  color: #111827;
}
@media (prefers-color-scheme: dark) {
  h2 {
    color: #9ca3af;
  }
}
main {
  max-width: 896px;
  margin: 0 auto;
  padding: 5rem 1rem 2rem;
}
section {
  margin-bottom: 2rem;
}
.sensor-container {
  border-radius: 0.5rem;
  overflow: hidden;
  gap: 2px;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  max-width: 500px;
  margin: 0 auto;
}
.controls-container {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  align-items: center;
}
footer {
  text-align: center;
  padding: 1rem 0;
  margin-top: 2rem;
  border-top: 1px solid #d1d5db;
  font-size: 0.875rem;
  color: #111827;
  max-width: 500px;
  margin-left: auto;
  margin-right: auto;
}
footer span {
  font-weight: 600;
}
@media (prefers-color-scheme: dark) {
  footer {
    border-top: 1px solid #4b5563;
    color: #9ca3af;
  }
}
@media (max-width: 640px) {
  .nav-btn {
    font-size: 0.85rem;
    padding: 0.5rem;
  }
  .nav-container {
    padding: 1.1rem 0.5rem;
  }
  .sensor-row p {
    font-size: 1rem;
  }
  .control-btn {
    font-size: 1rem;
    padding: 0.75rem;
    max-width: 175px;
  }
  h1 {
    font-size: 1.2rem;
  }
  h2 {
    font-size: 1.125rem;
  }
  footer {
    font-size: 0.75rem;
  }
  main {
    padding: 4.5rem 0.5rem 1.5rem;
  }
}
.status-circle {
  display: inline-block;
  font-size: 1rem;
  line-height: 1.5;
  margin-right: 0.5rem;
  vertical-align: text-bottom;
}
/* Config Page Table Styles */
.config-table {
  display: flex;
  flex-direction: column;
  border-radius: 0.5rem;
  overflow: hidden;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  gap: 2px;
  max-width: 500px;
  margin: 0 auto;
}
.table-row {
  display: flex;
  align-items: center;
  padding: 1rem;
  gap: 2px;
}
.table-row > * {
  margin: 0;
  font-size: 1.125rem;
  color: #111827;
  text-align: left;
}
.table-row > *:first-child {
  flex: 2;
}
.table-row > *:not(:first-child) {
  flex: 1;
}
.table-row p:first-child {
  font-weight: 500;
}
.table-row button {
  max-width: 100px;
  padding: 0.5rem;
  font-size: 1rem;
  font-weight: 500;
  text-align: center;
  margin-left: auto;
  box-sizing: border-box;
}
.hidden-password {
  color: #6b7280;
}
@media (prefers-color-scheme: dark) {
  .table-row > * {
    color: #9ca3af;
  }
  .hidden-password {
    color: #9ca3af;
  }
}
@media (max-width: 640px) {
  .table-row > * {
    font-size: 1rem;
  }
  .table-row button {
    max-width: 80px;
    padding: 0.4rem;
    font-size: 0.9rem;
    font-weight: 500;
    text-align: center;
    margin-left: auto;
    box-sizing: border-box;
  }
}
.modal {
  display: none;
  position: fixed;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  z-index: 1000;
  width: 333px;
  background-color: #ffffff;
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.2);
  border-radius: 12px;
  padding: 20px;
}
.modal-header {
  font-size: 20px;
  margin-bottom: 20px;
  color: #111827;
}
#genericInput {
  width: 90%;
  padding: 5px;
  border: 1px solid #d1d5db;
  border-radius: 4px;
  font-size: 16px;
  background-color: #f9fafb;
  color: #111827;
  margin-bottom: 10px;
  margin-top: 20px;
  margin-left: 10px;
  margin-right: 10px;
}
#genericInput:focus {
  outline: none;
  border-color: #22c55e;
}
.modal-footer {
  text-align: right;
}
.modal-footer button {
  padding: 10px 20px;
  margin: 6px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}
.btn-primary {
  background-color: #22c55e !important;
  color: #ffffff !important;
}
.btn-primary:hover {
  background-color: #16a34a !important;
}
.btn-primary:active {
  background-color: #4ade80 !important;
}
.btn-secondary {
  background-color: #6b7280 !important;
  color: #ffffff !important;
}
.btn-secondary:hover {
  background-color: #4b5563 !important;
}
.btn-secondary:active {
  background-color: #374151 !important;
}
.error {
  color: #ef4444;
  font-size: 14px;
  margin-top: 10px;
}
.success {
  color: #22c55e;
  font-size: 14px;
  margin-top: 12px;
}
.error-message {
  color: #ef4444;
}
.success-message {
  color: #22c55e;
}
/* Modal overlay */
.modal-overlay {
  display: none;
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-color: rgba(0, 0, 0, 0.5);
  z-index: 999;
}
@media (prefers-color-scheme: dark) {
  .modal {
    background-color: #1a202c;
  }
  .modal-header {
    color: #9ca3af;
  }
  #genericInput {
    background-color: #2d3748;
    border-color: #4a5568;
    color: #e2e8f0;
  }
}
/* Mobile adjustments for modal */
@media (max-width: 640px) {
  .modal {
    width: 90%;
    top: 35%;
    transform: translate(-50%, -35%);
  }
}
/* Logs page filter styles */
#filter-controls {
  display: flex;
  gap: 1rem;
  margin-bottom: 1rem;
  max-width: 896px;
  padding: 0 1rem;
}
#filter-dropdown {
  flex: 1;
  max-width: none;
  padding: 0.5rem;
  font-size: 1rem;
  background-color: #f9fafb;
  color: #111827;
  border: 1px solid #d1d5db;
  border-radius: 4px;
  box-sizing: border-box;
}
#filter-dropdown:focus {
  outline: none;
  border-color: #22c55e;
}
#filter-input {
  flex: 1;
  padding: 0.5rem;
  font-size: 1rem;
  background-color: #f9fafb;
  color: #111827;
  border: 1px solid #d1d5db;
  border-radius: 4px;
  box-sizing: border-box;
}
#filter-input:focus {
  outline: none;
  border-color: #22c55e;
}
#filter-input::placeholder {
  color: #6b7280;
}
#log-content div {
  line-height: 1.5;
}
#log-container {
  flex: 1;
  max-height: 60vh;
  overflow-x: auto;
  overflow-y: auto;
  background-color: #2d3748;
  padding: 1rem;
  border-radius: 0.5rem;
  font-family: monospace;
  font-size: 0.875rem;
  white-space: nowrap;
}
@media (prefers-color-scheme: light) {
  #log-container {
    background-color: #f9fafb;
  }
}
@media (prefers-color-scheme: dark) {
  #log-container {
    background-color: #2d3748;
  }
}
@media (prefers-color-scheme: dark) {
  #filter-dropdown {
    background-color: #2d3748;
    border-color: #4a5568;
    color: #e2e8f0;
  }
  #filter-input {
    background-color: #2d3748;
    border-color: #4a5568;
    color: #e2e8f0;
  }
  #filter-input::placeholder {
    color: #9ca3af;
  }
}
@media (max-width: 640px) {
  #filter-controls {
    flex-direction: column;
    gap: 0.5rem;
    padding: 0 0.5rem;
  }
  #filter-dropdown {
    font-size: 0.875rem;
    padding: 0.5rem;
    width: 100%;
    max-width: none;
    box-sizing: border-box;
  }
  #filter-input {
    font-size: 0.875rem;
    padding: 0.5rem;
    width: 100%;
    box-sizing: border-box;
  }
  @media (prefers-color-scheme: light) {
    #log-container {
      background-color: #f9fafb;
    }
  }
  @media (prefers-color-scheme: dark) {
    #log-container {
      background-color: #2d3748;
    }
  }
)=====";


// Index.html content in PROGMEM
const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>💧ESP32 Program Home💧</title>
  <link rel="stylesheet" href="/css?cb=33">
</head>
<body>
  <nav>
    <div class="nav-container">
      <button class="nav-btn active" onclick="window.location.href='/'">🏚️ HOME</button>
      <button class="nav-btn" onclick="window.location.href='/status'">📈 STATUS</button>
      <button class="nav-btn" onclick="window.location.href='/config'">⚙️ CONFIG</button>
      <button class="nav-btn" onclick="window.location.href='/logs'">📝 LOGS</button>
    </div>
  </nav>
  <main>
    <h1>💧 ESP32 Water Pump Control Interface 💧</h1>
    <section>
      <h2>Quick System Overview</h2>
      <div class="sensor-container">
        <div class="sensor-row bg-odd">
          <p>Current Program Status</p>
          <p><span class="status-circle" id="currentStateEmoji"></span></p><p><span id="currentState"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Tank Water Fill</p>
          <p><span id="tankFillPercentage"></span>%</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Water Distance to Tank Top</p>
          <p><span id="currentWaterLevel"></span> mm</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Last Cycle Pump Drew</p>
          <p><span id="postPumpAmps"></span> A</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Water Tank Temp</p>
          <p><span id="waterTankTemp"></span> °C</p>
        </div>
        <div class="sensor-row bg-even">
          <p>State Started</p>
          <p><span id="stateStartTime"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Local Date & Time</p>
          <p><span id="now"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>System Uptime</p>
          <p><span id="uptime"></span></p>
        </div>
      </div>
    </section>
    <section>
      <h2>System Controls</h2>
      <div class="controls-container">
        <button class="control-btn blue start" onclick="executeAction('pump-start', 'Start the pump?')">Pump Start</button>
        <button class="control-btn blue stop" onclick="executeAction('pump-stop', 'Stop the pump?')">Pump Stop</button>
        <button class="control-btn blue enable" onclick="executeAction('pump-enable', 'Enable the pump?')">Pump Enable</button>
        <button class="control-btn blue-btn" onclick="executeAction('pump-disable', 'Disable the pump?')">Pump Disable</button>
        <button class="control-btn blue-btn" onclick="executeAction('wifi-disable', 'Disable WiFi? Reboot to re-enable.')">WiFi Disable</button>
        <button class="control-btn dark-blue connect" onclick="executeAction('wifi-connect', 'Connect to WiFi AP?')">WiFi STA Connect</button>
        <button class="control-btn dark-blue connect" onclick="executeAction('sync-time', 'Sync time with NTP servers?')">NTP Sync Time</button>
        <button class="control-btn dark-blue sensors" onclick="executeAction('update-sensors', 'Update all sensors?')">Update Sensors</button>
        <button class="control-btn purple" onclick="executeAction('filter-cleaned', 'Confirm filter cleaned?')">Filter Cleaned</button>
        <button class="control-btn red" onclick="executeAction('reboot', 'Reboot the system?')">System Reboot</button>
      </div>
    </section>
    <footer>
      &#50;&#48;&#50;&#53;&#32;&#40;&#67;&#111;&#112;&#121;&#108;&#101;&#102;&#116;&#41; <span>&#65;&#110;&#116;&#111;&#110;&#86;&#80;&#32;&#40;&#32;&#97;&#32;&#41;&#32;&#103;&#109;&#97;&#105;&#108;&#46;&#99;&#111;&#109;</span>
    </footer>
  </main>
  <script>
    function formatElapsedTime(seconds) {
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = seconds % 60;
      return `${h.toString().padStart(2, '0')}h ${m.toString().padStart(2, '0')}m ${s.toString().padStart(2, '0')}s`;
    }
    function formatUptime(seconds) {
      const d = Math.floor(seconds / 86400);
      const h = Math.floor((seconds % 86400) / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = seconds % 60;
      return `Days ${d}, Hrs ${h}, Min ${m}, Sec ${s}`;
    }
    function fetchData() {
      fetch('/get-data')
        .then(response => {
          if (!response.ok) throw new Error('Fetch failed');
          return response.json();
        })
        .then(data => {
          document.getElementById('currentStateEmoji').textContent = data.currentStateEmoji;
          document.getElementById('currentState').textContent = data.currentState;
          document.getElementById('tankFillPercentage').textContent = data.tankFillPercentage;
          document.getElementById('currentWaterLevel').textContent = data.currentWaterLevel;
          document.getElementById('postPumpAmps').textContent = data.postPumpAmps.toFixed(3);
          document.getElementById('waterTankTemp').textContent = data.waterTankTemp.toFixed(2);
          document.getElementById('stateStartTime').textContent = formatElapsedTime(data.stateStartTime);
          document.getElementById('now').textContent = data.now;
          document.getElementById('uptime').textContent = formatUptime(data.uptime);
        })
        .catch(error => {
          console.error('Fetch error:', error);
          setTimeout(fetchData, 1000);
        });
    }
    function executeAction(endpoint, confirmMessage, button) {
      if (button) button.disabled = true;
      if (confirm(confirmMessage)) {
        fetch(`/${endpoint}`, { method: 'POST' })
          .then(response => {
            if (!response.ok) throw new Error('Action failed');
            return response.json();
          })
          .then(data => {
            if (data.status === 'success') {
              alert(data.message);
              fetchData();
            } else {
              alert('Error: ' + data.message);
            }
          })
          .catch(error => {
            console.error('Action error:', error);
            alert('Failed to execute action');
          })
          .finally(() => {
            if (button) button.disabled = false;
          });
      } else {
        if (button) button.disabled = false;
      }
    }
    window.addEventListener('load', () => {
      fetchData();
      setInterval(fetchData, 1000);
    });
  </script>
</body>
</html>
)=====";


// Status.html content in PROGMEM
const char status_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>💧ESP32 Program Status💧</title>
  <link rel="stylesheet" href="/css?cb=33">
</head>
<body>
  <nav>
    <div class="nav-container">
      <button class="nav-btn" onclick="window.location.href='/'">🏚️ HOME</button>
      <button class="nav-btn active" onclick="window.location.href='/status'">📈 STATUS</button>
      <button class="nav-btn" onclick="window.location.href='/config'">⚙️ CONFIG</button>
      <button class="nav-btn" onclick="window.location.href='/logs'">📝 LOGS</button>
    </div>
  </nav>
  <main>
    <h1>💧 ESP32 Program Status 💧</h1>
    <section>
      <h2>Live Values</h2>
      <div class="sensor-container">
        <div class="sensor-row bg-odd">
          <p>Tank Water Fill</p>
          <p><span id="tankFillPercentage"></span> %</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Water Distance to Tank Top</p>
          <p><span id="currentWaterLevel"></span> mm</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Current Program Status</p>
          <p><span class="status-circle" id="currentStateEmoji"></span></p><p><span id="currentState"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>State Started</p>
          <p><span id="stateStartTime"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Local Date & Time</p>
          <p><span id="now"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Last Pump Start</p>
          <p><span id="lastPumpStart"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Last Pump Stop</p>
          <p><span id="lastPumpStop"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Total Pump Cycles</p>
          <p><span id="totalPumpCycles"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Last Sensor Update</p>
          <p><span id="lastSensorUpdate"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Last NTP Sync</p>
          <p><span id="ntpLastSyncTime"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Hardware Timer Running</p>
          <p><span id="isHwTimer0Running"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Pump Halted</p>
          <p><span id="pumpIsHalted"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>System Start Time</p>
          <p><span id="systemStartTime"></span></p>
        </div>
      </div>
    </section>
    <section>
      <h2>Error Counters</h2>
      <div class="sensor-container">
        <div class="sensor-row bg-odd">
          <p>Critical Water Level</p>
          <p><span id="criticalWaterLevel"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Over Current Protection</p>
          <p><span id="overCurrentProtection"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Stuck Relay Error</p>
          <p><span id="errorStuckRelay"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Pump Start Error</p>
          <p><span id="errorPumpStart"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Red Error Count</p>
          <p><span id="errorRedCount"></span> times</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Purple Error Count</p>
          <p><span id="errorPurpleCount"></span> times</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Yellow Error Count</p>
          <p><span id="errorYellowCount"></span> times</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Critical Level Error Count</p>
          <p><span id="errorCriticalLvlCount"></span> times</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Last Error Recovery</p>
          <p><span id="errorLastRecovery"></span></p>
        </div>
      </div>
    </section>
    <section>
      <h2>Running Config</h2>
      <div class="sensor-container">
        <div class="sensor-row bg-odd">
          <p>Well Recovery Interval</p>
          <p><span id="wellRecoveryInterval"></span> sec</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Pump Runtime Interval</p>
          <p><span id="pumpRuntimeInterval"></span> sec</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Desired Water Level</p>
          <p><span id="desiredWaterLevel"></span> mm</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Hysteresis Threshold</p>
          <p><span id="hysteresisThreshold"></span> mm</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Sensor Update Interval</p>
          <p><span id="sensorUpdateInterval"></span> sec</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Tank Empty Height</p>
          <p><span id="tankEmptyHeight"></span> mm</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Tank Full Height</p>
          <p><span id="tankFullHeight"></span> mm</p>
        </div>
        <div class="sensor-row bg-even">
          <p>ADS Calibration Factor</p>
          <p><span id="adsCalibrationFactor"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Pump Max Amps</p>
          <p><span id="pumpMaxAmps"></span> Amps</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Pump On Amps</p>
          <p><span id="pumpOnAmps"></span> Amps</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Pump Off Amps</p>
          <p><span id="pumpOffAmps"></span> Amps</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Local Time Zone</p>
          <p><span id="localTimeZone"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Play Music</p>
          <p><span id="playMusic"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>NTP Sync Interval</p>
          <p><span id="ntpSyncInterval"></span> sec</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>NTP Sync Disabled</p>
          <p><span id="ntpSyncDisabled"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>AP SSID</p>
          <p><span id="apSSID"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>AP Password</p>
          <p><span id="apPass">[Hidden]</span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>STA SSID</p>
          <p><span id="staSSID"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>STA Password</p>
          <p><span id="staPass">[Hidden]</span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Web Server User</p>
          <p><span id="webServerUser"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Web Server Password</p>
          <p><span id="webServerPass">[Hidden]</span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>AP WiFi Mode Only</p>
          <p><span id="apWifiModeOnly"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Use Static IP</p>
          <p><span id="useStaStaticIP"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Static IP</p>
          <p><span id="staStaticIP"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Static Subnet Mask</p>
          <p><span id="staStaticSM"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Static Gateway</p>
          <p><span id="staStaticGW"></span></p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Static DNS 1</p>
          <p><span id="staStaticDN1"></span></p>
        </div>
        <div class="sensor-row bg-even">
          <p>Static DNS 2</p>
          <p><span id="staStaticDN2"></span></p>
        </div>
      </div>
    </section>
    <section>
      <h2>Water Temperatures</h2>
      <div class="sensor-container">
        <div class="sensor-row bg-odd">
          <p>Water Tank Temperature</p>
          <p><span id="waterTankTemp"></span> °C</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Pre-Pump Temperature</p>
          <p><span id="prePumpTemp"></span> °C</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Post-Pump Temperature</p>
          <p><span id="postPumpTemp"></span> °C</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Tank Delta Temperature</p>
          <p><span id="tankDeltaTemp"></span> °C</p>
        </div>
      </div>
    </section>
    <section>
      <h2>Electrical Current</h2>
      <div class="sensor-container">
        <div class="sensor-row bg-odd">
          <p>Current Pump Amps</p>
          <p><span id="currentPumpAmps"></span> Amps</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Pre-Pump Amps</p>
          <p><span id="prePumpAmps"></span> Amps</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>Post-Pump Amps</p>
          <p><span id="postPumpAmps"></span> Amps</p>
        </div>
        <div class="sensor-row bg-even">
          <p>Delta Pump Amps</p>
          <p><span id="deltaPumpAmps"></span> Amps</p>
        </div>
        <div class="sensor-row bg-odd">
          <p>ADC Sample Count</p>
          <p><span id="adcSampleCount"></span> samples</p>
        </div>
      </div>
    </section>
    <section>
      <div class="controls-container">
        <button class="control-btn green" onclick="togglePasswords()">Show Passwords</button>
      </div>
    </section>
    <footer>
      &#50;&#48;&#50;&#53;&#32;&#40;&#67;&#111;&#112;&#121;&#108;&#101;&#102;&#116;&#41; <span>&#65;&#110;&#116;&#111;&#110;&#86;&#80;&#32;&#40;&#32;&#97;&#32;&#41;&#32;&#103;&#109;&#97;&#105;&#108;&#46;&#99;&#111;&#109;</span>
    </footer>
  </main>
  <script>
    let passwordsVisible = false;
    function formatElapsedTime(seconds) {
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = seconds % 60;
      return `${h.toString().padStart(2, '0')}h ${m.toString().padStart(2, '0')}m ${s.toString().padStart(2, '0')}s`;
    }
    function togglePasswords() {
      passwordsVisible = !passwordsVisible;
      const button = document.querySelector('button[onclick="togglePasswords()"]');
      button.textContent = passwordsVisible ? 'Hide Passwords' : 'Show Passwords';
      fetchData();
    }
    function fetchData() {
      fetch('/get-status')
        .then(response => {
          if (!response.ok) throw new Error('Fetch failed');
          return response.json();
        })
        .then(data => {
          document.getElementById('tankFillPercentage').textContent = data.tankFillPercentage;
          document.getElementById('currentWaterLevel').textContent = data.currentWaterLevel;
          document.getElementById('currentStateEmoji').textContent = data.currentStateEmoji;
          document.getElementById('currentState').textContent = data.currentState;
          document.getElementById('stateStartTime').textContent = formatElapsedTime(data.stateStartTime);
          document.getElementById('now').textContent = data.now;
          document.getElementById('lastPumpStart').textContent = data.lastPumpStart;
          document.getElementById('lastPumpStop').textContent = data.lastPumpStop;
          document.getElementById('totalPumpCycles').textContent = data.totalPumpCycles;
          document.getElementById('lastSensorUpdate').textContent = data.lastSensorUpdate;
          document.getElementById('ntpLastSyncTime').textContent = data.ntpLastSyncTime;
          document.getElementById('isHwTimer0Running').textContent = data.isHwTimer0Running;
          document.getElementById('pumpIsHalted').textContent = data.pumpIsHalted;
          document.getElementById('systemStartTime').textContent = data.systemStartTime;
          document.getElementById('criticalWaterLevel').textContent = data.criticalWaterLevel;
          document.getElementById('overCurrentProtection').textContent = data.overCurrentProtection;
          document.getElementById('errorStuckRelay').textContent = data.errorStuckRelay;
          document.getElementById('errorPumpStart').textContent = data.errorPumpStart;
          document.getElementById('errorRedCount').textContent = data.errorRedCount;
          document.getElementById('errorPurpleCount').textContent = data.errorPurpleCount;
          document.getElementById('errorYellowCount').textContent = data.errorYellowCount;
          document.getElementById('errorCriticalLvlCount').textContent = data.errorCriticalLvlCount;
          document.getElementById('errorLastRecovery').textContent = data.errorLastRecovery;
          document.getElementById('wellRecoveryInterval').textContent = data.wellRecoveryInterval;
          document.getElementById('pumpRuntimeInterval').textContent = data.pumpRuntimeInterval;
          document.getElementById('desiredWaterLevel').textContent = data.desiredWaterLevel;
          document.getElementById('hysteresisThreshold').textContent = data.hysteresisThreshold;
          document.getElementById('sensorUpdateInterval').textContent = data.sensorUpdateInterval;
          document.getElementById('tankEmptyHeight').textContent = data.tankEmptyHeight;
          document.getElementById('tankFullHeight').textContent = data.tankFullHeight;
          document.getElementById('adsCalibrationFactor').textContent = data.adsCalibrationFactor.toFixed(5);
          document.getElementById('pumpMaxAmps').textContent = data.pumpMaxAmps.toFixed(3);
          document.getElementById('pumpOnAmps').textContent = data.pumpOnAmps.toFixed(3);
          document.getElementById('pumpOffAmps').textContent = data.pumpOffAmps.toFixed(3);
          document.getElementById('localTimeZone').textContent = data.localTimeZone;
          document.getElementById('playMusic').textContent = data.playMusic;
          document.getElementById('ntpSyncInterval').textContent = data.ntpSyncInterval;
          document.getElementById('ntpSyncDisabled').textContent = data.ntpSyncDisabled;
          document.getElementById('apSSID').textContent = data.apSSID;
          document.getElementById('apPass').textContent = passwordsVisible ? data.apPass : '[Hidden]';
          document.getElementById('staSSID').textContent = data.staSSID;
          document.getElementById('staPass').textContent = passwordsVisible ? data.staPass : '[Hidden]';
          document.getElementById('webServerUser').textContent = data.webServerUser;
          document.getElementById('webServerPass').textContent = passwordsVisible ? data.webServerPass : '[Hidden]';
          document.getElementById('apWifiModeOnly').textContent = data.apWifiModeOnly;
          document.getElementById('useStaStaticIP').textContent = data.useStaStaticIP;
          document.getElementById('staStaticIP').textContent = data.staStaticIP;
          document.getElementById('staStaticSM').textContent = data.staStaticSM;
          document.getElementById('staStaticGW').textContent = data.staStaticGW;
          document.getElementById('staStaticDN1').textContent = data.staStaticDN1;
          document.getElementById('staStaticDN2').textContent = data.staStaticDN2;
          document.getElementById('waterTankTemp').textContent = data.waterTankTemp.toFixed(2);
          document.getElementById('prePumpTemp').textContent = data.prePumpTemp.toFixed(2);
          document.getElementById('postPumpTemp').textContent = data.postPumpTemp.toFixed(2);
          document.getElementById('tankDeltaTemp').textContent = data.tankDeltaTemp.toFixed(2);
          document.getElementById('currentPumpAmps').textContent = data.currentPumpAmps.toFixed(3);
          document.getElementById('prePumpAmps').textContent = data.prePumpAmps.toFixed(3);
          document.getElementById('postPumpAmps').textContent = data.postPumpAmps.toFixed(3);
          document.getElementById('deltaPumpAmps').textContent = data.deltaPumpAmps.toFixed(3);
          document.getElementById('adcSampleCount').textContent = data.adcSampleCount;
        })
        .catch(error => {
          console.error('Fetch error:', error);
          setTimeout(fetchData, 1000);
        });
    }
    window.addEventListener('load', () => {
      fetchData();
      setInterval(fetchData, 1000);
    });
  </script>
</body>
</html>
)=====";


// Config page HTML in PROGMEM
const char config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>💧ESP32 Program Config💧</title>
  <link rel="stylesheet" href="/css?cb=33">
</head>
<body>
  <!-- Navigation Menu -->
  <nav>
    <div class="nav-container">
      <button class="nav-btn" onclick="window.location.href='/'">🏚️ HOME</button>
      <button class="nav-btn" onclick="window.location.href='/status'">📈 STATUS</button>
      <button class="nav-btn active" onclick="window.location.href='/config'">⚙️ CONFIG</button>
      <button class="nav-btn" onclick="window.location.href='/logs'">📝 LOGS</button>
    </div>
  </nav>

  <!-- Main Content -->
  <main>
    <h1>💧 ESP32 Program Configuration 💧</h1>
    <section>
      <h2>Program Startup Configuration</h2>
      <div class="config-table">
        <div class="table-row bg-odd">
          <p>Well Recovery Interval</p>
          <p data-key="p_wellRecIntvl">[Loading...]</p>
          <button class="control-btn green" data-type="uint" data-key="p_wellRecIntvl" data-name="Well Recovery Interval" role="button" aria-label="Edit Well Recovery Interval">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Pump Runtime Interval</p>
          <p data-key="p_pumpRunIntvl">[Loading...]</p>
          <button class="control-btn green" data-type="uint" data-key="p_pumpRunIntvl" data-name="Pump Runtime Interval" role="button" aria-label="Edit Pump Runtime Interval">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>Desired Water Level</p>
          <p data-key="p_desiredWtrLvl">[Loading...]</p>
          <button class="control-btn green" data-type="ushort" data-key="p_desiredWtrLvl" data-name="Desired Water Level" role="button" aria-label="Edit Desired Water Level">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Hysteresis Threshold</p>
          <p data-key="p_hystThreshold">[Loading...]</p>
          <button class="control-btn green" data-type="ushort" data-key="p_hystThreshold" data-name="Hysteresis Threshold" role="button" aria-label="Edit Hysteresis Threshold">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>Sensor Update Interval</p>
          <p data-key="p_sensrUpdIntvl">[Loading...]</p>
          <button class="control-btn green" data-type="uint" data-key="p_sensrUpdIntvl" data-name="Sensor Update Interval" role="button" aria-label="Edit Sensor Update Interval">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Tank Empty Height</p>
          <p data-key="p_tankEmptyHgt">[Loading...]</p>
          <button class="control-btn green" data-type="ushort" data-key="p_tankEmptyHgt" data-name="Tank Empty Height" role="button" aria-label="Edit Tank Empty Height">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>Tank Full Height</p>
          <p data-key="p_tankFullHgt">[Loading...]</p>
          <button class="control-btn green" data-type="ushort" data-key="p_tankFullHgt" data-name="Tank Full Height" role="button" aria-label="Edit Tank Full Height">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>ADS Calibration Factor</p>
          <p data-key="p_adsCalbFactor">[Loading...]</p>
          <button class="control-btn green" data-type="float" data-key="p_adsCalbFactor" data-name="ADS Calibration Factor" role="button" aria-label="Edit ADS Calibration Factor">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>Pump Max Amps</p>
          <p data-key="p_pumpMaxAmps">[Loading...]</p>
          <button class="control-btn green" data-type="float" data-key="p_pumpMaxAmps" data-name="Pump Max Amps" role="button" aria-label="Edit Pump Max Amps">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Pump On Amps</p>
          <p data-key="p_pumpOnAmps">[Loading...]</p>
          <button class="control-btn green" data-type="float" data-key="p_pumpOnAmps" data-name="Pump On Amps" role="button" aria-label="Edit Pump On Amps">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>Pump Off Amps</p>
          <p data-key="p_pumpOffAmps">[Loading...]</p>
          <button class="control-btn green" data-type="float" data-key="p_pumpOffAmps" data-name="Pump Off Amps" role="button" aria-label="Edit Pump Off Amps">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>AP SSID</p>
          <p data-key="p_apSSID">[Loading...]</p>
          <button class="control-btn green" data-type="string" data-key="p_apSSID" data-name="AP SSID" role="button" aria-label="Edit AP SSID">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>AP Password</p>
          <p data-key="p_apPass" class="hidden-password">[Hidden]</p>
          <button class="control-btn green" data-type="string" data-key="p_apPass" data-name="AP Password" role="button" aria-label="Edit AP Password">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>STA SSID</p>
          <p data-key="p_staSSID">[Loading...]</p>
          <button class="control-btn green" data-type="string" data-key="p_staSSID" data-name="STA SSID" role="button" aria-label="Edit STA SSID">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>STA Password</p>
          <p data-key="p_staPass" class="hidden-password">[Hidden]</p>
          <button class="control-btn green" data-type="string" data-key="p_staPass" data-name="STA Password" role="button" aria-label="Edit STA Password">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Web Server User</p>
          <p data-key="p_webUser">[Loading...]</p>
          <button class="control-btn green" data-type="string" data-key="p_webUser" data-name="Web Server User" role="button" aria-label="Edit Web Server User">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>Web Server Password</p>
          <p data-key="p_webPass" class="hidden-password">[Hidden]</p>
          <button class="control-btn green" data-type="string" data-key="p_webPass" data-name="Web Server Password" role="button" aria-label="Edit Web Server Password">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>AP WiFi Mode Only</p>
          <p data-key="p_apWifiModeOly">[Loading...]</p>
          <button class="control-btn green" data-type="boolean" data-key="p_apWifiModeOly" data-name="AP WiFi Mode Only" role="button" aria-label="Edit AP WiFi Mode Only">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>NTP Sync Disabled</p>
          <p data-key="p_ntpSyncDsbld">[Loading...]</p>
          <button class="control-btn green" data-type="boolean" data-key="p_ntpSyncDsbld" data-name="NTP Sync Disabled" role="button" aria-label="Edit NTP Sync Disabled">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Play Music</p>
          <p data-key="p_playMusic">[Loading...]</p>
          <button class="control-btn green" data-type="boolean" data-key="p_playMusic" data-name="Play Music" role="button" aria-label="Edit Play Music">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>NTP Sync Interval</p>
          <p data-key="p_ntpSyncIntvl">[Loading...]</p>
          <button class="control-btn green" data-type="uint" data-key="p_ntpSyncIntvl" data-name="NTP Sync Interval" role="button" aria-label="Edit NTP Sync Interval">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Local Time Zone</p>
          <p data-key="p_localTimeZone">[Loading...]</p>
          <button class="control-btn green" data-type="string" data-key="p_localTimeZone" data-name="Local Time Zone" role="button" aria-label="Edit Local Time Zone">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>Use Static IP</p>
          <p data-key="p_useStaStatcIP">[Loading...]</p>
          <button class="control-btn green" data-type="boolean" data-key="p_useStaStatcIP" data-name="Use Static IP" role="button" aria-label="Edit Use Static IP">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Static IP</p>
          <p data-key="p_staStaticIP">[Loading...]</p>
          <button class="control-btn green" data-type="ipaddress" data-key="p_staStaticIP" data-name="Static IP" role="button" aria-label="Edit Static IP">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>Static Subnet Mask</p>
          <p data-key="p_staStaticSM">[Loading...]</p>
          <button class="control-btn green" data-type="ipaddress" data-key="p_staStaticSM" data-name="Static Subnet Mask" role="button" aria-label="Edit Static Subnet Mask">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Static Gateway</p>
          <p data-key="p_staStaticGW">[Loading...]</p>
          <button class="control-btn green" data-type="ipaddress" data-key="p_staStaticGW" data-name="Static Gateway" role="button" aria-label="Edit Static Gateway">Edit</button>
        </div>
        <div class="table-row bg-odd">
          <p>Static DNS 1</p>
          <p data-key="p_staStaticDN1">[Loading...]</p>
          <button class="control-btn green" data-type="ipaddress" data-key="p_staStaticDN1" data-name="Static DNS 1" role="button" aria-label="Edit Static DNS 1">Edit</button>
        </div>
        <div class="table-row bg-even">
          <p>Static DNS 2</p>
          <p data-key="p_staStaticDN2">[Loading...]</p>
          <button class="control-btn green" data-type="ipaddress" data-key="p_staStaticDN2" data-name="Static DNS 2" role="button" aria-label="Edit Static DNS 2">Edit</button>
        </div>
      </div>
    </section>

    <!-- Action Buttons -->
    <section>
      <div style="display: flex; flex-direction: column; gap: 1rem; align-items: center;">
    <button class="control-btn green show-passwords" onclick="togglePasswords()">Show Passwords</button>
    <button class="control-btn blue" onclick="refreshConfig()">Read Settings</button>
    <button class="control-btn red" onclick="openConfirmModal('apply')">Apply Settings</button>
    <button class="control-btn red" onclick="openConfirmModal('reset')">Factory Reset</button>
  </div>
    </section>

    <!-- Modal for Editing -->
    <div id="modalOverlay" class="modal-overlay"></div>
    <div id="genericModal" class="modal">
      <div class="modal-header"></div>
      <label for="genericInput" class="input-label"></label>
      <input id="genericInput" type="text">
      <div class="error"></div>
      <div class="success"></div>
      <div class="modal-footer">
        <button class="btn-secondary close-button">Close</button>
        <button class="btn-primary save-button">Save</button>
        <button class="control-btn red" style="display: none;">Apply Settings</button>
      </div>
    </div>

    <!-- Modal for Confirmation -->
    <div id="confirmModal" class="modal">
      <div class="modal-header"></div>
      <div class="error"></div>
      <div class="modal-footer">
        <button class="btn-secondary close-button">Cancel</button>
        <button class="btn-primary confirm-button">Confirm</button>
      </div>
    </div>

    <!-- Footer -->
    <footer>
      &#50;&#48;&#50;&#53;&#32;&#40;&#67;&#111;&#112;&#121;&#108;&#101;&#102;&#116;&#41; <span>&#65;&#110;&#116;&#111;&#110;&#86;&#80;&#32;&#40;&#32;&#97;&#32;&#41;&#32;&#103;&#109;&#97;&#105;&#108;&#46;&#99;&#111;&#109;</span>
    </footer>
  </main>

  <!-- JavaScript -->
  <script>
    let showPasswords = false;

    function setActive(button) {
      document.querySelectorAll('.nav-btn').forEach(btn => btn.classList.remove('active'));
      button.classList.add('active');
      alert(button.textContent + ' page selected');
    }

    const modalOverlay = document.getElementById('modalOverlay');
    const editModal = document.getElementById('genericModal');
    const confirmModal = document.getElementById('confirmModal');
    const input = editModal.querySelector('#genericInput');
    const errorDiv = editModal.querySelector('.error');
    const successDiv = editModal.querySelector('.success');
    const editHeader = editModal.querySelector('.modal-header');
    const editLabel = editModal.querySelector('.input-label');
    const applyButton = editModal.querySelector('.control-btn.red');
    const confirmHeader = confirmModal.querySelector('.modal-header');
    const confirmError = confirmModal.querySelector('.error');

    const handlers = {
      float: {
        header: 'Edit Float p_ Key',
        validate: (value) => {
          value = value.trim();
          return value !== '' && /^[+-]?(0|[1-9]\d*)(\.\d{1,7})?$/.test(value);
        },
        success: (value) => `✔️ Successfully saved ${input.dataset.key}: ${value}`,
        error: '⚠️ Invalid float value. Max 7 decimal places.',
        writeError: '❌ Failed to save float preference.'
      },
      uint: {
        header: 'Edit UInt p_ Key',
        validate: (value) => {
          const num = Number(value);
          return Number.isInteger(num) && num >= 0 && num <= 4294967295 && /^([1-9]\d*|0)$/.test(value);
        },
        success: (value) => `✔️ Successfully saved ${input.dataset.key}: ${value}`,
        error: '⚠️ Invalid uint value. Range: 0 to 4294967295.',
        writeError: '❌ Failed to save uint preference.'
      },
      ushort: {
        header: 'Edit UShort p_ Key',
        validate: (value) => {
          const num = Number(value);
          return Number.isInteger(num) && num >= 0 && num <= 65535 && /^([1-9]\d*|0)$/.test(value);
        },
        success: (value) => `✔️ Successfully saved ${input.dataset.key}: ${value}`,
        error: '⚠️ Invalid ushort value. Range: 0 to 65535.',
        writeError: '❌ Failed to save ushort preference.'
      },
      string: {
        header: 'Edit String p_ Key',
        validate: (value) => value.length <= 32 && !/[<>]/.test(value),
        success: (value) => `✔️ Successfully saved ${input.dataset.key}: ${value}`,
        error: '⚠️ Invalid String. Maximum 32 characters.',
        writeError: '❌ Failed to save string preference.'
      },
      ipaddress: {
        header: 'Edit IP Address p_ Key',
        validate: (value) => /^(\d{1,3}\.){3}\d{1,3}$/.test(value) && value.split('.').every(octet => octet >= 0 && octet <= 255),
        success: (value) => `✔️ Successfully saved ${input.dataset.key}: ${value}`,
        error: '⚠️ Invalid IP address format.',
        writeError: '❌ Failed to save IP address preference.'
      },
      boolean: {
        header: 'Edit Boolean p_ Key',
        validate: (value) => ['true', 'false', '0', '1'].includes(value.toLowerCase()),
        success: (value) => {
          const normalized = value.toLowerCase();
          return `✔️ Successfully saved ${input.dataset.key}: ${normalized === '0' || normalized === 'false' ? 'false' : 'true'}`;
        },
        error: '⚠️ Invalid boolean. Use "true", "false", "0", or "1".',
        writeError: '❌ Failed to save boolean preference.'
      }
    };

    function openModal(type, key, name) {
      if (!handlers[type]) {
        showMessage('error', 'Invalid data type.');
        return;
      }
      editModal.style.display = 'block';
      modalOverlay.style.display = 'block';
      clearMessages();
      input.value = '';
      input.dataset.type = type;
      input.dataset.key = key;
      const handler = handlers[type];
      editHeader.textContent = `Edit ${name}`;
      editLabel.textContent = `Enter a new ${key} value:`;
      applyButton.style.display = 'none';
      input.style.display = 'block';
      input.focus();
    }

    function openConfirmModal(type) {
      confirmModal.style.display = 'block';
      modalOverlay.style.display = 'block';
      confirmError.textContent = '';
      if (type === 'apply') {
        confirmHeader.textContent = 'Apply Configuration';
        confirmModal.querySelector('.confirm-button').dataset.action = 'apply';
        confirmError.textContent = '❗ Must Reboot to Apply Settings. Proceed with reboot?';
      } else if (type === 'reset') {
        confirmHeader.textContent = 'Factory Reset';
        confirmModal.querySelector('.confirm-button').dataset.action = 'reset';
        confirmError.textContent = 'Reset all settings to defaults and reboot?';
      }
    }

    function closeModal() {
      editModal.style.display = 'none';
      confirmModal.style.display = 'none';
      modalOverlay.style.display = 'none';
    }

    function clearMessages() {
      errorDiv.textContent = '';
      successDiv.textContent = '';
      applyButton.style.display = 'none';
    }

    function showMessage(type, message) {
      clearMessages();
      editModal.querySelector(`.${type}`).textContent = message;
    }

    function togglePasswords() {
      showPasswords = !showPasswords;
      const btn = document.querySelector('.show-passwords');
      btn.classList.toggle('active');
      btn.textContent = showPasswords ? 'Hide Passwords' : 'Show Passwords';
      refreshConfig();
    }

    async function refreshConfig() {
      try {
        const response = await fetch('/get-config');
        const json = await response.json();
        if (json.status === 'success') {
          json.data.forEach(item => {
            const elem = document.querySelector(`p[data-key="${item.key}"]`);
            if (elem) {
              if (['p_apPass', 'p_staPass', 'p_webPass'].includes(item.key) && !showPasswords) {
                elem.textContent = '[Hidden]';
                elem.classList.add('hidden-password');
              } else {
                elem.textContent = item.value;
                elem.classList.remove('hidden-password');
              }
            }
          });
        } else {
          showMessage('error', json.message || 'Failed to load configuration.');
        }
      } catch (error) {
        showMessage('error', 'Error fetching configuration.');
      }
    }

    async function applyConfig() {
      try {
        const response = await fetch('/reboot', {
          method: 'POST'
        });
        const json = await response.json();
        if (json.status === 'success') {
          showMessage('success', 'Rebooting...');
        } else {
          showMessage('error', json.message || 'Failed to reboot.');
        }
      } catch (error) {
        showMessage('error', 'Error initiating reboot.');
      }
    }

    async function factoryReset() {
      try {
        const response = await fetch('/erase-config', {
          method: 'POST'
        });
        const json = await response.json();
        if (json.status === 'success') {
          showMessage('success', 'Settings reset. Rebooting...');
          closeModal(); // Close modal on success
        } else {
          showMessage('error', json.message || 'Failed to reset settings.');
        }
      } catch (error) {
        showMessage('error', 'Error resetting settings.');
      }
      setTimeout(closeModal, 2000);
    }

    document.querySelector('.config-table').addEventListener('click', (e) => {
      const button = e.target.closest('.control-btn.green');
      if (button) {
        const type = button.dataset.type;
        const key = button.dataset.key;
        const name = button.dataset.name;
        if (type && handlers[type]) {
          openModal(type, key, name);
        } else {
          showMessage('error', 'Invalid data type.');
        }
      }
    });

    editModal.querySelector('.close-button').addEventListener('click', closeModal);
    confirmModal.querySelector('.close-button').addEventListener('click', closeModal);
    modalOverlay.addEventListener('click', closeModal);

    editModal.querySelector('.save-button').addEventListener('click', async () => {
      const type = input.dataset.type;
      const key = input.dataset.key;
      const value = input.value.trim();
      const handler = handlers[type];
      if (value && handler.validate(value)) {
        try {
          const response = await fetch('/set-config', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json'
            },
            body: JSON.stringify({ key, value, type })
          });
          const json = await response.json();
          if (json.status === 'success') {
            showMessage('success', handler.success(json.value));
            applyButton.style.display = 'inline-block';
            refreshConfig();
          } else {
            showMessage('error', json.message || handler.writeError);
          }
        } catch (error) {
          showMessage('error', handler.writeError);
        }
      } else {
        input.value = '';
        showMessage('error', handler.error);
      }
    });

    if (applyButton) {
      applyButton.addEventListener('click', () => {
      openConfirmModal('apply');
    });
   }

    confirmModal.querySelector('.confirm-button').addEventListener('click', async () => {
      const action = confirmModal.querySelector('.confirm-button').dataset.action;
      if (action === 'apply') {
        await applyConfig();
        closeModal();
      } else if (action === 'reset') {
        await factoryReset();
        closeModal();
      }
      closeModal();
    });

    input.addEventListener('keypress', (e) => {
      if (e.key === 'Enter') {
        editModal.querySelector('.save-button').click();
      }
    });

    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && (editModal.style.display === 'block' || confirmModal.style.display === 'block')) {
        closeModal();
      }
    });

    // Load config on page load
    document.addEventListener('DOMContentLoaded', refreshConfig);
  </script>
</body>
</html>
)rawliteral";


// Logs page HTML
const char logs_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>💧ESP32 Program Logs💧</title>
  <link rel="stylesheet" href="/css?cb=33">
</head>
<body>
  <nav>
    <div class="nav-container">
      <button class="nav-btn" onclick="window.location.href='/'">🏚️ HOME</button>
      <button class="nav-btn" onclick="window.location.href='/status'">📊 STATUS</button>
      <button class="nav-btn" onclick="window.location.href='/config'">⚙️ CONFIG</button>
      <button class="nav-btn active" onclick="window.location.href='/logs'">📋 LOGS</button>
    </div>
  </nav>

  <main>
    <h1>💧 ESP32 Program Logs 💧</h1>
    <div id="filter-controls" style="display: flex; gap: 1rem; margin-bottom: 1rem;">
      <select id="filter-dropdown" style="flex: 1; padding: 0.5rem; font-size: 1rem;">
        <option value="ALL">All Logs</option>
        <option value="INFO">INFO</option>
        <option value="NOTICE">NOTICE</option>
        <option value="WARN">WARN</option>
        <option value="ERROR">ERROR</option>
        <option value="FATAL">FATAL</option>
        <option value="WIFI">WIFI</option>
        <option value="TIME">TIME</option>
      </select>
      <input id="filter-input" type="text" placeholder="Search logs..." style="flex: 1; padding: 0.5rem; font-size: 1rem;">
    </div>

    <div id="log-container">
      <div id="log-content" style="display: flex; flex-direction: column; align-items: flex-start;"></div>
    </div>

    <div style="display: flex; flex-direction: column; gap: 1rem; align-items: center; margin-top: 1rem;">
      <button class="control-btn green" onclick="refreshLogs()">Refresh Logs</button>
      <button class="control-btn blue" onclick="copyLogs()">Copy All Logs</button>
      <button class="control-btn red" onclick="clearLogs()">Clear Logs</button>
    </div>
  </main>

  <footer>
      &#50;&#48;&#50;&#53;&#32;&#40;&#67;&#111;&#112;&#121;&#108;&#101;&#102;&#116;&#41; <span>&#65;&#110;&#116;&#111;&#110;&#86;&#80;&#32;&#40;&#32;&#97;&#32;&#41;&#32;&#103;&#109;&#97;&#105;&#108;&#46;&#99;&#111;&#109;</span>
    </footer>

<script>
    let allLogs = [];

    function fetchLogs() {
      console.log('Fetching logs...');
      fetch('/get-logs', { credentials: 'same-origin' })
        .then(response => {
          if (!response.ok) throw new Error(`HTTP ${response.status}`);
          return response.json();
        })
        .then(data => {
          if (data.status === 'success') {
            allLogs = data.logs.split('\n').filter(line => line.trim());
            applyFilters();
          } else {
            alert(`Failed to fetch logs: ${data.message}`);
          }
        })
        .catch(error => {
          alert(`Error fetching logs: ${error.message}`);
        });
    }

    function applyFilters() {
      const level = document.getElementById('filter-dropdown').value.toUpperCase();
      const search = document.getElementById('filter-input').value.toLowerCase();
      const logContent = document.getElementById('log-content');
      logContent.innerHTML = '';

      const filteredLogs = allLogs.filter(log => {
        if (level === 'ALL' && !search) return true;
        const logLower = log.toLowerCase();
        const levelMatch = level === 'ALL' || log.match(new RegExp(`^\\[${level}\\]`, 'i'));
        const searchMatch = !search || logLower.includes(search);
        return levelMatch && searchMatch;
      });

      filteredLogs.forEach(log => {
        const div = document.createElement('div');
        div.textContent = log;
        logContent.appendChild(div);
      });
    }

    function refreshLogs() {
      fetchLogs();
    }

    function copyLogs() {
      console.log('Copying logs...');
      const text = allLogs.join('\n');
      if (navigator.clipboard && window.isSecureContext) {
        navigator.clipboard.writeText(text)
          .then(() => alert('Logs copied to clipboard'))
          .catch(() => fallbackCopyText(text));
      } else {
        fallbackCopyText(text);
      }
    }

    function fallbackCopyText(text) {
      try {
        const textarea = document.createElement('textarea');
        textarea.value = text;
        textarea.style.position = 'fixed';
        textarea.style.opacity = '0';
        document.body.appendChild(textarea);
        textarea.select();
        document.execCommand('copy');
        document.body.removeChild(textarea);
        alert('Logs copied to clipboard');
      } catch (err) {
        alert('Failed to copy logs');
      }
    }

    function clearLogs() {
      console.log('Clearing logs...');
      if (!confirm('Are you sure you want to clear all logs?')) {
        return;
      }
      console.log('Starting fetch to /clear-logs');
      const controller = new AbortController();
      const timeoutId = setTimeout(() => {
        controller.abort();
        console.log('Fetch timed out');
      }, 5000);
      fetch('/clear-logs', {
        method: 'POST',
        credentials: 'same-origin',
        signal: controller.signal
      })
        .then(response => {
          clearTimeout(timeoutId);
          console.log('Fetch response received, status:', response.status);
          if (!response.ok) throw new Error(`HTTP ${response.status}`);
          return response.text();
        })
        .then(data => {
          console.log('Fetch data:', data);
          if (data === 'OK') {
            allLogs = [];
            applyFilters();
            alert('Logs cleared successfully');
          } else {
            alert(`Failed to clear logs: ${data}`);
          }
        })
        .catch(error => {
          console.log('Fetch error:', error);
          if (error.name === 'AbortError') {
            alert('Error: Request timed out');
          } else {
            alert(`Error: ${error.message}`);
          }
        });
    }

    document.getElementById('filter-dropdown').addEventListener('change', applyFilters);
    document.getElementById('filter-input').addEventListener('input', applyFilters);

    window.onload = fetchLogs;
  </script>
 </body>
</html>
)rawliteral";


// Authentication middleware
bool authenticate(AsyncWebServerRequest *request) {
  if (!request->authenticate(webServerUser.c_str(), webServerPass.c_str())) {
    logSerial.printf("[WARN] 🔐 HTTP Authentication failed on %s\n", rtc.getTime("%d %b %H:%M:%S").c_str());
    request->requestAuthentication();
    return false;
  }
  return true;
}

// Initialize web server
void initWebServer() {

  // Serve CSS
  server.on("/css", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) {
      return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/css", css_content);
    response->addHeader("Cache-Control", "max-age=86400");
    request->send(response);
  });

  // Serve Home Page at /
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // Endpoint for Start Pump Button 
  server.on("/pump-start", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument response;
    if (currentState == WELL_RECOVERY && !pumpIsHalted) {
      currentState = STARTING_PUMP;
      logStateTransition(currentState);
      response["status"] = "success";
      response["message"] = "Pump started";
    } else {
      response["status"] = "error";
      response["message"] = "Pump can only start from WELL_RECOVERY when not halted";
    }
    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

    // Endpoint for Stop Pump Button
  server.on("/pump-stop", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument response;
    if (currentState == PUMPING_WATER) {
      currentState = STOPPING_PUMP;
      logStateTransition(currentState);
      response["status"] = "success";
      response["message"] = "Pump stopped";
    } else {
      response["status"] = "error";
      response["message"] = "Pump can only stop from PUMPING_WATER state";
    }
    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Endpoint for Enable Pump Button
  server.on("/pump-enable", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument response;
    if (currentState == HALTED_PUMP) {
      pumpIsHalted = false;
      currentState = STOPPING_PUMP;
      logStateTransition(currentState);
      response["status"] = "success";
      response["message"] = "Pump enabled";
    } else {
      response["status"] = "error";
      response["message"] = "Pump can only be enabled from HALTED_PUMP state";
    }
    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Endpoint for Disable Pump Button
  server.on("/pump-disable", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument response;
    if (currentState == HALTED_PUMP) {
      response["status"] = "error";
      response["message"] = "Pump is already disabled";
    } else if (currentState == ERROR_RED || currentState == ERROR_PURPLE) {
      response["status"] = "error";
      response["message"] = "Cannot disable pump in Red or Purple error states";
    } else {
      digitalWrite(WATER_PUMP_RELAY_PIN, LOW);
      delay(100);
      float current = getPumpCurrent();
      logSerial.printf("[INFO] ⓘ Pump Disabled: Current=%.3f A, Threshold=%.3f A\n", current, pumpOffAmps);
      if (current > pumpOffAmps) {
        currentState = ERROR_RED;
        logSerial.println("[FATAL] 🔴 STUCK RELAY #1 DETECTED IN HALTED_PUMP STATE, CHECK RELAY 1.");
        errorStuckRelay = true;
        errorRedCount++;
        stateStartTime = now;
        stopStateTimer();
        logStateTransition(currentState);
        response["status"] = "error";
        response["message"] = "Stuck relay detected";
      } else {
        strip.setLedColorData(0, 0x000000);
        strip.show();
        stopStateTimer();
        pumpIsHalted = true;
        stateStartTime = now;
        currentState = HALTED_PUMP;
        logStateTransition(currentState);
        response["status"] = "success";
        response["message"] = "Pump disabled";
      }
    }
    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Endpoint to Connect in STA Mode
  server.on("/wifi-connect", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument response;
    currentWiFiState = WIFI_STA_RECONNECT;
    response["status"] = "success";
    response["message"] = "WiFi reconnect initiated";
    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Endpoint to Disable WiFi
  server.on("/wifi-disable", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument response;
    currentWiFiState = WIFI_DISABLED;
    response["status"] = "success";
    response["message"] = "WiFi disabled, reboot to re-enable";
    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Endpoint to Sync time with NTP
  server.on("/sync-time", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument response;
    ntpLastSyncTime = now - ntpSyncInterval - 1;
    response["status"] = "success";
    response["message"] = "NTP sync initiated";
    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Endpoint for Manual Sensor Update
  server.on("/update-sensors", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument response;
    lastSensorUpdate = now - sensorUpdateInterval - 1;
    response["status"] = "success";
    response["message"] = "Sensor update initiated";
    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Endpoint to Clear Purple Error State
  server.on("/filter-cleaned", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument response;
    if (currentState == ERROR_PURPLE) {
      overCurrentProtection = false;
      currentState = STOPPING_PUMP;
      logStateTransition(currentState);
      response["status"] = "success";
      response["message"] = "Filter cleaned, pump resuming";
    } else {
      response["status"] = "error";
      response["message"] = "Can only clear filter in ERROR_PURPLE state";
    }
    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });


  // Get Data for Home Page
  server.on("/get-data", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument doc;
    doc["currentStateEmoji"] = getStateEmoji(currentState);
    doc["currentState"] = getStateString(currentState);
    doc["tankFillPercentage"] = tankFillPercentage;
    doc["currentWaterLevel"] = currentWaterLevel;
    doc["postPumpAmps"] = postPumpAmps;
    doc["waterTankTemp"] = waterTankTemp;
    doc["stateStartTime"] = now - stateStartTime;
    doc["now"] = formatTimestamp(now);
    doc["uptime"] = now - systemStartTime;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Serve Status Page at /status
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", status_html);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // Get Data for Status Page
  server.on("/get-status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) return;
    JsonDocument doc;
    doc["tankFillPercentage"] = tankFillPercentage;
    doc["currentWaterLevel"] = currentWaterLevel;
    doc["currentStateEmoji"] = getStateEmoji(currentState);
    doc["currentState"] = getStateString(currentState);
    doc["stateStartTime"] = now - stateStartTime;
    doc["now"] = formatTimestamp(now);
    doc["lastPumpStart"] = formatTimestamp(lastPumpStart);
    doc["lastPumpStop"] = formatTimestamp(lastPumpStop);
    doc["totalPumpCycles"] = totalPumpCycles;
    doc["lastSensorUpdate"] = formatTimestamp(lastSensorUpdate);
    doc["ntpLastSyncTime"] = formatTimestamp(ntpLastSyncTime);
    doc["isHwTimer0Running"] = isHwTimer0Running ? "true" : "false";
    doc["pumpIsHalted"] = pumpIsHalted ? "true" : "false";
    doc["systemStartTime"] = formatTimestamp(systemStartTime);
    doc["criticalWaterLevel"] = criticalWaterLevel ? "true" : "false";
    doc["overCurrentProtection"] = overCurrentProtection ? "true" : "false";
    doc["errorStuckRelay"] = errorStuckRelay ? "true" : "false";
    doc["errorPumpStart"] = errorPumpStart > 0 ? "true" : "false";
    doc["errorRedCount"] = errorRedCount;
    doc["errorPurpleCount"] = errorPurpleCount;
    doc["errorYellowCount"] = errorYellowCount;
    doc["errorCriticalLvlCount"] = errorCriticalLvlCount;
    doc["errorLastRecovery"] = formatTimestamp(errorLastRecovery);
    doc["wellRecoveryInterval"] = wellRecoveryInterval;
    doc["pumpRuntimeInterval"] = pumpRuntimeInterval;
    doc["desiredWaterLevel"] = desiredWaterLevel;
    doc["hysteresisThreshold"] = hysteresisThreshold;
    doc["sensorUpdateInterval"] = sensorUpdateInterval;
    doc["tankEmptyHeight"] = tankEmptyHeight;
    doc["tankFullHeight"] = tankFullHeight;
    doc["adsCalibrationFactor"] = adsCalibrationFactor;
    doc["pumpMaxAmps"] = pumpMaxAmps;
    doc["pumpOnAmps"] = pumpOnAmps;
    doc["pumpOffAmps"] = pumpOffAmps;
    doc["localTimeZone"] = localTimeZone;
    doc["playMusic"] = playMusic ? "true" : "false";
    doc["ntpSyncInterval"] = ntpSyncInterval;
    doc["ntpSyncDisabled"] = ntpSyncDisabled ? "true" : "false";
    doc["apSSID"] = apSSID;
    doc["apPass"] = apPass;
    doc["staSSID"] = staSSID;
    doc["staPass"] = staPass;
    doc["webServerUser"] = webServerUser;
    doc["webServerPass"] = webServerPass;
    doc["apWifiModeOnly"] = apWifiModeOnly ? "true" : "false";
    doc["useStaStaticIP"] = useStaStaticIP ? "true" : "false";
    doc["staStaticIP"] = staStaticIP.toString();
    doc["staStaticSM"] = staStaticSM.toString();
    doc["staStaticGW"] = staStaticGW.toString();
    doc["staStaticDN1"] = staStaticDN1.toString();
    doc["staStaticDN2"] = staStaticDN2.toString();
    doc["waterTankTemp"] = waterTankTemp;
    doc["prePumpTemp"] = prePumpTemp;
    doc["postPumpTemp"] = postPumpTemp;
    doc["tankDeltaTemp"] = tankDeltaTemp;
    doc["currentPumpAmps"] = currentPumpAmps;
    doc["prePumpAmps"] = prePumpAmps;
    doc["postPumpAmps"] = postPumpAmps;
    doc["deltaPumpAmps"] = deltaPumpAmps;
    doc["adcSampleCount"] = adcSampleCount;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Serve Config Page
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) {
      return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", config_html);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // Get Config Data
  server.on("/get-config", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) {
      return request->requestAuthentication();
    }
    JsonDocument response;
    JsonArray data = response["data"].to<JsonArray>();
    response["status"] = "success";
    Preferences prefs;
    prefs.begin("Startup_Config", true);
    const char* keys[] = {
      "p_wellRecIntvl", "p_pumpRunIntvl", "p_desiredWtrLvl", "p_hystThreshold",
      "p_sensrUpdIntvl", "p_tankEmptyHgt", "p_tankFullHgt", "p_adsCalbFactor",
      "p_pumpMaxAmps", "p_pumpOnAmps", "p_pumpOffAmps", "p_apSSID",
      "p_apPass", "p_staSSID", "p_staPass", "p_webUser",
      "p_webPass", "p_apWifiModeOly", "p_ntpSyncDsbld", "p_playMusic",
      "p_ntpSyncIntvl", "p_localTimeZone", "p_useStaStatcIP", "p_staStaticIP",
      "p_staStaticSM", "p_staStaticGW", "p_staStaticDN1", "p_staStaticDN2"
    };
    const char* types[] = {
      "uint", "uint", "ushort", "ushort", "uint", "ushort", "ushort", "float",
      "float", "float", "float", "string", "string", "string", "string",
      "string", "string", "boolean", "boolean", "boolean", "uint", "string",
      "boolean", "ipaddress", "ipaddress", "ipaddress", "ipaddress", "ipaddress"
    };

    for (size_t i = 0; i < 28; i++) {
      JsonObject obj = data.add<JsonObject>();
      obj["key"] = keys[i];
      obj["type"] = types[i];
      if (!prefs.isKey(keys[i])) {
        obj["value"] = "-1";
        continue;
      }
      if (strcmp(types[i], "uint") == 0) {
        obj["value"] = String(prefs.getUInt(keys[i], -1));
      } else if (strcmp(types[i], "ushort") == 0) {
        obj["value"] = String(prefs.getUShort(keys[i], -1));
      } else if (strcmp(types[i], "float") == 0) {
        obj["value"] = String(prefs.getFloat(keys[i], -1.0), 7);
      } else if (strcmp(types[i], "string") == 0 || strcmp(types[i], "ipaddress") == 0) {
        String value = prefs.getString(keys[i], "-1");
        obj["value"] = value;
      } else if (strcmp(types[i], "boolean") == 0) {
        obj["value"] = prefs.getBool(keys[i], false) ? "true" : "false";
      }
    }
    prefs.end();

    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Set Config
  server.on("/set-config", HTTP_POST, [](AsyncWebServerRequest *request) {}, 
  nullptr, // No upload handler
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!authenticate(request)) return;

    static String body;
    if (index == 0) {
      body = "";
      body.reserve(512);
    }

    if (len > 0 && total < 512) {
      body.concat((char*)data, len);
    }

    if (index + len == total) {
      JsonDocument doc;
      JsonDocument response;

      if (body.isEmpty()) {
        response["status"] = "error";
        response["message"] = "No data received.";
        String json;
        serializeJson(response, json);
        logSerial.printf("[ERROR] ❌ No Data was Received in HTTP Packet Body.\n");
        yield();
        request->send(400, "application/json", json);
        return;
      }

      DeserializationError error = deserializeJson(doc, body);
      if (error) {
        response["status"] = "error";
        response["message"] = "Invalid JSON data.";
        String json;
        serializeJson(response, json);
        logSerial.printf("[ERROR] ❌ JSON Deserialization Failed: %s\n", error.c_str());
        yield();
        request->send(400, "application/json", json);
        return;
      }

      String key = doc["key"].as<String>();
      String value = doc["value"].as<String>();
      String type = doc["type"].as<String>();

      if (value.isEmpty()) {
        response["status"] = "error";
        response["message"] = "Value cannot be empty.";
        String json;
        serializeJson(response, json);
        logSerial.printf("[ERROR] ❌ P_ Value is Empty.\n");
        yield();
        request->send(400, "application/json", json);
        return;
      }

      Preferences prefs;
      prefs.begin("Startup_Config", false);
      if (!prefs.isKey(key.c_str())) {
        prefs.end();
        response["status"] = "error";
        response["message"] = "Key " + key + " not found.";
        String json;
        serializeJson(response, json);
        logSerial.printf("[ERROR] ❌ P_ Key was Not Found: %s\n", key.c_str());
        yield();
        request->send(400, "application/json", json);
        return;
      }

      bool success = false;
      String currentValue;
      if (type == "uint") {
        currentValue = String(prefs.getUInt(key.c_str(), -1));
        if (currentValue != value) {
          success = prefs.putUInt(key.c_str(), value.toInt());
        } else {
          success = true; // No change, consider success
        }
      } else if (type == "ushort") {
        currentValue = String(prefs.getUShort(key.c_str(), -1));
        if (currentValue != value) {
          success = prefs.putUShort(key.c_str(), value.toInt());
        } else {
          success = true;
        }
      } else if (type == "float") {
        currentValue = String(prefs.getFloat(key.c_str(), -1.0), 7);
        if (currentValue != value) {
          success = prefs.putFloat(key.c_str(), value.toFloat());
        } else {
          success = true;
        }
      } else if (type == "string" || type == "ipaddress") {
        currentValue = prefs.getString(key.c_str(), "-1");
        if (currentValue != value) {
          success = prefs.putString(key.c_str(), value.c_str());
        } else {
          success = true;
        }
      } else if (type == "boolean") {
        value.toLowerCase();
        bool newValue = (value == "true" || value == "1");
        currentValue = prefs.getBool(key.c_str(), false) ? "true" : "false";
        if (currentValue != (newValue ? "true" : "false")) {
          success = prefs.putBool(key.c_str(), newValue);
        } else {
          success = true;
        }
      } else {
        prefs.end();
        response["status"] = "error";
        response["message"] = "Invalid data type.";
        String json;
        serializeJson(response, json);
        logSerial.printf("[ERROR] ❌ Invalid Data Type for P_ Key Value: %s\n", type.c_str());
        yield();
        request->send(400, "application/json", json);
        return;
      }

      logSerial.printf("[NOTICE] 💡 Writing key: %s, value: %s, type: %s - %s\n", key.c_str(), value.c_str(), type.c_str(), success ? "✔️ SUCCESS" : "❌ FAILURE");
      yield();
      delay(33);
      yield();
      prefs.end();

      response["status"] = success ? "success" : "error";
      response["key"] = key;
      response["value"] = value;
      response["type"] = type;
      if (!success) {
        response["message"] = "Failed to save " + type + " preference.";
      }
      String json;
      serializeJson(response, json);
      request->send(success ? 200 : 500, "application/json", json);
    }
  });

  // Erase Config Button
  server.on("/erase-config", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) {
      return request->requestAuthentication();
    }
    Preferences prefs;
    prefs.begin("Startup_Config", false);
    bool success = prefs.clear();
    prefs.end();
    yield();
    delay(100);
    yield();
    logSerial.println("[WARN] 🔔 All Preferences in the Startup_Config Namespace have been Errased.");
    yield();
    JsonDocument response;
    response["status"] = success ? "success" : "error";
    response["message"] = success ? "Settings reset successfully." : "Failed to reset settings.";
    String json;
    serializeJson(response, json);
    AsyncWebServerResponse *resp = request->beginResponse(success ? 200 : 500, "application/json", json);
    resp->addHeader("Connection", "close");
    request->send(resp);
    if (success) {
      yield();
      delay(100);
      ESP.restart();
    }
  });

  // Reboot Button
  server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) {
      return request->requestAuthentication();
    }
    JsonDocument response;
    response["status"] = "success";
    response["message"] = "Rebooting...";
    String json;
    serializeJson(response, json);
    AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
    resp->addHeader("Connection", "close");
    request->send(resp);
    delay(100);
    ESP.restart();
  });

  // Serve Logs Page
  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) {
      return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", logs_html);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
    yield();
  });

  // Get Logs Data
 server.on("/get-logs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authenticate(request)) {
      return request->requestAuthentication();
    }
    JsonDocument response;
    String logs = getLogsForWeb();
    if (logs.startsWith("PSRAM") || logs.startsWith("Invalid") || logs.startsWith("No Logs") || logs.startsWith("Memory")) {
      response["status"] = "error";
      response["message"] = logs;
      logSerial.printf("[ERROR] ❌ Failed to get logs: %s\n", logs.c_str());
    } else {
      response["status"] = "success";
      response["logs"] = logs;
    }
    String json;
    serializeJson(response, json);
    request->send(200, "application/json; charset=utf-8", json);
    yield();
  });

  // Clear Logs
 server.on("/clear-logs", HTTP_POST, [](AsyncWebServerRequest *request) {
  if (!authenticate(request)) {
    return request->requestAuthentication();
  }
  noInterrupts();
  logHead = 0;
  logTail = 0;
  if (psramLogBuffer != nullptr) {
    memset((void*)psramLogBuffer, 0, LOG_BUFFER_SIZE);
    request->send(200, "text/plain", "OK");
  } else {
    logSerial.println("[ERROR] ❌ PSRAM Log Buffer is Null");
    request->send(500, "text/plain", "ERROR: PSRAM buffer not initialized");
  }
  interrupts();
  logSerial.printf("[NOTICE] 💡 System Logs were Cleared on %s\n", rtc.getTime("%d %b %H:%M:%S").c_str());
  yield();
 });

  server.begin();
  logSerial.println("[INFO] ⓘ ESP AsyncWebServer Initialized Successfully!");
}