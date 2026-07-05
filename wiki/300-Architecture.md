# Architecture

## Design Approach

Version 2 is built around a single invariant: **`loop()` always returns quickly.** Every timing decision is made by comparing `millis()` to a stored deadline.

This is a deliberate departure from v1, where all CW element timing ran inside a blocking function, `loop_element_lengths()`, that spun in a `while (micros() - start < ticks)` busy-wait for the duration of every dit and dah. The whole program froze inside that loop while an element was being sent, so servicing paddles, the serial port, a display, or a rotary encoder during that time meant cramming it all inside the while loop — which grew increasingly unwieldy as more features were added. v2's non-blocking scheduler removes that constraint entirely.

## The CW State Machine

The heart of v2 is `service_cw_scheduler()`, defined in `keyer_2_cw.cpp`. It is called **multiple times per `loop()` iteration** and advances a state machine through these states:

```
IDLE  →  PTT_LEAD_TIME_WAIT  →  KEY_DOWN  →  KEY_UP  →  IDLE
                                KEY_DOWN_HOLD (straight key)
```

At each call, the scheduler:
1. Checks whether the current state's timer has expired and transitions if so
2. When IDLE, pops the next element (dit/dah/space) from the element buffer
3. When the element buffer is empty, pops the next character from the char buffer and converts it to elements

No state blocks. Each call takes microseconds.

## Two-Tier Send Buffer

```
 ┌─────────────────────────────────────────────────────────┐
 │  ASCII char buffer  (50 chars)                          │
 │  ← serial keyboard, memories, macros, Winkey            │
 └───────────────────────┬─────────────────────────────────┘
                         │  convert_char_to_elements()
 ┌─────────────────────────────────────────────────────────┐
 │  Element buffer  (20 elements: dit / dah / wordspace)   │
 └───────────────────────┬─────────────────────────────────┘
                         │  service_cw_scheduler() step 2
 ┌─────────────────────────────────────────────────────────┐
 │  Key state machine  →  TX pin  +  PTT pin  +  sidetone  │
 └─────────────────────────────────────────────────────────┘
```

## The Main Loop

```cpp
void loop() {
  check_paddles();                 // read paddle pins; queue elements immediately
  service_ptt_interlock();         // FEATURE_PTT_INTERLOCK
  service_cw_scheduler();          // advance key state machine
  check_ptt_tail();                // release PTT after tail time
  service_sequencer();             // FEATURE_SEQUENCER — drive TX sequencer output pins
  service_serial();                // process CLI / Winkey bytes
  service_cw_scheduler();          // (called again for responsiveness)
  service_sound();                 // advance beep/boop state machine
  service_beacon_mode();           // FEATURE_BEACON + FEATURE_MEMORIES
  check_potentiometer();           // FEATURE_POTENTIOMETER — WPM pot
  check_rotary_encoder();          // FEATURE_ROTARY_ENCODER
  check_sidetone_switch();         // FEATURE_SIDETONE_SWITCH — external sidetone on/off
  service_straight_key();          // FEATURE_STRAIGHT_KEY
  service_straight_key_echo();     // FEATURE_STRAIGHT_KEY_ECHO
  service_paddle_echo();           // FEATURE_PADDLE_ECHO — echo paddle chars to serial
  check_for_dirty_configuration(); // auto-save EEPROM after 30s
  service_cw_scheduler();          // (called again for responsiveness)
  service_winkey_housekeeping();   // FEATURE_WINKEY_EMULATION
  service_memory_program();        // FEATURE_MEMORIES — paddle memory entry
  service_command_mode();          // FEATURE_COMMAND_MODE
  check_buttons();                 // FEATURE_BUTTONS — analog button array
  service_cw_scheduler();          // (called again for responsiveness)
}
```

Every line above is wrapped in that feature's `#ifdef` in the actual source (omitted here for readability) except `check_paddles()`, `service_cw_scheduler()`, `check_ptt_tail()`, `service_serial()`, `service_sound()`, and `check_for_dirty_configuration()`, which are unconditional. `service_cw_scheduler()` is called four times per loop to minimize latency between a paddle hit (or the end of the previous element) and the start of the next one.

## PTT Lead Time

When a keydown is requested and PTT lead time is configured, the scheduler transitions to `PTT_LEAD_TIME_WAIT` state, asserts PTT, and stores the deadline. On the next (and subsequent) `loop()` calls, the scheduler checks whether the deadline has passed before asserting the TX key line.

## Key Data Structures

`config_struct` — runtime-adjustable settings, persisted to EEPROM.  
`cw_scheduler_struct` — all state for the CW state machine and send buffers.  
`tx_ptt_struct` — TX key line state, PTT state, pin assignments, timing.

These are global instances (`configuration`, `cw_scheduler`, `tx_ptt`) used throughout the sketch.
