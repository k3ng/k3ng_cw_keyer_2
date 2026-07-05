# Feature: Dead Operator Watchdog

`FEATURE_DEAD_OP_WATCHDOG` — **ships commented out by default**; uncomment it in `keyer_2_features_and_options.h` to compile it in.

## Overview

The Dead Operator Watchdog detects a stuck paddle — a situation where the paddle contacts are shorted or jammed closed — and clears TX to prevent the transmitter from keying indefinitely.

## How It Works

The watchdog counts consecutive elements (dits and dahs) sent during manual paddle sending. If more than a threshold of elements are sent without a pause, the watchdog assumes the paddle is stuck and disables TX.

The threshold is **100 consecutive elements**. At 20 WPM, a dit is about 60 ms, so 100 elements ≈ 6 seconds of continuous keying before the watchdog fires.

## Configuration

The threshold is a hardcoded literal `100` in three places in `check_paddles()` (`k3ng_keyer_2.ino`), not a named `#define` in `keyer_settings.h`. There is currently no single setting to adjust — changing it means editing all three occurrences in the source.

## Squeeze Detection

The watchdog also detects a stuck **squeeze** (both paddles simultaneously closed indefinitely), which would produce an endless dit-dah-dit-dah alternating pattern. This counts toward the element threshold.

## What Happens When the Watchdog Fires

The watchdog does exactly one thing: it sets `tx_ptt.cw_tx_enabled = 0`, which suppresses further key/PTT transitions (equivalent to sidetone-only practice mode). It does **not** explicitly release PTT, clear the send buffers, or print a serial message — if TX happened to already be asserted HIGH when the watchdog fires, the normal key-release path (which is itself gated on `cw_tx_enabled`) can no longer run, so the key line may stay stuck high until manually cleared. Worth being aware of if you're relying on this for unattended operation.

## When This Matters

Without this feature, a stuck paddle on an unattended beacon station or a paddle that gets something dropped on it could key the transmitter indefinitely — potentially causing interference, violating regulations, or overheating the PA.

## Watchdog vs. Beacon Mode

The watchdog monitors **manual** (paddle) sending only. Automatic memory playback does not increment the watchdog counter.
