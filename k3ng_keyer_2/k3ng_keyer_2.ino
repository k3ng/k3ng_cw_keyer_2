/*

  K3NG CW Keyer — Version 2
  Anthony Good, K3NG

  A ground-up rewrite of the K3NG CW Keyer using a non-blocking state machine
  for all CW timing, derived from the Chestnut transceiver project (K3NG).

  Core architecture:
    - check_paddles()       : reads paddle/straight key pins, queues elements
    - service_cw_scheduler(): advances state machine, drives key/PTT pins
    - loop()                : calls all service functions; never blocks

  No blocking delays anywhere in the timing path.

  -------------------------------------------------------------------------------
  Initial scaffold — Phase 1:
    Core CW state machine, paddle input, sidetone, PTT, serial CW keyboard
  -------------------------------------------------------------------------------

*/

#define CODE_VERSION "2-20260626.1810"

#include "keyer_2.h"
#include "keyer_2_features_and_options.h"
#include "keyer_2_pin_settings.h"
#include "keyer_2_cw.h"

#include <EEPROM.h>

// ---------------------------------------------------------------------------
// Global instances
// ---------------------------------------------------------------------------

config_struct configuration;

cw_scheduler_struct cw_scheduler;

tx_ptt_struct tx_ptt;

#ifdef FEATURE_MEMORY_MACROS
int serial_number = 1;    // incremented by \E and \C macros; decremented by \N
#endif

byte config_dirty = 0;
unsigned long last_config_write = 0;

#ifdef FEATURE_POTENTIOMETER
byte pot_wpm_low_value  = initial_pot_wpm_low_value;
byte pot_wpm_high_value = initial_pot_wpm_high_value;
byte last_pot_wpm_read  = 0;
byte pot_activated      = potentiometer_always_on;
#endif

#ifdef FEATURE_COMMAND_MODE

// Command mode state constants
#define CMD_IDLE         0   // not in command mode
#define CMD_ENTRY_1      1   // playing entry low tone (boop)
#define CMD_ENTRY_2      2   // playing entry high tone (beep)
#define CMD_INPUT        3   // waiting for / accumulating CW input from paddles
#define CMD_DISPATCH     4   // character received, dispatch command (one loop iteration)
#define CMD_WAIT_ACK     5   // waiting for ack/? CW to finish before next input
#define CMD_SPEED_ADJUST      6   // W command: continuous dits, paddles adjust WPM
#define CMD_TUNE              7   // T command: left=momentary TX, right=latch TX
#define CMD_EXIT_1            8   // playing exit high tone (beep)
#define CMD_EXIT_2            9   // playing exit low tone (boop)
#ifdef FEATURE_MEMORIES
#define CMD_PROGRAM_MEM_WAIT 10   // P command: waiting for memory-number digit from user
#endif

// Command mode state variables
byte          keyer_machine_mode          = KEYER_NORMAL;
byte          command_mode_state          = CMD_IDLE;
byte          command_mode_full           = 0;     // 1 = full cmd mode (short press), 0 = speed-adjust only (long press)
unsigned int  command_mode_cw_char        = 0;     // accumulated dit/dah digit code
byte          command_mode_element_in_prog = 0;
unsigned long command_mode_idle_since     = 0;     // millis() when scheduler last went IDLE
unsigned long command_mode_sound_timer    = 0;     // millis() when current beep started
byte          command_mode_saved_tx       = 0;     // cw_tx_enabled before entering
byte          command_mode_saved_keyer    = 0;     // keyer_mode before entering
byte          command_mode_tune_latched   = 0;     // TX latched on in tune mode
byte          command_mode_tune_rprev     = HIGH;  // previous right-paddle state in tune

#endif // FEATURE_COMMAND_MODE

#ifdef FEATURE_MEMORIES
uint16_t memory_area_end = 0;   // set in setup() to EEPROM.length() - 1
#endif

// ---------------------------------------------------------------------------
// forward declarations for functions defined below
// ---------------------------------------------------------------------------

void initialize_state();
#ifdef FEATURE_POTENTIOMETER
byte pot_value_wpm();
void check_potentiometer();
#endif
void write_settings_to_eeprom();
bool read_settings_from_eeprom();
void check_for_dirty_configuration();
void check_paddles();
void cw_key(struct tx_ptt_struct *tx_ptt_ptr, int state, config_struct *configuration_ptr);
void ptt(struct tx_ptt_struct *tx_ptt_ptr, byte key);
void sidetone(byte on);
void service_sound(uint8_t action, unsigned int parm1, unsigned int parm2);
void check_ptt_tail();
void service_serial();
#if defined(FEATURE_BUTTONS) || (defined(FEATURE_COMMAND_MODE) && (command_button > 0))
void check_buttons();
#endif
#ifdef FEATURE_BUTTONS
int8_t read_analog_buttons();
#endif
#ifdef FEATURE_COMMAND_MODE
void command_mode_enter();
void command_mode_enter_speed_adjust();
void command_mode_exit_start();
void command_mode_dispatch();
void service_command_mode();
#endif
#ifdef FEATURE_COMMAND_LINE_INTERFACE
int  serial_get_number_input(byte max_digits, int lower_limit, int upper_limit);
void process_cli_command(char cmd);
void serial_wpm_set();
void serial_set_sidetone_freq();
void serial_change_wordspace();
void serial_tune_command();
void serial_status();
#ifdef FEATURE_SERIAL_HELP
void print_serial_help();
#endif
#endif
void say_hi();
#ifdef FEATURE_MEMORIES
int  memory_start(byte memory_number);
int  memory_end(byte memory_number);
int  convert_cw_number_to_ascii(long cw_code);
void play_memory(byte memory_number);
void program_memory(byte memory_number);
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void cli_program_memory();
#endif
#endif

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------

void setup() {

  // Pin modes
  pinMode(paddle_left,      INPUT_PULLUP);
  pinMode(paddle_right,     INPUT_PULLUP);
  pinMode(tx_key_line_1,    OUTPUT);
  if (ptt_tx_1)   pinMode(ptt_tx_1,   OUTPUT);
  pinMode(sidetone_line,    OUTPUT);

  // Safe initial states
  digitalWrite(tx_key_line_1, LOW);
  if (ptt_tx_1)   digitalWrite(ptt_tx_1,   LOW);
  noTone(sidetone_line);

  // Serial port
  Serial.begin(default_serial_baud_rate);
  Serial.println(F("\r\nK3NG CW Keyer v2 by K3NG"));
  Serial.print(F("Version "));
  Serial.println(F(CODE_VERSION));
  Serial.println(F("Type to send CW. \\? for help."));

  // Initialize runtime state and structs with defaults
  initialize_state();

  // EEPROM memory area bounds (must come after initialize_state so config_struct size is known)
  #ifdef FEATURE_MEMORIES
  memory_area_end = EEPROM.length() - 1;
  #endif

  // Potentiometer init
  #ifdef FEATURE_POTENTIOMETER
  pinMode(potentiometer, INPUT);
  pot_activated      = 1;
  last_pot_wpm_read  = pot_value_wpm();
  #endif

  // Factory reset: squeeze both paddles at power-up to clear settings and memories
  if (digitalRead(paddle_left) == LOW && digitalRead(paddle_right) == LOW) {
    while (digitalRead(paddle_left) == LOW || digitalRead(paddle_right) == LOW) {}
    write_settings_to_eeprom();   // write defaults to EEPROM
    #ifdef FEATURE_MEMORIES
    for (byte m = 0; m < number_of_memories; m++) {
      EEPROM.update(memory_start(m), 255);
    }
    #endif
    // Three beep-boops as confirmation
    for (byte i = 0; i < 3; i++) {
      tone(sidetone_line, hz_high_beep); delay(150);
      noTone(sidetone_line);             delay(50);
      tone(sidetone_line, hz_low_beep);  delay(150);
      noTone(sidetone_line);             delay(50);
    }
    Serial.println(F("Factory reset complete."));
  } else {
    // Normal boot: load settings from EEPROM if they exist
    if (read_settings_from_eeprom()) {
      // Apply persisted config to runtime structs
      cw_scheduler.length_wordspace = configuration.length_wordspace;
      tx_ptt.cw_tx_enabled          = configuration.cw_tx_enabled;
      tx_ptt.ptt_lead_time          = configuration.ptt_lead_time;
      tx_ptt.ptt_tail_time          = configuration.ptt_tail_time;
    } else {
      // First boot or magic number mismatch — write defaults
      write_settings_to_eeprom();
    }
  }

  // Startup sound
  say_hi();

}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------

void loop() {

  check_paddles();
  service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
  check_ptt_tail();
  service_serial();
  service_sound(SERVICE, 0, 0);
  #ifdef FEATURE_POTENTIOMETER
  check_potentiometer();
  #endif
  check_for_dirty_configuration();
  #ifdef FEATURE_COMMAND_MODE
  service_command_mode();
  #endif
  #if defined(FEATURE_BUTTONS) || (defined(FEATURE_COMMAND_MODE) && (command_button > 0))
  check_buttons();
  #endif

}

// ---------------------------------------------------------------------------
// initialize_state()
// Sets defaults for all global structs.
// ---------------------------------------------------------------------------

void initialize_state() {

  // Configuration defaults
  configuration.wpm                = initial_speed_wpm;
  configuration.sidetone_frequency = initial_sidetone_freq_hz;
  configuration.keyer_mode         = IAMBIC_B;
  configuration.paddle_mode        = PADDLE_NORMAL;
  configuration.length_wordspace   = default_length_wordspace;
  configuration.cw_tx_enabled      = 1;
  configuration.future_uint8_t_3   = 0;
  configuration.future_uint8_t_4   = 0;
  configuration.ptt_lead_time      = initial_ptt_lead_time_ms;
  configuration.ptt_tail_time      = initial_ptt_tail_time_ms;

  // CW scheduler defaults
  cw_scheduler.cw_scheduler_state          = IDLE;
  cw_scheduler.currently_sending_element   = UNDEFINED;
  cw_scheduler.current_sending_type        = MANUAL_SENDING;
  cw_scheduler.last_sending_type           = MANUAL_SENDING;
  cw_scheduler.length_letterspace          = default_length_letterspace;
  cw_scheduler.length_wordspace            = default_length_wordspace;
  cw_scheduler.pause_sending_buffer        = 0;
  cw_scheduler.char_send_buffer_bytes      = 0;
  cw_scheduler.element_send_buffer_bytes   = 0;
  cw_scheduler.next_key_scheduler_transition_time = 0;
  cw_scheduler.key_scheduler_keydown_ms    = 0;
  cw_scheduler.key_scheduler_keyup_ms      = 0;

  // TX / PTT defaults
  tx_ptt.ptt_time          = 0;
  tx_ptt.ptt_line_asserted = 0;
  tx_ptt.ptt_tail_time     = initial_ptt_tail_time_ms;
  tx_ptt.ptt_lead_time     = initial_ptt_lead_time_ms;
  tx_ptt.key_state         = RIG_RECEIVE;
  tx_ptt.cw_tx_enabled     = 1;
  tx_ptt.pin_tx            = tx_key_line_1;
  tx_ptt.pin_ptt           = ptt_tx_1;
  tx_ptt.pin_sidetone      = sidetone_line;

}

// ---------------------------------------------------------------------------
// EEPROM settings persistence
// ---------------------------------------------------------------------------

void write_settings_to_eeprom() {

  EEPROM.update(0, eeprom_magic_number);
  const byte *p = (const byte *)(const void *)&configuration;
  for (unsigned int i = 0; i < sizeof(configuration); i++) {
    EEPROM.update(1 + i, p[i]);
  }
  config_dirty = 0;
  last_config_write = millis();

}

bool read_settings_from_eeprom() {

  if (EEPROM.read(0) != eeprom_magic_number) return false;
  byte *p = (byte *)(void *)&configuration;
  for (unsigned int i = 0; i < sizeof(configuration); i++) {
    p[i] = EEPROM.read(1 + i);
  }
  return true;

}

void check_for_dirty_configuration() {

  if (config_dirty &&
      ((millis() - last_config_write) > eeprom_write_time_ms) &&
      cw_scheduler.char_send_buffer_bytes    == 0 &&
      cw_scheduler.element_send_buffer_bytes == 0 &&
      tx_ptt.ptt_line_asserted               == 0 &&
      digitalRead(paddle_left)               == HIGH &&
      digitalRead(paddle_right)              == HIGH) {
    write_settings_to_eeprom();
  }

}

// ---------------------------------------------------------------------------
// Speed potentiometer (FEATURE_POTENTIOMETER)
// ---------------------------------------------------------------------------

#ifdef FEATURE_POTENTIOMETER

// Read the ADC and map to WPM.  Returns last value if change is below threshold.
byte pot_value_wpm() {

  static int  last_pot_read  = 0;
  static byte return_value   = initial_pot_wpm_low_value;

  int pot_read = analogRead(potentiometer);
  if (abs(pot_read - last_pot_read) > potentiometer_reading_threshold) {
    return_value = (byte)map(pot_read, 0, default_pot_full_scale_reading,
                             pot_wpm_low_value, pot_wpm_high_value);
    return_value = constrain(return_value, pot_wpm_low_value, pot_wpm_high_value);
    last_pot_read = pot_read;
  }
  return return_value;

}

void check_potentiometer() {

  static unsigned long last_pot_check_time = 0;

  if (!pot_activated) return;
  if ((millis() - last_pot_check_time) < potentiometer_check_interval_ms) return;
  last_pot_check_time = millis();

  if (potentiometer_enable_pin && digitalRead(potentiometer_enable_pin) == HIGH) return;

  byte pot_wpm = pot_value_wpm();
  if (abs((int)pot_wpm - (int)last_pot_wpm_read) >= potentiometer_change_threshold) {
    configuration.wpm = pot_wpm;
    last_pot_wpm_read = pot_wpm;
    config_dirty = 1;
    #ifdef FEATURE_COMMAND_LINE_INTERFACE
    Serial.print(F("WPM: "));
    Serial.println(configuration.wpm);
    #endif
  }

}

#endif // FEATURE_POTENTIOMETER

// ---------------------------------------------------------------------------
// Hardware functions — called by keyer_2_cw.cpp via prototypes in keyer_2.h
// ---------------------------------------------------------------------------

// cw_key(): assert or release the TX key line and sidetone
void cw_key(struct tx_ptt_struct *tx_ptt_ptr, int state, config_struct *configuration_ptr) {

  #ifdef DEBUG_CW_KEY
    Serial.print(F("cw_key: ")); Serial.println(state);
  #endif

  if (tx_ptt_ptr->cw_tx_enabled) {

    if ((state == RIG_TRANSMIT) && (tx_ptt_ptr->key_state == RIG_RECEIVE)) {
      ptt(tx_ptt_ptr, 1);
      digitalWrite(tx_ptt_ptr->pin_tx, HIGH);
      sidetone(1);
      tx_ptt_ptr->key_state = RIG_TRANSMIT;
    } else if ((state == RIG_RECEIVE) && (tx_ptt_ptr->key_state == RIG_TRANSMIT)) {
      digitalWrite(tx_ptt_ptr->pin_tx, LOW);
      sidetone(0);
      tx_ptt_ptr->key_state = RIG_RECEIVE;
    }

  } else {
    // TX disabled — sidetone practice only
    if ((state == RIG_TRANSMIT) && (tx_ptt_ptr->key_state == RIG_RECEIVE)) {
      sidetone(1);
      tx_ptt_ptr->key_state = SIDETONE_ONLY_TRANSMIT;
    } else if ((state == RIG_RECEIVE) && (tx_ptt_ptr->key_state == SIDETONE_ONLY_TRANSMIT)) {
      sidetone(0);
      tx_ptt_ptr->key_state = RIG_RECEIVE;
    }
  }

}

// ---------------------------------------------------------------------------

// ptt(): assert or release the PTT line
void ptt(struct tx_ptt_struct *tx_ptt_ptr, byte key) {

  if (key && tx_ptt_ptr->cw_tx_enabled) {
    if (!tx_ptt_ptr->ptt_line_asserted) {
      digitalWrite(tx_ptt_ptr->pin_ptt, HIGH);
      tx_ptt_ptr->ptt_line_asserted = 1;
    }
    tx_ptt_ptr->ptt_time = millis();
  } else {
    if (tx_ptt_ptr->ptt_line_asserted) {
      digitalWrite(tx_ptt_ptr->pin_ptt, LOW);
      tx_ptt_ptr->ptt_line_asserted = 0;
    }
  }

}

// ---------------------------------------------------------------------------

// sidetone(): start or stop the CW sidetone
void sidetone(byte on) {

  static byte sidetone_state = 0;

  if (on && !sidetone_state) {
    service_sound(TONE_ON, configuration.sidetone_frequency, 0);
    sidetone_state = 1;
  }
  if (!on && sidetone_state) {
    service_sound(TONE_OFF, 0, 0);
    sidetone_state = 0;
  }

}

// ---------------------------------------------------------------------------

// service_sound(): non-blocking sound state machine for beeps and sidetone
// action: TONE_ON, TONE_OFF, TONE_MS, BEEP_BOOP, BOOP_BEEP, SERVICE
// parm1: frequency (Hz) when applicable
// parm2: duration (ms) for TONE_MS
void service_sound(uint8_t action, unsigned int parm1, unsigned int parm2) {

  static byte sound_state = TONE_OFF;
  static unsigned long time_to_turn_off_tone = 0;
  static byte iterations = 0;

  switch (action) {

    case SERVICE:
      if ((sound_state == TONE_MS) && (millis() > time_to_turn_off_tone)) {
        noTone(sidetone_line);
        sound_state = TONE_OFF;
      }
      if ((sound_state == BEEP_BOOP_1) && (millis() > time_to_turn_off_tone)) {
        tone(sidetone_line, hz_low_beep);
        sound_state = BEEP_BOOP_2;
        time_to_turn_off_tone = millis() + 100;
      }
      if ((sound_state == BEEP_BOOP_2) && (millis() > time_to_turn_off_tone)) {
        iterations--;
        if (iterations) {
          tone(sidetone_line, hz_high_beep);
          sound_state = BEEP_BOOP_1;
          time_to_turn_off_tone = millis() + 100;
        } else {
          noTone(sidetone_line);
          sound_state = TONE_OFF;
        }
      }
      if ((sound_state == BOOP_BEEP_1) && (millis() > time_to_turn_off_tone)) {
        tone(sidetone_line, hz_high_beep);
        sound_state = BOOP_BEEP_2;
        time_to_turn_off_tone = millis() + 100;
      }
      if ((sound_state == BOOP_BEEP_2) && (millis() > time_to_turn_off_tone)) {
        iterations--;
        if (iterations) {
          tone(sidetone_line, hz_low_beep);
          sound_state = BOOP_BEEP_1;
          time_to_turn_off_tone = millis() + 100;
        } else {
          noTone(sidetone_line);
          sound_state = TONE_OFF;
        }
      }
      break;

    case TONE_MS:
      tone(sidetone_line, parm1);
      time_to_turn_off_tone = millis() + parm2;
      sound_state = TONE_MS;
      break;

    case TONE_OFF:
      noTone(sidetone_line);
      sound_state = TONE_OFF;
      break;

    case TONE_ON:
      tone(sidetone_line, parm1);
      sound_state = TONE_ON;
      break;

    case BEEP_BOOP:
      tone(sidetone_line, hz_high_beep);
      sound_state = BEEP_BOOP_1;
      time_to_turn_off_tone = millis() + 100;
      iterations = (parm1 > 0) ? parm1 : 1;
      break;

    case BOOP_BEEP:
      tone(sidetone_line, hz_low_beep);
      sound_state = BOOP_BEEP_1;
      time_to_turn_off_tone = millis() + 100;
      iterations = (parm1 > 0) ? parm1 : 1;
      break;
  }

}

// ---------------------------------------------------------------------------

// check_ptt_tail(): release PTT after the key has been up for ptt_tail_time ms
void check_ptt_tail() {

  if ((tx_ptt.key_state) || (cw_scheduler.cw_scheduler_state == PTT_LEAD_TIME_WAIT)) {
    tx_ptt.ptt_time = millis();   // reset tail timer while keying
  } else {
    if (tx_ptt.ptt_line_asserted && ((millis() - tx_ptt.ptt_time) > tx_ptt.ptt_tail_time)) {
      ptt(&tx_ptt, 0);
    }
  }

}

// ---------------------------------------------------------------------------
// check_paddles()
//
// Reads paddle pins every loop iteration. Completely non-blocking.
// In iambic mode, if the scheduler is IDLE it queues a dit or dah immediately.
// If the scheduler is busy it latches the opposing paddle into a 1-element buffer
// (dit_buffer / dah_buffer) so it fires as soon as the current element finishes.
// Straight key and bug mode are handled as KEY_DOWN_HOLD states.
// ---------------------------------------------------------------------------

void check_paddles() {

  // In command mode the CW input is handled by service_command_mode() — skip here.
  #ifdef FEATURE_COMMAND_MODE
  if (keyer_machine_mode == KEYER_COMMAND_MODE) return;
  #endif

  #define DIT 1
  #define DAH 2

  static byte dit_buffer = 0;   // 1 = dit has been latched while a dah was sending
  static byte dah_buffer = 0;   // 1 = dah has been latched while a dit was sending
  static byte last_sent  = 0;   // DIT or DAH — used for iambic squeeze suppression

  byte left  = digitalRead(paddle_left);
  byte right = digitalRead(paddle_right);

  // If automatic sending is in progress and a paddle is hit, abort the buffer
  if ((cw_scheduler.current_sending_type == AUTOMATIC_SENDING) &&
      (cw_scheduler.cw_scheduler_state != IDLE) &&
      ((left == LOW) || (right == LOW) || dit_buffer || dah_buffer)) {
    clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
  }

  // --- Iambic A / B ---
  if ((configuration.keyer_mode == IAMBIC_A) || (configuration.keyer_mode == IAMBIC_B)) {

    if (cw_scheduler.cw_scheduler_state == IDLE) {

      // Clear same-element buffer on return to idle (prevents repeating the same element)
      if (dit_buffer && (last_sent == DIT)) { dit_buffer = 0; }
      if (dah_buffer && (last_sent == DAH)) { dah_buffer = 0; }

      if (configuration.paddle_mode == PADDLE_NORMAL) {
        if ((left == LOW) || (dit_buffer && (last_sent == DAH))) {
          send_dit(&cw_scheduler, MANUAL_SENDING);
          dit_buffer = 0;
          last_sent  = DIT;
        }
        if ((right == LOW) || (dah_buffer && (last_sent == DIT))) {
          send_dah(&cw_scheduler, MANUAL_SENDING);
          dah_buffer = 0;
          last_sent  = DAH;
        }
      } else {
        // Reversed paddle
        if ((left == LOW) || (dah_buffer && (last_sent == DIT))) {
          send_dah(&cw_scheduler, MANUAL_SENDING);
          dah_buffer = 0;
          last_sent  = DAH;
        }
        if ((right == LOW) || (dit_buffer && (last_sent == DAH))) {
          send_dit(&cw_scheduler, MANUAL_SENDING);
          dit_buffer = 0;
          last_sent  = DIT;
        }
      }

    } else {
      // Scheduler is busy — latch the opposite paddle for iambic squeeze
      // Iambic A: only latch during KEY_UP (end of element); Iambic B: latch at any time
      if ((configuration.keyer_mode == IAMBIC_B) ||
          ((configuration.keyer_mode == IAMBIC_A) && (cw_scheduler.cw_scheduler_state == KEY_UP))) {

        if (configuration.paddle_mode == PADDLE_NORMAL) {
          if ((left  == LOW) && (cw_scheduler.currently_sending_element == THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP)) {
            dit_buffer = 1;
          }
          if ((right == LOW) && (cw_scheduler.currently_sending_element == ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP)) {
            dah_buffer = 1;
          }
        } else {
          if ((left  == LOW) && (cw_scheduler.currently_sending_element == ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP)) {
            dah_buffer = 1;
          }
          if ((right == LOW) && (cw_scheduler.currently_sending_element == THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP)) {
            dit_buffer = 1;
          }
        }

      }
    }

    // Reset last_sent when both paddles are released at idle
    if ((cw_scheduler.cw_scheduler_state == IDLE) && (left == HIGH) && (right == HIGH)) {
      last_sent = 0;
    }

  } // end iambic

  // --- Straight key (both paddles act as straight key) ---
  if (configuration.keyer_mode == STRAIGHT) {
    if ((left == LOW) || (right == LOW)) {
      if (cw_scheduler.cw_scheduler_state != KEY_DOWN_HOLD) {
        cw_scheduler.current_sending_type = MANUAL_SENDING;
        schedule_cw_keydown_keyup(&cw_scheduler, &tx_ptt, REQUEST_KEY_DOWN, REQUEST_KEY_DOWN, &configuration);
      }
    } else {
      if (cw_scheduler.cw_scheduler_state == KEY_DOWN_HOLD) {
        schedule_cw_keydown_keyup(&cw_scheduler, &tx_ptt, REQUEST_KEY_UP, REQUEST_KEY_UP, &configuration);
      }
    }
  }

  // --- Bug mode: right paddle = straight key (dah), left paddle = auto dit ---
  if (configuration.keyer_mode == BUG) {
    if (configuration.paddle_mode == PADDLE_NORMAL) {
      if (right == LOW) {
        if (cw_scheduler.cw_scheduler_state != KEY_DOWN_HOLD) {
          cw_scheduler.current_sending_type = MANUAL_SENDING;
          schedule_cw_keydown_keyup(&cw_scheduler, &tx_ptt, REQUEST_KEY_DOWN, REQUEST_KEY_DOWN, &configuration);
        }
      } else {
        if (cw_scheduler.cw_scheduler_state == KEY_DOWN_HOLD) {
          schedule_cw_keydown_keyup(&cw_scheduler, &tx_ptt, REQUEST_KEY_UP, REQUEST_KEY_UP, &configuration);
        } else if ((cw_scheduler.cw_scheduler_state == IDLE) && (left == LOW)) {
          send_dit(&cw_scheduler, MANUAL_SENDING);
        }
      }
    } else {
      // Reversed bug
      if (left == LOW) {
        if (cw_scheduler.cw_scheduler_state != KEY_DOWN_HOLD) {
          cw_scheduler.current_sending_type = MANUAL_SENDING;
          schedule_cw_keydown_keyup(&cw_scheduler, &tx_ptt, REQUEST_KEY_DOWN, REQUEST_KEY_DOWN, &configuration);
        }
      } else {
        if (cw_scheduler.cw_scheduler_state == KEY_DOWN_HOLD) {
          schedule_cw_keydown_keyup(&cw_scheduler, &tx_ptt, REQUEST_KEY_UP, REQUEST_KEY_UP, &configuration);
        } else if ((cw_scheduler.cw_scheduler_state == IDLE) && (right == LOW)) {
          send_dit(&cw_scheduler, MANUAL_SENDING);
        }
      }
    }
  }

}

// ---------------------------------------------------------------------------
// FEATURE_COMMAND_LINE_INTERFACE
//
// Full v1-compatible CLI over the serial port.
// Normal characters are queued for CW sending.
// Commands use the v1 protocol: backslash + uppercase letter, e.g. \A \W025.
// ESC clears the send buffer at any time.
//
// Commands implemented in v2:
//   \A  Iambic A
//   \B  Iambic B
//   \G  Bug mode
//   \I  TX enable/disable toggle
//   \N  Toggle paddle reverse
//   \S  Status
//   \T  Tune (hold TX until any key)
//   \F####  Set sidetone Hz
//   \W###   Set WPM
//   \Y##    Set wordspace (in dit units)
//   \\  Clear buffer
//   \~  Reset
//   \?  Help (requires FEATURE_SERIAL_HELP)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// serial_get_number_input()
// Reads up to max_digits decimal digits terminated by CR.
// Echoes input; keeps the CW scheduler alive while waiting.
// Returns the integer value, or -1 on error/out-of-range.
// lower_limit and upper_limit are exclusive bounds.
// ---------------------------------------------------------------------------

#ifdef FEATURE_COMMAND_LINE_INTERFACE

int serial_get_number_input(byte max_digits, int lower_limit, int upper_limit) {

  char num_buf[7] = {0};
  byte num_idx = 0;

  while (1) {

    // Keep keyer running while blocked on serial input
    service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
    check_ptt_tail();
    service_sound(SERVICE, 0, 0);

    if (Serial.available() > 0) {
      char ch = (char)Serial.read();

      if ((ch == '\r') || (ch == '\n')) {
        Serial.println();
        break;
      } else if (ch == 27) {  // ESC — cancel
        clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
        return -1;
      } else if ((ch >= '0') && (ch <= '9')) {
        Serial.write(ch);
        if (num_idx < max_digits) {
          num_buf[num_idx++] = ch;
        } else {
          Serial.println(F("\r\nError"));
          return -1;
        }
      } else {
        Serial.println(F("\r\nError"));
        return -1;
      }
    }
  }

  if (num_idx == 0) { return -1; }

  int value = atoi(num_buf);
  if ((value > lower_limit) && (value < upper_limit)) {
    return value;
  }
  Serial.println(F("Error"));
  return -1;

}

// ---------------------------------------------------------------------------

void serial_wpm_set() {

  int new_wpm = serial_get_number_input(3, 0, 1000);
  if (new_wpm > 0) {
    configuration.wpm = new_wpm;
    config_dirty = 1;
    Serial.print(F("WPM: "));
    Serial.println(configuration.wpm);
  }

}

// ---------------------------------------------------------------------------

void serial_set_sidetone_freq() {

  int new_hz = serial_get_number_input(4, 99, 20001);
  if (new_hz > 0) {
    configuration.sidetone_frequency = new_hz;
    config_dirty = 1;
    Serial.print(F("Sidetone: "));
    Serial.print(configuration.sidetone_frequency);
    Serial.println(F(" Hz"));
  }

}

// ---------------------------------------------------------------------------

void serial_change_wordspace() {

  int new_ws = serial_get_number_input(2, 0, 100);
  if (new_ws > 0) {
    cw_scheduler.length_wordspace    = new_ws;
    configuration.length_wordspace   = new_ws;
    config_dirty = 1;
    Serial.print(F("Wordspace: "));
    Serial.println(new_ws);
  }

}

// ---------------------------------------------------------------------------

void serial_tune_command() {

  delay(50);
  while (Serial.available() > 0) { Serial.read(); }

  cw_key(&tx_ptt, RIG_TRANSMIT, &configuration);
  Serial.println(F("Tuning - press any key to stop"));

  while (Serial.available() == 0) {
    service_sound(SERVICE, 0, 0);
  }
  while (Serial.available() > 0) { Serial.read(); }

  cw_key(&tx_ptt, RIG_RECEIVE, &configuration);

}

// ---------------------------------------------------------------------------

void serial_status() {

  Serial.println();
  Serial.print(F("Mode: "));
  switch (configuration.keyer_mode) {
    case IAMBIC_A: Serial.println(F("Iambic A")); break;
    case IAMBIC_B: Serial.println(F("Iambic B")); break;
    case BUG:      Serial.println(F("Bug"));      break;
    case STRAIGHT: Serial.println(F("Straight")); break;
    default:       Serial.println(F("?"));        break;
  }
  Serial.print(F("Paddle: "));
  Serial.println((configuration.paddle_mode == PADDLE_NORMAL) ? F("Normal") : F("Reversed"));
  Serial.print(F("WPM: "));
  Serial.println(configuration.wpm);
  Serial.print(F("Sidetone: "));
  Serial.print(configuration.sidetone_frequency);
  Serial.println(F(" Hz"));
  Serial.print(F("TX: "));
  Serial.println(tx_ptt.cw_tx_enabled ? F("Enabled") : F("Disabled (sidetone only)"));
  Serial.print(F("PTT lead: "));
  Serial.print(tx_ptt.ptt_lead_time);
  Serial.println(F(" ms"));
  Serial.print(F("PTT tail: "));
  Serial.print(tx_ptt.ptt_tail_time);
  Serial.println(F(" ms"));
  Serial.print(F("Wordspace: "));
  Serial.println(cw_scheduler.length_wordspace);

  #ifdef FEATURE_POTENTIOMETER
  Serial.print(F("Pot: "));
  Serial.print(pot_activated ? F("Active") : F("Inactive"));
  Serial.print(F("  WPM range: "));
  Serial.print(pot_wpm_low_value);
  Serial.print(F("-"));
  Serial.println(pot_wpm_high_value);
  #endif

  #ifdef FEATURE_MEMORIES
  Serial.println(F("Memories:"));
  for (byte m = 0; m < number_of_memories; m++) {
    Serial.print(F("  "));
    Serial.print(m + 1);
    Serial.print(F(": "));
    int start = memory_start(m);
    int end   = memory_end(m);
    bool empty = true;
    for (int y = start; y <= end; y++) {
      byte b = EEPROM.read(y);
      if (b == 255) break;
      Serial.write((char)b);
      empty = false;
    }
    if (empty) Serial.print(F("(empty)"));
    Serial.println();
  }
  #endif

}

// ---------------------------------------------------------------------------

#ifdef FEATURE_SERIAL_HELP
void print_serial_help() {

  Serial.println(F("\r\nK3NG CW Keyer v2 Commands:"));
  Serial.println(F("\\A\t\tIambic A"));
  Serial.println(F("\\B\t\tIambic B"));
  Serial.println(F("\\G\t\tBug mode"));
  Serial.println(F("\\I\t\tTX enable/disable toggle"));
  Serial.println(F("\\N\t\tToggle paddle reverse"));
  Serial.println(F("\\S\t\tStatus"));
  Serial.println(F("\\T\t\tTune (hold TX until keypress)"));
  Serial.println(F("\\F####\t\tSet sidetone Hz"));
  Serial.println(F("\\W###\t\tSet WPM"));
  Serial.println(F("\\Y##\t\tSet wordspace (dit units; default 7)"));
  Serial.println(F("\\\\\t\tClear send buffer"));
  Serial.println(F("\\~\t\tReset"));
  #ifdef FEATURE_POTENTIOMETER
  Serial.println(F("\\V\t\tToggle potentiometer active/inactive"));
  #endif
  Serial.println(F("\\?\t\tThis help"));
  #ifdef FEATURE_MEMORIES
  Serial.println(F("\\1 \\2 \\3\tPlay memory 1/2/3"));
  Serial.println(F("\\P#<text>\tProgram memory # with <text> (e.g. \\P1CQ CQ DE K3NG)"));
  #endif

}
#endif // FEATURE_SERIAL_HELP

// ---------------------------------------------------------------------------
#ifdef FEATURE_MEMORIES
// cli_program_memory() — serial CLI handler for \P
//
// Syntax: \P<n><text><CR>
//   <n>    = memory number 1–N (typed immediately after \P, no space)
//   <text> = message text, optionally including backslash macro sequences
//   <CR>   = Enter key terminates the message
//
// Example: \P1CQ CQ CQ DE K3NG\E
//
// If no digit follows \P within 5 s the command is cancelled.
// If the digit is followed immediately by CR (empty message), the memory is
// erased (written with just the 0xFF sentinel).
void cli_program_memory() {

  // Read the memory number digit
  unsigned long deadline = millis() + 5000UL;
  char num_char = 0;
  while (millis() < deadline) {
    if (Serial.available()) {
      num_char = (char)Serial.read();
      break;
    }
  }

  if (num_char < '1' || num_char > ('0' + number_of_memories)) {
    Serial.println(F("Cancelled."));
    return;
  }

  byte mem_num = num_char - '1';    // 0-based
  Serial.write(num_char);           // echo digit

  // Read message characters until CR / LF, writing each to EEPROM
  int mem_index = 0;
  int mem_max   = memory_end(mem_num) - memory_start(mem_num);  // max usable bytes

  deadline = millis() + 30000UL;   // 30 s to finish typing the message
  while (millis() < deadline) {
    if (!Serial.available()) continue;
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') break;
    if (mem_index >= mem_max) {
      // Slot full — drain remaining input until CR
      while (millis() < deadline) {
        if (Serial.available() && (Serial.read() == '\r' || Serial.read() == '\n')) break;
      }
      Serial.println(F(" [truncated]"));
      break;
    }
    Serial.write(c);  // echo
    EEPROM.update(memory_start(mem_num) + mem_index, (byte)toupper(c));
    mem_index++;
  }

  EEPROM.update(memory_start(mem_num) + mem_index, 255);  // sentinel

  Serial.println();
  Serial.print(F("Memory "));
  Serial.print((int)(mem_num + 1));
  Serial.println(F(" saved."));
}
#endif // FEATURE_MEMORIES

// ---------------------------------------------------------------------------

void process_cli_command(char cmd) {

  switch (cmd) {

    case 'A':
      configuration.keyer_mode = IAMBIC_A;
      config_dirty = 1;
      Serial.println(F("Iambic A"));
      break;

    case 'B':
      configuration.keyer_mode = IAMBIC_B;
      config_dirty = 1;
      Serial.println(F("Iambic B"));
      break;

    case 'G':
      configuration.keyer_mode = BUG;
      config_dirty = 1;
      Serial.println(F("Bug"));
      break;

    case 'F':
      serial_set_sidetone_freq();
      break;

    case 'I':
      tx_ptt.cw_tx_enabled          = !tx_ptt.cw_tx_enabled;
      configuration.cw_tx_enabled   = tx_ptt.cw_tx_enabled;
      config_dirty = 1;
      Serial.print(F("TX "));
      Serial.println(tx_ptt.cw_tx_enabled ? F("Enabled") : F("Disabled (sidetone only)"));
      break;

    case 'N':
      configuration.paddle_mode = (configuration.paddle_mode == PADDLE_NORMAL) ? PADDLE_REVERSE : PADDLE_NORMAL;
      config_dirty = 1;
      Serial.print(F("Paddles "));
      Serial.println((configuration.paddle_mode == PADDLE_NORMAL) ? F("Normal") : F("Reversed"));
      break;

    case 'S':
      serial_status();
      break;

    case 'T':
      serial_tune_command();
      break;

    case 'W':
      serial_wpm_set();
      break;

    case 'Y':
      serial_change_wordspace();
      break;

    #ifdef FEATURE_MEMORIES
    case '1': case '2': case '3':
      if ((cmd - '1') < number_of_memories) {
        play_memory(cmd - '1');
        Serial.print(F("Playing memory "));
        Serial.println((int)(cmd - '0'));
      } else {
        Serial.println(F("No such memory"));
      }
      break;

    case 'P':
      cli_program_memory();
      break;
    #endif // FEATURE_MEMORIES

    case '?':
      #ifdef FEATURE_SERIAL_HELP
        print_serial_help();
      #else
        Serial.println(F("Enable FEATURE_SERIAL_HELP for help text"));
      #endif
      break;

    case '\\':
      clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
      Serial.println(F("Buffer cleared"));
      break;

    case '~':
      #if defined(__AVR__)
        asm volatile ("jmp 0");
      #else
        setup();
      #endif
      break;

    // Not yet implemented — stubs matching v1 command letters
    case 'C': // Single paddle
    case 'D': // Ultimatic
    case 'E': // Set serial number
    case 'J': // Dah/dit ratio
    case 'K': // Training
    case 'L': // Weighting
    case 'M': // Farnsworth
    case 'O': // Sidetone mode cycle
    case 'Q': // QRSS mode
    case 'R': // Regular (non-QRSS) mode
    case 'U': // PTT toggle
    #ifdef FEATURE_POTENTIOMETER
    case 'V':
      pot_activated = !pot_activated;
      Serial.print(F("Pot "));
      Serial.println(pot_activated ? F("Active") : F("Inactive"));
      break;
    #endif // FEATURE_POTENTIOMETER
    case 'X': // Switch TX
    case 'Z': // Autospace
      Serial.println(F("Not implemented yet"));
      break;

    default:
      Serial.print(F("Unknown command: \\"));
      Serial.println(cmd);
      break;

  }

}

// ---------------------------------------------------------------------------
// service_serial() — main serial service routine called from loop()
// ---------------------------------------------------------------------------

void service_serial() {

  static boolean backslash_flag = false;

  while (Serial.available() > 0) {

    char incoming = (char)Serial.read();

    if (incoming == 27) {  // ESC — clear buffer immediately
      clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
      backslash_flag = false;
      continue;
    }

    if (backslash_flag) {
      backslash_flag = false;
      incoming = toupper(incoming);
      Serial.write(incoming);
      Serial.println();
      process_cli_command(incoming);
    } else if (incoming == '\\') {
      backslash_flag = true;
      Serial.write('\\');
    } else {
      // Regular character — queue for CW sending
      add_to_cw_char_send_buffer(&cw_scheduler, toupper(incoming));
    }

  }

}

#else // !FEATURE_COMMAND_LINE_INTERFACE

// Fallback: characters go directly to CW buffer, no commands
void service_serial() {
  while (Serial.available() > 0) {
    add_to_cw_char_send_buffer(&cw_scheduler, toupper((char)Serial.read()));
  }
}

#endif // FEATURE_COMMAND_LINE_INTERFACE

// ---------------------------------------------------------------------------
// FEATURE_MEMORIES — EEPROM-based CW message storage
// ---------------------------------------------------------------------------
//
// EEPROM layout (Arduino Uno/Nano = 1024 bytes):
//   Bytes 0 … sizeof(config_struct)-1   : configuration struct
//   Bytes sizeof(config_struct) … +4    : magic number / version headroom
//   Bytes memory_area_start … memory_area_end : N equal-sized memory slots
//
// Each slot stores ASCII bytes terminated by 0xFF (255).  An empty / erased
// slot starts with 0xFF.  Memory N starts at memory_start(N), ends at
// memory_end(N).  All N slots are equal size:
//   slot_size = (memory_area_end - memory_area_start) / number_of_memories
//
// ---------------------------------------------------------------------------

#ifdef FEATURE_MEMORIES

int memory_start(byte memory_number) {
  return (int)(memory_area_start +
    (unsigned int)memory_number * ((memory_area_end - memory_area_start) / number_of_memories));
}

int memory_end(byte memory_number) {
  return memory_start(memory_number) - 1 +
    (memory_area_end - memory_area_start) / number_of_memories;
}

// ---------------------------------------------------------------------------
// convert_cw_number_to_ascii — decode accumulated dit(1)/dah(2)/space(9) code
// ---------------------------------------------------------------------------
int convert_cw_number_to_ascii(long cw_code) {
  switch (cw_code) {
    case 12:    return 'A';
    case 2111:  return 'B';
    case 2121:  return 'C';
    case 211:   return 'D';
    case 1:     return 'E';
    case 1121:  return 'F';
    case 221:   return 'G';
    case 1111:  return 'H';
    case 11:    return 'I';
    case 1222:  return 'J';
    case 212:   return 'K';
    case 1211:  return 'L';
    case 22:    return 'M';
    case 21:    return 'N';
    case 222:   return 'O';
    case 1221:  return 'P';
    case 2212:  return 'Q';
    case 121:   return 'R';
    case 111:   return 'S';
    case 2:     return 'T';
    case 112:   return 'U';
    case 1112:  return 'V';
    case 122:   return 'W';
    case 2112:  return 'X';
    case 2122:  return 'Y';
    case 2211:  return 'Z';

    case 22222: return '0';
    case 12222: return '1';
    case 11222: return '2';
    case 11122: return '3';
    case 11112: return '4';
    case 11111: return '5';
    case 21111: return '6';
    case 22111: return '7';
    case 22211: return '8';
    case 22221: return '9';

    case 9:      return ' ';   // wordspace token
    case 21122:  return '/';
    case 21112:  return '=';   // BT
    case 211112: return '-';
    case 121212: return '.';
    case 221122: return ',';
    case 112211: return '?';
    case 122121: return '@';
    case 12121:  return '+';   // AR
    case 222222: return '\\';  // six dahs — special hack for backslash (macro prefix)
  }
  return -1;  // unknown
}

// ---------------------------------------------------------------------------
// play_memory — queue a stored memory for CW sending (non-blocking)
//
// With FEATURE_MEMORY_MACROS, backslash (0x5C) sequences are expanded:
//   \S        insert space
//   \E        send serial number, then increment
//   \C        send serial number with cut numbers (0=T, 9=N), then increment
//   \N        decrement serial number (no send)
//   \W###     change WPM (3 ASCII digits, e.g. \W025 = 25 WPM)
//   \Y#       increase WPM by # (1 digit)
//   \Z#       decrease WPM by # (1 digit)
//   \D###     delay ### seconds before continuing (capped at 255 s)
//   \U        assert PTT (manual key-up)
//   \V        release PTT (manual key-down)
//   \T###     key TX line for ### seconds (PTT on, delay, PTT off)
//   \F####    set sidetone frequency in Hz (4 digits, e.g. \F0700)
//   \I#       insert (inline expand) memory #  (\I1–\I9)
//   \0–\9     chain to memory (0 = memory 10, 1–9 = memories 1–9)
//
// Macros not yet implemented (\Q, \R, \H, \L, \X, \+) are silently skipped
// along with their parameter bytes.
// ---------------------------------------------------------------------------

#ifdef FEATURE_MEMORY_MACROS

static void queue_serial_number(bool cut_numbers) {
  char buf[8];
  itoa(serial_number, buf, 10);
  for (byte i = 0; buf[i] != '\0'; i++) {
    char c = buf[i];
    if (cut_numbers) {
      if (c == '0') c = 'T';
      else if (c == '9') c = 'N';
    }
    add_to_cw_char_send_buffer(&cw_scheduler, (byte)c);
  }
}

static void play_memory_with_depth(byte memory_number, byte depth);

static void play_memory_with_depth(byte memory_number, byte depth) {
  if (memory_number >= number_of_memories) return;
  if (depth >= 4) return;   // guard against infinite loops / deep chaining

  int y        = memory_start(memory_number);
  int end_addr = memory_end(memory_number);

  while (y <= end_addr) {
    byte b = EEPROM.read(y++);
    if (b == 255) break;    // end-of-memory sentinel

    if (b != '\\') {
      add_to_cw_char_send_buffer(&cw_scheduler, b);
      continue;
    }

    // Backslash — read macro character
    if (y > end_addr) break;
    byte mc = EEPROM.read(y++);
    if (mc == 255) break;

    // Memory chain: \0 = memory 10, \1–\9 = memories 1–9 (0-based: 9, 0–8)
    if (mc >= '0' && mc <= '9') {
      byte target = (mc == '0') ? 9 : (byte)(mc - '1');
      play_memory_with_depth(target, depth + 1);
      continue;
    }

    switch (mc) {

      case 'S':   // insert space
        add_to_cw_char_send_buffer(&cw_scheduler, ' ');
        break;

      case 'E':   // serial number, then increment
        queue_serial_number(false);
        serial_number++;
        break;

      case 'C':   // serial number with cut numbers, then increment
        queue_serial_number(true);
        serial_number++;
        break;

      case 'N':   // decrement serial number (no send)
        if (serial_number > 0) serial_number--;
        break;

      case 'I': { // insert memory — next byte is '1'–'9'
        if (y > end_addr) break;
        byte mc2 = EEPROM.read(y++);
        if (mc2 >= '1' && mc2 <= '9') {
          play_memory_with_depth((byte)(mc2 - '1'), depth + 1);
        }
        break;
      }

      case 'W': { // set WPM — 3 ASCII digit bytes follow (\W025 = 25 WPM)
        if (y + 2 > end_addr) { y = end_addr + 1; break; }
        byte d1 = EEPROM.read(y++);
        byte d2 = EEPROM.read(y++);
        byte d3 = EEPROM.read(y++);
        if (d1 >= '0' && d1 <= '9' && d2 >= '0' && d2 <= '9' && d3 >= '0' && d3 <= '9') {
          unsigned int wpm = (unsigned int)(d1-'0')*100 + (d2-'0')*10 + (d3-'0');
          if (wpm > 99) wpm = 99;
          if (wpm < 5)  wpm = 5;
          add_to_cw_char_send_buffer(&cw_scheduler, CW_CHAR_BUFFER_MACRO_WPM);
          add_to_cw_char_send_buffer(&cw_scheduler, (byte)wpm);
        }
        break;
      }

      case 'Y': { // increase WPM by 1 digit
        if (y > end_addr) break;
        byte dc = EEPROM.read(y++);
        if (dc >= '0' && dc <= '9') {
          unsigned int wpm = configuration.wpm + (dc - '0');
          if (wpm > 99) wpm = 99;
          add_to_cw_char_send_buffer(&cw_scheduler, CW_CHAR_BUFFER_MACRO_WPM);
          add_to_cw_char_send_buffer(&cw_scheduler, (byte)wpm);
        }
        break;
      }

      case 'Z': { // decrease WPM by 1 digit
        if (y > end_addr) break;
        byte dc = EEPROM.read(y++);
        if (dc >= '0' && dc <= '9') {
          byte delta = dc - '0';
          byte wpm = (configuration.wpm > (unsigned int)(delta + 5)) ?
                     (byte)(configuration.wpm - delta) : 5;
          add_to_cw_char_send_buffer(&cw_scheduler, CW_CHAR_BUFFER_MACRO_WPM);
          add_to_cw_char_send_buffer(&cw_scheduler, wpm);
        }
        break;
      }

      case 'D': { // delay — 3 ASCII digit bytes = seconds (\D005 = 5 s; max 255 s)
        if (y + 2 > end_addr) { y = end_addr + 1; break; }
        byte d1 = EEPROM.read(y++);
        byte d2 = EEPROM.read(y++);
        byte d3 = EEPROM.read(y++);
        if (d1 >= '0' && d1 <= '9' && d2 >= '0' && d2 <= '9' && d3 >= '0' && d3 <= '9') {
          unsigned int secs = (unsigned int)(d1-'0')*100 + (d2-'0')*10 + (d3-'0');
          byte sec_byte = (secs > 255) ? 255 : (byte)secs;
          if (sec_byte > 0) {
            add_to_cw_char_send_buffer(&cw_scheduler, CW_CHAR_BUFFER_MACRO_DELAY);
            add_to_cw_char_send_buffer(&cw_scheduler, sec_byte);
          }
        }
        break;
      }

      case 'U':   // assert PTT (manual)
        add_to_cw_char_send_buffer(&cw_scheduler, CW_CHAR_BUFFER_MACRO_PTT_ON);
        break;

      case 'V':   // release PTT (manual)
        add_to_cw_char_send_buffer(&cw_scheduler, CW_CHAR_BUFFER_MACRO_PTT_OFF);
        break;

      case 'T': { // key TX for ### seconds (PTT on, delay, PTT off)
        if (y + 2 > end_addr) { y = end_addr + 1; break; }
        byte d1 = EEPROM.read(y++);
        byte d2 = EEPROM.read(y++);
        byte d3 = EEPROM.read(y++);
        if (d1 >= '0' && d1 <= '9' && d2 >= '0' && d2 <= '9' && d3 >= '0' && d3 <= '9') {
          unsigned int secs = (unsigned int)(d1-'0')*100 + (d2-'0')*10 + (d3-'0');
          byte sec_byte = (secs > 255) ? 255 : (byte)secs;
          if (sec_byte > 0) {
            add_to_cw_char_send_buffer(&cw_scheduler, CW_CHAR_BUFFER_MACRO_PTT_ON);
            add_to_cw_char_send_buffer(&cw_scheduler, CW_CHAR_BUFFER_MACRO_DELAY);
            add_to_cw_char_send_buffer(&cw_scheduler, sec_byte);
            add_to_cw_char_send_buffer(&cw_scheduler, CW_CHAR_BUFFER_MACRO_PTT_OFF);
          }
        }
        break;
      }

      case 'F': { // set sidetone frequency — 4 ASCII digits (\F0600 = 600 Hz)
        if (y + 3 > end_addr) { y = end_addr + 1; break; }
        byte d1 = EEPROM.read(y++);
        byte d2 = EEPROM.read(y++);
        byte d3 = EEPROM.read(y++);
        byte d4 = EEPROM.read(y++);
        if (d1>='0'&&d1<='9' && d2>='0'&&d2<='9' && d3>='0'&&d3<='9' && d4>='0'&&d4<='9') {
          unsigned int hz = (unsigned int)(d1-'0')*1000 + (d2-'0')*100 + (d3-'0')*10 + (d4-'0');
          if (hz >= 100 && hz <= 4000) {
            configuration.sidetone_frequency = hz;
          }
        }
        break;
      }

      // Not yet in v2: \Q (QRSS), \R (normal speed), \H (Hell), \L (CW), \X (switch TX), \+ (prosign)
      default:
        break;
    }
  }
}

#endif // FEATURE_MEMORY_MACROS

void play_memory(byte memory_number) {
  if (memory_number >= number_of_memories) return;
  #ifdef FEATURE_MEMORY_MACROS
  play_memory_with_depth(memory_number, 0);
  #else
  for (int y = memory_start(memory_number); y <= memory_end(memory_number); y++) {
    byte b = EEPROM.read(y);
    if (b == 255) break;
    add_to_cw_char_send_buffer(&cw_scheduler, b);
  }
  #endif
}

// ---------------------------------------------------------------------------
// program_memory — interactively record a CW message into EEPROM
//
// Blocking: loops until button 0 pressed, both paddles squeezed with no
// element in progress, or memory slot is full.  Audio feedback (sidetone)
// is provided for each element entered by spinning on service_cw_scheduler().
// Safe to call from command mode (check_paddles early-return is already set).
// ---------------------------------------------------------------------------
void program_memory(byte memory_number) {
  if (memory_number >= number_of_memories) return;

  byte saved_tx = tx_ptt.cw_tx_enabled;
  clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
  tx_ptt.cw_tx_enabled = 0;  // sidetone practice only while programming

  #ifdef FEATURE_COMMAND_LINE_INTERFACE
  Serial.println();
  Serial.print(F("Pgm mem "));
  Serial.print(memory_number + 1);
  Serial.println(F(": key message; squeeze or button 0 to end"));
  #endif

  // Entry beep
  tone(sidetone_line, hz_high_beep);
  delay(100);
  noTone(sidetone_line);

  int  memory_location_index = 0;
  byte loop2 = 1;
  byte consecutive_spaces = 0;
  const byte max_consecutive_spaces = 3;   // caps trailing spaces; end-timeout governs actual exit
  #ifdef FEATURE_MEMORY_MACROS
  byte macro_flag = 0;  // when set, suppress wordspace after backslash so next char is the macro letter
  #endif

  // Two timers, both live outside the outer loop so they persist across characters:
  //   last_element_time  — reset on every dit/dah AND on every space written.
  //                        Drives wordspace detection (each space needs a fresh gap).
  //   last_dit_dah_time  — reset ONLY on actual dit/dah paddle hits.
  //                        Drives end-of-programming: fires when no real keying for
  //                        memory_program_end_timeout_ms, regardless of space resets.
  //
  // Each element's blocking spin already waits through the 1-dit inter-element keyup,
  // so letterspace/wordspace checks subtract 1 from the multiplier.
  unsigned long last_element_time  = millis();
  unsigned long last_dit_dah_time  = millis();

  // Wait for first paddle press or button 0 exit.
  // read_analog_buttons() (averaged) is used instead of raw analogRead to avoid
  // falsely exiting when button 1 is still held (button 1 ADC ≈ 93 < 100 raw threshold).
  while ((digitalRead(paddle_left) == HIGH) && (digitalRead(paddle_right) == HIGH)) {
    #ifdef FEATURE_BUTTONS
    if (read_analog_buttons() == 0) { loop2 = 0; break; }
    #endif
    service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
    service_sound(SERVICE, 0, 0);
  }

  while (loop2) {

    long cwchar = 0;
    byte paddle_hit = 0;
    // last_element_time intentionally NOT reset here — persists from last element sent
    byte loop1 = 1;

    while (loop1) {

      // Exit: button 0 pressed (use averaged read to avoid false trigger from button 1 ADC ≈ 93)
      #ifdef FEATURE_BUTTONS
      if (read_analog_buttons() == 0) { loop1 = 0; loop2 = 0; break; }
      #endif

      byte lp = digitalRead(paddle_left);
      byte rp = digitalRead(paddle_right);

      if ((lp == LOW) && (rp == HIGH)) {
        // Dit
        send_dit(&cw_scheduler, MANUAL_SENDING);
        while ((cw_scheduler.element_send_buffer_bytes > 0) ||
               (cw_scheduler.cw_scheduler_state != IDLE)) {
          service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
          service_sound(SERVICE, 0, 0);
        }
        cwchar = cwchar * 10 + 1;
        paddle_hit = 1;
        consecutive_spaces = 0;
        last_element_time = last_dit_dah_time = millis();

      } else if ((rp == LOW) && (lp == HIGH)) {
        // Dah
        send_dah(&cw_scheduler, MANUAL_SENDING);
        while ((cw_scheduler.element_send_buffer_bytes > 0) ||
               (cw_scheduler.cw_scheduler_state != IDLE)) {
          service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
          service_sound(SERVICE, 0, 0);
        }
        cwchar = cwchar * 10 + 2;
        paddle_hit = 1;
        consecutive_spaces = 0;
        last_element_time = last_dit_dah_time = millis();

      } else if ((lp == LOW) && (rp == LOW) && !paddle_hit) {
        // Squeeze with nothing keyed yet in this char → end of message
        loop1 = 0; loop2 = 0;

      } else {
        // Both HIGH (no paddle): check letterspace / wordspace timeouts.
        // Subtract 1 from each multiplier because the element spin already consumed
        // 1 dit of inter-element silence, so only (N-1) additional dits are needed.
        unsigned long dit_ms = 1200 / configuration.wpm;
        if (paddle_hit) {
          // Letterspace: standard 3 dits total; 1 already elapsed → wait 2 more
          if ((millis() - last_element_time) > (dit_ms * (cw_scheduler.length_letterspace - 1))) {
            loop1 = 0;  // character complete
          }
        } else if (memory_location_index > 0) {
          // Only check for spaces/end after the first character has been keyed.
          unsigned long now = millis();
          // End-of-programming: based on last real dit/dah, not last space write.
          // Spaces reset last_element_time but NOT last_dit_dah_time, so this
          // timeout correctly fires even while spaces keep being inserted.
          if ((now - last_dit_dah_time) > (unsigned long)memory_program_end_timeout_ms) {
            loop1 = 0; loop2 = 0;
          } else if ((now - last_element_time) > (dit_ms * (cw_scheduler.length_wordspace - 1))) {
            // Wordspace: standard 7 dits; 1 already elapsed in spin → wait 6 more.
            // Only insert if under the consecutive-space cap (prevents filling slot with spaces).
            // Suppress wordspace immediately after a backslash so the macro letter follows directly.
            #ifdef FEATURE_MEMORY_MACROS
            if (consecutive_spaces < max_consecutive_spaces && !macro_flag) {
            #else
            if (consecutive_spaces < max_consecutive_spaces) {
            #endif
              cwchar = 9;   // space token
              loop1 = 0;
            }
            // If cap reached, just keep spinning — end-timeout will fire eventually.
          }
        }
      }

      service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
      service_sound(SERVICE, 0, 0);
    } // inner loop

    if (!loop2) break;

    // Write the decoded character to EEPROM
    if (cwchar > 0) {
      int ascii = convert_cw_number_to_ascii(cwchar);
      if (ascii > 0) {
        EEPROM.update(memory_start(memory_number) + memory_location_index, (byte)ascii);
        memory_location_index++;
        #ifdef FEATURE_COMMAND_LINE_INTERFACE
        Serial.write((byte)ascii);
        #endif
        if (cwchar == 9) {
          // Reset last_element_time so the NEXT wordspace requires a fresh gap.
          // last_dit_dah_time is intentionally NOT reset here — it only moves on real elements.
          last_element_time = millis();
          consecutive_spaces++;
        } else {
          consecutive_spaces = 0;
          #ifdef FEATURE_MEMORY_MACROS
          macro_flag = (ascii == '\\') ? 1 : 0;  // set flag after backslash, clear on any other char
          #endif
        }
      }
    }

    // Memory slot full?
    if ((memory_start(memory_number) + memory_location_index) >= memory_end(memory_number)) {
      loop2 = 0;
    }
  } // outer loop

  // Write terminating sentinel
  EEPROM.update(memory_start(memory_number) + memory_location_index, 255);

  #ifdef FEATURE_COMMAND_LINE_INTERFACE
  Serial.println();
  Serial.print(F("Memory "));
  Serial.print(memory_number + 1);
  Serial.println(F(" saved"));
  #endif

  tx_ptt.cw_tx_enabled = saved_tx;

  // Confirmation beep
  tone(sidetone_line, hz_high_beep);
  delay(100);
  noTone(sidetone_line);
}

#endif // FEATURE_MEMORIES

// ---------------------------------------------------------------------------
// say_hi() — send HI in CW via sidetone at startup
//
// Controlled by OPTION_DO_NOT_SAY_HI in keyer_2_features_and_options.h.
// HI_TEXT can be overridden to send a different string.
// TX is disabled during the startup send; sidetone only.
// This is the one blocking call in the codebase — it runs once at boot.
// ---------------------------------------------------------------------------

void say_hi() {

  #ifndef OPTION_DO_NOT_SAY_HI

  byte old_tx_enabled = tx_ptt.cw_tx_enabled;
  tx_ptt.cw_tx_enabled = 0;  // sidetone only during startup message

  const char *hi_text = HI_TEXT;
  for (int x = 0; hi_text[x] != 0; x++) {
    add_to_cw_char_send_buffer(&cw_scheduler, hi_text[x]);
  }

  // Wait for the send to complete before entering loop()
  while ((cw_scheduler.char_send_buffer_bytes > 0) ||
         (cw_scheduler.element_send_buffer_bytes > 0) ||
         (cw_scheduler.cw_scheduler_state != IDLE)) {
    service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
    service_sound(SERVICE, 0, 0);
  }

  tx_ptt.cw_tx_enabled = old_tx_enabled;

  #endif // OPTION_DO_NOT_SAY_HI

}

// ---------------------------------------------------------------------------
// FEATURE_BUTTONS
//
// Analog multiplexed button array — multiple buttons on one analog pin using
// a resistor voltage-divider ladder (ported from v1 ButtonArray library by W6IPA).
//
// Circuit: analog_buttons_pin → R1 pullup to VCC.
// Each button connects to GND through a different number of R2 resistors in series.
// With R1=10, R2=1 (relative units) and 10-bit ADC:
//   No button → ADC ≈ 1023 (pin pulled high)
//   Button 0  → ADC ≈   0  [range  0.. 46]  command mode entry
//   Button 1  → ADC ≈  93  [range 47..131]  memory 1 (future)
//   Button 2  → ADC ≈ 171  [range 132..203] memory 2 (future)
//   Button 3  → ADC ≈ 236  [range 203..264] memory 3 (future)
//
// Short press (<  analog_button_hold_ms): button action (cmd mode, memory play)
// Long  press (>= analog_button_hold_ms): alternate action (speed adjust, mem repeat)
// ---------------------------------------------------------------------------

#if defined(FEATURE_BUTTONS)

// ---------------------------------------------------------------------------
// read_analog_buttons()
// Samples the analog pin 19 times with an exponential moving average (same
// as v1 buttonarray.cpp), then identifies the button using the resistor-ladder
// threshold formula.  Returns button index 0..N-1, or -1 if no button pressed.
// ---------------------------------------------------------------------------

int8_t read_analog_buttons() {

  // Exponential moving average over 19 samples (v1 formula: avg = (avg+sample)/2)
  int32_t avg = 0;
  for (byte i = 0; i < 19; i++) {
    int32_t s = (int32_t)analogRead(analog_buttons_pin);
    avg = (avg + s) / 2;
  }

  // Quick rejection — no button pressed when pin is near full scale
  if (avg > 600) return -1;

  // Scan buttons 0..N-1 using the v1 threshold formula:
  //   btn_v  = 1023 * (step * R2) / (step * R2 + R1)
  //   low    = btn_v - (btn_v - prev_v) / 2     (negative for button 0 → clamps to 0)
  //   high   = btn_v + (next_v - btn_v) / 2
  // Match condition: avg > low_limit && avg <= high_limit
  for (int8_t btn = 0; btn < analog_buttons_number_of_buttons; btn++) {
    float r1 = (float)analog_buttons_r1;
    float r2 = (float)analog_buttons_r2;

    float btn_v  = 1023.0f * (btn       * r2) / (btn       * r2 + r1);
    float prev_v = 1023.0f * ((btn - 1) * r2) / ((btn - 1) * r2 + r1); // negative for btn=0
    float next_v = 1023.0f * ((btn + 1) * r2) / ((btn + 1) * r2 + r1);

    int32_t low_lim  = (int32_t)(btn_v - (btn_v - prev_v) / 2.0f);
    int32_t high_lim = (int32_t)(btn_v + (next_v - btn_v) / 2.0f);

    if ((avg > low_lim) && (avg <= high_lim)) return btn;
  }

  return -1;
}

// ---------------------------------------------------------------------------
// check_buttons() — called every loop()
//
// Debounces, waits to distinguish short press from long press, then dispatches:
//   Button 0 short press → command_mode()
//   Button 0 long  press → speed adjust via paddle (no CW command entry)
//   Buttons 1+           → memory playback (placeholder; FEATURE_MEMORIES not yet implemented)
// ---------------------------------------------------------------------------

void check_buttons() {

  static unsigned long last_press_ms = 0;

  // Debounce gate
  if ((millis() - last_press_ms) < (unsigned long)analog_button_debounce_ms) return;

  int8_t button = read_analog_buttons();
  if (button < 0) return;

  // Special case: button 0 is still being held from the long-press speed adjust.
  // CMD_SPEED_ADJUST monitors the button pin directly for release; we just
  // refresh the debounce timer here so check_buttons() doesn't re-fire.
  #ifdef FEATURE_COMMAND_MODE
  if ((button == 0) && (command_mode_state == CMD_SPEED_ADJUST) && !command_mode_full) {
    last_press_ms = millis();
    return;
  }
  #endif

  unsigned long press_start = millis();
  last_press_ms = press_start;

  // Wait up to analog_button_hold_ms to distinguish short vs long press.
  // Keep the CW engine alive throughout.
  while ((millis() - press_start) < (unsigned long)analog_button_hold_ms) {
    service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
    service_sound(SERVICE, 0, 0);
    if (read_analog_buttons() < 0) break;   // released early → short press
  }

  // Held if and only if the full hold window elapsed (timer-based, not a re-read).
  // Re-reading the button after breaking early can give a false held=true if the
  // button is slow to float up electrically or has contact bounce.
  byte held = ((millis() - press_start) >= (unsigned long)analog_button_hold_ms);

  // Long press on button 0: enter speed adjust immediately WITHOUT waiting for release.
  // V1 behavior: user holds button + presses paddles simultaneously to adjust WPM.
  // service_command_mode()/CMD_SPEED_ADJUST detects button release as the exit condition.
  #ifdef FEATURE_COMMAND_MODE
  if ((button == 0) && held && (command_mode_state == CMD_IDLE)) {
    command_mode_enter_speed_adjust();
    last_press_ms = millis();   // reset debounce so the still-held button doesn't re-fire
    return;                     // skip wait-for-release; CMD_SPEED_ADJUST owns the exit
  }
  #endif

  // Long press on button 1+: program the corresponding memory (blocking interactive).
  // Wait-for-release happens inside program_memory() (user ends with squeeze/button).
  #ifdef FEATURE_MEMORIES
  if (held && (button > 0) && (button <= number_of_memories)) {
    program_memory(button - 1);
    last_press_ms = millis();
    return;
  }
  #endif

  // All other cases: wait for button release before dispatching.
  while (read_analog_buttons() >= 0) {
    service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
    service_sound(SERVICE, 0, 0);
  }

  // --- Dispatch ---

  switch (button) {

    case 0:   // Command button (short press, or press while already in command mode)
      #ifdef FEATURE_COMMAND_MODE
      if (command_mode_state != CMD_IDLE) {
        // Pressed while already in command mode → trigger exit
        if ((command_mode_state != CMD_EXIT_1) && (command_mode_state != CMD_EXIT_2)) {
          command_mode_exit_start();
        }
      } else {
        command_mode_enter();   // short press → full command mode with entry sound
      }
      #endif // FEATURE_COMMAND_MODE
      break;

    default:  // Buttons 1+ → play corresponding memory
      #ifdef FEATURE_MEMORIES
      if ((button > 0) && (button <= number_of_memories)) {
        play_memory(button - 1);
      }
      #else
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      Serial.print(F("Button "));
      Serial.print(button);
      Serial.println(F(" (no memory feature)"));
      #endif
      #endif // FEATURE_MEMORIES
      break;
  }

}

#elif defined(FEATURE_COMMAND_MODE) && (command_button > 0)

// ---------------------------------------------------------------------------
// check_buttons() fallback — single digital command button, no FEATURE_BUTTONS
// ---------------------------------------------------------------------------

void check_buttons() {
  static byte button_state = HIGH;
  byte current = digitalRead(command_button);
  if ((current == LOW) && (button_state == HIGH)) {
    delay(30);
    if (digitalRead(command_button) == LOW) {
      if (command_mode_state != CMD_IDLE) {
        if ((command_mode_state != CMD_EXIT_1) && (command_mode_state != CMD_EXIT_2)) {
          command_mode_exit_start();
        }
      } else {
        command_mode_enter();
      }
    }
  }
  button_state = current;
}

#endif // FEATURE_BUTTONS / fallback

// ---------------------------------------------------------------------------
// FEATURE_COMMAND_MODE  (non-blocking state machine)
//
// Enter:  short press on command button → command_mode_enter()
//         long  press on command button → command_mode_enter_speed_adjust()
// Exit:   key X (-..-), press button again, or squeeze during speed/tune
//
// State flow:
//   enter()  → CMD_ENTRY_1 → CMD_ENTRY_2 → CMD_INPUT
//   CMD_INPUT → CMD_DISPATCH → CMD_WAIT_ACK → CMD_INPUT  (normal commands)
//                            → CMD_SPEED_ADJUST → CMD_WAIT_ACK → CMD_INPUT  (W)
//                            → CMD_TUNE → CMD_WAIT_ACK → CMD_INPUT          (T)
//                            → CMD_EXIT_1 → CMD_EXIT_2 → CMD_IDLE           (X)
//   exit_start() reachable from any state → CMD_EXIT_1 → CMD_EXIT_2 → CMD_IDLE
//
// Commands (keyed as CW on the paddle):
//   A  (.-   = 12)    Iambic A
//   B  (-... = 2111)  Iambic B
//   G  (--.  = 221)   Bug mode (restored on exit)
//   I  (..   = 11)    TX enable/disable toggle
//   N  (-.   = 21)    Toggle paddle reverse
//   T  (-    = 2)     Tune: left=momentary TX, right=latch TX, squeeze=exit
//   W  (.--  = 122)   WPM: continuous dits, left=+1WPM, right=-1WPM, squeeze=exit
//   X  (-..- = 2112)  Exit command mode
// ---------------------------------------------------------------------------

#ifdef FEATURE_COMMAND_MODE

// ---------------------------------------------------------------------------
// get_cw_char_from_paddles()
// Blocks until the user finishes keying one CW character on the paddles.
// Returns accumulated digit code: DIT=1, DAH=2 (e.g. A=12, N=21, T=2).
// Returns 9 if the command button is pressed (exit signal).
// Returns 0 on overflow (too many elements — treat as unknown).
// CW scheduler is serviced throughout so sidetone plays normally.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// command_mode_exit_start() — begins exit sequence from any active state
// ---------------------------------------------------------------------------

void command_mode_exit_start() {
  if (tx_ptt.key_state != RIG_RECEIVE) {
    cw_key(&tx_ptt, RIG_RECEIVE, &configuration);
  }
  clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
  tone(sidetone_line, hz_high_beep);
  command_mode_sound_timer = millis();
  command_mode_state = CMD_EXIT_1;
}

// ---------------------------------------------------------------------------
// command_mode_dispatch() — called from CMD_DISPATCH, processes command_mode_cw_char
// ---------------------------------------------------------------------------

void command_mode_dispatch() {

  unsigned int cw = command_mode_cw_char;

  if (cw == 2112) {           // X (-..-) → exit
    command_mode_exit_start();
    return;
  }

  if (cw == 0) {              // overflow / unknown
    add_to_cw_char_send_buffer(&cw_scheduler, '?');
    command_mode_state = CMD_WAIT_ACK;
    return;
  }

  switch (cw) {

    case 12:    // A (.-)
      command_mode_saved_keyer = IAMBIC_A;
      configuration.keyer_mode = IAMBIC_A;
      config_dirty = 1;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      Serial.println(F("Iambic A"));
      #endif
      break;

    case 2111:  // B (-..)
      command_mode_saved_keyer = IAMBIC_B;
      configuration.keyer_mode = IAMBIC_B;
      config_dirty = 1;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      Serial.println(F("Iambic B"));
      #endif
      break;

    case 221:   // G (--.)  — bug restored on exit, stay iambic for cmd input
      command_mode_saved_keyer = BUG;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      Serial.println(F("Bug mode (on exit)"));
      #endif
      break;

    case 11:    // I (..)
      command_mode_saved_tx           = !command_mode_saved_tx;
      configuration.cw_tx_enabled     = command_mode_saved_tx;
      config_dirty = 1;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      Serial.print(F("TX "));
      Serial.println(command_mode_saved_tx ? F("Enabled") : F("Disabled"));
      #endif
      break;

    case 21:    // N (-.)
      configuration.paddle_mode = (configuration.paddle_mode == PADDLE_NORMAL) ?
                                    PADDLE_REVERSE : PADDLE_NORMAL;
      config_dirty = 1;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      Serial.print(F("Paddles "));
      Serial.println((configuration.paddle_mode == PADDLE_NORMAL) ? F("Normal") : F("Reversed"));
      #endif
      break;

    case 2:     // T (-)  — enter tune sub-state
      tx_ptt.cw_tx_enabled     = 1;
      command_mode_tune_latched = 0;
      command_mode_tune_rprev  = HIGH;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      Serial.println(F("Tune: left=momentary  right=latch  squeeze=exit"));
      #endif
      command_mode_state = CMD_TUNE;
      return;   // ack after squeeze exits tune

    case 122:   // W (.--) — enter speed adjust sub-state
      command_mode_state = CMD_SPEED_ADJUST;
      return;   // ack when squeeze exits speed adjust

    #ifdef FEATURE_MEMORIES
    case 1221:  // P (.--.) — program a memory; next CW digit selects which one
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      Serial.println(F("Pgm memory: key 1-3"));
      #endif
      command_mode_cw_char         = 0;
      command_mode_element_in_prog = 0;
      command_mode_idle_since      = 0;
      command_mode_state           = CMD_PROGRAM_MEM_WAIT;
      return;   // don't send ack yet; wait for memory-number digit
    #endif

    default:
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      Serial.print(F("? ("));
      Serial.print(cw);
      Serial.println(F(")"));
      #endif
      add_to_cw_char_send_buffer(&cw_scheduler, '?');
      command_mode_state = CMD_WAIT_ACK;
      return;
  }

  // Queue ack for all single-dispatch commands
  add_to_cw_char_send_buffer(&cw_scheduler, command_mode_acknowledgement_character);
  command_mode_state = CMD_WAIT_ACK;
}

// ---------------------------------------------------------------------------
// command_mode_enter() — called on short button press
// Saves state, plays entry boop-beep, then enters CMD_INPUT.
// ---------------------------------------------------------------------------

void command_mode_enter() {
  if (command_mode_state != CMD_IDLE) return;

  keyer_machine_mode       = KEYER_COMMAND_MODE;
  command_mode_full        = 1;
  command_mode_saved_tx    = tx_ptt.cw_tx_enabled;
  command_mode_saved_keyer = configuration.keyer_mode;

  clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
  tx_ptt.cw_tx_enabled = 0;

  if ((configuration.keyer_mode != IAMBIC_A) && (configuration.keyer_mode != IAMBIC_B)) {
    configuration.keyer_mode = IAMBIC_B;
  }

  // Start entry sound: low tone first; service_command_mode() advances the rest
  tone(sidetone_line, hz_low_beep);
  command_mode_sound_timer = millis();
  command_mode_state = CMD_ENTRY_1;
}

// ---------------------------------------------------------------------------
// command_mode_enter_speed_adjust() — called on long button press
// Goes straight to WPM adjust, no entry sound.
// ---------------------------------------------------------------------------

void command_mode_enter_speed_adjust() {
  if (command_mode_state != CMD_IDLE) return;

  keyer_machine_mode       = KEYER_COMMAND_MODE;
  command_mode_full        = 0;
  command_mode_saved_tx    = tx_ptt.cw_tx_enabled;
  command_mode_saved_keyer = configuration.keyer_mode;

  clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
  tx_ptt.cw_tx_enabled = 0;

  if ((configuration.keyer_mode != IAMBIC_A) && (configuration.keyer_mode != IAMBIC_B)) {
    configuration.keyer_mode = IAMBIC_B;
  }

  #ifdef FEATURE_COMMAND_LINE_INTERFACE
  Serial.println(F("Speed: left=+WPM  right=-WPM  squeeze=done"));
  #endif

  command_mode_state = CMD_SPEED_ADJUST;
}

// ---------------------------------------------------------------------------
// service_command_mode() — non-blocking state machine, called every loop()
// ---------------------------------------------------------------------------

void service_command_mode() {

  if (command_mode_state == CMD_IDLE) return;

  switch (command_mode_state) {

    // --- Entry sounds ---

    case CMD_ENTRY_1:
      // Low tone started by command_mode_enter(); after 100 ms switch to high
      if ((millis() - command_mode_sound_timer) >= 100) {
        tone(sidetone_line, hz_high_beep);
        command_mode_sound_timer = millis();
        command_mode_state = CMD_ENTRY_2;
      }
      break;

    case CMD_ENTRY_2:
      // High tone; after 100 ms silence and begin accepting input
      if ((millis() - command_mode_sound_timer) >= 100) {
        noTone(sidetone_line);
        service_sound(TONE_OFF, 0, 0);   // sync service_sound state
        command_mode_cw_char         = 0;
        command_mode_element_in_prog = 0;
        command_mode_idle_since      = 0;
        command_mode_state           = CMD_INPUT;
        #ifdef FEATURE_COMMAND_LINE_INTERFACE
        Serial.println(F("\r\nCommand mode  (X to exit)"));
        #endif
      }
      break;

    // --- CW input ---

    case CMD_INPUT:
      if ((cw_scheduler.cw_scheduler_state       == IDLE) &&
          (cw_scheduler.element_send_buffer_bytes == 0)) {

        // An element just finished — start the letterspace timer
        if (command_mode_element_in_prog) {
          command_mode_element_in_prog = 0;
          command_mode_idle_since      = millis();
        }

        byte left  = digitalRead(paddle_left);
        byte right = digitalRead(paddle_right);

        if (left == LOW) {
          send_dit(&cw_scheduler, MANUAL_SENDING);
          command_mode_cw_char         = command_mode_cw_char * 10 + 1;
          command_mode_element_in_prog = 1;
          command_mode_idle_since      = 0;
        } else if (right == LOW) {
          send_dah(&cw_scheduler, MANUAL_SENDING);
          command_mode_cw_char         = command_mode_cw_char * 10 + 2;
          command_mode_element_in_prog = 1;
          command_mode_idle_since      = 0;
        } else if ((command_mode_cw_char  > 0) &&
                   (command_mode_idle_since > 0) &&
                   !command_mode_element_in_prog) {
          // Check letterspace timeout
          unsigned int dit_ms         = 1200 / configuration.wpm;
          unsigned int letterspace_ms = dit_ms * cw_scheduler.length_letterspace;
          if ((millis() - command_mode_idle_since) > letterspace_ms) {
            command_mode_state = CMD_DISPATCH;
          }
        }

        // Overflow guard (> 5 elements)
        if (command_mode_cw_char > 22222) {
          command_mode_cw_char = 0;
          command_mode_state   = CMD_DISPATCH;
        }
      }
      break;

    // --- Dispatch ---

    case CMD_DISPATCH:
      command_mode_dispatch();
      break;

    // --- Wait for ack / '?' CW to finish ---

    case CMD_WAIT_ACK:
      if ((cw_scheduler.char_send_buffer_bytes   == 0) &&
          (cw_scheduler.element_send_buffer_bytes == 0) &&
          (cw_scheduler.cw_scheduler_state        == IDLE)) {
        command_mode_cw_char         = 0;
        command_mode_element_in_prog = 0;
        command_mode_idle_since      = 0;
        command_mode_state           = command_mode_full ? CMD_INPUT : CMD_IDLE;
        if (command_mode_state == CMD_IDLE) {
          // Speed-adjust-only session complete — restore state
          tx_ptt.cw_tx_enabled     = command_mode_saved_tx;
          configuration.keyer_mode  = command_mode_saved_keyer;
          keyer_machine_mode        = KEYER_NORMAL;
          #ifdef FEATURE_COMMAND_LINE_INTERFACE
          Serial.println(F("Normal mode"));
          #endif
        }
      }
      break;

    // --- Speed adjust (W command or long press) ---

    case CMD_SPEED_ADJUST:

      // Long-press path (command_mode_full == 0): exit when the command button
      // is released.  A single analogRead() > 600 means the pin is near VCC —
      // no button pressed.  This restores V1 "hold button + paddle to adjust"
      // behavior: the user releases the button when they're satisfied with the speed.
      #ifdef FEATURE_BUTTONS
      if (!command_mode_full) {
        if (analogRead(analog_buttons_pin) > 600) {
          // Button released — clean up and return to normal mode
          clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
          config_dirty = 1;
          #ifdef FEATURE_COMMAND_LINE_INTERFACE
          Serial.print(F("WPM: "));
          Serial.println(configuration.wpm);
          #endif
          tx_ptt.cw_tx_enabled     = command_mode_saved_tx;
          configuration.keyer_mode  = command_mode_saved_keyer;
          keyer_machine_mode        = KEYER_NORMAL;
          command_mode_state        = CMD_IDLE;
          break;
        }
      }
      #endif // FEATURE_BUTTONS

      if ((cw_scheduler.cw_scheduler_state       == IDLE) &&
          (cw_scheduler.element_send_buffer_bytes == 0)) {

        byte left  = digitalRead(paddle_left);
        byte right = digitalRead(paddle_right);

        if ((left == LOW) && (right == LOW)) {
          // Squeeze = done (also works as exit for full command mode W command)
          config_dirty = 1;
          #ifdef FEATURE_COMMAND_LINE_INTERFACE
          Serial.print(F("WPM: "));
          Serial.println(configuration.wpm);
          #endif
          if (command_mode_full) {
            add_to_cw_char_send_buffer(&cw_scheduler, command_mode_acknowledgement_character);
            command_mode_state = CMD_WAIT_ACK;
          } else {
            // Long-press speed adjust: button already held, squeeze also exits cleanly
            clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
            tx_ptt.cw_tx_enabled     = command_mode_saved_tx;
            configuration.keyer_mode  = command_mode_saved_keyer;
            keyer_machine_mode        = KEYER_NORMAL;
            command_mode_state        = CMD_IDLE;
          }
          break;
        }

        if (left  == LOW && configuration.wpm < 99) configuration.wpm++;
        if (right == LOW && configuration.wpm > 1)  configuration.wpm--;

        send_dit(&cw_scheduler, AUTOMATIC_SENDING);
      }
      break;

    // --- Tune (T command) ---

    case CMD_TUNE: {
      byte left  = digitalRead(paddle_left);
      byte right = digitalRead(paddle_right);

      if ((left == LOW) && (right == LOW)) {
        // Squeeze = exit tune
        if (tx_ptt.key_state != RIG_RECEIVE) {
          cw_key(&tx_ptt, RIG_RECEIVE, &configuration);
        }
        tx_ptt.cw_tx_enabled = 0;
        add_to_cw_char_send_buffer(&cw_scheduler, command_mode_acknowledgement_character);
        command_mode_state = CMD_WAIT_ACK;
        break;
      }

      // Left paddle = momentary TX
      if (!command_mode_tune_latched) {
        if ((left == LOW) && (tx_ptt.key_state == RIG_RECEIVE)) {
          cw_key(&tx_ptt, RIG_TRANSMIT, &configuration);
        } else if ((left == HIGH) && (tx_ptt.key_state != RIG_RECEIVE)) {
          cw_key(&tx_ptt, RIG_RECEIVE, &configuration);
        }
      }

      // Right paddle = latch toggle on each new press
      if ((right == LOW) && (command_mode_tune_rprev == HIGH)) {
        command_mode_tune_latched = !command_mode_tune_latched;
        cw_key(&tx_ptt,
               command_mode_tune_latched ? RIG_TRANSMIT : RIG_RECEIVE,
               &configuration);
      }
      command_mode_tune_rprev = right;
      break;
    }

    // --- Program memory (P command): wait for memory-number digit ---
    #ifdef FEATURE_MEMORIES
    case CMD_PROGRAM_MEM_WAIT:
      if ((cw_scheduler.cw_scheduler_state       == IDLE) &&
          (cw_scheduler.element_send_buffer_bytes == 0)) {

        if (command_mode_element_in_prog) {
          command_mode_element_in_prog = 0;
          command_mode_idle_since      = millis();
        }

        byte left  = digitalRead(paddle_left);
        byte right = digitalRead(paddle_right);

        if (left == LOW) {
          send_dit(&cw_scheduler, MANUAL_SENDING);
          command_mode_cw_char         = command_mode_cw_char * 10 + 1;
          command_mode_element_in_prog = 1;
          command_mode_idle_since      = 0;
        } else if (right == LOW) {
          send_dah(&cw_scheduler, MANUAL_SENDING);
          command_mode_cw_char         = command_mode_cw_char * 10 + 2;
          command_mode_element_in_prog = 1;
          command_mode_idle_since      = 0;
        } else if ((command_mode_cw_char  > 0) &&
                   (command_mode_idle_since > 0) &&
                   !command_mode_element_in_prog) {
          unsigned int dit_ms         = 1200 / configuration.wpm;
          unsigned int letterspace_ms = dit_ms * cw_scheduler.length_letterspace;
          if ((millis() - command_mode_idle_since) > letterspace_ms) {
            // Map CW digit to memory number (0-based)
            byte mem_num = 255;
            switch (command_mode_cw_char) {
              case 12222: mem_num = 0; break;   // 1
              case 11222: mem_num = 1; break;   // 2
              case 11122: mem_num = 2; break;   // 3
            }
            if (mem_num < number_of_memories) {
              program_memory(mem_num);
              add_to_cw_char_send_buffer(&cw_scheduler, command_mode_acknowledgement_character);
            } else {
              add_to_cw_char_send_buffer(&cw_scheduler, '?');
            }
            command_mode_cw_char = 0;
            command_mode_state   = CMD_WAIT_ACK;
          }
        }

        // Overflow guard
        if (command_mode_cw_char > 22222) {
          add_to_cw_char_send_buffer(&cw_scheduler, '?');
          command_mode_cw_char = 0;
          command_mode_state   = CMD_WAIT_ACK;
        }
      }
      break;
    #endif // FEATURE_MEMORIES

    // --- Exit sounds ---

    case CMD_EXIT_1:
      // High tone started by command_mode_exit_start(); after 100 ms switch to low
      if ((millis() - command_mode_sound_timer) >= 100) {
        tone(sidetone_line, hz_low_beep);
        command_mode_sound_timer = millis();
        command_mode_state = CMD_EXIT_2;
      }
      break;

    case CMD_EXIT_2:
      // Low tone; after 100 ms restore state and return to normal
      if ((millis() - command_mode_sound_timer) >= 100) {
        noTone(sidetone_line);
        service_sound(TONE_OFF, 0, 0);   // sync service_sound state
        tx_ptt.cw_tx_enabled     = command_mode_saved_tx;
        configuration.keyer_mode  = command_mode_saved_keyer;
        keyer_machine_mode        = KEYER_NORMAL;
        command_mode_state        = CMD_IDLE;
        #ifdef FEATURE_COMMAND_LINE_INTERFACE
        Serial.println(F("Normal mode"));
        #endif
      }
      break;

  } // end switch
}

#endif // FEATURE_COMMAND_MODE
