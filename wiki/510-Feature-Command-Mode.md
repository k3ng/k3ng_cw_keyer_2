# Feature: Command Mode

`FEATURE_COMMAND_MODE`

## Overview

Command mode lets you change keyer settings using the paddle, without needing a serial connection. You enter a CW character and the keyer executes the corresponding command.

## Entering Command Mode

Press the command button (button 0 on the analog button array). The keyer plays a **boop-beep** (low then high tone) to confirm entry.

If `command_button` is defined as a standalone digital pin in `keyer_2_pin_settings.h`, that pin can be used instead of the analog array button 0.

## Using Command Mode

After the entry tones, key a CW character with the paddle. The keyer plays the configured acknowledgement character (default: `R`) after each recognized command, or `?` for an unknown command.

Exit by keying `X` or pressing the command button again (plays beep-boop: high then low).

## Command Mode Commands

| CW | Command |
|----|---------|
| `A` | Iambic A mode |
| `B` | Iambic B mode |
| `G` | Bug mode |
| `N` | Toggle paddle reverse |
| `W` | Speed adjust mode (see below) |
| `T` | Tune mode (see below) |
| `P` | Program a memory (see below) |
| `X` | Exit command mode |

## Speed Adjust Mode

Key `W` to enter speed adjust mode. The keyer sends continuous dits at the current speed:
- **Dit paddle** (left) — increase speed
- **Dah paddle** (right) — decrease speed

The speed changes in real time as you press the paddles. Key `X` or press the command button to exit and save the new speed.

## Tune Mode

Key `T` to enter tune mode:
- **Dit paddle** — momentary TX (key down while held, key up when released)
- **Dah paddle** — latch TX on (stays keyed until dah paddle pressed again)

PTT and TX key line behave normally in tune mode. Exit with `X` or the command button.

## Programming a Memory in Command Mode

Key `P`, then key the memory number in CW:

| CW after P | Memory |
|-----------|--------|
| `.−−−` (1-dah dah dah) | Memory 1 |
| `··−−` | Memory 2 |
| `···−` | Memory 3 |

After the memory number is recognized (short pause), the keyer plays a beep and enters programming mode. Send your message in CW with the paddle. When you stop keying for 5 seconds, the keyer saves the memory and plays back what was stored (sidetone only, no TX). See [[Memories|520-Feature-Memories]] for more detail.

## LED Indicator

If `command_mode_active_led` is set to a pin number in `keyer_2_pin_settings.h`, that LED will light while command mode is active.
