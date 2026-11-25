# ESP32 Automated Greenhouse

A batteries-included ESP32 greenhouse controller: live sensor dashboard on an OLED,
a phone app served from the ESP32 itself (no cloud, no install), timed pump control,
and a standalone sensor-test sketch for bring-up.

> **Current bench status:** the sensors, OLED, web app and ESP32 output are working.
> The pump/relay load side is still being debugged — see
> [Relay status](#relay-status-read-this-first) before using it unattended.

---

## Contents

```
firmware/
  sensor_test/sensor_test.ino   <- run this first
  greenhouse/
    greenhouse.ino              <- final firmware
    config.h                    <- pins, output mode, WiFi defaults
    web_app.h                   <- phone UI (HTML/CSS/JS)
DEBUG_LOG.md                    <- build notes and relay debugging log
```

---

## 1. Quick start

### What you need

- ESP32 DevKit (30-pin), USB cable
- SSD1306 128x64 I2C OLED
- DHT11, HW-080 soil probe, LDR module
- 5V relay module, 5V boost, battery + solar/charger
- Pump, 5V supply capable of the pump's startup current

### Software

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (or `arduino-cli`).
2. Install libraries: **DHT sensor library**, **Adafruit GFX**, **Adafruit SSD1306**.
3. Install the **ESP32 boards package** (`esp32:esp32:esp32`).
4. Open `firmware/sensor_test/sensor_test.ino`, select board/port, upload, and check
   the Serial Monitor at **115200** baud (see [section 2](#2-sensor-test-sketch)).
5. Open `firmware/greenhouse/greenhouse.ino`, upload, then use the phone app.

### First boot

- The ESP32 tries saved WiFi for ~15 s. If it cannot connect, it starts an open AP
  called **`GREENHOUSE-SETUP`** at **`http://192.168.4.1`**.
- Join that network with your phone and open `192.168.4.1`. The app loads from the
  ESP32 — no internet required.
- In **Settings & calibration**, enter your home WiFi and tap **Save & restart**.
- Afterwards use **`http://greenhouse.local`** or the IP printed in Serial Monitor.

---

## 2. Sensor test sketch

Upload `firmware/sensor_test/sensor_test.ino` first. Serial Monitor at 115200 shows:

```
--- I2C scan ---
  device at 0x3C
OLED ready at 0x3C
T=26.4C  H=58%  SoilRAW=2890  LightRAW=1740  Btn=----  Sw=AUTO
```

| Check | Good result | If not |
|---|---|---|
| OLED | `device at 0x3C`; something on screen | Try 0x3D; recheck SDA/SCL (21/22), 3V3, GND |
| DHT11 | T/H change when warmed/breathed on | Bare sensor needs 10k from DATA to 3V3 |
| Soil RAW | Big change between dry air and water | Check AO on GPIO34 and module 3V3/GND |
| Light RAW | Changes when covered / torch used | Check AO on GPIO35; enable inversion in app if backwards |
| Button | `BUTTON: PRESSED` when pressed | Check GPIO25 to GND |
| Switch | `Sw=AUTO` when GPIO33 is shorted to GND | Check GPIO33 wiring |
| Relay | Keep `RELAY_TEST 0` with motors attached | Set to 1 only with loads disconnected |

### Calibrating soil moisture

1. Probe in **dry air** → note `SoilRAW`.
2. Probe in **water** at planting depth → note `SoilRAW`.
3. Enter those numbers in the app's `DRY` / `WET` fields, or use the calibrate
   buttons while the probe is in each state.

Known-good values from this build: `dry=4095`, `wet=1902`.
Light sensor is inverted here (covered = 4095, torch = 0).

---

## 3. Wiring

### Signal pins

| Part | Module pin | ESP32 pin | Notes |
|---|---|---|---|
| DHT11 | VCC / DATA / GND | 3V3 / GPIO4 / GND | 10k pull-up for bare 4-pin sensor |
| HW-080 soil | VCC / AO / GND | 3V3 / GPIO34 / GND | ADC1; works with WiFi |
| LDR module | VCC / AO / GND | 3V3 / GPIO35 / GND | ADC1 |
| OLED SSD1306 | VCC / SDA / SCL / GND | 3V3 / 21 / 22 / GND | I2C address 0x3C |
| Pump button | one side / other side | GPIO25 / GND | Internal pull-up |
| AUTO/MANUAL switch | one side / other side | GPIO33 / GND | Closed = AUTO |
| Relay **IN** | IN | **GPIO27** | Single-channel module |
| Relay board | VCC / GND | 5V rail / GND | **Not 3V3**; all grounds common |

GPIO34/35 are ADC1 and remain usable while WiFi is active.

### Load side

```
5V (+)  --> relay COM
relay NO --> pump (+)
pump (-) --> supply (-) / GND
```

Add a diode across the motor (1N4007, stripe toward `+`) and, if it is noisy, a
100nF capacitor across the motor terminals. Never power the pump from an ESP32 pin.

### Power

```
Solar --> charge controller --> 18650 --> switch --> 5V boost
                                                      |  |  |
                                                ESP32 VIN  relay VCC  pump via relay
```

- 5–6V panel: TP4056 **with protection** is OK. 12V panel: real PWM controller.
- Never connect a panel directly to a battery.
- Join **all** grounds: ESP32, sensors, relay board, boost and battery.
- Fuse the battery output; keep electronics sealed above pot level.

---

## 4. Using the firmware

The OLED pages every 6 s; long-press the button to cycle manually.

| Page | Shows |
|---|---|
| 1/4 OVERVIEW | Temperature, humidity, soil/light bars, pump/mode |
| 2/4 WATER / SOIL | Moisture vs target, raw/dry/wet, watering or soak countdown |
| 3/4 CLIMATE | Fan limits (shown as `--` while fan is unused), light, mode |
| 4/4 NETWORK | IP, WiFi/AP mode, signal, uptime |

- **AUTO**: short press = one timed watering cycle. GPIO33 switch sets the mode;
  the app can override until the switch is moved again.
- **MANUAL**: short press toggles the pump. It still stops after the run time.
- The fan is disabled in this build (`USE_FAN 0`) and hidden in the app.

### Phone app

Open `http://greenhouse.local` (or the IP in Serial Monitor), then use
**Share → Add to Home Screen** for an app-like icon.

- Dashboard: live readings, pump state, mode, IP, signal; refreshes every 2 s.
- **Settings & calibration**: targets, pump run/soak times, dry/wet values,
  invert relay, invert light, bright-light watering block, WiFi setup.

### Serial commands (115200 baud)

| Key | Action |
|---|---|
| `p` | Toggle pump for its timed run (8 s by default) |
| `i` | Toggle relay polarity (saved) |
| `l` | Toggle light sensor inversion (saved) |
| `C<dry> <wet>` | e.g. `C4095 1902` |
| `s` | Print full status |

A `STATUS` line is printed every 5 s, e.g.
`STATUS soilRaw=4095 lightRaw=0 soil%=0 light%=100 T=23.7 H=57 pump=0 relayInv=0 IN1=1 mode=MANUAL`.

### One-scan setup (WiFi QR + captive portal)

A QR code can hold **either** WiFi credentials **or** a URL, not both in one standard
code. This project uses the next best thing: scan one WiFi QR, the phone joins the
ESP32's setup AP, and the firmware's **captive portal** automatically opens the web
app — no second scan needed.

1. Print or display **`qr/wifi-setup.svg`** (it contains
   `WIFI:T:nopass;S:GREENHOUSE-SETUP;;`).
2. Scan it with your phone's camera. It joins the open `GREENHOUSE-SETUP` network.
3. Within a second or two the phone should detect the portal and open
   `http://192.168.4.1/` by itself. If it doesn't, open that address manually or
   scan **`qr/webapp-url.svg`**.
4. Use **Share → Add to Home Screen**, then set your home WiFi in the app.

Notes:

- The captive portal uses the wildcard DNS server plus redirects for Android/Windows
  (`/generate_204`, `/connecttest.txt`, `/ncsi.txt`) and Apple
  (`/hotspot-detect.html`). It only works while the ESP32 is in AP mode.
- After you save home WiFi, use `http://greenhouse.local` instead; the setup AP is gone.
- Some phones are fussy about open-network QR codes. If yours won't join, set a
  password in `WIFI_AP_PASS` in `config.h` and regenerate the QR as
  `WIFI:T:WPA;S:GREENHOUSE-SETUP;P:yourpassword;;`.

### HTTP API

| Endpoint | Purpose |
|---|---|
| `GET /api/state` | Readings, states and settings as JSON |
| `GET /api/command?cmd=pump_on\|pump_off\|pump_toggle\|water_now\|auto\|manual\|screen` | Control |
| `GET /api/settings?moist=35&pumpSec=8&soakMin=5&dry=4095&wet=1902&invert=0` | Update settings |
| `GET /api/wifi?ssid=NAME&pass=PASSWORD` | Save WiFi and reboot |
| `GET /api/scan` | Scan nearby networks |

---

## 5. Control logic

- **Watering (AUTO)**: when soil is below target and the soak timer has expired,
  run the pump for `pumpMs`, then rest for `soakMs` before checking again.
- Optional bright-light block skips watering while light is above the set %.
- Every output is timed or hysteresis-controlled so a stuck sensor cannot leave
  the pump on forever. Add a float switch if the tank can run dry.

---

## 6. Troubleshooting

| Symptom | Fix |
|---|---|
| OLED blank | Check boot I2C scan (0x3C/0x3D), SDA/SCL 21/22, 3V3 and GND |
| DHT11 `--` | Wait 2.5 s between reads; bare sensor needs 10k pull-up |
| Soil never 100% | Re-calibrate wet value; check probe depth |
| Phone cannot find `greenhouse.local` | Use the IP from Serial or the setup AP at 192.168.4.1 |
| ESP32 resets when pump starts | Bigger boost; 470–1000µF on 5V rail; diode across motor |
| Pump runs at boot | Toggle **Invert relay logic** in the app or press `i` |
| Pump stays on regardless of `i` | See below |

### Relay status (read this first)

Bench findings so far:

- This is a **single-channel** relay. IN is on **GPIO27**, load on COM/NO.
- Common ground between ESP32, relay and pump supply is confirmed.
- The ESP32 pin switches correctly (`IN1` alternates 0/1 in the status line).
- The pump still runs with **no ESP32 power**, with **IN unplugged**, and in both
  HIGH and LOW states. It only stops when the pump `+` wire is removed.
- Only connecting/disconnecting the IN wire produces a relay click.

That points to the load being on the **normally closed** side of the relay, or to
a fault in the relay module. Before relying on automation:

1. Power the relay from 5V with the ESP32 unplugged.
2. Move the pump `+` to the **other output terminal** (NO ↔ NC) and confirm the
   pump is **off** when the relay has no power.
3. With the ESP32 connected, send `p`: the relay must click and the pump must run
   for 8 s, then stop.
4. If neither output terminal behaves correctly, replace the relay module or add
   a logic-level transistor driver (NPN + 1k base resistor + 10k pull-up to 5V).

Full history is in [`DEBUG_LOG.md`](DEBUG_LOG.md).

---

## License

Open source — use and modify at your own risk. Mains-powered pumps and water near
electronics are hazardous; add fusing, isolation and a float switch.
