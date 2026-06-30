#ifndef keyer_2_h
#define keyer_2_h

// K3NG CW Keyer Version 2
// Anthony Good, K3NG
//
// Main header: enums, structs, constants, and hardware function prototypes.

#include <Arduino.h>
#include <stdio.h>
#include "keyer_settings.h"
#include "keyer_2_features_and_options.h"

// ---------------------------------------------------------------------------
// Debug options (uncomment to enable)
// ---------------------------------------------------------------------------
// #define DEBUG_CW_KEYDOWN_KEYUP
// #define DEBUG_CW_KEY
// #define DEBUG_SERVICE_CW_SCHEDULER
// #define DEBUG_SEND_CW_CHAR

// ---------------------------------------------------------------------------
// CW scheduler state machine
// ---------------------------------------------------------------------------
enum key_scheduler_type {
  IDLE,
  PTT_LEAD_TIME_WAIT,
  KEY_DOWN,
  KEY_UP,
  KEY_DOWN_HOLD
};

// ---------------------------------------------------------------------------
// Sending type: manual (paddle) vs. automatic (memory/serial)
// ---------------------------------------------------------------------------
enum sending_type {
  AUTOMATIC_SENDING,
  MANUAL_SENDING
};

// ---------------------------------------------------------------------------
// Element buffer token types
// ---------------------------------------------------------------------------
enum element_buffer_type {
  UNDEFINED,
  HALF_UNIT_KEY_UP,
  ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP,         // dit
  THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP,      // dah
  ONE_UNIT_KEYDOWN_3_UNITS_KEY_UP,
  THREE_UNIT_KEYDOWN_3_UNITS_KEY_UP,
  ONE_UNIT_KEYDOWN_7_UNITS_KEY_UP,
  THREE_UNIT_KEYDOWN_7_UNITS_KEY_UP,
  SEVEN_UNITS_KEY_UP,
  KEY_UP_LETTERSPACE_MINUS_1,
  KEY_UP_WORDSPACE_MINUS_4,
  KEY_UP_WORDSPACE
};

// ---------------------------------------------------------------------------
// Character send buffer special codes (values >= CW_CHAR_BUFFER_START_OF_SPECIAL_CHARS)
// ---------------------------------------------------------------------------
#define CW_CHAR_BUFFER_START_OF_SPECIAL_CHARS  200
#define CW_CHAR_BUFFER_TX_INHIBIT              211
#define CW_CHAR_BUFFER_TX_ENABLE               212

// Memory macro deferred-action tokens (each is followed by 1 parameter byte)
#ifdef FEATURE_MEMORY_MACROS
#define CW_CHAR_BUFFER_MACRO_PTT_ON            213   // assert PTT (no parameter byte)
#define CW_CHAR_BUFFER_MACRO_PTT_OFF           214   // release PTT (no parameter byte)
#define CW_CHAR_BUFFER_MACRO_DELAY             215   // next byte: delay in seconds (1-255)
#define CW_CHAR_BUFFER_MACRO_WPM               216   // next byte: new WPM (5-99)
#endif

// ---------------------------------------------------------------------------
// Letterspace / prosign helpers
// ---------------------------------------------------------------------------
#define NORMAL          0
#define OMIT_LETTERSPACE  1

// ---------------------------------------------------------------------------
// Keyer machine modes (overall state)
// ---------------------------------------------------------------------------
#define KEYER_NORMAL          0
#define KEYER_COMMAND_MODE    1
#define KEYER_BEACON          2
#define KEYER_MEMORY_PROGRAM  3   // interactive memory recording mode (non-blocking)

// ---------------------------------------------------------------------------
// Keyer modes
// ---------------------------------------------------------------------------
#define STRAIGHT    1
#define IAMBIC_B    2
#define IAMBIC_A    3
#define BUG         4

// ---------------------------------------------------------------------------
// Paddle orientation
// ---------------------------------------------------------------------------
#define PADDLE_NORMAL   0
#define PADDLE_REVERSE  1

// ---------------------------------------------------------------------------
// Sidetone / sound actions
// ---------------------------------------------------------------------------
#define TONE_OFF      0
#define TONE_MS       1
#define TONE_ON       2
#define BEEP_BOOP     3
#define BEEP_BOOP_1   4
#define BEEP_BOOP_2   5
#define VERY_HIGH_BEEP  6
#define HIGH_BEEP     7
#define MED_BEEP      8
#define LOW_BEEP      9
#define BOOP_BEEP     10
#define BOOP_BEEP_1   11
#define BOOP_BEEP_2   12
#define SERVICE       100

// ---------------------------------------------------------------------------
// TX key state
// ---------------------------------------------------------------------------
#define RIG_RECEIVE           0
#define RIG_TRANSMIT          1
#define SIDETONE_ONLY_TRANSMIT  2

// ---------------------------------------------------------------------------
// CW transmit control
// ---------------------------------------------------------------------------
#define NO_CW_TRANSMIT  1
#define TRANSMIT        2

// ---------------------------------------------------------------------------
// Serial port actions
// ---------------------------------------------------------------------------
#define SERIAL_SERVICE    0
#define SERIAL_INITIALIZE 1

// ---------------------------------------------------------------------------
// Keyer / scheduler helper constants
// ---------------------------------------------------------------------------
#define REQUEST_KEY_DOWN  32767
#define REQUEST_KEY_UP    32766

// ---------------------------------------------------------------------------
// Configuration structure
// Holds runtime-adjustable keyer settings (persisted to EEPROM).
// ---------------------------------------------------------------------------
struct config_struct {
  unsigned int wpm;
  unsigned int sidetone_frequency;
  uint8_t keyer_mode;     // IAMBIC_A, IAMBIC_B, STRAIGHT, BUG
  uint8_t paddle_mode;    // PADDLE_NORMAL, PADDLE_REVERSE

  uint8_t  length_wordspace;    // in dit units (default 7)
  uint8_t  cw_tx_enabled;       // 1 = TX enabled, 0 = sidetone only
  uint8_t  beacon_mode_on_boot_up; // 1 = enter beacon mode at boot (FEATURE_BEACON_SETTING)
  uint8_t  future_uint8_t_4;
  unsigned int ptt_lead_time;   // PTT lead time in ms
  unsigned int ptt_tail_time;   // PTT tail time in ms
};

// ---------------------------------------------------------------------------
// CW scheduler structure
// Holds all state for the non-blocking CW state machine.
// ---------------------------------------------------------------------------
struct cw_scheduler_struct {
  uint8_t  currently_sending_element;          // current element type from element_buffer_type enum
  uint8_t  current_sending_type;               // AUTOMATIC_SENDING or MANUAL_SENDING
  uint8_t  cw_scheduler_state;                 // current state from key_scheduler_type enum
  unsigned long next_key_scheduler_transition_time;  // millis() target for next state transition
  unsigned int key_scheduler_keyup_ms;         // saved keyup duration for pending transition
  unsigned int key_scheduler_keydown_ms;       // saved keydown duration for pending transition
  uint8_t  last_sending_type;
  uint8_t  length_letterspace;                 // in dit units (default 3)
  uint8_t  length_wordspace;                   // in dit units (default 7)
  uint8_t  pause_sending_buffer;               // when set, char buffer is not serviced

  uint8_t  char_send_buffer_bytes;
  uint8_t  char_send_buffer_status;
  uint8_t  element_send_buffer_bytes;
  uint8_t  element_send_buffer_array[element_send_buffer_size];
  uint8_t  char_send_buffer_array[char_send_buffer_size];

  // Set by service_cw_scheduler Step 3 when a char moves from char buffer to element buffer.
  // Cleared by whoever reads it (e.g. Winkey echo dispatch).
  uint8_t  char_keying_started;   // 1 = a new char just started keying this tick
  uint8_t  char_keying_char;      // which char started keying (valid when char_keying_started=1)

  // Set by service_cw_scheduler Step 1 when the letterspace/wordspace KEY_UP timer expires
  // (i.e. the char's last element just finished). Cleared by Winkey housekeeping after echo.
  uint8_t  char_keying_finished;  // 1 = a char's full keying (incl. letterspace) just completed

  #ifdef FEATURE_MEMORY_MACROS
  unsigned long macro_delay_until;  // millis() deadline for \D / \T delays; 0 = none
  #endif
};

// ---------------------------------------------------------------------------
// PTT / TX key line state structure
// ---------------------------------------------------------------------------
struct tx_ptt_struct {
  unsigned long ptt_time;          // millis() timestamp of last key activity (for tail timing)
  uint8_t  ptt_line_asserted;      // 1 if PTT is currently active
  unsigned int ptt_tail_time;      // PTT tail time in ms
  unsigned int ptt_lead_time;      // PTT lead time in ms
  uint8_t  key_state;              // RIG_RECEIVE, RIG_TRANSMIT, or SIDETONE_ONLY_TRANSMIT
  uint8_t  cw_tx_enabled;         // 1 = TX enabled, 0 = sidetone only
  uint8_t  pin_tx;                 // Arduino pin for TX key line
  uint8_t  pin_ptt;                // Arduino pin for PTT line
  uint8_t  pin_sidetone;           // Arduino pin for sidetone output
  uint8_t  sidetone_enabled;       // 1 = sidetone active; 0 = sidetone muted (Winkey PINCONFIG bit 1)
};

// ---------------------------------------------------------------------------
// Hardware function prototypes (implemented in .ino)
// These are called from keyer_2_cw.cpp so must be declared here.
// ---------------------------------------------------------------------------
void cw_key(struct tx_ptt_struct *tx_ptt_ptr, int state, config_struct *configuration_ptr);
void ptt(struct tx_ptt_struct *tx_ptt_ptr, byte key);
void service_sound(uint8_t action, unsigned int parm1, unsigned int parm2);

#endif // keyer_2_h
