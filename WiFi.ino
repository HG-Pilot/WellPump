// !!! Adjust Your Max Radio Transmit Power as per Table Below !!!

/**********************************************************************
 * 2025 (Copyleft) Anton Polishchuk
 * ESP32 Well Water Pump project with Web Interface and WiFi.
 * Authtor Contact: 📫 gmail account: antonvp
 * DESCRIPTION:
 * An ESP32-powered system designed to automate water well pump control
 * and water tank management, ideal for cabins and cottages.
 * Features a user-friendly WiFi web interface for seamless monitoring and control.
 * LICENSE:
 * This software is licensed under the MIT License.
 * Redistribution, modification, or forks must include this banner.
 * DISCLAIMER:
 * - This software is provided "as is," without warranty of any kind.
 * - The author is not responsible for any damages, data loss, or issues
 *   arising from its use, under any circumstances.
 * - For more information, refer to the full license terms.
 **********************************************************************/

#include <esp_wifi.h>
#include <WiFi.h>
#include "WiFiStates.h"

WiFiState currentWiFiState = WIFI_DISABLED;
unsigned long wifiStateStartTime = 0;
unsigned long apLastCheckTime = 0;

// Forward Function Declarations
void startAPMode();
void startWiFi();
void handleWiFi();
bool scanForSTA();
void onStationConnected(arduino_event_t* event);
void onStationDisconnected(arduino_event_t* event);
bool testGatewayConnectivity(IPAddress gateway);

/* esp_wifi_set_max_tx_power(Value); Below sets Max Transmit Power in 3 places. Saves power but reduces range.
Value  dBm	 Power	Output	Range
4	    1	 ~5%	1.3 mW	<1m
8	    2	 ~10%	1.6 mW	1-2m
12	    3	 ~15%	2.0 mW	2-3m
16	    4	 ~20%	2.5 mW	3-5m
20	    5	 ~25%	3.2 mW	5-7m
24	    6	 ~30%	4.0 mW	7-10m
28	    7	 ~35%	5.0 mW	10-12m
32	    8	 ~40%	6.3 mW	12-15m
36	    9	 ~45%	7.9 mW	15-18m
40	    10	 ~50%	10 mW	18-20m
44	    11	 ~55%	12.6 mW	20-25m
48	    12	 ~60%	16 mW	25-30m
52      13	 ~65%	20 mW	30-35m
56	    14	 ~70%	25 mW	35-40m
60	    15	 ~75%	32 mW	40-45m
64	    16	 ~80%	40 mW	45-50m
68	    17	 ~85%	50 mW	50-60m
72	    18	 ~90%	63 mW	60-70m
76	    19	 ~95%	79 mW	70-80m
80	    20	 ~100%	100 mW	80-100m
 */

// Function to test gateway connectivity using raw sockets
bool testGatewayConnectivity(IPAddress gateway) {
  WiFiClient client;
  uint16_t ports[] = { 53, 443, 80, 22, 23, 161, 53 };  // Ports to test
  logSerial.println("[WIFI] ⓘ Testing Gateway Connectivity Using Raw Sockets...");

  for (uint16_t port : ports) {
    client.setTimeout(1111);  // 1.1-second timeout for the connection attempt
    yield();
    logSerial.printf("[WIFI] ⓘ Testing Gateway Port %d...\n", port);
    yield();

    if (client.connect(gateway, port)) {  // Attempt to connect to the gateway on the current port
      yield();
      logSerial.printf("[WIFI] ⓘ Gateway is Reachable on Port %d\n", port);
      client.stop();  // Close the client connection
      yield();
      return true;
    } else {
      logSerial.printf("[WIFI] 🔔 Gateway Port %d test failed.\n", port);
      client.stop();  // Explicitly close failed connection
      yield();
      delay(150);
      yield();
    }
  }

  logSerial.println("[WIFI] ❌ Gateway Connectivity Test Failed on All Ports.");
  return false;  // If all port tests fail, return false
}

// Function to Scan for Nearby SSIDs
bool scanForSTA() {
  logSerial.println("[WIFI] 🔍 Scanning for Nearby Access Point SSIDs...");
  int networkCount = WiFi.scanNetworks();
  bool foundTarget = false;

  if (networkCount == 0) {
    logSerial.println("[WIFI] 💡 No nearby WiFi Networks were Found.");
  } else {
    logSerial.printf("[WIFI] 🔎 Found %d Network(s):\n", networkCount);

    // Print all networks first
    for (int i = 0; i < networkCount; i++) {
      logSerial.printf("[WIFI] 📶 %s (Channel: %d)\n", WiFi.SSID(i).c_str(), (int)WiFi.channel(i));

      if (strcmp(WiFi.SSID(i).c_str(), staSSID.c_str()) == 0) {
        foundTarget = true;
      }
    }

    // Report if target was found
    if (foundTarget) {
      logSerial.printf("[WIFI] ✔️ Configured staSSID %s was Found!\n", staSSID.c_str());
      WiFi.scanDelete();
      return true;
    } else {
      logSerial.printf("[WIFI] 🚨 Configured staSSID %s was Not Found.\n", staSSID.c_str());
      WiFi.scanDelete();
      return false;
    }
  }
  return false;
}

// Blocking Initial STA Connection Logic
void startWiFi() {
  logSerial.println("[WIFI] ⓘ Starting Initial WiFi Connection Logic...");

  if (apWifiModeOnly) {
    logSerial.println("[WIFI] 💡 AP-Only Mode Enabled. Skipping STA Connection and Starting in AP Mode.");
    currentWiFiState = WIFI_AP_MODE;
    startAPMode();
    return;
  }


  WiFi.disconnect(true);  // Disconnect from any current network and clear session data
  delay(100);             // Short delay for disconnection
  yield();
  WiFi.persistent(false);              // Disable saving credentials to flash
  WiFi.setHostname("ESP-WellPump");    // Set the hostname for the device
  yield();
  delay(50);  // Allow mode transition to complete
  yield();
  WiFi.setSleep(false);  // Disable Wi-Fi sleep mode for low latency
  WiFi.mode(WIFI_STA);   // Set Wi-Fi mode to Station
  yield();               // Allow other processes to run
  delay(100);            // Short delay for stability
  yield();


  currentWiFiState = WIFI_STA_CONNECTING;
  wifiStateStartTime = millis();

  if (!scanForSTA()) {
    logSerial.printf("[WIFI] 🔔 staSSID %s not found. Switching to AP Mode.\n", staSSID.c_str());
    currentWiFiState = WIFI_AP_MODE;
    startAPMode();
    return;
  }

  if (useStaStaticIP) {
    if (WiFi.config(staStaticIP, staStaticGW, staStaticSM, staStaticDN1, staStaticDN2)) {
      logSerial.println("[WIFI] ⓘ STA Static IP configuration applied.");
    } else {
      logSerial.println("[WIFI] ❌ STA Static IP configuration failed!");
    }
  }

  // Three connection attempts
  for (int attempt = 1; attempt <= 3; ++attempt) {
    logSerial.printf("[WIFI] ⓘ Attempt # %d to connect to configured staSSID: %s\n", attempt, staSSID.c_str());
    WiFi.begin(staSSID.c_str(), staPass.c_str());

    unsigned long attemptStartTime = millis();
    bool connected = false;

    // Allow 33 seconds for connection attempt
    while (millis() - attemptStartTime < 33333) {
      if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        break;
      }
      yield();
      delay(50);  // Avoid spamming the Wi-Fi stack
    }

    if (connected) {
      logSerial.printf("[WiFi] ✅ Connected to %s | IP: %s | Signal Strength: %d dBm\n", staSSID.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      IPAddress currentGateway = WiFi.gatewayIP();
      if (testGatewayConnectivity(currentGateway)) {
        logSerial.println("[WIFI] ✔️ Gateway connectivity verified.");
        currentWiFiState = WIFI_STA_CONNECTED;
        esp_wifi_set_max_tx_power(32);
      } else {
        logSerial.println("[WiFi] ⚠️ Gateway test failed. Switching to AP Mode.");
        currentWiFiState = WIFI_AP_MODE;
        startAPMode();
      }
      return;  // Exit after successful connection
    } else {
      logSerial.printf("[WIFI] ❌ Attempt # %d failed to connect to %s.\n", attempt, staSSID.c_str());
    }
  }

  // If all attempts fail
  logSerial.println("[WiFi] ❌ All STA connection attempts failed. Switching to AP Mode.");
  currentWiFiState = WIFI_AP_MODE;
  startAPMode();
}

void startAPMode() {
  logSerial.println("[WIFI] 💡 Starting in Access Point (AP) Mode...");

  // Set Wi-Fi mode to Access Point
  WiFi.disconnect();
  delay(100);  // Short delay for disconnection
  yield();
  WiFi.persistent(false);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);  // Don't flash settings
  WiFi.setHostname("ESP-WellPump");
  delay(50);  // Allow AP to stabilize
  yield();
  WiFi.mode(WIFI_AP);
  yield();    // Allow other processes to run
  delay(50);  // Short delay for stability
  yield();

  // Configure AP static IP, gateway, and subnet mask
  if (!WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0))) {
    logSerial.println("[WIFI] ❌ Failed to configure AP static IP. Reverting to default.");
  }

  // Start the AP with SSID and Password
  if (!WiFi.softAP(apSSID.c_str(), apPass.c_str())) {
    logSerial.println("[WIFI] ❌ Failed to start AP. Check SSID/Password configuration.");
    return;  // Exit if AP initialization fails
  }
  yield();
  delay(500);  // Allow AP to stabilize
  yield();

  // Set maximum transmission power for the AP
  esp_wifi_set_max_tx_power(32);  // !!! Reduced power (adjust if needed from above table) !!!

  // Enable power-saving by default
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  // Configure beacon interval
  wifi_config_t apConfig;
  if (esp_wifi_get_config(WIFI_IF_AP, &apConfig) == ESP_OK) {
    apConfig.ap.beacon_interval = 500;  // Set beacon interval to 500ms
    esp_wifi_set_config(WIFI_IF_AP, &apConfig);
  } else {
    logSerial.println("[WIFI] ❌ Failed to get AP configuration in AP mode.");
  }

  logSerial.printf("[WIFI] 💡 AP Mode started with SSID: %s on Channel %ld\n", apSSID.c_str(), WiFi.channel());
  logSerial.println("[WIFI] ⓘ Power-saving mode enabled for AP Mode.");

  // Monitor client connections with event handlers
  WiFi.onEvent(onStationConnected, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
  WiFi.onEvent(onStationDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

  // Update the state and reset timers
  currentWiFiState = WIFI_AP_MODE;
  wifiStateStartTime = millis();  // Reset state start time
  apLastCheckTime = millis();     // Reset periodic STA reconnection timer
}

// Event handlers
void onStationConnected(arduino_event_t* event) {
  esp_wifi_set_ps(WIFI_PS_NONE);
  logSerial.printf("[WIFI] ⓘ %s Client connected. Disabling power-saving for AP.\n", rtc.getTime("%d %b %H:%M:%S").c_str());
}

void onStationDisconnected(arduino_event_t* event) {
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  logSerial.printf("[WIFI] ⓘ %s Client disconnected. Enabling power-saving for AP.\n", rtc.getTime("%d %b %H:%M:%S").c_str());
}

// Non-blocking WiFi Handler
void handleWiFi() {
  switch (currentWiFiState) {
    case WIFI_STA_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        IPAddress currentGateway = WiFi.gatewayIP();
        if (testGatewayConnectivity(currentGateway)) {
          logSerial.println("[WIFI] ✔️ Gateway connectivity has been verified successfully.");
          currentWiFiState = WIFI_STA_CONNECTED;
          wifiStateStartTime = millis();
          logSerial.printf("[WIFI] ✅ %s Reconnected to %s on Channel: %ld, Signal: %d dBm, IP: %s\n", rtc.getTime("%d %b %H:%M:%S").c_str(), staSSID.c_str(), WiFi.channel(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
          esp_wifi_set_max_tx_power(32);
        } else {
          logSerial.println("[WIFI] ❌ Gateway Test has failed. Switching to AP Mode.");
          currentWiFiState = WIFI_AP_MODE;
          wifiStateStartTime = millis();
          startAPMode();
        }
      } else if (millis() - wifiStateStartTime > 33333) {
        logSerial.printf("[WIFI] ❌ %s Reconnect Timeout. Switching to AP Mode.\n", staSSID.c_str());
        currentWiFiState = WIFI_AP_MODE;
        wifiStateStartTime = millis();
        startAPMode();
      }
      break;

    case WIFI_STA_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        logSerial.printf("[WIFI] ⚠️ %s STA got Disconnected. Attempting to Reconnect...\n", rtc.getTime("%d %b %H:%M:%S").c_str());
        currentWiFiState = WIFI_STA_RECONNECT;
        wifiStateStartTime = millis();
      }
      break;

    case WIFI_STA_RECONNECT:
      if (WiFi.status() == WL_CONNECTED) {
        currentWiFiState = WIFI_STA_CONNECTING;  // Move to verification
        wifiStateStartTime = millis();

      } else {
        logSerial.println("[WIFI] 💡 Attempting STA reconnection...");
        WiFi.disconnect(true, true);  // Disconnect from any current network
        delay(100);                   // Short delay for disconnection
        yield();
        WiFi.persistent(false);              // Disable saving credentials to flash
        WiFi.setHostname("ESP-WellPump");    // Set the hostname for the device
        yield();
        delay(50);  // Allow mode transition to complete
        yield();
        WiFi.setSleep(false);  // Disable Wi-Fi sleep mode for low latency
        WiFi.mode(WIFI_STA);   // Set Wi-Fi mode to Station
        esp_wifi_set_ps(WIFI_PS_NONE);
        esp_wifi_set_channel(0, WIFI_SECOND_CHAN_NONE);  // Use channel 0 for auto-selection
        yield();                                         // Allow other processes to run
        delay(100);                                      // Short delay for stability
        yield();
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);  // Clear cached IP

        if (useStaStaticIP) {
          WiFi.config(staStaticIP, staStaticGW, staStaticSM, staStaticDN1, staStaticDN2);
        }

        WiFi.begin(staSSID.c_str(), staPass.c_str());
        yield();
        delay(500);
        yield();
        currentWiFiState = WIFI_STA_CONNECTING;  // Transition to connecting state
        wifiStateStartTime = millis();
        yield();
      }
      break;

    case WIFI_AP_MODE:
      if (!apWifiModeOnly && (millis() - apLastCheckTime > 336699)) {  // ~5 min STA SSID check
        apLastCheckTime = millis();
        logSerial.printf("[WIFI] ⓘ %s Checking STA availability from AP Mode...\n", rtc.getTime("%d %b %H:%M:%S").c_str());
        if (scanForSTA()) {
          WiFi.mode(WIFI_OFF);
          currentWiFiState = WIFI_STA_RECONNECT;  // Then hand off to reconnect logic
          wifiStateStartTime = millis();
        } else {
          logSerial.printf("[WIFI] 🔔 SSID %s was not found. Remaining in AP Mode.\n", staSSID.c_str());
        }
      }
      break;

    case WIFI_DISABLED:
      static bool wifiDisabledMessagePrinted = false;
      if (!wifiDisabledMessagePrinted) {
        logSerial.printf("[WIFI] ⭕ %s WiFi has been disabled, Reboot ESP32 to Re-Enable.\n", rtc.getTime("%d %b %H:%M:%S").c_str());
        wifiDisabledMessagePrinted = true;
      }
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      break;
  }
}