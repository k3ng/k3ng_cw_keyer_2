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

Set with command mode straight key selection. A dedicated straight key pin can also be configured independently — see [[Straight Key Feature|590-Feature-Straight-Key]].

## Paddle Reverse

Swaps the dit and dah paddles. Useful if you prefer to hold your paddle the other way, or if you've wired the paddles backwards.

Toggle with `\N` or command mode `N`.

---

## CMOS Super Keyer Iambic B Timing

An enhancement to Iambic B timing. The keyer starts latching the opposing paddle after a configurable percentage of the current element has elapsed, rather than waiting until the element ends. This allows slightly earlier triggering of the alternating element and can feel more responsive at high speeds.

Enable/disable with `\&`. Set the timing percentage with `\%##` (0–99, default varies).

Requires `FEATURE_CMOS_SUPER_KEYER_IAMBIC_B_TIMING`.

---

## Dynamic Dah/Dit Ratio

By default the dah/dit ratio is fixed at 3:1 (300). With dynamic ratio enabled, the keyer automatically adjusts the ratio based on WPM to match the feel of mechanical keyers at various speeds.

- `\J###` sets a fixed ratio (150–810, where 300 = 3:1)
- `\^` toggles dynamic auto-adjustment on/off

Requires `FEATURE_DYNAMIC_DAH_TO_DIT_RATIO`.

---

## Switching Modes

All mode changes are saved to EEPROM automatically after 30 seconds (or immediately with `\$`).
