/**
 * ESP32 Bewässerungssteuerung (8 Kanäle) — VOLLVERSION
 * - Manuell + Gruppen-Pläne (sequentielle Schritte), Queue, Winter-Entwässerung
 * - WLAN STA + AP-Konfig (Captive-Portal, QR), Hostname/mDNS
 * - Protokollierung: LittleFS (lokal CSV) + optional MQTT mit Retry-Queue
 * - DS3231 RTC als Backup-Zeit, NTP-Sync
 * - Web-UI: Seiten Start/Pläne/Konfig/Winter/Logs, dunkles Theme, Auto-Refresh
 * - WiFi-Signalqualität (%) in [Klammern] hinter IP
 * - OLED Helligkeit-Management (nach 10 Min Inaktivität auf 30%)
 * - Optional: Externer Taster für Helligkeit (GPIO 34, aktivierbar per #define)
 * - Verbessertes WiFi-Reconnect: 10 Sekunden Interval
 * - Queue: Manuelle Starts werden gequeued wenn Kanal aktiv (SINGLE_CHANNEL_MODE)
 * 
 * Frameworks: Arduino-ESP32
 * Libraries:  Adafruit_GFX, Adafruit_SSD1306, QRCode (ricmoo), PubSubClient,
 *             RTClib (optional DS3231), LittleFS
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <qrcode.h>
#include <deque>
#include <LittleFS.h>
#include <PubSubClient.h>
#include "RTClib.h"

// ============================================================
// =================== KONFIGURATION ==========================
// ============================================================

static const uint8_t  CHANNEL_PINS[8]   = { 13, 14, 25, 26, 16, 2, 15, 3 };
static const bool     RELAY_ACTIVE_LOW_DEFAULT = true;

#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C
static const uint8_t  PIN_SDA = 5;
static const uint8_t  PIN_SCL = 4;

static const uint8_t  PIN_AP_BUTTON     = 0;
static const uint32_t AP_BUTTON_HOLD_MS = 3000;

// ============================================================
// ================== EXTERNE TASTENKONFIGURATION ============
// ============================================================

// EXTERN_BRIGHTNESS_BUTTON aktivieren: 1 = ja, 0 = nein
#define EXTERN_BRIGHTNESS_BUTTON 0

#if EXTERN_BRIGHTNESS_BUTTON
  static const uint8_t PIN_BRIGHTNESS_BUTTON = 34;
#endif

static const char* WIFI_SSID = "YourSSID";
static const char* WIFI_PASS = "YourPassword";

static const char* AP_SSID = "Irrigation-AP";
static const char* AP_PASS = "irrigate123";
static const bool  AP_CONFIG_REQUIRE_AUTH = false;

static const char* WEB_USER  = "admin";
static const char* WEB_PASS  = "irrigation";

static const char* DEVICE_HOSTNAME = "IrrigationController";

static const char* NTP_SERVER          = "pool.ntp.org";
static const long  GMT_OFFSET_SEC      = 3600;
static const int   DAYLIGHT_OFFSET_SEC = 3600;

static const uint16_t DEFAULT_RUNTIME_SEC = 1800;
static const uint16_t MAX_RUNTIME_SEC     = 7200;
static const uint16_t MIN_GAP_SEC         = 5;
static const bool     SINGLE_CHANNEL_MODE = true;

static const uint8_t  MAX_GROUPS     = 8;
static const uint8_t  MAX_STEPS      = 8;
static const uint8_t  CH_NAME_MAX    = 20;
static const uint8_t  GROUP_NAME_MAX = 16;

static const uint8_t  MAX_API_TOKENS = 5;
static const uint8_t  API_TOKEN_LEN  = 32;

static const uint8_t  SSID_MIN_LEN = 1,  SSID_MAX_LEN = 32;
static const uint8_t  PASS_MIN_LEN = 8,  PASS_MAX_LEN = 63;

static const uint16_t WINTER_BLOW_SEC       = 90;
static const uint16_t WINTER_ZONE_PAUSE_SEC = 120;
static const uint8_t  WINTER_PASSES         = 2;
static const uint16_t WINTER_MAX_RUN_SEC    = 600;
static const uint16_t WINTER_COOLDOWN_SEC   = 600;

static const uint32_t WIFI_RECONNECT_RETRY_MS = 10000;
static const uint32_t DISPLAY_REFRESH_MS      = 1000;
static const uint32_t SCHEDULE_CHECK_MS       = 1000;
static const uint32_t RTC_SYNC_INTERVAL_MS    = 3600000;

static const uint16_t RUNTIME_OPTIONS_MIN[] = { 5, 10, 15, 30, 45, 60, 90, 120 };
static const uint8_t  RUNTIME_OPTIONS_CNT   = 8;
static const uint16_t RUNTIME_DEFAULT_MIN   = 30;

// --- MQTT ---
static const uint8_t  MQTT_HOST_MAX = 64;
static const uint8_t  MQTT_USER_MAX = 32;
static const uint8_t  MQTT_PASS_MAX = 64;
static const uint16_t MQTT_PORT_DEFAULT = 1883;
static const uint8_t  MAX_MQTT_RETRY_QUEUE = 100;

// --- Logging ---
#define LOG_FILE "/logs.csv"
static const uint32_t LOG_MAX_SIZE_BYTES = 1048576;

// --- Display Brightness Management ---
static uint32_t lastButtonPress = 0;
static const uint32_t BRIGHTNESS_TIMEOUT_MS = 600000;
static const uint8_t BRIGHTNESS_NORMAL = 255;
static const uint8_t BRIGHTNESS_LOW = 50;
static uint8_t currentBrightness = BRIGHTNESS_NORMAL;

// --- Theme ---
static const char* TH_BG     = "#0d1117";
static const char* TH_PANEL  = "#161b22";
static const char* TH_PANEL2 = "#1c2330";
static const char* TH_TEXT   = "#e6edf3";
static const char* TH_MUTED  = "#8b949e";
static const char* TH_ACCENT = "#3fb950";
static const char* TH_DANGER = "#f85149";
static const char* TH_BORDER = "#30363d";

// ============================================================
// ==================== GLOBALS ===============================
// ============================================================

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
WebServer        server(80);
DNSServer        dnsServer;
Preferences      prefs;
WiFiClient       wifiClient;
PubSubClient     mqttClient(wifiClient);
RTC_DS3231       rtc;

static const byte DNS_PORT = 53;

enum WifiOpMode { OP_STA, OP_AP };
WifiOpMode currentMode = OP_STA;

char staSsid[SSID_MAX_LEN + 1] = {0};
char staPass[PASS_MAX_LEN + 1] = {0};
char channelNames[8][CH_NAME_MAX + 1] = {{0}};
char apiTokens[MAX_API_TOKENS][API_TOKEN_LEN + 1] = {{0}};
bool relayActiveLow = RELAY_ACTIVE_LOW_DEFAULT;

bool mqttEnabled = false;
char mqttHost[MQTT_HOST_MAX + 1] = {0};
uint16_t mqttPort = MQTT_PORT_DEFAULT;
char mqttUser[MQTT_USER_MAX + 1] = {0};
char mqttPass[MQTT_PASS_MAX + 1] = {0};
char mqttTopic[64] = "irrigation";

struct MqttRetryEvent {
  uint32_t timestamp;
  char event[256];
};
std::deque<MqttRetryEvent> mqttRetryQueue;

bool rtcPresent = false;
uint32_t lastRtcSync = 0;

static uint32_t lastWifiReconnectAttempt = 0;

struct ChannelState { bool active; uint32_t startMs; uint32_t durationMs; uint32_t lastStopMs; };
ChannelState channels[8];

struct WateringTask { uint8_t channel; uint16_t durationSec; };
std::deque<WateringTask> taskQueue;

struct GroupStep { uint8_t channel; uint16_t durationSec; };

struct Group {
  bool      enabled;
  char      name[GROUP_NAME_MAX + 1];
  uint8_t   hour;
  uint8_t   minute;
  uint8_t   daysMask;
  uint8_t   stepCount;
  GroupStep steps[MAX_STEPS];
  uint32_t  lastRunDay;
  uint16_t  lastRunMin;
};
Group groups[MAX_GROUPS];

enum WinterPhase { WP_BLOW, WP_PAUSE, WP_COOLDOWN };
bool        winterActive     = false;
WinterPhase winterPhase      = WP_BLOW;
uint8_t     winterChannel    = 0;
uint8_t     winterPass       = 0;
uint16_t    winterCumRunSec  = 0;
uint32_t    winterPhaseStart = 0;

uint32_t buttonPressedSince = 0;
uint32_t lastDisplayUpdate  = 0;
uint32_t lastScheduleCheck  = 0;
uint32_t lastMqttRetry      = 0;
bool     ntpSynced          = false;
bool     rtcSynced          = false;

void stopAllChannels();
void clearQueue();
bool checkWebAuthOnly();
String buildApConfigHtml(const String& msg);
void loadWifiCreds();
void saveChannelName(uint8_t i, const String& name);
void logEvent(uint8_t channel, const char* action, uint16_t durationSec, const char* source);

// ============================================================
// =================== HELPER =================================
// ============================================================

String channelName(uint8_t ch) {
  if (ch < 8 && channelNames[ch][0] != '\0') return String(channelNames[ch]);
  return "Kanal " + String(ch + 1);
}

String channelShort(uint8_t ch) {
  String n = channelName(ch);
  if (n.length() > 8) n = n.substring(0, 8);
  return n;
}

String htmlEscape(const String& in) {
  String o; o.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&': o += "&amp;";  break;
      case '<': o += "&lt;";   break;
      case '>': o += "&gt;";   break;
      case '"': o += "&quot;"; break;
      case '\'':o += "&#39;";  break;
      default:  o += c;
    }
  }
  return o;
}

String sanitizeField(const String& in, uint8_t maxLen) {
  String o; o.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '|' || c == '\n' || c == '\r' || c == ',') continue;
    o += c;
  }
  if (o.length() > maxLen) o = o.substring(0, maxLen);
  return o;
}

String daysMaskToStr(uint8_t mask) {
  static const char* lbl[7] = { "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa" };
  String s;
  for (int d = 0; d < 7; d++)
    if (mask & (1 << d)) { if (s.length()) s += " "; s += lbl[d]; }
  if (!s.length()) s = "(keine)";
  return s;
}

String groupLabel(uint8_t i) {
  if (groups[i].name[0]) return String(groups[i].name);
  return "Gruppe " + String(i + 1);
}

// ============================================================
// ================= RTC (DS3231) =============================
// ============================================================

void initRTC() {
  if (!rtc.begin()) {
    Serial.println("[RTC] DS3231 nicht gefunden");
    rtcPresent = false;
    return;
  }
  if (rtc.lostPower()) {
    Serial.println("[RTC] Batterieverlust erkannt, Zeit ungültig");
  }
  rtcPresent = true;
  Serial.println("[RTC] DS3231 OK");
}

void syncRtcFromNtp() {
  if (!rtcPresent || !ntpSynced) return;
  if (millis() - lastRtcSync < RTC_SYNC_INTERVAL_MS) return;
  lastRtcSync = millis();
  struct tm ti;
  if (!getLocalTime(&ti, 50)) return;
  rtc.adjust(DateTime(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                      ti.tm_hour, ti.tm_min, ti.tm_sec));
  Serial.println("[RTC] synchronisiert mit NTP");
  rtcSynced = true;
}

String getRtcTimeStr() {
  if (!rtcPresent) return "--";
  DateTime now = rtc.now();
  char buf[20];
  snprintf(buf, sizeof(buf), "%02d.%02d. %02d:%02d:%02d",
           now.day(), now.month(), now.hour(), now.minute(), now.second());
  return String(buf);
}

struct tm getRtcLocalTime() {
  struct tm t = {0};
  if (rtcPresent) {
    DateTime now = rtc.now();
    t.tm_year = now.year() - 1900;
    t.tm_mon  = now.month() - 1;
    t.tm_mday = now.day();
    t.tm_hour = now.hour();
    t.tm_min  = now.minute();
    t.tm_sec  = now.second();
    t.tm_wday = now.dayOfTheWeek();
  }
  return t;
}

// ============================================================
// ==================== ZEIT-HELPERS ==========================
// ============================================================

String nowTimeStr() {
  if (ntpSynced) {
    struct tm ti;
    if (getLocalTime(&ti, 50)) {
      char buf[20];
      strftime(buf, sizeof(buf), "%d.%m. %H:%M:%S", &ti);
      return String(buf);
    }
  }
  if (rtcPresent) return getRtcTimeStr();
  return "--";
}

struct tm getLocalTimeWithFallback() {
  struct tm ti;
  if (getLocalTime(&ti, 50)) return ti;
  if (rtcPresent) return getRtcLocalTime();
  memset(&ti, 0, sizeof(ti));
  return ti;
}

// ============================================================
// ================= WIFI-QUALITÄT ============================
// ============================================================

int8_t getWifiSignalQuality() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  int rssi = WiFi.RSSI();
  int quality = 2 * (rssi + 100);
  if (quality > 100) quality = 100;
  if (quality < 0) quality = 0;
  return quality;
}

String buildWifiStatusWithQuality() {
  if (WiFi.status() != WL_CONNECTED) {
    return "WiFi: offline";
  }
  int quality = getWifiSignalQuality();
  return String(WiFi.localIP().toString()) + " [" + String(quality) + "%]";
}

// ============================================================
// ================= HELLIGKEIT-VERWALTUNG ===================
// ============================================================

void manageBrightness() {
  uint32_t inactiveSince = millis() - lastButtonPress;
  
  if (inactiveSince > BRIGHTNESS_TIMEOUT_MS) {
    if (currentBrightness != BRIGHTNESS_LOW) {
      currentBrightness = BRIGHTNESS_LOW;
      display.ssd1306_command(0x81);
      display.ssd1306_command(BRIGHTNESS_LOW);
      Serial.println("[OLED] Helligkeit: 30% (Inaktivität)");
    }
  } else {
    if (currentBrightness != BRIGHTNESS_NORMAL) {
      currentBrightness = BRIGHTNESS_NORMAL;
      display.ssd1306_command(0x81);
      display.ssd1306_command(BRIGHTNESS_NORMAL);
      Serial.println("[OLED] Helligkeit: 100% (aktiv)");
    }
  }
}

// ============================================================
// ================ EXTERNER HELLIGKEIT-TASTER ===============
// ============================================================

#if EXTERN_BRIGHTNESS_BUTTON
static uint32_t lastExternalButtonPress = 0;
static const uint32_t EXTERNAL_BUTTON_DEBOUNCE_MS = 50;

void handleExternalBrightnessButton() {
  bool pressed = (digitalRead(PIN_BRIGHTNESS_BUTTON) == LOW);
  
  if (pressed) {
    if (millis() - lastExternalButtonPress > EXTERNAL_BUTTON_DEBOUNCE_MS) {
      lastExternalButtonPress = millis();
      lastButtonPress = millis();
      Serial.println("[BTN] Externer Helligkeit-Taster");
    }
  }
}
#endif

// ============================================================
// ================= LOGGING (LittleFS + MQTT) ===============
// ============================================================

void initLogging() {
  if (!LittleFS.begin()) {
    Serial.println("[LOG] LittleFS mount failed");
    return;
  }
  Serial.println("[LOG] LittleFS OK");
  
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) {
    f = LittleFS.open(LOG_FILE, "w");
    if (f) {
      f.println("timestamp,channel,name,action,durationSec,source");
      f.close();
      Serial.println("[LOG] Neue Log-Datei erstellt");
    }
  } else {
    f.close();
  }
}

void logEvent(uint8_t channel, const char* action, uint16_t durationSec, const char* source) {
  struct tm ti = getLocalTimeWithFallback();
  char timestamp[32];
  if (ti.tm_year > 0) {
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
             ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    strcpy(timestamp, "YYYY-MM-DD HH:MM:SS");
  }
  String nm = channelName(channel);
  String line = String(timestamp) + "," + String(channel + 1) + "," + nm + "," +
                String(action) + "," + String(durationSec) + "," + String(source) + "\n";

  File f = LittleFS.open(LOG_FILE, "a");
  if (f) {
    size_t written = f.print(line);
    f.close();
    Serial.printf("[LOG] Event geschrieben (%u Bytes): %s\n", written, action);
  } else {
    Serial.printf("[LOG] FEHLER: Datei konnte nicht geöffnet werden\n");
  }
  f = LittleFS.open(LOG_FILE, "r");
  if (f && f.size() > LOG_MAX_SIZE_BYTES) {
    f.close();
    Serial.printf("[LOG] Datei > %lu Bytes, könnte gekürzt werden\n", LOG_MAX_SIZE_BYTES);
  } else if (f) f.close();

  if (mqttEnabled && mqttClient.connected()) {
    String payload = "{\"ts\":\"" + String(timestamp) + "\",\"ch\":" + String(channel + 1) +
                     ",\"name\":\"" + nm + "\",\"action\":\"" + String(action) +
                     "\",\"sec\":" + String(durationSec) + ",\"src\":\"" + String(source) + "\"}";
    if (!mqttClient.publish(mqttTopic, payload.c_str())) {
      if (mqttRetryQueue.size() < MAX_MQTT_RETRY_QUEUE) {
        mqttRetryQueue.push_back({(uint32_t)millis(), {}});
        strncpy(mqttRetryQueue.back().event, payload.c_str(), sizeof(mqttRetryQueue.back().event) - 1);
        Serial.printf("[MQTT] Event in Retry-Queue (%u)\n", (unsigned)mqttRetryQueue.size());
      }
    } else {
      Serial.printf("[MQTT] Event gesendet: %s\n", action);
    }
  } else if (mqttEnabled && !mqttClient.connected()) {
    if (mqttRetryQueue.size() < MAX_MQTT_RETRY_QUEUE) {
      String payload = "{\"ts\":\"" + String(timestamp) + "\",\"ch\":" + String(channel + 1) +
                       ",\"name\":\"" + nm + "\",\"action\":\"" + String(action) +
                       "\",\"sec\":" + String(durationSec) + ",\"src\":\"" + String(source) + "\"}";
      mqttRetryQueue.push_back({(uint32_t)millis(), {}});
      strncpy(mqttRetryQueue.back().event, payload.c_str(), sizeof(mqttRetryQueue.back().event) - 1);
    }
  }
}

// ============================================================
// ==================== MQTT-VERWALTUNG =======================
// ============================================================

void loadMqttConfig() {
  prefs.begin("mqtt", true);
  mqttEnabled = prefs.getBool("en", false);
  String h = prefs.getString("host", "");
  mqttPort = prefs.getUShort("port", MQTT_PORT_DEFAULT);
  String u = prefs.getString("user", "");
  String p = prefs.getString("pass", "");
  prefs.end();
  strncpy(mqttHost, h.c_str(), MQTT_HOST_MAX);
  strncpy(mqttUser, u.c_str(), MQTT_USER_MAX);
  strncpy(mqttPass, p.c_str(), MQTT_PASS_MAX);
  Serial.printf("[MQTT] enabled=%d host=%s port=%u\n", mqttEnabled, mqttHost, mqttPort);
}

void saveMqttConfig(bool en, const String& host, uint16_t port, const String& user, const String& pass) {
  prefs.begin("mqtt", false);
  prefs.putBool("en", en);
  prefs.putString("host", host);
  prefs.putUShort("port", port);
  prefs.putString("user", user);
  prefs.putString("pass", pass);
  prefs.end();
  loadMqttConfig();
}

void mqttConnect() {
  if (!mqttEnabled || !WiFi.isConnected() || mqttClient.connected()) return;
  if (!mqttHost[0]) { Serial.println("[MQTT] kein Host konfiguriert"); return; }
  mqttClient.setServer(mqttHost, mqttPort);
  Serial.printf("[MQTT] Verbinde zu %s:%u...\n", mqttHost, mqttPort);
  if (mqttUser[0] && mqttPass[0]) {
    if (mqttClient.connect(DEVICE_HOSTNAME, mqttUser, mqttPass)) {
      Serial.println("[MQTT] verbunden (mit Auth)");
      while (!mqttRetryQueue.empty()) {
        if (mqttClient.publish(mqttTopic, mqttRetryQueue.front().event))
          Serial.println("[MQTT] Retry-Event gesendet");
        else break;
        mqttRetryQueue.pop_front();
      }
    } else {
      Serial.printf("[MQTT] Connect fehlgeschlagen (rc=%d)\n", mqttClient.state());
    }
  } else {
    if (mqttClient.connect(DEVICE_HOSTNAME)) {
      Serial.println("[MQTT] verbunden (ohne Auth)");
      while (!mqttRetryQueue.empty()) {
        if (mqttClient.publish(mqttTopic, mqttRetryQueue.front().event))
          Serial.println("[MQTT] Retry-Event gesendet");
        else break;
        mqttRetryQueue.pop_front();
      }
    }
  }
}

void maintainMqtt() {
  if (!mqttEnabled) return;
  if (millis() - lastMqttRetry < 10000) return;
  lastMqttRetry = millis();
  if (!mqttClient.connected() && WiFi.isConnected()) mqttConnect();
  if (mqttClient.connected()) mqttClient.loop();
}

// ============================================================
// ================= API-TOKEN-VERWALTUNG =====================
// ============================================================

void loadApiTokens() {
  prefs.begin("tokens", true);
  for (uint8_t i = 0; i < MAX_API_TOKENS; i++) {
    String v = prefs.getString(("t"+String(i)).c_str(), "");
    strncpy(apiTokens[i], v.c_str(), API_TOKEN_LEN);
    apiTokens[i][API_TOKEN_LEN] = '\0';
  }
  prefs.end();
}

void persistApiTokens() {
  prefs.begin("tokens", false);
  for (uint8_t i = 0; i < MAX_API_TOKENS; i++)
    prefs.putString(("t"+String(i)).c_str(), apiTokens[i]);
  prefs.end();
}

int firstFreeTokenSlot() {
  for (uint8_t i = 0; i < MAX_API_TOKENS; i++) if (!apiTokens[i][0]) return i;
  return -1;
}

String genToken() {
  static const char hex[] = "0123456789abcdef";
  String t; t.reserve(API_TOKEN_LEN);
  for (uint8_t i = 0; i < API_TOKEN_LEN; i++) t += hex[esp_random() & 0x0F];
  return t;
}

bool tokenValid(const String& t) {
  if (t.length() != API_TOKEN_LEN) return false;
  for (uint8_t i = 0; i < MAX_API_TOKENS; i++)
    if (apiTokens[i][0] && t == apiTokens[i]) return true;
  return false;
}

// ============================================================
// ==================== KANAL-STEUERUNG =======================
// ============================================================

static inline void writeRelay(uint8_t ch, bool on) {
  bool level = relayActiveLow ? !on : on;
  digitalWrite(CHANNEL_PINS[ch], level ? HIGH : LOW);
}

bool anyChannelActive() {
  for (uint8_t i = 0; i < 8; i++) if (channels[i].active) return true;
  return false;
}

void stopChannel(uint8_t ch) {
  if (ch >= 8) return;
  writeRelay(ch, false);
  if (channels[ch].active) {
    channels[ch].lastStopMs = millis();
    uint32_t elapsed = (millis() - channels[ch].startMs) / 1000;
    logEvent(ch, "STOP", (uint16_t)elapsed, "manual");
    Serial.printf("[%s] STOP (%lus)\n", channelName(ch).c_str(), (unsigned long)elapsed);
  }
  channels[ch].active = false;
}

void stopAllChannels() { 
  for (uint8_t i = 0; i < 8; i++) stopChannel(i); 
}

bool startChannel(uint8_t ch, uint16_t durationSec) {
  if (winterActive) return false;
  if (ch >= 8) return false;
  if (durationSec == 0 || durationSec > MAX_RUNTIME_SEC) return false;
  if (channels[ch].lastStopMs &&
      (millis() - channels[ch].lastStopMs) < (uint32_t)MIN_GAP_SEC * 1000UL) return false;
  if (SINGLE_CHANNEL_MODE)
    for (uint8_t i = 0; i < 8; i++)
      if (i != ch && channels[i].active) stopChannel(i);
  writeRelay(ch, true);
  channels[ch].active     = true;
  channels[ch].startMs    = millis();
  channels[ch].durationMs = (uint32_t)durationSec * 1000UL;
  logEvent(ch, "START", durationSec, "manual");
  Serial.printf("[%s] START für %u s\n", channelName(ch).c_str(), durationSec);
  return true;
}

// ============================================================
// ========================= QUEUE ============================
// ============================================================

void enqueueTask(uint8_t ch, uint16_t durationSec, const char* source = "plan") {
  if (winterActive) return;
  if (ch >= 8) return;
  if (durationSec == 0 || durationSec > MAX_RUNTIME_SEC) return;
  taskQueue.push_back({ ch, durationSec });
  logEvent(ch, "QUEUE", durationSec, source);
  Serial.printf("[QUEUE] +%s %us (size=%u)\n",
                channelName(ch).c_str(), durationSec, (unsigned)taskQueue.size());
}

void clearQueue() {
  while (!taskQueue.empty()) taskQueue.pop_front();
  Serial.println("[QUEUE] geleert");
}

void processQueue() {
  for (uint8_t i = 0; i < 8; i++)
    if (channels[i].active &&
        (millis() - channels[i].startMs) >= channels[i].durationMs)
      stopChannel(i);
  if (SINGLE_CHANNEL_MODE && anyChannelActive()) return;
  if (taskQueue.empty()) return;
  WateringTask t = taskQueue.front();
  if (channels[t.channel].lastStopMs &&
      (millis() - channels[t.channel].lastStopMs) < (uint32_t)MIN_GAP_SEC * 1000UL) return;
  taskQueue.pop_front();
  startChannel(t.channel, t.durationSec);
}

// ============================================================
// ================ WINTER-ENTWÄSSERUNG =======================
// ============================================================

void winterBeginBlow() {
  writeRelay(winterChannel, true);
  channels[winterChannel].active     = true;
  channels[winterChannel].startMs    = millis();
  channels[winterChannel].durationMs = (uint32_t)WINTER_BLOW_SEC * 1000UL;
  winterPhase = WP_BLOW; winterPhaseStart = millis();
  logEvent(winterChannel, "BLOW", WINTER_BLOW_SEC, "winter");
  Serial.printf("[WINTER] Blow %s (%u/%u)\n", channelName(winterChannel).c_str(), winterPass, WINTER_PASSES);
}

void startWinterization() {
  if (winterActive) return;
  stopAllChannels(); clearQueue();
  winterActive = true; winterPass = 1; winterChannel = 0; winterCumRunSec = 0;
  logEvent(0, "WINTER_START", 0, "system");
  winterBeginBlow();
}

void stopWinterization() { 
  if (!winterActive) return; 
  winterActive = false; 
  stopAllChannels(); 
  logEvent(0, "WINTER_STOP", 0, "system"); 
}

void winterAdvanceAfterBlow() {
  stopChannel(winterChannel);
  winterCumRunSec += WINTER_BLOW_SEC;
  winterChannel++;
  if (winterChannel >= 8) {
    winterChannel = 0; winterPass++;
    if (winterPass > WINTER_PASSES) { stopWinterization(); return; }
  }
  if (winterCumRunSec >= WINTER_MAX_RUN_SEC) { winterPhase = WP_COOLDOWN; winterPhaseStart = millis(); }
  else                                       { winterPhase = WP_PAUSE;    winterPhaseStart = millis(); }
}

void processWinterization() {
  if (!winterActive) return;
  uint32_t el = (millis() - winterPhaseStart) / 1000;
  switch (winterPhase) {
    case WP_BLOW:     if (el >= WINTER_BLOW_SEC)       winterAdvanceAfterBlow(); break;
    case WP_PAUSE:    if (el >= WINTER_ZONE_PAUSE_SEC) winterBeginBlow();        break;
    case WP_COOLDOWN: if (el >= WINTER_COOLDOWN_SEC){ winterCumRunSec=0; winterBeginBlow(); } break;
  }
}

uint32_t winterPhaseRemainingSec() {
  uint32_t el = (millis() - winterPhaseStart) / 1000;
  uint32_t total = (winterPhase == WP_BLOW)?WINTER_BLOW_SEC:(winterPhase==WP_PAUSE)?WINTER_ZONE_PAUSE_SEC:WINTER_COOLDOWN_SEC;
  return (el >= total) ? 0 : (total - el);
}

// ============================================================
// ============== PERSISTENZ ==================================
// ============================================================

void saveGroups() {
  prefs.begin("irrig", false);
  prefs.putBytes("groups", groups, sizeof(groups));
  prefs.end();
}

void loadGroups() {
  prefs.begin("irrig", true);
  size_t sz = prefs.getBytesLength("groups");
  if (sz == sizeof(groups)) prefs.getBytes("groups", groups, sizeof(groups));
  else                       memset(groups, 0, sizeof(groups));
  prefs.end();
  for (uint8_t i = 0; i < MAX_GROUPS; i++) {
    groups[i].lastRunDay = 0;
    groups[i].lastRunMin = 0xFFFF;
    groups[i].name[GROUP_NAME_MAX] = '\0';
    if (groups[i].stepCount > MAX_STEPS) groups[i].stepCount = 0;
  }
}

void loadWifiCreds() {
  prefs.begin("wifi", true);
  String s = prefs.getString("ssid", ""); 
  String p = prefs.getString("pass", "");
  prefs.end();
  if (s.length() >= SSID_MIN_LEN && s.length() <= SSID_MAX_LEN) strncpy(staSsid, s.c_str(), sizeof(staSsid)-1);
  else strncpy(staSsid, WIFI_SSID, sizeof(staSsid)-1);
  staSsid[sizeof(staSsid)-1] = '\0';
  if (p.length() >= PASS_MIN_LEN && p.length() <= PASS_MAX_LEN) strncpy(staPass, p.c_str(), sizeof(staPass)-1);
  else strncpy(staPass, WIFI_PASS, sizeof(staPass)-1);
  staPass[sizeof(staPass)-1] = '\0';
  Serial.printf("[WiFi] STA-SSID: '%s'\n", staSsid);
}

void saveWifiCreds(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid); 
  prefs.putString("pass", pass);
  prefs.end();
}

void loadChannelNames() {
  prefs.begin("names", true);
  for (uint8_t i = 0; i < 8; i++) {
    String v = prefs.getString(("ch"+String(i)).c_str(), "");
    strncpy(channelNames[i], v.c_str(), CH_NAME_MAX);
    channelNames[i][CH_NAME_MAX] = '\0';
  }
  prefs.end();
}

void saveChannelName(uint8_t i, const String& name) {
  if (i >= 8) return;
  String v = sanitizeField(name, CH_NAME_MAX);
  prefs.begin("names", false);
  prefs.putString(("ch"+String(i)).c_str(), v);
  prefs.end();
  strncpy(channelNames[i], v.c_str(), CH_NAME_MAX);
  channelNames[i][CH_NAME_MAX] = '\0';
}

void loadRelayPolarity() {
  prefs.begin("relay", true);
  relayActiveLow = prefs.getBool("activeLow", RELAY_ACTIVE_LOW_DEFAULT);
  prefs.end();
  Serial.printf("[RELAY] activeLow=%d\n", relayActiveLow ? 1 : 0);
}

void saveRelayPolarity(bool activeLow) {
  prefs.begin("relay", false);
  prefs.putBool("activeLow", activeLow);
  prefs.end();
  relayActiveLow = activeLow;
}

// ============================================================
// ====================== GRUPPEN-SCHEDULER ===================
// ============================================================

void checkGroups() {
  if (!ntpSynced && !rtcSynced) return;
  if (millis() - lastScheduleCheck < SCHEDULE_CHECK_MS) return;
  lastScheduleCheck = millis();
  struct tm ti = getLocalTimeWithFallback();
  if (ti.tm_year <= 0) return;
  uint8_t  dayBit = 1 << ti.tm_wday;
  uint32_t today  = (ti.tm_year+1900)*10000UL + (ti.tm_mon+1)*100UL + ti.tm_mday;
  uint16_t nowMin = ti.tm_hour*60 + ti.tm_min;
  for (uint8_t g = 0; g < MAX_GROUPS; g++) {
    Group &G = groups[g];
    if (!G.enabled || G.stepCount == 0) continue;
    if (!(G.daysMask & dayBit)) continue;
    if (G.hour != ti.tm_hour || G.minute != ti.tm_min) continue;
    if (G.lastRunDay == today && G.lastRunMin == nowMin) continue;
    for (uint8_t k = 0; k < G.stepCount; k++)
      enqueueTask(G.steps[k].channel, G.steps[k].durationSec, "plan");
    G.lastRunDay = today; G.lastRunMin = nowMin;
    Serial.printf("[GROUP %u] '%s' getriggert (%u Schritte)\n", g, groupLabel(g).c_str(), G.stepCount);
  }
}

// ============================================================
// =================== EXPORT / IMPORT ========================
// ============================================================

String buildExport() {
  String s = "# IrrigationController config v3 (ohne WLAN-Passwort)\n";
  s += "WIFI|" + String(staSsid) + "\n";
  for (int i = 0; i < 8; i++)
    if (channelNames[i][0]) s += "NAME|" + String(i) + "|" + String(channelNames[i]) + "\n";
  for (int g = 0; g < MAX_GROUPS; g++) {
    Group &G = groups[g];
    if (!G.enabled && G.stepCount == 0 && !G.name[0]) continue;
    s += "GROUP|" + String(g) + "|" + String(G.enabled?1:0) + "|" + String(G.hour) + "|" +
         String(G.minute) + "|" + String(G.daysMask) + "|" + String(G.name) + "\n";
    for (uint8_t k = 0; k < G.stepCount; k++)
      s += "STEP|" + String(g) + "|" + String(G.steps[k].channel) + "|" + String(G.steps[k].durationSec) + "\n";
  }
  return s;
}

String getField(const String& s, int idx) {
  int cur = 0, from = 0;
  while (true) {
    int p = s.indexOf('|', from);
    if (cur == idx) return (p < 0) ? s.substring(from) : s.substring(from, p);
    if (p < 0) return "";
    from = p + 1; cur++;
  }
}

int applyImport(const String& txt) {
  memset(groups, 0, sizeof(groups));
  for (int g = 0; g < MAX_GROUPS; g++) { groups[g].lastRunMin = 0xFFFF; groups[g].name[GROUP_NAME_MAX] = '\0'; }
  int applied = 0, start = 0;
  while (start < (int)txt.length()) {
    int nl = txt.indexOf('\n', start);
    String line = (nl < 0) ? txt.substring(start) : txt.substring(start, nl);
    start = (nl < 0) ? txt.length() : nl + 1;
    line.trim();
    if (!line.length() || line[0] == '#') continue;

    if (line.startsWith("WIFI|")) {
      continue;
    } else if (line.startsWith("NAME|")) {
      int idx = getField(line,1).toInt(); 
      String nm = getField(line,2);
      if (idx >= 0 && idx < 8) { saveChannelName(idx, nm); applied++; }
    } else if (line.startsWith("GROUP|")) {
      int g = getField(line,1).toInt();
      if (g < 0 || g >= MAX_GROUPS) continue;
      Group &G = groups[g];
      G.enabled  = getField(line,2).toInt() != 0;
      G.hour     = (uint8_t) constrain(getField(line,3).toInt(), 0, 23);
      G.minute   = (uint8_t) constrain(getField(line,4).toInt(), 0, 59);
      G.daysMask = (uint8_t)(getField(line,5).toInt() & 0x7F);
      String nm  = sanitizeField(getField(line,6), GROUP_NAME_MAX);
      strncpy(G.name, nm.c_str(), GROUP_NAME_MAX); 
      G.name[GROUP_NAME_MAX] = '\0';
      G.stepCount = 0;
      applied++;
    } else if (line.startsWith("STEP|")) {
      int g = getField(line,1).toInt();
      int ch = getField(line,2).toInt();
      int sec = getField(line,3).toInt();
      if (g < 0 || g >= MAX_GROUPS) continue;
      if (ch < 0 || ch > 7) continue;
      Group &G = groups[g];
      if (G.stepCount >= MAX_STEPS) continue;
      G.steps[G.stepCount].channel     = (uint8_t)ch;
      G.steps[G.stepCount].durationSec = (uint16_t)constrain(sec, 1, (int)MAX_RUNTIME_SEC);
      G.stepCount++;
      applied++;
    }
  }
  saveGroups();
  return applied;
}

// ============================================================
// ========================= WIFI =============================
// ============================================================

void startMDNS() {
  if (MDNS.begin(DEVICE_HOSTNAME)) { 
    MDNS.addService("http","tcp",80); 
    Serial.printf("[mDNS] %s.local\n", DEVICE_HOSTNAME); 
  }
  else Serial.println("[mDNS] Start fehlgeschlagen");
}

void wifiBeginSTA() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(staSsid, staPass);
  Serial.printf("[WiFi] Verbinde mit %s ", staSsid);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 15000) { delay(250); Serial.print("."); }
  if (WiFi.status() == WL_CONNECTED) { 
    Serial.printf("\n[WiFi] OK IP=%s\n", WiFi.localIP().toString().c_str()); 
    startMDNS(); 
  }
  else Serial.println("\n[WiFi] FEHLGESCHLAGEN");
  currentMode = OP_STA;
}

void wifiStartAP() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  currentMode = OP_AP;
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.printf("[AP] IP=%s\n", WiFi.softAPIP().toString().c_str());
}

void maintainWifi() {
  if (currentMode != OP_STA) return;
  
  if (WiFi.status() == WL_CONNECTED) {
    lastWifiReconnectAttempt = 0;
    return;
  }
  
  if (millis() - lastWifiReconnectAttempt > WIFI_RECONNECT_RETRY_MS) {
    lastWifiReconnectAttempt = millis();
    Serial.println("[WiFi] Reconnect-Versuch (nach Timeout)");
    WiFi.disconnect(false);
    WiFi.begin(staSsid, staPass);
  }
}

// ============================================================
// ========================== NTP =============================
// ============================================================

void initTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  struct tm ti;
  if (getLocalTime(&ti, 8000)) { 
    ntpSynced = true; 
    Serial.println("[NTP] sync"); 
  }
  else Serial.println("[NTP] noch nicht sync");
}

// ============================================================
// ======================== DISPLAY ===========================
// ============================================================

int drawWrapped(int x, int y, int wpx, const String& s) {
  const int charW = 6, lineH = 8;
  int maxChars = wpx / charW; 
  if (maxChars < 1) maxChars = 1;
  for (int i = 0; i < (int)s.length(); i += maxChars) {
    display.setCursor(x, y); 
    display.print(s.substring(i, i+maxChars)); 
    y += lineH;
  }
  return y;
}

void showQRCodeForAP() {
  String s = String("WIFI:T:WPA;S:") + AP_SSID + ";P:" + AP_PASS + ";;";
  QRCode qr; 
  uint8_t buffer[qrcode_getBufferSize(3)];
  qrcode_initText(&qr, buffer, 3, ECC_LOW, s.c_str());
  display.clearDisplay();
  const int scale = 2, qrPx = qr.size*scale, offX = 0;
  int offY = (OLED_HEIGHT - qrPx)/2; 
  if (offY < 0) offY = 0;
  for (uint8_t y = 0; y < qr.size; y++)
    for (uint8_t x = 0; x < qr.size; x++)
      if (qrcode_getModule(&qr, x, y))
        display.fillRect(offX+x*scale, offY+y*scale, scale, scale, SSD1306_WHITE);
  const int tx = offX+qrPx+4, tw = OLED_WIDTH-tx; 
  int ty = 0;
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(tx, ty); 
  display.print("AP-Login"); 
  ty += 10;
  display.setCursor(tx, ty); 
  display.print("Netz:");    
  ty += 8;
  ty = drawWrapped(tx, ty, tw, String(AP_SSID));
  display.setCursor(tx, ty); 
  display.print("Pass:");    
  ty += 8;
  ty = drawWrapped(tx, ty, tw, String(AP_PASS));
  display.display();
}

void updateStatusDisplay() {
  if (currentMode == OP_AP) return;
  display.clearDisplay();
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE); 
  display.setCursor(0,0);
  
  if (winterActive) {
    display.println("WINTER-ENTWAESSERUNG");
    display.printf("Durchgang %u/%u\n", winterPass, WINTER_PASSES);
    const char* ph = (winterPhase==WP_BLOW)?"BLASEN":(winterPhase==WP_PAUSE)?"PAUSE":"COOLDOWN";
    if (winterPhase==WP_BLOW){ 
      display.print(channelShort(winterChannel)); 
      display.printf(" %s\n",ph); 
    }
    else display.printf("%s\n", ph);
    display.printf("Rest: %lus\n",(unsigned long)winterPhaseRemainingSec());
    display.printf("kumRun: %us\n", winterCumRunSec);
    display.display(); 
    return;
  }
  
  display.println("Bewaesserung ESP32");
  
  if (WiFi.status()==WL_CONNECTED) {
    display.print(WiFi.localIP().toString());
    display.printf(" [%d%%]\n", getWifiSignalQuality());
  } else {
    display.println("WiFi: offline");
  }
  
  display.println(nowTimeStr());
  
  bool any = false;
  for (uint8_t i = 0; i < 8; i++)
    if (channels[i].active) {
      uint32_t rem = (channels[i].durationMs-(millis()-channels[i].startMs))/1000;
      display.print(channelShort(i)); 
      display.printf(" %lus\n",(unsigned long)rem); 
      any = true;
    }
  if (!any) display.println("Alle Kanaele aus");
  
  display.printf("Queue: %u\n",(unsigned)taskQueue.size());
  display.display();
}

// ============================================================
// ===================== WEB: BASIS ===========================
// ============================================================

bool checkAuth() {
  if (server.hasHeader("Authorization")) {
    String h = server.header("Authorization");
    if (h.startsWith("Bearer ") && tokenValid(h.substring(7))) return true;
  }
  if (!server.authenticate(WEB_USER, WEB_PASS)) { 
    server.requestAuthentication(); 
    return false; 
  }
  return true;
}

bool checkWebAuthOnly() {
  if (!server.authenticate(WEB_USER, WEB_PASS)) { 
    server.requestAuthentication(); 
    return false; 
  }
  return true;
}

String themeRoot() {
  String s = ":root{";
  s += String("--bg:")+TH_BG+";--panel:"+TH_PANEL+";--panel2:"+TH_PANEL2+";--text:"+TH_TEXT+
       ";--muted:"+TH_MUTED+";--accent:"+TH_ACCENT+";--danger:"+TH_DANGER+";--border:"+TH_BORDER+
       ";--radius:10px;--lblw:130px;}";
  return s;
}

static const char STYLE_BASE[] PROGMEM = R"CSS(
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,sans-serif;background:var(--bg);color:var(--text)}
.wrap{max-width:680px;margin:0 auto;padding:16px}
h1{font-size:1.3rem;margin:0 0 4px}
h2{font-size:1.05rem;margin:0 0 .6em}
nav{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0}
nav a{flex:1;text-align:center;padding:10px;border:1px solid var(--border);border-radius:var(--radius);color:var(--text);text-decoration:none;background:var(--panel);font-size:.9rem}
nav a.active{background:var(--accent);color:var(--bg);border-color:var(--accent);font-weight:600}
.card{background:var(--panel);border:1px solid var(--border);border-radius:var(--radius);padding:14px;margin:10px 0}
.ch{background:var(--panel);border:1px solid var(--border);border-radius:var(--radius);padding:10px;margin:8px 0}
.chhead{display:flex;justify-content:space-between;align-items:baseline}
.chhead .nm{font-weight:600}
.chhead .st{color:var(--muted);font-size:.85rem}
.chrow{display:flex;gap:10px;align-items:center;margin-top:6px}
.chleft{flex:1;display:flex;align-items:center;gap:6px;min-width:0}
.chright{flex:1;display:flex;gap:8px;justify-content:flex-end}
.chright form{margin:0}
.chright button{padding:8px 14px}
select,input[type=text],input[type=number]{background:var(--panel2);color:var(--text);border:1px solid var(--border);border-radius:8px;padding:8px;font-size:.95rem;width:100%}
.chleft select{width:auto;min-width:0}
textarea{width:100%;min-height:140px;background:var(--panel2);color:var(--text);border:1px solid var(--border);border-radius:8px;padding:8px;font-family:monospace}
button{cursor:pointer;border:none;border-radius:8px;padding:10px 16px;font-size:.95rem;font-weight:600;color:var(--bg);background:var(--accent)}
button.stop{background:var(--danger);color:#fff}
.btns{display:flex;gap:8px;margin-top:10px;flex-wrap:wrap}
.btns form{margin:0}
.mt{margin-top:10px}
.muted{color:var(--muted);font-size:.85rem}
.warn{color:var(--danger);font-weight:600}
code{background:var(--panel2);padding:2px 5px;border-radius:5px}
#overview div{padding:2px 0;font-size:.92rem}
.row{display:grid;grid-template-columns:var(--lblw) minmax(0,1fr);gap:10px;align-items:center;margin:8px 0}
.row>label{color:var(--muted);font-size:.9rem}
.inline{display:flex;gap:8px;align-items:center;flex-wrap:wrap;min-width:0}
.inline select{width:auto;min-width:0;flex:1 1 auto;max-width:100%}
.step{display:flex;gap:6px;align-items:center;margin:6px 0;min-width:0}
.step .snum{flex:0 0 auto;color:var(--muted);font-size:.85rem;width:1.6em;text-align:right}
.step select{flex:1 1 0;min-width:0;width:auto}
.qtable{width:100%;border-collapse:collapse;margin:10px 0}
.qtable td{padding:6px;border-bottom:1px solid var(--border);font-size:.9rem}
.qtable td:first-child{color:var(--muted)}
.days{display:flex;gap:14px;flex-wrap:wrap}
.days label{display:flex;align-items:center;gap:4px;color:var(--text);font-size:.9rem}
pre{white-space:pre-wrap;background:var(--panel2);padding:10px;border-radius:8px;border:1px solid var(--border);max-height:400px;overflow-y:auto}
.grp{font-weight:600;margin-top:12px;color:var(--accent)}
a.btnlink{display:inline-block;text-decoration:none;color:var(--bg);background:var(--accent);padding:10px 16px;border-radius:8px;font-weight:600}
)CSS";

static const char STATUS_SCRIPT[] PROGMEM = R"JS(
<script>
function esc(s){return (''+s).replace(/&/g,'&amp;').replace(/</g,'&lt;')}
async function refreshStatus(){
 try{
  const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)return;const d=await r.json();
  const ov=document.getElementById('overview');
  if(ov){
   const act=d.channels.filter(c=>c.active).map(c=>esc(c.name)+' '+c.remaining_s+'s');
   let l=['Gerät: '+esc(d.host),'Zeit: '+esc(d.time)];
   let wifi='WLAN: '+(d.ip?esc(d.ip):'offline')+' ['+(d.signal_quality||0)+'%]';
   l.push(wifi);
   l.push('Aktiv: '+(act.length?act.join(', '):'keine'));
   if(d.queue_items&&d.queue_items.length)l.push('Queue: '+d.queue+' — '+d.queue_items.map(it=>esc(it.name)+' '+Math.round(it.sec/60)+'min').join(' → '));
   else l.push('Queue: '+d.queue);
   if(d.winter&&d.winter.active)l.push('WINTER: Zone '+d.winter.channel+' '+esc(d.winter.phase)+' '+d.winter.remaining_s+'s');
   ov.innerHTML=l.map(x=>'<div>'+x+'</div>').join('');
  }
  const qt=document.getElementById('queue-table');
  if(qt&&d.queue_items){
   let h='<tr><td style="font-weight:600">Kanal</td><td style="font-weight:600">Dauer</td></tr>';
   d.queue_items.forEach(it=>{h+='<tr><td>'+esc(it.name)+'</td><td>'+it.sec+'s</td></tr>';});
   qt.innerHTML=h;
  }
  d.channels.forEach((c,i)=>{const e=document.getElementById('ch-status-'+i);
   if(e)e.textContent=c.active?('AN · '+c.remaining_s+'s'):'aus';});
 }catch(e){}
}
setInterval(refreshStatus,3000);refreshStatus();
</script>
)JS";

String navLink(const String& href, const String& label, bool active) {
  return "<a href='"+href+"'"+(active?" class='active'":"")+">"+label+"</a>";
}

String pageHead(const String& title, const String& active) {
  String h = "<!doctype html><html lang='de'><head><meta charset='utf-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'><title>"+title+"</title><style>";
  h += themeRoot(); 
  h += FPSTR(STYLE_BASE); 
  h += "</style></head><body><div class='wrap'>";
  h += "<h1>Bewässerungssteuerung</h1><nav>";
  h += navLink("/", "Start", active=="home");
  h += navLink("/queue", "Queue", active=="queue");
  h += navLink("/plans", "Pläne", active=="plans");
  h += navLink("/logs", "Logs", active=="logs");
  h += navLink("/config", "Konfig", active=="config");
  h += navLink("/winter", "Winter", active=="winter");
  h += "</nav>";
  return h;
}

String pageFoot(bool withScript) {
  String h; 
  if (withScript) h += FPSTR(STATUS_SCRIPT);
  h += "</div></body></html>"; 
  return h;
}

String durationSelect(const String& field, const String& formAttr = "") {
  String f = formAttr.length() ? (" form='"+formAttr+"'") : "";
  String s = "<select name='"+field+"'"+f+">";
  for (uint8_t i = 0; i < RUNTIME_OPTIONS_CNT; i++) {
    uint16_t m = RUNTIME_OPTIONS_MIN[i]; 
    uint32_t sec = (uint32_t)m*60UL;
    s += "<option value='"+String(sec)+"'"+(m==RUNTIME_DEFAULT_MIN?" selected":"")+">"+String(m)+" min</option>";
  }
  s += "</select>"; 
  return s;
}

String intSelect(const String& field, const String& id, int lo, int hi, int sel, bool pad) {
  String s = "<select name='"+field+"' id='"+id+"'>";
  for (int v = lo; v <= hi; v++) {
    char b[4]; 
    if (pad) snprintf(b,4,"%02d",v); 
    else snprintf(b,4,"%d",v);
    s += "<option value='"+String(v)+"'"+(v==sel?" selected":"")+">"+String(b)+"</option>";
  }
  s += "</select>"; 
  return s;
}

String groupSlotSelect() {
  String s = "<select name='slot' id='slot'>";
  for (int i = 0; i < MAX_GROUPS; i++) {
    String lbl = "Gruppe " + String(i+1);
    if (groups[i].name[0] || groups[i].stepCount) {
      char t[8]; 
      snprintf(t,8,"%02u:%02u", groups[i].hour, groups[i].minute);
      lbl += " — " + groupLabel(i) + " " + t + (groups[i].enabled?"":" (inaktiv)");
    } else lbl += " (frei)";
    s += "<option value='"+String(i)+"'>"+htmlEscape(lbl)+"</option>";
  }
  s += "</select>"; 
  return s;
}

String channelStepSelect(const String& field, int selCh) {
  String s = "<select name='"+field+"'><option value='-1'"+(selCh<0?" selected":"")+">— (kein) —</option>";
  for (uint8_t i = 0; i < 8; i++)
    s += "<option value='"+String(i)+"'"+(selCh==(int)i?" selected":"")+">"+htmlEscape(channelName(i))+"</option>";
  s += "</select>"; 
  return s;
}

// ============================================================
// ===================== WEB: SEITEN ==========================
// ============================================================

String buildHome() {
  String h = pageHead("Bewässerung", "home");
  h += "<div class='card'><div id='overview'>";
  h += "<div>Gerät: " + String(DEVICE_HOSTNAME) + "</div>";
  h += "<div>Zeit: " + nowTimeStr() + "</div>";
  h += "<div>WLAN: " + buildWifiStatusWithQuality() + "</div>";
  h += "</div></div></div>";

  if (winterActive)
    h += "<div class='card'><span class='warn'>Winter-Entwässerung aktiv</span> – siehe Winter-Seite.</div>";

  for (uint8_t i = 0; i < 8; i++) {
    String fid = "start-" + String(i);
    String stTxt = "aus";
    if (channels[i].active) {
      uint32_t rem = (channels[i].durationMs-(millis()-channels[i].startMs))/1000;
      stTxt = "AN · " + String(rem) + "s";
    }
    h += "<div class='ch'>";
    h += "<div class='chhead'><span class='nm'>" + htmlEscape(channelName(i)) +
         "</span><span class='st' id='ch-status-" + String(i) + "'>" + stTxt + "</span></div>";
    h += "<div class='chrow'>";
    h += "<div class='chleft'><label class='muted'>Laufzeit:</label> " + durationSelect("sec", fid) + "</div>";
    h += "<div class='chright'>"
         "<form id='" + fid + "' action='/start' method='POST'>"
         "<input type='hidden' name='ch' value='" + String(i) + "'>"
         "<button type='submit'>Start</button></form>"
         "<form action='/stop' method='POST'>"
         "<input type='hidden' name='ch' value='" + String(i) + "'>"
         "<button class='stop' type='submit'>Stop</button></form>"
         "</div></div></div>";
  }
  h += "<div class='card btns'><form action='/stopall' method='POST'>"
       "<button class='stop' type='submit'>NOT-AUS (alles stoppen + Queue leeren)</button></form></div>";
  h += pageFoot(true);
  return h;
}

String buildQueue() {
  String h = pageHead("Queue", "queue");
  h += "<div class='card'><div id='overview'></div></div>";
  h += "<div class='card'><h2>Warteschlange</h2>";
  h += "<table class='qtable' id='queue-table'><tr><td>Kanal</td><td>Dauer</td></tr></table>";
  h += "<div class='btns'><form action='/queue/clear' method='POST'>"
       "<button class='stop' type='submit'>Queue leeren</button></form></div></div>";
  h += pageFoot(true);
  return h;
}

String buildLogs(const String& msg = "") {
  String h = pageHead("Logs", "logs");
  if (msg.length()) h += "<div class='card'>" + htmlEscape(msg) + "</div>";
  h += "<div class='card'>";
  h += "<div class='muted' style='margin-bottom:10px;'>WLAN: " + buildWifiStatusWithQuality() + "</div>";
  h += "<h2>Ereignisprotokolle</h2>";
  h += "<p class='muted'>Lokal: CSV-Datei auf dem Gerät. ";
  if (mqttEnabled && mqttHost[0])
    h += "MQTT: " + String(mqttHost) + " (Queue: " + String((unsigned)mqttRetryQueue.size()) + " ausstehend).";
  else
    h += "MQTT: deaktiviert.";
  h += "</p>";
  h += "<div class='btns'>";
  h += "<a class='btnlink' href='/logs/download'>Download CSV</a>";
  h += "<form action='/logs/clear' method='POST' style='display:inline'>"
       "<button class='stop' type='submit'>Logs löschen</button></form>";
  h += "</div>";
  h += "<h2 class='mt'>Letzten 50 Einträge</h2><pre id='log-preview'>(lädt...)</pre>";
  h += "<script>async function loadPreview(){try{const r=await fetch('/logs/preview');const t=await r.text();document.getElementById('log-preview').textContent=t;}catch(e){}}loadPreview();</script>";
  h += "</div>" + pageFoot(false);
  return h;
}

String buildPlans(const String& msg = "") {
  String h = pageHead("Pläne", "plans");
  if (msg.length()) h += "<div class='card'>" + htmlEscape(msg) + "</div>";

  h += "<div class='card'>";
  h += "<div class='muted' style='margin-bottom:10px;'>WLAN: " + buildWifiStatusWithQuality() + "</div>";
  h += "<h2>Gruppe anlegen / bearbeiten</h2><form action='/setgroup' method='POST'>";
  h += "<div class='row'><label>Gruppe (Slot)</label>" + groupSlotSelect() + "</div>";
  h += "<div class='row'><label>Name</label>"
       "<input type='text' name='gname' id='gname' maxlength='" + String(GROUP_NAME_MAX) +
       "' placeholder='z. B. Vorgarten'></div>";
  h += "<div class='row'><label>Aktiv</label><div><input type='checkbox' id='en' name='en' checked></div></div>";
  h += "<div class='row'><label>Startzeit</label><div class='inline'>" +
       intSelect("h","h",0,23,6,true) + "<span>:</span>" + intSelect("m","m",0,59,0,true) + "</div></div>";

  h += "<div class='row'><label>Tage</label><div class='days'>";
  const uint8_t order[7] = {1,2,3,4,5,6,0};
  const char* labels[7]  = {"Mo","Di","Mi","Do","Fr","Sa","So"};
  for (uint8_t k = 0; k < 7; k++)
    h += "<label><input type='checkbox' id='d"+String(order[k])+"' name='d"+String(order[k])+"' checked>"+String(labels[k])+"</label>";
  h += "</div></div>";

  h += "<h2 class='mt'>Schritte (Reihenfolge = Abarbeitung)</h2>";
  for (uint8_t k = 0; k < MAX_STEPS; k++) {
    h += "<div class='step'><span class='snum'>" + String(k+1) + ".</span>" +
         channelStepSelect("s"+String(k)+"ch", -1) +
         durationSelect("s"+String(k)+"sec") + "</div>";
  }
  h += "<div class='btns'><button type='submit'>Gruppe speichern</button></div></form></div>";

  h += "<div class='card'><h2>Bewässerungspläne</h2>";
  bool any = false;
  for (int g = 0; g < MAX_GROUPS; g++) {
    Group &G = groups[g];
    if (!G.enabled && G.stepCount == 0 && !G.name[0]) continue;
    any = true;
    char t[8]; 
    snprintf(t,8,"%02u:%02u", G.hour, G.minute);
    h += "<div class='grp'>[" + String(g) + "] " + htmlEscape(groupLabel(g)) +
         " · " + String(t) + " · [" + daysMaskToStr(G.daysMask) + "] · " +
         (G.enabled ? "aktiv" : "<span class='warn'>inaktiv</span>") + "</div><pre>";
    if (G.stepCount == 0) h += "(keine Schritte)";
    for (uint8_t k = 0; k < G.stepCount; k++)
      h += String(k+1) + ". " + htmlEscape(channelName(G.steps[k].channel)) +
           " — " + String(G.steps[k].durationSec/60) + " min\n";
    h += "</pre>";
  }
  if (!any) h += "<pre>(keine Gruppen)</pre>";
  h += "<form action='/cleargroup' method='POST' class='mt'><div class='row'><label>Gruppe löschen</label>"
       "<select name='slot'>";
  for (int i = 0; i < MAX_GROUPS; i++) h += "<option value='"+String(i)+"'>Gruppe "+String(i+1)+"</option>";
  h += "</select></div><div class='btns'><button class='stop' type='submit'>Löschen</button></div></form></div>";

  String data = "[";
  for (int g = 0; g < MAX_GROUPS; g++) {
    Group &G = groups[g];
    if (g) data += ",";
    String nm = String(G.name); 
    nm.replace("\\","\\\\"); 
    nm.replace("'","\\'");
    data += "{en:" + String(G.enabled?1:0) + ",name:'" + nm + "',h:" + String(G.hour) +
            ",m:" + String(G.minute) + ",days:" + String(G.daysMask) + ",steps:[";
    for (uint8_t k = 0; k < G.stepCount; k++) {
      if (k) data += ",";
      data += "{ch:" + String(G.steps[k].channel) + ",sec:" + String(G.steps[k].durationSec) + "}";
    }
    data += "]}";
  }
  data += "]";
  h += "<script>const GROUPS=" + data + ";const MAXSTEPS=" + String(MAX_STEPS) + ";";
  h += R"JS(
function loadGroup(){
 var i=+document.getElementById('slot').value;var g=GROUPS[i];if(!g)return;
 document.getElementById('en').checked=!!g.en;
 document.getElementById('gname').value=g.name;
 document.getElementById('h').value=g.h;
 document.getElementById('m').value=g.m;
 for(var d=0;d<7;d++){var e=document.getElementById('d'+d);if(e)e.checked=!!(g.days&(1<<d));}
 for(var k=0;k<MAXSTEPS;k++){
  var ce=document.getElementsByName('s'+k+'ch')[0];
  var se=document.getElementsByName('s'+k+'sec')[0];
  var st=g.steps[k];
  if(ce)ce.value=st?st.ch:-1;
  if(se&&st)se.value=st.sec;
 }
}
document.getElementById('slot').addEventListener('change',loadGroup);loadGroup();
</script>)JS";

  h += pageFoot(false);
  return h;
}

String buildConfig(const String& msg = "") {
  String h = pageHead("Konfiguration", "config");
  if (msg.length()) h += "<div class='card'>" + htmlEscape(msg) + "</div>";

  h += "<div class='card'><h2>Gerät</h2>";
  h += "<div class='muted'>Hostname: " + String(DEVICE_HOSTNAME) + ".local</div>";
  h += "<div class='muted'>WLAN: " + buildWifiStatusWithQuality() + "</div>";
  if (rtcPresent) h += "<div class='muted'>RTC: " + getRtcTimeStr() + "</div>";
  h += "<div class='muted mt'>WLAN-Zugangsdaten nur im AP-Konfig-Modus änderbar (BOOT-Taste 3 s).</div></div>";

  h += "<div class='card'><h2>Kanalnamen</h2><form action='/setnames' method='POST'>";
  for (uint8_t i = 0; i < 8; i++)
    h += "<div class='row'><label>Kanal " + String(i+1) + "</label>"
         "<input type='text' name='ch" + String(i) + "' maxlength='" + String(CH_NAME_MAX) +
         "' value='" + htmlEscape(String(channelNames[i])) + "'></div>";
  h += "<div class='btns'><button type='submit'>Namen speichern</button></div></form></div>";

  h += "<div class='card'><h2>Relais-Polarität</h2><form action='/setrelay' method='POST'>";
  h += "<div class='row'><label>Relais active-LOW</label>"
       "<div><input type='checkbox' name='activeLow'" + String(relayActiveLow ? " checked" : "") +
       "> <span class='muted'>aktiv = Schaltpegel LOW (Standard für 5V-Relaismodule)</span></div></div>"
       "<div class='btns'><button class='stop' type='submit'>Polarität ändern (schaltet alles ab)</button></div></form></div>";

  h += "<div class='card'><h2>API-Tokens</h2>";
  h += "<p class='muted'>Tokens werden maskiert angezeigt; Klartext nur direkt nach Erzeugung. "
       "Für API-Aufrufe: <code>Authorization: Bearer &lt;token&gt;</code></p><pre>";
  bool anyTok = false;
  for (uint8_t i = 0; i < MAX_API_TOKENS; i++) {
    if (!apiTokens[i][0]) continue;
    anyTok = true;
    String masked = String(apiTokens[i]).substring(0,4) + "…" +
                    String(apiTokens[i]).substring(API_TOKEN_LEN-4);
    h += "[" + String(i) + "] " + masked + "\n";
  }
  if (!anyTok) h += "(keine Tokens)";
  h += "</pre><div class='btns'>";
  h += "<form action='/token/add' method='POST'><button type='submit'>Token erzeugen</button></form>";
  h += "<form action='/token/delete' method='POST' class='inline'>"
       "<select name='idx'>";
  for (uint8_t i = 0; i < MAX_API_TOKENS; i++) h += "<option value='"+String(i)+"'>Slot "+String(i)+"</option>";
  h += "</select> <button class='stop' type='submit'>Token löschen</button></form>";
  h += "</div></div>";

  h += "<div class='card'><h2>MQTT-Protokollierung</h2><form action='/mqtt/config' method='POST'>";
  h += "<div class='row'><label>MQTT aktiv</label><div><input type='checkbox' name='en'" +
       String(mqttEnabled ? " checked" : "") + "></div></div>";
  h += "<div class='row'><label>Broker (Host)</label>"
       "<input type='text' name='host' maxlength='" + String(MQTT_HOST_MAX) +
       "' value='" + htmlEscape(String(mqttHost)) + "' placeholder='mqtt.example.com'></div>";
  h += "<div class='row'><label>Port</label>"
       "<input type='number' name='port' value='" + String(mqttPort) + "' min='1' max='65535'></div>";
  h += "<div class='row'><label>Benutzer</label>"
       "<input type='text' name='user' maxlength='" + String(MQTT_USER_MAX) +
       "' value='" + htmlEscape(String(mqttUser)) + "' placeholder='(optional)'></div>";
  h += "<div class='row'><label>Passwort</label>"
       "<input type='text' name='pass' maxlength='" + String(MQTT_PASS_MAX) +
       "' value='" + htmlEscape(String(mqttPass)) + "' placeholder='(optional)'></div>";
  h += "<div class='btns'><button type='submit'>MQTT speichern</button></div></form>";
  if (mqttEnabled && mqttHost[0]) {
    h += "<p class='muted mt'>Status: " + String(mqttClient.connected() ? "verbunden" : "nicht verbunden") +
         " | Retry-Queue: " + String((unsigned)mqttRetryQueue.size()) + "</p>";
  }
  h += "</div>";

  h += "<div class='card'><h2>Konfiguration sichern</h2>";
  h += "<p class='muted'>Export/Import: Kanalnamen, Gruppen-Pläne, SSID (ohne Passwort).</p>";
  h += "<a class='btnlink' href='/export'>Export herunterladen</a>";
  h += "<form action='/import' method='POST' class='mt'>"
       "<label class='muted'>Import:</label>"
       "<textarea name='cfg' placeholder='# IrrigationController config v3 ...'></textarea>"
       "<div class='btns'><button type='submit'>Import anwenden</button></div></form></div>";

  h += pageFoot(false);
  return h;
}

String buildWinter() {
  String h = pageHead("Winter", "winter");
  h += "<div class='card'>";
  h += "<div class='muted' style='margin-bottom:10px;'>WLAN: " + buildWifiStatusWithQuality() + "</div>";
  h += "<div id='overview'></div>";
  h += "</div>";
  h += "<div class='card'><h2>Winter-Entwässerung</h2>";
  h += "<p class='warn'>Erst Wasserzufuhr schließen und Druckluft anschließen. Blasdruck per Druckminderer hardwareseitig begrenzen.</p>";
  if (winterActive) {
    h += "<p>Aktiv – Durchgang " + String(winterPass) + "/" + String(WINTER_PASSES) +
         ", Zone " + htmlEscape(channelName(winterChannel)) + ", Phase " +
         String(winterPhase==WP_BLOW?"Blasen":winterPhase==WP_PAUSE?"Pause":"Cooldown") + ".</p>";
    h += "<form action='/winter' method='POST'><input type='hidden' name='action' value='stop'>"
         "<div class='btns'><button class='stop' type='submit'>Winter-Routine STOP</button></div></form>";
  } else {
    h += "<form action='/winter' method='POST'><input type='hidden' name='action' value='start'>"
         "<div class='btns'><button type='submit'>Winter-Routine starten</button></div></form>";
  }
  h += "</div>" + pageFoot(true);
  return h;
}

String buildApConfigHtml(const String& msg = "") {
  String h = pageHead("WLAN-Konfig", "");
  h += "<div class='card'><h2>WLAN-Konfiguration (AP-Modus)</h2>";
  if (msg.length()) h += "<p class='warn'>" + htmlEscape(msg) + "</p>";
  h += "<p class='muted'>Aktuelles STA-Netz: <b>" + htmlEscape(String(staSsid)) + "</b></p>";
  h += "<form action='/setwifi' method='POST'>"
       "<div class='row'><label>SSID (1-32)</label><input type='text' name='ssid' maxlength='32' required></div>"
       "<div class='row'><label>Passwort (8-63)</label><input type='text' name='pass' minlength='8' maxlength='63' required></div>"
       "<div class='btns'><button type='submit'>Speichern &amp; Neustart</button></div></form>"
       "<p class='muted'>Nach dem Speichern Neustart in den STA-Modus.</p></div>";
  h += "</div></body></html>";
  return h;
}

// ============================================================
// =================== WEB: HANDLER ===========================
// ============================================================

void handleRoot() {
  if (currentMode == OP_AP) {
    if (AP_CONFIG_REQUIRE_AUTH && !checkWebAuthOnly()) return;
    server.send(200, "text/html; charset=utf-8", buildApConfigHtml("")); 
    return;
  }
  if (!checkAuth()) return;
  server.send(200, "text/html; charset=utf-8", buildHome());
}

void handleQueue()      { if (!checkAuth()) return; server.send(200,"text/html; charset=utf-8", buildQueue()); }
void handleLogs()       { if (!checkAuth()) return; server.send(200,"text/html; charset=utf-8", buildLogs()); }
void handlePlans()      { if (!checkAuth()) return; server.send(200,"text/html; charset=utf-8", buildPlans()); }
void handleConfig()     { if (!checkAuth()) return; server.send(200,"text/html; charset=utf-8", buildConfig()); }
void handleWinterPage() { if (!checkAuth()) return; server.send(200,"text/html; charset=utf-8", buildWinter()); }

void handleSetWifi() {
  if (currentMode != OP_AP) { server.send(403,"text/plain","Nur im AP-Konfig-Modus"); return; }
  if (AP_CONFIG_REQUIRE_AUTH && !checkWebAuthOnly()) return;
  if (!server.hasArg("ssid") || !server.hasArg("pass")) {
    server.send(400,"text/html; charset=utf-8", buildApConfigHtml("SSID und Passwort erforderlich")); 
    return; 
  }
  String ssid = server.arg("ssid"), pass = server.arg("pass");
  if (ssid.length() < SSID_MIN_LEN || ssid.length() > SSID_MAX_LEN) {
    server.send(400,"text/html; charset=utf-8", buildApConfigHtml("SSID-Länge ungültig (1-32)")); 
    return; 
  }
  if (pass.length() < PASS_MIN_LEN || pass.length() > PASS_MAX_LEN) {
    server.send(400,"text/html; charset=utf-8", buildApConfigHtml("Passwort-Länge ungültig (8-63)")); 
    return; 
  }
  saveWifiCreds(ssid, pass);
  server.send(200,"text/html; charset=utf-8","<!doctype html><meta charset='utf-8'><p>Gespeichert. Neustart...</p>");
  delay(1500); 
  ESP.restart();
}

void handleSetNames() {
  if (!checkAuth()) return;
  for (uint8_t i = 0; i < 8; i++) {
    String key = "ch" + String(i);
    if (server.hasArg(key)) saveChannelName(i, server.arg(key));
  }
  server.send(200,"text/html; charset=utf-8", buildConfig("Kanalnamen gespeichert."));
}

void handleSetRelay() {
  if (!checkAuth()) return;
  stopWinterization();
  stopAllChannels();
  clearQueue();
  bool al = server.hasArg("activeLow");
  saveRelayPolarity(al);
  for (uint8_t i = 0; i < 8; i++)
    digitalWrite(CHANNEL_PINS[i], relayActiveLow ? HIGH : LOW);
  server.send(200, "text/html; charset=utf-8",
    buildConfig(String("Relais-Polarität gesetzt: ") + (al ? "active-LOW" : "active-HIGH") +
                ". Alle Kanäle wurden sicherheitshalber abgeschaltet."));
}

void handleTokenAdd() {
  if (!checkAuth()) return;
  int slot = firstFreeTokenSlot();
  if (slot < 0) { server.send(200,"text/html; charset=utf-8", buildConfig("Token-Limit erreicht.")); return; }
  String t = genToken();
  strncpy(apiTokens[slot], t.c_str(), API_TOKEN_LEN); 
  apiTokens[slot][API_TOKEN_LEN]='\0';
  persistApiTokens();
  server.send(200,"text/html; charset=utf-8",
    buildConfig("Neuer Token (jetzt notieren, wird nur einmal vollständig gezeigt): " + t));
}

void handleTokenDelete() {
  if (!checkAuth()) return;
  if (!server.hasArg("idx")) { server.send(400,"text/plain","missing idx"); return; }
  int idx = server.arg("idx").toInt();
  if (idx < 0 || idx >= MAX_API_TOKENS) { server.send(400,"text/plain","bad idx"); return; }
  apiTokens[idx][0] = '\0'; 
  persistApiTokens();
  server.send(200,"text/html; charset=utf-8", buildConfig("Token gelöscht."));
}

void handleStart() {
  if (!checkAuth()) return;
  if (winterActive) { server.send(409,"text/plain","Winterroutine aktiv"); return; }
  if (!server.hasArg("ch") || !server.hasArg("sec")) { server.send(400,"text/plain","missing args"); return; }
  
  uint8_t ch = (uint8_t)server.arg("ch").toInt();
  uint16_t sec = (uint16_t)server.arg("sec").toInt();
  
  // Wenn SINGLE_CHANNEL_MODE aktiv und ein Kanal läuft → queueen
  if (SINGLE_CHANNEL_MODE && anyChannelActive()) {
    enqueueTask(ch, sec, "manual");
    Serial.printf("[START] Kanal %u in Queue (bereits aktiv)\n", ch + 1);
  } else {
    // Sonst direkt starten
    startChannel(ch, sec);
  }
  
  server.sendHeader("Location","/"); 
  server.send(303);
}

void handleStop() {
  if (!checkAuth()) return;
  if (!server.hasArg("ch")) { server.send(400,"text/plain","missing ch"); return; }
  stopChannel((uint8_t)server.arg("ch").toInt());
  server.sendHeader("Location","/"); 
  server.send(303);
}

void handleStopAll() {
  if (!checkAuth()) return;
  stopWinterization(); 
  stopAllChannels(); 
  clearQueue();
  server.sendHeader("Location","/"); 
  server.send(303);
}

void handleQueueClear() {
  if (!checkAuth()) return;
  clearQueue();
  server.sendHeader("Location","/queue"); 
  server.send(303);
}

void handleLogDownload() {
  if (!checkAuth()) return;
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) { server.send(404, "text/plain", "Keine Logdatei"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=irrigation-log.csv");
  server.streamFile(f, "text/plain; charset=utf-8");
  f.close();
}

void handleLogPreview() {
  if (!checkAuth()) return;
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) { server.send(200, "text/plain", "(Keine Einträge)"); return; }
  String preview;
  int lines = 0;
  f.seek(0, SeekEnd);
  size_t sz = f.position();
  f.seek(0, SeekSet);
  
  if (sz > 10000) f.seek(sz - 10000, SeekSet);
  while (f.available() && lines < 50) {
    String line = f.readStringUntil('\n');
    if (line.length()) preview += line + "\n";
    lines++;
  }
  f.close();
  server.send(200, "text/plain; charset=utf-8", preview);
}

void handleLogClear() {
  if (!checkAuth()) return;
  LittleFS.remove(LOG_FILE);
  server.send(200,"text/html; charset=utf-8", buildLogs("Logs gelöscht."));
}

void handleMqttConfig() {
  if (!checkAuth()) return;
  bool en = server.hasArg("en");
  String host = server.hasArg("host") ? server.arg("host") : "";
  uint16_t port = server.hasArg("port") ? (uint16_t)server.arg("port").toInt() : MQTT_PORT_DEFAULT;
  String user = server.hasArg("user") ? server.arg("user") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  if (port < 1 || port > 65535) port = MQTT_PORT_DEFAULT;
  saveMqttConfig(en, host, port, user, pass);
  mqttConnect();
  server.send(200,"text/html; charset=utf-8", buildConfig("MQTT-Konfiguration gespeichert."));
}

void handleWinter() {
  if (!checkAuth()) return;
  String a = server.hasArg("action") ? server.arg("action") : "start";
  if (a == "stop") stopWinterization(); else startWinterization();
  server.sendHeader("Location","/winter"); 
  server.send(303);
}

void handleSetGroup() {
  if (!checkAuth()) return;
  if (!server.hasArg("slot") || !server.hasArg("h") || !server.hasArg("m")) {
    server.send(400,"text/plain","missing args"); 
    return; 
  }
  uint8_t slot = (uint8_t)server.arg("slot").toInt();
  if (slot >= MAX_GROUPS) { server.send(400,"text/plain","bad slot"); return; }
  Group &G = groups[slot];
  String nm = sanitizeField(server.hasArg("gname") ? server.arg("gname") : "", GROUP_NAME_MAX);
  strncpy(G.name, nm.c_str(), GROUP_NAME_MAX); 
  G.name[GROUP_NAME_MAX] = '\0';
  G.enabled = server.hasArg("en");
  G.hour    = (uint8_t) constrain(server.arg("h").toInt(), 0, 23);
  G.minute  = (uint8_t) constrain(server.arg("m").toInt(), 0, 59);
  uint8_t mask = 0;
  for (int d = 0; d < 7; d++) if (server.hasArg("d"+String(d))) mask |= (1 << d);
  G.daysMask = mask;
  uint8_t cnt = 0;
  for (uint8_t k = 0; k < MAX_STEPS; k++) {
    String chKey = "s"+String(k)+"ch", secKey = "s"+String(k)+"sec";
    if (!server.hasArg(chKey)) continue;
    int ch = server.arg(chKey).toInt();
    if (ch < 0 || ch > 7) continue;
    int sec = server.hasArg(secKey) ? server.arg(secKey).toInt() : DEFAULT_RUNTIME_SEC;
    G.steps[cnt].channel     = (uint8_t)ch;
    G.steps[cnt].durationSec = (uint16_t)constrain(sec, 1, (int)MAX_RUNTIME_SEC);
    cnt++;
  }
  G.stepCount = cnt;
  G.lastRunDay = 0; 
  G.lastRunMin = 0xFFFF;
  saveGroups();
  server.send(200,"text/html; charset=utf-8", buildPlans("Gruppe " + String(slot+1) + " gespeichert (" + String(cnt) + " Schritte)."));
}

void handleClearGroup() {
  if (!checkAuth()) return;
  if (!server.hasArg("slot")) { server.send(400,"text/plain","missing slot"); return; }
  uint8_t slot = (uint8_t)server.arg("slot").toInt();
  if (slot >= MAX_GROUPS) { server.send(400,"text/plain","bad slot"); return; }
  memset(&groups[slot], 0, sizeof(Group));
  groups[slot].lastRunMin = 0xFFFF; 
  groups[slot].name[GROUP_NAME_MAX] = '\0';
  saveGroups();
  server.send(200,"text/html; charset=utf-8", buildPlans("Gruppe " + String(slot+1) + " gelöscht."));
}

void handleExport() {
  if (!checkAuth()) return;
  server.sendHeader("Content-Disposition","attachment; filename=irrigation-config.txt");
  server.send(200,"text/plain; charset=utf-8", buildExport());
}

void handleImport() {
  if (!checkAuth()) return;
  if (!server.hasArg("cfg")) { server.send(400,"text/plain","missing cfg"); return; }
  int n = applyImport(server.arg("cfg"));
  server.send(200,"text/html; charset=utf-8", buildConfig("Import angewendet: " + String(n) + " Einträge."));
}

void handleApiStatus() {
  if (!checkAuth()) return;
  String j = "{\"host\":\"" + String(DEVICE_HOSTNAME) + "\"";
  j += ",\"time\":\"" + nowTimeStr() + "\"";
  j += ",\"ip\":\"" + String(WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString() : "") + "\"";
  j += ",\"wifi\":\"" + String(WiFi.status()==WL_CONNECTED ? "STA" : (currentMode==OP_AP?"AP":"off")) + "\"";
  
  if (WiFi.status()==WL_CONNECTED) {
    j += ",\"rssi\":" + String(WiFi.RSSI());
    j += ",\"signal_quality\":" + String(getWifiSignalQuality());
  } else {
    j += ",\"rssi\":0,\"signal_quality\":0";
  }
  
  j += ",\"ntp\":" + String(ntpSynced?"true":"false");
  j += ",\"channels\":[";
  for (uint8_t i = 0; i < 8; i++) {
    if (i) j += ",";
    uint32_t rem = 0;
    if (channels[i].active) rem = (channels[i].durationMs-(millis()-channels[i].startMs))/1000;
    String nm = channelName(i); 
    nm.replace("\\","\\\\"); 
    nm.replace("\"","\\\"");
    j += "{\"ch\":" + String(i+1) + ",\"name\":\"" + nm + "\",\"active\":" +
         String(channels[i].active?"true":"false") + ",\"remaining_s\":" + String(rem) + "}";
  }
  j += "],\"queue\":" + String((unsigned)taskQueue.size());
  j += ",\"queue_items\":[";
  for (size_t k = 0; k < taskQueue.size(); k++) {
    if (k) j += ",";
    String nm = channelName(taskQueue[k].channel);
    nm.replace("\\","\\\\"); 
    nm.replace("\"","\\\"");
    j += "{\"ch\":" + String(taskQueue[k].channel + 1) +
         ",\"name\":\"" + nm + "\",\"sec\":" + String(taskQueue[k].durationSec) + "}";
  }
  j += "]";
  j += ",\"winter\":{\"active\":" + String(winterActive?"true":"false");
  if (winterActive) {
    j += ",\"pass\":" + String(winterPass) + ",\"channel\":" + String(winterChannel+1) +
         ",\"phase\":\"" + String(winterPhase==WP_BLOW?"blow":winterPhase==WP_PAUSE?"pause":"cooldown") +
         "\",\"remaining_s\":" + String(winterPhaseRemainingSec());
  }
  j += "}}";
  server.send(200,"application/json", j);
}

void handleApiStart() {
  if (!checkAuth()) return;
  if (winterActive) { server.send(409,"application/json","{\"ok\":false,\"err\":\"winter\"}"); return; }
  if (!server.hasArg("ch") || !server.hasArg("sec")) { server.send(400,"application/json","{\"ok\":false,\"err\":\"args\"}"); return; }
  startChannel((uint8_t)server.arg("ch").toInt(), (uint16_t)server.arg("sec").toInt());
  server.send(200,"application/json","{\"ok\":true}");
}

void handleApiStop() {
  if (!checkAuth()) return;
  if (server.hasArg("ch")) stopChannel((uint8_t)server.arg("ch").toInt());
  else { stopWinterization(); stopAllChannels(); clearQueue(); }
  server.send(200,"application/json","{\"ok\":true}");
}

void handleApiWinter() {
  if (!checkAuth()) return;
  String a = server.hasArg("action") ? server.arg("action") : "start";
  if (a == "stop") stopWinterization(); 
  else startWinterization();
  server.send(200,"application/json","{\"ok\":true}");
}

void handleNotFound() {
  if (currentMode == OP_AP) {
    if (AP_CONFIG_REQUIRE_AUTH && !checkWebAuthOnly()) return;
    server.send(200,"text/html; charset=utf-8", buildApConfigHtml("")); 
    return;
  }
  server.send(404,"text/plain","Not found");
}

void setupWebRoutes() {
  server.on("/",             HTTP_GET,  handleRoot);
  server.on("/queue",        HTTP_GET,  handleQueue);
  server.on("/logs",         HTTP_GET,  handleLogs);
  server.on("/plans",        HTTP_GET,  handlePlans);
  server.on("/config",       HTTP_GET,  handleConfig);
  server.on("/winter",       HTTP_GET,  handleWinterPage);
  server.on("/setwifi",      HTTP_POST, handleSetWifi);
  server.on("/setnames",     HTTP_POST, handleSetNames);
  server.on("/setrelay",     HTTP_POST, handleSetRelay);
  server.on("/token/add",    HTTP_POST, handleTokenAdd);
  server.on("/token/delete", HTTP_POST, handleTokenDelete);
  server.on("/start",        HTTP_POST, handleStart);
  server.on("/stop",         HTTP_POST, handleStop);
  server.on("/stopall",      HTTP_POST, handleStopAll);
  server.on("/queue/clear",  HTTP_POST, handleQueueClear);
  server.on("/logs/download",HTTP_GET,  handleLogDownload);
  server.on("/logs/preview", HTTP_GET,  handleLogPreview);
  server.on("/logs/clear",   HTTP_POST, handleLogClear);
  server.on("/mqtt/config",  HTTP_POST, handleMqttConfig);
  server.on("/winter",       HTTP_POST, handleWinter);
  server.on("/setgroup",     HTTP_POST, handleSetGroup);
  server.on("/cleargroup",   HTTP_POST, handleClearGroup);
  server.on("/export",       HTTP_GET,  handleExport);
  server.on("/import",       HTTP_POST, handleImport);
  server.on("/api/status",   HTTP_GET,  handleApiStatus);
  server.on("/api/start",    HTTP_POST, handleApiStart);
  server.on("/api/stop",     HTTP_POST, handleApiStop);
  server.on("/api/winter",   HTTP_POST, handleApiWinter);
  server.onNotFound(handleNotFound);
  const char* hkeys[] = { "Authorization" };
  server.collectHeaders(hkeys, 1);
}

// ============================================================
// ========================= BUTTON ===========================
// ============================================================

void handleButton() {
  bool pressed = (digitalRead(PIN_AP_BUTTON) == LOW);
  
  if (pressed) {
    lastButtonPress = millis();
    
    if (buttonPressedSince == 0) {
      buttonPressedSince = millis();
      Serial.println("[BTN] BOOT-Button gedrückt");
    } 
    else if (currentMode != OP_AP && millis() - buttonPressedSince > AP_BUTTON_HOLD_MS) {
      Serial.println("[BTN] Long-Press -> AP-Modus");
      stopWinterization(); 
      stopAllChannels(); 
      clearQueue();
      wifiStartAP(); 
      showQRCodeForAP();
    }
  } else {
    buttonPressedSince = 0;
  }
}

// ============================================================
// ===================== SETUP / LOOP =========================
// ============================================================

void setup() {
  Serial.begin(115200); 
  delay(200);
  Serial.println("\n=== ESP32 Bewässerungssteuerung ===");
  Serial.println("[INFO] WiFi Reconnect: 10s | Signal-Qualität | OLED Brightness");
  #if EXTERN_BRIGHTNESS_BUTTON
    Serial.println("[INFO] Externer Helligkeit-Taster: AKTIVIERT (GPIO " + String(PIN_BRIGHTNESS_BUTTON) + ")");
  #else
    Serial.println("[INFO] Externer Helligkeit-Taster: DEAKTIVIERT");
  #endif
  
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(CHANNEL_PINS[i], RELAY_ACTIVE_LOW_DEFAULT ? HIGH : LOW);
    pinMode(CHANNEL_PINS[i], OUTPUT);
    digitalWrite(CHANNEL_PINS[i], RELAY_ACTIVE_LOW_DEFAULT ? HIGH : LOW);
    channels[i] = { false, 0, 0, 0 };
  }
  pinMode(PIN_AP_BUTTON, INPUT_PULLUP);
  
  #if EXTERN_BRIGHTNESS_BUTTON
    pinMode(PIN_BRIGHTNESS_BUTTON, INPUT_PULLUP);
  #endif

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] Init fehlgeschlagen");
  } else {
    display.ssd1306_command(0x81);
    display.ssd1306_command(BRIGHTNESS_NORMAL);
    lastButtonPress = millis();
    display.clearDisplay(); 
    display.setTextSize(1); 
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0); 
    display.println("Bewaesserung"); 
    display.println("startet..."); 
    display.display();
    Serial.println("[OLED] OK, Helligkeit: 100%");
  }

  initRTC();
  initLogging();
  delay(100);
  logEvent(0, "SYSTEM_START", 0, "system");

  loadWifiCreds();
  loadChannelNames();
  loadApiTokens();
  loadGroups();
  loadRelayPolarity();
  loadMqttConfig();

  wifiBeginSTA();
  if (WiFi.status() == WL_CONNECTED) initTime();

  setupWebRoutes();
  server.begin();
  Serial.println("[HTTP] Server gestartet");
}

void loop() {
  if (currentMode == OP_AP) dnsServer.processNextRequest();
  server.handleClient();

  if (winterActive) processWinterization();
  else { processQueue(); checkGroups(); }

  handleButton();
  
  #if EXTERN_BRIGHTNESS_BUTTON
    handleExternalBrightnessButton();
  #endif
  
  maintainWifi();
  maintainMqtt();
  syncRtcFromNtp();

  if (!ntpSynced && WiFi.status() == WL_CONNECTED) {
    struct tm ti;
    if (getLocalTime(&ti, 50)) { 
      ntpSynced = true; 
      Serial.println("[NTP] späte Sync ok"); 
    }
  }
  
  if (millis() - lastDisplayUpdate > DISPLAY_REFRESH_MS) {
    lastDisplayUpdate = millis();
    manageBrightness();
    updateStatusDisplay();
  }
  
  delay(5);
}