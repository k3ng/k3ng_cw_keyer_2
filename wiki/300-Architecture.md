# Architecture — Non-Blocking Design

## The Problem with v1

The original K3NG CW Keyer accumulated blocking `delay()` calls over many years of feature additions. While functional, these made the code harder to extend and meant the Arduino was unavailable for other work during timing waits.

## The v2 Approach

Version 2 is built around a single invariant: **`loop()` never blocks.** Every timing decision is made by comparing `millis()` to a stored deadline, never by sleeping.

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
  check_paddles();             // read paddle pins; queue elements immediately
  service_cw_scheduler();      // advance key state machine
  check_ptt_tail();            // release PTT after tail time
  service_sequencer();         // drive TX sequencer output pins
  service_serial();            // process CLI / Winkey bytes
  service_cw_scheduler();      // (called again for responsiveness)
  service_sound();             // advance beep/boop state machine
  service_beacon_mode();       // beacon mode timing
  check_potentiometer();       // WPM pot
  check_sidetone_switch();     // external sidetone on/off
  service_straight_key();      // straight key pin
  service_paddle_echo();       // echo paddle chars to serial
  check_for_dirty_configuration(); // auto-save EEPROM after 30s
  service_memory_program();    // non-blocking paddle memory entry
  service_command_mode();      // CW command mode state machine
  check_buttons();             // analog button array
  service_cw_scheduler();      // (called again for responsiveness)
}
```

`service_cw_scheduler()` is called three times per loop to minimize latency between a paddle hit and the start of the element.

## PTT Lead Time (Non-Blocking)

In v1, PTT lead time was implemented with a blocking `delay()`. In v2, when a keydown is requested and PTT lead time is configured, the scheduler transitions to `PTT_LEAD_TIME_WAIT` state, asserts PTT, and stores the deadline. On the next (and subsequent) `loop()` calls, the scheduler checks whether the deadline has passed before asserting the TX key line. The loop is free to do other work during the wait.

## Key Data Structures

`config_struct` — runtime-adjustable settings, persisted to EEPROM.  
`cw_scheduler_struct` — all state for the CW state machine and send buffers.  
`tx_ptt_struct` — TX key line state, PTT state, pin assignments, timing.

These are global instances (`configuration`, `cw_scheduler`, `tx_ptt`) used throughout the sketch.
