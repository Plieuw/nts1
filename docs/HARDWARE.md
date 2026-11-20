# Listening on the instrument

Everything in this repository was measured on a PC against the stand-in SDK in
`tests/host`. That is enough to fit levels, to check that a knob moves something
and to catch a NaN, and it is not enough to know how a unit sounds or what it
costs on the NTS-1. This file is the list of things that only the instrument can
answer, in the order that matters.

Load one unit at a time (slot files are in `dist/`), and keep the other slots on
stock effects so a problem cannot come from a neighbour.

## 1. Does every unit fit and run cleanly?

For each of the six units: play a note, hold it, move every knob across its full
range, then release. Before touching anything, hold one note with the unit
just installed: it has to sound at once, on its own start values. A fresh
program sends zero for every parameter, and zero is silence — a unit that is
quiet until a knob is moved is the start-value bug this repository already had,
not a quiet patch.

| Check | What a failure sounds like |
| --- | --- |
| No dropouts, clicks or gated noise | a unit that is too expensive for its slot stutters instead of failing quietly |
| No thump when a note starts or ends | DC or an uninitialised buffer reaching the output |
| Parameter changes are smooth | a `zip` per knob movement means a coefficient is being recomputed per block without smoothing |
| The last note does not hang | a tail that never ends is a filter that is not losing energy |

Release a key with the synth's Release turned up: the note must follow the amp EG
and not ring on under it. `resonator` with A fully open is the test — a bowl or a
gong holds more than half a minute of energy at that setting, so a unit that keeps
a tail of its own is heard at once as a note that outlives the key.

The units to watch are `bowed` while a note is held with the bow hard down (a 4
KB delay loop) and `reflect`, which is four delay lines plus two allpasses in the
delay slot and is the most expensive unit here on the host: 39 ns a frame.

## 2. Does the crossfade and preset switching survive?

* `resonator`: change `Model` while a note rings. The voice is re-landed onto the
  new bank; the note should keep its pitch and change object under it.
* `reso` and `wavefolder`: turn the knobs under a held note. Both ramp their
  parameters over 20 ms, so the change should slope rather than step.
* `reflect`: turn A (size) under a held note. The lines glide their read
  distance, so the pitch should swoop like a tape delay for the 30 ms the glide
  takes, not click. Turn B (decay) with a note ringing: the tail should get
  longer or shorter at about the same level, not louder and quieter. Release the
  key: the tail must follow the amp EG's release and end with it.

## 3. Are the levels right in the room?

The host fit normalises the models at middle C, and the keyboard fit corrects
each model across C1..C7 (worst case 3.8 dB, mostly mode interference). On the
instrument, check three things the host cannot:

* a low C and a high C on the same model sound like the same instrument,
* switching `Model` does not jump in volume, and
* `resonator` at the same `Level` sits next to `bowed` at the same `Level`.

## 4. The LFO

Both oscillators read the synth's LFO to shape. Route the LFO to shape, set a
slow rate, and check that `noise`'s character and `resonator`'s decay actually
move. This is the one API feature that was never heard on the instrument: on the
host it lands the same value as the equivalent knob position, within 0.1 dB, but
how fast and how deep it feels is a judgement.

## 5. The effects in context

* `reso`: full wet, and check that a high note does not thin out. Turn DEPTH up
  while a note rings: the ring should follow the knob and then end with the note,
  not hang under the next one.
* `wavefolder`: a held note through it should get brighter and more complex as
  FOLD goes up, at about the same level (the host fit is within 1 dB of the dry
  signal on a full scale pluck). A single coil-like buzz at the top is the
  five-fold cap doing its job; a drop in level is not.
* `reflect`: a short pluck should come back as a handful of separate
  reflections first and a short tail after them, not as one slurred echo. With
  A low the room should sound like a box a metre across — a colour on the note —
  and with A high like a small room. One thing to listen for that the host could
  not settle: with DECAY near 1 and the note held, the room should thicken the
  note rather than run away; the feedback is capped at 0.98 and the level of the
  wet path shrinks as it is turned up, so a build-up that keeps growing is a bug,
  not the top of the knob.
* All six at once (one oscillator, one shaper, `reflect`, stock reverb): with
  `reflect` in the delay slot the reverb is the instrument's own, which is the
  combination that was not possible while the body model lived there.

## What to write down

For each failure: unit, knob position, note, and whether it survived a reload.
The host harness can reproduce anything that is a number (level, spectrum, NaN);
it cannot reproduce a click that only happens when a buffer is cut short by the
instrument's own scheduling.
