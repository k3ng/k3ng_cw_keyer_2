# Feature: Speed Control

## Speed Potentiometer

`FEATURE_POTENTIOMETER` — **active by default**.

A linear potentiometer connected to an analog pin provides continuous WPM adjustment. Because this feature ships enabled, a stock build with nothing wired to A0 will see erratic WPM from the floating pin — comment out `FEATURE_POTENTIOMETER` if you're not using a pot.

**Wiring:**
```
VCC ──── Pot one end
GND ──── Pot other end
A0  ──── Pot wiper
```

A 1kΩ to 10kΩ pot works well.

**Configuration** in `keyer_2_pin_settings.h`:
```cpp
#define potentiometer  A0
```

**Configuration** in `keyer_settings.h`:
```cpp
#define initial_pot_wpm_low_value   13   // WPM at pot minimum
#define initial_pot_wpm_high_value  35   // WPM at pot maximum
```

Toggle the pot active/inactive with `\V`. When inactive, speed is controlled by CLI/command mode only. This is useful when you want to set a precise speed without the pot overriding it.

**Important:** Do not enable `FEATURE_POTENTIOMETER` unless a potentiometer is physically connected. A floating analog pin will cause the WPM to change erratically.

## Rotary Encoder

`FEATURE_ROTARY_ENCODER` — **ships commented out by default**; uncomment it in `keyer_2_features_and_options.h` to compile it in.

A rotary encoder provides click-by-click WPM adjustment.

**Configuration** in `keyer_2_pin_settings.h`:
```cpp
#define rotary_pin1  0   // set to actual pin
#define rotary_pin2  0   // set to actual pin
```

The encoder uses a full-step state table (Ben Buxton's implementation). Enable `OPTION_ENCODER_HALF_STEP_MODE` in `keyer_2_features_and_options.h` for half-step encoders.

Each encoder click changes WPM by 1. The new speed is echoed to the serial port and saved to EEPROM with the normal 30-second auto-save.

## Command Mode Speed Adjust

In [[Command Mode|510-Feature-Command-Mode]], key `W` to enter speed adjust mode:
- **Left paddle (dit)** — increase WPM
- **Right paddle (dah)** — decrease WPM

The keyer sends continuous dits at the current speed as audio feedback while you adjust.

## CLI Speed Setting

```
\W20       set to 20 WPM
\W###      any value (practical range: 5–60 WPM)
```
