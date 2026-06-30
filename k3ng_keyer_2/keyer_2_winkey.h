#ifndef keyer_2_winkey_h
#define keyer_2_winkey_h

#include <Arduino.h>
#include "keyer_2.h"

// Winkey protocol version reported to host
#define WINKEY_VERSION  23   // 23 = WK2, 10 = WK1

// Parse-state constants (winkey_status in v1 terminology)
#define WINKEY_NO_COMMAND_IN_PROGRESS         0
#define WINKEY_ADMIN_COMMAND                  1
#define WINKEY_ADMIN_COMMAND_ECHO             2
#define WINKEY_UNBUFFERED_SPEED_COMMAND       3
#define WINKEY_WEIGHTING_COMMAND              4
#define WINKEY_PTT_TIMES_PARM1_COMMAND        5
#define WINKEY_PTT_TIMES_PARM2_COMMAND        6
#define WINKEY_SET_POT_PARM1_COMMAND          7
#define WINKEY_SET_POT_PARM2_COMMAND          8
#define WINKEY_SET_POT_PARM3_COMMAND          9
#define WINKEY_PAUSE_COMMAND                  10
#define WINKEY_SET_PINCONFIG_COMMAND          11
#define WINKEY_KEY_COMMAND                    12
#define WINKEY_FARNSWORTH_COMMAND             13
#define WINKEY_SETMODE_COMMAND                14
#define WINKEY_LOAD_SETTINGS_PARM_1_COMMAND   101  // 101-115: 15 load-settings params
#define WINKEY_FIRST_EXTENSION_COMMAND        16
#define WINKEY_KEYING_COMPENSATION_COMMAND    17
#define WINKEY_UNSUPPORTED_COMMAND            18
#define WINKEY_SOFTWARE_PADDLE_COMMAND        19
#define WINKEY_POINTER_COMMAND                20
#define WINKEY_POINTER_01_COMMAND             21
#define WINKEY_POINTER_02_COMMAND             35
#define WINKEY_POINTER_03_COMMAND             22
#define WINKEY_DAH_TO_DIT_RATIO_COMMAND       23
#define WINKEY_BUFFERED_SPEED_COMMAND         24
#define WINKEY_BUFFERED_HSCW_COMMAND          25
#define WINKEY_MERGE_COMMAND                  26
#define WINKEY_MERGE_PARM_2_COMMAND           27
#define WINKEY_HSCW_COMMAND                   28
#define WINKEY_SEND_MSG                       29
#define WINKEY_SIDETONE_FREQ_COMMAND          30
#define WINKEY_KEY_BUFFERED_COMMAND           31
#define WINKEY_WAIT_BUFFERED_COMMAND          32
#define WINKEY_BUFFFERED_PTT_COMMMAND         33
#define WINKEY_DAH_DIT_RATIO_COMMAND          34
#define WINKEY_ADMIN_PADDLE_A2D_PARM_1        36
#define WINKEY_SETMODE_PARM_2                 37

// XOFF/XON flow-control thresholds (char_send_buffer_size = 50)
#define WK_XOFF_THRESHOLD  40
#define WK_XON_THRESHOLD   15

// WK2 1ms minimum wait before sending 0xC0 (matches v1's winkey_c0_wait_time)
#define WK_C0_WAIT_MS  1

struct WinkeyState {
  Stream*        port;
  uint8_t        winkey_status;        // WINKEY_* parse-state constant (v1: winkey_status)
  uint8_t        host_open;            // 1 after admin 0x02 (v1: winkey_host_open)
  uint8_t        wk2_mode;             // 1=WK1 compat, 2=WK2 (v1: wk2_mode)
  uint8_t        serial_echo;          // 1 = echo chars to host after each char arrives
  uint8_t        sending;              // 0 or 0x04 OR'd into status byte (v1: winkey_sending)
  uint8_t        xoff;                 // 0 or 0x10 OR'd into status byte (v1: winkey_xoff)
  uint8_t        interrupted;          // 1 = paddle grabbed TX (v1: winkey_interrupted)
  uint8_t        paddle_only_sidetone;
  uint8_t        load_count;           // byte index for 0x0F 15-byte bulk load
  uint8_t        ptt_lead_param;       // first byte of 0x04 PTT command
  uint8_t        ptt_tail_10ms;        // PTT tail in 10ms units from 0x04
  uint8_t        pending_memory;       // 0=none; 1-6=memory slot to play
  uint8_t        winkey_parmcount;     // param counter for WINKEY_UNSUPPORTED_COMMAND
  unsigned int   saved_wpm;            // WPM before buffered speed change
  unsigned long  last_activity_ms;     // millis() of last byte received (v1: winkey_last_activity)

  // Buffer pointer tracking for POINTER command overwrite/append mode (v1: winkey_buffer_counter/pointer)
  uint8_t        winkey_buffer_counter;  // count of chars added to buffer since last pointer reset
  uint8_t        winkey_buffer_pointer;  // 0 = append mode; >0 = overwrite mode position
};

void winkey_init(WinkeyState* wk, Stream* port);
void service_winkey_byte(WinkeyState* wk, uint8_t b,
                         cw_scheduler_struct* sched,
                         tx_ptt_struct* ptt_s,
                         config_struct* cfg);
void service_winkey_housekeeping(WinkeyState* wk,
                                 cw_scheduler_struct* sched,
                                 tx_ptt_struct* ptt_s,
                                 config_struct* cfg);

// Call this when the paddle fires during a Winkey-controlled automatic send.
// Sends the WK2 BREAKIN status byte (0xC6) immediately so N1MM knows transmission
// was interrupted, and arms the housekeeping path to send 0xC6+0xC0 when PTT drops.
void winkey_notify_paddle_interrupt(WinkeyState* wk);

#endif // keyer_2_winkey_h
