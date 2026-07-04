#ifndef keyer_2_features_and_options_h
#define keyer_2_features_and_options_h

// K3NG CW Keyer v2 — Features and Options
//
// Compile-time feature enables and behavioral options.
// FEATURES add code and increase binary size.
// OPTIONS change behavior of existing code without adding new features.
//
// Adapted from keyer_features_and_options.h (K3NG CW Keyer v1).
//
// ---------------------------------------------------------------------------
// Core functionality — always compiled in, no feature flag required:
//
//   Iambic A mode
//   Iambic B mode
//   Straight key mode
//   Bug mode
//   Paddle reverse
//   Sidetone (via tone())
//   PTT with lead time and tail time
//   Basic serial CW keyboard and command interface
//   SAY HI — sends HI_TEXT in CW via sidetone at startup
//   FEATURE_COMMAND_LINE_INTERFACE — full v1-compatible backslash CLI
//   FEATURE_SERIAL_HELP — \? help text
// ---------------------------------------------------------------------------

// #define OPTION_DO_NOT_SAY_HI                           // Skip the startup CW send (or change HI_TEXT to customize)

#define FEATURE_COMMAND_LINE_INTERFACE                    // Full v1-compatible CLI over serial port
#define FEATURE_SERIAL_HELP                               // \? help text in CLI
#define FEATURE_COMMAND_MODE                              // Button-activated CW command mode
#define FEATURE_BUTTONS                                   // Analog multiplexed button array (R1/R2 resistor ladder on analog_buttons_pin)
#define FEATURE_MEMORIES                                  // CW memory storage/playback via EEPROM (buttons 1-3, \1-\3, command mode P)
#define FEATURE_MEMORY_MACROS                             // Backslash macro commands within memories (\S \E \C \N \W \Y \Z \D \U \V \T \F \I \0-\9)
#define FEATURE_POTENTIOMETER                     // Speed control potentiometer (do not enable without hardware connected)
#define FEATURE_PADDLE_ECHO                       // Echo paddle characters to serial port
#define FEATURE_BEACON                            // Beacon mode: hold paddle_left at boot to loop memory 1
#define FEATURE_BEACON_SETTING                    // \_ CLI command to persist beacon-on-boot setting to EEPROM
#define OPTION_BEACON_MODE_MEMORY_REPEAT_TIME     // Delay between successive memory-1 playbacks in beacon mode
#define OPTION_BEACON_MODE_PTT_TAIL_TIME          // Wait for PTT tail to drop before replaying in beacon mode
// #define FEATURE_WINKEY_EMULATION               // Winkeyer 2 protocol emulation (disable ASR — see docs)
#define FEATURE_ADDITIONAL_TX_AND_PTT_PINS            // Additional TX key lines 2-6 and PTT lines 2-6; \X# CLI command
// #define FEATURE_ROTARY_ENCODER                 // Rotary encoder speed control
#define FEATURE_FARNSWORTH                     // Farnsworth sending speed
#define FEATURE_SIDETONE_SWITCH                // External toggle switch for sidetone on/off
#define FEATURE_DEAD_OP_WATCHDOG               // Watchdog that clears TX if paddle stuck > 100 consecutive dits or dahs
//#define FEATURE_QLF                            // QLF (poor fist) mode
#define FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING  // CMOS Super Keyer Iambic B timing
#define FEATURE_DYNAMIC_DAH_TO_DIT_RATIO       // Dynamically adjust dah-to-dit ratio
#define FEATURE_STRAIGHT_KEY                   // Dedicated straight key input on separate pin

// #define OPTION_CMOS_SUPER_KEYER_IAMBIC_B_TIMING_ON_BY_DEFAULT  // Enable CMOS Super Keyer timing by default


// #define DEBUG_WINKEY_EMULATION             // Verbose Winkey debug output on DEBUG_WINKEY_PORT
// #define DEBUG_WINKEY_PORT       Serial3    // Serial port for Winkey debug output
// #define DEBUG_WINKEY_PORT_BAUD  115200     // Baud rate for debug port



// *** Not implemented yet ***

// --- Features ---

// #define FEATURE_SIDETONE_NEWTONE               // Use NewTone library instead of standard tone() (~1k smaller; timer1, pins 9/10)
// #define FEATURE_AUTOSPACE                      // Automatic character spacing




#define FEATURE_STRAIGHT_KEY_ECHO              // Echo straight key characters to serial port
// #define FEATURE_CW_DECODER                     // CW decoder (https://github.com/k3ng/k3ng_cw_keyer/wiki/385-Feature:-CW-Decoder)
// #define FEATURE_TRAINING_COMMAND_LINE_INTERFACE // CW training via CLI
// #define FEATURE_ALPHABET_SEND_PRACTICE         // Command mode S: alphabet practice (by Ryan, KC2ZWM)
// #define FEATURE_COMMAND_MODE_PROGRESSIVE_5_CHAR_ECHO_PRACTICE  // Command mode U
// #define FEATURE_COMMAND_MODE_ENHANCED_CMD_ACKNOWLEDGEMENT
// #define FEATURE_HELL                           // Hellschreiber mode
// #define FEATURE_AMERICAN_MORSE                 // American Morse mode
// #define FEATURE_CAPACITIVE_PADDLE_PINS         // Capacitive touch paddle pins (remove bypass caps from paddle lines)
// #define FEATURE_PTT_INTERLOCK                  // PTT interlock input pin
#define FEATURE_SEQUENCER                      // TX sequencer output pins
// #define FEATURE_DL2SBA_BANKSWITCH              // Memory bank switching (http://dl2sba.com)
// #define FEATURE_EEPROM_E24C1024                // External 1Mbit I2C EEPROM support
// #define FEATURE_SD_CARD_SUPPORT                // SD card support
// #define FEATURE_SLEEP                          // Sleep mode after inactivity (not compatible with Arduino Due)
// #define FEATURE_INTERNET_LINK                  // Internet linking (https://github.com/k3ng/k3ng_cw_keyer/wiki/390-Feature:-Ethernet,-Web-Server,-and-Internet-Linking)
// #define FEATURE_WEB_SERVER                     // Built-in web server (https://github.com/k3ng/k3ng_cw_keyer/wiki/390-Feature:-Ethernet,-Web-Server,-and-Internet-Linking)

// --- Display features (choose one) ---
// #define FEATURE_LCD_4BIT                       // Classic HD44780 LCD, 4-bit parallel
// #define FEATURE_LCD_8BIT                       // Classic HD44780 LCD, 8-bit parallel
// #define FEATURE_LCD_ADAFRUIT_I2C               // Adafruit I2C LCD (MCP23017 at 0x20)
// #define FEATURE_LCD_ADAFRUIT_BACKPACK          // Adafruit I2C LCD backpack (MCP23008)
// #define FEATURE_LCD_YDv1                       // YourDuino I2C LCD (LCM 1602 V1)
// #define FEATURE_LCD_TWILIQUIDCRYSTAL           // I2C 1602 with backlight (TwiLiquidCrystal library)
// #define FEATURE_LCD1602_N07DH                  // LinkSprite 16x2 LCD keypad shield
// #define FEATURE_LCD_SAINSMART_I2C              // SainSmart I2C LCD
// #define FEATURE_LCD_FABO_PCF8574               // FaBo PCF8574 I2C LCD
// #define FEATURE_LCD_MATHERTEL_PCF8574          // mathertel LiquidCrystal_PCF8574
// #define FEATURE_LCD_I2C_FDEBRABANDER           // fdebrabander Arduino LiquidCrystal I2C library
// #define FEATURE_LCD_HD44780                    // HD44780 direct
// #define FEATURE_OLED_SSD1306                   // SSD1306 OLED (SSD1306Ascii library)
// #define FEATURE_LCD_BACKLIGHT_AUTO_DIM         // Auto-dim LCD backlight and power LED after inactivity

// --- Keyboard / input features ---
// #define FEATURE_PS2_KEYBOARD                   // PS/2 keyboard input
// #define FEATURE_USB_KEYBOARD                   // USB keyboard input (uncomment include lines in .ino — search note_usb_uncomment_lines)
// #define FEATURE_USB_MOUSE                      // USB mouse input (uncomment include lines in .ino — search note_usb_uncomment_lines)
// #define FEATURE_CW_COMPUTER_KEYBOARD           // Send paddle characters as USB HID keystrokes (Due/Leonardo)
// #define FEATURE_4x4_KEYPAD                     // 4x4 matrix keypad (by Jack, W0XR — see wiki/380)
// #define FEATURE_3x4_KEYPAD                     // 3x4 matrix keypad (by Jack, W0XR — see wiki/380)
// #define FEATURE_LED_RING                       // Mayhew Labs LED ring
// #define FEATURE_MIDI                           // MIDI output on supported hardware (Teensy 3.x)

// --- SO2R features ---
// #define FEATURE_SO2R_BASE                      // SO2R box base protocol extensions
// #define FEATURE_SO2R_SWITCHES                  // SO2R box TX/RX switches
// #define FEATURE_SO2R_ANTENNA                   // SO2R box antenna selection (not fully implemented)


// --- Options ---

// #define OPTION_SUPPRESS_SERIAL_BOOT_MSG                // Do not print the startup message to serial port
// #define OPTION_DO_NOT_SEND_UNKNOWN_CHAR_QUESTION       // Send nothing (instead of ?) for unknown characters
// #define OPTION_INVERT_PADDLE_PIN_LOGIC                 // Invert paddle pin logic (for active-high paddles)
// #define OPTION_INCLUDE_PTT_TAIL_FOR_MANUAL_SENDING     // Include PTT tail time for manual (paddle) sending
// #define OPTION_EXCLUDE_PTT_HANG_TIME_FOR_MANUAL_SENDING // Exclude PTT hang time for manual sending
// #define OPTION_UNKNOWN_CHARACTER_ERROR_TONE            // Sound an error tone for unknown characters
// #define OPTION_WATCHDOG_TIMER                          // Enable 4-second ATmega watchdog (unattended/remote use only)
// #define OPTION_SWAP_PADDLE_PARAMETER_CHANGE_DIRECTION  // Reverse up/down direction for paddle-based WPM/frequency change
// #define OPTION_NON_ENGLISH_EXTENSIONS                  // Additional CW characters (À, Å, Þ, etc.)
// #define OPTION_DISPLAY_NON_ENGLISH_EXTENSIONS          // LCD display support for non-English characters (by OZ1JHM)
// #define OPTION_PROSIGN_SUPPORT                         // Additional prosign support for echo and memory storage
// #define OPTION_ADVANCED_SPEED_DISPLAY                  // "Nerd" speed display: WPM, CPM, dit/dah ms, ratio (by Giorgio, IZ2XBZ)
// #define OPTION_CW_KEYBOARD_CAPSLOCK_BEEP               // Beep when Caps Lock is toggled on CW keyboard
// #define OPTION_CW_KEYBOARD_ITALIAN                     // Italian keyboard layout
// #define OPTION_CW_KEYBOARD_GERMAN                      // German keyboard layout
// #define OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR      // Goertzel audio CW decoder (see wiki/385)
// #define OPTION_SIDETONE_DIGITAL_OUTPUT_NO_SQUARE_WAVE  // Sidetone on digital output without square wave
// #define OPTION_DIRECT_PADDLE_PIN_READS_MEGA            // Direct port reads on Mega pins 2 and 5 (minor performance gain)
// #define OPTION_DIRECT_PADDLE_PIN_READS_UNO             // Direct port reads on Uno/Nano pins 2 and 5 (not for NanoKeyer)
// #define OPTION_MORE_DISPLAY_MSGS                       // Additional optional display messages (uses more memory)
// #define OPTION_MOUSE_MOVEMENT_PADDLE                   // Experimental: mouse movement acts as a paddle
// #define OPTION_PS2_NON_ENGLISH_CHAR_LCD_DISPLAY_SUPPORT  // Non-English PS2 characters on LCD (by Marcin, SP5IOU)
// #define OPTION_PS2_KEYBOARD_RESET                      // Reset PS2 keyboard at startup with 0xFF (by Bill, W9BEL)
// #define OPTION_SAVE_MEMORY_NANOKEYER                   // Reduce memory usage for NanoKeyer builds
// #define OPTION_REVERSE_BUTTON_ORDER                    // Reverse button order (for DJ0MY NanoKeyer)
// #define OPTION_PROG_MEM_TRIM_TRAILING_SPACES           // Trim trailing spaces from memory when programming in command mode
// #define OPTION_DIT_PADDLE_NO_SEND_ON_MEM_RPT           // Smoother dit paddle memory interruption
// #define OPTION_DISPLAY_MEMORY_CONTENTS_COMMAND_MODE    // Display memory contents in command mode
// #define OPTION_PERSONALIZED_STARTUP_SCREEN             // User-defined string on second/fourth LCD row at startup
// #define OPTION_EXCLUDE_EXTENDED_CLI_COMMANDS           // Remove extended CLI commands (\:) to save memory
// #define OPTION_EXCLUDE_MILL_MODE                       // Remove Mill mode to save memory
// #define OPTION_NO_ULTIMATIC                            // Remove Ultimatic mode to save memory
// #define OPTION_DISABLE_SERIAL_PORT_CHECKING_WHILE_SENDING_CW  // Disable serial port checking during CW sending

// --- Winkey emulation options ---
// #define OPTION_PRIMARY_SERIAL_PORT_DEFAULT_WINKEY_EMULATION  // Winkey emulation is default; hold command button at boot for CLI
// #define OPTION_WINKEY_DISCARD_BYTES_AT_STARTUP         // Discard errant serial bytes at startup (when ASR is not disabled)
// #define OPTION_WINKEY_STRICT_EEPROM_WRITES_MAY_WEAR_OUT_EEPROM  // Write non-volatile settings to EEPROM on Winkey set commands
// #define OPTION_WINKEY_SEND_WORDSPACE_AT_END_OF_BUFFER
// #define OPTION_WINKEY_STRICT_HOST_OPEN                 // Require Winkey host open command before any other commands
// #define OPTION_WINKEY_2_SUPPORT                        // Winkey version 2 emulation (comment out for version 1)
// #define OPTION_WINKEY_SEND_BREAKIN_STATUS_BYTE
// #define OPTION_WINKEY_INTERRUPTS_MEMORY_REPEAT
// #define OPTION_WINKEY_UCXLOG_9600_BAUD                 // UCXLog 9600 baud Winkey mode
// #define OPTION_WINKEY_2_HOST_CLOSE_NO_SERIAL_PORT_RESET  // Required for Win-Test compatibility
// #define OPTION_WINKEY_FREQUENT_STATUS_REPORT           // More frequent status reports (for RUMlog/RUMped)
// #define OPTION_WINKEY_IGNORE_LOWERCASE                 // K1EL Winkeyer behavior: ignore lowercase (workaround for SkookumLogger bug)
// #define OPTION_WINKEY_BLINK_PTT_ON_HOST_OPEN
// #define OPTION_WINKEY_SEND_VERSION_ON_HOST_CLOSE
// #define OPTION_WINKEY_PINCONFIG_PTT_CONTROLS_PTT_LINE  // Winkey PTT setting activates/deactivates PTT line
// #define OPTION_WINKEY_PROSIGN_COMPATIBILITY            // Additional character mappings for K1EL Winkey prosigns

// --- Beacon options ---
// #define OPTION_BEACON_MODE_MEMORY_REPEAT_TIME          // Space out repeated playback of memory 1 in beacon mode
// #define OPTION_BEACON_MODE_PTT_TAIL_TIME               // Add PTT tail time between beacon playbacks

// --- Wordsworth training word list options ---
// #define OPTION_WORDSWORTH_CZECH
// #define OPTION_WORDSWORTH_DEUTSCH
// #define OPTION_WORDSWORTH_NORSK
// #define OPTION_WORDSWORTH_POLISH

// --- Russian language ---
// #define OPTION_RUSSIAN_LANGUAGE_SEND_CLI               // Russian language CLI sending (by Павел Бирюков, UA1AQC)

// --- MIDI options (used with FEATURE_MIDI) ---
// #define OPTION_MIDI_BASE_NOTE           0   // Base MIDI note number
// #define OPTION_MIDI_KEYER_CHANNEL       1   // MIDI channel for keyer output
// #define OPTION_MIDI_INPUT_CHANNEL       2   // MIDI channel for commands from computer
// #define OPTION_MIDI_WPM_CONTROL         0   // MIDI control number for WPM
// #define OPTION_MIDI_IS_KEYER_CONTROL    1   // MIDI control: keyer vs. dumb interface
// #define OPTION_MIDI_REVERSE_CONTROL     2   // MIDI control: paddle reverse
// #define OPTION_MIDI_IAMBIC_CONTROL      3   // MIDI control: Iambic A vs. B
// #define OPTION_MIDI_GET_KEYER_STATE_CONTROL 4
// #define OPTION_MIDI_RESPONSE_CHANNEL    3
// #define OPTION_MIDI_RESPONSE_FAIL       0
// #define OPTION_MIDI_RESPONSE_OK         1
// #define OPTION_MIDI_RESPONSE_IS_KEYER   2
// #define OPTION_MIDI_RESPONSE_WPM        3
// #define OPTION_MIDI_RESPONSE_REVERSE    4
// #define OPTION_MIDI_RESPONSE_IAMBIC     5

// --- DFRobot LCD ---
// #define OPTION_DFROBOT_LCD_COMMAND_BUTTONS

// --- Misc ---
// #define OPTION_SWAP_PADDLE_PARAMETER_CHANGE_DIRECTION  // Reverses up/down direction for paddle-based parameter changes

#endif // keyer_2_features_and_options_h
