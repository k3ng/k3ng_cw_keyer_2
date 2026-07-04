# Feature: Buttons

`FEATURE_BUTTONS`

## Overview

The analog button array uses a single analog input pin and a resistor voltage divider to read up to 4 (or more) buttons. This saves digital I/O pins and requires only one analog pin for all buttons.

## Default Button Functions

| Button | Short press | Long press (≥500 ms) |
|--------|-------------|----------------------|
| 0 | Enter command mode | Enter command mode |
| 1 | Play memory 1 | Program memory 1 |
| 2 | Play memory 2 | Program memory 2 |
| 3 | Play memory 3 | Program memory 3 |

## Circuit

The buttons form a resistor ladder on the analog input pin. Each button connects the pin to a lower resistor value, producing a unique voltage:

```
VCC
 │
[R1] ← pullup (e.g. 10kΩ)
 │
 ├──── Button 0 (command) ──── GND         (ADC ≈ 0)
 │
[R2] ──── Button 1 (memory 1) ──── GND     (ADC ≈ 93 with R1=10, R2=1)
 │
[R2] ──── Button 2 (memory 2) ──── GND     (ADC ≈ 171)
 │
[R2] ──── Button 3 (memory 3) ──── GND     (ADC ≈ 236)
 │
A1 (analog input)
```

The resistor values use relative units. With `R1=10, R2=1`:
- Use a 10kΩ pullup and 1kΩ series resistors per rung, **or**
- Scale proportionally: 100kΩ pullup and 10kΩ series resistors

## Configuration

In `keyer_settings.h`:
```cpp
#define analog_buttons_r1  10    // pullup resistor (relative units)
#define analog_buttons_r2   1    // per-rung resistor (relative units)
#define analog_buttons_number_of_buttons  4
#define analog_button_debounce_ms   200  // ms between button events
#define analog_button_hold_ms       500  // long-press threshold
```

In `keyer_2_pin_settings.h`:
```cpp
#define analog_buttons_pin  A1
```

## ADC Reading

The keyer uses a 19-sample exponential moving average (EMA) to filter the ADC reading, which eliminates false triggers from electrical noise or button bounce. Button thresholds are calculated from the R1/R2 values at startup.

## Adding More Buttons

Increase `analog_buttons_number_of_buttons` and add more `R2` rungs to the ladder. The keyer calculates expected ADC values automatically from the R1/R2 ratio.
