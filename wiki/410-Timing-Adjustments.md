# Timing Adjustments

## PTT Lead Time

The delay between PTT asserting and the TX key line going active. Gives the transmitter (or linear amplifier) time to switch into transmit mode before CW starts.

Set in `keyer_settings.h`:
```cpp
#define initial_ptt_lead_time_ms  10
```

Implemented via the `PTT_LEAD_TIME_WAIT` state in the CW scheduler.

Runtime adjustment: _currently set at compile time; runtime CLI command planned._

## PTT Tail Time

How long PTT stays active after the last CW element ends. Prevents the transmitter from dropping out between characters or words.

Set in `keyer_settings.h`:
```cpp
#define initial_ptt_tail_time_ms  10
```

Typical values: 10–100 ms for SSB rigs; longer for some linears.

## Wordspace

The space between words, in dit units. Standard CW is 7 dit units. Increasing this value slows the effective word rate without changing character speed.

- Default: 7
- Range: 1–9
- CLI: `\Y#`

## Farnsworth Timing

Farnsworth timing sends characters at full speed but inserts extra space between characters and words so the overall word-per-minute rate is lower. This is useful for training — you learn to recognize characters at real speed while having more time to write them down.

Set with `\M###` (0 = disabled). The Farnsworth WPM must be lower than the current sending WPM to have any effect.

Requires `FEATURE_FARNSWORTH`.

## Dah/Dit Ratio

The ratio of dah duration to dit duration. Standard CW is 3:1.

- `\J300` sets 3:1 (default)
- `\J250` sets 2.5:1 (some operators prefer this at high speeds)
- Range: 150–810

With `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO`, the ratio can auto-adjust with WPM (`\^` to toggle).

## Sidetone Frequency

- Default: 600 Hz
- CLI: `\F####` (e.g. `\F700` for 700 Hz)
- Range: limited by Arduino `tone()` function (roughly 31–65535 Hz; practical range 200–2000 Hz)

## TX/RX Sequencer Timing

See [[TX/RX Sequencer|560-Feature-Sequencer]] for PTT-to-sequencer activation and de-activation timing.
