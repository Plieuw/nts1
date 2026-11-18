# Listening on the instrument

Everything in this repository was measured on a PC against the stand-in SDK in
`tests/host`. That is enough to fit levels, to check that a knob moves something
and to catch a NaN, and it is not enough to know how a unit sounds or what it
costs on the NTS-1. This file is the list of things that only the instrument can
answer, in the order that matters.

Load one unit at a time (slot files are in `dist/`), and keep the other slots on
stock effects so a problem cannot come from a neighbour.

## 1. Does every unit fit and run cleanly?

For each of the eight units: play a chord, hold it, move every knob across its
full range, then release.

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

The two units to watch are `bowed` with four notes held (16 KB of delay memory
and four voices) and `irbody` while the A knob is being turned (a body change
runs an 85 ms crossfade from the other engine slot).

## 2. Does the crossfade and preset switching survive?

* `irbody`: hold a chord, turn A slowly from one end to the other and back. The
  body should walk, not jump, and the level should stay put (the host fit holds
  it within 4 dB).
* `resonator`: change `Model` while a note rings. Every voice is re-landed onto
  the new bank; the note should keep its pitch and change object under it.
* `barber`: turn the rate knob while a chord rings, and stop the LFO by holding
  it at one end. No zipper, no pitch shift of the dry signal.

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

* `irbody`: does the stereo body survive the mono sum the NTS-1 makes on its own
  output? It should; the two channels average back to the old level. Play a note
  and let go: the body must be gone with the note — the unit gates its own output
  20 ms after the input goes quiet, so a tail that stays is not the unit.
* `lpg`: the gate should close on the *note*, not on the oscillator's waveform.
  Play a slow attack and a staccato note.
* `elements` and `reso`: full wet, and check that a high note does not thin out.
  On `elements`, turn DEPTH up while a note rings: the ring should follow the
  knob and then end with the note, not hang under the next one.
* `barber`: at the highest feedback setting it should still not run away.

## What to write down

For each failure: unit, knob position, note, and whether it survived a reload.
The host harness can reproduce anything that is a number (level, spectrum, NaN);
it cannot reproduce a click that only happens when a buffer is cut short by the
instrument's own scheduling.
