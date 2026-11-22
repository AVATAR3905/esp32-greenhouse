#pragma once

// ---------------------------------------------------------------------------
// Pin map -- matches the wiring in README.md
// ---------------------------------------------------------------------------
#define DHTPIN        4      // DHT11 data
#define DHTTYPE       DHT11
#define SOIL_PIN      34     // HW-080 AO (ADC1)
#define LDR_PIN       35     // light sensor AO (ADC1)
#define PUMP_PIN      27     // relay IN (single-channel module)
#define FAN_PIN       26     // spare; unused while USE_FAN is 0
#define BTN_PIN       25     // pump push button (INPUT_PULLUP)
#define MODE_PIN      33     // AUTO / MANUAL slide switch (INPUT_PULLUP)

#define SOIL_PWR_PIN  -1     // -1 = soil sensor permanently on 3V3.
                             // Set to 32 and move soil VCC to GPIO32 to
                             // power the probe only while reading (stops corrosion).

#define OLED_SDA      21
#define OLED_SCL      22

// Open-drain: the module's own pull-up raises IN to 5V when the pin releases.
// This is needed because 3.3V from the ESP32 is not enough to switch this board off.
#define PUMP_OPEN_DRAIN  1

#define RELAY_ACTIVE_LOW  true   // LOW = relay ON, HIGH (5V via pull-up) = OFF

// Set to 0 while only the pump is connected. Change back to 1 when you add the fan.
#define USE_FAN  0

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
#define WIFI_AP_SSID   "GREENHOUSE-SETUP"
#define WIFI_AP_PASS   ""        // open AP; set a password if you prefer
#define MDNS_NAME      "greenhouse"

// Override these from the Arduino IDE by editing this file, or leave blank
// and set them later from the phone app (stored in Preferences).
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif
