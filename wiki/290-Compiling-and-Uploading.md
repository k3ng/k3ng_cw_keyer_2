# Compiling and Uploading

## Requirements

- [Arduino IDE](https://www.arduino.cc/en/software) 1.8.x or 2.x
- No external libraries — only Arduino core (EEPROM is built in)

## Steps

1. Open `k3ng_keyer_2/k3ng_keyer_2.ino` in the Arduino IDE
2. All `.h` and `.cpp` files in the same folder are compiled automatically
3. Select your board: **Tools → Board → Arduino Uno** (or Nano, etc.)
4. Select your port: **Tools → Port → /dev/cu.usbserial-...** (Mac) or **COM#** (Windows)
5. Click **Upload**

## Verified Boards

| Board | Status |
|-------|--------|
| Arduino Uno | ✅ Primary target |
| Arduino Nano | ✅ Tested |
| Arduino Mega 2560 | Should work — more pins, more flash |
| Arduino Due | Not tested (3.3V logic, different timers) |

## Memory Usage

On an Uno (32k flash, 2k RAM), a typical build with most features enabled uses approximately:

- Flash: ~24–28k (75–88%)
- RAM: ~1.2–1.6k (60–80%)

If you are running low on flash, disable features you don't use in `keyer_2_features_and_options.h`. `FEATURE_WINKEY_EMULATION` is the largest single feature.

## Compilation Errors

**"X was not declared in this scope"** — A feature is enabled that depends on another feature. Check the `#ifdef` guard comments in `keyer_2_features_and_options.h`.

**"avrdude: stk500_recv(): programmer is not responding"** — Wrong port selected, or the board isn't in bootloader mode. Try pressing the Reset button on the Arduino just before clicking Upload.

**RAM/flash overflow** — Disable unused features. `FEATURE_WINKEY_EMULATION`, `FEATURE_MEMORY_MACROS`, and `FEATURE_COMMAND_MODE` are the largest consumers.

## First Boot

After uploading, open the serial monitor at **115200 baud** with **line ending: Carriage Return** (important — the CLI expects CR). You should see:

```
K3NG CW Keyer v2 by K3NG
Version 2-20260630.0003
Type to send CW. \? for help.
```

And hear "HI" sent in CW via the sidetone.
