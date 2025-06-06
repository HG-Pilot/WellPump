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

// Include Arduino Libraries (RTTTL is external)
#include <Wire.h>
#include <atomic>
#include <RTTTL.h>             // Get Lib here: https://github.com/2ni/ESP32-RTTTL and then import it.
#include <VL53L0X.h>           // Install from Arduino IDE, info: https://github.com/pololu/vl53l0x-arduino
#include <AsyncTCP.h>
#include <ESP32Time.h>
#include "PumpStates.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_SHT31.h>
#include <esp32-hal-timer.h>
#include <Adafruit_ADS1X15.h>
#include <ESPAsyncWebServer.h>
#include "Freenove_WS2812_Lib_for_ESP32.h"

// Define the pins and addresses
#define BOOT_BUTTON_PIN 0                  // Get Out of Ampere Jail or HALTED_PUMP state Button, use After Cleaning your Sediment Filter
#define SDA_PIN 1                          // I2C SDA Pin White
#define SCL_PIN 2                          // I2C SCL Pin Yellow Or Green
#define WATER_PUMP_RELAY_PIN 4             // Relay 1 Pin
#define EMERGENCY_PUMP_DISCONNECT_PIN 5    // Relay 2 Pin
#define WATER_CRITICAL_LEVEL_SENSOR_PIN 6  // Moisture sensor DI Pin
#define ADS_ALERT_PIN 7                    // ALERT/RDY Pin ADS1115 Int
#define BUZZER_PIN 15                      // Buzzer connected to GPIO 15
#define NEOPIXEL_PIN 48                    // LED Pin
#define TOF_SENSOR_ADDRESS 0x29            // ToF Laser Sensor Address
#define TEMP_SENSOR_ADDRESS 0x44           // Temp Sensor Address
#define ADS_SENSOR_ADDRESS 0x48            // ADC+PGA Sensor Address

// Declare global state variable
PumpState currentState = static_cast<PumpState>(-1);  // Invalid initial value

// Time Variables
const char* ntpServers[] = { "time.nist.gov", "time.epl.ca", "pool.ntp.org" };
String localTimeZone = "PST8PDT";
uint32_t ntpSyncInterval = 43333;  // ~12 hours in seconds
int64_t stateStartTime = 0;
int64_t lastSensorUpdate = 0;
int64_t ntpLastSyncTime = 0;
int64_t ntpNewTime = 0;
int64_t systemStartTime = 0;
volatile int64_t now = 0;
bool ntpSyncDisabled = false;
std::atomic<time_t> stateRemainingSeconds(0);  // Tracks timer countdown
hw_timer_t* stateCountdownTimer = NULL;        // GPTimer handle
bool isHwTimer0Running = false;                // Track Hardware Timer 0 State

// Global Variables for Water Level Control
uint32_t wellRecoveryInterval = 14400; // in seconds
uint32_t pumpRuntimeInterval = 540;    // in seconds
uint16_t desiredWaterLevel = 150;      // Pump water to this target level
uint16_t hysteresisThreshold = 160;    // Pump Hysteresis Vlaue
uint16_t currentWaterLevel = 1;        // Distance from ToF Sensor to Water Level in mm
uint32_t sensorUpdateInterval = 1200;  // in seconds
uint16_t tankFillPercentage = 0;       // Updated by getWaterPercentage()
uint16_t tankEmptyHeight = 737;        // 0% level (mm)
uint16_t tankFullHeight = 150;         // 100% level (mm)
uint32_t totalPumpCycles = 0;
int64_t lastPumpStart = 0;  // Last time Pump started
int64_t lastPumpStop = 0;   // Last time Pump stopped
bool pumpIsHalted = false;  // Disables Pump
bool playMusic = true;      // Music enabled by default

// Error counters & Variables
volatile bool criticalWaterLevel = false;     // Variable Updated directly by ISR, stores if water level is normal or critical 
volatile bool overCurrentProtection = false;  //Reset to 0 by Boot Button, Leads to Purple error state.
volatile bool errorStuckRelay = false;        //Reboot to clear, Leads to Red error state, Check Relay 1 if true.
uint16_t errorPumpStart = 0;
uint16_t errorRedCount = 0;
uint16_t errorPurpleCount = 0;
uint16_t errorYellowCount = 0;
uint32_t errorCriticalLvlCount = 0;
int64_t errorLastRecovery = 0;

// Global Temperature variables
float waterTankTemp = 0.0F;
float prePumpTemp = 0.0F;
float postPumpTemp = 0.0F;
float tankDeltaTemp = 0.0F;

// Global Current (Amps) variables
float currentPumpAmps = 0.0F;  // getPumpCurrent() updates this Var
float prePumpAmps = 0.0F;
float postPumpAmps = 0.0F;
float deltaPumpAmps = 0.0F;
float pumpMaxAmps = 2.45F;  // Value from Pref
float pumpOnAmps = 1.11F;   // Value from Pref 1.2
float pumpOffAmps = 0.36F;  // Value from Pref 0.3

// ADS Constants & Vars
const float multiplier = 0.0625F;       // ADS1115 default multiplier for GAIN_TWO 0.0625F
float adsCalibrationFactor = 0.01033F;  // Use for calibration of your ADS1115 ADC
uint16_t adcSampleCount = 0;            // Tracks ADS voltage samples per Window
volatile bool adsDataReady = false;     // ADS & ISR Data Readiness Flag

// WiFi SSIDs & Passwords
String apSSID = "ESP32_WellPump";
String apPass = "12345678";
String staSSID = "YourSSID";
String staPass = "12345678";

// Static IP Configuration (STA Mode)
IPAddress staStaticIP(192, 168, 1, 3);
IPAddress staStaticSM(255, 255, 255, 0);
IPAddress staStaticGW(192, 168, 1, 1);
IPAddress staStaticDN1(8, 8, 8, 8);
IPAddress staStaticDN2(1, 1, 1, 1);
bool useStaStaticIP = false;              // Enable Static IP in STA WiFi Mode
bool apWifiModeOnly = false;              // Use AP WiFi Mode only and don't try to re-connect to STA SSID

// WebServer Login credentials
String webServerUser = "admin";
String webServerPass = "WellPump";

// Forward Declarations, these live in Web.ino & WiFi.ino
extern void initWebServer();
extern void startWiFi();
extern void handleWiFi();

// PSRAM Log Buffer Vars & Functions
const size_t LOG_BUFFER_SIZE = 61440; // 60KB !!! It will crash at 64KB so stay below it !!! To go above 64KB you would need to chunk the JSON logs to ~48KB and reasemble at the client side, no time to do this :-(
char* volatile psramLogBuffer = nullptr;
volatile size_t logHead = 0;
volatile size_t logTail = 0;

class PSRAMLogger : public Print {
private:
  static const size_t SRAM_BUFFER_SIZE = 128; // Small SRAM intermediate buffer for chunking
  char sramBuffer[SRAM_BUFFER_SIZE]; // SRAM buffer array
  size_t sramIndex = 0; // Tracks SRAM buffer position
public:
  PSRAMLogger() { // Initialize SRAM buffer
    memset(sramBuffer, 0, SRAM_BUFFER_SIZE);
  }
  size_t write(uint8_t c) override {
    // Store in SRAM buffer
    if (psramLogBuffer) {
      sramBuffer[sramIndex++] = c; // Store in SRAM buffer
      if (c == '\n' || sramIndex >= SRAM_BUFFER_SIZE) { // Changed: Commit on \n or full
        noInterrupts(); // Protect only PSRAM commit
        for (size_t i = 0; i < sramIndex; i++) {
          psramLogBuffer[logHead] = sramBuffer[i];
          logHead = (logHead + 1) % LOG_BUFFER_SIZE;
          if (logHead == logTail) logTail = (logTail + 1) % LOG_BUFFER_SIZE;
        }
        interrupts();
        sramIndex = 0; // Reset SRAM buffer index
      }
    }
    // Always mirror to Serial
    return Serial.write(c);
  }
};

// Global instance
PSRAMLogger logSerial;

void setupLogger() {
  Serial.begin(115200); // Initialize real Serial first
  if (!psramInit()) {   // Initialize PSRAM
    Serial.println("[ERROR] ❌ PSRAM Init Failed");
    return;
  }
  if (!psramFound()) {
    Serial.println("[ERROR] ❌ PSRAM Was Not Found");
    return;
  }
  psramLogBuffer = (char*)ps_malloc(LOG_BUFFER_SIZE);
  if (!psramLogBuffer) { // Check ps_malloc failure
    Serial.println("[ERROR] ❌ PSRAM allocation failed for Log Buffer");
    return;
  }
  Serial.println("[INFO] ⓘ PSRAM Logger Initialized Successfully!, 480KB Allocated");
}

String getLogsForWeb() {
  if (!psramLogBuffer) return "PSRAM Logging Not Available";
  
  if (logHead >= LOG_BUFFER_SIZE || logTail >= LOG_BUFFER_SIZE) return "Invalid buffer indices";

  noInterrupts();
  size_t currentTail = logTail;
  size_t currentHead = logHead;
  interrupts();

  if (currentHead == currentTail) return "No Logs Available";

  if (currentHead >= LOG_BUFFER_SIZE || currentTail >= LOG_BUFFER_SIZE) return "Invalid Buffer Indices";

  // Create a temporary non-volatile buffer
  size_t available = (currentHead >= currentTail) ? 
    (currentHead - currentTail) : 
    (LOG_BUFFER_SIZE - currentTail + currentHead);
    logSerial.printf("[INFO] ⓘ Sending %u bytes of Logs to Web, Free Heap: %lu, Free PSRAM: %lu\n", available, ESP.getFreeHeap(), ESP.getFreePsram());
  if (!psramFound()) return "PSRAM Not Initialized";
  char* tempBuffer = (char*)ps_malloc(available + 1);
  if (!tempBuffer) return "PSRAM Log Buffer Memory Error";

  // Copy from volatile to temporary buffer
  if (currentHead > currentTail) {
    memcpy(tempBuffer, (const void*)(psramLogBuffer + currentTail), available);
  } else {
    size_t firstChunk = LOG_BUFFER_SIZE - currentTail;
    memcpy(tempBuffer, (const void*)(psramLogBuffer + currentTail), firstChunk);
    memcpy(tempBuffer + firstChunk, (const void*)psramLogBuffer, currentHead);
  }

  tempBuffer[available] = '\0';
  String result(tempBuffer);
  free(tempBuffer);
  
  return result;
}

// Web server object
AsyncWebServer server(80);

// Declaring the RTC object
ESP32Time rtc;

// Declaring ToF_Sensor object
VL53L0X ToF_Sensor;

// Declaring Temp_Sensor object
Adafruit_SHT31 sht31;

// Declaring ADS_Sensor object
Adafruit_ADS1115 ads;

// Global RTTTL object
RTTTL rtttl(BUZZER_PIN); // Default LEDC channel 0

// Declaring LED strip
Freenove_ESP32_WS2812 strip(1, NEOPIXEL_PIN, 0, TYPE_GRB);

//Setting up an ISR to update a Variable when Pin6 goes Low. Choose from Two Options Compatibility vs speed, second is faster.
// void IRAM_ATTR criticalWaterLevelISR() { criticalWaterLevel = (digitalRead(WATER_CRITICAL_LEVEL_SENSOR_PIN) == LOW);}
void IRAM_ATTR criticalWaterLevelISR() {
  criticalWaterLevel = !(REG_READ(GPIO_IN_REG) & (1 << WATER_CRITICAL_LEVEL_SENSOR_PIN));
  errorCriticalLvlCount++;
}

// ISR for ADS ALERT/RDY Pin to Set flag when data is ready
void IRAM_ATTR adsDataReadyISR() {
  adsDataReady = true;
}

// ISR for Well Recovery & Pump Timers
void IRAM_ATTR onTimer() {
  time_t current = stateRemainingSeconds.load();  // Atomic read
  if (current > 0) {
    stateRemainingSeconds.store(current - 1);  // Atomic write
  }
}

// RTTTL melodies for each state
const char* WellRecovery = "SMBtheme:d=4,o=5,b=100:8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,16p";
const char* PumpingWater = "DasBoot:d=4,o=5,b=100:d#.4,8d4,8c4,8d4,8d#4,8g4,a#.4,8a4,8g4,8a4,8a#4,8d,2f.,p";
const char* ErrorRed = "StarWars:d=4,o=5,b=112:8d.,16p,8d.,16p,8d.,16p,8a#4,16p,16f,8d.,16p,8a#4,16p,16f,d.,8p,8a.,16p,8a.,16p,8a.,16p,8a#,16p,16f,8c#.,16p,8a#4,16p,16f,d.,8p";
const char* ErrorPurple = "Impossible:d=16,o=5,b=100:32d,32d#,32e,32f,32f#,32g,g,8p,g,8p,a#,p,c6,p,g,8p,g,8p,f,p,f#,p,g,8p,g,8p,a#,p,c6,p,g,8p,g,8p,f,p,f#,p,a#,g,2d,32p,a#,g,2c#,32p,a#,g,2c,p,a#4,c";
const char* ErrorYellow = "Toccata:d=4,o=5,b=160:16a4,16g4,1a4,16g4,16f4,16d4,16e4,2c#4,16p,d.4,2p,16a4,16g4,1a4,8e.4,8f.4,8c#.4,2d4";
const char* HaltedPump = "SMBunderground:d=16,o=6,b=100:c,c5,a5,a,a#5,a#,2p,8p,c,c5,a5,a,a#5,a#,2p,8p,f5,f,d5,d,d#5,d#,2p,8p,f5,f,d5,d,d#5,d#,2p,32d#,d,32c#,c,p,d#,p,d,p,g#5,p,g5,p,c#,p,32c,f#,32f,32e,a#,32a,g#,32p,d#,b5,32p,a#5,32p,a5,g#5";

// Factory Reset Function to initialize Preferences in NVS
void initializePreferences() {
  Preferences prefs;
  prefs.begin("Startup_Config", false);  // Open the namespace in read/write mode

  // Check if already initialized
  if (!prefs.isKey("initialized")) {
    logSerial.println("[WARN] 🔔 First boot detected: Writing default preferences...");

    // Populate default preference values
    prefs.putUInt("p_wellRecIntvl", 14400);         // Well recovery interval (seconds)
    prefs.putUInt("p_pumpRunIntvl", 540);           // Pump run interval (seconds)
    prefs.putUShort("p_desiredWtrLvl", 150);        // Desired water level (cm)
    prefs.putUShort("p_hystThreshold", 160);        // Hysteresis threshold (cm)
    prefs.putUInt("p_sensrUpdIntvl", 1200);         // Sensor update interval (seconds)
    prefs.putUShort("p_tankEmptyHgt", 737);         // Tank empty height (cm)
    prefs.putUShort("p_tankFullHgt", 150);          // Tank full height (cm)
    prefs.putFloat("p_adsCalbFactor", 0.01033);     // ADS calibration factor
    prefs.putFloat("p_pumpMaxAmps", 2.45);          // Max pump current (amps)
    prefs.putFloat("p_pumpOnAmps", 1.11);           // Pump ON current (amps)
    prefs.putFloat("p_pumpOffAmps", 0.36);          // Pump OFF current (amps)
    prefs.putString("p_apSSID", "ESP32_WellPump");  // AP Mode WiFi SSID
    prefs.putString("p_apPass", "12345678");        // AP Mode WiFi password
    prefs.putString("p_staSSID", "YourSSID");          // STA Mode WiFi SSID
    prefs.putString("p_staPass", "12345678");      // STA Mode WiFi password
    prefs.putString("p_webUser", "admin");          // Web UI username
    prefs.putString("p_webPass", "WellPump");       // Web UI password
    prefs.putBool("p_apWifiModeOly", false);        // Set stand alone AP WiFi mode
    prefs.putBool("p_ntpSyncDsbld", false);         // Enable time sync with NTP servers
    prefs.putBool("p_playMusic", true);             // Enable music effects
    prefs.putUInt("p_ntpSyncIntvl", 43333);         // Interval bertween NTP sync attempts
    prefs.putString("p_localTimeZone", "PST8PDT");  // Local time zone
    prefs.putBool("p_useStaStatcIP", false);        // Use Static IP in STA WiFi mode
    prefs.putString("p_staStaticIP", "192.168.1.3");   // Static IP Address
    prefs.putString("p_staStaticSM", "255.255.255.0"); // Static Subnet Mask
    prefs.putString("p_staStaticGW", "192.168.1.1");   // Static Default Gateway
    prefs.putString("p_staStaticDN1", "8.8.8.8");      // Static Primary DNS Server
    prefs.putString("p_staStaticDN2", "1.1.1.1");      // Static Secondary DNS Server

    // Mark preferences as initialized
    prefs.putBool("initialized", true);
    logSerial.println("[NOTICE] 💡 Default preferences were written successfully!");
  } else {
    logSerial.println("[INFO] ⓘ Preferences already initialized. Skipping default initialization.");
  }
  prefs.end();  // Close Preferences
}

// Copy Startup Config to Running Config Function
void startUpConfig() {
  Preferences prefs;
  prefs.begin("Startup_Config", true);
  bool configSuccess = true;

  // Check NVS initialization at the start
  if (!prefs.isKey("initialized")) {
    logSerial.println("[ERROR] ❌ Failed to Apply Start-Up Configuration Factory Reset Required!");
    prefs.end();
    return;
  }

  // UInt preferences
  if (prefs.isKey("p_wellRecIntvl")) {
    wellRecoveryInterval = prefs.getUInt("p_wellRecIntvl", 14400);
  } else {
    wellRecoveryInterval = 14400;
    logSerial.println("[ERROR] ❌ Failed to read key p_wellRecIntvl, using default: 14400");
    configSuccess = false;
  }

  if (prefs.isKey("p_pumpRunIntvl")) {
    pumpRuntimeInterval = prefs.getUInt("p_pumpRunIntvl", 540);
  } else {
    pumpRuntimeInterval = 540;
    logSerial.println("[ERROR] ❌ Failed to read key p_pumpRunIntvl, using default: 540");
    configSuccess = false;
  }

  if (prefs.isKey("p_sensrUpdIntvl")) {
    sensorUpdateInterval = prefs.getUInt("p_sensrUpdIntvl", 1200);
  } else {
    sensorUpdateInterval = 1200;
    logSerial.println("[ERROR] ❌ Failed to read key p_sensrUpdIntvl, using default: 1200");
    configSuccess = false;
  }

  if (prefs.isKey("p_ntpSyncIntvl")) {
    ntpSyncInterval = prefs.getUInt("p_ntpSyncIntvl", 43333);
  } else {
    ntpSyncInterval = 43333;
    logSerial.println("[ERROR] ❌ Failed to read key p_ntpSyncIntvl, using default: 43333");
    configSuccess = false;
  }

  // UShort preferences
  if (prefs.isKey("p_desiredWtrLvl")) {
    desiredWaterLevel = prefs.getUShort("p_desiredWtrLvl", 150);
  } else {
    desiredWaterLevel = 150;
    logSerial.println("[ERROR] ❌ Failed to read key p_desiredWtrLvl, using default: 150");
    configSuccess = false;
  }

  if (prefs.isKey("p_hystThreshold")) {
    hysteresisThreshold = prefs.getUShort("p_hystThreshold", 160);
  } else {
    hysteresisThreshold = 160;
    logSerial.println("[ERROR] ❌ Failed to read key p_hystThreshold, using default: 160");
    configSuccess = false;
  }

  if (prefs.isKey("p_tankEmptyHgt")) {
    tankEmptyHeight = prefs.getUShort("p_tankEmptyHgt", 737);
  } else {
    tankEmptyHeight = 737;
    logSerial.println("[ERROR] ❌ Failed to read key p_tankEmptyHgt, using default: 737");
    configSuccess = false;
  }

  if (prefs.isKey("p_tankFullHgt")) {
    tankFullHeight = prefs.getUShort("p_tankFullHgt", 150);
  } else {
    tankFullHeight = 150;
    logSerial.println("[ERROR] ❌ Failed to read key p_tankFullHgt, using default: 150");
    configSuccess = false;
  }

  // Float preferences
  if (prefs.isKey("p_adsCalbFactor")) {
    adsCalibrationFactor = prefs.getFloat("p_adsCalbFactor", 0.01033);
  } else {
    adsCalibrationFactor = 0.01033;
    logSerial.println("[ERROR] ❌ Failed to read key p_adsCalbFactor, using default: 0.01033");
    configSuccess = false;
  }

  if (prefs.isKey("p_pumpMaxAmps")) {
    pumpMaxAmps = prefs.getFloat("p_pumpMaxAmps", 2.45);
  } else {
    pumpMaxAmps = 2.45;
    logSerial.println("[ERROR] ❌ Failed to read key p_pumpMaxAmps, using default: 2.45");
    configSuccess = false;
  }

  if (prefs.isKey("p_pumpOnAmps")) {
    pumpOnAmps = prefs.getFloat("p_pumpOnAmps", 1.11);
  } else {
    pumpOnAmps = 1.11;
    logSerial.println("[ERROR] ❌ Failed to read key p_pumpOnAmps, using default: 1.11");
    configSuccess = false;
  }

  if (prefs.isKey("p_pumpOffAmps")) {
    pumpOffAmps = prefs.getFloat("p_pumpOffAmps", 0.36);
  } else {
    pumpOffAmps = 0.36;
    logSerial.println("[ERROR] ❌ Failed to read key p_pumpOffAmps, using default: 0.36");
    configSuccess = false;
  }

  // String preferences
  if (prefs.isKey("p_apSSID")) {
    apSSID = prefs.getString("p_apSSID", "ESP32_WellPump");
  } else {
    apSSID = "ESP32_WellPump";
    logSerial.println("[ERROR] ❌ Failed to read key p_apSSID, using default: ESP32_WellPump");
    configSuccess = false;
  }

  if (prefs.isKey("p_apPass")) {
    apPass = prefs.getString("p_apPass", "12345678");
  } else {
    apPass = "12345678";
    logSerial.println("[ERROR] ❌ Failed to read key p_apPass, using default: 12345678");
    configSuccess = false;
  }

  if (prefs.isKey("p_staSSID")) {
    staSSID = prefs.getString("p_staSSID", "YourSSID");
  } else {
    staSSID = "YourSSID";
    logSerial.println("[ERROR] ❌ Failed to read key p_staSSID, using default: YourSSID");
    configSuccess = false;
  }

  if (prefs.isKey("p_staPass")) {
    staPass = prefs.getString("p_staPass", "12345678");
  } else {
    staPass = "12345678";
    logSerial.println("[ERROR] ❌ Failed to read key p_staPass, using default: 12345678");
    configSuccess = false;
  }

  if (prefs.isKey("p_webUser")) {
    webServerUser = prefs.getString("p_webUser", "admin");
  } else {
    webServerUser = "admin";
    logSerial.println("[ERROR] ❌ Failed to read key p_webUser, using default: admin");
    configSuccess = false;
  }

  if (prefs.isKey("p_webPass")) {
    webServerPass = prefs.getString("p_webPass", "WellPump");
  } else {
    webServerPass = "WellPump";
    logSerial.println("[ERROR] ❌ Failed to read key p_webPass, using default: WellPump");
    configSuccess = false;
  }

  if (prefs.isKey("p_localTimeZone")) {
    localTimeZone = prefs.getString("p_localTimeZone", "PST8PDT");
  } else {
    localTimeZone = "PST8PDT";
    logSerial.println("[ERROR] ❌ Failed to read key p_localTimeZone, using default: PST8PDT");
    configSuccess = false;
  }

  // Bool preferences
  if (prefs.isKey("p_apWifiModeOly")) {
    apWifiModeOnly = prefs.getBool("p_apWifiModeOly", false);
  } else {
    apWifiModeOnly = false;
    logSerial.println("[ERROR] ❌ Failed to read key p_apWifiModeOly, using default: false");
    configSuccess = false;
  }

  if (prefs.isKey("p_ntpSyncDsbld")) {
    ntpSyncDisabled = prefs.getBool("p_ntpSyncDsbld", false);
  } else {
    ntpSyncDisabled = false;
    logSerial.println("[ERROR] ❌ Failed to read key p_ntpSyncDsbld, using default: false");
    configSuccess = false;
  }

  if (prefs.isKey("p_playMusic")) {
    playMusic = prefs.getBool("p_playMusic", true);
  } else {
    playMusic = true;
    logSerial.println("[ERROR] ❌ Failed to read key p_playMusic, using default: true");
    configSuccess = false;
  }

  if (prefs.isKey("p_useStaStatcIP")) {
    useStaStaticIP = prefs.getBool("p_useStaStatcIP", false);
  } else {
    useStaStaticIP = false;
    logSerial.println("[ERROR] ❌ Failed to read key p_useStaStatcIP, using default: false");
    configSuccess = false;
  }

  // IPAddress preferences
  // Note: p_staStaticIP is the Preferences key; staStaticIP is the global IPAddress variable
  if (prefs.isKey("p_staStaticIP")) {
    String ipStr = prefs.getString("p_staStaticIP", "");
    if (ipStr.isEmpty() || !staStaticIP.fromString(ipStr)) {
      staStaticIP = IPAddress(192, 168, 1, 3);
      logSerial.println("[ERROR] ❌ Failed to parse key p_staStaticIP, using default: 192.168.1.3");
      configSuccess = false;
    }
  } else {
    staStaticIP = IPAddress(192, 168, 1, 3);
    logSerial.println("[ERROR] ❌ Failed to read key p_staStaticIP, using default: 192.168.1.3");
    configSuccess = false;
  }

  // Note: p_staStaticSM is the Preferences key; staStaticSM is the global IPAddress variable
  if (prefs.isKey("p_staStaticSM")) {
    String smStr = prefs.getString("p_staStaticSM", "");
    if (smStr.isEmpty() || !staStaticSM.fromString(smStr)) {
      staStaticSM = IPAddress(255, 255, 255, 0);
      logSerial.println("[ERROR] ❌ Failed to parse key p_staStaticSM, using default: 255.255.255.0");
      configSuccess = false;
    }
  } else {
    staStaticSM = IPAddress(255, 255, 255, 0);
    logSerial.println("[ERROR] ❌ Failed to read key p_staStaticSM, using default: 255.255.255.0");
    configSuccess = false;
  }

  // Note: p_staStaticGW is the Preferences key; staStaticGW is the global IPAddress variable
  if (prefs.isKey("p_staStaticGW")) {
    String gwStr = prefs.getString("p_staStaticGW", "");
    if (gwStr.isEmpty() || !staStaticGW.fromString(gwStr)) {
      staStaticGW = IPAddress(192, 168, 1, 1);
      logSerial.println("[ERROR] ❌ Failed to parse key p_staStaticGW, using default: 192.168.1.1");
      configSuccess = false;
    }
  } else {
    staStaticGW = IPAddress(192, 168, 1, 1);
    logSerial.println("[ERROR] ❌ Failed to read key p_staStaticGW, using default: 192.168.1.1");
    configSuccess = false;
  }

  // Note: p_staStaticDN1 is the Preferences key; staStaticDN1 is the global IPAddress variable
  if (prefs.isKey("p_staStaticDN1")) {
    String dn1Str = prefs.getString("p_staStaticDN1", "");
    if (dn1Str.isEmpty() || !staStaticDN1.fromString(dn1Str)) {
      staStaticDN1 = IPAddress(8, 8, 8, 8);
      logSerial.println("[ERROR] ❌ Failed to parse key p_staStaticDN1, using default: 8.8.8.8");
      configSuccess = false;
    }
  } else {
    staStaticDN1 = IPAddress(8, 8, 8, 8);
    logSerial.println("[ERROR] ❌ Failed to read key p_staStaticDN1, using default: 8.8.8.8");
    configSuccess = false;
  }

  // Note: p_staStaticDN2 is the Preferences key; staStaticDN2 is the global IPAddress variable
  if (prefs.isKey("p_staStaticDN2")) {
    String dn2Str = prefs.getString("p_staStaticDN2", "");
    if (dn2Str.isEmpty() || !staStaticDN2.fromString(dn2Str)) {
      staStaticDN2 = IPAddress(1, 1, 1, 1);
      logSerial.println("[ERROR] ❌ Failed to parse key p_staStaticDN2, using default: 1.1.1.1");
      configSuccess = false;
    }
  } else {
    staStaticDN2 = IPAddress(1, 1, 1, 1);
    logSerial.println("[ERROR] ❌ Failed to read key p_staStaticDN2, using default: 1.1.1.1");
    configSuccess = false;
  }

  // Final status
  if (configSuccess) {
    logSerial.println("[INFO] ⓘ Start-Up Configuration was Applied Successfully!");
  } else {
    logSerial.println("[ERROR] ❌ Failed to Apply Start-Up Configuration Factory Reset Required!");
  }

  prefs.end();
}

// Initialize hardware timer
void setupStateTimer() {
  // Begin timer with 1 KHz frequency (1 ms resolution)
  stateCountdownTimer = timerBegin(1000);
  if (stateCountdownTimer == NULL) {
    logSerial.println("[ERROR] ❌ Failed to Initialize Hardware Timer 0!");
    yield();
    return;
  }

  // Attach interrupt service routine
  timerAttachInterrupt(stateCountdownTimer, &onTimer);

  // Set alarm to trigger every 1 second (1000 ticks at 1 KHz), auto-reload
  timerAlarm(stateCountdownTimer, 1000, true, 0);
  logSerial.println("[INFO] ⓘ Hardware State Timer 0 Initialized Successfully!");
  yield();
}

// Start the state timer with a given duration
void startStateTimer(uint32_t duration_seconds) {
  stateRemainingSeconds.store(duration_seconds);  // Atomic write
  yield();
  yield();
  if (!isHwTimer0Running) {
    timerStart(stateCountdownTimer);
    isHwTimer0Running = true;
  }
  //logSerial.printf("[DEBUG] State timer started for %lu seconds\n", duration_seconds);
}

// Stop the state timer
void stopStateTimer() {
  stateRemainingSeconds.store(0);  // Atomic write
  yield();
  yield();
  if (isHwTimer0Running) {
    timerStop(stateCountdownTimer);
    isHwTimer0Running = false;
  }
  //logSerial.println("[DEBUG] State timer stopped");
}

// Function to read Water Temp to different Variables
bool getWaterTemp(const char* parameter = nullptr) {
  float temp = sht31.readTemperature();

  if (isnan(temp)) {
    waterTankTemp = prePumpTemp = postPumpTemp = 0.0F;
    return false;
  }

  if (!parameter) {
    waterTankTemp = temp;
    yield();
  } else if (strcmp(parameter, "pre") == 0) {
    prePumpTemp = temp;
    yield();
  } else if (strcmp(parameter, "post") == 0) {
    postPumpTemp = temp;
    tankDeltaTemp = postPumpTemp - prePumpTemp;
    yield();
  }
  return true;
}

// Helper function to implement compare-swap in the sorting network below
void compareSwap(uint16_t& a, uint16_t& b) {
  if (a > b) {
    uint16_t temp = a;
    a = b;
    b = temp;
  }
}

// Function to get Water Level distance to Tank Top reliably with Dual Errors Correction and a Median value
uint16_t getWaterLevel() {
  uint16_t readings[5];

  // Take 5 readings
  for (int i = 0; i < 5; i++) {
    yield();
    uint8_t retries = 0;
    uint16_t distance = 0;

    while (retries < 3) {  // Retry up to 3 times
      distance = ToF_Sensor.readRangeSingleMillimeters();
      yield();

      if (!ToF_Sensor.timeoutOccurred() && distance >= 33 && distance <= 1800) {
        readings[i] = distance;  // Valid reading
        yield();
        break;
      }

      retries++;

      if (currentState != ERROR_RED && currentState != ERROR_PURPLE && currentState != ERROR_YELLOW) {
        logSerial.print("[NOTICE] 💡 ToF Reading ");
        logSerial.print(i);
        logSerial.print(" - Retry ");
        logSerial.print(retries);
        logSerial.println("/3");
        yield();
      }
      yield();
    }

    // If retries exceeded, record invalid reading as 0
    if (retries == 3) {
      readings[i] = 0;
      yield();
      if (currentState != ERROR_RED && currentState != ERROR_PURPLE && currentState != ERROR_YELLOW) {
        logSerial.print("[ALERT] 🚨 Failed to get a valid # ");
        logSerial.print(i);
        logSerial.println(" reading after 3 attempts. Assigning 0.");
        yield();
      }
      yield();
    }
  }

  // Sorting Network recipe to find the median out of 5 readings
  compareSwap(readings[0], readings[1]);
  compareSwap(readings[3], readings[4]);
  compareSwap(readings[2], readings[4]);
  compareSwap(readings[2], readings[3]);
  compareSwap(readings[0], readings[3]);
  compareSwap(readings[0], readings[2]);
  compareSwap(readings[1], readings[4]);
  compareSwap(readings[1], readings[3]);
  compareSwap(readings[1], readings[2]);
  // The median is now in readings[2] (middle of 5 elements)
  uint16_t median = readings[2];

  // Update current water level var & return the value
  currentWaterLevel = median;
  return median;
}

// Function to read the pump current in differential mode
float getPumpCurrent() {
  float voltage = 0.0F;
  float amperes = 0.0F;
  float sum = 0.0F;
  adcSampleCount = 0;  // Sample counter reset before next window
  long startMillis = millis();

  while (millis() - startMillis < 317) {  // Getting ~150 samples)
    yield();
    adsDataReady = false;
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, true);  // Start single conversion
    // Wait for conversion to complete (non-blocking approach)
    uint32_t waitStart = millis();
    while (!adsDataReady && (millis() - waitStart < 10)) {
      delayMicroseconds(1);  // Minimal delay to avoid busy-waiting
      yield();               // Allow WiFi/RTOS tasks (~every 1μs)
    }

    if (adsDataReady) {
      int16_t adcValue = ads.getLastConversionResults();
      voltage = adcValue * multiplier;           // Convert to volts (peak)
      amperes = voltage * adsCalibrationFactor;  // Calculate current in amperes
      sum += sq(amperes);                        // Sum the square of the current
      adcSampleCount++;
      yield();
    } else {
      yield();  // Extra yield if timeout occurs
    }
  }

  // Calculate RMS current
  yield();
  if (adcSampleCount > 0) {
    amperes = sqrtf(sum / adcSampleCount);
    currentPumpAmps = amperes;
  } else {
    logSerial.printf("[ERROR] ❌ %s NO VOLTAGE SAMPLES COLLECTED. RETURNING 0.0\n", rtc.getTime("%d %b %H:%M:%S").c_str());
    yield();
    amperes = 0.0F;  // Default to 0.0 if no samples collected
  }

  return amperes;
}

void getWaterPercentage() {
  const uint16_t range = tankEmptyHeight - tankFullHeight;

  // Clamp the input
  uint16_t adjustedLevel = currentWaterLevel;
  if (adjustedLevel < tankFullHeight) adjustedLevel = tankFullHeight;
  if (adjustedLevel > tankEmptyHeight) adjustedLevel = tankEmptyHeight;

  // Calculate fill range
  uint16_t filledHeight = tankEmptyHeight - adjustedLevel;

  // 60/40 split point calculation
  const uint16_t splitPoint = (range * 60) / 100;

  // Calculate percentage
  uint32_t tempPct;
  if (filledHeight <= splitPoint) {
    // Bottom 60% of volume
    tempPct = (uint32_t)filledHeight * 60 * 100 / splitPoint;
  } else {
    // Top 40% of volume
    uint16_t topHeight = filledHeight - splitPoint;
    tempPct = 60 * 100 + (uint32_t)topHeight * 40 * 100 / (range - splitPoint);
  }

  tankFillPercentage = tempPct / 100;
  if (tankFillPercentage > 100) tankFillPercentage = 100;
}

// Logging State Changes with Data and Time Stamp
void logStateTransition(PumpState state) {
  static PumpState lastState = HALTED_PUMP;
  String timestamp = rtc.getTime("%d %b %H:%M:%S");  // e.g., 06-Apr 15:27:12
  switch (state) {
    case WELL_RECOVERY:
      logSerial.print(timestamp);
      logSerial.print(" 🟢 Entering WELL_RECOVERY state [ Tank ");
      logSerial.print(tankFillPercentage);
      logSerial.print("% | Post ");
      logSerial.print(postPumpAmps, 2);
      logSerial.print("A | Delta ");
      logSerial.print(deltaPumpAmps, 2);
      logSerial.println("A ]");
      yield();
      if (playMusic && lastState != STARTING_PUMP) {
        rtttl.loadSong(WellRecovery);
      }
      break;
    case STARTING_PUMP:
      logSerial.print(timestamp);
      logSerial.print(" ⚪ Entering STARTING_PUMP state [ Level ");
      logSerial.print(currentWaterLevel);
      logSerial.println("mm ]");
      yield();
      break;
    case PUMPING_WATER:
      logSerial.print(timestamp);
      logSerial.print(" 🔵 Entering PUMPING_WATER state [ Level ");
      logSerial.print(currentWaterLevel);
      logSerial.print("mm | Cycles ");
      logSerial.print(totalPumpCycles);
      logSerial.print(" | Pre ");
      logSerial.print(prePumpAmps, 2);
      logSerial.println("A ]");
      yield();
      if (playMusic) {
        rtttl.loadSong(PumpingWater);
      }
      break;
    case STOPPING_PUMP:
      logSerial.print(timestamp);
      logSerial.print(" ⚪ Entering STOPPING_PUMP state [ Level ");
      logSerial.print(currentWaterLevel);
      logSerial.print("mm | Current ");
      logSerial.print(currentPumpAmps, 2);
      logSerial.println("A ]");
      yield();
      break;
    case ERROR_RED:
      logSerial.print(timestamp);
      logSerial.print(" 🔴 Entering ERROR_RED state! [ Reason: ");
      logSerial.print(errorStuckRelay ? "errorStuckRelay " : "");
      logSerial.print(errorPumpStart >= 1 ? "errorPumpStart " : "");
      logSerial.println(" ]");
      yield();
      if (playMusic) {
        rtttl.loadSong(ErrorRed);
      }
      break;
    case ERROR_PURPLE:
      logSerial.print(timestamp);
      logSerial.print(" 🟣 Entering ERROR_PURPLE state! [ Reason: ");
      logSerial.print(overCurrentProtection ? "overCurrentProtection" : "");
      logSerial.println(" ]");
      yield();
      if (playMusic) {
        rtttl.loadSong(ErrorPurple);
      }
      break;
    case ERROR_YELLOW:
      logSerial.print(timestamp);
      logSerial.print(" 🟡 Entering ERROR_YELLOW state! [ Reason: ");
      logSerial.print(criticalWaterLevel ? "criticalWaterLevel " : "");
      logSerial.print(currentWaterLevel == 0 ? "currentWaterLevel " : "");
      logSerial.println(" ]");
      yield();
      if (playMusic) {
        rtttl.loadSong(ErrorYellow);
      }
      break;
    case HALTED_PUMP:
      logSerial.print(timestamp);
      logSerial.print(" ⛔ Entering HALTED_PUMP state! [ Halted: ");
      logSerial.print(pumpIsHalted ? "Yes" : "No");
      logSerial.println(" | ⚠️ Press BOOT to Resume! ]");
      yield();
      if (playMusic) {
        rtttl.loadSong(HaltedPump);
      }
      break;
  }
  // Update lastState for the next transition
  lastState = state;
}

// Sensor update Function using a var for interval
void getSensorsUpdated() {
  if (now - lastSensorUpdate >= sensorUpdateInterval) {
    yield();
    getWaterLevel();
    yield();
    getWaterPercentage();
    yield();
    getWaterTemp();
    yield();
    lastSensorUpdate = now;
  }
}

// NTP Sync and Time Keeping Functions
void ntpInitialSync() {
  if (ntpSyncDisabled) return;  // Never attempt if NTP is Disabled

  if (WiFi.status() != WL_CONNECTED || WiFi.getMode() == WIFI_AP || (ntpSyncDisabled)) {
    rtc.setTime(0);
    logSerial.println("[TIME] ❌ NTP Initial Sync Failed (Will Retry Later), Using Unix Epoch 0");
    systemStartTime = 0;
    return;
  }

  logSerial.println("[TIME] Attempting Initial NTP Sync...");
  bool ntpSyncSuccess = false;
  yield();

  for (int i = 0; i < 3; i++) {  // Hardcoded 3 servers count
    logSerial.printf("[TIME] ⏳ Trying to Sync with %s ", ntpServers[i]);

    if (syncWithServer(ntpServers[i])) {
      ntpLastSyncTime = systemStartTime = rtc.getEpoch();
      logSerial.printf("✅ Success! Now: %s\n", rtc.getTime("%d-%b-%y %H:%M:%S").c_str());
      ntpSyncSuccess = true;
      break;
    }
    configTzTime(localTimeZone.c_str(), "0.invalid");
    yield();  // Allow other tasks to run
  }

  if (!ntpSyncSuccess) {
    rtc.setTime(0);
    systemStartTime = 0;
    configTzTime(localTimeZone.c_str(), "0.invalid");
    logSerial.println("[TIME] ❌ NTP Initial Sync Failed (Will Retry Later), Using Unix Epoch 0");
  }
}

void ntpPeriodicSync() {
  if (ntpSyncDisabled) return;  // Never attempt if NTP is Disabled

  now = rtc.getEpoch();
  yield();

  if (now - ntpLastSyncTime < ntpSyncInterval) return;

  if (WiFi.status() != WL_CONNECTED || WiFi.getMode() == WIFI_AP) {
    logSerial.printf("[TIME] ❌ %s Periodic NTP Sync: WiFi Is Not Ready!\n", rtc.getTime("%d %b %H:%M:%S").c_str());
    ntpLastSyncTime = now;
    return;
  }

  logSerial.printf("[TIME] ⏰ %s Attempting Periodic NTP Sync...\n", rtc.getTime("%d %b %H:%M:%S").c_str());
  now = rtc.getEpoch();
  yield();

  for (int i = 0; i < 3; i++) {
    logSerial.printf("[TIME] ⏳ Trying to Sync with %s ", ntpServers[i]);
     // Adjust time related vars to a new timeline
    if (syncWithServer(ntpServers[i])) {
      int64_t deviation = ntpNewTime - now;
      yield();
      if (systemStartTime == 0) {
        systemStartTime = ntpNewTime - now;
        if (stateStartTime != 0) {
          stateStartTime = ntpNewTime - (now - stateStartTime);
        }
        if (errorLastRecovery != 0) {
          errorLastRecovery = ntpNewTime - (now - errorLastRecovery);
        }
        if (lastSensorUpdate != 0) {
          lastSensorUpdate = ntpNewTime - (now - lastSensorUpdate);
        }
        if (lastPumpStart != 0) {
          lastPumpStart = ntpNewTime - (now - lastPumpStart);
        }
        if (lastPumpStop != 0) {
          lastPumpStop = ntpNewTime - (now - lastPumpStop);
        }
        yield(); 
      }
      logSerial.printf("✅ Success! Deviation: %llds, Now: %s\n", deviation, rtc.getTime("%d-%b-%y %H:%M:%S").c_str());
      now = ntpNewTime;  // Update now to the new timeline
      ntpLastSyncTime = now;
      yield();
      return;
    }
    configTzTime(localTimeZone.c_str(), "0.invalid");
    yield();
  }

  logSerial.printf("[TIME] ❌ %s Periodic NTP Sync Has Failed, Maintaining Old Time!\n", rtc.getTime("%d %b %H:%M:%S").c_str());
  ntpLastSyncTime = now;  // Prevents rapid retries
  configTzTime(localTimeZone.c_str(), "0.invalid");
}

bool syncWithServer(const char* server) {

  if (server == NULL || strlen(server) == 0) {
    logSerial.println("[TIME] ❌ ERROR: Invalid NTP Server Address");
    return false;
  }

  configTzTime(localTimeZone.c_str(), server);
  struct tm timeinfo;
  yield();

  if (!getLocalTime(&timeinfo, 3333)) {  // 3.3 second timeout
    logSerial.printf("❌ Failed!\n[TIME] 🔔 NTP Sync Timeout has Occurred!\n");
    return false;
  }

  ntpNewTime = mktime(&timeinfo);
  yield();

  if (ntpNewTime < 1696969696) {  // Reject timestamps before 2023
    logSerial.printf("❌ Failed!\n[TIME] ⚠️ Invalid NTP Time was Received: %llu\n", ntpNewTime);
    return false;
  }

  rtc.setTime(ntpNewTime);
  yield();
  return true;
}


void setup() {

  Serial.begin(115200);
  delay(100);
  yield();

  // Log Buffer and Redirect
  setupLogger();
  yield();
  
  logSerial.println("[INFO] ⓘ ESP32-S3 Anton Polishchuk (Copyleft) 2025");

  // Run one-time Preferences Settings initialization code
  initializePreferences();
  yield();

  // Startup to Running Config
  startUpConfig();
  yield();
 
  logSerial.println("[INFO] ⓘ Beginning System Start Up Routine!");
  logSerial.println("[INFO] ⓘ Serial communication established @ 115200");
  yield();

  // Starting WiFi & Attempting to Connect
  startWiFi();
  yield();

  // Initializing Hardware Timer 0 for Recovery & Pumping
  setupStateTimer();

  // Syncing Time with NTP
  ntpInitialSync();
  yield();

  now = rtc.getEpoch();
  logSerial.print("[INFO] ⏰ System started on : ");
  logSerial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));

  // Initialize the I2C bus
  Wire.begin(SDA_PIN, SCL_PIN);  // Use 1.5kOhm Pullup resitors across SDA&VCC and SCL&VCC)
  Wire.setClock(100000);         // Set I2C clock speed to 100 kHz (Slow is good, keep cables under 1.5m)
  Wire.setTimeOut(65500);        // I2C Bus 65.5ms Timeout for stability

  // Probing my I2C devices
  Wire.beginTransmission(TOF_SENSOR_ADDRESS);
  logSerial.print("[INFO] ⓘ VL53L0X ToF Sensor (0x29): ");
  logSerial.println(Wire.endTransmission() ? "MISSING" : "FOUND");
  Wire.beginTransmission(TEMP_SENSOR_ADDRESS);
  logSerial.print("[INFO] ⓘ SHT3x Temp Sensor  (0x44): ");
  logSerial.println(Wire.endTransmission() ? "MISSING" : "FOUND");
  Wire.beginTransmission(ADS_SENSOR_ADDRESS);
  logSerial.print("[INFO] ⓘ ADS1115 16bit ADC  (0x48): ");
  logSerial.println(Wire.endTransmission() ? "MISSING" : "FOUND");

  // Set the pin modes
  pinMode(WATER_PUMP_RELAY_PIN, OUTPUT);
  pinMode(EMERGENCY_PUMP_DISCONNECT_PIN, OUTPUT);
  pinMode(WATER_CRITICAL_LEVEL_SENSOR_PIN, INPUT);  // Built in 10kOhm already Pulls-up (Sensor pulls LOW)
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);           // Enable internal pull-up (Button pulls LOW)
  pinMode(ADS_ALERT_PIN, INPUT_PULLUP);             // Configure ALERT/RDY pin
  pinMode(NEOPIXEL_PIN, OUTPUT);                    // Adafruit library sucks big time after v.1.12.4, using Freenove instead

  // Start with both relay pins LOW (Using High-Level Triggered 3.3v Relays)
  digitalWrite(WATER_PUMP_RELAY_PIN, LOW);
  digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, LOW);

  // Test & Exercise Relays, Look at LEDs, Listen to the Clicks and Decide...
  logSerial.println("[INFO] ⓘ Starting Relay Click-Clack test and exercise sequence");
  digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, HIGH);
  delay(369);
  digitalWrite(WATER_PUMP_RELAY_PIN, HIGH);
  delay(369);
  digitalWrite(WATER_PUMP_RELAY_PIN, LOW);
  delay(369);
  digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, LOW);
  logSerial.println("[INFO] ⓘ Relays 1 & 2 Initialization Complete");
  yield();

  // Attach interrupt to trigger on any state change
  attachInterrupt(digitalPinToInterrupt(WATER_CRITICAL_LEVEL_SENSOR_PIN), criticalWaterLevelISR, CHANGE);

  // Attach interrupt to trigger on falling edge
  attachInterrupt(digitalPinToInterrupt(ADS_ALERT_PIN), adsDataReadyISR, FALLING);

  // Kicking Off the Async Web Server
  initWebServer();
  yield();
  yield();

  // Initialize ToF Sensor VL53L0X
  ToF_Sensor.setAddress(TOF_SENSOR_ADDRESS);
  ToF_Sensor.setTimeout(500);
  if (!ToF_Sensor.init()) {
    logSerial.println("[ERROR] ❌ VL53L0X ToF Sensor Initialization Failed!");
  } else {
    // Configure sensor parameters
    ToF_Sensor.setSignalRateLimit(0.25);
    ToF_Sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
    ToF_Sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
    ToF_Sensor.setMeasurementTimingBudget(50000);
    logSerial.println("[INFO] ⓘVL53L0X ToF Sensor Initialized Successfully!");
  }

  // Initialize Temp Sensor SHT31
  if (!sht31.begin(TEMP_SENSOR_ADDRESS)) {
    logSerial.println("[ERROR] ❌ SHT31 Temp Sensor Initialization Failed!");
  } else {
    logSerial.println("[INFO] ⓘ SHT31  Temp Sensor Initialized Successfully!");
  }

  // Initialize ADC Sensor ADS1115
  if (!ads.begin(ADS_SENSOR_ADDRESS)) {
    logSerial.println("[ERROR] ❌ ADS1115 ADC Sensor Initialization Failed!");
  } else {
    ads.setGain(GAIN_TWO);                                       // Set gain to GAIN_TWO to measure up to ±2.048V
    ads.setDataRate(RATE_ADS1115_475SPS);                        // Set 475 samples per second
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, true);  // Start dummy conversion to configure ALERT/RDY functionality
    logSerial.println("[INFO] ⓘ ADS1115 ADC Sensor Initialized Successfully!");
    yield();
  }

  logSerial.println();
  logSerial.println(" ╔═════════════════════════════════════════════════╗ ");
  logSerial.println(" ║ 💧💧💧💧💧💧💧 STARTING MAIN PROGRAM 💧💧💧💧💧💧💧 ║ ");
  logSerial.println(" ╚═════════════════════════════════════════════════╝ ");
  logSerial.println();
  yield();

  // Check initial state of water level sensor
  if (digitalRead(WATER_CRITICAL_LEVEL_SENSOR_PIN) == LOW) {
    criticalWaterLevel = true;
    digitalWrite(WATER_PUMP_RELAY_PIN, LOW);
    digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, HIGH);
    currentState = ERROR_YELLOW;
    logSerial.println("[ERROR] ❌ CRITICAL WATER LEVEL DETECTED ON STARTUP!");
    errorYellowCount++;
    stateStartTime = now;
    stopStateTimer();
    logStateTransition(currentState);
  }

  // Check for stuck relay condition at startup (current flowing when pump should be off)
  if (getPumpCurrent() > pumpOffAmps) {
    currentState = ERROR_RED;
    logSerial.println("[FATAL] 🔴 STUCK RELAY #1 CONDITION DETECTED ON STARTUP, PROGRAM HALTED!");
    errorStuckRelay = true;
    errorRedCount++;
    digitalWrite(WATER_PUMP_RELAY_PIN, LOW);
    digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, HIGH);
    stateStartTime = now;
    logStateTransition(currentState);
    yield();
  }

  // Only set to WELL_RECOVERY if no errors found
  if (currentState == static_cast<PumpState>(-1)) {
    currentState = WELL_RECOVERY;
    strip.setLedColorData(0, 0x008000);
    strip.show();  // Green
    stopStateTimer();
    yield();
    yield();
    stateStartTime = now;
    startStateTimer(wellRecoveryInterval);
    logStateTransition(currentState);
  }

  // Initial Sensor Update
  getSensorsUpdated();

  // Initialize NeoPixel LED
  strip.begin();
  strip.setBrightness(3);  // 0-255 I like them dim
  yield();
}


void loop() {

  // Pereodic NTP Sync
  ntpPeriodicSync();
  yield();

  // Update playback (non-blocking)
  rtttl.play();
  yield();

  // Update WiFi State Machine
  handleWiFi();
  yield();

  static int64_t lastCurrentCheck = 0;
  static int64_t lastLevelCheck = 0;
  static bool criticalMessagePrinted = false;
  
  now = rtc.getEpoch();
  yield();


  switch (currentState) {
    case WELL_RECOVERY:
      strip.setLedColorData(0, 0x008000);
      strip.show();  // Green

      if (criticalWaterLevel == true || currentWaterLevel == 0) {
        currentState = ERROR_YELLOW;
        logSerial.printf("[ERROR] ❌ %s!\n", criticalWaterLevel ? "CRITICAL WATER LEVEL WAS DETECTED" : "TOF SENSOR RANGING FAILURE AFTER 15 ATTEMPTS");
        stateStartTime = now;
        errorYellowCount++;
        stopStateTimer();
        logStateTransition(currentState);
        break;
      }

      if (stateRemainingSeconds.load() == 0) {  // Atomic read
        currentState = STARTING_PUMP;
        stateStartTime = now;
        stopStateTimer();
        logStateTransition(currentState);
      } else {
        getSensorsUpdated();
      }
      break;

    case STARTING_PUMP:

      strip.setLedColorData(0, 0x808080);
      strip.show();  // White
      if (criticalWaterLevel == true || overCurrentProtection == true) {
        if (overCurrentProtection == true) {
          currentState = ERROR_PURPLE;
          logSerial.println("[ERROR] ❌ OVER CURRENT PROTECTION TRIGGERED DURING PUMP STARTUP!");
          stateStartTime = now;
          errorPurpleCount++;
        } else if (criticalWaterLevel == true) {
          currentState = ERROR_YELLOW;
          logSerial.println("[ERROR] ❌ CRITICAL WATER LEVEL DETECTED DURING PUMP STARTUP!");
          stateStartTime = now;
          errorYellowCount++;
        }
        stopStateTimer();
        logStateTransition(currentState);
        break;
      }

      getWaterLevel();
      if (currentWaterLevel <= desiredWaterLevel + hysteresisThreshold) {
        currentState = WELL_RECOVERY;
        stateStartTime = now;
        startStateTimer(wellRecoveryInterval);
        logStateTransition(currentState);
      } else {
        getWaterTemp("pre");
        digitalWrite(WATER_PUMP_RELAY_PIN, HIGH);
        startStateTimer(pumpRuntimeInterval);
        lastPumpStart = now;
        totalPumpCycles++;
        yield();

        if (getPumpCurrent() >= pumpOnAmps) {
          prePumpAmps = currentPumpAmps;
          currentState = PUMPING_WATER;
          stateStartTime = now;
          logStateTransition(currentState);
        } else {
          errorPumpStart++;
          currentState = ERROR_RED;
          logSerial.println("[FATAL] 🔴 CURRENT WASN'T DETECTED AFTER PUMP STARTUP, PROGRAM HALTED!");
          stateStartTime = now;
          errorRedCount++;
          stopStateTimer();
          logStateTransition(currentState);
        }
      }
      break;

    case PUMPING_WATER:

      strip.setLedColorData(0, 0x0080FF);
      strip.show();  // Cyan
      if (criticalWaterLevel == true || currentWaterLevel == 0) {
        currentState = ERROR_YELLOW;
        logSerial.printf("[ERROR] ❌ %s!\n", criticalWaterLevel ? "CRITICAL WATER LEVEL WAS DETECTED" : "TOF SENSOR RANGING FAILURE AFTER 15 ATTEMPTS");
        stateStartTime = now;
        errorYellowCount++;
        stopStateTimer();
        logStateTransition(currentState);
        break;
      }

      // Check current every 600ms (non-blocking)
      if (millis() - lastCurrentCheck >= 600) {
        getPumpCurrent();  // Blocking I/O, but throttled
        yield();
        lastCurrentCheck = millis();

        yield();
        if (currentPumpAmps >= pumpMaxAmps) {
          overCurrentProtection = true;
          currentState = ERROR_PURPLE;
          logSerial.println("[ERROR] ❌ OVERCURRENT CONDITION DETECTED DURING PUMPING, CLEAN FILTER OR CHECK PUMP!");
          logSerial.println("[ALERT] 🚨 ONCE FILTER IS CLEANED PRESS BOOT BUTTON TO RESUME THE PROGRAM!");
          stateStartTime = now;
          errorPurpleCount++;
          stopStateTimer();
          logStateTransition(currentState);
          break;
        }
      }

      if (millis() - lastLevelCheck >= 1000) {
        getWaterLevel();  // Blocking I/O, but throttled
        lastLevelCheck = millis();
        yield();

        if ((currentWaterLevel > 0 && currentWaterLevel <= desiredWaterLevel) || (stateRemainingSeconds.load() == 0)) {  // Atomic read
          currentState = STOPPING_PUMP;
          stateStartTime = now;
          stopStateTimer();
          logStateTransition(currentState);
        }
      }
      break;

    case STOPPING_PUMP:

      strip.setLedColorData(0, 0x808080);
      strip.show();  // White
      getWaterTemp("post");
      yield();
      postPumpAmps = getPumpCurrent();
      digitalWrite(WATER_PUMP_RELAY_PIN, LOW);
      lastPumpStop = now;
      yield();
      deltaPumpAmps = postPumpAmps - prePumpAmps;
      getWaterLevel();
      yield();
      getWaterPercentage();

      if (getPumpCurrent() > pumpOffAmps) {
        currentState = ERROR_RED;
        logSerial.println("[FATAL] 🔴 STUCK RELAY #1 CONDITION DETECTED, CHECK RELAY 1, PROGRAM HALTED!");
        errorStuckRelay = 1;
        errorRedCount++;
        stateStartTime = now;
        stopStateTimer();
        logStateTransition(currentState);
        yield();
      } else {
        currentState = WELL_RECOVERY;
        stateStartTime = now;
        startStateTimer(wellRecoveryInterval);
        logStateTransition(currentState);
        yield();
      }
      break;

    case ERROR_RED:
      strip.setLedColorData(0, 0x800000);
      strip.show();  // Red
      digitalWrite(WATER_PUMP_RELAY_PIN, LOW);

      if (errorStuckRelay == true) {
        digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, HIGH);
      } else if (errorPumpStart == 1) {
        if (criticalWaterLevel) {
          digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, HIGH);
          if (!criticalMessagePrinted) {
            logSerial.printf("[ERROR] ❌ %s CRITICAL WATER LEVEL DETECTED IN ERROR_RED STATE!\n", rtc.getTime("%d %b %H:%M:%S").c_str());
            criticalMessagePrinted = true;
          }
        } else {
          digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, LOW);
          criticalMessagePrinted = false;
        }
      }
      getSensorsUpdated();
      break;

    case ERROR_PURPLE:
      strip.setLedColorData(0, 0x8000FF);
      strip.show();  // Purple
      digitalWrite(WATER_PUMP_RELAY_PIN, LOW);

      if (!criticalWaterLevel) {
        digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, LOW);
      }
      if (criticalWaterLevel) {
        if (!criticalMessagePrinted) {
          logSerial.printf("[ERROR] ❌ %s CRITICAL WATER LEVEL DETECTED IN ERROR_PURPLE STATE!\n", rtc.getTime("%d %b %H:%M:%S").c_str());
          criticalMessagePrinted = true;
        }
        digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, HIGH);
      } else {
        criticalMessagePrinted = false;
        digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, LOW);
      }
      if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        overCurrentProtection = false;
        currentState = WELL_RECOVERY;
        logSerial.printf("[NOTICE] 💡 %s BOOT BUTTON WAS PRESSED, RESUMING NORMAL OPERATION!\n", rtc.getTime("%d %b %H:%M:%S").c_str());
        errorLastRecovery = now;
        stateStartTime = now;
        startStateTimer(wellRecoveryInterval);
        logStateTransition(currentState);
      }
      getSensorsUpdated();
      break;

    case ERROR_YELLOW:
      strip.setLedColorData(0, 0x808000);
      strip.show();  // Yellow
      digitalWrite(WATER_PUMP_RELAY_PIN, LOW);
      lastLevelCheck = 0;

      if (criticalWaterLevel) {
        if (!criticalMessagePrinted) {
          logSerial.printf("[ERROR] ❌ %s CRITICAL WATER LEVEL DETECTED IN ERROR_YELLOW STATE!\n", rtc.getTime("%d %b %H:%M:%S").c_str());
          criticalMessagePrinted = true;
        }
        digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, HIGH);
      } else {
        criticalMessagePrinted = false;
        digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, LOW);
      }

      if (currentWaterLevel == 0 && millis() - lastLevelCheck >= 3000) {
        getWaterLevel();  // Retry sensor read (throttled to every 3s)
        yield();
        lastLevelCheck = millis();
      }
      if (!criticalWaterLevel && currentWaterLevel > 0) {
        digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, LOW);
        currentState = WELL_RECOVERY;
        logSerial.printf("[NOTICE] 💡 %s WATER LEVEL IS NORMAL, RESUMING NORMAL OPERATION!\n", rtc.getTime("%d %b %H:%M:%S").c_str());
        errorLastRecovery = now;
        stateStartTime = now;
        startStateTimer(wellRecoveryInterval);
        logStateTransition(currentState);
      }
      getSensorsUpdated();
      break;

    case HALTED_PUMP:
      if (criticalWaterLevel) {
        if (!criticalMessagePrinted) {
          logSerial.printf("[ERROR] ❌ %s CRITICAL WATER LEVEL DETECTED IN HALTED_PUMP STATE!\n", rtc.getTime("%d %b %H:%M:%S").c_str());
          criticalMessagePrinted = true;
        }
        digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, HIGH);
      } else {
        criticalMessagePrinted = false;
        digitalWrite(EMERGENCY_PUMP_DISCONNECT_PIN, LOW);
      }
      if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        pumpIsHalted = false;
        currentState = WELL_RECOVERY;
        logSerial.printf("[NOTICE] 💡 %s BOOT BUTTON WAS PRESSED, RESUMING NORMAL OPERATION!\n", rtc.getTime("%d %b %H:%M:%S").c_str());
        stateStartTime = now;
        startStateTimer(wellRecoveryInterval);
        logStateTransition(currentState);
      }
      getSensorsUpdated();
      break;
  }
}