# Feature: Command Line Interface

`FEATURE_COMMAND_LINE_INTERFACE`

## Overview

The CLI provides full keyer control over a serial connection (USB). Connect at **115200 baud**.

- Type text directly to queue it for CW sending
- Prefix commands with a backslash `\`
- Commands are case-insensitive

Single-character commands (like `\A` or `\S`) execute the instant the character after `\` is received — no line ending needed. Commands that take a numeric argument (like `\W###`) read digits until you press Enter; either Carriage Return or Newline terminates them.

## Using the Arduino Serial Monitor

Set the baud rate to 115200. Either line ending setting works.

## Sending CW Text

Any text typed without a leading `\` is added to the CW send buffer and sent as Morse code. The buffer holds up to 50 characters. Sending continues while you type more.

Press **ESC** to immediately clear the buffer and stop sending.

## Command Format

| Format | Example | Meaning |
|--------|---------|---------|
| `\X` | `\A` | Single-character command |
| `\X###` | `\W20` | Command with numeric argument |
| `\X#text` | `\P1CQ DE K3NG` | Command with digit then text |

## All Commands

See [[Command Reference|700-Command-Reference]] for the full table.

## Multiple Serial Ports

The keyer supports multiple simultaneous serial ports. Each port can be independently configured as CLI or Winkey mode. The primary port (port 0) defaults to CLI at 115200 baud. Additional ports are defined in `keyer_settings.h` (`keyer_2_serial.h` just holds the port struct and mode constants).

## Status Display

`\S` prints a status report built one line at a time from whichever features are compiled in — each line is gated by its own `#ifdef`, so the exact output depends on `keyer_2_features_and_options.h`. With the currently-shipped default feature set, it looks like this:

```
Mode: Iambic B
Paddle: Normal
WPM: 20
Sidetone: 600 Hz
TX: Enabled
Active TX line: 1
PTT lead: 10 ms
PTT tail: 10 ms
Wordspace: 7
Farnsworth: Disabled
Dah/dit ratio: 3.00
CMOS Super Keyer: Off (33%)
Paddle echo: On
Beacon: Inactive  Boot: Disabled
Pot: Active  WPM range: 13-35
Memories:
  1: (empty)
  2: (empty)
  3: (empty)
```

Enabling other features (e.g. `FEATURE_AUTOSPACE`, `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO`, `FEATURE_SEQUENCER`) adds their own status lines — see `serial_status()` in `k3ng_keyer_2.ino` for the complete set.
