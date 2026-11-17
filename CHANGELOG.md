# Changelog

Each unit keeps its own version in its manifest; this file says what changed.
Older history is in the commit log.

## resonator — 1.5-0

- The synth's LFO reaches the unit at last: it is added to the decay knob (the
  shape parameter), so a ringing bank breathes with the LFO.
- Levels are fitted across the keyboard, per model, not only at middle C. Every
  semitone from C1 to C7, six strikes per note; each model gets a slope and a
  curvature in octaves from middle C. Worst deviation from a model's own middle
  C: 6.5 dB before, 3.8 dB after, C1 and C7 within a decibel of the middle.

## noise — 1.2-0

- The synth's LFO reaches the unit: half of it sweeps the character knob from
  one end to the other.

## All units

- `dev_id` set to 1349081445 (`Plie`) instead of 0, which the SDK reserves for
  "invalid".
