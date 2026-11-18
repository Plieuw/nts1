# Changelog

Each unit keeps its own version in its manifest; this file says what changed.
Older history is in the commit log.

## irbody — two lines

The pass before this one capped every body at a guitar and took the network from
four delay lines to three, which was 5%: still the most expensive unit here. The
lines that were left to cut are the third comb and the five millisecond allpass
on the tail, and the tail is a few per cent of the wet signal — what a body is
comes from its modes, and those are untouched.

- Two comb lines, and the allpass on the tail is gone: the tail now goes through
the 2 ms one and the side through its own. Per sample the two lines are read
four times between them, where the network used to read nine.
- Host, five alternating passes against the previous build in one session,
  minimum: 39.7 → 36.3 ns/frame at the knob's default body (-9%), 41.4 → 37.6 at
  the piano (-9%). The idle path is untouched at 2.1 ns.
- Counted off the image with `tools/cycles.py`, with every loop body at its own
  iteration count: the per-sample engine went 701 → 573 cycles on the M4 for an
  eleven-mode body (-18%), and 601 → 473 for the drum at seven.
- What it does to the sound: rendered through the piano body, a plucked tone
  differs from the previous build by -32 dB, and all of that is in the first
  150 ms — the tail. Third-octave levels stay within 1.5 dB, the stereo
  correlation is 0.98 as before, and the -40 dB point is the same 150 ms. The
  body's modes, which carry the identity of the object, do not move at all.
- SDRAM 11 → 5 KB and SRAM 4.3 → 2.3 KB, because the lines that are gone took
  their buffers with them.

The goldens moved, in the tail only.

Versions: `irbody` 1.8-0.

## irbody — three lines and a guitar

This was the most expensive unit here, and part of the reason was what the knob
was allowed to be: a sweep from a violin to a piano soundboard and a steel plate
is not a body, it is a small room, and the fourth delay line plus the nine
millisecond allpass on the side signal were paid for on every sample.

- No body is larger than a guitar. The comb scale and both decay constants are
  capped at the guitar's, so a piano or a plate keeps its own modes, colour and
  damping but the size of the object it happens in no longer grows past a body
  you could hold. The longest comb line is 9.3 ms now, the tail 8 to 22 ms.
- Three comb lines instead of four, and the long allpass on the side signal is
  gone.
- Host, five alternating passes against the previous build in one session,
  minimum: 42.4 → 40.2 ns/frame playing (-5%), 2.1 → 1.8 idle. Counted off the
  image for the M4, the per-sample engine is 340 → 321 cycles.
- Memory: the combs are as long as the largest body needs and no longer, so the
  SDRAM went 52 → 11 KB. The engines and the short lines moved from SDRAM to
  SRAM (4.3 KB); the host cost of that is nil, it is about where a sample waits
  on the instrument.
- The level table was fitted against the old sizes; with the sizes capped the
  bodies sat 5.5 dB apart. Measured per preset again, they are 4.2 dB apart.

The goldens moved, which is the point of this change: the sound is what was
altered.

Versions: `irbody` 1.7-0.

## manifests — the versions the notes named

Four manifests were behind the notes, for one reason: their commits bumped the
`api` line instead of the `version` line. `api` is the SDK a unit is built
against, not the unit's own version, so nothing on the instrument changed — but
the unit announced a version it had never been given, and the notes for those
releases had been written from the `api` number, which is why the two looked
consistent.

- `barber` 1.1-0 → 1.3-0. Its `api` line had walked 1.1-0 → 1.2-0 → 1.3-0, one
  bump per release.
- `lpg` and `reso` 1.1-0 → 1.2-0, with `api` 1.1-0 → 1.2-0 in the cost pass.
- `elements` 1.1-0 → 1.4-0, with `api` 1.1-0 → 1.3-0 over two passes.

`api` is 1.1-0 in all of them again, which is the SDK they are built against.
Nothing in the code moved: the binaries are the same, only their metadata says
what it should.

## elements — one bank

The instrument is mono, so `elements` was rendering two banks of sixteen modes
where it could only ever be heard through one. The slot is handed a main and a
sub buffer; the NTS-1 has one oscillator, and the SDK says a platform without a
sub timbre may ignore the sub buffers. A host that still arrives with a separate
sub output now gets the same samples there.

- One bank, and it steps two samples at a time: a mode's coefficients and its two
  states are used for the pair instead of being read and written once per sample.
  Every value is the same float, so a single note measures and sounds exactly as
  before — the goldens did not move.
- The four ratio sets (four times sixteen floats) are gone; the geometry is
  worked out from the tables when the knob moves.
- Host cost, five alternating passes against the previous build in one session,
  minimum: 30.2 → 16.3 ns per frame playing, 2.5 → 1.6 idle.
- Size: 3604 → 3032 bytes of the slot's 6 KB, of which the SRAM is 752 → 356
  bytes. The CI's gcc 5.4 build, which is what ships: 3360 → 2848 bytes.

Versions: `elements` 1.4-0, and see the entry above for why its manifest had said
1.1-0.

## all three oscillators — mono

The NTS-1 is monophonic: one note at a time, whether it comes from the keys, the
arpeggiator or the sequencer. `bowed` and `resonator` kept four voices each —
16 KB of delay loops and 1.5 KB of banks in slots that may never hold a second
note — and walked all four of them every frame. Both play one voice now.

- A new note strikes the same voice: `bowed` resets its loop and bows it,
  `resonator` re-lands its bank over the ringing one. The voice allocator, the
  age counter it stole by and the mix that tucked extra notes under the first
  are gone.
- A single note sounds exactly as it did, sample for sample: the mix keeps the
  quarter it was normalised by when four voices could sum, and `tests/run.sh`
  passes against untouched goldens. `bowed` measures 14.1 → 10.2 ns per frame
  playing and 7.3 → 1.6 idle; `resonator` 31.9 → 27.0 and 1.4 → 0.4 (host, five
  alternating passes, minimum, the four voice build run in the same session).
- `bowed` holds one delay loop of 1024 samples (4 KB) instead of four (16 KB), and
  `resonator` one bank of modes (0.4 KB of SRAM instead of 1.5).
- `noise` was already a single generator and is unchanged.

The ARM build prices `bowed`'s audio loop at 216 cycles per frame, 230 with the
`bowNoise` call it makes per sample. The four voice version measured 777 for one
active voice and three idle ones: 6% of a 1.333 ms block on the 180 MHz core now,
a fifth before. The two counts come from different gcc versions (5.4 then, 10.3
here), which does not change the direction of it.

Versions: `bowed` 1.5-0, `resonator` 1.9-0. `noise` keeps its.

## all eight units — cost

Four slots run at once, so their cost adds up: one oscillator, one modulation
effect, the delay and the reverb. Measured on the host with `tools/bench.cpp`
(the same workload in every unit, the previous build run alternately in the same
session, minimum of three passes), one slot of each type together went from 129
to 105 ns per frame while playing and from 103 to 9.9 ns per frame with the
instrument idle.

- `elements` and `reso` compute both timbres in one pass over the modes: the
  coefficients are shared by the two channels and were loaded twice, now once.
  -23% and -24% while playing.
- `resonator` skips the per-mode drive coefficient once the strike is over, at
  -10%. The excitation term is exactly zero after it, so both paths agree sample
  for sample.
- Every effect slot skips a block that is digital silence in with silence inside
  — -88% to -94% of an idle slot. Every term of the sum is zero there, so the
  output is written and the DSP is not run; `barber` and `irbody` also stop
  advancing their own state, which is 20 ms behind and below -180 dB by then.
- `barber` steps its two cascades in one pass (same arithmetic, two chains for
  the core to interleave), and `lpg` gets the per-channel skip.
- `noise` and `bowed` are unchanged: they were the cheap ones, and a noise source
  has to render every frame.

One golden moved on purpose: `barber`'s `bands_tail`, from -187 dB of residue to
exact silence, because the block is now skipped rather than processed to nothing.

Versions: `resonator` 1.8-0, `lpg` 1.2-0, `reso` 1.2-0, `elements` 1.3-0,
`barber` 1.3-0, `irbody` 1.6-0. `noise` and `bowed` keep theirs.

## all eight units — start values

A newly installed unit sounded like nothing on the instrument until a knob was
moved. The synth hands a unit the parameter values stored in the program, and a
program that has never held that unit sends zero for every one of them — zero is
a level of zero, so the unit opened its own output and was silent. `noise` and
`bowed` were reported exactly like that on the NTS-1, and `resonator` only
sounded after its Level had been turned by hand. Nothing was wrong with the DSP:
rendered on the host, all three are loud at those settings once the level is not
zero.

So a unit now keeps what its `*_INIT` set until the program sends a parameter
value that is not zero. From the first such value on the patch is in charge and
zeros count, so a knob turned all the way down still means what it says. The A
and B knobs are latched on their own flag: on `bowed` A is the bow pressure and
zero there is close to silence, so an A position that was never chosen cannot
pull the level down with it, and an A that is chosen takes over without touching
the edit parameters.

Versions: `noise` 1.3-0, `resonator` 1.7-0, `bowed` 1.4-0, `lpg` 1.1-0, `reso`
1.1-0, `elements` 1.2-0, `barber` 1.2-0, `irbody` 1.5-0.

- The harness measures it: `boot_rms` for an oscillator and `boot_db` for an
effect, taken with every parameter at zero, last in `main()` so the random
stream the other figures draw from does not move. `tests/run.sh` fails below
0.01 RMS (an oscillator has to be heard) and below -25 dB for an effect (it has
to change the signal rather than pass it through). Building `noise` without the
latch measures 0.0000 there and the suite rejects it, which is the bug the
instrument showed.

## barber — 1.1-0

- The number of notches is a control now, on the shifted DEPTH knob: one to
  eight, one per all-pass section. It used to be fixed at eight. A section that
  comes back into the sweep is emptied first, so the count can be turned while a
  chord rings without a click from a filter state that no longer belongs there.
- The dry/wet mix does not move any more: it sits at the middle, which is where
  the notches are deepest. With the whole cascade on the wet side the unit is a
  phase sweep and nothing else, and at the dry end there is no phaser at all, so
  of the three controls this slot has the count was the one worth adding.
- The harness prints `notch_db` for the delay unit: the level of the difference
  between one notch and eight, with the sweep stopped, against the signal itself.
  It measures -28 dB, and `tests/run.sh` fails below -45, because a knob that
  does nothing measures -300. This is what a third octave band cannot see: an
  all-pass cascade moves phase, not band energy.

## irbody — 1.4-0

- The unit is silent 40 ms after the input goes quiet, so it cannot leave a note
  sounding. A body's own tail is 8 to 35 ms — the mode bank, the four combs (all
  capped at 0.93) and the allpasses are gone well inside that — so anything still
  audible after it is state being recited, not a body. Twenty milliseconds of
  true digital silence now closes a gate over 10 ms, and it opens again the same
  way; a held note or a release keeps it open, because the input is only exactly
  zero once the amp EG of the synth has already taken the note down. Measured on
  the host, the output was already at -334 dB ten seconds in, in every knob
  position, so this changes nothing that was there; it guarantees the unit
  cannot be the thing that hangs.
- A bank that has gone non-finite is dropped instead of passed on: if an engine's
  output leaves ±100 it is cleared, so a NaN in a feedback line cannot come back
  with the next note.
- The harness now measures channel width just after the input stops (a 1024
  sample window) instead of over the tail: a unit that gates itself has no tail
  to measure, and the width was being read off the decayed residue.

## elements — 1.1-0

- DEPTH no longer leaves a note hanging. The ring time of the bank was
  `0.35 s * 2^(3g)` with `g` the knob from string (0) to bell (3), so the bell end
  reached 179 seconds: not a bell but a note with no end in sight, and since a
  modulation effect sits after the amp EG of the synth, nothing downstream would
  ever cut it. Turning DEPTH up under a ringing note left it sounding. The bell
  end is now 2 s (`0.35 s * 2^(0.84g)`), the membrane 0.63 s and the plate 1.1 s;
  a held note still fills the bank, so the pad at the top of the range stays.
- A shorter ring also makes a bank louder, since a mode stores about (1-r^2) of
  what is fed into it: on its own the new decay time raised the level 19 dB. The
  input gain now scales with the decay time (with 7.92 s, the old ring time in
  the middle of the range, as the reference), so the loudness stays where it was
  and DEPTH is a knob about the tail and nothing else.
- The harness prints `tail_db` and `tail_db_top` for the effect units: the level
  over the last second of a ten second render with the input gone after a quarter
  of a second, relative to the level while the input was there, with the knobs in
  the middle and with them at the top. `tests/run.sh` fails above -60 and -12 dB;
  the top limit is the looser one because `reso` rings for 5 s by design and
  measures -16 dB there. The old `elements` measured -3.3 and +1.4 dB; it now
  measures -85.5 and -36.5 dB.
- The ratios, the morph and the exciter are untouched. Only the decay.

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
