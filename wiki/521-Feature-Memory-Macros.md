# Feature: Memory Macros

`FEATURE_MEMORY_MACROS`

## Overview

Memories can contain backslash macro commands that are executed when the memory plays. Macros allow messages to include dynamic content (serial numbers, speed changes, pauses, PTT control) without reprogramming the memory.

## Entering Macros

Macros must be entered via the CLI (`\P#<text>`) or typed directly — they cannot be keyed in CW via the paddle because `\` has no Morse code equivalent.

Example — a contest CQ with serial number:
```
\P1CQ CQ TEST DE K3NG \E
```
Every time memory 1 plays, it sends "CQ CQ TEST DE K3NG" followed by the current serial number, then increments the number.

## Available Macros

| Macro | Action |
|-------|--------|
| `\S` | Insert a space |
| `\E` | Send serial number, then increment |
| `\C` | Send serial number with cut numbers (T=0, N=9), then increment |
| `\N` | Decrement serial number (do not send) |
| `\W###` | Set sending speed to ### WPM |
| `\Y#` | Increase speed by # WPM |
| `\Z#` | Decrease speed by # WPM |
| `\F####` | Set sidetone to #### Hz |
| `\D###` | Delay ### milliseconds |
| `\T###` | Key TX for ### milliseconds (timed carrier) |
| `\U` | Activate PTT |
| `\V` | Deactivate PTT |
| `\I#` | Insert (call) memory # then return here |
| `\0`–`\9` | Jump to memory # (does not return) |
| `\X#` | Switch to transmitter # |

## Serial Numbers

The serial number starts at 1 and increments with each `\E` or `\C`. It is stored in RAM (not EEPROM) and resets to 1 at power-up.

- `\E` — sends the number in plain digits, then increments
- `\C` — sends with cut numbers (T for 0, N for 9), then increments
- `\N` — decrements without sending (e.g. to un-do a mis-sent QSO)

## Nested Memory Calls

`\I#` inserts another memory into the current playback and then continues where it left off. This is different from `\0`–`\9` which jump and do not return. Useful for building compound messages from reusable parts.

## Speed Changes in Macros

Speed changes made by `\W`, `\Y`, or `\Z` inside a memory are temporary — they apply only during playback. The keyer's configured WPM is restored after the memory finishes.

_Note: speed changes are not saved to EEPROM during memory playback to avoid excessive EEPROM writes, especially in beacon mode._
