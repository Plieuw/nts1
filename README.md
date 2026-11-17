# NTS-1 units

Five custom units for the **KORG Nu:Tekt NTS-1 (mkI)**, built with the
[logue SDK](https://github.com/korginc/logue-sdk) v1.1-0. One noise oscillator,
two modulation effects, and two delay effects — a body/IR convolution and an
endless barberpole phaser.

| Unit | Slot | What it does | File |
| --- | --- | --- | --- |
| `noise` | OSC | Eight noise algorithms, from white to tuned resonance | `noise.ntkdigunit` |
| `lpg` | MOD | Vactrol style low pass gate driven by an envelope follower | `lpg.ntkdigunit` |
| `reso` | MOD | Twelve mode resonator with beating pairs, Rings in spirit | `reso.ntkdigunit` |
| `barber` | DELAY | Barberpole phaser that rises or falls forever | `barber.ntkdigunit` |
| `irbody` | DELAY | Convolution with modelled instrument bodies and spaces | `irbody.ntkdigunit` |

The NTS-1 has one slot per type, so `lpg` and `reso` are alternatives, as are
`barber` and `irbody`. Loading them all is fine; you pick per slot.

Note that there is no user slot for an envelope generator on the NTS-1: the MOD
slot only accepts (audio) modulation effects. `lpg` is the way to get an
EG-like behaviour out of that slot.

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
| Edit 6 | `Trans`: noise transient on every note-on |

What the A-knob does depends on the algorithm: velvet sets the impulse density,
crackle the event rate, step the sample & hold rate as a multiple of the note,
tuned the resonance of the tracking band-pass, and hybrid mixes the band-pass
with a sine at the same pitch. White, pink and brown stay close to their usual
character — the tilt is on the B-knob.

`Trans` adds a short noise transient to every note-on: full band noise that is
shaped by the same tilt, decaying in about ten milliseconds. It gives the noise
a percussive, struck attack instead of a fade-in, and it is most obvious with a
fast amp EG on the NTS-1. Velvet and crackle already produce transients at grain
level, so turn `Trans` down when you use those for a dry, dusty sound.

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

### irbody — delay effect (impulse response / body)

Convolves the incoming sound with an impulse response, so the synth sits inside
a violin body, a piano soundboard, a metal plate or a small space. The responses
are modelled rather than sampled, but they are true impulse responses and the
unit convolves with them sample by sample: every partial of the source is
coloured the way a real body would colour it.

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
Block processing adds 5.3 ms of latency, which the dry path is delayed by as
well so the two stay aligned. Responses end with a short fade, so the cut at
85 ms is not audible.

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
change the models in `units/body/src/ir_model.h`):

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
* The mkI gives a modfx unit 6 KB of SRAM and a delfx unit 12 KB of code space
  plus 2.4 MB of SDRAM; all units stay well inside those budgets.

## License

BSD 3-Clause, see [LICENSE](LICENSE). The Makefiles, linker scripts and unit
entry templates come from the logue SDK, Copyright (c) 2018, KORG INC., under
the same license.
