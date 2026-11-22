/*
 * ESP32 Greenhouse - SENSOR TEST SKETCH
 * ---------------------------------------------------------------------------
 * Use this BEFORE loading the main firmware. It verifies every connection and
 * prints the raw numbers you need for soil-sensor calibration.
 *
 * No WiFi, no libraries beyond: Adafruit GFX + Adafruit SSD1306 + DHT sensor.
 *
 * What to look for in Serial Monitor (115200 baud):
 *   OLED ............ found at 0x3C or 0x3D
 *   DHT11 ........... temperature and humidity update, no "failed" spam
 *   Soil RAW ........ changes a lot between dry air and water
 *   Light RAW ....... changes when you cover the sensor / shine a light
 *   Button .......... "PRESSED" when you press the pump button
 *   Switch .......... AUTO when GPIO33 is shorted to GND, MANUAL otherwise
 *   Relays .......... only tested when RELAY_TEST is set to 1 (see below)
 *
 * Short press the pump button to cycle OLED pages.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// --- Set to 1 ONLY with pump/fan DISCONNECTED. Then relays click every 2 s ---
#define RELAY_TEST 0

// --- Pins (same as the main firmware) ---
#define DHTPIN        4
#define DHTTYPE       DHT11
#define SOIL_PIN      34
#define LDR_PIN       35
#define PUMP_PIN      26
#define FAN_PIN       27
#define BTN_PIN       25
#define MODE_PIN      33
#define SOIL_PWR_PIN  -1      // set to 32 if you power the probe from a GPIO
#define OLED_SDA      21
#define OLED_SCL      22

#define RELAY_ACTIVE_LOW true

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledReady = false;

float tempC = NAN, humPct = NAN;
int soilRaw = 0, lightRaw = 0;
int page = 0;
const int PAGES = 3;

bool lastRaw = false, stable = false;
unsigned long changedAt = 0, pressedAt = 0;
unsigned long lastRead = 0, lastDht = 0, lastDraw = 0, lastRelay = 0;
bool relayState = false;

void relayWrite(int pin, bool on) {
  bool level = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(pin, level ? HIGH : LOW);
}

void scanI2C() {
  Serial.println(F("--- I2C scan ---"));
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  device at 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) Serial.println(F("  NO I2C DEVICES FOUND - check SDA/SCL and OLED power"));
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== ESP32 Greenhouse sensor test ==="));

  pinMode(PUMP_PIN, OUTPUT); pinMode(FAN_PIN, OUTPUT);
  relayWrite(PUMP_PIN, false); relayWrite(FAN_PIN, false);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(MODE_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);
  scanI2C();
  for (uint8_t addr : {(uint8_t)0x3C, (uint8_t)0x3D}) {
    if (oled.begin(SSD1306_SWITCHCAPVCC, addr)) {
      oledReady = true;
      Serial.printf("OLED ready at 0x%02X\n", addr);
      break;
    }
  }
  if (!oledReady) Serial.println(F("WARNING: OLED not detected"));

  dht.begin();
  lastDht = millis() - 3000UL;

  Serial.println(F("\nWaiting for readings..."));
  Serial.println(F("--------------------------------------------------------------"));
}

void handleButton() {
  bool raw = (digitalRead(BTN_PIN) == LOW);
  if (raw != lastRaw) { lastRaw = raw; changedAt = millis(); }
  if (millis() - changedAt < 45UL) return;
  if (raw != stable) {
    stable = raw;
    if (raw) {
      pressedAt = millis();
      Serial.println(F("BUTTON: PRESSED"));
    } else {
      page = (page + 1) % PAGES;
      Serial.print(F("BUTTON: released after "));
      Serial.print(millis() - pressedAt);
      Serial.println(F(" ms -> next OLED page"));
    }
  }
}

void draw() {
  if (!oledReady) return;
  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.setTextSize(1);

  oled.fillRect(0, 0, 128, 11, WHITE);
  oled.setTextColor(BLACK);
  oled.setCursor(3, 2);
  oled.print(page == 0 ? F("SENSOR TEST") : page == 1 ? F("CALIBRATION") : F("I/O TEST"));
  oled.setCursor(104, 2);
  oled.print(String(page + 1) + "/" + String(PAGES));
  oled.setTextColor(WHITE);

  if (page == 0) {
    oled.setCursor(2, 14); oled.print(F("TEMP"));
    oled.setCursor(66, 14); oled.print(F("HUMID"));
    oled.setTextSize(2);
    oled.setCursor(2, 22); oled.print(String(tempC, 1) + "C");
    oled.setCursor(66, 22); oled.print(String(humPct, 0) + "%");
    oled.setTextSize(1);
    oled.setCursor(2, 42); oled.print(F("SOIL ")); oled.print(soilRaw);
    oled.setCursor(66, 42); oled.print(F("LIGHT ")); oled.print(lightRaw);
    oled.drawLine(0, 53, 127, 53, WHITE);
    oled.setCursor(2, 55); oled.print(F("Button -> next page"));
  } else if (page == 1) {
    oled.setCursor(2, 16); oled.print(F("SOIL RAW NOW: ")); oled.print(soilRaw);
    oled.setCursor(2, 30); oled.print(F("Dry air  = record"));
    oled.setCursor(2, 40); oled.print(F("In water = record"));
    oled.setCursor(2, 50); oled.print(F("Use values in app"));
  } else {
    bool sw = (digitalRead(MODE_PIN) == LOW);
    oled.setCursor(2, 16); oled.print(F("BUTTON  ")); oled.print(stable ? F("PRESSED") : F("released"));
    oled.setCursor(2, 28); oled.print(F("SWITCH  ")); oled.print(sw ? F("AUTO") : F("MANUAL"));
    oled.setCursor(2, 40); oled.print(F("RELAY1  ")); oled.print(relayState ? F("ON") : F("OFF"));
    oled.setCursor(2, 50); oled.print(F("RELAY2  ")); oled.print(relayState ? F("ON") : F("OFF"));
  }
  oled.display();
}

void loop() {
  unsigned long now = millis();
  handleButton();

  if (now - lastDht >= 2500UL) {
    lastDht = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) tempC = t;
    if (!isnan(h)) humPct = h;
    if (isnan(t) || isnan(h)) Serial.println(F("DHT11: read failed (check pull-up / wiring)"));
  }

  if (now - lastRead >= 1000UL) {
    lastRead = now;
    if (SOIL_PWR_PIN >= 0) {
      pinMode(SOIL_PWR_PIN, OUTPUT);
      digitalWrite(SOIL_PWR_PIN, HIGH);
      delay(30);
    }
    soilRaw = analogRead(SOIL_PIN);
    if (SOIL_PWR_PIN >= 0) digitalWrite(SOIL_PWR_PIN, LOW);
    lightRaw = analogRead(LDR_PIN);

    Serial.print(F("T="));   Serial.print(tempC, 1);
    Serial.print(F("C  H=")); Serial.print(humPct, 0);
    Serial.print(F("%  SoilRAW=")); Serial.print(soilRaw);
    Serial.print(F("  LightRAW=")); Serial.print(lightRaw);
    Serial.print(F("  Btn=")); Serial.print(digitalRead(BTN_PIN) == LOW ? F("PRESS") : F("----"));
    Serial.print(F("  Sw=")); Serial.print(digitalRead(MODE_PIN) == LOW ? F("AUTO") : F("MAN"));
    Serial.println();
  }

#if RELAY_TEST
  if (now - lastRelay >= 2000UL) {
    lastRelay = now;
    relayState = !relayState;
    relayWrite(PUMP_PIN, relayState);
    relayWrite(FAN_PIN, relayState);
    Serial.print(F("RELAY TEST -> ")); Serial.println(relayState ? F("ON") : F("OFF"));
  }
#endif

  if (now - lastDraw >= 300UL) { lastDraw = now; draw(); }
}
