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
| `\D###` | Delay ### seconds (max 255) |
| `\T###` | Key TX for ### seconds (PTT on, delay, PTT off) |
| `\U` | Activate PTT |
| `\V` | Deactivate PTT |
| `\I#` | Insert memory # into playback, then continue |
| `\0`–`\9` | Insert memory # into playback, then continue (same mechanism as `\I#`, just addressed by a bare digit) |

`\X`, along with `\Q`, `\R`, `\H`, `\L`, and `\+`, is not implemented as a memory macro yet — bytes for these are silently skipped during playback. (There is a CLI command `\X#` for switching transmitters, but it only works typed directly at the serial prompt, not embedded inside a memory.)

## Serial Numbers

The serial number starts at 1 and increments with each `\E` or `\C`. It is stored in RAM (not EEPROM) and resets to 1 at power-up.

- `\E` — sends the number in plain digits, then increments
- `\C` — sends with cut numbers (T for 0, N for 9), then increments
- `\N` — decrements without sending (e.g. to un-do a mis-sent QSO)

## Nested Memory Calls

`\I#` inserts another memory into the current playback and then continues where it left off. `\0`–`\9` do exactly the same thing — insert-and-continue, not jump-and-abandon — they're just addressed with a bare digit instead of `\I` plus a digit. Useful for building compound messages from reusable parts.

## Speed Changes in Macros

Speed changes made by `\W`, `\Y`, or `\Z` inside a memory set `configuration.wpm` directly and **persist** after the memory finishes — there's no save/restore mechanism, so the new speed stays in effect (in RAM) until you change it again or power-cycle the keyer.

_Note: these speed changes are never written to EEPROM, in memory playback or otherwise — the macro handler doesn't mark settings dirty. This avoids excessive EEPROM writes (especially useful in beacon mode) but also means the speed reverts to the EEPROM-saved value on the next boot._
