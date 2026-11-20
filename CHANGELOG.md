# Changelog

Each unit keeps its own version in its manifest; this file says what changed.
Older history is in the commit log.

## five units, and nothing else

- The repository holds five units from v0.4.18-beta on: `noise`, `resonator`,
  `bowed`, `reso` and `wavefolder`. `lpg`, `elements`, `barber`, `irbody` and the
  delay build of `irbody` are gone from the tree — not deprecated, removed, with
  their tests, goldens and documentation. They are in the history if they are
  ever wanted back.
- What that costs: the delay and reverb slots are the instrument's own effects
  again, and the MK's "a custom delay and a custom reverb share one runtime"
  constraint no longer applies to anything here. `packs/live.txt` is the whole
  build now, so a release is simply what the workflow attaches.
- The intro of a unit is what makes this possible: every one of the five is
  either an oscillator or an audio shaper, and none of them needs a knob the mkI
  does not give its slot.

## a wavefolder in the mod slot

- `wavefolder` 1.0-0 — a new modulation effect: a five fold wavefolder. A is the
drive into the fold, 1x to 8x, so full scale material is folded five times at
the top of the knob and rides the cap past that; B morphs the curve from a
triangle fold (piecewise straight, a corner at every reflection) to a sine fold
(round corners). Both knobs move as 20 ms slopes spread over the samples of the
block, the unit keeps no state at all, and a silent block is skipped exactly.
- The drive is gain as well as fold, so the level is compensated: 1.7 dB down
throughout plus 1.0 to 1.9 that saturates with the drive. A plain 1/drive is
wrong here — a folded signal is bounded, so its RMS stops growing once the input
is folded — and fitting it at the harness's −30 dB source put the middle 3.3 dB
down and the top 9.6 dB down on material at full scale. Measured with
`tools/levels.cpp --pluck --peak 0.9`: −0.1 dB at the clean end, −0.2 dB at the
middle and −0.1 dB at the top, the whole sweep within 0.8 dB of the dry level.
On the broadband source, which peaks 20 dB lower, it is +4.0 dB at the middle and
+12.5 dB at the top: quieter than full scale, the drive amplifies more than it
folds, and that is what a folder does. With the drive in the middle, the two ends
of the SHAPE knob land 1.8 dB apart on the 0.9 peak pluck.
- `tools/levels.cpp` grew `--peak` and `--settle` for that measurement: the first
scales the source to the level a patch reaches, the second runs the unit before
the source starts so a 20 ms knob ramp has arrived. The default source peaks at
0.1 and the pluck at 0.54, so a waveshaper measured on either is measured well
below where it is used.
- On the harness's −30 dB source the folder is mostly gain: folding starts where
the driven signal reaches full scale, so the golden records the ramps and the
curve and the folds themselves are an instrument-level thing. The unit costs
14.4 ns a frame in `tools/bench.cpp`, between `bowed` and `reso`.

## knobs that move instead of jumping

- `reso` 1.2-1 and `elements` 1.7-1 — the two knobs are ramps now, 20 ms whatever
the block size, instead of steps between one parameter message and the next. A
  sustained sound with a hand on the knob zippered; now it retunes. `reso`'s
golden is regenerated because the attack window falls inside that 20 ms: the
  steady state is unchanged, which I checked by building the same code with the
  ramp replaced by the old step and diffing the report — bit-identical. The
  level moves by 2.7%, the tail by 0.2%. `lpg` already smoothed its filter every
  16 samples.
- `resonator` 1.12-2 — a note that arrives up to 30 ms after the last key came
  up is still a slur. Some hosts send the note off first, and a string that is
  still ringing is still the same string. `bowed` and `noise` do not do this on
  purpose: a lifted bow and a re-armed noise band are new articulations, and the
  split test shows them behaving that way. `tests/legato.cpp --split` covers the
  case for the resonator.
- `resonator` 1.12-3 — the split slur retuned a voice that still thought no key
  was down: the key that started the window path had already been released, and
  the slur branch never set the held flag back, so the voice released itself and
  its own key-up was ignored. Both slur paths set it now. The split test could
  not see this before because its note-off went out with pitch 0, which matches
  no held note, so the voice was never actually released and the window path was
  never exercised; with the key-up carrying the note, the old code drops the
  slurred note to nothing on the spot (level ×0.00, no G4 to measure).
  `tests/legato.cpp` now plays the key-up after the slur and
  `tests/legato_check.py` checks that it stops the note (release ratio 0.00 on
  all three oscillators).

## a bug in the damper, a trap in the retune, and the tests that were missing

A pass over the open list: one confirmed bug, one rough edge, and the checks that
would have caught them.

- `irbody` 1.13-1 — the damper's coefficient was `1 - arc_expneg(a)`, and
  `arc_expneg` is a series for small arguments. At 18 kHz `a` is 2.36 and the
  series comes out negative, so the coefficient landed above one: from B ≈ 0.86
  up, the "damping" low pass was a filter that grows, +7.9 dB around 18 kHz at
  the top of the knob. It is `1 - 2^-a` now, which is exact over the whole range
  and costs the same. Measured with B at the top: the 8–12 kHz bands drop by 7 to
  17 dB.
- `resonator` 1.12-1 — a bend used to replace every mode coefficient in one step
  when the pitch moved more than five cents. It now retunes along a 5 ms ramp,
  the same straight line a slur uses, so the modes move instead of being
  replaced.
- The legato glide is in the tests now: `tests/legato.cpp` plays C4 and then G4
  without releasing, and `tests/legato_check.py` asserts the three things that
  make it a slur — the new note is reached, the pitch is still moving at 10 ms,
  and the level does not restart. A regression cannot pass unnoticed any more.
- `tools/bench.cpp` strikes a note every half second, so an oscillator is timed
  while it is sounding. The old numbers timed a decayed note and flattered the
  three oscillators by 1–2 ns/frame; the README quotes the new ones.
- `packs/` and `./scripts/build.sh --pack rig`: the five files for this rig,
  built with one command, so a release for that rig is reproducible instead of
  a list retyped by hand.
- Manifests: `dev_id` is 0 in all nine now. The SDK's manifest reference says
  "Developer ID, currently unused, set to 0" and its own units ship 0; the
  repository's linter had that rule backwards and now checks for 0 instead.
  `prg_id` is unique per unit (it was 6 twice, and 2 twice) — the SDK calls it
  free to set for reference.
- Denormal and idle audit: every unit with a decaying tail zeroes it (lpg,
  barber, irbody, reso, elements, resonator); `noise` and `bowed` have no tail to
  guard. Nothing to change.

## the oscillators — a slur between legato notes

The mkI has no portamento to hand to a unit: its MIDI chart has no CC 5 and there
is no glide parameter, so a custom oscillator that wants one has to make it. All
three now do. A key that arrives while the previous one is still down is a change
of fingering and not a new note: nothing is re-struck or restarted, and the pitch
walks to the new note in a straight line over 32 ms — the same time for any
interval, which is what makes it read as one note moving. A key that arrives
after the previous one is up is a new note and behaves exactly as before, which
is why the goldens did not move.

- `bowed` 1.6-0 — the bow keeps going: no re-seed of the loop, no new attack,
  only the period moves. Measured C4 → G4: 184, 173, 150, 133, 121 samples at 0,
  10, 20 and 30 ms, ending on 123 where G4 is 122.4, with the level unchanged.
- `resonator` 1.12-0 — the body is retuned, not struck again: the modes keep
  their phase and the strike is not repeated. The same measurement: 183, 166,
  146, 128, 121, ending on 122.
- `noise` 1.4-0 — the algorithms are not reset and no transient is fired; the
  tuned ones follow the pitch. Measured on the tuned algorithm: the energy at C4
  (0.0021) drops below the energy at G4 (0.0043) after the slur.
- `bowed` also follows the pitch the synth reports *within* a note — a bend, and
  the `Tune` knob — by sliding the loop period per sample rather than jumping the
  read pointer at a block edge. From C4 to G4 that is 184 to 123 samples; over a
  133 ms bend the largest sample step is 0.031 against 0.034 for the same note
  standing still, so nothing clicks. 10.19 → 10.55 ns/frame, and only when the
  pitch moves.
- The legato state is cleared on init, so a re-loaded unit's first note is a new
  note and not a glide in from the last one. That was a real bug: without it,
  `noise`'s spectrum moved 5.8 dB in the harness.

Versions: `noise` 1.4-0, `resonator` 1.12-0, `bowed` 1.6-0.

## irbody — the same body, from the delay slot

`irbody` is now built twice from one source: `irbody.ntkdigunit` for REVERB and
`irbody_delay.ntkdigunit` for DELAY. Same DSP, same bodies, same knobs; only
the module type, its linker script and the magic in the hook table differ.

- Why it exists: on the mkI a custom delay and a custom reverb share one
  runtime, so `barber` and `irbody` cannot be enabled together. With the body in
  the delay slot the reverb slot can stay on a stock reverb, which is the only
  way to have a body and a reverb at once.
- The delay build is a two-line wrapper that includes the reverb unit's source,
  so the two cannot drift; the level metrics of both builds are identical.
- One thing to know: in DELAY the body hears the sound before that stock reverb
  rather than after everything, which is the same place `barber` sits in.

Versions: `irbody` 1.13-0, `irbody_delay` 1.13-0.

## irbody — the same samples, less work

A pass over the unit against the SDK's own habits: work that was multiplied by
zero, and calls where the SDK would inline.

- The box's six reflection taps ran on every sample of every body. They now run
  only while a body that has taps is playing or being crossfaded to; seven of
  the nine bodies have none. The input history is still written every sample, so
  switching to the box mid note starts from real reflections, not from silence.
- `bankSample` and `tailSample` are `always_inline`, which is how the SDK marks
  its own hot helpers. They run once or twice per sample, so the call itself was
  a measurable part of the unit; `tools/check_budget.py` still fits the 12 KB
  reverb slot.
- Measured on the host bench: 40.2 → 27.8 ns/frame, 31% less. The goldens did
  not move a byte — the output is identical, so this is speed and nothing else.

Versions: `irbody` 1.13-0.

## irbody — a soundbox played from delays

Every body in this unit was a bank of resonances plus a diffuse tail. `box` is
the first with no modes at all: a cavity is not a set of partials, it is a train
of reflections between its walls.

- Six taps off a 5 ms history of the input — 0.79, 1.17, 1.75, 2.33, 2.90 and
  3.60 ms, alternating in sign — over the same damped tail the other bodies stand
  in. The taps are the walls, the tail is the wood.
- The reference response is 40 dB down at 17 ms where the modal bodies take 35 to
  80 ms (`ir/box.wav`): a box, not a room. The taps live in `ir_model.h`, so the
  WAV and the firmware are the same six delays.
- Levelled with the rest: −1.6 dB against a dry plucked note with the knobs at the
  middle, between the drum at −1.2 and the steel at +1.1. Walking the A knob from
  the drum into the box steps by at most 0.7 dB between neighbouring quarter
  steps, and the knob now has nine bodies, 128 counts apart with the same 3%
  hold on either side of each.
- The history buffer is 1 KB of SRAM; the taps cost six multiply-adds a sample.

Versions: `irbody` 1.12-0.

## resonator — Damp is a damper, not a second decay

`Damp` shared the job of `B`: it shortened the ring of the high modes and left
the attack as it was, which moved the RMS by 0.01 dB over the knob's whole travel
at C6 — a control nobody could hear. It is a damper now: a one pole low pass on
the voice from 24 kHz down to 500 Hz, so it mutes what the body radiates instead
of changing how long it rings.

- Measured at full damp, a struck C6 loses 7.4 dB of RMS and 7.0 dB of peak,
  where C4 loses 1.7 dB of RMS and 1.4 dB of peak: the signature of a low pass,
  not of a decay. At the default (20) the corner sits at 11 kHz and the level at
  C4 and C6 is within 0.06 dB of where it was.
- `B` owns the decay of the high modes alone now, which is why the golden moved:
  the modes no longer die faster when the knob turns, the top is filtered
  instead. The golden's level and peak are where they were (0.2229 → 0.2221 RMS,
  0.501 → 0.498 peak), and the filter's own cost is one multiply-add per sample
  (17.4 → 18.3 ns/frame on the host bench with only that line differing).
- The coefficient is `1 - 2^(-2 pi fc / fs * log2 e)`. `arc_expneg` is a series
  for small arguments: at 24 kHz it returns a value past one, which as a one
  pole coefficient is a filter that grows instead of one that damps.

Versions: `resonator` 1.11-0.

## barber — the allpass had the wrong sign

`allpassCoef` returned `(1 - t)/(1 + t)`. The first-order allpass in this unit is
`y = x1 + c(x - y1)`, whose pole sits at `-c`, so the corner is at `fc` only when
`c` is negative and close to −1; that form put the pole at −1 and mirrored the
corner to `fs/2 - fc`. Every notch of the cascade sat in the top octave and the
sweep moved nothing in the band.

- Measured across 100 Hz .. 4 kHz at a still phase: the shipped unit (eight
  sections) and 1.4-0 (sixteen) both had **zero notches** in the band, the
  response flat within 0.1 and 1.1 dB. A steady tone moved by 0.0, 0.0 and 0.2 dB
  at 500, 977 and 2000 Hz over 30 s. That, and not the depth of the sweep, is why
  the slot read as a phase drift.
- `(t - 1)/(t + 1)` fixes it: six notches between 100 Hz and 4 kHz at a still
  phase — 172, 532, 938, 1436, 2126 and 3238 Hz, 34, 25, 22, 20, 20 and 22 dB
  deep — and the same three tones now swing 58.7, 50.0 and 54.8 dB as the notches
  pass. Eight sections with the fix give two notches at 6 dB, which is the mix
  ceiling 1.4-0 removed.
- `kMakeup` 0.80 keeps the deeper notches at unity: −0.11 dB at the middle of the
  knobs, measured with a plucked note.

Versions: `barber` 1.5-0.

## barber — twice the notches, deeper notches

The barberpole had eight sections and a dry/wet mix applied twice, so its notches
were capped at 6 dB however deep the phase cancellation went. Measured on a
steady tone, the sweep moved the tone by 0.1 dB: what was left was the feedback
ringing, not a moving comb.

- **Eight to sixteen all-pass sections**, so eight notches in the audible range at
the top setting, and the shifted DEPTH control spreads one to sixteen rather than
one to eight.
- **The second dry/wet mix is gone.** Each cascade is still mixed 50/50 with the
dry signal, because that is what turns a phase sweep into notches, but that sum
is the output now instead of being mixed with the dry signal again at half
weight — a quarter of the all-pass against three quarters of the dry.
- **A wider corner**: 40 Hz .. 6.4 kHz instead of 60 Hz .. 2 kHz, so a notch
travels 7.3 octaves per cycle rather than 5.1.
- **More feedback**: 0.8 × DEPTH instead of 0.65; the soft clip in the loop keeps
it bounded, and 20 s at full depth and full rate stays finite with a peak of
0.28.
- `kMakeup` 0.74 brings the deeper notches back to unity: +1.08 dB before,
−0.03 dB now, measured with a plucked note at the middle of the knobs.

The movement measured here, 3 to 8 dB at a steady tone, came from the feedback
loop and not from the comb: the notches were still above the band, and 1.5-0 is
what put them in it — see that entry for the numbers that hold. Host time 33.8 →
47.9 ns/frame, 2.4 KB of text and 0.6 KB of SRAM of the delay slot's 12 KB. The
barber golden moved with it.

`barber` 1.4-0.

## out of the band without a step

The band that holds the four `elements` geometries met the morph with a hard
edge, and at 220 Hz that edge was a step of 7.2 dB between two neighbouring knob
counts — a click while turning, not a detent.

- Inside the band nothing changed: 0, 341, 682 and 1023 play the string, the
  membrane, the plate and the bell exactly, and ten counts either side of each do
  too.
- Outside it the geometry is now eased back into the morph over the next ten
  counts by a smoothstep, which leaves the band and re-enters the morph with zero
  slope. Over the whole B sweep the largest change between neighbouring counts is
  3.6 dB, against 7.6 dB for the hard edge; the morph alone reaches 2.5 dB per
  count at its steepest with no band at all.
- 900 of the 1024 knob positions are bit-identical to the morph without any of
  this; the other 124 are the band and its return.

`elements` 1.7-0. `irbody`'s band still has its hard edge: its worst step is 1.65
dB over the whole sweep, so it was not worth the extra counts there.

## snap to the model

Four geometries on `elements` and eight bodies on `irbody` are single points on a
knob with no detents, so landing on one by hand was luck and a hair off gave a
blend of two objects. Both now hold exactly near each point:

- `elements`: within 3% of the way between two geometries — ten counts of the B
  knob — the geometry snaps to that model, so 0, 341, 682 and 1023 are exactly
  the string, the membrane, the plate and the bell. Ten counts and not 3% of the
  knob because the step at the edge of the band is the morph the band swallows:
  over the whole B sweep these ten counts move 60 of 1024 positions, by up to
  7.2 dB at 220 Hz (a note on a mode, where the morph is steepest) and about a
  decibel elsewhere, while 31 counts would have covered a quarter of the travel
  at up to 7.7 dB.
- `irbody`: each body is exact for 3% of the A knob on either side of it, ±31
  counts instead of the ±18 the quarter step already gave, so the glide into the
  next body has not started near one. Over the whole A sweep that moves 174 of
  1024 positions, by at most 1.65 dB.
- Everywhere else both knobs are bit-identical to before: the between positions
  of the morph did not change in either unit.

Versions: `elements` 1.6-0, `irbody` 1.11-0.

## levels — the SDK's reference

A patch should not jump when the unit changes, and two of these did: `elements`
was 10 dB quieter than the note it was handed and `irbody` 17 dB louder, with
its soft clip doing the limiting on every note.

- An oscillator at `Level` 100 peaks at full scale, which is what the SDK's own
  `waves` demo does — it writes its wavetable straight out at peak 1.0. Measured
  at note 60 with the new `tools/levels.cpp`: `noise` 0.90, `bowed` 0.82,
  `resonator` 0.90. `resonator` used to peak at 0.53, because it was fitted when
  it was polyphonic and sat at a quarter of the level knob: `kOutSum` 0.008 →
  0.0144, +5.1 dB. `bowed` was already there — at 100 its mix sits in its own
  soft clip, so raising the trim only added distortion, and it did not move.
- An effect is unity at the middle of its knobs, measured with one plucked note:
  `lpg` +0.4 dB, `reso` +1.1 and `barber` +0.8 were already there; `elements`
  was 9.8 dB down (`out_gain` 6 → 18.6, mid knobs now −0.0 dB) and `irbody` was
  17.3 dB up (`kOutTrim` 0.043 on the wet path, mid knobs now +0.8 dB). The
  body's band-to-band fit is untouched: the trim is one number on the whole wet
  path.
- `tools/levels.cpp` is how it was measured and how a future change can be
  checked: the same `--ranges` as the harness, RMS and peak for an oscillator,
  in against out for an effect, and `--pluck` for a single note.

The goldens for `resonator`, `elements` and `irbody` moved by exactly that.

Versions: `resonator` 1.10-0, `elements` 1.5-0, `irbody` 1.10-0.

## irbody — one tail

The unit was down to two comb lines and still the most expensive thing here, and
on the instrument it was the moment a knob was turned that pulled it over: a
body change ran two engines, and each of them carried a tail. Every body on this
unit is the guitar's size, so the two delay lines are the same length whichever
preset is chosen; two engines can stand in one set of them.

- One tail for both engines. A crossfade now costs a second mode bank, and the
  network's settings — feedback, damping, the two tail poles, the level — are
  walked from one body's numbers to the other's once a block for the 85 ms it
  runs. The tail keeps sounding through a switch instead of being rebuilt.
- That also took the last buffer out of SDRAM. The comb lines are 2.6 KB in
  SRAM now, where a sample does not wait for them, and the whole unit is 9432 of
  its 12288 bytes with nothing in the external memory — it was 5 KB there, and
  the slot had no room for it here.
- Counted off the image with `tools/cycles.py`, with every loop body at its own
  iteration count, an eleven mode body: 715 → 654 cycles a frame standing still
  (-8.5%, and the host agrees at 36.2 → 32.3 ns), and 1327 → 1031 with a body
  change running (-22%). At 168 MHz that peak is 38% → 29% of one 48 kHz frame,
  on the slot that was accused of not fitting.
- What it does to the sound: standing still, sample for sample identical — the
  same input through the violin body hashes the same, because the arithmetic of
  the tail is untouched. While the knob moves the two builds differ by -19 dB
  (correlation 0.994), because one tail carries both bodies where two used to be
  summed.

The irbody goldens moved, in the parameters the harness sweeps — that is, in the
crossfades. `audio/ir-sweep-old.wav` and `-new.wav` are the same two second
sweep in both builds.

Versions: `irbody` 1.9-0.

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
