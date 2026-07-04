# Command Reference

Connect at 115200 baud with carriage return line endings. Prefix all commands with `\`. Text without `\` is queued for CW sending.

Press **ESC** at any time to clear the send buffer and stop sending.

---

## Mode Commands

| Command | Action | Feature flag |
|---------|--------|--------------|
| `\A` | Iambic A mode | — |
| `\B` | Iambic B mode | — |
| `\G` | Bug mode | — |
| `\N` | Toggle paddle reverse | — |
| `\I` | Toggle TX enable/disable (sidetone-only practice when disabled) | — |

---

## Speed and Timing

| Command | Action | Feature flag |
|---------|--------|--------------|
| `\W###` | Set WPM (e.g. `\W20`) | — |
| `\F####` | Set sidetone frequency in Hz (e.g. `\F600`) | — |
| `\Y#` | Set wordspace in dit units (1–9, default 7) | — |
| `\M###` | Set Farnsworth inter-character WPM (0 = disable) | `FEATURE_FARNSWORTH` |
| `\J###` | Set dah/dit ratio × 100 (300 = 3:1, range 150–810) | `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO` |
| `\^` | Toggle dynamic dah/dit ratio auto-adjustment | `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO` |
| `\&` | Toggle CMOS Super Keyer Iambic B timing on/off | `FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING` |
| `\%##` | Set CMOS Super Keyer timing threshold % (0–99) | `FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING` |

---

## Memory

| Command | Action | Feature flag |
|---------|--------|--------------|
| `\1` | Play memory 1 | `FEATURE_MEMORIES` |
| `\2` | Play memory 2 | `FEATURE_MEMORIES` |
| `\3` | Play memory 3 | `FEATURE_MEMORIES` |
| `\P#` | Program memory # via paddle (e.g. `\P1` then Enter) | `FEATURE_MEMORIES` |
| `\P#text` | Write text directly to memory # (e.g. `\P1CQ DE K3NG`) | `FEATURE_MEMORIES` |

---

## TX / PTT

| Command | Action | Feature flag |
|---------|--------|--------------|
| `\T` | Tune mode (TX until any key pressed) | — |
| `\X#` | Switch active TX line 1–6 (e.g. `\X2`) | `FEATURE_ADDITIONAL_TX_AND_PTT_PINS` |

---

## Sequencer

| Command | Action | Feature flag |
|---------|--------|--------------|
| `\<` | Set sequencer PTT→active delay (prompts: seq# 1–5, ms) | `FEATURE_SEQUENCER` |
| `\>` | Set sequencer PTT→inactive delay (prompts: seq# 1–5, ms) | `FEATURE_SEQUENCER` |

---

## Miscellaneous

| Command | Action | Feature flag |
|---------|--------|--------------|
| `\S` | Status display | — |
| `\?` | Help text | `FEATURE_SERIAL_HELP` |
| `\$` | Save settings to EEPROM immediately | — |
| `\\` | Clear send buffer immediately | — |
| `\~` | Reset (software reboot) | — |
| `\V` | Toggle speed potentiometer active/inactive | `FEATURE_POTENTIOMETER` |
| `\*` | Toggle paddle echo on/off | `FEATURE_PADDLE_ECHO` |
| `` \` `` | Toggle straight key echo on/off | `FEATURE_STRAIGHT_KEY_ECHO` |
| `\_` | Toggle beacon-on-boot enable/disable | `FEATURE_BEACON_SETTING` |
| `\{` | Toggle QLF (poor fist) mode on/off | `FEATURE_QLF` |

---

## Command Mode (CW via Paddle)

Enter command mode by pressing the command button. See [[Command Mode|510-Feature-Command-Mode]] for full details.

| CW | Action |
|----|--------|
| `A` | Iambic A |
| `B` | Iambic B |
| `G` | Bug mode |
| `N` | Paddle reverse |
| `W` | Speed adjust mode |
| `T` | Tune mode |
| `P` | Program memory (follow with memory number in CW) |
| `X` | Exit command mode |
