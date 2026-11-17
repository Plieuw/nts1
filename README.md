# NTS-1 units

Six custom units for the **KORG Nu:Tekt NTS-1 (mkI)**, built with the
[logue SDK](https://github.com/korginc/logue-sdk) v1.1-0. One noise oscillator,
three modulation effects, an endless barberpole phaser in the delay slot and a
body/IR convolution in the reverb slot — a unit for every user slot the
instrument has.

| Unit | Slot | What it does | File |
| --- | --- | --- | --- |
| `noise` | OSC | Eight noise algorithms, from white to tuned resonance | `noise.ntkdigunit` |
| `lpg` | MOD | Vactrol style low pass gate driven by an envelope follower | `lpg.ntkdigunit` |
| `reso` | MOD | Twelve mode resonator with beating pairs, Rings in spirit | `reso.ntkdigunit` |
| `elements` | MOD | Geometry morphing resonator: string, membrane, plate, bell | `elements.ntkdigunit` |
| `barber` | DELAY | Barberpole phaser that rises or falls forever | `barber.ntkdigunit` |
| `irbody` | REVERB | Convolution with modelled instrument bodies and spaces | `irbody.ntkdigunit` |

The NTS-1 has one slot per type, so `lpg`, `reso` and `elements` are
alternatives for each other. `barber` and `irbody` are alternatives too, for a
reason that is easy to miss: on the mkI a custom **delay** and a custom
**reverb** are loaded into the same runtime, so only one of the two can be a
custom effect at a time (see [Stacking the units](#stacking-the-units)).

Note that there is no user slot for an envelope generator on the NTS-1: the MOD
slot only accepts (audio) modulation effects. `lpg` is the way to get an
EG-like behaviour out of that slot.

### Where each unit sits

The NTS-1 chain is fixed: oscillator, filter, amp/EG, then the three effect
slots — modulation, delay, reverb, in that order. Every unit is in the slot
where that chain does the most for it.

| Slot | Comes after | Unit | Why there |
| --- | --- | --- | --- |
| OSC | — | `noise` | the only slot that makes sound |
| MOD | amp/EG | `lpg` | first slot after the amp envelope, so the follower hears the shape of the note, which is exactly what a vactrol gate wants. The NTS-1 has no user filter or EG slot |
| MOD | amp/EG | `reso`, `elements` | they turn the oscillator into an instrument, so everything time based should come after them |
| DELAY | mod | `barber` | a phaser belongs before the reverb, and it needs the dry/wet knob that only a delay or reverb unit gets; a modulation effect has no shift-depth parameter |
| REVERB | delay | `irbody` | a space belongs at the end of the chain, and a reverb unit gets the same 12 KB of code, 2.4 MB of SDRAM and shift-depth mix as a delay unit |

## The units

### noise — oscillator

Noise is the sound source everything else in this collection likes to chew on.

| Control | Function |
| --- | --- |
| A (SHAPE) | character of the selected algorithm |
| B (SHIFT) | spectral tilt: dark ↔ bright |
| Edit 1 | `Algo`: white, pink, brown, velvet, crackle, step, tuned, hybrid |
| Edit 2 | `Tune`: ± 24 semitones |
| Edit 3 | `Keytrk`: how much the algorithm follows the played note |
| Edit 4 | `Grain`: grain length for the impulse based algorithms |
| Edit 5 | `Level` |
| Edit 6 | `Trans`: transient shape, one continuous sweep from mallet to knock |

What the A-knob does depends on the algorithm: velvet sets the impulse density,
crackle the event rate, step the sample & hold rate as a multiple of the note,
tuned the resonance of the tracking band-pass, and hybrid mixes the band-pass
with a sine at the same pitch. White, pink and brown stay close to their usual
character — the tilt is on the B-knob.

`Trans` puts a transient on every note-on, and the knob sweeps continuously
through five of them. They are the five points of one morph — 0 % mallet, 25 %
stick, 50 % pluck, 75 % ping, 100 % knock — and everywhere in between the two
neighbouring shapes are mixed coefficient by coefficient: a quarter turn between
mallet and stick is a half-soft head rather than a switch, and no position is
silent. The shapes follow the objects the other units model, so the noise can
excite them the way they are actually hit — and a fast amp EG on the NTS-1 makes
the attack stand out.

| `Trans` | Shape | Sounds like | Pairs with |
| --- | --- | --- | --- |
| 0 % | mallet | soft head, no high end, 14 ms thump | `elements` membrane/plate, `irbody` piano and guitar |
| 25 % | stick | hard full-band click, gone in 3 ms | marimba, `irbody` wood and steel |
| 50 % | pluck | bright edge over a woody body, 28 ms | `elements` string end, `irbody` violin and cello |
| 75 % | ping | narrow ring tuned to the note, 140 ms | `elements` bell end, `irbody` plate |
| 100 % | knock | knuckle on wood, 5 kHz and down, slow | `irbody` room, or a low thump under anything |

The five shapes were measured to land at the same attack level, and the level
between them is trimmed against a measured curve, so the sweep stays within about
1 dB from end to end: the knob changes the shape and not the volume. The attack
peaks 1.7 to 2.5 times the steady level of the noise. The transient itself cannot
be dialled away any more — every position of the knob is a shape — so for a dry,
dusty sound use velvet or crackle, which carry their own transient at grain
level.

### lpg — modulation effect (low pass gate)

A vactrol style gate: whatever arrives at the MOD slot opens both a VCA and a
low pass filter, and lets them fall back slowly and non-linearly. Play short
notes and it plucks; hold a chord and it breathes.

| Control | Function |
| --- | --- |
| A (TIME) | release time, 4 ms .. 2 s |
| B (DEPTH) | how far the gate closes (0 nearly open, 1 full gate) |

The follower is normalised with a slow peak tracker, so quiet and loud sources
open the gate the same way.

### reso — modulation effect (modal resonator)

Twelve tuned resonators in near pairs, so the modes beat against each other.
Feed it clicks, noise or plucks and it becomes a struck bar, a bell or a body;
feed it sustained sound and it turns into a tuned drone.

| Control | Function |
| --- | --- |
| A (TIME) | fundamental, 40 Hz .. 2 kHz |
| B (DEPTH) | ring time, 0.1 s .. 5 s |

### elements — modulation effect (geometry morphing resonator)

Where `reso` keeps one fixed mode set, this one moves the modes themselves. The
B-knob morphs the geometry: a stiff string first (harmonic series with the
inharmonic stretch a real string has), then the Bessel ratios of a circular
membrane, then the modes of a flat plate, and finally the inharmonic series of a
bell. Decay, damping and the colour of the exciter follow along, so a string is
short and bright and a bell rings long and dark — because that is what those
objects do.

It is an independent implementation inspired by the resonators of Mutable
Instruments Elements, not a port: a modulation effect on the NTS-1 gets two
knobs and no note information, so the pitch is set by hand here.

| Control | Function |
| --- | --- |
| A (TIME) | fundamental, 40 Hz .. 2 kHz |
| B (DEPTH) | geometry: string → membrane → plate → bell |

### barber — delay effect (barberpole phaser)

Eight all-pass sections sweep 60 Hz .. 9.6 kHz, and a second cascade half a cycle
out of phase takes over where the first wraps around, crossfaded with a raised
cosine. The result never jumps, so it rises (or falls) forever. Feedback sharpens
the notches into the classic jet sound.

| Control | Function |
| --- | --- |
| A (TIME) | rate and direction: left of centre falls, right of centre rises, further out is faster (0.02 .. 10 Hz) |
| B (DEPTH) | feedback / resonance |
| DEL + B | dry/wet — around the middle gives the deepest notches |

### irbody — reverb effect (impulse response / body)

Convolves the incoming sound with an impulse response, so the synth sits inside
a violin body, a piano soundboard, a metal plate or a small space. The responses
are modelled rather than sampled, but they are true impulse responses and the
unit convolves with them sample by sample: every partial of the source is
coloured the way a real body would colour it. It is a reverb effect, so it runs
last: whatever the modulation units and the delay have made is what it puts in
the body or the room.

The models are physical: the violin, cello and guitar responses use the air and
wood resonances of the real instruments (A0, B1-, B1+ and the bridge hill), the
wood preset the free-free bar series of a marimba bar, the steel preset a
plate-like inharmonic series with an even, long decay. On top of the modes each
response has a short broadband knock, a diffuse tail whose high frequencies die
away faster than its low ones, and — for the spaces — early reflections. The
same responses are written out as WAV files in [`ir/`](ir) by
[`tools/make_irs.cpp`](tools/make_irs.cpp), so you can look at them, drop them
in a DAW, or convolve something else with them; they are bit for bit what the
unit uses.

| Control | Function |
| --- | --- |
| A (TIME) | response: violin, cello, piano, guitar, wood, steel, room, plate |
| B (DEPTH) | damping of the wet signal, dark .. open |
| DEL + B | dry/wet |

The convolution runs in sixteen frequency domain partitions (FFT 512, hop 256),
so an 85 ms response costs a fraction of what direct convolution would need.
The input is real, so only the lower half of each spectrum is accumulated and
the upper half is filled in as its mirror image, which halves the multiply and
add work of the convolution itself.
Block processing adds 5.3 ms of latency, which the dry path is delayed by as
well so the two stay aligned. Responses end with a short fade, so the cut at
85 ms is not audible.

All eight responses are transformed once, when the unit loads — that is where
the SDRAM goes, about 600 KB for the spectra — so turning the body knob never
builds anything while audio is running. A change instead crossfades between the
old and the new response over 85 ms: both convolutions run from the same input
spectra and their outputs are mixed, which is exactly a crossfade of the two
responses and cannot click. Intermediate knob positions are skipped while a
crossfade runs, so whipping the knob around does not queue up a fade per value.

## Stacking the units

The NTS-1 chain is oscillator → filter → amp/EG → modulation → delay → reverb,
and every unit only ever sees the audio of what comes before it. Nothing here
depends on its neighbours, so a unit can sit next to the stock effects, next to
someone else's unit, or next to another one of these.

What the mkI *does* constrain is which slots can be full at the same time:

| Combination | Works | Why |
| --- | --- | --- |
| any one of `lpg`, `reso`, `elements` | yes | there is one MOD slot, so they are alternatives |
| `noise` with any of them | yes | the oscillator and the effect slots are independent |
| `barber` in DELAY with a stock reverb | yes | a stock delay and a stock reverb do not collide |
| `irbody` in REVERB with a stock delay | yes | same |
| `barber` **and** `irbody` together | **no** | on the mkI a custom delay and a custom reverb are loaded into the same runtime: the SDK notes that when both are enabled only one of them can be a custom effect, and enabling both anyway has been reported to make them malfunction. Pick one, and put a stock effect in the other slot |

So a full house is `noise`, one of the three modulators, and one of `barber` or
`irbody` — with the remaining effect slot on a stock type.

### What each unit promises its slot

The point of the list below is that you can drop any of them next to anything
without having to think about it:

* **Any block size.** Every unit was run with frame counts from 1 to 96, in
  place, without a glitch, a NaN or an overrun; none of them assumes the 32 or
  64 frames the stock host happens to send.
* **Parameters in any order.** A knob value that arrives before the unit is
  initialised is kept, and initialising twice is harmless — the state is zeroed
  and the latched values are applied, so an early program change cannot leave a
  unit half set up.
* **Nothing but audio in the audio callback.** Everything expensive happens in
  the init callback: `irbody` transforms its eight responses there, `reso` and
  `elements` build their coefficients there. Switching a program never builds
  anything while sound is running.
* **Suspend and resume leave silence.** After a resume nothing comes out of
  stale state (measured: 0.0000 from all six). A unit that is suspended loses
  its tail rather than replaying it later.
* **No non-finite samples.** Each unit was hammered with 20 000 random
  parameter jumps across the full range of every control, mid-stream: no NaN and
  no infinity in the output. `reso` and `elements` guard their mode banks — if a
  jump moves the modes so far under a ringing bank that it starts to run away,
  the bank is dropped instead of passing a NaN down the chain.
* **Bounded output.** All six end in a soft clip, so a slot cannot hand the next
  one more than full scale.
* **No maths library.** A single call into libm is enough for the linker to pull
  newlib's reentrancy struct into the image: about a kilobyte of RAM and a few
  kilobytes of code, out of the 6 or 12 KB a slot gets. Every unit does its own
  exponentials, square roots and sines with the SDK's tables or a short
  polynomial instead.
* **Tails end.** Ten seconds after the last note every tail has fallen below
  -70 dB (the longest are `reso` at its 5 s ring time and `elements` at its
  bell end, which is a very quiet but very long sustain by design).

Measured on the host (x86, gcc 10.3, minimum of five pinned runs, one 64 frame
block at 48 kHz is 1.333 ms); the numbers are a comparison between the units,
not an M4 prediction. One caveat: the SDK's lookup helpers are libm calls in the
host harness, so the units that refresh coefficients while they run (`lpg`,
`barber`) look relatively more expensive here than they are on the M4, where
those helpers are table lookups.

| Unit | Text + data | SRAM | Cost |
| --- | --- | --- | --- |
| `noise` | 5.7 KB | 0.4 KB | 15 ns/frame |
| `lpg` | 1.6 KB | 0.1 KB | 27 ns/frame |
| `reso` | 1.5 KB | 0.4 KB | 13 ns/frame |
| `elements` | 2.1 KB | 0.7 KB | 18 ns/frame |
| `barber` | 1.8 KB | 0.3 KB | 45 ns/frame |
| `irbody` | 10.7 KB | 603 KB SDRAM | 52 ns/frame |

## Installing on the NTS-1

The unit files are `*.ntkdigunit` packages. Load them with either

* [logue-cli](https://github.com/korginc/logue-sdk/tree/main/tools/logue-cli), or
* the KORG [Librarian application](https://www.korg.com/products/dj/nts_1/librarian_contents.php)
  (the "User Oscillator" / "User Effect" tabs).

After transferring, the units appear at the end of the selection list of their
slot. Custom units need NTS-1 firmware **1.02 or newer**.

## Building

```sh
./scripts/build.sh                # everything -> dist/
./scripts/build.sh noise lpg      # just these
./scripts/build.sh --setup        # only fetch the SDK and toolchain
```

Regenerating the impulse response WAV files (not needed to build, only when you
change the models in `units/irbody/src/ir_model.h`):

```sh
g++ -O2 tools/make_irs.cpp -o /tmp/make_irs && /tmp/make_irs ir
```

The script clones the logue SDK into `.sdk/` and downloads the gcc 5.4 toolchain
Korg recommends for the mkI (32 bit, so on Debian/Ubuntu you need
`libc6:i386`: `sudo dpkg --add-architecture i386 && sudo apt-get install libc6:i386`).
It also needs `make`, `git` and `zip`. Use `SDK_DIR` or `GCC_BIN_PATH` to point
at your own copies. If gcc 5.4 is unavailable the script falls back to whatever
`arm-none-eabi-gcc` it finds, with a warning: those binaries are not guaranteed
to run on the hardware.

Prefer the SDK's own layout? Copy a `units/<name>` directory into
`logue-sdk/platform/nutekt-digital/` and run `make install` there; the defaults
in the Makefile then line up.

`.github/workflows/build.yml` builds all units on every push, uploads them as an
artifact and attaches them to a GitHub release when you publish one.

## Changing the identity of a unit

`dev_id` and `prg_id` live in each `units/*/manifest.json`. If you intend to
publish these, set a unique `dev_id` for yourself (see
[developer_ids.md](https://github.com/korginc/logue-sdk/blob/main/developer_ids.md))
so your units cannot collide with someone else's.

## Limitations

* Everything here was written and verified by compiling and running the DSP on a
  host, not on hardware. Levels and ranges are measured, but not auditioned.
* The files in `dist/` here were built with gcc 10.3, because the gcc 5.4
  toolchain Korg recommends is 32 bit only. Run `./scripts/build.sh` or the
  GitHub workflow (which installs the 32 bit libs and uses 5.4) if you want
  binaries from the exact toolchain Korg ships for the mkI.
* A modfx unit only gets the two knobs (`TIME`, `DEPTH`); the NTS-1 does not
  hand modulation effects any more parameters, which is why `lpg` and `reso`
  have no extra edit page.
* `irbody` is mono in the wet path, so the tail is centred, and its responses
  are capped at 85 ms because that is what fits in the partitioned convolution.
  Longer spaces need the stock reverb after it.
* The mkI gives a modfx unit 6 KB of SRAM and a delay or reverb unit 12 KB of
  code space plus 2.4 MB of SDRAM; all units stay well inside those budgets now
  that none of them links the maths library. `irbody` uses about 600 KB of that
  SDRAM to keep all eight responses precomputed, and is the one unit that still
  carries libm (about 1 KB of RAM and 2 KB of code), because its responses are
  modelled with `sinf`, `cosf` and `expf` in a header that the WAV generator
  shares — those files are meant to be bit for bit what the unit plays.

## License

BSD 3-Clause, see [LICENSE](LICENSE). The Makefiles, linker scripts and unit
entry templates come from the logue SDK, Copyright (c) 2018, KORG INC., under
the same license.
