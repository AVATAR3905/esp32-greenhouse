/*
 * ESP32 Greenhouse Controller - main firmware
 * ---------------------------------------------------------------------------
 * Sensors : DHT11 (GPIO4), HW-080 soil (GPIO34), LDR (GPIO35)
 * Outputs : 2-channel relay -> pump (GPIO26), fan (GPIO27)
 * Inputs  : pump button (GPIO25), AUTO/MANUAL switch (GPIO33)
 * Display : 128x64 SSD1306 OLED on I2C GPIO21/22 (0x3C or 0x3D)
 * Phone   : http://greenhouse.local  or  http://192.168.4.1 in setup mode
 *
 * Libraries needed: DHT sensor library, Adafruit GFX, Adafruit SSD1306
 * (WiFi, WebServer, DNSServer, ESPmDNS, Preferences are built into the ESP32 core)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#include "config.h"
#include "web_app.h"

#define OLED_W 128
#define OLED_H 64

// ---------------------------------------------------------------------------
// Hardware objects
// ---------------------------------------------------------------------------
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

// ---------------------------------------------------------------------------
// Settings (stored in flash, editable from the phone app)
// ---------------------------------------------------------------------------
struct Settings {
  int   soilDry;        // raw ADC when probe is in dry air
  int   soilWet;        // raw ADC when probe is in water
  int   moist;          // water below this %
  int   pumpMs;         // max pump run time
  int   soakMs;         // wait before checking soil again
  float tempOn, tempOff;
  float humOn, humOff;
  bool  lightBlock;     // skip watering in very bright light
  int   lightBlockPct;
  bool  ldrInvert;
  bool  relayActiveLow;  // true = IN LOW switches the relay ON (most boards)
};
Settings cfg;

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------
float temperature = NAN;
float humidity    = NAN;
int   soilRaw     = 0;
int   soilPct     = 0;
int   lightRaw    = 0;
int   lightPct    = 0;

bool autoMode      = true;
bool fanOn         = false;
bool pumpOn        = false;
unsigned long pumpUntil    = 0;
unsigned long lastWaterEnd = 0;

unsigned long lastSensorMs = 0;
unsigned long lastDhtMs    = 0;
unsigned long lastUiMs     = 0;
unsigned long lastScreenMs = 0;
unsigned long lastBeatMs = 0;
unsigned long lastStatusMs = 0;

int  screen      = 0;
const int SCREEN_COUNT = 4;
bool oledReady   = false;
bool apMode      = false;
String ipStr     = "0.0.0.0";
String currentSsid = "";

// Button debounce + long-press
bool btnLastRaw = false, btnStable = false;
unsigned long btnChanged = 0, btnPressedAt = 0;
int modeLastRaw = -1;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void relayWrite(int pin, bool on) {
  bool level = cfg.relayActiveLow ? !on : on;
  digitalWrite(pin, level ? HIGH : LOW);
}

int rawToPercent(int raw, int dry, int wet) {
  if (dry == wet) return 0;
  long v = (long)(raw - dry) * 100L / (long)(wet - dry);
  return (int)constrain(v, 0L, 100L);
}

const char *fanLabel() {
#if USE_FAN
  return fanOn ? "ON" : "OFF";
#else
  return "--";
#endif
}

unsigned long remainSeconds() {
  unsigned long now = millis();
  if (pumpOn) return (pumpUntil > now) ? (pumpUntil - now) / 1000UL : 0UL;
  unsigned long since = now - lastWaterEnd;
  return (since < (unsigned long)cfg.soakMs) ? ((unsigned long)cfg.soakMs - since) / 1000UL : 0UL;
}

String numJson(float v, int digits) {
  if (isnan(v) || isinf(v)) return F("null");
  return String(v, digits);
}

String escJson(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += F("\\n");
    else if (c == '\r') o += F("\\r");
    else o += c;
  }
  return o;
}

void startPump(int ms) {
  if (ms < 100) ms = 100;
  pumpOn    = true;
  pumpUntil = millis() + (unsigned long)ms;
}
void stopPump() {
  pumpOn        = false;
  pumpUntil     = 0;
  lastWaterEnd  = millis();
}

// ---------------------------------------------------------------------------
// Setup / load / save
// ---------------------------------------------------------------------------
void loadSettings() {
  cfg.soilDry      = prefs.getInt("soilDry", 3200);
  cfg.soilWet      = prefs.getInt("soilWet", 1400);
  cfg.moist        = prefs.getInt("moist", 35);
  cfg.pumpMs       = prefs.getInt("pumpMs", 8000);
  cfg.soakMs       = prefs.getInt("soakMs", 300000);
  cfg.tempOn       = prefs.getFloat("tempOn", 30.0f);
  cfg.tempOff      = prefs.getFloat("tempOff", 27.0f);
  cfg.humOn        = prefs.getFloat("humOn", 80.0f);
  cfg.humOff       = prefs.getFloat("humOff", 72.0f);
  cfg.lightBlock   = prefs.getBool("lightBlock", false);
  cfg.lightBlockPct= prefs.getInt("lightBlockPct", 80);
  cfg.ldrInvert    = prefs.getBool("ldrInvert", false);
  cfg.relayActiveLow = prefs.getBool("relayActiveLow", RELAY_ACTIVE_LOW);
  if (cfg.soakMs < 1000) cfg.soakMs = 300000;  // never allow rapid re-triggering
}

void saveSettings() {
  prefs.putInt("soilDry", cfg.soilDry);
  prefs.putInt("soilWet", cfg.soilWet);
  prefs.putInt("moist", cfg.moist);
  prefs.putInt("pumpMs", cfg.pumpMs);
  prefs.putInt("soakMs", cfg.soakMs);
  prefs.putFloat("tempOn", cfg.tempOn);
  prefs.putFloat("tempOff", cfg.tempOff);
  prefs.putFloat("humOn", cfg.humOn);
  prefs.putFloat("humOff", cfg.humOff);
  prefs.putBool("lightBlock", cfg.lightBlock);
  prefs.putInt("lightBlockPct", cfg.lightBlockPct);
  prefs.putBool("ldrInvert", cfg.ldrInvert);
  prefs.putBool("relayActiveLow", cfg.relayActiveLow);
}

bool connectWiFi() {
  String ssid = prefs.getString("ssid", WIFI_SSID);
  String pass = prefs.getString("pass", WIFI_PASS);
  currentSsid = ssid;
  if (ssid.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000UL) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    ipStr = WiFi.localIP().toString();
    Serial.print(F("WiFi connected: ")); Serial.println(ipStr);
    return true;
  }
  Serial.println(F("WiFi failed - opening setup access point"));
  return false;
}

void scanI2C() {
  Serial.println(F("--- I2C scan ---"));
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device at 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) Serial.println(F("  NO I2C DEVICE - check OLED VCC/GND/SDA/SCL"));
}

void printStatus() {
  Serial.print(F("STATUS soilRaw=")); Serial.print(soilRaw);
  Serial.print(F(" lightRaw=")); Serial.print(lightRaw);
  Serial.print(F(" soil%=")); Serial.print(soilPct);
  Serial.print(F(" light%=")); Serial.print(lightPct);
  Serial.print(F(" T=")); Serial.print(temperature, 1);
  Serial.print(F(" H=")); Serial.print(humidity, 0);
  Serial.print(F(" pump=")); Serial.print(pumpOn ? 1 : 0);
  Serial.print(F(" fan=")); Serial.print(fanOn ? 1 : 0);
  Serial.print(F(" relayInv=")); Serial.print(cfg.relayActiveLow ? 0 : 1);
  Serial.print(F(" IN1=")); Serial.print(digitalRead(PUMP_PIN));
  Serial.print(F(" mode=")); Serial.println(autoMode ? F("AUTO") : F("MANUAL"));
}

// Serial commands: i = invert relay, p = toggle pump, s = print status
void handleSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == 'i') {
      cfg.relayActiveLow = !cfg.relayActiveLow;
      saveSettings();
      Serial.print(F("Relay inverted = ")); Serial.println(cfg.relayActiveLow ? 0 : 1);
      printStatus();
    } else if (c == 'p') {
      if (pumpOn) stopPump(); else startPump(cfg.pumpMs);
      printStatus();
    } else if (c == 's') {
      printStatus();
    } else if (c == 'l') {
      cfg.ldrInvert = !cfg.ldrInvert;
      saveSettings();
      readSensors();
      Serial.print(F("Light invert = ")); Serial.println(cfg.ldrInvert ? 1 : 0);
      printStatus();
    } else if (c == 'C') {  // C<dry> <wet>  e.g. C4095 1902
      long d = Serial.parseInt();
      long w = Serial.parseInt();
      while (Serial.available() > 0) Serial.read();
      if (d >= 0 && d <= 4095 && w >= 0 && w <= 4095 && d != w) {
        cfg.soilDry = (int)d;
        cfg.soilWet = (int)w;
        saveSettings();
        readSensors();
        Serial.print(F("Calibration saved dry=")); Serial.print(cfg.soilDry);
        Serial.print(F(" wet=")); Serial.println(cfg.soilWet);
      } else {
        Serial.println(F("Bad calibration values"));
      }
      printStatus();
    }
  }
}

void startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  IPAddress apIP = WiFi.softAPIP();
  ipStr = apIP.toString();
  dnsServer.start(53, "*", apIP);
  Serial.print(F("AP mode: ")); Serial.println(ipStr);
}

// ---------------------------------------------------------------------------
// Sensors and control
// ---------------------------------------------------------------------------
void readSensors() {
  if (SOIL_PWR_PIN >= 0) {
    pinMode(SOIL_PWR_PIN, OUTPUT);
    digitalWrite(SOIL_PWR_PIN, HIGH);
    delay(30);
  }
  soilRaw = analogRead(SOIL_PIN);
  if (SOIL_PWR_PIN >= 0) digitalWrite(SOIL_PWR_PIN, LOW);

  lightRaw = analogRead(LDR_PIN);
  soilPct  = rawToPercent(soilRaw, cfg.soilDry, cfg.soilWet);
  int p    = (int)((long)lightRaw * 100L / 4095L);
  lightPct = cfg.ldrInvert ? 100 - p : p;
}

void updateControl() {
  // Fan: hysteresis on both temperature and humidity (AUTO only)
#if USE_FAN
  if (autoMode) {
    if (!isnan(temperature)) {
      if (temperature >= cfg.tempOn)  fanOn = true;
      else if (temperature <= cfg.tempOff) fanOn = false;
    }
    if (!isnan(humidity)) {
      if (humidity >= cfg.humOn)  fanOn = true;
      else if (humidity <= cfg.humOff) fanOn = false;
    }
  }
#endif

  // Pump: soil below target, rested long enough, not blocked by bright light
  if (autoMode && !pumpOn && (millis() - lastWaterEnd >= (unsigned long)cfg.soakMs)) {
    bool brightBlock = cfg.lightBlock && (lightPct >= cfg.lightBlockPct);
    if (soilPct < cfg.moist && !brightBlock) startPump(cfg.pumpMs);
  }

  // Non-blocking pump timeout
  if (pumpOn && millis() >= pumpUntil) stopPump();

  relayWrite(PUMP_PIN, pumpOn);
#if USE_FAN
  relayWrite(FAN_PIN,  fanOn);
#endif
}

void handleButton() {
  bool raw = (digitalRead(BTN_PIN) == LOW);
  if (raw != btnLastRaw) { btnLastRaw = raw; btnChanged = millis(); }
  if (millis() - btnChanged < 45UL) return;

  if (raw != btnStable) {
    btnStable = raw;
    if (raw) {
      btnPressedAt = millis();
    } else {
      unsigned long held = millis() - btnPressedAt;
      if (held > 800UL) {                 // long press: cycle OLED page
        screen = (screen + 1) % SCREEN_COUNT;
        lastScreenMs = millis();
      } else if (autoMode) {              // short press in AUTO: one-shot watering
        startPump(cfg.pumpMs);
      } else {                            // short press in MANUAL: latch on/off
        if (pumpOn) stopPump(); else startPump(cfg.pumpMs);
      }
    }
  }
}

void handleModeSwitch() {
  int raw = digitalRead(MODE_PIN);
  if (raw != modeLastRaw) {
    modeLastRaw = raw;
    autoMode = (raw == LOW);
    Serial.print(F("Mode: ")); Serial.println(autoMode ? F("AUTO") : F("MANUAL"));
  }
}

// ---------------------------------------------------------------------------
// OLED UI
// ---------------------------------------------------------------------------
void uiHead(const String &title) {
  oled.fillRect(0, 0, OLED_W, 11, WHITE);
  oled.setTextColor(BLACK);
  oled.setTextSize(1);
  oled.setCursor(3, 2);
  oled.print(title);
  oled.setCursor(105, 2);
  oled.print(String(screen + 1) + "/" + String(SCREEN_COUNT));
  oled.setTextColor(WHITE);
}
void uiText(int x, int y, const String &s, int size = 1) {
  oled.setTextSize(size);
  oled.setCursor(x, y);
  oled.print(s);
}
void uiBar(int x, int y, int w, int h, int pct) {
  oled.drawRect(x, y, w, h, WHITE);
  int inner = w - 4;
  if (inner < 1) inner = 1;
  int fill = (int)((long)inner * constrain(pct, 0, 100) / 100L);
  if (fill > 0) oled.fillRect(x + 2, y + 2, fill, h - 4, WHITE);
}
void uiFoot(const String &s) {
  oled.drawLine(0, 54, OLED_W - 1, 54, WHITE);
  uiText(2, 55, s);
}

void drawScreen() {
  if (!oledReady) return;
  oled.clearDisplay();

  switch (screen) {
    case 0: { // Overview
      uiHead(F("OVERVIEW"));
      uiText(2, 12, F("TEMP"));
      uiText(66, 12, F("HUMID"));
      uiText(2, 20, String(temperature, 1) + "C", 2);
      uiText(66, 20, String(humidity, 0) + "%", 2);
      uiText(2, 36, F("SOIL"));
      uiBar(32, 35, 58, 9, soilPct);
      uiText(96, 36, String(soilPct) + "%");
      uiText(2, 46, F("LIGHT"));
      uiBar(38, 45, 52, 9, lightPct);
      uiText(96, 46, String(lightPct) + "%");
      uiFoot(String("P:") + (pumpOn ? "ON " : "OFF") + " F:" + fanLabel() +
             " " + (autoMode ? "AUTO" : "MAN"));
      break;
    }
    case 1: { // Water / soil
      uiHead(F("WATER / SOIL"));
      uiText(2, 13, String("MOIST ") + soilPct + "%  GOAL " + cfg.moist + "%");
      uiBar(2, 22, 124, 11, soilPct);
      uiText(2, 34, String("RAW ") + soilRaw + "   DRY " + cfg.soilDry);
      uiText(2, 43, String("WET ") + cfg.soilWet + "   SOAK " + (cfg.soakMs / 60000));
      uiFoot(pumpOn ? String("WATERING ") + remainSeconds() + "s"
                    : String("IDLE NEXT ") + remainSeconds() + "s");
      break;
    }
    case 2: { // Climate / fan
      uiHead(F("CLIMATE / FAN"));
      uiText(2, 14, String("T ") + String(temperature, 1) + "C   FAN " + fanLabel());
      uiText(2, 24, String("  ON >") + String(cfg.tempOn, 0) + "  OFF <" + String(cfg.tempOff, 0));
      uiText(2, 36, String("H ") + String(humidity, 0) + "%   LIGHT " + lightPct + "%");
      uiText(2, 46, String("  ON >") + String(cfg.humOn, 0) + "  OFF <" + String(cfg.humOff, 0));
      uiFoot(String("MODE ") + (autoMode ? "AUTO" : "MANUAL") + "  SOIL " + soilPct + "%");
      break;
    }
    case 3: { // Network
      uiHead(F("NETWORK"));
      uiText(2, 14, String("IP ") + ipStr);
      uiText(2, 25, String("MODE ") + (apMode ? "AP SETUP" : "WIFI"));
      if (apMode) uiText(2, 36, F("RSSI  -- dBm"));
      else uiText(2, 36, String("RSSI ") + WiFi.RSSI() + " dBm");
      uiText(2, 45, String("UP ") + (millis() / 1000UL) + " s");
      uiFoot(F("greenhouse.local"));
      break;
    }
  }
  oled.display();
}

// ---------------------------------------------------------------------------
// Web API
// ---------------------------------------------------------------------------
void sendState() {
  String j;
  j.reserve(700);
  j += F("{\"ok\":true,\"t\":");   j += numJson(temperature, 1);
  j += F(",\"h\":");               j += numJson(humidity, 0);
  j += F(",\"soil\":");            j += soilPct;
  j += F(",\"soilRaw\":");         j += soilRaw;
  j += F(",\"light\":");           j += lightPct;
  j += F(",\"lightRaw\":");        j += lightRaw;
  j += F(",\"pump\":");            j += pumpOn ? F("true") : F("false");
  j += F(",\"fan\":");             j += fanOn ? F("true") : F("false");
  j += F(",\"fanEnabled\":");      j += USE_FAN ? F("true") : F("false");
  j += F(",\"mode\":\"");          j += autoMode ? F("AUTO") : F("MANUAL"); j += '"';
  j += F(",\"ap\":");              j += apMode ? F("true") : F("false");
  j += F(",\"ip\":\"");            j += escJson(ipStr); j += '"';
  j += F(",\"ssid\":\"");          j += escJson(currentSsid); j += '"';
  j += F(",\"rssi\":");
  if (apMode || WiFi.status() != WL_CONNECTED) j += F("null");
  else j += WiFi.RSSI();
  j += F(",\"remain\":");          j += remainSeconds();
  j += F(",\"up\":");              j += millis() / 1000UL;
  j += F(",\"cfg\":{");
  j += F("\"moist\":");            j += cfg.moist;
  j += F(",\"pumpMs\":");          j += cfg.pumpMs;
  j += F(",\"soakMs\":");          j += cfg.soakMs;
  j += F(",\"tempOn\":");          j += String(cfg.tempOn, 1);
  j += F(",\"tempOff\":");         j += String(cfg.tempOff, 1);
  j += F(",\"humOn\":");           j += String(cfg.humOn, 0);
  j += F(",\"humOff\":");          j += String(cfg.humOff, 0);
  j += F(",\"soilDry\":");         j += cfg.soilDry;
  j += F(",\"soilWet\":");         j += cfg.soilWet;
  j += F(",\"lightBlockPct\":");   j += cfg.lightBlockPct;
  j += F(",\"lightBlock\":");      j += cfg.lightBlock ? F("true") : F("false");
  j += F(",\"ldrInvert\":");       j += cfg.ldrInvert ? F("true") : F("false");
  j += F(",\"relayActiveLow\":");  j += cfg.relayActiveLow ? F("true") : F("false");
  j += F("}}");
  server.send(200, "application/json", j);
}

void handleCommand() {
  String c = server.hasArg("cmd") ? server.arg("cmd") : "";
  if      (c == "pump_on")    startPump(cfg.pumpMs);
  else if (c == "pump_off")   stopPump();
  else if (c == "pump_toggle"){ if (pumpOn) stopPump(); else startPump(cfg.pumpMs); }
#if USE_FAN
  else if (c == "fan_on")     fanOn = true;
  else if (c == "fan_off")    fanOn = false;
  else if (c == "fan_toggle") fanOn = !fanOn;
#endif
  else if (c == "auto")       autoMode = true;
  else if (c == "manual")     autoMode = false;
  else if (c == "mode_auto")  autoMode = true;
  else if (c == "mode_manual")autoMode = false;
  else if (c == "water_now")  startPump(cfg.pumpMs);
  else if (c == "screen")     { screen = (screen + 1) % SCREEN_COUNT; lastScreenMs = millis(); }
  else { server.send(400, "application/json", F("{\"ok\":false,\"err\":\"unknown command\"}")); return; }
  server.send(200, "application/json", F("{\"ok\":true}"));
}

int argInt(const char *key, int def, int lo, int hi) {
  if (!server.hasArg(key)) return def;
  return (int)constrain(server.arg(key).toInt(), (long)lo, (long)hi);
}
float argFlt(const char *key, float def, float lo, float hi) {
  if (!server.hasArg(key)) return def;
  float v = server.arg(key).toFloat();
  if (isnan(v)) return def;
  return constrain(v, lo, hi);
}

void handleSettings() {
  cfg.moist         = argInt("moist", cfg.moist, 0, 100);
  cfg.pumpMs        = argInt("pumpSec", cfg.pumpMs / 1000, 1, 120) * 1000;
  cfg.soakMs        = argInt("soakMin", cfg.soakMs / 60000, 0, 120) * 60000;
  cfg.soilDry       = argInt("dry", cfg.soilDry, 0, 4095);
  cfg.soilWet       = argInt("wet", cfg.soilWet, 0, 4095);
  cfg.lightBlockPct = argInt("lightPct", cfg.lightBlockPct, 0, 100);
  cfg.tempOn        = argFlt("tOn", cfg.tempOn, 0, 60);
  cfg.tempOff       = argFlt("tOff", cfg.tempOff, 0, 60);
  cfg.humOn         = argFlt("hOn", cfg.humOn, 0, 100);
  cfg.humOff        = argFlt("hOff", cfg.humOff, 0, 100);
  cfg.lightBlock    = server.hasArg("block") ? (server.arg("block") == "1") : false;
  cfg.ldrInvert     = server.hasArg("invert") ? (server.arg("invert") == "1") : false;
  if (server.hasArg("relayInv")) cfg.relayActiveLow = (server.arg("relayInv") != "1");
  if (cfg.tempOff > cfg.tempOn)  cfg.tempOff = cfg.tempOn;
  if (cfg.humOff  > cfg.humOn)   cfg.humOff  = cfg.humOn;
  saveSettings();
  readSensors();
  server.send(200, "application/json", F("{\"ok\":true}"));
}

void handleWifi() {
  String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  ssid.trim();
  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64) {
    server.send(400, "application/json", F("{\"ok\":false,\"err\":\"bad credentials\"}"));
    return;
  }
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  server.send(200, "application/json", F("{\"ok\":true,\"restart\":true}"));
  delay(300);
  ESP.restart();
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String j = F("{\"ok\":true,\"nets\":[");
  for (int i = 0; i < n; i++) {
    if (i) j += ',';
    j += F("{\"ssid\":\""); j += escJson(WiFi.SSID(i));
    j += F("\",\"rssi\":"); j += WiFi.RSSI(i);
    j += F(",\"secure\":"); j += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? F("true") : F("false");
    j += '}';
  }
  j += F("]}");
  WiFi.scanDelete();
  server.send(200, "application/json", j);
}

// Captive portal: phone OS probes these paths when it joins the AP. Sending a
// redirect (instead of the expected "no portal" answer) makes the OS open the
// web app automatically after the phone joins GREENHOUSE-SETUP.
void handleCaptiveRedirect() {
  server.sendHeader(F("Location"), F("/"));
  server.send(303);
}

void handleCaptivePage() {
  server.send_P(200, "text/html", WEB_APP);
}

void setupRoutes() {
  server.on(F("/"), HTTP_GET, []() { server.send_P(200, "text/html", WEB_APP); });
  server.on(F("/manifest.webmanifest"), HTTP_GET, []() { server.send_P(200, "application/manifest+json", MANIFEST); });
  server.on(F("/api/state"), HTTP_GET, sendState);
  server.on(F("/api/command"), HTTP_GET, handleCommand);
  server.on(F("/api/settings"), HTTP_GET, handleSettings);
  server.on(F("/api/wifi"), HTTP_GET, handleWifi);
  server.on(F("/api/scan"), HTTP_GET, handleScan);

  // Android / Windows / ChromeOS captive-portal probes
  server.on(F("/generate_204"), HTTP_ANY, handleCaptiveRedirect);
  server.on(F("/connecttest.txt"), HTTP_ANY, handleCaptiveRedirect);
  server.on(F("/ncsi.txt"), HTTP_ANY, handleCaptiveRedirect);
  server.on(F("/fwlink"), HTTP_ANY, handleCaptiveRedirect);
  server.on(F("/redirect"), HTTP_ANY, handleCaptiveRedirect);

  // Apple captive-portal probes
  server.on(F("/hotspot-detect.html"), HTTP_ANY, handleCaptivePage);
  server.on(F("/hotspot.html"), HTTP_ANY, handleCaptivePage);
  server.on(F("/library/test/success.html"), HTTP_ANY, handleCaptiveRedirect);

  server.onNotFound([]() { server.sendHeader(F("Location"), F("/")); server.send(303); });
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  // Load saved settings BEFORE touching the relay: relayWrite() needs the
  // stored polarity, otherwise the pump can be energised during boot.
  prefs.begin("greenhouse", false);
  loadSettings();

  // Set relay pins to their OFF level immediately to avoid a boot flicker.
#if PUMP_OPEN_DRAIN
  pinMode(PUMP_PIN, OUTPUT_OPEN_DRAIN);
#else
  pinMode(PUMP_PIN, OUTPUT);
#endif
  relayWrite(PUMP_PIN, false);
#if USE_FAN
#if PUMP_OPEN_DRAIN
  pinMode(FAN_PIN, OUTPUT_OPEN_DRAIN);
#else
  pinMode(FAN_PIN, OUTPUT);
#endif
  relayWrite(FAN_PIN, false);
#endif

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(MODE_PIN, INPUT_PULLUP);
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);   // heartbeat: blinking = sketch is running
#endif
  Serial.println(F("BOOT OK - pump relay pin forced OFF"));

  Wire.begin(OLED_SDA, OLED_SCL);
  scanI2C();
  for (uint8_t addr : {(uint8_t)0x3C, (uint8_t)0x3D}) {
    if (oled.begin(SSD1306_SWITCHCAPVCC, addr)) {
      oledReady = true;
      Serial.printf("OLED found at 0x%02X\n", addr);
      break;
    }
  }
  if (!oledReady) Serial.println(F("OLED not found - continuing without display"));

  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  dht.begin();

  if (!connectWiFi()) startAP();

  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println(F("mDNS ready: http://greenhouse.local"));
  }

  setupRoutes();
  server.begin();
  readSensors();
  lastWaterEnd = millis();         // start the soak timer at boot, not at 0
  lastDhtMs = millis() - 2500UL;   // read DHT immediately on first loop
  lastScreenMs = millis();

  Serial.println(F("Greenhouse controller ready"));
  Serial.print(F("Open http://")); Serial.print(ipStr); Serial.println(F("/ on your phone"));
}

void loop() {
  unsigned long now = millis();

  handleSerial();

#ifdef LED_BUILTIN
  if (now - lastBeatMs >= 1000UL) {
    lastBeatMs = now;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
#endif

  handleButton();
  handleModeSwitch();

  if (now - lastSensorMs >= 1000UL) { lastSensorMs = now; readSensors(); }
  if (now - lastDhtMs >= 2500UL) {
    lastDhtMs = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temperature = t;
    if (!isnan(h)) humidity = h;
  }
  if (now - lastScreenMs >= 6000UL) {
    lastScreenMs = now;
    screen = (screen + 1) % SCREEN_COUNT;
  }

  updateControl();

  if (now - lastUiMs >= 400UL) { lastUiMs = now; drawScreen(); }
  if (now - lastStatusMs >= 5000UL) { lastStatusMs = now; printStatus(); }

  server.handleClient();
  if (apMode) dnsServer.processNextRequest();
}
