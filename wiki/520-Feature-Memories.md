# Feature: Memories

`FEATURE_MEMORIES`

## Overview

The keyer stores CW messages in EEPROM. Messages survive power cycles and can be played back with a button press, a CLI command, or from command mode.

## Configuration

Set the number of memories in `keyer_settings.h`:

```cpp
#define number_of_memories  3
```

The EEPROM memory area is divided equally among all slots at runtime. With 3 memories on a Uno (1024 byte EEPROM), each slot holds approximately 330 characters.

## Playing a Memory

**Button:** Short press of button 1, 2, or 3 on the analog button array.

**CLI:** `\1`, `\2`, `\3`

Playback uses the normal CW send buffer, so it can be interrupted with ESC.

## Programming a Memory

### Method 1: CLI text entry

```
\P1CQ CQ CQ DE K3NG
```

Writes the text directly into memory 1. No CR needed mid-stream — type the text and hit Enter. Uppercase is stored; lowercase is converted automatically.

### Method 2: CLI paddle programming

```
\P1
```

(Enter with no text after the digit.) The keyer enters interactive paddle programming mode — send the message in CW with the paddle. Stop keying for 5 seconds to end programming. The keyer plays back the stored message (sidetone only).

### Method 3: Button long press

Hold a memory button for ≥500 ms. The keyer plays an entry beep and enters paddle programming mode for that memory slot.

### Method 4: Command mode

Enter command mode, key `P`, then key the memory number. See [[Command Mode|510-Feature-Command-Mode]].

## Programming Mode Behavior

- Characters are recognized at the inter-character pause (standard letterspace)
- Word spaces are inserted automatically at the inter-word pause
- Up to 3 consecutive spaces are stored; after that, programming times out
- After 5 seconds of silence from the last dit or dah, programming ends and the memory is saved
- The keyer plays back the stored memory (sidetone only) as confirmation

## Memory Macros

Memories can contain backslash macro commands — for example, `\E` to send and increment a serial number, or `\W25` to temporarily change speed mid-message. See [[Memory Macros|521-Feature-Memory-Macros]].

## Viewing Memory Contents

`\S` prints the contents of all memories.

## Clearing a Memory

Program it with an empty message, or perform a [[factory reset|800-Factory-Reset]] to clear all memories.

## Beacon Mode

If `FEATURE_BEACON` is enabled, holding the left paddle at power-up causes the keyer to loop memory 1 continuously. See [[Beacon Mode|570-Feature-Beacon]].
