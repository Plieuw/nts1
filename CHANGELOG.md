# Changelog

Each unit keeps its own version in its manifest; this file says what changed.
Older history is in the commit log.

## resonator — 1.6-0

- A released key ends the note. The bank used to ring on after note-off — at the
  top of the A knob a bowl or a gong holds more than half a minute of energy —
  which put the unit under the amp EG of the NTS-1 instead of in it: 300 ms after
  the key came up, a note still measured only 2.2 dB below its held level. The
  voice now fades in about 4 ms and the EG's release knob is the tail; the same
  measurement is 159 dB down. The voice allocator takes the voice whose fade is
  furthest along first, so a fast line cannot cut one that is still fading, and
  a voice's bank is cleared when its fade is over rather than when it happens to
  ring down.
- The harness prints `release_db` for the oscillator units: the level 300 ms
  after note-off relative to the level while the key was down. `tests/run.sh`
  fails if it grows by more than 10 dB.

## bowed — 1.3-0

- When the key comes up the bow leaves at once instead of following the attack
  coefficient, so the release no longer depends on the Attack knob: at 100 %
  attack the old release took its time constant from the bow's catch time. The
  amp EG of the NTS-1 comes after the oscillator slot, so the tail is its job at
  every setting (measured 168 dB down 300 ms after note-off).

## bowed — 1.2-0

- The Bow parameter works. It and the A knob are the same control on two pages,
  and the old bookkeeping let A's value cancel whatever Bow had just set: a
  sweep of the knob was flat within 2 dB and not even monotonic. Both now write
  one pressure, and the LFO offsets that one value, so the knob has an 18 dB
  range from a whisper to a growl.
- Note-on no longer zeroes the delay line before filling it: the fill overwrites
  every sample, so the zeroing was a second pass over 4 KB for nothing. It was
  3% of the note-on, which is what a measurement says and a guess would not.
  Seeding only the first period instead (all the loop reads before it has
  written the rest) takes note-on from 1.7 to 0.4 us, but it moves the attack by
  6 dB, so it was left out: the sound is worth more than a microsecond.

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

## irbody — 1.3-0

- The unit no longer links the maths library. Its two calls into it — the level
  from a decibel figure and the geometric midpoint between two bodies — are
  `arc_dbtolevel` and `arc_sqrtf` from units/common/arcmath.h now, and the impulse
  response renderer in ir_model.h is behind IRMODEL_RENDER, since that is the WAV
  generator's code and not the unit's. On the gcc 5.4 build the instrument gets:
  4980 bytes of SRAM where there were 11656, and a 4252 byte package where there
  were 8361. `nm` on the unit file finds nothing; the recorded measurements differ
  in one band, by 0.1 dB, at -150 dB.

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
