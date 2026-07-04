# Factory Reset

A factory reset restores all settings to their compiled-in defaults and clears all CW memories.

## How to Perform a Factory Reset

1. Hold **both paddles** (dit and dah) simultaneously
2. While holding both paddles, **apply power** (plug in USB or power supply)
3. Keep holding until you hear **three boop-beep pairs** (low-high-low-high-low-high)
4. Release the paddles

The keyer confirms completion on the serial port:
```
Factory reset complete.
```

## What Gets Reset

- All settings in `config_struct`: WPM, sidetone frequency, keyer mode, paddle reverse, PTT timing, wordspace, dah/dit ratio, and all feature-specific settings
- All CW memories (cleared to empty)
- EEPROM magic number is rewritten with fresh defaults

Settings revert to the values defined in `keyer_settings.h` at compile time.

## What Does NOT Get Reset

- The compiled-in code itself (obviously)
- Nothing is permanently lost that can't be recovered by re-programming memories and adjusting settings

## When to Use a Factory Reset

- After a firmware upgrade that changed `config_struct` (usually handled automatically on first boot, but a manual reset ensures a clean state)
- If the keyer behaves erratically due to corrupt EEPROM
- When giving the keyer to someone else
- If you've changed `keyer_settings.h` defaults and want the new values applied

## Automatic Reset on First Boot

If the EEPROM magic number doesn't match the compiled-in value (first boot, or after a significant firmware change), the keyer automatically writes defaults to EEPROM without the paddle-squeeze procedure. This is equivalent to a factory reset but happens silently.
