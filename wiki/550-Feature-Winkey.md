# Feature: Winkey Emulation

`FEATURE_WINKEY_EMULATION`

## Overview

The keyer emulates the K1EL Winkey 2 serial protocol, allowing it to interface with logging and contest software such as N1MM+, Win-Test, RUMlog, and others that support Winkey-compatible keyers.

## Important: Auto Serial Reset (ASR)

**This is the most common source of problems with Winkey emulation.**

Most logging software opens the serial port with DTR asserted. On a standard Arduino, DTR triggers the hardware reset line, resetting the microcontroller every time the software connects. This makes the keyer appear unresponsive.

**You must disable ASR before using Winkey emulation.** Two methods:

1. **Cut the reset-enable trace** on the Arduino PCB (irreversible — check your board's documentation for the trace location, often labeled "RESET-EN")
2. **Add a 10 µF capacitor** between the RESET pin and GND (reversible, but must be removed for programming)

With ASR disabled, `FEATURE_WINKEY_EMULATION` can be enabled safely.

## Enabling

In `keyer_2_features_and_options.h`:
```cpp
#define FEATURE_WINKEY_EMULATION
```

## Protocol Support

The emulation supports Winkey version 2 commands including:
- Host open/close handshake
- WPM setting
- PTT on/off
- Sending buffered text
- Paddle status reporting
- PINCONFIG (sidetone enable, PTT hold)
- Speed pot (reported as software-controlled)

## Logging Software Setup

Configure your logging software to use the keyer's COM port at 1200 baud (Winkey standard). The keyer will handshake automatically when the software opens the port.

## Simultaneous CLI and Winkey

Multiple serial ports can run in different modes simultaneously. Port 0 can be CLI while port 1 runs Winkey, for example. Configure additional ports in `keyer_2_serial.h`.

## Known Limitations

- Winkey EEPROM read/write commands are not implemented
- Some Winkey 2 extended commands may not be supported
- `OPTION_WINKEY_2_SUPPORT` must be enabled for version 2 mode (enabled by default)
