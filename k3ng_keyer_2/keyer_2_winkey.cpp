// K3NG CW Keyer v2 — Winkey emulation
//
// Architecture note: this module is modelled as closely as possible on the v1
// Winkey implementation in k3ng_keyer.ino.  The main adaptation for v2's
// non-blocking engine is:
//
//   v1: echo is sent from inside the blocking send_char() call, so N1MM
//       receives it after the char has been keyed (~640 ms at 30 WPM).
//
//   v2: echo is sent IMMEDIATELY when the char byte arrives from the host.
//       This replicates the N1MM-visible behaviour: N1MM's sliding window
//       opens as fast as possible, so it bursts all macro chars before the
//       keyer has keyed even the first one.  All chars queue up in
//       char_send_buffer and are keyed in order without any further serial
//       traffic.  0xC0 fires once when the buffer and key-state machine are
//       truly idle (matching v1's condition plus a 1 ms debounce).

#include "keyer_2_winkey.h"
#include "keyer_2_features_and_options.h"
#include "keyer_2_cw.h"
#include <EEPROM.h>

#ifdef FEATURE_WINKEY_EMULATION

// ---------------------------------------------------------------------------
// Debug macros
// ---------------------------------------------------------------------------
#ifdef DEBUG_WINKEY_EMULATION
  #ifndef DEBUG_WINKEY_PORT
    #define DEBUG_WINKEY_PORT  Serial3
  #endif
  #define WK_DBG(msg)     DEBUG_WINKEY_PORT.print(F(msg))
  #define WK_DBGLN(msg)   DEBUG_WINKEY_PORT.println(F(msg))
  #define WK_DBG_HEX(v)   DEBUG_WINKEY_PORT.print((unsigned long)(v), HEX)
  #define WK_DBG_DEC(v)   DEBUG_WINKEY_PORT.print((long)(v), DEC)
  #define WK_DBG_CHAR(v)  DEBUG_WINKEY_PORT.print((char)(v))
  #define WK_DBG_NL()     DEBUG_WINKEY_PORT.println()
#else
  #define WK_DBG(msg)
  #define WK_DBGLN(msg)
  #define WK_DBG_HEX(v)
  #define WK_DBG_DEC(v)
  #define WK_DBG_CHAR(v)
  #define WK_DBG_NL()
#endif

// ---------------------------------------------------------------------------
// Pot WPM globals (defined in .ino)
// ---------------------------------------------------------------------------
#ifdef FEATURE_POTENTIOMETER
extern byte pot_wpm_low_value;
extern byte pot_wpm_high_value;
#else
static const byte pot_wpm_low_value  = 5;
static const byte pot_wpm_high_value = 40;
#endif

// EEPROM dirty flag (defined in .ino) -- OPTION_WINKEY_STRICT_EEPROM_WRITES_MAY_WEAR_OUT_EEPROM
extern byte config_dirty;

// ---------------------------------------------------------------------------
// Sidetone table  (index 1-10 -> Hz)
// ---------------------------------------------------------------------------
static const int wk_sidetone_hz[11] = { 0, 4000, 2000, 1333, 1000, 800, 667, 571, 500, 444, 400 };

static uint8_t hz_to_sidetone_index(unsigned int hz) {
  uint8_t best = 5;
  int best_diff = 9999;
  for (uint8_t i = 1; i <= 10; i++) {
    int diff = abs((int)hz - wk_sidetone_hz[i]);
    if (diff < best_diff) { best_diff = diff; best = i; }
  }
  return best;
}

// ---------------------------------------------------------------------------
// wk_write() - low-level byte to host
// ---------------------------------------------------------------------------
static void wk_write(WinkeyState* wk, uint8_t b) {
  wk->port->write(b);
  WK_DBG("WK>> 0x"); WK_DBG_HEX(b); WK_DBG_NL();
}

// ---------------------------------------------------------------------------
// Status helpers
// ---------------------------------------------------------------------------
static void wk_send_status(WinkeyState* wk) {
  wk->port->write((uint8_t)(0xC0 | wk->sending | wk->xoff));
}

static void wk_start_sending(WinkeyState* wk) {
  wk->sending = 0x04;
  wk_send_status(wk);
  WK_DBG("WK start sending -> 0x"); WK_DBG_HEX(0xC0|wk->sending|wk->xoff); WK_DBG_NL();
}

static void wk_done_sending(WinkeyState* wk) {
  wk->sending = 0;
  wk_send_status(wk);
  WK_DBGLN("WK done sending -> 0xC0");
}

// ---------------------------------------------------------------------------
// Admin get-values: 15-byte settings block
// ---------------------------------------------------------------------------
static void wk_send_values(WinkeyState* wk, config_struct* cfg, tx_ptt_struct* ptt_s) {
  uint8_t mode_byte = 0;
  if (cfg->keyer_mode == IAMBIC_A)        mode_byte |= 0x01;
  if (cfg->paddle_mode == PADDLE_REVERSE) mode_byte |= 0x02;
  if (wk->serial_echo)                    mode_byte |= 0x04;
  wk_write(wk, mode_byte);
  wk_write(wk, (uint8_t)cfg->wpm);
  wk_write(wk, hz_to_sidetone_index(cfg->sidetone_frequency));
  wk_write(wk, 50);   // weight (50 = normal)
  wk_write(wk, (uint8_t)(ptt_s->ptt_lead_time / 10));
  wk_write(wk, wk->ptt_tail_10ms);
  wk_write(wk, (uint8_t)pot_wpm_low_value);
  uint8_t wpm_range = (pot_wpm_high_value > pot_wpm_low_value) ?
                      (uint8_t)(pot_wpm_high_value - pot_wpm_low_value) : 35;
  wk_write(wk, wpm_range);
  wk_write(wk, 0);    // first extension
  wk_write(wk, 0);    // keying compensation
  wk_write(wk, 0);    // Farnsworth WPM
  wk_write(wk, 255);  // pot full scale
  wk_write(wk, 50);   // dah/dit ratio
  wk_write(wk, 0);    // pin config
  wk_write(wk, 0);    // WK2 extra
}

// ---------------------------------------------------------------------------
// Stop all keying and drain buffers  (v1: clear_send_buffer + hard key-off)
// ---------------------------------------------------------------------------
static void wk_clear_buffers(WinkeyState* wk,
                              cw_scheduler_struct* sched,
                              tx_ptt_struct*        ptt_s,
                              config_struct*        cfg) {
  clear_buffers_and_stop_sending(sched, ptt_s, cfg);
  sched->pause_sending_buffer = 0;
  sched->char_keying_started  = 0;
  sched->char_keying_finished = 0;
}

// ---------------------------------------------------------------------------
// winkey_init()
// ---------------------------------------------------------------------------
void winkey_init(WinkeyState* wk, Stream* port) {
  memset(wk, 0, sizeof(WinkeyState));
  wk->port     = port;
  wk->wk2_mode = 2;
}

// ---------------------------------------------------------------------------
// winkey_notify_paddle_interrupt()
// Call immediately when the paddle fires during automatic (Winkey) sending.
// Mirrors v1 lines 2793-2797: sends 0xC6 (BREAKIN status) right away and arms
// the housekeeping path to send 0xC6+0xC0 once PTT drops.
//
// v1's OPTION_WINKEY_SEND_BREAKIN_STATUS_BYTE defers the 0xC6 send by one
// loop() pass via a flag, and needs a separate inhibit flag to suppress it
// during local Command Mode. v2 doesn't need either: this function is called
// synchronously at the trigger site (check_paddles()) so there's no deferral,
// and check_paddles() already returns immediately in Command Mode, so this
// path can never be reached then. The caller gates this call on
// OPTION_WINKEY_SEND_BREAKIN_STATUS_BYTE.
// ---------------------------------------------------------------------------
void winkey_notify_paddle_interrupt(WinkeyState* wk) {
  if (!wk->host_open || !wk->sending || wk->interrupted) return;
  wk->interrupted = 1;
  // 0xC6 = 0xC0 | 0x04 (sending) | 0x02 (breakin) — v1: 0xc2|winkey_sending|winkey_xoff
  wk->port->write((uint8_t)(0xC0 | wk->sending | wk->xoff | 0x02));
  WK_DBGLN("WK paddle interrupt -> 0xC6");
}

// ---------------------------------------------------------------------------
// winkey_load_settings_command()
// Called for the 15 parameter bytes of the 0x0F command.
// ---------------------------------------------------------------------------
static void winkey_load_settings_command(WinkeyState* wk,
                                          uint8_t param_index,
                                          uint8_t param_value,
                                          config_struct* cfg,
                                          tx_ptt_struct* ptt_s) {
  switch (param_index) {
    case 0:
      if (param_value & 0x01) cfg->keyer_mode  = IAMBIC_A; else cfg->keyer_mode = IAMBIC_B;
      if (param_value & 0x02) cfg->paddle_mode = PADDLE_REVERSE; else cfg->paddle_mode = PADDLE_NORMAL;
      wk->serial_echo = (param_value & 0x04) ? 1 : 0;
      break;
    case 1:
      if (param_value >= 5 && param_value <= 99) cfg->wpm = param_value;
      break;
    case 2:
      if (param_value >= 1 && param_value <= 10)
        cfg->sidetone_frequency = wk_sidetone_hz[param_value];
      break;
    case 4:
      ptt_s->ptt_lead_time = (unsigned int)param_value * 10;
      cfg->ptt_lead_time   = ptt_s->ptt_lead_time;
      break;
    case 5:
      wk->ptt_tail_10ms    = param_value;
      ptt_s->ptt_tail_time = (unsigned int)param_value * 10;
      cfg->ptt_tail_time   = ptt_s->ptt_tail_time;
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// service_winkey_housekeeping()
// Called every loop().  Mirrors v1's WINKEY_HOUSEKEEPING action.
// ---------------------------------------------------------------------------
void service_winkey_housekeeping(WinkeyState* wk,
                                 cw_scheduler_struct* sched,
                                 tx_ptt_struct*        ptt_s,
                                 config_struct*        cfg) {
  if (!wk->host_open) return;

  // v1 line 11529: send 0xC0 when:
  //   available() == 0   AND   winkey_sending   AND   send_buffer_bytes < 1
  //   AND   (millis() - winkey_last_activity) > 1 ms
  //
  // v2 adaptation: also require element buffer empty and scheduler IDLE so
  // we don't fire while the last character is still being keyed.
  uint8_t is_empty = (sched->char_send_buffer_bytes   == 0 &&
                      sched->element_send_buffer_bytes == 0 &&
                      sched->cw_scheduler_state        == IDLE);

#ifdef DEBUG_WINKEY_EMULATION
  {
    static unsigned long _last_dump = 0;
    if (wk->sending && (millis() - _last_dump > 500UL)) {
      _last_dump = millis();
      WK_DBG("WK sched: char="); WK_DBG_DEC(sched->char_send_buffer_bytes);
      WK_DBG(" elem="); WK_DBG_DEC(sched->element_send_buffer_bytes);
      WK_DBG(" state="); WK_DBG_DEC(sched->cw_scheduler_state);
      WK_DBG_NL();
    }
  }
#endif

  // Clear flags from v2 scheduler (not used for echo in this implementation)
  if (sched->char_keying_started)  sched->char_keying_started  = 0;
  if (sched->char_keying_finished) sched->char_keying_finished = 0;

  // Interrupt recovery: paddle grabbed TX (v1 lines 11506-11526)
  if (wk->interrupted) {
    if (ptt_s->ptt_line_asserted == 0) {
      wk->sending     = 0;
      wk->interrupted = 0;
      wk_clear_buffers(wk, sched, ptt_s, cfg);
      wk_write(wk, 0xC6);
      wk_write(wk, 0xC0);
      wk->winkey_buffer_counter = 0;
      wk->winkey_buffer_pointer = 0;
    }
    return;
  }

  // 0xC0 condition — v1 line 11529
  if (wk->sending && is_empty &&
      (wk->port->available() == 0) &&
      ((millis() - wk->last_activity_ms) > WK_C0_WAIT_MS)) {

    WK_DBG("WK hk: empty -> 0xC0 (idle ");
    WK_DBG_DEC(millis() - wk->last_activity_ms);
    WK_DBG_NL();
    wk_done_sending(wk);
    wk->winkey_buffer_counter = 0;
    wk->winkey_buffer_pointer = 0;
  }

  // XOFF / XON flow control
  if (!wk->xoff && sched->char_send_buffer_bytes >= WK_XOFF_THRESHOLD) {
    wk->xoff = 0x10;
    wk_send_status(wk);
  }
  if (wk->xoff && sched->char_send_buffer_bytes <= WK_XON_THRESHOLD) {
    wk->xoff = 0;
    wk_send_status(wk);
  }

  // Command timeout failsafe (v1 line 11545)
  if (wk->winkey_status != WINKEY_NO_COMMAND_IN_PROGRESS &&
      (millis() - wk->last_activity_ms) > 2000UL) {
    wk->winkey_status         = WINKEY_NO_COMMAND_IN_PROGRESS;
    wk->winkey_buffer_counter = 0;
    wk->winkey_buffer_pointer = 0;
    wk_send_status(wk);
  }
}

// ---------------------------------------------------------------------------
// service_winkey_byte()
// Called for every byte received from the host.
// Mirrors v1's SERVICE_SERIAL_BYTE action.
// ---------------------------------------------------------------------------
void service_winkey_byte(WinkeyState* wk, uint8_t b,
                          cw_scheduler_struct* sched,
                          tx_ptt_struct*        ptt_s,
                          config_struct*        cfg) {
  WK_DBG("WK<< 0x"); WK_DBG_HEX(b);
  if (b > 31 && b < 127) { WK_DBG(" '"); WK_DBG_CHAR(b); WK_DBG("'"); }
  WK_DBG(" state="); WK_DBG_DEC(wk->winkey_status);
  WK_DBG_NL();

  wk->last_activity_ms = millis();

  // -------------------------------------------------------------------------
  // TOP LEVEL: no command in progress
  // -------------------------------------------------------------------------
  if (wk->winkey_status == WINKEY_NO_COMMAND_IN_PROGRESS) {

    // CW character (printable ASCII > 31) — v1 lines 11611-11668
    #ifdef OPTION_WINKEY_IGNORE_LOWERCASE
    if ((b > 31 && b < 97) || b == 124) {
      // Lowercase (97-122) excluded from text classification here -- it falls
      // through to the control-byte switch below where nothing matches (all
      // cases are <=31), so it's silently ignored. Genuine K1EL Winkeyer
      // behavior; workaround for SkookumLogger sending lowercase. Byte 124
      // ('|', half-dit-space) stays recognized as text either way.
    #else
    if (b > 31) {

      // Normalise lowercase (v1 line 11620)
      if (b > 96 && b < 123) b -= 32;
    #endif

      WK_DBG("WK char '"); WK_DBG_CHAR(b);
      WK_DBG("' buf="); WK_DBG_DEC(sched->char_send_buffer_bytes);
      WK_DBG_NL();

      // POINTER overwrite mode (v1 lines 11625-11639)
      if (wk->winkey_buffer_pointer > 0) {
        uint8_t pos = sched->char_send_buffer_bytes -
                      (wk->winkey_buffer_counter - wk->winkey_buffer_pointer) - 1;
        if (sched->char_send_buffer_bytes && pos < sched->char_send_buffer_bytes)
          sched->char_send_buffer_array[pos] = b;
        wk->winkey_buffer_pointer++;
      } else {
        add_to_cw_char_send_buffer(sched, b);
        wk->winkey_buffer_counter++;
      }

      // Send 0xC4 on first char — v1 lines 11658-11668
      if (!wk->sending) {
        wk_start_sending(wk);
      }

      // Immediate echo — v1 sends echo from inside the blocking send_char()
      // call.  We echo at receipt so N1MM's window stays open and all macro
      // chars burst in before keying of the first char even starts.
      if (wk->serial_echo) {
        WK_DBG("WK echo 0x"); WK_DBG_HEX(b); WK_DBG_NL();
        wk_write(wk, b);
      }

      return;
    }

    // Control byte (<= 31): dispatch on command opcode
    #ifdef OPTION_WINKEY_STRICT_HOST_OPEN
    if (wk->host_open || b == 0) {
    #endif
    switch (b) {
      case 0x00: wk->winkey_status = WINKEY_ADMIN_COMMAND;                break;
      case 0x01: wk->winkey_status = WINKEY_SIDETONE_FREQ_COMMAND;        break;
      case 0x02: wk->winkey_status = WINKEY_UNBUFFERED_SPEED_COMMAND;     break;
      case 0x03: wk->winkey_status = WINKEY_WEIGHTING_COMMAND;            break;
      case 0x04: wk->winkey_status = WINKEY_PTT_TIMES_PARM1_COMMAND;      break;
      case 0x05: wk->winkey_status = WINKEY_SET_POT_PARM1_COMMAND;        break;
      case 0x06: wk->winkey_status = WINKEY_PAUSE_COMMAND;                break;
      case 0x07:  // report pot
        wk_write(wk, (uint8_t)((cfg->wpm - pot_wpm_low_value) | 0x80));
        break;
      case 0x08:  // backspace
        if (sched->char_send_buffer_bytes > 0) {
          sched->char_send_buffer_bytes--;
          if (wk->winkey_buffer_counter > 0) wk->winkey_buffer_counter--;
        }
        break;
      case 0x09: wk->winkey_status = WINKEY_SET_PINCONFIG_COMMAND; break;
      case 0x0A:  // CLEAR BUFFER — v1 lines 11743-11770
        WK_DBGLN("WK 0x0A clear buffer");
        wk_clear_buffers(wk, sched, ptt_s, cfg);
        if (wk->sending) {
          wk->sending = 0;
          wk_send_status(wk);
        }
        wk->winkey_buffer_counter = 0;
        wk->winkey_buffer_pointer = 0;
        if (wk->saved_wpm) { cfg->wpm = wk->saved_wpm; wk->saved_wpm = 0; }
        break;
      case 0x0B: wk->winkey_status = WINKEY_KEY_COMMAND;              break;
      case 0x0C: wk->winkey_status = WINKEY_HSCW_COMMAND;             break;
      case 0x0D: wk->winkey_status = WINKEY_FARNSWORTH_COMMAND;       break;
      case 0x0E: wk->winkey_status = WINKEY_SETMODE_COMMAND;          break;
      case 0x0F:
        wk->winkey_status = WINKEY_LOAD_SETTINGS_PARM_1_COMMAND;
        wk->load_count    = 0;
        break;
      case 0x10: wk->winkey_status = WINKEY_FIRST_EXTENSION_COMMAND;       break;
      case 0x11: wk->winkey_status = WINKEY_KEYING_COMPENSATION_COMMAND;   break;
      case 0x12:
        wk->winkey_status    = WINKEY_UNSUPPORTED_COMMAND;
        wk->winkey_parmcount = 1;
        break;
      case 0x13: break;  // NULL no-op
      case 0x14: wk->winkey_status = WINKEY_SOFTWARE_PADDLE_COMMAND; break;
      case 0x15: wk_send_status(wk); break;  // report status
      case 0x16: wk->winkey_status = WINKEY_POINTER_COMMAND;          break;
      case 0x17: wk->winkey_status = WINKEY_DAH_DIT_RATIO_COMMAND;    break;
      case 0x18: wk->winkey_status = WINKEY_BUFFFERED_PTT_COMMMAND;   break;
      case 0x19: wk->winkey_status = WINKEY_KEY_BUFFERED_COMMAND;     break;
      case 0x1A: wk->winkey_status = WINKEY_WAIT_BUFFERED_COMMAND;    break;
      case 0x1B: wk->winkey_status = WINKEY_MERGE_COMMAND;            break;
      case 0x1C: wk->winkey_status = WINKEY_BUFFERED_SPEED_COMMAND;   break;
      case 0x1D: wk->winkey_status = WINKEY_BUFFERED_HSCW_COMMAND;    break;
      case 0x1E:  // CANCEL BUFFERED SPEED — v1 lines 11912-11931
        if (wk->saved_wpm) { cfg->wpm = wk->saved_wpm; wk->saved_wpm = 0; }
        wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
        break;
      case 0x1F: break;  // buffered NOP
      default:   break;
    }
    #ifdef OPTION_WINKEY_STRICT_HOST_OPEN
    }
    #endif
    return;
  }

  // -------------------------------------------------------------------------
  // PARAMETER BYTES for in-progress commands
  // -------------------------------------------------------------------------

  if (wk->winkey_status == WINKEY_UNSUPPORTED_COMMAND) {
    if (wk->winkey_parmcount > 0) wk->winkey_parmcount--;
    if (wk->winkey_parmcount == 0) {
      wk_send_status(wk);
      wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    }
    return;
  }

  // 0x0F bulk load settings (15 bytes)
  if (wk->winkey_status >= WINKEY_LOAD_SETTINGS_PARM_1_COMMAND &&
      wk->winkey_status <= WINKEY_LOAD_SETTINGS_PARM_1_COMMAND + 14) {
    winkey_load_settings_command(wk, wk->load_count++, b, cfg, ptt_s);
    wk->winkey_status++;
    if (wk->winkey_status > WINKEY_LOAD_SETTINGS_PARM_1_COMMAND + 14)
      wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_MERGE_COMMAND) {
    add_to_cw_char_send_buffer(sched, b);
    wk->winkey_status = WINKEY_MERGE_PARM_2_COMMAND;
    return;
  }
  if (wk->winkey_status == WINKEY_MERGE_PARM_2_COMMAND) {
    add_to_cw_char_send_buffer(sched, b);
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_UNBUFFERED_SPEED_COMMAND) {
    if (b >= 5 && b <= 99) {
      cfg->wpm = b;
      #ifdef OPTION_WINKEY_STRICT_EEPROM_WRITES_MAY_WEAR_OUT_EEPROM
      config_dirty = 1;
      #endif
    }
    WK_DBG("WK unbuf speed="); WK_DBG_DEC(cfg->wpm); WK_DBG_NL();
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_FARNSWORTH_COMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_HSCW_COMMAND) {
    if (b > 1) {
      cfg->wpm = (unsigned int)b * 100 / 5;
      #ifdef OPTION_WINKEY_STRICT_EEPROM_WRITES_MAY_WEAR_OUT_EEPROM
      config_dirty = 1;
      #endif
    }
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  // BUF_SPEED (0x1C) parameter — v1 adds 3-byte WPM_CHANGE token to send_buffer.
  // v2 applies WPM immediately (functionally identical: BUF_SPEED always
  // precedes the macro chars in N1MM's send sequence).
  if (wk->winkey_status == WINKEY_BUFFERED_SPEED_COMMAND) {
    WK_DBG("WK BUF_SPEED param="); WK_DBG_DEC(b); WK_DBG_NL();
    if (b >= 5 && b <= 99) {
      if (!wk->saved_wpm) wk->saved_wpm = cfg->wpm;
      cfg->wpm = b;
    }
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_BUFFERED_HSCW_COMMAND) {
    if (b > 1) {
      if (!wk->saved_wpm) wk->saved_wpm = cfg->wpm;
      cfg->wpm = (unsigned int)b * 100 / 5;
    }
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_KEY_BUFFERED_COMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_WAIT_BUFFERED_COMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_BUFFFERED_PTT_COMMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  // POINTER 0x16 sub-command
  if (wk->winkey_status == WINKEY_POINTER_COMMAND) {
    switch (b) {
      case 0x00:
        wk->winkey_buffer_counter = 0;
        wk->winkey_buffer_pointer = 0;
        wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
        break;
      case 0x01: wk->winkey_status = WINKEY_POINTER_01_COMMAND; break;
      case 0x02: wk->winkey_status = WINKEY_POINTER_02_COMMAND; break;
      case 0x03: wk->winkey_status = WINKEY_POINTER_03_COMMAND; break;
      default:   wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS; break;
    }
    return;
  }

  if (wk->winkey_status == WINKEY_POINTER_01_COMMAND) {
    wk->winkey_buffer_pointer = b;
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }
  if (wk->winkey_status == WINKEY_POINTER_02_COMMAND) {
    wk->winkey_buffer_pointer = 0;  // append mode, ignore position byte
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }
  if (wk->winkey_status == WINKEY_POINTER_03_COMMAND) {
    for (uint8_t i = b; i > 0; i--) {
      if (wk->winkey_buffer_pointer > 0) {
        // Overwrite mode: null an existing buffer slot
        uint8_t pos = sched->char_send_buffer_bytes -
                      (wk->winkey_buffer_counter - wk->winkey_buffer_pointer) - 1;
        if (sched->char_send_buffer_bytes && pos < sched->char_send_buffer_bytes)
          sched->char_send_buffer_array[pos] = ' ';  // overwrite with silence
        wk->winkey_buffer_pointer++;
      } else {
        // Append mode: v1 adds SERIAL_SEND_BUFFER_NULL (skipped by keying engine).
        // In v2 we just advance the counter — adding 0x00 would key as '?'.
        wk->winkey_buffer_counter++;
      }
    }
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_SIDETONE_FREQ_COMMAND) {
    // Bit 7: paddle-only sidetone flag.  Bits 0-6: frequency index (1-10).
    wk->paddle_only_sidetone = (b & 0x80) ? 1 : 0;
    uint8_t idx = b & 0x7F;
    if (idx >= 1 && idx <= 10) {
      cfg->sidetone_frequency = wk_sidetone_hz[idx];
      #ifdef OPTION_WINKEY_STRICT_EEPROM_WRITES_MAY_WEAR_OUT_EEPROM
      config_dirty = 1;
      #endif
    }
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_WEIGHTING_COMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_PTT_TIMES_PARM1_COMMAND) {
    wk->ptt_lead_param   = b;
    ptt_s->ptt_lead_time = (unsigned int)b * 10;
    cfg->ptt_lead_time   = ptt_s->ptt_lead_time;
    wk->winkey_status    = WINKEY_PTT_TIMES_PARM2_COMMAND;
    return;
  }
  if (wk->winkey_status == WINKEY_PTT_TIMES_PARM2_COMMAND) {
    wk->ptt_tail_10ms    = b;
    ptt_s->ptt_tail_time = (unsigned int)b * 10;
    cfg->ptt_tail_time   = ptt_s->ptt_tail_time;
    wk->winkey_status    = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_SET_POT_PARM1_COMMAND) {
    wk->winkey_status = WINKEY_SET_POT_PARM2_COMMAND; return;
  }
  if (wk->winkey_status == WINKEY_SET_POT_PARM2_COMMAND) {
    wk->winkey_status = WINKEY_SET_POT_PARM3_COMMAND; return;
  }
  if (wk->winkey_status == WINKEY_SET_POT_PARM3_COMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS; return;
  }

  if (wk->winkey_status == WINKEY_PAUSE_COMMAND) {
    sched->pause_sending_buffer = (b & 0x01) ? 1 : 0;
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_SET_PINCONFIG_COMMAND) {
    // Bit 1: sidetone enable (1=on, 0=muted).
    ptt_s->sidetone_enabled = (b & 0x02) ? 1 : 0;
    // Bit 0: PTT enable. Only acted on (gates the actual hardware PTT line in
    // ptt()) when OPTION_WINKEY_PINCONFIG_PTT_CONTROLS_PTT_LINE is defined;
    // otherwise informational only, matching v1's default behavior.
    #ifdef OPTION_WINKEY_PINCONFIG_PTT_CONTROLS_PTT_LINE
    ptt_s->ptt_line_enabled = (b & 0x01) ? 1 : 0;
    #endif
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_KEY_COMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_SETMODE_COMMAND) {
    if (b & 0x01) cfg->keyer_mode  = IAMBIC_A;      else cfg->keyer_mode  = IAMBIC_B;
    if (b & 0x02) cfg->paddle_mode = PADDLE_REVERSE; else cfg->paddle_mode = PADDLE_NORMAL;
    wk->serial_echo          = (b & 0x04) ? 1 : 0;
    wk->paddle_only_sidetone = (b & 0x10) ? 1 : 0;
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_FIRST_EXTENSION_COMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_KEYING_COMPENSATION_COMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_SOFTWARE_PADDLE_COMMAND) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_DAH_DIT_RATIO_COMMAND) {
    // TODO: not yet implemented (byte discarded). When it is, gate the
    // cfg->dah_to_dit_ratio write with OPTION_WINKEY_STRICT_EEPROM_WRITES_MAY_WEAR_OUT_EEPROM
    // to match the other Winkey speed/ratio/sidetone commands.
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  // ADMIN sub-commands
  if (wk->winkey_status == WINKEY_ADMIN_COMMAND) {
    switch (b) {
      case 0x00:
        wk->winkey_status    = WINKEY_UNSUPPORTED_COMMAND;
        wk->winkey_parmcount = 1;
        break;
      case 0x01:  // reset (not implemented)
        wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
        break;
      case 0x02:  // HOST OPEN — v1 line 12226
        wk_write(wk, WINKEY_VERSION);
        wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
        wk->host_open = 1;
        wk->sending   = 0;
        WK_DBGLN("WK host open");
        break;
      case 0x03:  // HOST CLOSE
        #ifdef OPTION_WINKEY_SEND_VERSION_ON_HOST_CLOSE
        wk_write(wk, WINKEY_VERSION);
        #endif
        wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
        wk->host_open = 0;
        WK_DBGLN("WK host close");
        break;
      case 0x04:
        wk->winkey_status = WINKEY_ADMIN_COMMAND_ECHO;
        break;
      case 0x05:
        wk_write(wk, 0x00);
        wk->winkey_status = WINKEY_ADMIN_PADDLE_A2D_PARM_1;
        break;
      case 0x06:
        wk_write(wk, 0x00);
        wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
        break;
      case 0x07:
        wk_send_values(wk, cfg, ptt_s);
        wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
        break;
      case 0x09:
        wk_write(wk, 0x00);
        wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
        break;
      case 0x0C:
        wk->winkey_status    = WINKEY_UNSUPPORTED_COMMAND;
        wk->winkey_parmcount = 1;
        break;
      case 0x0E:
        wk->winkey_status = WINKEY_SEND_MSG;
        break;
      default:
        wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
        break;
    }
    return;
  }

  if (wk->winkey_status == WINKEY_ADMIN_COMMAND_ECHO) {
    wk_write(wk, b);
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_ADMIN_PADDLE_A2D_PARM_1) {
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  if (wk->winkey_status == WINKEY_SEND_MSG) {
    if (b >= 1 && b <= 6) wk->pending_memory = b;
    wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
    return;
  }

  // Catch-all
  wk->winkey_status = WINKEY_NO_COMMAND_IN_PROGRESS;
}

#endif // FEATURE_WINKEY_EMULATION
