# Hardware and Wiring

## Minimal Circuit

The absolute minimum to get a working keyer:

```
Arduino Uno/Nano
  Pin 2  ──── Dit paddle tip ──── GND
  Pin 5  ──── Dah paddle tip ──── GND
  Pin 4  ──── Sidetone speaker (+) ──── Speaker ──── GND
  GND    ──── Paddle common / sleeve
```

The paddle pins use the Arduino's internal pullups, so no external resistors are needed on the paddle lines.

## TX Key Line

```
  Pin 11 ──── TX key line (NPN transistor base via 1k resistor recommended)
```

The TX key line goes HIGH during a dit or dah. Use an NPN transistor or optoisolator to interface with your transceiver's key jack:

```
  Pin 11 ──[1kΩ]──┐
                   │
               NPN Base (e.g. 2N2222)
               NPN Collector ──── Rig KEY jack tip
               NPN Emitter  ──── GND (and Rig KEY jack sleeve)
```

## PTT Line

```
  Pin 13 ──── PTT drive (same transistor/optoisolator approach as TX key line)
```

PTT goes HIGH when PTT is active. See [[Timing Adjustments|410-Timing-Adjustments]] for lead and tail time configuration.

**Note:** Pin 13 also drives the on-board LED, which makes it a convenient visual indicator of PTT state. If this conflicts with your hardware, change `ptt_tx_1` in `keyer_2_pin_settings.h`.

## Sidetone

The sidetone output drives a small speaker or piezo buzzer directly. For better volume and tone quality:

```
  Pin 4 ──[100Ω]──── Speaker ──── GND
```

Or use a small audio amplifier module between the pin and speaker. The sidetone frequency defaults to 600 Hz and is adjustable with `\F####`.

## Analog Button Array

See [[Buttons|530-Feature-Buttons]] for full wiring details. The button array uses a single analog pin and a resistor voltage divider.

## Speed Potentiometer

```
  A0 ──── Potentiometer wiper
  VCC ─── Potentiometer one end
  GND ─── Potentiometer other end
```

A 1kΩ to 10kΩ potentiometer works well. Enable `FEATURE_POTENTIOMETER` and set `potentiometer A0` in `keyer_2_pin_settings.h`.

**Important:** Do not enable `FEATURE_POTENTIOMETER` unless a potentiometer is actually connected — a floating analog input will cause the WPM to jump around.

## TX/RX Sequencer Outputs

Up to 5 sequencer output pins can be defined. See [[TX/RX Sequencer|560-Feature-Sequencer]] for wiring and timing details.

## All Pin Assignments

All pins are configured in `keyer_2_pin_settings.h`. The defaults:

```cpp
#define paddle_left      2    // Dit paddle
#define paddle_right     5    // Dah paddle
#define tx_key_line_1   11    // TX key line 1
#define ptt_tx_1        13    // PTT line 1
#define sidetone_line    4    // Sidetone speaker
#define analog_buttons_pin  A1   // Button array
#define potentiometer       A0   // Speed pot
```

Set any unused pin to `0` to disable it.
