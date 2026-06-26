#ifndef keyer_2_serial_h
#define keyer_2_serial_h

#include <Arduino.h>

#define SERIAL_MODE_DISABLED  0
#define SERIAL_MODE_CLI       1
#define SERIAL_MODE_WINKEY    2   // reserved for future FEATURE_WINKEY_EMULATION

#define KEYER_MAX_SERIAL_PORTS  4

struct KeyerSerialPort {
  HardwareSerial* port;     // pointer to Serial, Serial1, Serial2, or Serial3
  long             baud;
  uint8_t          mode;           // SERIAL_MODE_* constant
  uint8_t          backslash_flag; // CLI parse state: 1 if last char was backslash
};

#endif // keyer_2_serial_h
