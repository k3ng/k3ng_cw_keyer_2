// K3NG CW Keyer Version 2
// Anthony Good, K3NG
//
// keyer_settings.h — User-adjustable default values and startup settings.
// Edit this file to customize the keyer's default behavior.

#ifndef keyer_settings_h
#define keyer_settings_h

#include "keyer_2_serial.h"

// ---------------------------------------------------------------------------
// Default values
// ---------------------------------------------------------------------------
#define initial_speed_wpm          20
#define initial_sidetone_freq_hz   600
#define char_send_buffer_size      50
#define element_send_buffer_size   20
#define default_length_letterspace 3
#define default_length_wordspace   7
#define initial_ptt_lead_time_ms   10
#define initial_ptt_tail_time_ms   10
#define default_serial_baud_rate   115200
#define eeprom_write_time_ms       30000

// ---------------------------------------------------------------------------
// Startup CW message (sent via sidetone at boot unless OPTION_DO_NOT_SAY_HI)
// Must be uppercase.
// ---------------------------------------------------------------------------
#define HI_TEXT "HI"

// ---------------------------------------------------------------------------
// Beep/boop tone frequencies
// Used for command mode entry/exit sounds and service_sound() beep sequences.
// ---------------------------------------------------------------------------
#define hz_high_beep  1500
#define hz_low_beep    400

// ---------------------------------------------------------------------------
// Command mode
// ---------------------------------------------------------------------------
#define command_mode_acknowledgement_character  'R'   // CW character sent after each command
#define initial_command_mode_speed_wpm          20    // default command mode speed (for future \M command)

// ---------------------------------------------------------------------------
// CW memory storage (FEATURE_MEMORIES)
// ---------------------------------------------------------------------------
// Number of memories stored in EEPROM.  Maps to buttons 1, 2, 3 on the
// analog array and CLI commands \1 \2 \3.  Default 3.
#define number_of_memories  3

// How long (ms) the user must be silent — with no paddle activity — for
// program_memory() to decide the message is complete.  Inter-word pauses
// shorter than this are fine and will just insert spaces.  Increase this
// if you need longer pauses between words while programming.
#define memory_program_end_timeout_ms  5000

// EEPROM layout:
//   Byte 0               : magic number (eeprom_magic_number)
//   Bytes 1..sizeof(config_struct) : config_struct
//   4 bytes headroom
//   memory_area_start onward : memories
// sizeof(config_struct) = 14 bytes on AVR → memory_area_start = 1+14+4 = 19
#define eeprom_magic_number  42
#define memory_area_start    (sizeof(config_struct) + 5)

// ---------------------------------------------------------------------------
// Beacon mode (FEATURE_BEACON / FEATURE_BEACON_SETTING)
// ---------------------------------------------------------------------------
// Delay between successive memory-1 playbacks in beacon mode (ms).
// Only used when OPTION_BEACON_MODE_MEMORY_REPEAT_TIME is enabled.
#define beacon_memory_repeat_time_ms  3000

// ---------------------------------------------------------------------------
// Speed potentiometer (FEATURE_POTENTIOMETER)
// ---------------------------------------------------------------------------
#define initial_pot_wpm_low_value         13    // WPM when pot is fully CCW
#define initial_pot_wpm_high_value        35    // WPM when pot is fully CW
#define potentiometer_change_threshold     1    // minimum WPM change before updating speed
#define potentiometer_check_interval_ms  150    // how often to read the pot (ms)
#define potentiometer_reading_threshold    1    // min ADC change before recalculating WPM
#define potentiometer_always_on            0    // 1 = always active; 0 = activated by \V
#define default_pot_full_scale_reading  1023    // max ADC reading (10-bit = 1023)

// ---------------------------------------------------------------------------
// Analog button array (FEATURE_BUTTONS)
// R1 = pullup to VCC, R2 = series resistor per button rung.
// With R1=10, R2=1 and a 10-bit ADC (0-1023):
//   Button 0: ADC ≈   0  (command mode)
//   Button 1: ADC ≈  93  (memory 1)
//   Button 2: ADC ≈ 171  (memory 2)
//   Button 3: ADC ≈ 236  (memory 3)
// ---------------------------------------------------------------------------
#define analog_buttons_number_of_buttons  4     // includes command button (button 0) + 3 memory buttons
#define analog_buttons_r1                 10    // pullup resistor value (relative units)
#define analog_buttons_r2                 1     // per-rung resistor value (relative units)
#define analog_button_debounce_ms         200   // minimum ms between button events
#define analog_button_hold_ms             500   // press duration threshold: < hold = short, >= hold = long

// ---------------------------------------------------------------------------
// Serial port configuration
// Up to KEYER_MAX_SERIAL_PORTS (4) ports may be configured.
// MODE: SERIAL_MODE_CLI, SERIAL_MODE_WINKEY (future), SERIAL_MODE_DISABLED
// Each port needs a corresponding PORT, BAUD, and MODE define.
// Port 0 is the primary port (boot messages printed here).
// ---------------------------------------------------------------------------
#define KEYER_SERIAL_PORT_0        Serial
#define KEYER_SERIAL_PORT_0_BAUD   115200
#define KEYER_SERIAL_PORT_0_MODE   SERIAL_MODE_CLI

// Uncomment to enable additional ports (Serial1/Serial2/Serial3 on Mega, etc.)
// #define KEYER_SERIAL_PORT_1        Serial1
// #define KEYER_SERIAL_PORT_1_BAUD   115200
// #define KEYER_SERIAL_PORT_1_MODE   SERIAL_MODE_CLI

// #define KEYER_SERIAL_PORT_2        Serial2
// #define KEYER_SERIAL_PORT_2_BAUD   115200
// #define KEYER_SERIAL_PORT_2_MODE   SERIAL_MODE_CLI

// #define KEYER_SERIAL_PORT_3        Serial3
// #define KEYER_SERIAL_PORT_3_BAUD   115200
// #define KEYER_SERIAL_PORT_3_MODE   SERIAL_MODE_CLI

#endif // keyer_settings_h
