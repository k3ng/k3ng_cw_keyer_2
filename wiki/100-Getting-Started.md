# Getting Started

## What You Need

**Hardware**
- Arduino Uno, Nano, or compatible AVR board
- CW paddle (single or dual lever iambic)
- Small speaker or piezo buzzer for sidetone
- Optional: resistor ladder for button array (see [[Buttons|530-Feature-Buttons]])
- Optional: 10k potentiometer for speed control

**Software**
- [Arduino IDE](https://www.arduino.cc/en/software) 1.8.x or 2.x
- No external libraries required — only the Arduino core and built-in EEPROM library

## Quick Start

1. **Download** the sketch from the [repository](https://github.com/k3ng/k3ng_cw_keyer_2)
2. **Open** `k3ng_keyer_2/k3ng_keyer_2.ino` in the Arduino IDE
3. **Edit** `keyer_2_pin_settings.h` to match your wiring (paddle pins, sidetone pin, PTT pin)
4. **Edit** `keyer_2_features_and_options.h` to enable/disable features
5. **Compile and upload** to your Arduino
6. **Open** the serial monitor at 115200 baud — you should see the boot message and hear "HI" in CW
7. **Type** in the serial monitor to send CW, or key the paddle directly

## Default Pin Assignments

See [[Hardware and Wiring|200-Hardware-and-Wiring]] for full details and a wiring diagram.

| Function | Pin |
|----------|-----|
| Dit paddle | 2 |
| Dah paddle | 5 |
| TX key line 1 | 11 |
| PTT line 1 | 13 |
| Sidetone | 4 |
| Analog button array | A1 |
| Speed potentiometer | A0 |

## First Steps After Booting

- Type text in the serial monitor — it sends as CW
- `\W20` sets speed to 20 WPM
- `\S` shows full status
- `\?` lists all CLI commands
- Squeeze both paddles at power-up to do a [[factory reset|800-Factory-Reset]]

## Getting Help

- `\?` in the serial terminal (requires `FEATURE_SERIAL_HELP`)
- This wiki
- [GitHub Issues](https://github.com/k3ng/k3ng_cw_keyer_2/issues)
