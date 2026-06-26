#ifndef keyer_2_cw_h
#define keyer_2_cw_h

// K3NG CW Keyer Version 2
// CW scheduler — header
//
// Adapted from chestnut_cw.h (Chestnut transceiver project).
// Implements a non-blocking, state-machine-based CW sending engine.
//
// Two-tier buffer design:
//   char_send_buffer  : holds ASCII characters queued for sending
//   element_send_buffer : holds low-level element tokens (dit, dah, spaces)
//
// Call service_cw_scheduler() from loop() on every iteration.
// It never blocks; all timing is done via millis() comparisons.

#include "keyer_2.h"
#include <Arduino.h>
#include <stdio.h>

// Add a single ASCII character to the character send buffer
void add_to_cw_char_send_buffer(cw_scheduler_struct *cw_scheduler_ptr, byte incoming_byte);

// Add a null-terminated string to the character send buffer
// Pass transmit = NO_CW_TRANSMIT to suppress RF output (sidetone only)
void send_cw_character_string(cw_scheduler_struct *cw_scheduler_ptr, char *string_to_send, byte transmit);

// Queue a single CW character's elements into the element buffer
// Pass omit_letterspace = OMIT_LETTERSPACE when sending prosigns (no inter-letter gap)
void send_cw_char(cw_scheduler_struct *cw_scheduler_ptr, char cw_char, byte omit_letterspace);

// Queue a dit into the element buffer
void send_dit(cw_scheduler_struct *cw_scheduler_ptr, byte sending_type);

// Queue a dah into the element buffer
void send_dah(cw_scheduler_struct *cw_scheduler_ptr, byte sending_type);

// Queue multiple dits
void send_dits(cw_scheduler_struct *cw_scheduler_ptr, int dits);

// Queue multiple dahs
void send_dahs(cw_scheduler_struct *cw_scheduler_ptr, int dahs);

// Low-level: add a token to the element send buffer
void add_to_cw_element_send_buffer(cw_scheduler_struct *cw_scheduler_ptr, byte element_byte);

// Remove the front entry from each buffer (FIFO shift)
void remove_from_cw_element_send_buffer(cw_scheduler_struct *cw_scheduler_ptr);
void remove_from_cw_char_send_buffer(cw_scheduler_struct *cw_scheduler_ptr);

// Schedule a keydown/keyup event (non-blocking; sets state and timestamp)
void schedule_cw_keydown_keyup(cw_scheduler_struct *cw_scheduler_ptr, tx_ptt_struct *tx_ptt_ptr,
                               unsigned int keydown_ms, unsigned int keyup_ms,
                               config_struct *configuration_ptr);

// Clear both buffers, stop sending, reset state to idle
void clear_buffers_and_stop_sending(cw_scheduler_struct *cw_scheduler_ptr, tx_ptt_struct *tx_ptt_ptr,
                                    config_struct *configuration_ptr);

// Main service routine — call from loop() every iteration
void service_cw_scheduler(cw_scheduler_struct *cw_scheduler_ptr, tx_ptt_struct *tx_ptt_ptr,
                           config_struct *configuration_ptr);

#endif // keyer_2_cw_h
