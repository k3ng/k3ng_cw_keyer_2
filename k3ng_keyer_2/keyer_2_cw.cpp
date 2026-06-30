// K3NG CW Keyer Version 2
// CW scheduler — implementation
//
// Non-blocking CW sending engine driven by a state machine.
// Call service_cw_scheduler() from loop() on every iteration.

#include "keyer_2.h"
#include "keyer_2_cw.h"
#include <Arduino.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Element send buffer — FIFO of element tokens
// ---------------------------------------------------------------------------

void add_to_cw_element_send_buffer(cw_scheduler_struct *cw_scheduler_ptr, byte element_byte) {

  if (cw_scheduler_ptr->element_send_buffer_bytes < element_send_buffer_size) {
    cw_scheduler_ptr->element_send_buffer_array[cw_scheduler_ptr->element_send_buffer_bytes] = element_byte;
    cw_scheduler_ptr->element_send_buffer_bytes++;
  }

}

// ---------------------------------------------------------------------------

void send_dit(cw_scheduler_struct *cw_scheduler_ptr, byte sending_type) {

  add_to_cw_element_send_buffer(cw_scheduler_ptr, ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP);
  cw_scheduler_ptr->current_sending_type = sending_type;

}

// ---------------------------------------------------------------------------

void send_dah(cw_scheduler_struct *cw_scheduler_ptr, byte sending_type) {

  add_to_cw_element_send_buffer(cw_scheduler_ptr, THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP);
  cw_scheduler_ptr->current_sending_type = sending_type;

}

// ---------------------------------------------------------------------------

void send_dits(cw_scheduler_struct *cw_scheduler_ptr, int dits) {

  for (; dits > 0; dits--) {
    send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING);
  }

}

// ---------------------------------------------------------------------------

void send_dahs(cw_scheduler_struct *cw_scheduler_ptr, int dahs) {

  for (; dahs > 0; dahs--) {
    send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING);
  }

}

// ---------------------------------------------------------------------------

void remove_from_cw_element_send_buffer(cw_scheduler_struct *cw_scheduler_ptr) {

  if (cw_scheduler_ptr->element_send_buffer_bytes > 0) {
    cw_scheduler_ptr->element_send_buffer_bytes--;
  }
  if (cw_scheduler_ptr->element_send_buffer_bytes > 0) {
    for (int x = 0; x < cw_scheduler_ptr->element_send_buffer_bytes; x++) {
      cw_scheduler_ptr->element_send_buffer_array[x] = cw_scheduler_ptr->element_send_buffer_array[x + 1];
    }
  }

}

// ---------------------------------------------------------------------------
// Character send buffer — FIFO of ASCII bytes
// ---------------------------------------------------------------------------

void add_to_cw_char_send_buffer(cw_scheduler_struct *cw_scheduler_ptr, byte incoming_byte) {

  if (cw_scheduler_ptr->char_send_buffer_bytes < char_send_buffer_size) {
    if (incoming_byte != 127) {   // 127 = DEL / backspace
      cw_scheduler_ptr->char_send_buffer_array[cw_scheduler_ptr->char_send_buffer_bytes] = incoming_byte;
      cw_scheduler_ptr->char_send_buffer_bytes++;
    } else {
      if (cw_scheduler_ptr->char_send_buffer_bytes > 0) {
        cw_scheduler_ptr->char_send_buffer_bytes--;
      }
    }
  }

}

// ---------------------------------------------------------------------------

void send_cw_character_string(cw_scheduler_struct *cw_scheduler_ptr, char *string_to_send, byte transmit) {

  if (transmit == NO_CW_TRANSMIT) {
    add_to_cw_char_send_buffer(cw_scheduler_ptr, CW_CHAR_BUFFER_TX_INHIBIT);
  }

  for (int x = 0; x < 64; x++) {
    if (string_to_send[x] != 0) {
      add_to_cw_char_send_buffer(cw_scheduler_ptr, toupper(string_to_send[x]));
    } else {
      break;
    }
  }

  if (transmit == NO_CW_TRANSMIT) {
    add_to_cw_char_send_buffer(cw_scheduler_ptr, CW_CHAR_BUFFER_TX_ENABLE);
  }

}

// ---------------------------------------------------------------------------

void remove_from_cw_char_send_buffer(cw_scheduler_struct *cw_scheduler_ptr) {

  if (cw_scheduler_ptr->char_send_buffer_bytes > 0) {
    cw_scheduler_ptr->char_send_buffer_bytes--;
  }
  if (cw_scheduler_ptr->char_send_buffer_bytes > 0) {
    for (int x = 0; x < cw_scheduler_ptr->char_send_buffer_bytes; x++) {
      cw_scheduler_ptr->char_send_buffer_array[x] = cw_scheduler_ptr->char_send_buffer_array[x + 1];
    }
  }

}

// ---------------------------------------------------------------------------
// Character → element mapping
// ---------------------------------------------------------------------------

void send_cw_char(cw_scheduler_struct *cw_scheduler_ptr, char cw_char, byte omit_letterspace) {

  #ifdef DEBUG_SEND_CW_CHAR
    Serial.print("\nsend_cw_char: ");
    Serial.print(cw_char);
    if (omit_letterspace) Serial.print(" OMIT_LETTERSPACE");
    Serial.println();
  #endif

  if ((cw_char == 10) || (cw_char == 13)) { return; }  // ignore CR/LF

  switch (cw_char) {

    // --- Letters ---
    case 'A': send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'B': send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 3); break;
    case 'C': send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'D': send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 2); break;
    case 'E': send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'F': send_dits(cw_scheduler_ptr, 2); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'G': send_dahs(cw_scheduler_ptr, 2); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'H': send_dits(cw_scheduler_ptr, 4); break;
    case 'I': send_dits(cw_scheduler_ptr, 2); break;
    case 'J': send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 3); break;
    case 'K': send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'L': send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 2); break;
    case 'M': send_dahs(cw_scheduler_ptr, 2); break;
    case 'N': send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'O': send_dahs(cw_scheduler_ptr, 3); break;
    case 'P': send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 2); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'Q': send_dahs(cw_scheduler_ptr, 2); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'R': send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'S': send_dits(cw_scheduler_ptr, 3); break;
    case 'T': send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'U': send_dits(cw_scheduler_ptr, 2); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'V': send_dits(cw_scheduler_ptr, 3); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'W': send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 2); break;
    case 'X': send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 2); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case 'Y': send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 2); break;
    case 'Z': send_dahs(cw_scheduler_ptr, 2); send_dits(cw_scheduler_ptr, 2); break;

    // --- Digits ---
    case '0': send_dahs(cw_scheduler_ptr, 5); break;
    case '1': send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 4); break;
    case '2': send_dits(cw_scheduler_ptr, 2); send_dahs(cw_scheduler_ptr, 3); break;
    case '3': send_dits(cw_scheduler_ptr, 3); send_dahs(cw_scheduler_ptr, 2); break;
    case '4': send_dits(cw_scheduler_ptr, 4); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case '5': send_dits(cw_scheduler_ptr, 5); break;
    case '6': send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 4); break;
    case '7': send_dahs(cw_scheduler_ptr, 2); send_dits(cw_scheduler_ptr, 3); break;
    case '8': send_dahs(cw_scheduler_ptr, 3); send_dits(cw_scheduler_ptr, 2); break;
    case '9': send_dahs(cw_scheduler_ptr, 4); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;

    // --- Punctuation and prosigns ---
    case ' ':  add_to_cw_element_send_buffer(cw_scheduler_ptr, KEY_UP_WORDSPACE_MINUS_4); break;
    case '=':  send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 3); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;                              // BT
    case '/':  send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 2); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case '.':  send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case ',':  send_dahs(cw_scheduler_ptr, 2); send_dits(cw_scheduler_ptr, 2); send_dahs(cw_scheduler_ptr, 2); break;
    case '?':  send_dits(cw_scheduler_ptr, 2); send_dahs(cw_scheduler_ptr, 2); send_dits(cw_scheduler_ptr, 2); break;
    case '\'': send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 4); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;                              // apostrophe
    case '!':  send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 2); break;
    case '(':  send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 2); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case ')':  send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 2); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case '&':  send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 3); break;                              // AS
    case ':':  send_dahs(cw_scheduler_ptr, 3); send_dits(cw_scheduler_ptr, 3); break;
    case ';':  send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case '+':  send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;  // AR
    case '-':  send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 4); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case '_':  send_dits(cw_scheduler_ptr, 2); send_dahs(cw_scheduler_ptr, 2); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case '"':  send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 2); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case '$':  send_dits(cw_scheduler_ptr, 3); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 2); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case '@':  send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dahs(cw_scheduler_ptr, 2); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;
    case '<':  send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); break;   // AR
    case '>':  send_dits(cw_scheduler_ptr, 3); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;                                                                   // SK
    case '*':  send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dits(cw_scheduler_ptr, 3); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dit(cw_scheduler_ptr, AUTOMATIC_SENDING); send_dah(cw_scheduler_ptr, AUTOMATIC_SENDING); break;                   // BK

    // Half-dit space (timing adjustment)
    case '|':  add_to_cw_element_send_buffer(cw_scheduler_ptr, HALF_UNIT_KEY_UP); return;

    case '\n': break;
    case '\r': break;

    // Unknown character — send ..--.. (?)
    default:   send_dits(cw_scheduler_ptr, 2); send_dahs(cw_scheduler_ptr, 2); send_dits(cw_scheduler_ptr, 2); break;
  }

  if (omit_letterspace != OMIT_LETTERSPACE) {
    // Subtract 1 because send_dit/send_dah already include a trailing inter-element space
    add_to_cw_element_send_buffer(cw_scheduler_ptr, KEY_UP_LETTERSPACE_MINUS_1);
  }

}

// ---------------------------------------------------------------------------
// schedule_cw_keydown_keyup
//
// Sets up a keydown/keyup event in the state machine.
// REQUEST_KEY_DOWN / REQUEST_KEY_UP are sentinel values for straight-key hold.
// ---------------------------------------------------------------------------

void schedule_cw_keydown_keyup(cw_scheduler_struct *cw_scheduler_ptr, tx_ptt_struct *tx_ptt_ptr,
                               unsigned int keydown_ms, unsigned int keyup_ms,
                               config_struct *configuration_ptr) {

  #ifdef DEBUG_CW_KEYDOWN_KEYUP
    char debugtext[64];
    sprintf(debugtext, "schedule_cw_keydown_keyup: dn:%u up:%u", keydown_ms, keyup_ms);
    Serial.println(debugtext);
  #endif

  // Straight key hold: assert key indefinitely
  if ((keydown_ms == REQUEST_KEY_DOWN) && (keyup_ms == REQUEST_KEY_DOWN)) {
    cw_key(tx_ptt_ptr, RIG_TRANSMIT, configuration_ptr);
    cw_scheduler_ptr->cw_scheduler_state = KEY_DOWN_HOLD;
    return;
  }

  // Straight key release
  if ((keydown_ms == REQUEST_KEY_UP) && (keyup_ms == REQUEST_KEY_UP)) {
    cw_key(tx_ptt_ptr, RIG_RECEIVE, configuration_ptr);
    cw_scheduler_ptr->cw_scheduler_state = KEY_UP;
    return;
  }

  if ((keydown_ms > 0) && (cw_scheduler_ptr->cw_scheduler_state != KEY_DOWN_HOLD)) {
    // Keydown requested — check whether PTT lead time applies
    if ((tx_ptt_ptr->ptt_lead_time > 0) && (!tx_ptt_ptr->ptt_line_asserted)) {
      ptt(tx_ptt_ptr, 1);
      cw_scheduler_ptr->cw_scheduler_state = PTT_LEAD_TIME_WAIT;
      cw_scheduler_ptr->next_key_scheduler_transition_time = millis() + tx_ptt_ptr->ptt_lead_time;
      cw_scheduler_ptr->key_scheduler_keydown_ms = keydown_ms;
      cw_scheduler_ptr->key_scheduler_keyup_ms   = keyup_ms;
    } else {
      cw_key(tx_ptt_ptr, RIG_TRANSMIT, configuration_ptr);
      cw_scheduler_ptr->cw_scheduler_state = KEY_DOWN;
      cw_scheduler_ptr->next_key_scheduler_transition_time = millis() + keydown_ms;
      cw_scheduler_ptr->key_scheduler_keyup_ms = keyup_ms;
    }
  } else {
    // No keydown — go straight to keyup timing
    cw_key(tx_ptt_ptr, RIG_RECEIVE, configuration_ptr);
    cw_scheduler_ptr->cw_scheduler_state = KEY_UP;
    cw_scheduler_ptr->next_key_scheduler_transition_time = millis() + keyup_ms;
  }

}

// ---------------------------------------------------------------------------

void clear_buffers_and_stop_sending(cw_scheduler_struct *cw_scheduler_ptr, tx_ptt_struct *tx_ptt_ptr,
                                    config_struct *configuration_ptr) {

  #ifdef DEBUG_SERVICE_CW_SCHEDULER
    Serial.println("clear_buffers_and_stop_sending");
  #endif

  cw_scheduler_ptr->char_send_buffer_bytes    = 0;
  cw_scheduler_ptr->element_send_buffer_bytes = 0;
  cw_scheduler_ptr->currently_sending_element = UNDEFINED;
  cw_key(tx_ptt_ptr, RIG_RECEIVE, configuration_ptr);
  tx_ptt_ptr->cw_tx_enabled = 1;

}

// ---------------------------------------------------------------------------
// service_cw_scheduler — call from loop() every iteration
//
// Step 1: Advance the key state machine (PTT wait → keydown → keyup → idle).
// Step 2: When idle, pop next entry from the element buffer and kick it off.
// Step 3: When element buffer is empty, pop next char from char buffer and
//         convert it to elements.
// ---------------------------------------------------------------------------

void service_cw_scheduler(cw_scheduler_struct *cw_scheduler_ptr, tx_ptt_struct *tx_ptt_ptr,
                           config_struct *configuration_ptr) {

  #ifdef DEBUG_SERVICE_CW_SCHEDULER
    char debugtext[64];
  #endif

  // --- Step 1: Advance key state machine ---

  switch (cw_scheduler_ptr->cw_scheduler_state) {

    case PTT_LEAD_TIME_WAIT:
      if (millis() >= cw_scheduler_ptr->next_key_scheduler_transition_time) {
        cw_key(tx_ptt_ptr, RIG_TRANSMIT, configuration_ptr);
        cw_scheduler_ptr->cw_scheduler_state = KEY_DOWN;
        cw_scheduler_ptr->next_key_scheduler_transition_time = millis() + cw_scheduler_ptr->key_scheduler_keydown_ms;
      }
      break;

    case KEY_DOWN:
      if (millis() >= cw_scheduler_ptr->next_key_scheduler_transition_time) {
        cw_key(tx_ptt_ptr, RIG_RECEIVE, configuration_ptr);
        cw_scheduler_ptr->cw_scheduler_state = KEY_UP;
        if (cw_scheduler_ptr->key_scheduler_keyup_ms > 0) {
          cw_scheduler_ptr->next_key_scheduler_transition_time = millis() + cw_scheduler_ptr->key_scheduler_keyup_ms;
        } else {
          cw_scheduler_ptr->cw_scheduler_state = IDLE;
        }
      }
      break;

    case KEY_UP:
      if (millis() >= cw_scheduler_ptr->next_key_scheduler_transition_time) {
        cw_scheduler_ptr->cw_scheduler_state = IDLE;
        // The letterspace/wordspace element is always the last element of a char.
        // When its KEY_UP timer expires, the char is fully keyed — signal the echo layer.
        if (cw_scheduler_ptr->currently_sending_element == KEY_UP_LETTERSPACE_MINUS_1 ||
            cw_scheduler_ptr->currently_sending_element == KEY_UP_WORDSPACE_MINUS_4   ||
            cw_scheduler_ptr->currently_sending_element == KEY_UP_WORDSPACE) {
          cw_scheduler_ptr->char_keying_finished = 1;
        }
      }
      break;

    case KEY_DOWN_HOLD:
      // Held by straight key — nothing to advance; check_paddles() releases it
      break;

    case IDLE:
    default:
      break;
  }

  // --- Step 2: Service the element buffer ---

  if ((cw_scheduler_ptr->cw_scheduler_state == IDLE) && (cw_scheduler_ptr->element_send_buffer_bytes > 0)) {

    cw_scheduler_ptr->currently_sending_element = cw_scheduler_ptr->element_send_buffer_array[0];

    unsigned int dit_ms = 1200 / configuration_ptr->wpm;  // one dit period in ms

    switch (cw_scheduler_ptr->element_send_buffer_array[0]) {

      case HALF_UNIT_KEY_UP:
        schedule_cw_keydown_keyup(cw_scheduler_ptr, tx_ptt_ptr, 0, dit_ms / 2, configuration_ptr);
        break;

      case ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP:          // dit
        schedule_cw_keydown_keyup(cw_scheduler_ptr, tx_ptt_ptr, dit_ms, dit_ms, configuration_ptr);
        break;

      case THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP:       // dah
        schedule_cw_keydown_keyup(cw_scheduler_ptr, tx_ptt_ptr, 3 * dit_ms, dit_ms, configuration_ptr);
        break;

      case KEY_UP_LETTERSPACE_MINUS_1:
        // Full letter space is 3 dits; minus 1 because the last element already had 1 dit of keyup
        schedule_cw_keydown_keyup(cw_scheduler_ptr, tx_ptt_ptr, 0,
                                  (cw_scheduler_ptr->length_letterspace - 1) * dit_ms, configuration_ptr);
        break;

      case KEY_UP_WORDSPACE_MINUS_4:
        // Full word space is 7 dits; minus 4 because letter space + inter-element space already consumed
        schedule_cw_keydown_keyup(cw_scheduler_ptr, tx_ptt_ptr, 0,
                                  (cw_scheduler_ptr->length_wordspace - 4) * dit_ms, configuration_ptr);
        break;

      case KEY_UP_WORDSPACE:
        schedule_cw_keydown_keyup(cw_scheduler_ptr, tx_ptt_ptr, 0,
                                  cw_scheduler_ptr->length_wordspace * dit_ms, configuration_ptr);
        break;
    }

    remove_from_cw_element_send_buffer(cw_scheduler_ptr);
  }

  // --- Step 3: Service the character buffer ---

  if ((cw_scheduler_ptr->char_send_buffer_bytes > 0) &&
      (cw_scheduler_ptr->pause_sending_buffer == 0) &&
      (cw_scheduler_ptr->element_send_buffer_bytes == 0)) {

    #ifdef FEATURE_MEMORY_MACROS
    // If a macro delay is active, block char buffer until it expires.
    if (cw_scheduler_ptr->macro_delay_until > 0) {
      if (millis() < cw_scheduler_ptr->macro_delay_until) {
        return;
      }
      cw_scheduler_ptr->macro_delay_until = 0;
    }
    #endif

    byte next_char = cw_scheduler_ptr->char_send_buffer_array[0];
    byte bytes_to_remove = 1;

    if (next_char < CW_CHAR_BUFFER_START_OF_SPECIAL_CHARS) {
      send_cw_char(cw_scheduler_ptr, (char)next_char, NORMAL);
      // Signal that a char just started keying. V1 sends Winkey echo here
      // (synchronously, right after send_char). We set this flag so the Winkey
      // housekeeping layer can fire the echo at the same logical moment.
      if (next_char > 30 && next_char != 0x7C) {
        cw_scheduler_ptr->char_keying_started = 1;
        cw_scheduler_ptr->char_keying_char    = next_char;
      }
    } else {
      // Special control codes
      if (next_char == CW_CHAR_BUFFER_TX_INHIBIT) {
        tx_ptt_ptr->cw_tx_enabled = 0;
      }
      if (next_char == CW_CHAR_BUFFER_TX_ENABLE) {
        tx_ptt_ptr->cw_tx_enabled = 1;
      }
      #ifdef FEATURE_MEMORY_MACROS
      if (next_char == CW_CHAR_BUFFER_MACRO_PTT_ON) {
        ptt(tx_ptt_ptr, 1);
      }
      if (next_char == CW_CHAR_BUFFER_MACRO_PTT_OFF) {
        ptt(tx_ptt_ptr, 0);
      }
      if (next_char == CW_CHAR_BUFFER_MACRO_WPM) {
        if (cw_scheduler_ptr->char_send_buffer_bytes > 1) {
          byte new_wpm = cw_scheduler_ptr->char_send_buffer_array[1];
          if (new_wpm >= 5 && new_wpm <= 99) {
            configuration_ptr->wpm = new_wpm;
          }
        }
        bytes_to_remove = 2;
      }
      if (next_char == CW_CHAR_BUFFER_MACRO_DELAY) {
        if (cw_scheduler_ptr->char_send_buffer_bytes > 1) {
          byte delay_secs = cw_scheduler_ptr->char_send_buffer_array[1];
          if (delay_secs > 0) {
            cw_scheduler_ptr->macro_delay_until = millis() + (unsigned long)delay_secs * 1000UL;
          }
        }
        bytes_to_remove = 2;
      }
      #endif
    }

    for (byte i = 0; i < bytes_to_remove; i++) {
      remove_from_cw_char_send_buffer(cw_scheduler_ptr);
    }
  }

}
