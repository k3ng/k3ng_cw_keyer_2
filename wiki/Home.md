# K3NG CW Keyer — Version 2

A ground-up rewrite of the [K3NG CW Keyer](https://github.com/k3ng/k3ng_cw_keyer) built around a fully **non-blocking CW state machine**. All timing is driven by `millis()` — there are no blocking `delay()` calls anywhere in the keying path.

---

## Features

- Iambic A and B, straight key, bug mode, paddle reverse
- PTT with configurable lead time and tail time
- Adjustable sidetone frequency
- Up to 6 selectable TX/PTT lines
- Up to 5 TX/RX sequencer outputs with independent timing
- CW memories stored in EEPROM with backslash macros
- Analog multiplexed button array (play/program memories, enter command mode)
- CW command mode (change settings via paddle)
- Speed potentiometer and rotary encoder
- Farnsworth timing
- CMOS Super Keyer Iambic B timing
- Dynamic dah/dit ratio
- Dead Operator Watchdog
- Straight key with character echo
- Paddle character echo to serial
- Beacon / fox mode
- Winkey v2 protocol emulation
- Full serial CLI (`\W`, `\F`, `\S`, etc.)
- EEPROM settings persistence with auto-save
- Factory reset (squeeze both paddles at power-up)

---

## Quick Navigation

### Getting Started
- [[Getting Started|100-Getting-Started]]
- [[Hardware and Wiring|200-Hardware-and-Wiring]]
- [[Configuring the Software|210-Configuring-the-Software]]
- [[Compiling and Uploading|290-Compiling-and-Uploading]]

### Architecture
- [[Architecture — Non-Blocking Design|300-Architecture]]
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
