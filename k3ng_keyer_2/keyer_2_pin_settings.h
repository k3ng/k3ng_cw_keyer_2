/* K3NG CW Keyer v2 — Pin Settings
 *
 * Review and configure all pins for your hardware before compiling.
 * 0 = pin not used / feature disabled.
 *
 * Adapted from keyer_pin_settings.h (K3NG CW Keyer v1).
 */

#ifndef keyer_2_pin_settings_h
#define keyer_2_pin_settings_h

// ---------------------------------------------------------------------------
// Core pins — active in v2 code
// ---------------------------------------------------------------------------

#define paddle_left      2    // Dit paddle input  (active LOW, uses internal pullup)
#define paddle_right     5    // Dah paddle input  (active LOW, uses internal pullup)

#define tx_key_line_1   11    // TX key line 1  (HIGH = key down / TX on)

#define ptt_tx_1         13    // PTT line for TX 1  (0 = not used)

#define sidetone_line    4    // Sidetone speaker output  (via tone())

#define status_led      13    // Status / heartbeat LED

#define command_button   0    // Command mode entry button — standalone digital pin (0 = disabled)
                               // Only used when FEATURE_BUTTONS is NOT defined.
                               // When FEATURE_BUTTONS is defined, button 0 on the analog array is
                               // the command button and this define is ignored.

// Analog multiplexed button array (FEATURE_BUTTONS)
// Connect a resistor ladder to this pin: R1 pullup to VCC, buttons to GND via N*R2 resistors.
// See keyer_settings.h for R1/R2 values and analog_buttons_number_of_buttons.
#ifdef FEATURE_BUTTONS
  #define analog_buttons_pin       A1   // Analog input pin for the button array
  #define command_mode_active_led   0   // LED to illuminate while in command mode (0 = disabled)
#endif


// *** Not implemented yet ***

// Additional TX key lines
#define tx_key_line_2    0
#define tx_key_line_3    0
#define tx_key_line_4    0
#define tx_key_line_5    0
#define tx_key_line_6    0

// Additional PTT lines
#define ptt_tx_2         0
#define ptt_tx_3         0
#define ptt_tx_4         0
#define ptt_tx_5         0
#define ptt_tx_6         0

// Separate dit / dah output pins (go active during each element, any transmitter)
#define tx_key_dit       0
#define tx_key_dah       0

// Speed potentiometer
#define potentiometer              A0   // 0–5 V speed pot (1k–10k ohm)
#define potentiometer_enable_pin    0   // Hold LOW to enable pot; 0 = always enabled

// TX inhibit / pause pins
#define tx_inhibit_pin   0    // Hold HIGH to inhibit TX key line
#define tx_pause_pin     0    // Hold HIGH to pause sending

// Sending-mode indicator output pins
#define pin_sending_mode_automatic  0   // Goes HIGH during automatic (memory/serial) sending
#define pin_sending_mode_manual     0   // Goes HIGH during manual (paddle/straight key) sending

// External PTT input interlock
#define ptt_input_pin    0

// Training mode feedback LEDs
#define correct_answer_led  0
#define wrong_answer_led    0

// Straight key (separate from paddles)
#ifdef FEATURE_STRAIGHT_KEY
  #define pin_straight_key  52
#endif

// Sidetone on/off toggle switch
#ifdef FEATURE_SIDETONE_SWITCH
  #define SIDETONE_SWITCH   8
#endif

// Dedicated straight key input pin (FEATURE_STRAIGHT_KEY)
// Set to a real pin number; pulled up internally — key tip connects pin to GND.
#ifdef FEATURE_STRAIGHT_KEY
  #define pin_straight_key  0   // 0 = not connected; set to actual pin
#endif

// LCD pins (4-bit mode)
#if defined(FEATURE_LCD_4BIT) || defined(FEATURE_LCD_8BIT)
  #define lcd_rs      A2
  #define lcd_enable  10
  #define lcd_d4       6
  #define lcd_d5       7
  #define lcd_d6       8
  #define lcd_d7       9
#endif

#if defined(FEATURE_LCD_8BIT)
  #define lcd_d0  20
  #define lcd_d1  21
  #define lcd_d2  22
  #define lcd_d3  23
#endif

// PS/2 keyboard
#ifdef FEATURE_PS2_KEYBOARD
  #define ps2_keyboard_data   A3
  #define ps2_keyboard_clock   3   // must be interrupt-capable
#endif

// Rotary encoder (speed knob)
#ifdef FEATURE_ROTARY_ENCODER
  #define rotary_pin1  0
  #define rotary_pin2  0
  #define OPTION_ENCODER_ENABLE_PULLUPS
#endif

// Buttons
#ifdef FEATURE_BUTTONS
  #define analog_buttons_pin     A1
  #define command_mode_active_led  0
#endif

// Sequencer output pins
#ifdef FEATURE_SEQUENCER
  #define sequencer_1_pin  0
  #define sequencer_2_pin  0
  #define sequencer_3_pin  0
  #define sequencer_4_pin  0
  #define sequencer_5_pin  0
#endif

// CW decoder
#define cw_decoder_pin             0   // External hardware decoder input
#define cw_decoder_audio_input_pin 0   // Analog audio input for Goertzel decoder
#define cw_decoder_indicator       0   // Output HIGH when CW tone detected

// LCD backlight auto-dim (must be PWM-capable)
#ifdef FEATURE_LCD_BACKLIGHT_AUTO_DIM
  #define keyer_power_led  0
#endif

// PTT interlock
#ifdef FEATURE_PTT_INTERLOCK
  #define ptt_interlock  0
#endif

#else
  #error "Multiple keyer_2_pin_settings.h files included"
#endif // keyer_2_pin_settings_h
