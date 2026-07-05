# Configuring the Software

Three files control the build. Edit them before compiling.

---

## keyer_2_features_and_options.h

This is the main feature switch file. Comment out (`//`) features you don't want; uncomment features you do want.

Currently shipped active by default:

```cpp
#define FEATURE_COMMAND_LINE_INTERFACE            // Serial CLI (\W, \F, \S, etc.)
#define FEATURE_SERIAL_HELP                       // \? help text
#define FEATURE_COMMAND_MODE                      // CW command mode via button
#define FEATURE_BUTTONS                           // Analog button array
#define FEATURE_MEMORIES                          // CW memories in EEPROM
#define FEATURE_MEMORY_MACROS                     // \E, \S, \W macros in memories
#define FEATURE_POTENTIOMETER                     // Speed pot (only if hardware present!)
#define FEATURE_PADDLE_ECHO                       // Echo paddle chars to serial
#define FEATURE_BEACON                            // Beacon / fox mode
#define FEATURE_BEACON_SETTING                    // \_ persists beacon-on-boot to EEPROM
#define FEATURE_ADDITIONAL_TX_AND_PTT_PINS        // Up to 6 TX/PTT line pairs, \X# to select
#define FEATURE_FARNSWORTH                        // Farnsworth inter-character spacing
#define FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING  // Early opposite-paddle latching in Iambic B
#define FEATURE_CAPACITIVE_PADDLE_PINS            // Capacitive touch paddle input
```

Fully ported but shipped commented out — either because they need matching hardware or were disabled to isolate another feature during testing. Uncomment to use:

```cpp
// #define FEATURE_WINKEY_EMULATION           // Winkey v2 (requires disabling ASR)
// #define FEATURE_ROTARY_ENCODER             // Rotary encoder speed control
// #define FEATURE_SIDETONE_SWITCH            // External sidetone on/off switch
// #define FEATURE_AUTOSPACE                  // Auto letterspace on paddle sending
// #define FEATURE_DEAD_OP_WATCHDOG           // Clears TX if paddle stuck
// #define FEATURE_QLF                        // QLF (poor fist) practice mode
// #define FEATURE_SEQUENCER                  // TX/RX sequencer outputs
// #define FEATURE_DYNAMIC_DAH_TO_DIT_RATIO   // Auto-adjust dah/dit ratio with WPM
// #define FEATURE_STRAIGHT_KEY               // Dedicated straight key input pin
// #define FEATURE_STRAIGHT_KEY_ECHO          // Echo straight key chars to serial
// #define FEATURE_PTT_INTERLOCK              // Input pin suppresses PTT when asserted
```

See the project README's Migration Checklist for the complete list, including features with no implementation yet.

**Important:** `FEATURE_POTENTIOMETER` will cause erratic WPM if enabled without hardware. `FEATURE_WINKEY_EMULATION` requires Auto Serial Reset to be disabled on the Arduino — see [[Winkey Emulation|550-Feature-Winkey]].

---

## keyer_2_pin_settings.h

Maps Arduino pin numbers to keyer functions. Set unused pins to `0`.

```cpp
#define paddle_left      2     // Dit paddle (active LOW, internal pullup)
#define paddle_right     5     // Dah paddle
#define tx_key_line_1   11     // TX key output (HIGH = key down)
#define ptt_tx_1        13     // PTT output (HIGH = active)
#define sidetone_line    4     // Sidetone via tone()
```

For the button array (requires `FEATURE_BUTTONS`):
```cpp
#define analog_buttons_pin      A1
#define command_mode_active_led  0    // LED while in command mode; 0 = disabled
```

For the sequencer (requires `FEATURE_SEQUENCER`):
```cpp
#define sequencer_1_pin  0   // Set to actual pin number, or 0 to disable
#define sequencer_2_pin  0
// ... up to sequencer_5_pin
```

---

## keyer_settings.h

Tunable defaults that take effect at first boot (or after a factory reset). These are the values written to EEPROM when no valid saved settings are found.

```cpp
#define initial_speed_wpm          20      // Starting WPM
#define initial_sidetone_freq_hz   600     // Sidetone frequency
#define initial_ptt_lead_time_ms   10      // PTT lead time
#define initial_ptt_tail_time_ms   10      // PTT tail time
#define default_length_wordspace   7       // Word spacing (dit units)
#define number_of_memories         3       // How many EEPROM memories
#define memory_program_end_timeout_ms 5000 // Silence → end of programming
```

Button array resistor values (see [[Buttons|530-Feature-Buttons]]):
```cpp
#define analog_buttons_r1  10    // Pullup resistor (relative units)
#define analog_buttons_r2   1    // Per-rung resistor
#define analog_button_hold_ms 500 // Long-press threshold (ms)
```

Sequencer pin polarity:
```cpp
#define sequencer_pins_active_state    HIGH
#define sequencer_pins_inactive_state  LOW
```

---

## Changing Settings at Runtime

Most settings can be changed at runtime via the [[CLI|500-Feature-Command-Line-Interface]] or [[Command Mode|510-Feature-Command-Mode]] and are automatically saved to EEPROM after 30 seconds of inactivity. A [[factory reset|800-Factory-Reset]] restores all defaults.
