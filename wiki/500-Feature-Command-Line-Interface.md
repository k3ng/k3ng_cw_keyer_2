# Feature: Command Line Interface

`FEATURE_COMMAND_LINE_INTERFACE`

## Overview

The CLI provides full keyer control over a serial connection (USB). Connect at **115200 baud** with carriage return line endings.

- Type text directly to queue it for CW sending
- Prefix commands with a backslash `\`
- Commands are case-insensitive

## Using the Arduino Serial Monitor

Set the baud rate to 115200 and the line ending to **Carriage Return**. Without CR, commands won't execute.

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

The keyer supports multiple simultaneous serial ports. Each port can be independently configured as CLI or Winkey mode. The primary port (port 0) defaults to CLI at 115200 baud. Additional ports are defined in `keyer_2_serial.h`.

## Status Display

`\S` prints a full status report:

```
Mode: Iambic B
Paddle: Normal
WPM: 20
Sidetone: 600 Hz
TX: Enabled
PTT lead: 10 ms
PTT tail: 10 ms
Wordspace: 7
Dah/dit ratio: 3.00 (dynamic)
Sequencer (ms):
  #  PTT->Active  PTT->Inactive
  1  0          0
  ...
Memories:
  1: CQ CQ CQ DE K3NG
  2: (empty)
  3: (empty)
```
