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

#define CODE_VERSION "2-20260630.0003"

#include "keyer_2_serial.h"
#include "keyer_2.h"
#include "keyer_2_features_and_options.h"
#include "keyer_2_pin_settings.h"
#include "keyer_2_cw.h"

#ifdef FEATURE_WINKEY_EMULATION
#include "keyer_2_winkey.h"
#endif

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

#ifdef FEATURE_WINKEY_EMULATION
WinkeyState winkey_state;
#endif

// Serial port array — built in setup() from KEYER_SERIAL_PORT_* defines
Stream* primary_serial_port = &Serial;   // set before each CLI port service call
KeyerSerialPort keyer_serial_ports[KEYER_MAX_SERIAL_PORTS];
uint8_t keyer_num_serial_ports = 0;

#ifdef FEATURE_PADDLE_ECHO
long          paddle_echo_buffer            = 0;
unsigned long paddle_echo_buffer_decode_time = 0;
byte          paddle_echo_active            = 1;   // toggled by \*
#endif

#ifdef FEATURE_POTENTIOMETER
byte pot_wpm_low_value  = initial_pot_wpm_low_value;
byte pot_wpm_high_value = initial_pot_wpm_high_value;
byte last_pot_wpm_read  = 0;
byte pot_activated      = potentiometer_always_on;
#endif

#ifdef FEATURE_ROTARY_ENCODER
// Full-step state table (emits code at 00 only).  From Ben Buxton's implementation.
#ifdef OPTION_ENCODER_HALF_STEP_MODE
  static const unsigned char ttable[6][4] PROGMEM = {
    {0x3 , 0x2, 0x1,  0x0}, {0x23, 0x0, 0x1, 0x0},
    {0x13, 0x2, 0x0,  0x0}, {0x3 , 0x5, 0x4, 0x0},
    {0x3 , 0x3, 0x4, 0x10}, {0x3 , 0x5, 0x3, 0x20}
  };
#else
  static const unsigned char ttable[7][4] PROGMEM = {
    {0x0, 0x2, 0x4,  0x0}, {0x3, 0x0, 0x1, 0x10},
    {0x3, 0x2, 0x0,  0x0}, {0x3, 0x2, 0x1,  0x0},
    {0x6, 0x0, 0x4,  0x0}, {0x6, 0x5, 0x0, 0x20},
    {0x6, 0x5, 0x4,  0x0},
  };
#endif
#define DIR_CCW 0x10
#define DIR_CW  0x20
static unsigned char encoder_state = 0;
#endif // FEATURE_ROTARY_ENCODER

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

// Memory programming mode sub-states (used when keyer_machine_mode == KEYER_MEMORY_PROGRAM)
#define MEM_PROG_ENTRY_BEEP  0   // playing entry beep
#define MEM_PROG_INTER       1   // between elements; watch paddles and timeouts
#define MEM_PROG_ELEMENT     2   // element queued; waiting for scheduler idle
#define MEM_PROG_EXIT_BEEP   3   // playing exit beep; then return to prior mode

byte          mem_prog_state         = MEM_PROG_ENTRY_BEEP;
byte          mem_prog_slot          = 0;
long          mem_prog_cwchar        = 0;
byte          mem_prog_paddle_hit    = 0;
int           mem_prog_index         = 0;
byte          mem_prog_consec_sp     = 0;
unsigned long mem_prog_last_elem     = 0;
unsigned long mem_prog_last_dd       = 0;
unsigned long mem_prog_sound_until   = 0;
byte          mem_prog_saved_tx      = 0;
byte          mem_prog_from_cmd_mode = 0;   // 1 = entered from command mode P-command
#ifdef FEATURE_MEMORY_MACROS
byte          mem_prog_macro_flag    = 0;
#endif
#endif // FEATURE_MEMORIES

// ---------------------------------------------------------------------------
// forward declarations for functions defined below
// ---------------------------------------------------------------------------

void initialize_state();
#if defined(FEATURE_BEACON) && defined(FEATURE_MEMORIES)
void service_beacon_mode();
#endif
#ifdef FEATURE_POTENTIOMETER
byte pot_value_wpm();
void check_potentiometer();
#endif
#ifdef FEATURE_ROTARY_ENCODER
void speed_change(int change);
int  chk_rotary_encoder();
void check_rotary_encoder();
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
#ifdef FEATURE_FARNSWORTH
void serial_set_farnsworth();
#endif
void serial_tune_command();
void serial_status();
#ifdef FEATURE_SERIAL_HELP
void print_serial_help();
#endif
#endif
void say_hi();
#ifdef FEATURE_ADDITIONAL_TX_AND_PTT_PINS
void select_tx(byte tx_num);
#endif
#if defined(FEATURE_MEMORIES) || defined(FEATURE_PADDLE_ECHO)
int  convert_cw_number_to_ascii(long cw_code);
#endif
#ifdef FEATURE_PADDLE_ECHO
void service_paddle_echo();
#endif
#ifdef FEATURE_MEMORIES
int  memory_start(byte memory_number);
int  memory_end(byte memory_number);
void play_memory(byte memory_number);
void memory_program_enter(byte memory_number, byte from_cmd_mode);
void service_memory_program();
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void cli_program_memory();
#endif
#endif

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------

void setup() {

  #ifndef FEATURE_WINKEY_EMULATION
  delay(2000);
  #endif
  
  // Pin modes
  pinMode(paddle_left,      INPUT_PULLUP);
  pinMode(paddle_right,     INPUT_PULLUP);
  pinMode(tx_key_line_1,    OUTPUT);
  if (ptt_tx_1)   pinMode(ptt_tx_1,   OUTPUT);
  pinMode(sidetone_line,    OUTPUT);
  #ifdef FEATURE_ADDITIONAL_TX_AND_PTT_PINS
  if (tx_key_line_2) { pinMode(tx_key_line_2, OUTPUT); }
  if (tx_key_line_3) { pinMode(tx_key_line_3, OUTPUT); }
  if (tx_key_line_4) { pinMode(tx_key_line_4, OUTPUT); }
  if (tx_key_line_5) { pinMode(tx_key_line_5, OUTPUT); }
  if (tx_key_line_6) { pinMode(tx_key_line_6, OUTPUT); }
  if (ptt_tx_2)      { pinMode(ptt_tx_2,      OUTPUT); }
  if (ptt_tx_3)      { pinMode(ptt_tx_3,      OUTPUT); }
  if (ptt_tx_4)      { pinMode(ptt_tx_4,      OUTPUT); }
  if (ptt_tx_5)      { pinMode(ptt_tx_5,      OUTPUT); }
  if (ptt_tx_6)      { pinMode(ptt_tx_6,      OUTPUT); }
  #endif

  // Safe initial states
  digitalWrite(tx_key_line_1, LOW);
  if (ptt_tx_1)   digitalWrite(ptt_tx_1,   LOW);
  #ifdef FEATURE_ADDITIONAL_TX_AND_PTT_PINS
  if (tx_key_line_2) { digitalWrite(tx_key_line_2, LOW); }
  if (tx_key_line_3) { digitalWrite(tx_key_line_3, LOW); }
  if (tx_key_line_4) { digitalWrite(tx_key_line_4, LOW); }
  if (tx_key_line_5) { digitalWrite(tx_key_line_5, LOW); }
  if (tx_key_line_6) { digitalWrite(tx_key_line_6, LOW); }
  if (ptt_tx_2)      { digitalWrite(ptt_tx_2,      LOW); }
  if (ptt_tx_3)      { digitalWrite(ptt_tx_3,      LOW); }
  if (ptt_tx_4)      { digitalWrite(ptt_tx_4,      LOW); }
  if (ptt_tx_5)      { digitalWrite(ptt_tx_5,      LOW); }
  if (ptt_tx_6)      { digitalWrite(ptt_tx_6,      LOW); }
  #endif
  noTone(sidetone_line);

  // Debug serial port for Winkey tracing (must come before other serial init)
  #if defined(DEBUG_WINKEY_EMULATION) && defined(DEBUG_WINKEY_PORT)
  DEBUG_WINKEY_PORT.begin(DEBUG_WINKEY_PORT_BAUD);
  #endif

  // Serial ports — build port array from configured defines and print boot message on each CLI port
  #ifdef KEYER_SERIAL_PORT_0
  KEYER_SERIAL_PORT_0.begin(KEYER_SERIAL_PORT_0_BAUD);
  keyer_serial_ports[keyer_num_serial_ports++] = { &KEYER_SERIAL_PORT_0, KEYER_SERIAL_PORT_0_BAUD, KEYER_SERIAL_PORT_0_MODE, 0 };
  #endif
  #ifdef KEYER_SERIAL_PORT_1
  KEYER_SERIAL_PORT_1.begin(KEYER_SERIAL_PORT_1_BAUD);
  keyer_serial_ports[keyer_num_serial_ports++] = { &KEYER_SERIAL_PORT_1, KEYER_SERIAL_PORT_1_BAUD, KEYER_SERIAL_PORT_1_MODE, 0 };
  #endif
  #ifdef KEYER_SERIAL_PORT_2
  KEYER_SERIAL_PORT_2.begin(KEYER_SERIAL_PORT_2_BAUD);
  keyer_serial_ports[keyer_num_serial_ports++] = { &KEYER_SERIAL_PORT_2, KEYER_SERIAL_PORT_2_BAUD, KEYER_SERIAL_PORT_2_MODE, 0 };
  #endif
  #ifdef KEYER_SERIAL_PORT_3
  KEYER_SERIAL_PORT_3.begin(KEYER_SERIAL_PORT_3_BAUD);
  keyer_serial_ports[keyer_num_serial_ports++] = { &KEYER_SERIAL_PORT_3, KEYER_SERIAL_PORT_3_BAUD, KEYER_SERIAL_PORT_3_MODE, 0 };
  #endif

  for (uint8_t _i = 0; _i < keyer_num_serial_ports; _i++) {
    if (keyer_serial_ports[_i].mode == SERIAL_MODE_CLI) {
      primary_serial_port = keyer_serial_ports[_i].port;
      primary_serial_port->println(F("\r\nK3NG CW Keyer v2 by K3NG"));
      primary_serial_port->print(F("Version "));
      primary_serial_port->println(F(CODE_VERSION));
      primary_serial_port->println(F("Type to send CW. \\? for help."));
    }
  }
  primary_serial_port = keyer_serial_ports[0].port;  // restore to port 0 default

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

  // Rotary encoder init
  #ifdef FEATURE_ROTARY_ENCODER
  #ifdef OPTION_ENCODER_ENABLE_PULLUPS
  pinMode(rotary_pin1, INPUT_PULLUP);
  pinMode(rotary_pin2, INPUT_PULLUP);
  #else
  pinMode(rotary_pin1, INPUT);
  pinMode(rotary_pin2, INPUT);
  digitalWrite(rotary_pin1, HIGH);
  digitalWrite(rotary_pin2, HIGH);
  #endif
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
    primary_serial_port->println(F("Factory reset complete."));
  } else {
    // Normal boot: load settings from EEPROM if they exist
    if (read_settings_from_eeprom()) {
      // Apply persisted config to runtime structs
      cw_scheduler.length_wordspace = configuration.length_wordspace;
      tx_ptt.cw_tx_enabled          = configuration.cw_tx_enabled;
      tx_ptt.ptt_lead_time          = configuration.ptt_lead_time;
      tx_ptt.ptt_tail_time          = configuration.ptt_tail_time;
      #ifdef FEATURE_ADDITIONAL_TX_AND_PTT_PINS
      // Sanitize current_tx in case an old EEPROM save has 0 in the former future_uint8_t_4 slot
      if (configuration.current_tx < 1 || configuration.current_tx > number_of_transmitters) {
        configuration.current_tx = initial_tx;
      }
      select_tx(configuration.current_tx);
      #endif
      #ifdef FEATURE_FARNSWORTH
      if (configuration.wpm_farnsworth > wpm_limit_high) configuration.wpm_farnsworth = 0;
      #endif
    } else {
      // First boot or magic number mismatch — write defaults
      write_settings_to_eeprom();
    }
  }

  // Beacon mode: enter if paddle_left held LOW at boot, or if EEPROM setting is active
  #if defined(FEATURE_BEACON) && defined(FEATURE_MEMORIES)
  if (digitalRead(paddle_left) == LOW) {
    keyer_machine_mode = KEYER_BEACON;
  }
  #endif
  #if defined(FEATURE_BEACON_SETTING) && defined(FEATURE_MEMORIES)
  if (configuration.beacon_mode_on_boot_up) {
    keyer_machine_mode = KEYER_BEACON;
  }
  #endif

  #ifdef FEATURE_WINKEY_EMULATION
  for (uint8_t _i = 0; _i < keyer_num_serial_ports; _i++) {
    if (keyer_serial_ports[_i].mode == SERIAL_MODE_WINKEY) {
      winkey_init(&winkey_state, keyer_serial_ports[_i].port);
      break;
    }
  }
  #endif

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
  service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
  service_sound(SERVICE, 0, 0);
  #if defined(FEATURE_BEACON) && defined(FEATURE_MEMORIES)
  service_beacon_mode();
  #endif
  #ifdef FEATURE_POTENTIOMETER
  check_potentiometer();
  #endif
  #ifdef FEATURE_ROTARY_ENCODER
  check_rotary_encoder();
  #endif
  #ifdef FEATURE_PADDLE_ECHO
  service_paddle_echo();
  #endif
  check_for_dirty_configuration();
  service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);
  #ifdef FEATURE_WINKEY_EMULATION
  service_winkey_housekeeping(&winkey_state, &cw_scheduler, &tx_ptt, &configuration);
  #endif
  #ifdef FEATURE_MEMORIES
  service_memory_program();
  #endif
  #ifdef FEATURE_COMMAND_MODE
  service_command_mode();
  #endif
  #if defined(FEATURE_BUTTONS) || (defined(FEATURE_COMMAND_MODE) && (command_button > 0))
  check_buttons();
  #endif
  service_cw_scheduler(&cw_scheduler, &tx_ptt, &configuration);

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
  configuration.beacon_mode_on_boot_up = 0;
  configuration.current_tx         = initial_tx;
  configuration.ptt_lead_time      = initial_ptt_lead_time_ms;
  configuration.ptt_tail_time      = initial_ptt_tail_time_ms;
  #ifdef FEATURE_FARNSWORTH
  configuration.wpm_farnsworth     = 0;
  #endif

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
  tx_ptt.sidetone_enabled  = 1;
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
// Beacon mode (FEATURE_BEACON / FEATURE_BEACON_SETTING)
// ---------------------------------------------------------------------------

#if defined(FEATURE_BEACON) && defined(FEATURE_MEMORIES)

void service_beacon_mode() {

  if (keyer_machine_mode != KEYER_BEACON) return;

  if (cw_scheduler.char_send_buffer_bytes    != 0) return;
  if (cw_scheduler.element_send_buffer_bytes != 0) return;

  #ifdef OPTION_BEACON_MODE_PTT_TAIL_TIME
  if (tx_ptt.ptt_line_asserted) return;
  #endif

  #ifdef OPTION_BEACON_MODE_MEMORY_REPEAT_TIME
  static unsigned long last_beacon_play_time = 0;
  if ((millis() - last_beacon_play_time) < beacon_memory_repeat_time_ms) return;
  last_beacon_play_time = millis();
  #endif

  play_memory(0);   // always plays memory 1

}

#endif // FEATURE_BEACON && FEATURE_MEMORIES

// ---------------------------------------------------------------------------
// Paddle echo (FEATURE_PADDLE_ECHO)
// ---------------------------------------------------------------------------

#ifdef FEATURE_PADDLE_ECHO

void service_paddle_echo() {

  if (!paddle_echo_active) return;

  static byte paddle_echo_space_sent = 1;

  unsigned long dit_ms = 1200UL / configuration.wpm;

  // Decode the accumulated buffer once the letter-space timeout expires
  if (paddle_echo_buffer && millis() > paddle_echo_buffer_decode_time) {
    int ch = convert_cw_number_to_ascii(paddle_echo_buffer);
    if (ch > 0) primary_serial_port->write((char)ch);
    paddle_echo_buffer = 0;
    paddle_echo_buffer_decode_time = millis() + dit_ms * cw_scheduler.length_letterspace;
    paddle_echo_space_sent = 0;
  }

  // Print a word space after the inter-word gap
  if (!paddle_echo_buffer && !paddle_echo_space_sent &&
      millis() > paddle_echo_buffer_decode_time +
                 dit_ms * (cw_scheduler.length_wordspace - cw_scheduler.length_letterspace)) {
    primary_serial_port->write(' ');
    paddle_echo_space_sent = 1;
  }

}

#endif // FEATURE_PADDLE_ECHO

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
    primary_serial_port->print(F("WPM: "));
    primary_serial_port->println(configuration.wpm);
    #endif
  }

}

#endif // FEATURE_POTENTIOMETER

// ---------------------------------------------------------------------------

#ifdef FEATURE_ROTARY_ENCODER

// speed_change(): apply a WPM delta, clamped to wpm_limit_low/high
void speed_change(int change) {
  int new_wpm = (int)configuration.wpm + change;
  if (new_wpm > wpm_limit_low && new_wpm < wpm_limit_high) {
    configuration.wpm = (unsigned int)new_wpm;
    config_dirty = 1;
  }
}

// chk_rotary_encoder(): read encoder pins and return step (+1/+2/-1/-2 or 0)
// Returns ±2 when turning fast (4 steps within 250 ms), ±1 otherwise.
int chk_rotary_encoder() {

  static unsigned long timestamp[5];

  unsigned char pinstate = (digitalRead(rotary_pin2) << 1) | digitalRead(rotary_pin1);
  encoder_state = pgm_read_byte(&ttable[encoder_state & 0xf][pinstate]);
  unsigned char result = encoder_state & 0x30;
  if (result) {
    timestamp[0] = timestamp[1];
    timestamp[1] = timestamp[2];
    timestamp[2] = timestamp[3];
    timestamp[3] = timestamp[4];
    timestamp[4] = millis();
    unsigned long elapsed = timestamp[4] - timestamp[0];
    if (result == DIR_CW)  { return (elapsed < 250) ? 2 : 1; }
    if (result == DIR_CCW) { return (elapsed < 250) ? -2 : -1; }
  }
  return 0;

}

// check_rotary_encoder(): called from loop() every iteration
void check_rotary_encoder() {

  int step = chk_rotary_encoder();
  if (step != 0) {
    speed_change(step);
  }

}

#endif // FEATURE_ROTARY_ENCODER

// ---------------------------------------------------------------------------
// Hardware functions — called by keyer_2_cw.cpp via prototypes in keyer_2.h
// ---------------------------------------------------------------------------

// cw_key(): assert or release the TX key line and sidetone
void cw_key(struct tx_ptt_struct *tx_ptt_ptr, int state, config_struct *configuration_ptr) {

  #ifdef DEBUG_CW_KEY
    primary_serial_port->print(F("cw_key: ")); primary_serial_port->println(state);
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

  #ifdef FEATURE_ADDITIONAL_TX_AND_PTT_PINS
  if (!tx_ptt_ptr->pin_ptt) return;  // PTT pin not wired for this TX
  #endif

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

#ifdef FEATURE_ADDITIONAL_TX_AND_PTT_PINS
// select_tx(): switch the active TX/PTT line
void select_tx(byte tx_num) {

  if (tx_num < 1 || tx_num > number_of_transmitters) tx_num = 1;
  byte new_pin_tx = 0, new_pin_ptt = 0;
  switch (tx_num) {
    case 1: new_pin_tx = tx_key_line_1; new_pin_ptt = ptt_tx_1; break;
    #if number_of_transmitters >= 2
    case 2: new_pin_tx = tx_key_line_2; new_pin_ptt = ptt_tx_2; break;
    #endif
    #if number_of_transmitters >= 3
    case 3: new_pin_tx = tx_key_line_3; new_pin_ptt = ptt_tx_3; break;
    #endif
    #if number_of_transmitters >= 4
    case 4: new_pin_tx = tx_key_line_4; new_pin_ptt = ptt_tx_4; break;
    #endif
    #if number_of_transmitters >= 5
    case 5: new_pin_tx = tx_key_line_5; new_pin_ptt = ptt_tx_5; break;
    #endif
    #if number_of_transmitters >= 6
    case 6: new_pin_tx = tx_key_line_6; new_pin_ptt = ptt_tx_6; break;
    #endif
  }
  if (new_pin_tx == 0 && tx_num != 1) return;  // unconfigured TX line
  configuration.current_tx = tx_num;
  tx_ptt.pin_tx  = new_pin_tx;
  tx_ptt.pin_ptt = new_pin_ptt;

}
#endif // FEATURE_ADDITIONAL_TX_AND_PTT_PINS

// ---------------------------------------------------------------------------

// sidetone(): start or stop the CW sidetone
void sidetone(byte on) {

  static byte sidetone_state = 0;

  if (on) {
    // Global sidetone disable (Winkey PINCONFIG bit 1 = 0)
    if (!tx_ptt.sidetone_enabled) { on = 0; }
    // Paddle-only sidetone: suppress during automatic (serial/Winkey) sending
    #ifdef FEATURE_WINKEY_EMULATION
    else if (winkey_state.paddle_only_sidetone &&
             cw_scheduler.current_sending_type == AUTOMATIC_SENDING) { on = 0; }
    #endif
  }

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

  // In beacon mode paddles are not used for keying.
  #if defined(FEATURE_BEACON) && defined(FEATURE_MEMORIES)
  if (keyer_machine_mode == KEYER_BEACON) return;
  #endif

  // In memory programming mode paddle input is handled by service_memory_program().
  #ifdef FEATURE_MEMORIES
  if (keyer_machine_mode == KEYER_MEMORY_PROGRAM) return;
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
    #ifdef DEBUG_WINKEY_EMULATION
    DEBUG_WINKEY_PORT.print(F("DBG paddle-abort: left="));
    DEBUG_WINKEY_PORT.print(left);
    DEBUG_WINKEY_PORT.print(F(" right="));
    DEBUG_WINKEY_PORT.print(right);
    DEBUG_WINKEY_PORT.print(F(" dit_buf="));
    DEBUG_WINKEY_PORT.print(dit_buffer);
    DEBUG_WINKEY_PORT.print(F(" dah_buf="));
    DEBUG_WINKEY_PORT.print(dah_buffer);
    DEBUG_WINKEY_PORT.print(F(" state="));
    DEBUG_WINKEY_PORT.println(cw_scheduler.cw_scheduler_state);
    #endif
    clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
    #ifdef FEATURE_WINKEY_EMULATION
    winkey_notify_paddle_interrupt(&winkey_state);
    #endif
  }

  // --- Iambic A / B ---
  if ((configuration.keyer_mode == IAMBIC_A) || (configuration.keyer_mode == IAMBIC_B)) {

    if (cw_scheduler.cw_scheduler_state == IDLE) {

      // Clear same-element buffer on return to idle (prevents repeating the same element)
      if (dit_buffer && (last_sent == DIT)) { dit_buffer = 0; }
      if (dah_buffer && (last_sent == DAH)) { dah_buffer = 0; }

      #ifdef FEATURE_PADDLE_ECHO
      // dit element = 2 dit-times; dah element = 4 dit-times; add 1 dit-time letter-space margin
      unsigned long _pe_dit_ms = 1200UL / configuration.wpm;
      #define PADDLE_ECHO_DIT() do { if (paddle_echo_active) { paddle_echo_buffer = paddle_echo_buffer * 10 + 1; paddle_echo_buffer_decode_time = millis() + _pe_dit_ms * 3; } } while(0)
      #define PADDLE_ECHO_DAH() do { if (paddle_echo_active) { paddle_echo_buffer = paddle_echo_buffer * 10 + 2; paddle_echo_buffer_decode_time = millis() + _pe_dit_ms * 5; } } while(0)
      #endif

      if (configuration.paddle_mode == PADDLE_NORMAL) {
        if ((left == LOW) || (dit_buffer && (last_sent == DAH))) {
          send_dit(&cw_scheduler, MANUAL_SENDING);
          dit_buffer = 0;
          last_sent  = DIT;
          #ifdef FEATURE_PADDLE_ECHO
          PADDLE_ECHO_DIT();
          #endif
        }
        if ((right == LOW) || (dah_buffer && (last_sent == DIT))) {
          send_dah(&cw_scheduler, MANUAL_SENDING);
          dah_buffer = 0;
          last_sent  = DAH;
          #ifdef FEATURE_PADDLE_ECHO
          PADDLE_ECHO_DAH();
          #endif
        }
      } else {
        // Reversed paddle
        if ((left == LOW) || (dah_buffer && (last_sent == DIT))) {
          send_dah(&cw_scheduler, MANUAL_SENDING);
          dah_buffer = 0;
          last_sent  = DAH;
          #ifdef FEATURE_PADDLE_ECHO
          PADDLE_ECHO_DAH();
          #endif
        }
        if ((right == LOW) || (dit_buffer && (last_sent == DAH))) {
          send_dit(&cw_scheduler, MANUAL_SENDING);
          dit_buffer = 0;
          last_sent  = DIT;
          #ifdef FEATURE_PADDLE_ECHO
          PADDLE_ECHO_DIT();
          #endif
        }
      }

      #ifdef FEATURE_PADDLE_ECHO
      #undef PADDLE_ECHO_DIT
      #undef PADDLE_ECHO_DAH
      #endif

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
          #ifdef FEATURE_PADDLE_ECHO
          { if (paddle_echo_active) { unsigned long _d = 1200UL / configuration.wpm; paddle_echo_buffer = paddle_echo_buffer * 10 + 1; paddle_echo_buffer_decode_time = millis() + _d * 3; } }
          #endif
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
          #ifdef FEATURE_PADDLE_ECHO
          { if (paddle_echo_active) { unsigned long _d = 1200UL / configuration.wpm; paddle_echo_buffer = paddle_echo_buffer * 10 + 1; paddle_echo_buffer_decode_time = millis() + _d * 3; } }
          #endif
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

    if (primary_serial_port->available() > 0) {
      char ch = (char)primary_serial_port->read();

      if ((ch == '\r') || (ch == '\n')) {
        primary_serial_port->println();
        break;
      } else if (ch == 27) {  // ESC — cancel
        clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
        return -1;
      } else if ((ch >= '0') && (ch <= '9')) {
        primary_serial_port->write(ch);
        if (num_idx < max_digits) {
          num_buf[num_idx++] = ch;
        } else {
          primary_serial_port->println(F("\r\nError"));
          return -1;
        }
      } else {
        primary_serial_port->println(F("\r\nError"));
        return -1;
      }
    }
  }

  if (num_idx == 0) { return -1; }

  int value = atoi(num_buf);
  if ((value > lower_limit) && (value < upper_limit)) {
    return value;
  }
  primary_serial_port->println(F("Error"));
  return -1;

}

// ---------------------------------------------------------------------------

void serial_wpm_set() {

  int new_wpm = serial_get_number_input(3, 0, 1000);
  if (new_wpm > 0) {
    configuration.wpm = new_wpm;
    config_dirty = 1;
    primary_serial_port->print(F("WPM: "));
    primary_serial_port->println(configuration.wpm);
  }

}

// ---------------------------------------------------------------------------

void serial_set_sidetone_freq() {

  int new_hz = serial_get_number_input(4, 99, 20001);
  if (new_hz > 0) {
    configuration.sidetone_frequency = new_hz;
    config_dirty = 1;
    primary_serial_port->print(F("Sidetone: "));
    primary_serial_port->print(configuration.sidetone_frequency);
    primary_serial_port->println(F(" Hz"));
  }

}

// ---------------------------------------------------------------------------

void serial_change_wordspace() {

  int new_ws = serial_get_number_input(2, 0, 100);
  if (new_ws > 0) {
    cw_scheduler.length_wordspace    = new_ws;
    configuration.length_wordspace   = new_ws;
    config_dirty = 1;
    primary_serial_port->print(F("Wordspace: "));
    primary_serial_port->println(new_ws);
  }

}

// ---------------------------------------------------------------------------

#ifdef FEATURE_FARNSWORTH
void serial_set_farnsworth() {

  // 0 = disable; otherwise must be > wpm to have any effect.
  int new_fw = serial_get_number_input(3, 0, wpm_limit_high);
  if (new_fw >= 0) {
    configuration.wpm_farnsworth = (uint8_t)new_fw;
    config_dirty = 1;
    if (new_fw == 0) {
      primary_serial_port->println(F("Farnsworth disabled"));
    } else {
      primary_serial_port->print(F("Farnsworth WPM: "));
      primary_serial_port->println(configuration.wpm_farnsworth);
    }
  }

}
#endif // FEATURE_FARNSWORTH

// ---------------------------------------------------------------------------

void serial_tune_command() {

  delay(50);
  while (primary_serial_port->available() > 0) { primary_serial_port->read(); }

  cw_key(&tx_ptt, RIG_TRANSMIT, &configuration);
  primary_serial_port->println(F("Tuning - press any key to stop"));

  while (primary_serial_port->available() == 0) {
    service_sound(SERVICE, 0, 0);
  }
  while (primary_serial_port->available() > 0) { primary_serial_port->read(); }

  cw_key(&tx_ptt, RIG_RECEIVE, &configuration);

}

// ---------------------------------------------------------------------------

void serial_status() {

  primary_serial_port->println();
  primary_serial_port->print(F("Mode: "));
  switch (configuration.keyer_mode) {
    case IAMBIC_A: primary_serial_port->println(F("Iambic A")); break;
    case IAMBIC_B: primary_serial_port->println(F("Iambic B")); break;
    case BUG:      primary_serial_port->println(F("Bug"));      break;
    case STRAIGHT: primary_serial_port->println(F("Straight")); break;
    default:       primary_serial_port->println(F("?"));        break;
  }
  primary_serial_port->print(F("Paddle: "));
  primary_serial_port->println((configuration.paddle_mode == PADDLE_NORMAL) ? F("Normal") : F("Reversed"));
  primary_serial_port->print(F("WPM: "));
  primary_serial_port->println(configuration.wpm);
  primary_serial_port->print(F("Sidetone: "));
  primary_serial_port->print(configuration.sidetone_frequency);
  primary_serial_port->println(F(" Hz"));
  primary_serial_port->print(F("TX: "));
  primary_serial_port->println(tx_ptt.cw_tx_enabled ? F("Enabled") : F("Disabled (sidetone only)"));
  #ifdef FEATURE_ADDITIONAL_TX_AND_PTT_PINS
  primary_serial_port->print(F("Active TX line: "));
  primary_serial_port->println(configuration.current_tx);
  #endif
  primary_serial_port->print(F("PTT lead: "));
  primary_serial_port->print(tx_ptt.ptt_lead_time);
  primary_serial_port->println(F(" ms"));
  primary_serial_port->print(F("PTT tail: "));
  primary_serial_port->print(tx_ptt.ptt_tail_time);
  primary_serial_port->println(F(" ms"));
  primary_serial_port->print(F("Wordspace: "));
  primary_serial_port->println(cw_scheduler.length_wordspace);
  #ifdef FEATURE_FARNSWORTH
  primary_serial_port->print(F("Farnsworth: "));
  if (configuration.wpm_farnsworth > 0)
    primary_serial_port->println(configuration.wpm_farnsworth);
  else
    primary_serial_port->println(F("Disabled"));
  #endif

  #ifdef FEATURE_PADDLE_ECHO
  primary_serial_port->print(F("Paddle echo: "));
  primary_serial_port->println(paddle_echo_active ? F("On") : F("Off"));
  #endif

  #if defined(FEATURE_BEACON) && defined(FEATURE_MEMORIES)
  primary_serial_port->print(F("Beacon: "));
  primary_serial_port->print(keyer_machine_mode == KEYER_BEACON ? F("Active") : F("Inactive"));
  #ifdef FEATURE_BEACON_SETTING
  primary_serial_port->print(F("  Boot: "));
  primary_serial_port->print(configuration.beacon_mode_on_boot_up ? F("Enabled") : F("Disabled"));
  #endif
  primary_serial_port->println();
  #endif

  #ifdef FEATURE_POTENTIOMETER
  primary_serial_port->print(F("Pot: "));
  primary_serial_port->print(pot_activated ? F("Active") : F("Inactive"));
  primary_serial_port->print(F("  WPM range: "));
  primary_serial_port->print(pot_wpm_low_value);
  primary_serial_port->print(F("-"));
  primary_serial_port->println(pot_wpm_high_value);
  #endif

  #ifdef FEATURE_MEMORIES
  primary_serial_port->println(F("Memories:"));
  for (byte m = 0; m < number_of_memories; m++) {
    primary_serial_port->print(F("  "));
    primary_serial_port->print(m + 1);
    primary_serial_port->print(F(": "));
    int start = memory_start(m);
    int end   = memory_end(m);
    bool empty = true;
    for (int y = start; y <= end; y++) {
      byte b = EEPROM.read(y);
      if (b == 255) break;
      primary_serial_port->write((char)b);
      empty = false;
    }
    if (empty) primary_serial_port->print(F("(empty)"));
    primary_serial_port->println();
  }
  #endif

}

// ---------------------------------------------------------------------------

#ifdef FEATURE_SERIAL_HELP
void print_serial_help() {

  primary_serial_port->println(F("\r\nK3NG CW Keyer v2 Commands:"));
  primary_serial_port->println(F("\\A\t\tIambic A"));
  primary_serial_port->println(F("\\B\t\tIambic B"));
  primary_serial_port->println(F("\\G\t\tBug mode"));
  primary_serial_port->println(F("\\I\t\tTX enable/disable toggle"));
  primary_serial_port->println(F("\\N\t\tToggle paddle reverse"));
  primary_serial_port->println(F("\\S\t\tStatus"));
  primary_serial_port->println(F("\\T\t\tTune (hold TX until keypress)"));
  primary_serial_port->println(F("\\F####\t\tSet sidetone Hz"));
  primary_serial_port->println(F("\\W###\t\tSet WPM"));
  #ifdef FEATURE_FARNSWORTH
  primary_serial_port->println(F("\\M###\t\tSet Farnsworth inter-char WPM (0=off)"));
  #endif
  primary_serial_port->println(F("\\Y##\t\tSet wordspace (dit units; default 7)"));
  primary_serial_port->println(F("\\$\t\tSave settings to EEPROM immediately"));
  primary_serial_port->println(F("\\\\\t\tClear send buffer"));
  primary_serial_port->println(F("\\~\t\tReset"));
  #ifdef FEATURE_POTENTIOMETER
  primary_serial_port->println(F("\\V\t\tToggle potentiometer active/inactive"));
  #endif
  #ifdef FEATURE_PADDLE_ECHO
  primary_serial_port->println(F("\\*\t\tToggle paddle echo on/off"));
  #endif
  #if defined(FEATURE_BEACON_SETTING) && defined(FEATURE_MEMORIES)
  primary_serial_port->println(F("\\_\t\tToggle beacon-on-boot enable/disable"));
  #endif
  #ifdef FEATURE_ADDITIONAL_TX_AND_PTT_PINS
  primary_serial_port->println(F("\\X#\t\tSwitch active TX line (1-6)"));
  #endif
  primary_serial_port->println(F("\\?\t\tThis help"));
  #ifdef FEATURE_MEMORIES
  primary_serial_port->println(F("\\1 \\2 \\3\tPlay memory 1/2/3"));
  primary_serial_port->println(F("\\P#<text>\tProgram memory # with text (e.g. \\P1CQ CQ DE K3NG)"));
  primary_serial_port->println(F("\\P#\t\tProgram memory # via paddle (Enter with no text)"));
  #endif

}
#endif // FEATURE_SERIAL_HELP

// ---------------------------------------------------------------------------
#ifdef FEATURE_MEMORIES
// cli_program_memory() — serial CLI handler for \P
//
// Syntax: \P<n><text><CR>  — write text directly into memory slot <n>
//         \P<n><CR>        — enter interactive paddle programming mode for slot <n>
//
// If no digit follows \P within 5 s the command is cancelled.
void cli_program_memory() {

  // Read the memory number digit
  unsigned long deadline = millis() + 5000UL;
  char num_char = 0;
  while (millis() < deadline) {
    if (primary_serial_port->available()) {
      num_char = (char)primary_serial_port->read();
      break;
    }
  }

  if (num_char < '1' || num_char > ('0' + number_of_memories)) {
    primary_serial_port->println(F("Cancelled."));
    return;
  }

  byte mem_num = num_char - '1';    // 0-based
  primary_serial_port->write(num_char);           // echo digit

  // Read the first character of the message (with a short timeout)
  deadline = millis() + 5000UL;
  char first_char = 0;
  while (millis() < deadline) {
    if (primary_serial_port->available()) {
      first_char = (char)primary_serial_port->read();
      break;
    }
  }

  // \P<n><CR> — no text → enter paddle programming mode
  if (first_char == '\r' || first_char == '\n' || first_char == 0) {
    primary_serial_port->println();
    memory_program_enter(mem_num, 0);
    return;
  }

  // \P<n><text><CR> — write text directly to EEPROM
  int mem_index = 0;
  int mem_max   = memory_end(mem_num) - memory_start(mem_num);
  char c        = first_char;   // already read; process before looping

  deadline = millis() + 30000UL;   // 30 s to finish typing the message
  while (1) {
    if (c == '\r' || c == '\n') break;
    if (mem_index >= mem_max) {
      // Slot full — drain remaining input until CR
      while (millis() < deadline) {
        if (primary_serial_port->available() && (primary_serial_port->read() == '\r' || primary_serial_port->read() == '\n')) break;
      }
      primary_serial_port->println(F(" [truncated]"));
      break;
    }
    primary_serial_port->write(c);  // echo
    EEPROM.update(memory_start(mem_num) + mem_index, (byte)toupper(c));
    mem_index++;

    // Read next character
    c = 0;
    while (millis() < deadline) {
      if (primary_serial_port->available()) { c = (char)primary_serial_port->read(); break; }
    }
    if (c == 0) break;   // timeout
  }

  EEPROM.update(memory_start(mem_num) + mem_index, 255);  // sentinel

  primary_serial_port->println();
  primary_serial_port->print(F("Memory "));
  primary_serial_port->print((int)(mem_num + 1));
  primary_serial_port->println(F(" saved."));
}
#endif // FEATURE_MEMORIES

// ---------------------------------------------------------------------------

void process_cli_command(char cmd) {

  switch (cmd) {

    case 'A':
      configuration.keyer_mode = IAMBIC_A;
      config_dirty = 1;
      primary_serial_port->println(F("Iambic A"));
      break;

    case 'B':
      configuration.keyer_mode = IAMBIC_B;
      config_dirty = 1;
      primary_serial_port->println(F("Iambic B"));
      break;

    case 'G':
      configuration.keyer_mode = BUG;
      config_dirty = 1;
      primary_serial_port->println(F("Bug"));
      break;

    case 'F':
      serial_set_sidetone_freq();
      break;

    case 'I':
      tx_ptt.cw_tx_enabled          = !tx_ptt.cw_tx_enabled;
      configuration.cw_tx_enabled   = tx_ptt.cw_tx_enabled;
      config_dirty = 1;
      primary_serial_port->print(F("TX "));
      primary_serial_port->println(tx_ptt.cw_tx_enabled ? F("Enabled") : F("Disabled (sidetone only)"));
      break;

    case 'N':
      configuration.paddle_mode = (configuration.paddle_mode == PADDLE_NORMAL) ? PADDLE_REVERSE : PADDLE_NORMAL;
      config_dirty = 1;
      primary_serial_port->print(F("Paddles "));
      primary_serial_port->println((configuration.paddle_mode == PADDLE_NORMAL) ? F("Normal") : F("Reversed"));
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
        primary_serial_port->print(F("Playing memory "));
        primary_serial_port->println((int)(cmd - '0'));
      } else {
        primary_serial_port->println(F("No such memory"));
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
        primary_serial_port->println(F("Enable FEATURE_SERIAL_HELP for help text"));
      #endif
      break;

    #ifdef FEATURE_PADDLE_ECHO
    case '*':
      paddle_echo_active = !paddle_echo_active;
      primary_serial_port->print(F("Paddle echo "));
      primary_serial_port->println(paddle_echo_active ? F("On") : F("Off"));
      break;
    #endif

    #if defined(FEATURE_BEACON_SETTING) && defined(FEATURE_MEMORIES)
    case '_':
      configuration.beacon_mode_on_boot_up = !configuration.beacon_mode_on_boot_up;
      config_dirty = 1;
      primary_serial_port->print(F("Beacon on boot: "));
      primary_serial_port->println(configuration.beacon_mode_on_boot_up ? F("Enabled") : F("Disabled"));
      break;
    #endif

    case '$':
      write_settings_to_eeprom();
      config_dirty = 0;
      primary_serial_port->println(F("Settings saved to EEPROM"));
      break;

    case '\\':
      clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
      primary_serial_port->println(F("Buffer cleared"));
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
    #ifdef FEATURE_FARNSWORTH
    case 'M': // \M### — set Farnsworth inter-char WPM (0 = disable)
      serial_set_farnsworth();
      break;
    #endif
    case 'K': // Training
    case 'L': // Weighting
    case 'O': // Sidetone mode cycle
    case 'Q': // QRSS mode
    case 'R': // Regular (non-QRSS) mode
    case 'U': // PTT toggle
    #ifdef FEATURE_POTENTIOMETER
    case 'V':
      pot_activated = !pot_activated;
      primary_serial_port->print(F("Pot "));
      primary_serial_port->println(pot_activated ? F("Active") : F("Inactive"));
      break;
    #endif // FEATURE_POTENTIOMETER
    #ifdef FEATURE_ADDITIONAL_TX_AND_PTT_PINS
    case 'X': { // Switch TX — \X1 through \X6
      int tx_num = serial_get_number_input(1, 1, number_of_transmitters);
      if (tx_num < 1) {
        primary_serial_port->println(F("TX# out of range"));
      } else {
        select_tx((byte)tx_num);
        primary_serial_port->print(F("TX "));
        primary_serial_port->println(configuration.current_tx);
      }
      break;
    }
    #endif // FEATURE_ADDITIONAL_TX_AND_PTT_PINS
    case 'Z': // Autospace
      primary_serial_port->println(F("Not implemented yet"));
      break;

    default:
      primary_serial_port->print(F("Unknown command: \\"));
      primary_serial_port->println(cmd);
      break;

  }

}

// ---------------------------------------------------------------------------
// service_serial() — main serial service routine called from loop()
// ---------------------------------------------------------------------------

static void service_cli_port(KeyerSerialPort* sp) {

  while (sp->port->available() > 0) {

    char incoming = (char)sp->port->read();

    if (incoming == 27) {  // ESC — clear buffer immediately
      clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);
      sp->backslash_flag = false;
      continue;
    }

    if (sp->backslash_flag) {
      sp->backslash_flag = false;
      incoming = toupper(incoming);
      primary_serial_port->write(incoming);
      primary_serial_port->println();
      process_cli_command(incoming);
    } else if (incoming == '\\') {
      sp->backslash_flag = true;
      primary_serial_port->write('\\');
    } else {
      // Regular character — queue for CW sending
      add_to_cw_char_send_buffer(&cw_scheduler, toupper(incoming));
    }

  }

}

void service_serial() {
  for (uint8_t _i = 0; _i < keyer_num_serial_ports; _i++) {
    #ifdef FEATURE_WINKEY_EMULATION
    if (keyer_serial_ports[_i].mode == SERIAL_MODE_WINKEY) {
      while (keyer_serial_ports[_i].port->available() > 0) {
        service_winkey_byte(&winkey_state,
                            (uint8_t)keyer_serial_ports[_i].port->read(),
                            &cw_scheduler, &tx_ptt, &configuration);
      }
      #if defined(FEATURE_WINKEY_EMULATION) && defined(FEATURE_MEMORIES)
      if (winkey_state.pending_memory > 0) {
        play_memory(winkey_state.pending_memory - 1);  // pending_memory is 1-based
        winkey_state.pending_memory = 0;
      }
      #endif
      continue;
    }
    #endif
    if (keyer_serial_ports[_i].mode != SERIAL_MODE_CLI) continue;
    primary_serial_port = keyer_serial_ports[_i].port;
    service_cli_port(&keyer_serial_ports[_i]);
  }
}

#else // !FEATURE_COMMAND_LINE_INTERFACE

void service_serial() {
  for (uint8_t _i = 0; _i < keyer_num_serial_ports; _i++) {
    #ifdef FEATURE_WINKEY_EMULATION
    if (keyer_serial_ports[_i].mode == SERIAL_MODE_WINKEY) {
      while (keyer_serial_ports[_i].port->available() > 0) {
        service_winkey_byte(&winkey_state,
                            (uint8_t)keyer_serial_ports[_i].port->read(),
                            &cw_scheduler, &tx_ptt, &configuration);
      }
      #if defined(FEATURE_WINKEY_EMULATION) && defined(FEATURE_MEMORIES)
      if (winkey_state.pending_memory > 0) {
        play_memory(winkey_state.pending_memory - 1);  // pending_memory is 1-based
        winkey_state.pending_memory = 0;
      }
      #endif
      continue;
    }
    #endif
    if (keyer_serial_ports[_i].mode != SERIAL_MODE_CLI) continue;
    while (keyer_serial_ports[_i].port->available() > 0) {
      add_to_cw_char_send_buffer(&cw_scheduler, toupper((char)keyer_serial_ports[_i].port->read()));
    }
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

#if defined(FEATURE_MEMORIES) || defined(FEATURE_PADDLE_ECHO)

// ---------------------------------------------------------------------------
// convert_cw_number_to_ascii — decode accumulated dit(1)/dah(2) code to ASCII
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
    case 9:      return ' ';
    case 21121:  return '/';
    case 21112:  return '=';
    case 211112: return '-';
    case 121212: return '.';
    case 221122: return ',';
    case 112211: return '?';
    case 122121: return '@';
    case 12121:  return '+';
    case 222222: return '\\';
  }
  return -1;
}

#endif // FEATURE_MEMORIES || FEATURE_PADDLE_ECHO

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

      // Not yet in v2: \Q (QRSS), \R (normal speed), \H (Hell), \L (CW), \+ (prosign)
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
// mem_prog_write_char() — decode mem_prog_cwchar, write to EEPROM, echo to CLI.
// Returns 1 if the slot is now full, 0 otherwise.
// ---------------------------------------------------------------------------
static byte mem_prog_write_char() {
  if (mem_prog_cwchar == 0) return 0;
  int ascii = convert_cw_number_to_ascii(mem_prog_cwchar);
  if (ascii > 0) {
    EEPROM.update(memory_start(mem_prog_slot) + mem_prog_index, (byte)ascii);
    mem_prog_index++;
    #ifdef FEATURE_COMMAND_LINE_INTERFACE
    primary_serial_port->write((byte)ascii);
    #endif
    if (mem_prog_cwchar == 9) {
      // Wordspace written: reset element timer so next space needs a fresh gap.
      // mem_prog_last_dd is intentionally NOT reset (end-timeout uses real keying only).
      mem_prog_last_elem = millis();
      mem_prog_consec_sp++;
    } else {
      mem_prog_consec_sp = 0;
      #ifdef FEATURE_MEMORY_MACROS
      mem_prog_macro_flag = (ascii == '\\') ? 1 : 0;
      #endif
    }
  }
  mem_prog_cwchar     = 0;
  mem_prog_paddle_hit = 0;
  return ((memory_start(mem_prog_slot) + mem_prog_index) >= memory_end(mem_prog_slot));
}

// ---------------------------------------------------------------------------
// mem_prog_start_exit() — write sentinel, print confirmation, start exit beep
// ---------------------------------------------------------------------------
static void mem_prog_start_exit() {
  EEPROM.update(memory_start(mem_prog_slot) + mem_prog_index, 255);
  #ifdef FEATURE_COMMAND_LINE_INTERFACE
  primary_serial_port->println();
  primary_serial_port->print(F("Memory "));
  primary_serial_port->print(mem_prog_slot + 1);
  primary_serial_port->println(F(" saved"));
  #endif
  tone(sidetone_line, hz_high_beep);
  mem_prog_sound_until = millis() + 100UL;
  mem_prog_state       = MEM_PROG_EXIT_BEEP;
}

// ---------------------------------------------------------------------------
// memory_program_enter() — initialise state and enter KEYER_MEMORY_PROGRAM mode.
// Called from the button handler (from_cmd_mode=0) or CMD_PROGRAM_MEM_WAIT (=1).
// ---------------------------------------------------------------------------
void memory_program_enter(byte memory_number, byte from_cmd_mode) {
  if (memory_number >= number_of_memories) return;

  clear_buffers_and_stop_sending(&cw_scheduler, &tx_ptt, &configuration);

  mem_prog_slot          = memory_number;
  mem_prog_from_cmd_mode = from_cmd_mode;
  mem_prog_state         = MEM_PROG_ENTRY_BEEP;
  mem_prog_cwchar        = 0;
  mem_prog_paddle_hit    = 0;
  mem_prog_index         = 0;
  mem_prog_consec_sp     = 0;
  mem_prog_last_elem     = millis();
  mem_prog_last_dd       = millis();
  mem_prog_sound_until   = millis() + 100UL;
  mem_prog_saved_tx      = tx_ptt.cw_tx_enabled;
  #ifdef FEATURE_MEMORY_MACROS
  mem_prog_macro_flag    = 0;
  #endif

  tx_ptt.cw_tx_enabled = 0;   // sidetone only while programming

  #ifdef FEATURE_COMMAND_LINE_INTERFACE
  primary_serial_port->println();
  primary_serial_port->print(F("Pgm mem "));
  primary_serial_port->print(memory_number + 1);
  primary_serial_port->println(F(": key message; squeeze or button 0 to end"));
  #endif

  tone(sidetone_line, hz_high_beep);
  keyer_machine_mode = KEYER_MEMORY_PROGRAM;
}

// ---------------------------------------------------------------------------
// service_memory_program() — non-blocking memory programming state machine.
// Called every loop() when keyer_machine_mode == KEYER_MEMORY_PROGRAM.
//
// Sub-states (MEM_PROG_*):
//   ENTRY_BEEP — wait 100 ms for entry tone, then advance to INTER
//   INTER      — watch paddles; check letterspace/wordspace/end timeouts
//   ELEMENT    — element queued in scheduler; wait for scheduler idle
//   EXIT_BEEP  — wait 100 ms for exit tone, then restore prior mode
//
// Timer notes (matching original blocking program_memory() logic):
//   mem_prog_last_elem — reset on every element AND after each wordspace write.
//                        Drives wordspace detection (each space needs a fresh gap).
//   mem_prog_last_dd   — reset only on real paddle hits.
//                        Drives end-of-programming regardless of spaces inserted.
// Each element's scheduler run already consumes 1 dit of inter-element silence,
// so letterspace/wordspace multipliers subtract 1 (same as the blocking version).
// ---------------------------------------------------------------------------
void service_memory_program() {

  if (keyer_machine_mode != KEYER_MEMORY_PROGRAM) return;

  const byte max_consecutive_spaces = 3;

  switch (mem_prog_state) {

    // -----------------------------------------------------------------
    case MEM_PROG_ENTRY_BEEP:
      if (millis() >= mem_prog_sound_until) {
        noTone(sidetone_line);
        mem_prog_state = MEM_PROG_INTER;
      }
      break;

    // -----------------------------------------------------------------
    case MEM_PROG_INTER: {

      #ifdef FEATURE_BUTTONS
      if (read_analog_buttons() == 0) { mem_prog_start_exit(); break; }
      #endif

      byte left  = digitalRead(paddle_left);
      byte right = digitalRead(paddle_right);

      // Squeeze with nothing keyed in current char → end of message
      if ((left == LOW) && (right == LOW) && !mem_prog_paddle_hit) {
        mem_prog_start_exit();
        break;
      }

      if ((left == LOW) && (right == HIGH)) {
        send_dit(&cw_scheduler, MANUAL_SENDING);
        mem_prog_cwchar = mem_prog_cwchar * 10 + 1;
        mem_prog_state  = MEM_PROG_ELEMENT;
        break;
      }

      if ((right == LOW) && (left == HIGH)) {
        send_dah(&cw_scheduler, MANUAL_SENDING);
        mem_prog_cwchar = mem_prog_cwchar * 10 + 2;
        mem_prog_state  = MEM_PROG_ELEMENT;
        break;
      }

      // Neither paddle pressed — check timeouts
      {
        unsigned long dit_ms = 1200UL / configuration.wpm;
        unsigned long now    = millis();

        if (mem_prog_paddle_hit) {
          // Letterspace: 3 dits total; 1 already elapsed in element → wait 2 more
          if ((now - mem_prog_last_elem) > (dit_ms * (cw_scheduler.length_letterspace - 1))) {
            if (mem_prog_write_char()) { mem_prog_start_exit(); }
          }
        } else if (mem_prog_index > 0) {
          // After first char: check end-of-programming and wordspace
          if ((now - mem_prog_last_dd) > (unsigned long)memory_program_end_timeout_ms) {
            mem_prog_start_exit();
          } else if ((now - mem_prog_last_elem) > (dit_ms * (cw_scheduler.length_wordspace - 1))) {
            #ifdef FEATURE_MEMORY_MACROS
            if (mem_prog_consec_sp < max_consecutive_spaces && !mem_prog_macro_flag) {
            #else
            if (mem_prog_consec_sp < max_consecutive_spaces) {
            #endif
              mem_prog_cwchar = 9;   // space token
              if (mem_prog_write_char()) { mem_prog_start_exit(); }
            }
            // If cap reached, keep spinning — end-timeout will fire eventually.
          }
        }
      }
      break;
    }

    // -----------------------------------------------------------------
    case MEM_PROG_ELEMENT:
      // Wait for the scheduler to go idle after the dit/dah
      if ((cw_scheduler.element_send_buffer_bytes == 0) &&
          (cw_scheduler.cw_scheduler_state == IDLE)) {
        mem_prog_paddle_hit = 1;
        mem_prog_consec_sp  = 0;
        mem_prog_last_elem  = millis();
        mem_prog_last_dd    = millis();
        mem_prog_state      = MEM_PROG_INTER;
      }
      // Allow button-0 abort even while an element is sounding
      #ifdef FEATURE_BUTTONS
      if (read_analog_buttons() == 0) { mem_prog_start_exit(); }
      #endif
      break;

    // -----------------------------------------------------------------
    case MEM_PROG_EXIT_BEEP:
      if (millis() >= mem_prog_sound_until) {
        noTone(sidetone_line);
        tx_ptt.cw_tx_enabled = mem_prog_saved_tx;
        #ifdef FEATURE_COMMAND_MODE
        if (mem_prog_from_cmd_mode) {
          add_to_cw_char_send_buffer(&cw_scheduler, command_mode_acknowledgement_character);
          command_mode_state = CMD_WAIT_ACK;
          keyer_machine_mode = KEYER_COMMAND_MODE;
          break;
        }
        #endif
        keyer_machine_mode = KEYER_NORMAL;
      }
      break;

  } // end switch
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

  // Memory programming mode owns all paddle/button input.
  #ifdef FEATURE_MEMORIES
  if (keyer_machine_mode == KEYER_MEMORY_PROGRAM) return;
  #endif

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

  // Long press on button 1+: enter memory programming mode (non-blocking).
  #ifdef FEATURE_MEMORIES
  if (held && (button > 0) && (button <= number_of_memories)) {
    memory_program_enter(button - 1, 0);
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
      primary_serial_port->print(F("Button "));
      primary_serial_port->print(button);
      primary_serial_port->println(F(" (no memory feature)"));
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
      primary_serial_port->println(F("Iambic A"));
      #endif
      break;

    case 2111:  // B (-..)
      command_mode_saved_keyer = IAMBIC_B;
      configuration.keyer_mode = IAMBIC_B;
      config_dirty = 1;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      primary_serial_port->println(F("Iambic B"));
      #endif
      break;

    case 221:   // G (--.)  — bug restored on exit, stay iambic for cmd input
      command_mode_saved_keyer = BUG;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      primary_serial_port->println(F("Bug mode (on exit)"));
      #endif
      break;

    case 11:    // I (..)
      command_mode_saved_tx           = !command_mode_saved_tx;
      configuration.cw_tx_enabled     = command_mode_saved_tx;
      config_dirty = 1;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      primary_serial_port->print(F("TX "));
      primary_serial_port->println(command_mode_saved_tx ? F("Enabled") : F("Disabled"));
      #endif
      break;

    case 21:    // N (-.)
      configuration.paddle_mode = (configuration.paddle_mode == PADDLE_NORMAL) ?
                                    PADDLE_REVERSE : PADDLE_NORMAL;
      config_dirty = 1;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      primary_serial_port->print(F("Paddles "));
      primary_serial_port->println((configuration.paddle_mode == PADDLE_NORMAL) ? F("Normal") : F("Reversed"));
      #endif
      break;

    case 2:     // T (-)  — enter tune sub-state
      tx_ptt.cw_tx_enabled     = 1;
      command_mode_tune_latched = 0;
      command_mode_tune_rprev  = HIGH;
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      primary_serial_port->println(F("Tune: left=momentary  right=latch  squeeze=exit"));
      #endif
      command_mode_state = CMD_TUNE;
      return;   // ack after squeeze exits tune

    case 122:   // W (.--) — enter speed adjust sub-state
      command_mode_state = CMD_SPEED_ADJUST;
      return;   // ack when squeeze exits speed adjust

    #ifdef FEATURE_MEMORIES
    case 1221:  // P (.--.) — program a memory; next CW digit selects which one
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      primary_serial_port->println(F("Pgm memory: key 1-3"));
      #endif
      command_mode_cw_char         = 0;
      command_mode_element_in_prog = 0;
      command_mode_idle_since      = 0;
      command_mode_state           = CMD_PROGRAM_MEM_WAIT;
      return;   // don't send ack yet; wait for memory-number digit
    #endif

    default:
      #ifdef FEATURE_COMMAND_LINE_INTERFACE
      primary_serial_port->print(F("? ("));
      primary_serial_port->print(cw);
      primary_serial_port->println(F(")"));
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
  primary_serial_port->println(F("Speed: left=+WPM  right=-WPM  squeeze=done"));
  #endif

  command_mode_state = CMD_SPEED_ADJUST;
}

// ---------------------------------------------------------------------------
// service_command_mode() — non-blocking state machine, called every loop()
// ---------------------------------------------------------------------------

void service_command_mode() {

  if (command_mode_state == CMD_IDLE) return;

  // Memory programming mode takes over input; pause command-mode processing.
  #ifdef FEATURE_MEMORIES
  if (keyer_machine_mode == KEYER_MEMORY_PROGRAM) return;
  #endif

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
        primary_serial_port->println(F("\r\nCommand mode  (X to exit)"));
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
          primary_serial_port->println(F("Normal mode"));
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
          primary_serial_port->print(F("WPM: "));
          primary_serial_port->println(configuration.wpm);
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
          primary_serial_port->print(F("WPM: "));
          primary_serial_port->println(configuration.wpm);
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
              // Enter non-blocking memory programming mode.
              // service_memory_program() will send the ack and restore CMD_WAIT_ACK when done.
              memory_program_enter(mem_num, 1);
            } else {
              add_to_cw_char_send_buffer(&cw_scheduler, '?');
              command_mode_state = CMD_WAIT_ACK;
            }
            command_mode_cw_char = 0;
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
        primary_serial_port->println(F("Normal mode"));
        #endif
      }
      break;

  } // end switch
}

#endif // FEATURE_COMMAND_MODE
