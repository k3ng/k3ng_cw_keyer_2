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

## Sub-Options

`FEATURE_WINKEY_EMULATION` has 13 compile-time sub-options in `keyer_2_features_and_options.h`, each defaulting to whatever v1 ships (not all-off) since they represent years of real-world logging-software compatibility fixes:

**Active by default:**

| Option | Effect |
|---|---|
| `OPTION_WINKEY_STRICT_HOST_OPEN` | Refuses to process any Winkey command byte other than the admin prefix until the host sends HOST OPEN (0x02). Matches real Winkey hardware. |
| `OPTION_WINKEY_SEND_BREAKIN_STATUS_BYTE` | Sends the BREAKIN status byte (0xC6) the instant a paddle interrupts host-buffered sending — needed for N1MM's CQ Repeat to recognize paddle break-in. |
| `OPTION_WINKEY_IGNORE_LOWERCASE` | Genuine K1EL behavior: lowercase bytes (97-122) are silently ignored rather than uppercased. Workaround for older SkookumLogger versions. |

**Off by default** (uncomment to enable):

| Option | Effect |
|---|---|
| `OPTION_WINKEY_DISCARD_BYTES_AT_STARTUP` | Discards the first few bytes seen on the port, working around ASR-glitch garbage bytes at connection time. |
| `OPTION_WINKEY_STRICT_EEPROM_WRITES_MAY_WEAR_OUT_EEPROM` | Persists Winkey-originated speed/sidetone changes to EEPROM (normally session-only, to avoid wear and connect-time write glitches). |
| `OPTION_WINKEY_SEND_WORDSPACE_AT_END_OF_BUFFER` | Keys one extra wordspace before reporting the buffer empty (0xC0) to the host. |
| `OPTION_WINKEY_FREQUENT_STATUS_REPORT` | Sends a status byte after every character sent, not just on state transitions — for RUMlog/RUMped. |
| `OPTION_WINKEY_BLINK_PTT_ON_HOST_OPEN` | Blinks the PTT line in Morse (".." / "--") on host open/close instead of doing nothing — a visible cue for headless/unattended setups. |
| `OPTION_WINKEY_SEND_VERSION_ON_HOST_CLOSE` | Also echoes the version byte on HOST CLOSE, not just HOST OPEN. |
| `OPTION_WINKEY_PINCONFIG_PTT_CONTROLS_PTT_LINE` | Makes the PINCONFIG PTT bit gate the actual hardware PTT line, not just internal bookkeeping. |
| `OPTION_WINKEY_PROSIGN_COMPATIBILITY` | Remaps `\`, `[`, `:`, `;`, `]` to K1EL's prosign convention instead of standard Morse punctuation. Affects all sending, not just Winkey. |
| `OPTION_WINKEY_UCXLOG_9600_BAUD` | Changes the `WINKEY_DEFAULT_BAUD` constant to 9600 for UcxLog compatibility (standard Winkey is 1200). |

Not ported: `OPTION_WINKEY_2_SUPPORT` (v2 is WK2-only — see Known Limitations), `OPTION_PRIMARY_SERIAL_PORT_DEFAULT_WINKEY_EMULATION` (shared-port boot-mode selection, out of scope), and `OPTION_WINKEY_2_HOST_CLOSE_NO_SERIAL_PORT_RESET` (only meaningful as a sub-behavior of `OPTION_WINKEY_2_SUPPORT`'s baud-switching commands, which don't exist in v2).

## Logging Software Setup

Configure your logging software to use the keyer's COM port at 1200 baud (Winkey standard). The keyer will handshake automatically when the software opens the port.

## Simultaneous CLI and Winkey

Multiple serial ports can run in different modes simultaneously. Port 0 can be CLI while port 1 runs Winkey, for example. Configure additional ports in `keyer_2_serial.h`.

## Known Limitations

- Winkey EEPROM read/write commands are not implemented
- Some Winkey 2 extended commands may not be supported
- Unlike v1, v2 always emulates Winkey version 2 (version byte 23) unconditionally — there is no v1-compatible mode and no `OPTION_WINKEY_2_SUPPORT` toggle
