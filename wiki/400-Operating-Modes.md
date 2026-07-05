# Operating Modes

## Iambic A

Standard Curtis Iambic A mode. When both paddles are squeezed, the keyer alternates dits and dahs. The last element sent while both paddles are held is **not** repeated after one paddle is released.

Set with `\A` or command mode `A`.

## Iambic B

Curtis Iambic B mode. Same as Iambic A, except that if the opposite paddle is pressed during the last element of a squeeze, that element is queued and sent once — even if the paddle is released before the element starts. Many operators find this more natural.

Set with `\B` or command mode `B`.

## Bug Mode

Emulates a semi-automatic mechanical bug key. The dit paddle produces a stream of automatic dits; the dah paddle must be held manually for each dah.

Set with `\G` (G for "bug").

## Straight Key Mode

Both paddle inputs are treated as a single straight key input. Key down = TX on; key up = TX off. No automatic element timing.

**Currently unreachable via CLI or command mode:** the `STRAIGHT` keyer mode exists in the code (`check_paddles()` handles it, and `\S` will print "Straight" if it's ever active) but nothing in the current build actually sets `configuration.keyer_mode = STRAIGHT` — there is no CLI command or command-mode option that selects it.

For straight-key operation today, use a dedicated straight key on its own pin instead — see `FEATURE_STRAIGHT_KEY` in [[Configuring the Software|210-Configuring-the-Software]] (ships commented out by default; independent of the paddle-based `STRAIGHT` mode above).

## Paddle Reverse

Swaps the dit and dah paddles. Useful if you prefer to hold your paddle the other way, or if you've wired the paddles backwards.

Toggle with `\N` or command mode `N`.

---

## CMOS Super Keyer Iambic B Timing

An enhancement to Iambic B timing. The keyer starts latching the opposing paddle after a configurable percentage of the current element has elapsed, rather than waiting until the element ends. This allows slightly earlier triggering of the alternating element and can feel more responsive at high speeds.

Enable/disable with `\&`. Set the timing percentage with `\%#` — despite the two-digit-looking notation, the CLI currently only accepts a **single digit (0–9)**; two-digit values like the compiled-in default of 33% cannot be entered this way once changed.

Requires `FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING` (active by default).

---

## Dynamic Dah/Dit Ratio

By default the dah/dit ratio is fixed at 3:1 (300). With dynamic ratio enabled, the keyer automatically adjusts the ratio based on WPM to match the feel of mechanical keyers at various speeds.

- `\J###` sets a fixed ratio (151–809, where 300 = 3:1 — bounds are exclusive, so exactly 150 or 810 are rejected)
- `\^` toggles dynamic auto-adjustment on/off

`\J` works regardless of this flag, since the underlying config field and CW timing engine are unconditional. `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO` (**ships commented out by default**) only gates the `\^` auto-adjust toggle and its WPM-based recalculation.

---

## Switching Modes

All mode changes are saved to EEPROM automatically after 30 seconds (or immediately with `\$`).
