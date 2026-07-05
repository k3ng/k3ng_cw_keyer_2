# K3NG CW Keyer — Version 2

Anthony Good, K3NG  

---

## Overview

Version 2 is a ground-up rewrite of the K3NG CW Keyer. The core keyer functionality is carried over from v1, but the CW timing engine has been completely redesigned using a non-blocking state machine derived from the Chestnut transceiver project (also by K3NG). All features from v1 will be migrated over incrementally.

---

## The Core Design Change: Non-Blocking CW Timing

### The v1 Problem

In v1, all CW element timing was done inside a blocking function called `loop_element_lengths()`. This function spun in a `while (micros() - start < ticks)` busy-wait loop for the duration of every dit and dah. The entire program was frozen inside that loop while a CW element was being sent. To service anything during that time — paddles, serial port, display, rotary encoder — it all had to be crammed inside the while loop, which grew increasingly unwieldy over time.

### The v2 Solution: State Machine

In v2, the CW engine never blocks. All timing is handled by a state machine in `service_cw_scheduler()`, which is called from `loop()` on every iteration. Instead of waiting, it records a timestamp (`next_key_scheduler_transition_time`) and returns immediately. On the next call, it checks `millis()` against that timestamp and advances the state if the time has elapsed.

The state machine has five states:

| State | Meaning |
|---|---|
| `IDLE` | Not sending; ready for next element |
| `PTT_LEAD_TIME_WAIT` | PTT asserted; waiting for lead time before keying |
| `KEY_DOWN` | Key line active; waiting for element duration to expire |
| `KEY_UP` | Key line released; waiting for inter-element space to expire |
| `KEY_DOWN_HOLD` | Straight key held down; waiting for paddle release |

### Two-Tier Send Buffer

Characters and elements are queued in two FIFO buffers:

- **Character buffer** (`char_send_buffer`): holds ASCII characters waiting to be converted to elements. Fed by the serial keyboard, memories, or any other source.
- **Element buffer** (`element_send_buffer`): holds low-level element tokens (dit, dah, inter-element space, letter space, word space). The character buffer drains into this one character at a time.

`service_cw_scheduler()` services both on every `loop()` call:
1. Advance the key state machine if a timer has expired.
2. If `IDLE` and element buffer is not empty, pop the next token and start it.
3. If element buffer is empty and character buffer is not empty, convert the next character to elements.

### The Main Loop

```cpp
void loop() {
  check_paddles();
  service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
  check_ptt_tail();
  service_serial();
  service_sound(SERVICE, 0, 0);
  blink_led();
}
```

No function here blocks. As features are ported, new service calls are added to `loop()`.

---

## File Structure

```
k3ng_cw_keyer-2/
└── k3ng_keyer_2/
    ├── k3ng_keyer_2.ino                  Main sketch: setup(), loop(), hardware functions
    ├── keyer_2.h                          Main header: enums, structs, constants
    ├── keyer_2_features_and_options.h     Feature and option compile-time switches
    ├── keyer_2_pin_settings.h             Pin assignments
    ├── keyer_2_cw.h                       CW scheduler header
    └── keyer_2_cw.cpp                     CW scheduler implementation
```

### File Descriptions

**`keyer_2.h`**  
The master header included by all other files. Defines all enums (`key_scheduler_type`, `sending_type`, `element_buffer_type`), structs (`config_struct`, `cw_scheduler_struct`, `tx_ptt_struct`), and constants. Also declares the hardware function prototypes (`cw_key`, `ptt`, `service_sound`) that are implemented in the `.ino` but called from `keyer_2_cw.cpp`.

**`keyer_2_features_and_options.h`**  
The migration roadmap (see below). Compile-time `#define` switches for all optional features and behavioral options, mirroring `keyer_features_and_options.h` from v1. Anything above the `// *** Not implemented yet ***` line is active. Everything below it is commented out and waiting to be ported.

**`keyer_2_pin_settings.h`**  
All hardware pin assignments. Mirrors `keyer_pin_settings.h` from v1. Pins actively used in the current v2 code are above the `// *** Not implemented yet ***` line. Unused pins are below it. As features are ported, their pins move up.

**`keyer_2_cw.h` / `keyer_2_cw.cpp`**  
The CW state machine engine. Ported from `chestnut_cw.h/.cpp` (Chestnut transceiver project). Completely non-blocking. Contains the element and character buffers, the full ASCII-to-CW character mapping, and `service_cw_scheduler()`. Has no knowledge of hardware pins — it calls `cw_key()` and `ptt()` which are implemented in the `.ino`.

**`k3ng_keyer_2.ino`**  
The main sketch. Contains:
- Pin definitions (via `keyer_2_pin_settings.h`)
- Global instances: `configuration`, `cw_scheduler`, `tx_ptt`
- `setup()` and `loop()`
- Hardware functions: `cw_key()`, `ptt()`, `sidetone()`, `service_sound()`, `check_ptt_tail()`
- `check_paddles()` — paddle/straight key/bug mode input
- `service_serial()` — serial CW keyboard and basic command interface
- `blink_led()`, `say_hi()`

---

## Currently Implemented (Phase 1)

The following is built in and always compiled — no feature flag required:

- **Iambic A mode** — squeeze keying with Iambic A memory behavior
- **Iambic B mode** — squeeze keying with Iambic B memory behavior  
- **Straight key mode** — both paddles act as straight key (KEY_DOWN_HOLD)
- **Bug mode** — right paddle = straight key (dah), left paddle = auto dit
- **Paddle reverse** — swap dit/dah paddle assignments
- **Sidetone** — non-blocking via `tone()` / `service_sound()` state machine
- **PTT** — with configurable lead time and tail time
- **TX enable/disable** — sidetone-only practice mode when TX is off
- **Serial CW keyboard** — type to send CW; characters queued in char buffer
- **Basic serial command interface** — `\?`, `\a`, `\b`, `\g`, `\t`, `\n`, `\r`, `\w###`, `\f####`, `\i`, `\s`, `\\`

### Default Pin Assignments (keyer_2_pin_settings.h)

| Signal | Pin | Notes |
|---|---|---|
| `paddle_left` | 2 | Dit paddle, active LOW, internal pullup |
| `paddle_right` | 5 | Dah paddle, active LOW, internal pullup |
| `tx_key_line_1` | 11 | TX key output, HIGH = key down |
| `ptt_tx_1` | 0 | PTT output (0 = disabled) |
| `sidetone_line` | 4 | Sidetone speaker via `tone()` |
| `status_led` | 13 | Heartbeat LED |

### Default Settings (keyer_2.h)

| Setting | Default |
|---|---|
| Speed | 20 WPM |
| Sidetone frequency | 600 Hz |
| Keyer mode | Iambic B |
| Paddle mode | Normal |
| PTT lead time | 10 ms |
| PTT tail time | 10 ms |

---

## Migration Checklist

This tracks the porting of features and options from v1 to v2. Items are listed in rough order of priority/dependency. Check off an item when it is ported, then move its `#define` above the `// *** Not implemented yet ***` line in `keyer_2_features_and_options.h` and its pin above the line in `keyer_2_pin_settings.h`.

### High Priority — Core Features

- [ ] `FEATURE_COMMAND_LINE_INTERFACE` — full v1 CLI (`\?`, `\a`–`\z`, `\w`, `\f`, etc.)
- [ ] `FEATURE_MEMORIES` — CW memory storage and playback (EEPROM)
- [ ] `FEATURE_MEMORY_MACROS` — macro commands within memories
- [ ] `FEATURE_BUTTONS` — pushbutton support (command mode entry, memory playback)
- [ ] `FEATURE_COMMAND_MODE` — command mode (enter with button 0)
- [ ] `FEATURE_POTENTIOMETER` — analog speed control potentiometer
- [ ] `FEATURE_EEPROM_E24C1024` — external 1Mbit I2C EEPROM
- [ ] `FEATURE_AUTOSPACE` — automatic character spacing

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

- [ ] `FEATURE_WINKEY_EMULATION` — K1EL Winkeyer protocol over serial
- [ ] `OPTION_PRIMARY_SERIAL_PORT_DEFAULT_WINKEY_EMULATION`
- [ ] `OPTION_WINKEY_2_SUPPORT`
- [ ] `OPTION_WINKEY_STRICT_HOST_OPEN`
- [ ] `OPTION_WINKEY_SEND_BREAKIN_STATUS_BYTE`
- [ ] `OPTION_WINKEY_INTERRUPTS_MEMORY_REPEAT`
- [ ] `OPTION_WINKEY_2_HOST_CLOSE_NO_SERIAL_PORT_RESET`
- [ ] `OPTION_WINKEY_IGNORE_LOWERCASE`
- [ ] `OPTION_WINKEY_DISCARD_BYTES_AT_STARTUP`
- [ ] `OPTION_WINKEY_STRICT_EEPROM_WRITES_MAY_WEAR_OUT_EEPROM`
- [ ] `OPTION_WINKEY_SEND_WORDSPACE_AT_END_OF_BUFFER`
- [ ] `OPTION_WINKEY_FREQUENT_STATUS_REPORT`
- [ ] `OPTION_WINKEY_BLINK_PTT_ON_HOST_OPEN`
- [ ] `OPTION_WINKEY_SEND_VERSION_ON_HOST_CLOSE`
- [ ] `OPTION_WINKEY_PINCONFIG_PTT_CONTROLS_PTT_LINE`
- [ ] `OPTION_WINKEY_PROSIGN_COMPATIBILITY`
- [ ] `OPTION_WINKEY_UCXLOG_9600_BAUD`

### Keyboard / Input

- [ ] `FEATURE_PS2_KEYBOARD` — PS/2 keyboard input
- [ ] `FEATURE_USB_KEYBOARD` — USB keyboard input
- [ ] `FEATURE_USB_MOUSE` — USB mouse input
- [ ] `FEATURE_CW_COMPUTER_KEYBOARD` — send paddle chars as USB HID keystrokes (Due/Leonardo)
- [ ] `FEATURE_4x4_KEYPAD` — 4×4 matrix keypad
- [ ] `FEATURE_3x4_KEYPAD` — 3×4 matrix keypad
- [ ] `FEATURE_ROTARY_ENCODER` — rotary encoder speed control

### Timing and Keying Options

- [x] `FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING` — CMOS Super Keyer Iambic B timing
- [ ] `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO` — adjustable dah-to-dit ratio
- [ ] `FEATURE_FARNSWORTH` — Farnsworth sending speed
- [ ] `FEATURE_QLF` — QLF (poor fist) mode
- [ ] `OPTION_INCLUDE_PTT_TAIL_FOR_MANUAL_SENDING`
- [ ] `OPTION_EXCLUDE_PTT_HANG_TIME_FOR_MANUAL_SENDING`
- [ ] `OPTION_CMOS_SUPER_KEYER_IAMBIC_B_TIMING_ON_BY_DEFAULT`
- [ ] `OPTION_INVERT_PADDLE_PIN_LOGIC`
- [ ] `OPTION_DIRECT_PADDLE_PIN_READS_MEGA`
- [ ] `OPTION_DIRECT_PADDLE_PIN_READS_UNO`
- [ ] `OPTION_SIDETONE_DIGITAL_OUTPUT_NO_SQUARE_WAVE`

### CW Decoder

- [ ] `FEATURE_CW_DECODER` — CW decoder (hardware or Goertzel audio)
- [ ] `OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR`

### Echo and Straight Key

- [ ] `FEATURE_STRAIGHT_KEY` — dedicated straight key input on separate pin
- [ ] `FEATURE_STRAIGHT_KEY_ECHO` — echo straight key chars to serial
- [ ] `FEATURE_PADDLE_ECHO` — echo paddle chars to serial

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
- [ ] `FEATURE_SEQUENCER` — TX sequencer output pins
- [x] `FEATURE_CAPACITIVE_PADDLE_PINS` — capacitive touch paddle pins
- [ ] `FEATURE_PTT_INTERLOCK` — PTT interlock input pin
- [ ] `FEATURE_LED_RING` — Mayhew Labs LED ring
- [ ] `FEATURE_DEAD_OP_WATCHDOG` — watchdog timer
- [ ] `OPTION_WATCHDOG_TIMER` — ATmega hardware watchdog (4-second)
- [ ] `FEATURE_SIDETONE_SWITCH` — external toggle switch for sidetone
- [ ] `FEATURE_SIDETONE_NEWTONE` — NewTone library (timer1, pins 9/10)

### Beacon

- [ ] `FEATURE_BEACON` — beacon mode if paddle_left is LOW at boot
- [ ] `FEATURE_BEACON_SETTING` — beacon mode controlled by EEPROM (`\_` command)
- [ ] `OPTION_BEACON_MODE_MEMORY_REPEAT_TIME`
- [ ] `OPTION_BEACON_MODE_PTT_TAIL_TIME`

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
- [ ] `OPTION_DO_NOT_SAY_HI`
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

## How to Port a Feature

1. Find the feature in v1 (`k3ng_cw_keyer-1/k3ng_keyer/k3ng_keyer.ino`) by searching for the `#ifdef FEATURE_*` blocks.
2. Port the relevant functions and variables to v2, wrapping in the same `#ifdef`.
3. Add any new service call to `loop()` in `k3ng_keyer_2.ino`.
4. Add any new configuration fields to `config_struct` in `keyer_2.h` (use the `future_*` reserved fields first to avoid breaking EEPROM layout).
5. Move the `#define` above the `// *** Not implemented yet ***` line in `keyer_2_features_and_options.h`.
6. Move any associated pins above the line in `keyer_2_pin_settings.h`.
7. Check the box in this document.
