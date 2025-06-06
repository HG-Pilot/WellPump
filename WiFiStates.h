#pragma once

// WiFi State Machine Enum
enum WiFiState {
  WIFI_DISABLED,
  WIFI_STA_CONNECTING,
  WIFI_STA_CONNECTED,
  WIFI_STA_RECONNECT,
  WIFI_AP_MODE
};

// Declare global variables as extern
extern WiFiState currentWiFiState;