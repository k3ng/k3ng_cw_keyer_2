# Feature: TX/RX Sequencer

`FEATURE_SEQUENCER` — **ships commented out by default**. It's fully implemented, but none of this code is compiled in until you uncomment it in `keyer_2_features_and_options.h`.

## Overview

The TX/RX sequencer drives up to 5 output pins with configurable delays relative to PTT transitions. This is essential for stations with external devices — linear amplifiers, LNAs, T/R switches — where incorrect switching order can damage equipment.

## Theory of Operation

```
            +-----------------------------+                   PTT active
            |                             |
PTT --------+                             +------------------ PTT inactive
                                          :
                  +--------------------+  :                   TX key active
                  |                    |  :
TX key -----------+                    +---------------------- TX key inactive

         seq1:  |<--delay A-->|                  |<--delay B-->|
                              |                  |
seq1 pin ────────────────────-+──────────────────+─────────────

         seq2:  |<---delay A--->|              |<---delay B--->|
                                |              |
seq2 pin ──────────────────────-+--------------+───────────────
```

- **Delay A** (`ptt_active_to_sequencer_active_time`): time from PTT assert → pin asserts
- **Delay B** (`ptt_inactive_to_sequencer_inactive_time`): time from PTT de-assert → pin de-asserts

Each of the 5 pins has independent delays, allowing cascaded sequencing.

## Important Timing Constraint

Configure `ptt_lead_time` ≥ the longest `ptt_active_to_sequencer_active_time`. This ensures all sequencer pins assert before the CW key line goes active. `service_sequencer()` fires the pins at exactly the right moment during the PTT lead time wait.

## Hardware

Sequencer output pins can drive NPN transistors or optoisolators to control relays, amplifier T/R lines, or antenna switches.

```
Arduino pin ──[1kΩ]──┐
                      │
                  NPN Base
                  NPN Collector ──── relay coil (+) ──── VCC
                  NPN Emitter   ──── GND (+ flyback diode across coil)
```

**Disclaimer:** Incorrect T/R sequencing can damage equipment. Test thoroughly with low power before connecting to expensive amplifiers.

## Configuration

Pin assignments in `keyer_2_pin_settings.h`:
```cpp
#define sequencer_1_pin  0   // 0 = disabled; set to actual Arduino pin
#define sequencer_2_pin  0
#define sequencer_3_pin  0
#define sequencer_4_pin  0
#define sequencer_5_pin  0
```

Pin polarity in `keyer_settings.h`:
```cpp
#define sequencer_pins_active_state    HIGH   // HIGH or LOW
#define sequencer_pins_inactive_state  LOW
```

## Setting Timing at Runtime

Timing is stored in EEPROM and adjustable via CLI:

| Command | Action |
|---------|--------|
| `\<` | Set PTT→active delay (prompts for seq# 1–5 and ms) |
| `\>` | Set PTT→inactive delay (prompts for seq# 1–5 and ms) |

Example session:
```
\<
Seq#(1-5):1 Active delay ms:20
Seq 1 active delay: 20 ms
\>
Seq#(1-5):1 Inactive delay ms:50
Seq 1 inactive delay: 50 ms
\$
Settings saved to EEPROM
```

Delays of 0 ms cause the pin to transition on the very next `loop()` iteration after the PTT event — effectively immediate.

## Viewing Current Timing

`\S` displays all sequencer timing alongside PTT settings.

## Default Values

All delays default to 0 ms. Once `FEATURE_SEQUENCER` is enabled, it's fully operational but has no effect until at least one pin is defined and timing is configured.
