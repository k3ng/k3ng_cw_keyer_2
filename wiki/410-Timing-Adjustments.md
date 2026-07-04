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

## Autospace

Autospace automatically inserts a letterspace pause after each manually-keyed dit or dah when the operator pauses — that is, when neither paddle is pressed after the element finishes. This cleans up sending by ensuring clean character boundaries even when the operator's timing is slightly loose.

Requires `FEATURE_AUTOSPACE`.

**How it works:** After a dit or dah completes its normal 1-dit inter-element space, the scheduler checks whether another element has been queued. If not, it waits an additional `autospace_timing_factor` dits before advancing. At the default of 2.0, total inter-character space becomes 3 dits — the standard CW letterspace.

If the operator presses the next paddle during the autospace window, the extra wait is skipped and sending continues normally. There is no effect on automatic sending (memories, serial keyboard).

| Setting | Value |
|---------|-------|
| Default | Off |
| CLI toggle | `\z` |
| CLI factor | `\Z###` (integer × 100; e.g. `\Z200` = 2.0 dits) |
| Default factor | 200 (2.0 dits extra) |
| Factor range | 10–999 |

`keyer_settings.h` default:
```cpp
#define default_autospace_timing_factor  200   // 2.0 dit units extra
```

## TX/RX Sequencer Timing

See [[TX/RX Sequencer|560-Feature-Sequencer]] for PTT-to-sequencer activation and de-activation timing.
