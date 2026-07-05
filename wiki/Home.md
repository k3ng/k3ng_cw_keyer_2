# K3NG CW Keyer — Version 2

A ground-up rewrite of the [K3NG CW Keyer](https://github.com/k3ng/k3ng_cw_keyer) built around a CW state machine driven entirely by `millis()` — no `delay()` calls in the keying path.

---

## Features

All optional features are compile-time switches in `keyer_2_features_and_options.h`. A feature below can have a complete, working implementation while still being commented out in that file — either isolated for testing or waiting on matching hardware. See the [K3NG_CW_KEYER v2 README](https://github.com/k3ng/k3ng_cw_keyer_2#readme) Migration Checklist for the exhaustive per-item v1→v2 port status.

**Core (always compiled):** Iambic A and B, straight key mode, bug mode, paddle reverse, sidetone, PTT with lead/tail time, EEPROM settings persistence with auto-save, factory reset (squeeze both paddles at power-up), full serial CLI (`\W`, `\F`, `\S`, etc.)

**Currently active:** CW command mode, analog button array, CW memories with backslash macros, speed potentiometer, paddle character echo, beacon/fox mode, up to 6 selectable TX/PTT lines, Farnsworth timing, CMOS Super Keyer Iambic B timing, capacitive touch paddle input

**Ported, but currently disabled** (flip the `#define` on to use): Winkey v2 protocol emulation, rotary encoder speed control, external sidetone on/off switch, autospace, Dead Operator Watchdog, QLF (poor fist) practice mode, TX/RX sequencer outputs, dynamic dah/dit ratio, dedicated straight key input with character echo, PTT interlock input

---

## Quick Navigation

### Getting Started
- [[Getting Started|100-Getting-Started]]
- [[Hardware and Wiring|200-Hardware-and-Wiring]]
- [[Configuring the Software|210-Configuring-the-Software]]
- [[Compiling and Uploading|290-Compiling-and-Uploading]]

### Architecture
- [[Architecture|300-Architecture]]
- [[EEPROM and Settings Persistence|310-EEPROM-and-Settings]]

### Operating
- [[Operating Modes|400-Operating-Modes]]
- [[Timing Adjustments|410-Timing-Adjustments]]
- [[Command Reference|700-Command-Reference]]
- [[Factory Reset|800-Factory-Reset]]

### Features
- [[Command Line Interface|500-Feature-Command-Line-Interface]]
- [[Command Mode|510-Feature-Command-Mode]]
- [[Memories|520-Feature-Memories]]
- [[Memory Macros|521-Feature-Memory-Macros]]
- [[Buttons|530-Feature-Buttons]]
- [[Speed Control|540-Feature-Speed-Control]]
- [[TX/RX Sequencer|560-Feature-Sequencer]]
- [[Beacon Mode|570-Feature-Beacon]]
- [[Winkey Emulation|550-Feature-Winkey]]
- [[Dead Operator Watchdog|580-Feature-Dead-Operator-Watchdog]]

### Reference
- [[Acknowledgements|900-Acknowledgements]]

---

## Related

- [K3NG CW Keyer v1](https://github.com/k3ng/k3ng_cw_keyer) — the original
- [v1 Wiki](https://github.com/k3ng/k3ng_cw_keyer/wiki) — legacy documentation
