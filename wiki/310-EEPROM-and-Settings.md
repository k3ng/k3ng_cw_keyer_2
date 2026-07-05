# EEPROM and Settings

## What Is Stored

Most runtime settings are persisted to EEPROM so they survive power cycles. This includes WPM, sidetone frequency, keyer mode, PTT timing, dah/dit ratio, and all other adjustable parameters. CW memories are also stored in EEPROM.

## Auto-Save

Settings are **not** written on every change — EEPROM has a limited write lifetime (~100,000 cycles per byte). Instead, the keyer marks settings as "dirty" when they change and writes them to EEPROM **30 seconds after the last change**. This means you must wait ~30 seconds after making a change before powering down, or use `\$` to force an immediate save.

## EEPROM Layout

```
Byte 0                       : Magic number (validates that EEPROM contains valid data)
Bytes 1 … sizeof(config)     : config_struct (WPM, sidetone, PTT timing, etc.)
Bytes sizeof(config)+1 … +4  : Reserved (4 bytes headroom)
Bytes memory_area_start … EEPROM.length()-1 : CW memory slots
```

`memory_area_start` is computed as `sizeof(config_struct) + 5` (`keyer_settings.h`) — it isn't a fixed number, since `config_struct` grows as features that add their own config fields (e.g. `FEATURE_SEQUENCER`, `FEATURE_AUTOSPACE`) are enabled. The memory area is divided equally among all configured memories (`number_of_memories` in `keyer_settings.h`). Each slot is terminated by a `0xFF` sentinel byte. On a standard Arduino Uno/Nano with 1024 bytes of EEPROM and the default feature set, `sizeof(config_struct)` is 21 bytes:

- Memory area starts at byte 26
- With 3 memories: each slot ≈ 332 bytes (~332 characters)

## First Boot / Magic Number Mismatch

On first boot, or if the magic number byte doesn't match (e.g. after a firmware upgrade that changed `config_struct`), the keyer writes all defaults to EEPROM. This effectively performs an automatic factory reset.

## Forcing a Save

```
\$
```

Writes current settings to EEPROM immediately. Confirmed with:
```
Settings saved to EEPROM
```

## Factory Reset

Squeeze both paddles simultaneously at power-up, then release them; three beep-boop pairs confirm the reset completed. This writes factory defaults and clears all memories. See [[Factory Reset|800-Factory-Reset]] for the exact sequence.

## Viewing Current Settings

```
\S
```

Displays all current settings including WPM, PTT timing, sequencer timing, and memory contents.
