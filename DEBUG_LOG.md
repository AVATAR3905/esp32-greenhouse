# Greenhouse build — session log and current status

This file records what we have built, tested and changed while bringing the
greenhouse controller up on your bench.

## Project files

| File | Purpose |
|---|---|
| `README.md` | Original wiring, calibration and phone-app guide |
| `firmware/sensor_test/sensor_test.ino` | Bench test for sensors and I2C |
| `firmware/greenhouse/greenhouse.ino` | Main firmware (OLED, web app, control logic) |
| `firmware/greenhouse/config.h` | Pin map and output-mode settings |
| `firmware/greenhouse/web_app.h` | Phone app HTML/CSS/JS served from ESP32 flash |

## What is confirmed working

These were verified from the live Serial Monitor during this session:

| Item | Evidence |
|---|---|
| ESP32 boots and runs the sketch | `BOOT OK`, heartbeat LED, `Greenhouse controller ready` |
| OLED display wiring | I2C scan found `device at 0x3C`, `OLED found at 0x3C` |
| DHT11 | `T≈23–24 °C`, `H≈56–62 %` |
| Soil sensor (HW-080) | `soilRaw=4095` in dry air, ≈1500–2000 in water |
| Soil calibration saved | dry=4095, wet=1902 → probe in water reads ≈96–100 % |
| Light sensor | Dark = 4095, torch = 0. Inversion enabled → torch now reads 100 %, covered 0 % |
| Phone app / web server | AP mode at `192.168.4.1`, mDNS `greenhouse.local` |
| Pump and 5V supply | Pump spins when connected directly to 5V and when COM↔NO is bridged |
| Code-side pump control | Sending `p` drives the pin LOW for 8 s then HIGH; state confirmed in serial |
| Common ground | ESP32 GND, battery GND and pump/motor GND are now joined |

## What is NOT working

**The relay module does not switch the pump.** The ESP32 output is correct, the
pump is good, and the load wiring has been checked, but sending `p` produces no
click and no pump motion.

Important facts discovered:

- The relay is a **single-channel** module (there is no IN2 / COM2 / NO2).
- With the pump on `NO`, it ran continuously whenever the input floated.
- With the pump on `NC`, it stayed off.
- Jumpering the input to the relay's **GND** turned the relay off.
- Both LEDs on the module were lit at one point.
- The module has **no JD-VCC jumper**, so its coil must be powered from 5V.
- It did not respond reliably to 3.3V from the ESP32, even with common ground.
- An open-drain workaround (using the module's internal pull-up to reach 5V)
  also did not make it switch correctly.

Most likely conclusion: the relay module's input stage needs a full 5V logic
signal and the ESP32 cannot provide it directly, or the module itself is faulty.

## Changes made during debugging

1. Added I2C scan to main firmware (prints OLED address at boot).
2. Added heartbeat LED and `BOOT OK` serial line.
3. Added live `STATUS` line every 5 seconds showing all readings and pin state.
4. Added serial commands: `p` (test pump), `i` (toggle relay polarity),
   `l` (toggle light inversion), `C<dry> <wet>` (set soil calibration), `s` (status).
5. Made relay polarity, light inversion and soil calibration saved settings, so
   they survive reboots and can be changed from the phone app.
6. Fixed boot order: settings are loaded before the relay pin is initialised.
7. Started the soak timer at boot and clamped it to a 1-second minimum.
8. Tested open-drain output, then reverted to normal push-pull.
9. Moved the pump control pin from GPIO26 to GPIO27 while testing channels —
   **with a single-channel relay, the IN wire simply goes to whichever GPIO the
   firmware drives (currently GPIO27).**

## Current firmware configuration

- `PUMP_PIN = 27` (connect the relay's single IN pin here)
- `PUMP_OPEN_DRAIN = 0` (normal push-pull)
- `RELAY_ACTIVE_LOW = true` (saved value `relayInv=0`)
- `USE_FAN = 0` (fan not installed)
- Soil calibration: dry=4095, wet=1902
- Light inversion: enabled
- Mode: MANUAL (GPIO33 not shorted to GND)

## Wiring that should be in place

```
Relay VCC  --> 5V rail
Relay GND  --> common GND (ESP32 + battery + pump supply)
Relay IN   --> GPIO27
Supply +   --> relay COM
Pump +     <-- relay NO
Pump −     --> supply − / GND
```

## Next step — definitive relay test

1. **Undo the channel-2 experiment**: put the load wires back on the actual
   `COM` and `NO` terminals of the single-channel relay. Keep IN on GPIO27.
2. Power everything off. Unplug the IN wire from GPIO27.
3. Power the relay board from 5V. Jumper its IN pin directly to its GND.
   - You must hear a click, and COM↔NO should close.
   - **No click → the relay module is faulty.** Replace it with a known-good
     3.3V-compatible or 5V relay module / logic-level MOSFET module.
   - Click works → the problem is the logic-level interface; the clean fix is a
     small NPN transistor (2N2222/BC547/S8050) with a 1kΩ base resistor and a
     10kΩ pull-up from IN to 5V, as described earlier in the conversation.
4. After replacing or adding the driver, re-run `p` in Serial Monitor. The pump
   must run 8 s and stop.

## Serial command reference

| Key | Action |
|---|---|
| `p` | Toggle pump for its timed 8-second run |
| `i` | Toggle relay polarity (saves setting) |
| `l` | Toggle light sensor inversion (saves setting) |
| `C<dry> <wet>` | Example: `C4095 1902` |
| `s` | Print full status |

## Final bench observations (after this log was written)

The relay remained ON in **every** tested state:

- Pump runs as soon as USB/ESP32 is plugged in.
- Pump runs with **no power to the ESP32**.
- Pump runs with the IN wire unplugged from GPIO27.
- Pump runs with the IN wire driven HIGH **and** LOW, in both polarities, with
  push-pull and with open-drain output.
- Only physically unplugging/replugging the IN wire produces a click.
- Pump stops only when its `+` wire is removed from the relay output terminal.

Interpretation: the input is not controlling the load. The most likely cause is
that the pump is connected to the **NC (normally closed)** terminal (so the relay
at rest completes the circuit), or the relay module itself is faulty/stuck.

Firmware state at push time:

- `PUMP_PIN = 27`, `FAN_PIN = 26` (spare; fan disabled)
- `PUMP_OPEN_DRAIN = 1`, `RELAY_ACTIVE_LOW = true`
- Saved `relayInv=0` from the last serial test
- Soil dry=4095, wet=1902; light inversion enabled; mode MANUAL

Required before unattended use: move pump `+` to the other output terminal and
verify the pump is OFF with the relay unpowered, then confirm `p` gives an 8 s run.
If that fails, replace the relay module or add a 5V logic-level driver.

## QR + captive portal setup (added 2026-11-25)

- Added explicit captive-portal routes for Android/Windows and Apple probe paths;
  unknown URLs still redirect to `/`. The wildcard DNS server in `startAP()` was
  already present, so the phone's "sign in to network" prompt now opens the app.
- Generated `qr/wifi-setup.svg` (WiFi QR for the open `GREENHOUSE-SETUP` AP) and
  `qr/webapp-url.svg` (URL QR for `http://192.168.4.1/`) as a fallback.
- Firmware recompiled and uploaded to COM3 successfully.
