# Feature: Dead Operator Watchdog

`FEATURE_DEAD_OP_WATCHDOG`

## Overview

The Dead Operator Watchdog detects a stuck paddle — a situation where the paddle contacts are shorted or jammed closed — and clears the TX to prevent the transmitter from keying indefinitely.

## How It Works

The watchdog counts consecutive elements (dits and dahs) sent during manual paddle sending. If more than a configurable threshold of elements are sent without a pause, the watchdog assumes the paddle is stuck, immediately clears the TX key line and send buffers, and releases PTT.

The default threshold is **100 consecutive elements**. At 20 WPM, a dit is about 60 ms, so 100 elements ≈ 6 seconds of continuous keying before the watchdog fires.

## Configuration

In `keyer_settings.h`:
```cpp
#define dead_op_watchdog_element_count  100
```

Increase this if you are a very fast sender and find the watchdog triggering during normal sending. Decrease it for tighter protection.

## Squeeze Detection

The watchdog also detects a stuck **squeeze** (both paddles simultaneously closed indefinitely), which would produce an endless dit-dah-dit-dah alternating pattern. This counts toward the element threshold.

## What Happens When the Watchdog Fires

- TX key line is immediately released
- PTT is released
- Send buffers are cleared
- A serial message is printed if the CLI is active

## When This Matters

Without this feature, a stuck paddle on an unattended beacon station or a paddle that gets something dropped on it could key the transmitter indefinitely — potentially causing interference, violating regulations, or overheating the PA.

## Watchdog vs. Beacon Mode

The watchdog monitors **manual** (paddle) sending only. Automatic memory playback does not increment the watchdog counter.
