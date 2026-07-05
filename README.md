# K3NG CW Keyer — Version 2

**Author:** Anthony Good, K3NG  
**Version:** 2-20260630  
**License:** GNU GPL v3

A ground-up rewrite of the [K3NG CW Keyer](https://github.com/k3ng/k3ng_cw_keyer) (v1).  If this is your first encounter with the K3NG CW Keyer, you should download, compile, and use v1, not this version.  This version is in progress and very experimental at this time.

---

## Motivation

Version 2 is a clean rewrite of v1 with the following goals:

- **Responsive by design.** All CW element timing, PTT sequencing, and inter-character spacing are handled by a state machine that advances on each `loop()` call.
- **Same feature set as v1.** All major v1 features are ported or in progress. Configuration flags, CLI commands, and EEPROM layout follow v1 conventions where practical.
- **Readable, maintainable code.** Each feature is isolated in `#ifdef FEATURE_*` blocks. The core architecture is documented inline.

---

## Architecture

### CW state machine

The heart of v2 is `service_cw_scheduler()`, called multiple times per `loop()` iteration:

```
loop()
  ├── check_paddles()          — read paddle pins, queue dits/dahs
  ├── service_cw_scheduler()  — advance key state machine
  ├── check_ptt_tail()         — release PTT after tail time
  ├── service_sequencer()      — drive TX sequencer output pins
  ├── service_serial()         — process CLI and Winkey bytes
  ├── service_memory_program() — paddle memory entry
  ├── service_command_mode()   — CW command mode state machine
  └── check_buttons()          — analog button array
```

### Two-tier send buffer

Characters queued via serial keyboard, memories, or macros flow through two buffers before hitting the key line:

```
ASCII char buffer  →  element buffer (dit/dah tokens)  →  key state machine  →  TX/PTT pins
```

### File layout

| File | Purpose |
|------|---------|
| `k3ng_keyer_2.ino` | Main sketch — `setup()`, `loop()`, all hardware-facing functions |
| `keyer_2.h` | Shared structs: `config_struct`, `cw_scheduler_struct`, `tx_ptt_struct` |
| `keyer_2_cw.h/.cpp` | CW state machine — `service_cw_scheduler()`, element/char buffer helpers |
| `keyer_2_features_and_options.h` | Feature and option `#define` switches |
| `keyer_2_pin_settings.h` | All Arduino pin assignments |
| `keyer_settings.h` | Tunable defaults (WPM, timing, memory count, etc.) |
| `keyer_2_serial.h` | Serial port abstraction for multi-port support |
| `keyer_2_winkey.h/.cpp` | Winkey v2 protocol emulation |

---

## Features

All features are compile-time switches in `keyer_2_features_and_options.h`.

### Core (always compiled in)

- Iambic A and B keying modes
- Straight key mode
- Bug mode
- Paddle reverse
- Sidetone via `tone()`
- PTT with configurable lead time and tail time
- EEPROM settings persistence (auto-save after 30 s of inactivity)
- Factory reset (squeeze both paddles at power-up)
- Startup CW greeting (`HI` by default)

### Optional features (currently enabled)

| Feature | Description |
|---------|-------------|
| `FEATURE_COMMAND_LINE_INTERFACE` | Backslash CLI over serial (`\W`, `\F`, `\S`, etc.) |
| `FEATURE_SERIAL_HELP` | `\?` prints all CLI commands |
| `FEATURE_COMMAND_MODE` | Button-activated CW command mode (speed, tune, program memory, etc.) |
| `FEATURE_BUTTONS` | Analog multiplexed button array (resistor ladder on one analog pin) |
| `FEATURE_MEMORIES` | Up to N CW memories stored in EEPROM; play via buttons or `\1`–`\3` |
| `FEATURE_MEMORY_MACROS` | Backslash macros embedded in memories (`\S` serial number, `\W` wait, etc.) |
| `FEATURE_POTENTIOMETER` | Speed control via analog potentiometer |
| `FEATURE_PADDLE_ECHO` | Echo paddle-keyed characters to serial port |
| `FEATURE_BEACON` | Beacon mode — hold left paddle at boot to loop memory 1 continuously |
| `FEATURE_BEACON_SETTING` | `\_` CLI command persists beacon-on-boot setting to EEPROM |
| `FEATURE_ADDITIONAL_TX_AND_PTT_PINS` | Up to 6 TX key lines and PTT lines; `\X#` to select |
| `FEATURE_FARNSWORTH` | Farnsworth inter-character spacing (`\M###`) |
| `FEATURE_AUTOSPACE` | Auto-insert letterspace after paddle elements when operator pauses (`\z` toggle, `\Z###` factor) |
| `FEATURE_SIDETONE_SWITCH` | External toggle switch for sidetone on/off |
| `FEATURE_DEAD_OP_WATCHDOG` | Clears TX if paddle is stuck for > 100 consecutive elements |
| `FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING` | CMOS Super Keyer Iambic B timing |
| `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO` | Auto-adjust dah/dit ratio with WPM |
| `FEATURE_STRAIGHT_KEY` | Dedicated straight key input on a separate pin |
| `FEATURE_STRAIGHT_KEY_ECHO` | Echo straight key characters to serial port |
| `FEATURE_SEQUENCER` | Up to 5 TX sequencer output pins with independent PTT-relative timing |
| `FEATURE_PTT_INTERLOCK` | Input pin that suppresses PTT (TX key line unaffected) when asserted |
| `FEATURE_WINKEY_EMULATION` | Winkey v2 protocol emulation (disabled by default; see note below) |

### Planned / not yet ported

- `FEATURE_CW_DECODER`
- LCD display features
- PS/2 and USB keyboard input
- SO2R base features

---

## Serial CLI Commands

Connect at 115200 baud. Type CW text directly; prefix commands with `\`.

| Command | Action |
|---------|--------|
| `\A` | Iambic A mode |
| `\B` | Iambic B mode |
| `\G` | Bug mode |
| `\W###` | Set WPM |
| `\F####` | Set sidetone frequency (Hz) |
| `\S` | Status display |
| `\T` | Tune (TX until any key) |
| `\I` | Toggle TX enable/disable |
| `\N` | Toggle paddle reverse |
| `\Y##` | Set wordspace (dit units) |
| `\M###` | Set Farnsworth WPM (0 = off) |
| `\J###` | Set dah/dit ratio (300 = 3:1) |
| `\1` `\2` `\3` | Play memory 1 / 2 / 3 |
| `\P#` | Program memory # (paddle or text) |
| `\X#` | Switch active TX line (1–6) |
| `\<` | Set sequencer PTT→active delay |
| `\>` | Set sequencer PTT→inactive delay |
| `\$` | Save settings to EEPROM immediately |
| `\\` | Clear send buffer |
| `\~` | Reset |
| `\?` | Help |

---

## CW Command Mode

Enter by pressing the command button (button 0 on the analog array). The keyer plays a boop-beep and accepts CW input:

| CW | Action |
|----|--------|
| `W` | Speed adjust (dits = faster, dahs = slower) |
| `T` | Tune |
| `X` | Exit command mode |
| `P` then `·−−−` / `··−−−` / `···−−` | Program memory 1 / 2 / 3 |

---

## Memory System

- Number of memories set by `number_of_memories` in `keyer_settings.h` (default 3).
- EEPROM space is divided equally at runtime after the configuration struct.
- Short press of a memory button plays it; long press enters programming mode.
- Memories can contain backslash macros (e.g. `\E` for serial number).

---

## TX Sequencer

Up to 5 output pins (`sequencer_1_pin` through `sequencer_5_pin` in `keyer_2_pin_settings.h`). Each pin has independent delays:

- **PTT→active delay** (`ptt_active_to_sequencer_active_time[]`): how long after PTT asserts before this pin asserts. Configure this ≤ `ptt_lead_time` so all pins are active before the CW key goes active.
- **PTT→inactive delay** (`ptt_inactive_to_sequencer_inactive_time[]`): how long after PTT de-asserts before this pin de-asserts.

Both phases are driven by `service_sequencer()` called from `loop()`.

---

## Winkey Emulation

`FEATURE_WINKEY_EMULATION` implements the Winkey v2 serial protocol. It is commented out by default because it requires **Auto Serial Reset (ASR) to be disabled** on the Arduino — otherwise the host's DTR signal resets the board each time the logging software opens the port. Disable ASR by cutting the reset-enable trace or adding a 10 µF cap between RESET and GND.

---

## Hardware Configuration

Edit `keyer_2_pin_settings.h` to match your wiring. Edit `keyer_settings.h` to adjust defaults (WPM, PTT timing, memory count, button resistor values, etc.).

Default pin assignments (Arduino Uno/Nano):

| Signal | Pin |
|--------|-----|
| Dit paddle | 2 |
| Dah paddle | 5 |
| TX key line 1 | 11 |
| PTT line 1 | 13 |
| Sidetone | 4 |
| Analog button array | A1 |
| Speed potentiometer | A0 |

---

## Building

Open `k3ng_keyer_2/k3ng_keyer_2.ino` in the Arduino IDE. All source files in the same folder are compiled together automatically. No external libraries are required beyond the Arduino core (EEPROM is built in).

Tested on Arduino Uno and Nano. Should work on any AVR-based Arduino with sufficient flash and EEPROM.

---

## License

GNU General Public License v3. See [LICENSE](LICENSE) or https://www.gnu.org/licenses/gpl-3.0.html.

---

## Related

- [K3NG CW Keyer v1](https://github.com/k3ng/k3ng_cw_keyer) — the original
- [K3NG CW Keyer Wiki](https://github.com/k3ng/k3ng_cw_keyer/wiki) — feature documentation
