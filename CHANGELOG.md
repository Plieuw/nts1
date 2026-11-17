# Changelog

Each unit keeps its own version in its manifest; this file says what changed.
Older history is in the commit log.

## bowed — 1.1-0

- The body's resonator coefficients are computed when the model changes instead
  of twice per voice per sample, and with the repo's own polynomial instead of
  `cosf`. The unit file now has no external symbols at all: 3100 bytes of code
  instead of 7100, and 13 ns/frame with one voice and 39 with four, where it was
  24 and 79. Nothing audible moved; the recorded measurements are the same.

## bowed — 1.0-0

- A new oscillator: a bowed string rather than a struck one. Four voices, five
  models (string, metal, pipe, hum, glass), bow pressure on A and brightness on
  B. It sustains for as long as the key is down and falls away when it is
  released.
- Pitch verified against the note (262 Hz for a played C4), DC blocked at 12 Hz
  in the loop, four 1024-sample delay lines in the oscillator slot's memory.

## irbody — 1.2-0

- The wet signal is stereo. The tail comes from two points of the corpus and the
  modes take a short detour on the right; the channels average back to the level
  the unit had before.
- The A knob is a continuum: between two bodies the damping, brightness, level
  and size glide, so families walk into each other instead of stepping.
- Levels fitted per body, within 4 dB over the whole travel.

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
