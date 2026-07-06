# K3NG CW Keyer — Version 2

**Author:** Anthony Good, K3NG
**Version:** 2-20260630
**License:** GNU GPL v3

A ground-up rewrite of the [K3NG CW Keyer](https://github.com/k3ng/k3ng_cw_keyer) (v1). If this is your first encounter with the K3NG CW Keyer, you should download, compile, and use v1, not this version. This version is in progress and very experimental at this time.

---

## Motivation

Version 2 is a clean rewrite of v1 with the following goals:

- **Responsive by design.** All CW element timing, PTT sequencing, and inter-character spacing are handled by a state machine that advances on each `loop()` call.
- **Same feature set as v1.** All major v1 features are ported or in progress. Configuration flags, CLI commands, and EEPROM layout follow v1 conventions where practical.
- **Readable, maintainable code.** Each feature is isolated in `#ifdef FEATURE_*` blocks. The core architecture is documented inline.

---

## Architecture

### The Problem With v1: Blocking Timing

In v1, all CW element timing was done inside a blocking function called `loop_element_lengths()`. This function spun in a `while (micros() - start < ticks)` busy-wait loop for the duration of every dit and dah. The entire program was frozen inside that loop while a CW element was being sent. To service anything during that time — paddles, serial port, display, rotary encoder — it all had to be crammed inside the while loop, which grew increasingly unwieldy over time.

### The v2 Solution: A Non-Blocking State Machine

In v2, the CW engine never blocks. The heart of v2 is `service_cw_scheduler()` — derived from the non-blocking scheduler in K3NG's Chestnut transceiver project — called from `loop()` on every iteration. Instead of waiting, it records a timestamp (`next_key_scheduler_transition_time`) and returns immediately. On the next call, it checks `millis()` against that timestamp and advances the state if the time has elapsed.

The state machine has five states:

| State | Meaning |
|---|---|
| `IDLE` | Not sending; ready for next element |
| `PTT_LEAD_TIME_WAIT` | PTT asserted; waiting for lead time before keying |
| `KEY_DOWN` | Key line active; waiting for element duration to expire |
| `KEY_UP` | Key line released; waiting for inter-element space to expire |
| `KEY_DOWN_HOLD` | Straight key held down; waiting for paddle release |

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

No function here blocks. As features are ported, new service calls are added to `loop()`.

### Two-Tier Send Buffer

Characters and elements are queued in two FIFO buffers before hitting the key line:

```
ASCII char buffer  →  element buffer (dit/dah tokens)  →  key state machine  →  TX/PTT pins
```

- **Character buffer** (`char_send_buffer`): holds ASCII characters waiting to be converted to elements. Fed by the serial keyboard, memories, or any other source.
- **Element buffer** (`element_send_buffer`): holds low-level element tokens (dit, dah, inter-element space, letter space, word space). The character buffer drains into this one character at a time.

`service_cw_scheduler()` services both on every `loop()` call:
1. Advance the key state machine if a timer has expired.
2. If `IDLE` and the element buffer is not empty, pop the next token and start it.
3. If the element buffer is empty and the character buffer is not empty, convert the next character to elements.

### File Structure

```
k3ng_cw_keyer-2/
├── README.md                              This document
├── wiki/                                  Full user documentation (GitHub wiki source)
└── k3ng_keyer_2/
    ├── k3ng_keyer_2.ino                  Main sketch: setup(), loop(), hardware functions
    ├── keyer_2.h                          Main header: enums, structs, constants
    ├── keyer_2_features_and_options.h     Feature and option compile-time switches
    ├── keyer_2_pin_settings.h             Pin assignments
    ├── keyer_settings.h                   Tunable defaults (WPM, timing, memory count, etc.)
    ├── keyer_2_cw.h / keyer_2_cw.cpp      CW scheduler (state machine, element/char buffers)
    ├── keyer_2_serial.h                   Serial port abstraction for multi-port support
    └── keyer_2_winkey.h / keyer_2_winkey.cpp   Winkey v2 protocol emulation
```

#### File Descriptions

**`keyer_2.h`**
The master header included by all other files. Defines all enums (`key_scheduler_type`, `sending_type`, `element_buffer_type`), structs (`config_struct`, `cw_scheduler_struct`, `tx_ptt_struct`), and constants. Also declares the hardware function prototypes (`cw_key`, `ptt`, `service_sound`) that are implemented in the `.ino` but called from `keyer_2_cw.cpp`.

**`keyer_2_features_and_options.h`**
The migration roadmap (see below). Compile-time `#define` switches for all optional features and behavioral options, mirroring `keyer_features_and_options.h` from v1. Anything above the `// *** Not ported from v1 yet ***` line has a working implementation somewhere in the codebase — some are commented out there too, either because they need matching hardware (e.g. `FEATURE_POTENTIOMETER`) or because they were deliberately disabled to isolate another feature during testing. Everything below that line has no implementation yet.

**`keyer_2_pin_settings.h`**
All hardware pin assignments. Mirrors `keyer_pin_settings.h` from v1. Core pins are always active; pins for optional features are gated by that feature's `#ifdef` so they only need to be configured when the feature is enabled.

**`keyer_settings.h`**
Tunable defaults that aren't pin assignments: WPM limits, timing constants, memory count, EEPROM layout, per-feature thresholds (e.g. `capacitance_threshold`, `qlf_dit_min`), and serial port configuration.

**`keyer_2_cw.h` / `keyer_2_cw.cpp`**
The CW state machine engine. Ported from `chestnut_cw.h/.cpp` (Chestnut transceiver project). Completely non-blocking. Contains the element and character buffers, the full ASCII-to-CW character mapping, and `service_cw_scheduler()`. Has no knowledge of hardware pins — it calls `cw_key()` and `ptt()` which are implemented in the `.ino`.

**`keyer_2_serial.h`**
Serial port abstraction so the CLI and Winkey emulation can each run on any of up to 4 hardware serial ports (`KEYER_SERIAL_PORT_0`–`3` in `keyer_settings.h`).

**`keyer_2_winkey.h` / `keyer_2_winkey.cpp`**
K1EL Winkeyer v2 protocol emulation for logging software (N1MM, Win-Test, etc.): host open/close, speed and mode set commands, PTT/sidetone control bytes, status reporting, and N1MM-style macro/paddle interrupt handling. Currently disabled by default (`FEATURE_WINKEY_EMULATION` commented out) because it requires Auto Serial Reset disabled on the Arduino — see [Winkey Emulation](#winkey-emulation) below.

**`k3ng_keyer_2.ino`**
The main sketch. Contains:
- Pin definitions (via `keyer_2_pin_settings.h`)
- Global instances: `configuration`, `cw_scheduler`, `tx_ptt`
- `setup()` and `loop()`
- Hardware functions: `cw_key()`, `ptt()`, `sidetone()`, `service_sound()`, `check_ptt_tail()`
- `check_paddles()` / `paddle_pin_read()` — paddle/straight key/bug mode input, including capacitive touch sensing
- `service_serial()` — serial CW keyboard, CLI, and Winkey byte handling
- `service_memory_program()`, `service_command_mode()`, `check_buttons()`, `service_sequencer()`, `service_ptt_interlock()` — optional-feature service routines
- `blink_led()`, `say_hi()`

---

## Features

All optional features are compile-time switches in `keyer_2_features_and_options.h`. A feature can be fully ported but still shown commented out there — either isolated for testing or waiting on matching hardware. For the exhaustive per-item v1→v2 port status, see the [Migration Checklist](#migration-checklist) below.

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
- Full serial CLI (`FEATURE_COMMAND_LINE_INTERFACE`) — backslash commands over serial (`\W`, `\F`, `\S`, etc.)

### Currently active (feature flags enabled)

| Feature | Description |
|---------|-------------|
| `FEATURE_SERIAL_HELP` | `\?` prints all CLI commands |
| `FEATURE_COMMAND_MODE` | Button-activated CW command mode (speed, tune, program memory, etc.) |
| `FEATURE_BUTTONS` | Analog multiplexed button array (resistor ladder on one analog pin) |
| `FEATURE_MEMORIES` | Up to N CW memories stored in EEPROM; play via buttons or `\1`–`\3` |
| `FEATURE_MEMORY_MACROS` | Backslash macros embedded in memories (`\S` serial number, `\W` wait, etc.) |
| `FEATURE_POTENTIOMETER` | Speed control via analog potentiometer (don't enable without one wired up) |
| `FEATURE_PADDLE_ECHO` | Echo paddle-keyed characters to serial port |
| `FEATURE_BEACON` | Beacon mode — hold left paddle at boot to loop memory 1 continuously |
| `FEATURE_BEACON_SETTING` | `\_` CLI command persists beacon-on-boot setting to EEPROM |
| `FEATURE_ADDITIONAL_TX_AND_PTT_PINS` | Up to 6 TX key lines and PTT lines; `\X#` to select |
| `FEATURE_FARNSWORTH` | Farnsworth inter-character spacing (`\M###`) |
| `FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING` | Early opposite-paddle latching in Iambic B (`\&` toggle, `\%##` threshold) |
| `FEATURE_CAPACITIVE_PADDLE_PINS` | Capacitive touch paddle input — senses finger proximity through `paddle_left`/`paddle_right` in place of mechanical contacts |

### Ported, but currently disabled

Working implementations exist for these; flip the `#define` on in `keyer_2_features_and_options.h` to use them.

| Feature | Description |
|---------|-------------|
| `FEATURE_WINKEY_EMULATION` | Winkey v2 protocol emulation (see [Winkey Emulation](#winkey-emulation) below) |
| `FEATURE_ROTARY_ENCODER` | Rotary encoder speed control |
| `FEATURE_SIDETONE_SWITCH` | External toggle switch for sidetone on/off |
| `FEATURE_AUTOSPACE` | Auto-insert letterspace after paddle elements when operator pauses (`\z` toggle, `\Z###` factor) |
| `FEATURE_DEAD_OP_WATCHDOG` | Clears TX if paddle is stuck for > 100 consecutive elements |
| `FEATURE_QLF` | QLF (poor fist) mode — randomized element timing for practice |
| `FEATURE_SEQUENCER` | Up to 5 TX sequencer output pins with independent PTT-relative timing |
| `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO` | Auto-adjust dah/dit ratio with WPM |
| `FEATURE_STRAIGHT_KEY` | Dedicated straight key input on a separate pin |
| `FEATURE_STRAIGHT_KEY_ECHO` | Echo straight key characters to serial port |
| `FEATURE_PTT_INTERLOCK` | Input pin that suppresses PTT (TX key line unaffected) when asserted |

### Planned / not yet ported

- `FEATURE_CW_DECODER`
- LCD display features
- PS/2 and USB keyboard input
- SO2R base features

See the [Migration Checklist](#migration-checklist) for the complete list.

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

### Default Pin Assignments

| Signal | Pin | Notes |
|---|---|---|
| `paddle_left` | 2 | Dit paddle, active LOW, internal pullup |
| `paddle_right` | 5 | Dah paddle, active LOW, internal pullup |
| `tx_key_line_1` | 11 | TX key output, HIGH = key down |
| `ptt_tx_1` | 13 | PTT output — shares the Uno/Nano onboard LED pin as a visual indicator |
| `sidetone_line` | 4 | Sidetone speaker via `tone()` |
| `status_led` | 13 | Heartbeat LED |
| `analog_buttons_pin` | A1 | Analog button array (`FEATURE_BUTTONS`) |
| `potentiometer` | A0 | Speed potentiometer (`FEATURE_POTENTIOMETER`) |
| `capacitive_paddle_pin_inhibit_pin` | 0 (disabled) | Forces mechanical paddle reads when driven HIGH (`FEATURE_CAPACITIVE_PADDLE_PINS`) |

### Default Settings

| Setting | Default |
|---|---|
| Speed | 20 WPM |
| Sidetone frequency | 600 Hz |
| Keyer mode | Iambic B |
| Paddle mode | Normal |
| PTT lead time | 10 ms |
| PTT tail time | 10 ms |

---

## Building

Open `k3ng_keyer_2/k3ng_keyer_2.ino` in the Arduino IDE. All source files in the same folder are compiled together automatically. No external libraries are required beyond the Arduino core (EEPROM is built in).

Tested on Arduino Uno and Nano. Should work on any AVR-based Arduino with sufficient flash and EEPROM.

---

## Migration Checklist

This tracks the porting of features and options from v1 to v2. Items are listed in rough order of priority/dependency. Check off an item when it is ported, then move its `#define` above the `// *** Not ported from v1 yet ***` line in `keyer_2_features_and_options.h` and its pin above the equivalent line in `keyer_2_pin_settings.h`.

### High Priority — Core Features

- [x] `FEATURE_COMMAND_LINE_INTERFACE` — full v1 CLI (`\?`, `\a`–`\z`, `\w`, `\f`, etc.)
- [x] `FEATURE_MEMORIES` — CW memory storage and playback (EEPROM)
- [x] `FEATURE_MEMORY_MACROS` — macro commands within memories
- [x] `FEATURE_BUTTONS` — pushbutton support (command mode entry, memory playback)
- [x] `FEATURE_COMMAND_MODE` — command mode (enter with button 0)
- [x] `FEATURE_POTENTIOMETER` — analog speed control potentiometer
- [ ] `FEATURE_EEPROM_E24C1024` — external 1Mbit I2C EEPROM
- [x] `FEATURE_AUTOSPACE` — automatic character spacing

### Display

- [ ] `FEATURE_LCD_4BIT` — HD44780 LCD, 4-bit parallel
- [ ] `FEATURE_LCD_8BIT` — HD44780 LCD, 8-bit parallel
- [ ] `FEATURE_LCD_ADAFRUIT_I2C` — Adafruit I2C LCD (MCP23017)
- [ ] `FEATURE_LCD_ADAFRUIT_BACKPACK` — Adafruit I2C LCD backpack (MCP23008)
- [ ] `FEATURE_LCD_TWILIQUIDCRYSTAL` — TwiLiquidCrystal I2C library
- [ ] `FEATURE_LCD1602_N07DH` — LinkSprite 16×2 LCD keypad shield
- [ ] `FEATURE_LCD_SAINSMART_I2C`
- [ ] `FEATURE_LCD_FABO_PCF8574`
- [ ] `FEATURE_LCD_MATHERTEL_PCF8574`
- [ ] `FEATURE_LCD_I2C_FDEBRABANDER`
- [ ] `FEATURE_LCD_HD44780`
- [ ] `FEATURE_LCD_YDv1`
- [ ] `FEATURE_OLED_SSD1306` — SSD1306 OLED (SSD1306Ascii library)
- [ ] `FEATURE_LCD_BACKLIGHT_AUTO_DIM` — auto-dim backlight after inactivity

### Winkey Emulation

- [x] `FEATURE_WINKEY_EMULATION` — K1EL Winkeyer protocol over serial
- [ ] `OPTION_PRIMARY_SERIAL_PORT_DEFAULT_WINKEY_EMULATION` — not ported (shared CLI/Winkey port boot-mode selection; out of scope)
- [ ] `OPTION_WINKEY_2_SUPPORT` — not ported; v2 is WK2-only by design (see `550-Feature-Winkey.md`)
- [x] `OPTION_WINKEY_STRICT_HOST_OPEN` — active by default
- [x] `OPTION_WINKEY_SEND_BREAKIN_STATUS_BYTE` — active by default
- [ ] `OPTION_WINKEY_INTERRUPTS_MEMORY_REPEAT` — N/A, no memory-repeat mechanism exists in v2 to interrupt
- [ ] `OPTION_WINKEY_2_HOST_CLOSE_NO_SERIAL_PORT_RESET` — N/A, only meaningful as a sub-behavior of `OPTION_WINKEY_2_SUPPORT`'s baud-switching, which doesn't exist in v2
- [x] `OPTION_WINKEY_IGNORE_LOWERCASE` — active by default
- [x] `OPTION_WINKEY_DISCARD_BYTES_AT_STARTUP` — ported, disabled by default
- [x] `OPTION_WINKEY_STRICT_EEPROM_WRITES_MAY_WEAR_OUT_EEPROM` — ported, disabled by default
- [x] `OPTION_WINKEY_SEND_WORDSPACE_AT_END_OF_BUFFER` — ported, disabled by default
- [x] `OPTION_WINKEY_FREQUENT_STATUS_REPORT` — ported, disabled by default
- [x] `OPTION_WINKEY_BLINK_PTT_ON_HOST_OPEN` — ported, disabled by default
- [x] `OPTION_WINKEY_SEND_VERSION_ON_HOST_CLOSE` — ported, disabled by default
- [x] `OPTION_WINKEY_PINCONFIG_PTT_CONTROLS_PTT_LINE` — ported, disabled by default
- [x] `OPTION_WINKEY_PROSIGN_COMPATIBILITY` — ported, disabled by default
- [x] `OPTION_WINKEY_UCXLOG_9600_BAUD` — ported, disabled by default

### Keyboard / Input

- [ ] `FEATURE_PS2_KEYBOARD` — PS/2 keyboard input
- [ ] `FEATURE_USB_KEYBOARD` — USB keyboard input
- [ ] `FEATURE_USB_MOUSE` — USB mouse input
- [ ] `FEATURE_CW_COMPUTER_KEYBOARD` — send paddle chars as USB HID keystrokes (Due/Leonardo)
- [ ] `FEATURE_4x4_KEYPAD` — 4×4 matrix keypad
- [ ] `FEATURE_3x4_KEYPAD` — 3×4 matrix keypad
- [x] `FEATURE_ROTARY_ENCODER` — rotary encoder speed control

### Timing and Keying Options

- [x] `FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING` — CMOS Super Keyer Iambic B timing
- [x] `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO` — adjustable dah-to-dit ratio
- [x] `FEATURE_FARNSWORTH` — Farnsworth sending speed
- [x] `FEATURE_QLF` — QLF (poor fist) mode
- [ ] `OPTION_INCLUDE_PTT_TAIL_FOR_MANUAL_SENDING`
- [ ] `OPTION_EXCLUDE_PTT_HANG_TIME_FOR_MANUAL_SENDING`
- [x] `OPTION_CMOS_SUPER_KEYER_IAMBIC_B_TIMING_ON_BY_DEFAULT`
- [ ] `OPTION_INVERT_PADDLE_PIN_LOGIC`
- [ ] `OPTION_DIRECT_PADDLE_PIN_READS_MEGA`
- [ ] `OPTION_DIRECT_PADDLE_PIN_READS_UNO`
- [ ] `OPTION_SIDETONE_DIGITAL_OUTPUT_NO_SQUARE_WAVE`

### CW Decoder

- [ ] `FEATURE_CW_DECODER` — CW decoder (hardware or Goertzel audio)
- [ ] `OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR`

### Echo and Straight Key

- [x] `FEATURE_STRAIGHT_KEY` — dedicated straight key input on separate pin
- [x] `FEATURE_STRAIGHT_KEY_ECHO` — echo straight key chars to serial
- [x] `FEATURE_PADDLE_ECHO` — echo paddle chars to serial

### Training

- [ ] `FEATURE_TRAINING_COMMAND_LINE_INTERFACE` — CW training via CLI
- [ ] `FEATURE_ALPHABET_SEND_PRACTICE` — command mode S: alphabet practice
- [ ] `FEATURE_COMMAND_MODE_PROGRESSIVE_5_CHAR_ECHO_PRACTICE` — command mode U
- [ ] `OPTION_WORDSWORTH_CZECH`
- [ ] `OPTION_WORDSWORTH_DEUTSCH`
- [ ] `OPTION_WORDSWORTH_NORSK`
- [ ] `OPTION_WORDSWORTH_POLISH`

### Modes and Extensions

- [ ] `FEATURE_HELL` — Hellschreiber mode
- [ ] `FEATURE_AMERICAN_MORSE` — American Morse mode
- [ ] `OPTION_NON_ENGLISH_EXTENSIONS` — additional CW characters (À, Å, Þ, etc.)
- [ ] `OPTION_DISPLAY_NON_ENGLISH_EXTENSIONS`
- [ ] `OPTION_RUSSIAN_LANGUAGE_SEND_CLI`
- [ ] `OPTION_PROSIGN_SUPPORT`

### Hardware / Power

- [ ] `FEATURE_SLEEP` — sleep mode after inactivity (not compatible with Due)
- [x] `FEATURE_SEQUENCER` — TX sequencer output pins
- [x] `FEATURE_CAPACITIVE_PADDLE_PINS` — capacitive touch paddle pins
- [x] `FEATURE_PTT_INTERLOCK` — PTT interlock input pin
- [ ] `FEATURE_LED_RING` — Mayhew Labs LED ring
- [x] `FEATURE_DEAD_OP_WATCHDOG` — watchdog timer
- [ ] `OPTION_WATCHDOG_TIMER` — ATmega hardware watchdog (4-second)
- [x] `FEATURE_SIDETONE_SWITCH` — external toggle switch for sidetone
- [ ] `FEATURE_SIDETONE_NEWTONE` — NewTone library (timer1, pins 9/10)

### Beacon

- [x] `FEATURE_BEACON` — beacon mode if paddle_left is LOW at boot
- [x] `FEATURE_BEACON_SETTING` — beacon mode controlled by EEPROM (`\_` command)
- [x] `OPTION_BEACON_MODE_MEMORY_REPEAT_TIME`
- [x] `OPTION_BEACON_MODE_PTT_TAIL_TIME`

### Networking

- [ ] `FEATURE_WEB_SERVER` — built-in Ethernet web server
- [ ] `FEATURE_INTERNET_LINK` — Internet linking over UDP

### SO2R

- [ ] `FEATURE_SO2R_BASE` — SO2R box base protocol extensions
- [ ] `FEATURE_SO2R_SWITCHES` — SO2R box TX/RX switches
- [ ] `FEATURE_SO2R_ANTENNA` — SO2R box antenna selection

### MIDI

- [ ] `FEATURE_MIDI` — MIDI output on supported hardware (Teensy 3.x)

### Storage

- [ ] `FEATURE_SD_CARD_SUPPORT` — SD card support
- [ ] `FEATURE_DL2SBA_BANKSWITCH` — memory bank switching
- [ ] `FEATURE_EEPROM_E24C1024` — external 1Mbit I2C EEPROM

### Misc Options

- [ ] `OPTION_SUPPRESS_SERIAL_BOOT_MSG`
- [x] `OPTION_DO_NOT_SAY_HI`
- [ ] `OPTION_DO_NOT_SEND_UNKNOWN_CHAR_QUESTION`
- [ ] `OPTION_UNKNOWN_CHARACTER_ERROR_TONE`
- [ ] `OPTION_PROG_MEM_TRIM_TRAILING_SPACES`
- [ ] `OPTION_DIT_PADDLE_NO_SEND_ON_MEM_RPT`
- [ ] `OPTION_SWAP_PADDLE_PARAMETER_CHANGE_DIRECTION`
- [ ] `OPTION_MORE_DISPLAY_MSGS`
- [ ] `OPTION_ADVANCED_SPEED_DISPLAY`
- [ ] `OPTION_SAVE_MEMORY_NANOKEYER`
- [ ] `OPTION_REVERSE_BUTTON_ORDER`
- [ ] `OPTION_EXCLUDE_EXTENDED_CLI_COMMANDS`
- [ ] `OPTION_EXCLUDE_MILL_MODE`
- [ ] `OPTION_NO_ULTIMATIC`
- [ ] `OPTION_DISABLE_SERIAL_PORT_CHECKING_WHILE_SENDING_CW`
- [ ] `OPTION_PERSONALIZED_STARTUP_SCREEN`
- [ ] `OPTION_DISPLAY_MEMORY_CONTENTS_COMMAND_MODE`
- [ ] `OPTION_PS2_NON_ENGLISH_CHAR_LCD_DISPLAY_SUPPORT`
- [ ] `OPTION_PS2_KEYBOARD_RESET`
- [ ] `OPTION_CW_KEYBOARD_CAPSLOCK_BEEP`
- [ ] `OPTION_CW_KEYBOARD_ITALIAN`
- [ ] `OPTION_CW_KEYBOARD_GERMAN`
- [ ] `OPTION_COMMAND_MODE_ENHANCED_CMD_ACKNOWLEDGEMENT`
- [ ] `OPTION_MOUSE_MOVEMENT_PADDLE`
- [ ] `OPTION_DFROBOT_LCD_COMMAND_BUTTONS`

---

## Contributing: How to Port a Feature

1. Find the feature in v1 (`k3ng_cw_keyer-1/k3ng_keyer/k3ng_keyer.ino`) by searching for the `#ifdef FEATURE_*` blocks.
2. Port the relevant functions and variables to v2, wrapping in the same `#ifdef`.
3. Add any new service call to `loop()` in `k3ng_keyer_2.ino`.
4. Add any new configuration fields to `config_struct` in `keyer_2.h` (use the `future_*` reserved fields first to avoid breaking EEPROM layout).
5. Move the `#define` above the `// *** Not ported from v1 yet ***` line in `keyer_2_features_and_options.h`.
6. Move any associated pins above the equivalent line in `keyer_2_pin_settings.h`.
7. Check the box in the [Migration Checklist](#migration-checklist) above.

---

## License

GNU General Public License v3. See [LICENSE](LICENSE) or https://www.gnu.org/licenses/gpl-3.0.html.

---

## Related

- [K3NG CW Keyer v1](https://github.com/k3ng/k3ng_cw_keyer) — the original
- [K3NG CW Keyer Wiki](https://github.com/k3ng/k3ng_cw_keyer/wiki) — feature documentation
