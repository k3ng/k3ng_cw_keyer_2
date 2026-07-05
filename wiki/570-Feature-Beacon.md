# Feature: Beacon Mode

`FEATURE_BEACON`

## Overview

Beacon mode causes the keyer to repeatedly transmit memory 1 in a loop — useful for fox hunts, propagation beacons, or automated ID transmissions.

## Entering Beacon Mode

**At power-up:** Hold the left (dit) paddle while powering on. The keyer enters beacon mode immediately and begins looping memory 1.

**Automatic on boot:** If `FEATURE_BEACON_SETTING` is enabled and the beacon-on-boot setting is active, the keyer enters beacon mode automatically at every power-up without holding the paddle. Toggle this setting with `\_`.

## Exiting Beacon Mode

Keying the paddle has **no effect** in beacon mode — paddle input is ignored entirely while `KEYER_BEACON` is active. Press the command button (button 0) to enter command mode, which interrupts beacon playback; exiting command mode returns to normal operation. (Buttons 1–3 just play their memory directly and don't affect beacon mode.)

## Beacon Memory

Memory 1 is always the beacon memory. Program it with the transmission you want repeated — typically your callsign and any relevant info:

```
\P1K3NG/B EN91 QRP
```

## Repeat Timing

With `OPTION_BEACON_MODE_MEMORY_REPEAT_TIME` enabled, a configurable delay is inserted between successive playbacks. Set the delay in `keyer_settings.h`:

```cpp
#define beacon_memory_repeat_time_ms  3000   // ms between repetitions
```

With `OPTION_BEACON_MODE_PTT_TAIL_TIME` enabled, the keyer waits for PTT to drop before starting the next repetition, which prevents a rapid PTT pulse between words.

## Beacon-on-Boot Setting

`FEATURE_BEACON_SETTING`

- `\_` — toggle beacon-on-boot on/off; saved to EEPROM immediately
- `\S` shows current beacon status and boot setting

This is useful for unattended beacon stations that must resume beacon mode after a power outage.

## Memory Macros in Beacon Mode

Memory macros such as `\E` (serial number) and `\D###` (delay) work in beacon mode. Speed changes made by `\W` macros are never saved to EEPROM — in beacon mode or otherwise, since the macro handler doesn't mark settings dirty — which happens to avoid wearing out EEPROM locations during long beacon runs, but isn't beacon-specific behavior.
