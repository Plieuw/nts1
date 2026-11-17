| Model | Object | Character |
| --- | --- | --- |
| 0 | string | harmonic series with the inharmonic stretch of a stiff string |
| 1 | membrane | Bessel ratios of a circular membrane: drum heads |
| 2 | bell | strongly inharmonic partials, the high ones carry the character |
| 3 | kalimba | free-free tine, almost all of its energy in the first partials |
| 4 | glass | circular plate with a free edge: glass harp, cymbal |
| 5 | marimba | tuned bar (1 : 3.9 : 9.2), warm, rings a second or two |
| 6 | xylophone | undercut bar (1 : 3 : 6), brittle, all attack |
| 7 | vibraphone | bar with every partial doubled a few cents apart: slow tremolo |
| 8 | piano | stiff string with detuned unisons, hammered, long |
| 9 | steel drum | octave led pan: the octave blooms over the note |
| 10 | gong | dense inharmonic metal, beating, highs outlive the lows |
| 11 | bowl | singing bowl: a few widely spaced modes, seconds long |
| 12 | triangle | bent rod: almost nothing but high partials, very long |
| 13 | woodblock | hollow block: two modes and a knock, a fifth of a second |
| 14 | log drum | slit drum, hollow 1 : 2.75, short with a big fundamental |
| 15 | tabla | loaded head, tuned almost harmonic, short and snappy |
| 16 | reed | stopped pipe with a reed: odd harmonics only, hollow |

Seventeen, not the twenty-three it grew to: the models were compared against each
other, one sixth of an octave at a time, and the ones that were a different name
for the same bank of modes are gone. Trumpet was the harmonic series of `pipe`
with a brighter tilt; `pipe` itself was `string` for the first fifteen partials;
`plate` sat 3.1 standard deviations from `membrane` with nearly the same dense
two dimensional set; `chime` was `bell` with a harmonic start; `handpan` was
`string` below 6 kHz with a shimmer on top; and `multi`, which was meant to merge
the four basics, merged `string` with itself — it only ever took that model's own
modes. Everything left is at least 4.4 standard deviations of its spectral
envelope away from its closest neighbour (4.3 at the closest, median 9.4), so a
move of the model knob changes the object and not just its brightness. The numbering moved with it: this is the
first version where model 5 is not the same instrument it used to be.# NTS-1 units

Seven custom units for the **KORG Nu:Tekt NTS-1 (mkI)**, built with the
[logue SDK](https://github.com/korginc/logue-sdk) v1.1-0. Two oscillators, three
modulation effects, an endless barberpole phaser in the delay slot and a body and
space model in the reverb slot — a unit for every user slot the instrument has.

| Unit | Slot | What it does | File |
| --- | --- | --- | --- |
| `noise` | OSC | Eight noise algorithms, from white to tuned resonance | `noise.ntkdigunit` |
| `resonator` | OSC | 17 model modal resonator, four voices, played from the keyboard | `resonator.ntkdigunit` |
| `bowed` | OSC | Bowed string, four voices, sustains while the key is down | `bowed.ntkdigunit` |
| `lpg` | MOD | Vactrol style low pass gate driven by an envelope follower | `lpg.ntkdigunit` |
| `reso` | MOD | Twelve mode resonator with beating pairs, Rings in spirit | `reso.ntkdigunit` |
| `elements` | MOD | Geometry morphing resonator: string, membrane, plate, bell | `elements.ntkdigunit` |
| `barber` | DELAY | Barberpole phaser that rises or falls forever | `barber.ntkdigunit` |
| `irbody` | REVERB | Instrument bodies — soundbox, tube, shell — played as filters | `irbody.ntkdigunit` |

The NTS-1 has one slot per type, so `noise` and `resonator` are alternatives
for the oscillator slot, and `lpg`, `reso` and `elements` are alternatives for
the modulation slot. `barber` and `irbody` are alternatives too, for a
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
| OSC | — | `noise` | the only slot that makes sound, and it needs no note to do it |
| OSC | — | `resonator` | the only slot that is handed the note and sits inside the amp envelope, which is what makes a struck object playable |
| OSC | — | `bowed` | the only slot that is handed the note and keeps sounding for as long as the key is down; nothing later in the chain can make a struck sound sustain |
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
| Edit 1 | `Algo`: white, pink, brown, velvet, crackle, step, tuned, hybrid (the menu counts from 1) |
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

### The LFO

The synth routes its LFO to the shape parameter, and both oscillators honour it:
`noise` adds it to the A knob (half the LFO sweeps the character from one end to
the other) and `resonator` adds it to the decay, so a ringing bank breathes with
the LFO the way it moves with the knob. It arrives smoothed at 750 updates a
second, so a fast LFO walks the filters instead of stepping them block by block.

### resonator — oscillator (multi model modal resonator)

`elements` is the same engine with the pitch set by hand, because a modulation
effect gets no note. Here the note is available, so this one is played from the
keyboard: a strike goes into a bank of modes and the amp envelope decides how
long you hear it ring.

| Model | Object | Character |
| --- | --- | --- |
| 0 | string | harmonic series with the inharmonic stretch of a stiff string |
| 1 | membrane | Bessel ratios of a circular membrane: drum heads, gongs |
| 2 | plate | two dimensional bending modes: metal plates and sheets |
| 3 | bell | strongly inharmonic partials, the high ones carry the character |
| 4 | multi | one fundamental and the partials of all four objects at once |
| 5 | chime | tubular bell: near harmonics that break up in the upper ones |
| 6 | kalimba | free-free tine, almost all of its energy in the first partials |
| 7 | glass | circular plate with a free edge: handpan, glass harp, gong |
| 8 | pipe | open air column: harmonic, a struck wood or bamboo tube |
| 9 | marimba | tuned bar (1 : 3.9 : 9.2), warm, rings a second or two |
| 10 | xylophone | undercut bar (1 : 3 : 6), brittle, all attack |
| 11 | vibraphone | bar with every partial doubled a few cents apart: slow tremolo |
| 12 | piano | stiff string with detuned unisons, hammered, long |
| 13 | handpan | octave and fifth tuned steel, near harmonic, shimmering |
| 14 | steel drum | octave led pan: the octave blooms over the note |
| 15 | gong | dense inharmonic metal, beating, highs outlive the lows |
| 16 | bowl | singing bowl: a few widely spaced modes, seconds long |
| 17 | triangle | bent rod: almost nothing but high partials, very long |
| 18 | woodblock | hollow block: two modes and a knock, a fifth of a second |
| 19 | log drum | slit drum, hollow 1 : 2.75, short with a big fundamental |
| 20 | tabla | loaded head, tuned almost harmonic, short and snappy |
| 21 | trumpet | brass bore: every harmonic present, strident highs |
| 22 | reed | stopped pipe with a reed: odd harmonics only, hollow |

| Control | Function |
| --- | --- |
| A (SHAPE) | decay, 0.1 s .. 12 s |
| B (SHIFT) | brightness: how long the high modes live, and how open the strike is |
| Edit 1 | `Model`: 0 string, 1 membrane, 2 bell, 3 kalimba, 4 glass, 5 marimba, 6 xylophone, 7 vibraphone, 8 piano, 9 steel drum, 10 gong, 11 bowl, 12 triangle, 13 woodblock, 14 log drum, 15 tabla, 16 reed (the menu counts from 1) |
| Edit 2 | `Strike`: soft mallet .. hard hammer |
| Edit 3 | `Pos`: where the strike lands across the object |
| Edit 4 | `Tune`: ± 24 semitones |
| Edit 5 | `Level` |
| Edit 6 | `Damp`: extra damping of the high modes |

A model is a ratio set plus the way that object behaves under the knobs. On top
of the ratios each one carries three numbers: how long it rings, how much faster
its high modes die than its low ones, and how much of the strike reaches the top.
That is what separates the two bars tuned to 1 : 4 : 10 — a xylophone is gone in
a fraction of a second with nothing but attack, a vibraphone is still ringing
when the next note comes — and it is how the wood block, the log drum and the
tabla sit next to the glass and the bell instead of sounding like shorter
versions of them. The models keep the knobs: `Damp` darkens any of them, `Strike`
and `Pos` move the attack, `Tune` moves the note.

The strike goes straight into the modes instead of through a coupling factor, so
a short string is not a hundred times louder than a long bell: the decay knob
changes the length of the note, not its level. The first models the unit had set
the reference level; every model is fitted to the same measured loudness, so
switching models does not jump in volume.

The keyboard is fitted too, per model. Every semitone from C1 to C7 was measured
with six strikes per note, because one realisation of the strike noise is not
enough: a single burst moves the level of a note by up to 8 dB against its
neighbours, which is the interference of sixteen modes under one attack and not
a property of the instrument. Averaged, each model gets its own slope and
curvature in octaves from middle C, fitted through the note it was levelled at.
That takes the worst deviation from a model's own middle C from 6.5 dB down to
3.8 dB, with C1 and C7 within a decibel of the middle, and it brings the models
to within 0.8 to 2.5 dB of each other at every note — before this, a piano and a
kalimba could be 7 dB apart an octave above middle C. What is left is that same
mode interference, note by note; there is no smooth curve to fit to it. `Pos` puts a node under the modes
a real bar would miss, which is the difference between striking a bar in the
middle and at the end. The attack is normalised over the sum of the modes and
over the keyboard — the measured 7:1 spread between the bottom and the top of the
range comes down to about 1.1:1 — so one note is as loud as the next.

Four voices: the oscillator slot hands out note-on and note-off, so a new note
takes a free voice instead of cutting off the note before it, and a released key
leaves its object ringing. A fifth note steals the voice that has rung longest.
Each voice keeps the pitch it was struck at — a struck object does not glide —
except the newest one, which follows the keyboard and the tune knob so a solo line
still bends; a note that jumps far enough to put a bank into a runaway drops that
bank instead of passing a NaN on. Four notes at once are not four times as loud:
a single voice keeps its level and the rest are tucked in under it.

### bowed — oscillator (a sustained note)

Everything else here answers a hit. `bowed` is the one that keeps going: a delay
loop of one period is rubbed by a bow of filtered noise plus one small push per
period, and the loop's losses decide how long the note takes to speak and how it
dies. Four voices, because a chord of sustained notes is the point.

| Control | Function |
| --- | --- |
| A (SHAPE) | bow pressure: how hard it presses and how fast it drags |
| B (SHIFT) | brightness: the loop's damping |
| Edit 1 | `Model`: string, metal, pipe, hum, glass (the menu counts from 1) |
| Edit 2 | `Bow`: pressure, separately from A |
| Edit 3 | `Rosin`: how much of the bow's noise reaches the loop |
| Edit 4 | `Tune`: ± 24 semitones |
| Edit 5 | `Level` |
| Edit 6 | `Attack`: how long the bow takes to catch the string |

The five models are the loop's character — how lossy it is, where the body sits,
how much noise gets through — and each one was measured to move the sound: every
model answers the brightness knob by at least 18 dB in some band, and the whole
travel of every knob moves something by 13 dB or more.

Three things had to be measured rather than assumed. **Pitch:** a delay loop
that is fed a constant collects DC and then sings at whatever the body resonates
at — the first version had a 0.19 DC offset and a drone around the body
frequency. The push per period is a pulse now, and a one pole at 12 Hz takes DC
out of the loop itself; a played C4 measures 262 Hz against the 261.6 it should
be. **Sustain:** with the key down the level settles and stays (measured over two
seconds), and after the key comes up it falls to nothing within two seconds.
**Size:** four delay loops of 1024 samples is 16 KB, which is the largest
allocation in this collection and still fits the oscillator slot's 32 KB.

It costs 24 ns/frame with one voice and 79 with four.

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

### irbody — reverb effect (instrument bodies)

The wet signal is stereo now. Two points on a corpus do not hear the same
thing: the tail is read from the four delay lines a few samples apart and goes
through a long allpass on the difference, and the modes themselves take a
quarter of a millisecond detour on the right. Measured over the tails: the two
channels correlate between 0.46 and 0.99 depending on how diffuse the body is —
a wood block is wide, a drum is a point — and the two average back to the level
the unit had before.

The A knob is a continuum. It still picks the family, but between two bodies the
damping, brightness, level and size glide from one into the next, so a violin
can be walked into a cello instead of jumping. The mode ratios stay with the
nearest body, because half of one body's partials with half of another's is
neither. Levels are fitted per body (measured on the host, within 4 dB over the
whole travel) so turning the knob does not change how loud you are.

### irbody — reverb effect (instrument bodies)

Puts the synth inside a body: a violin or cello soundbox, a piano soundboard, a
guitar, a marimba bar, a metal plate, a clarinet bore, a drum shell. The bodies
are models rather than samples, and the model is played instead of convolved: a
bank of resonances for the object's own modes and a network of delay lines for
the diffuse part. It is a reverb effect, so it runs last: whatever the modulation
units and the delay have made is what it puts in the body.

The models are physical: the violin, cello and guitar use the air and wood
resonances of the real instruments (A0, B1-, B1+ and the bridge hill), the wood
preset the free-free bar series of a marimba bar, the steel preset a plate-like
inharmonic series with an even, long decay, the clarinet the odd harmonics of a
closed-open tube with the damping of a wooden bore, and the drum the few modes of
a cylindrical shell. On top of the modes each object has a diffuse tail whose
high frequencies die away faster than its low ones. All of that lives in
[`units/irbody/src/ir_model.h`](units/irbody/src/ir_model.h); the same header
still writes the responses out as WAV files in [`ir/`](ir) through
[`tools/make_irs.cpp`](tools/make_irs.cpp), and those files are what the unit's
levels were measured against.

| Control | Function |
| --- | --- |
| A (TIME) | body: violin, cello, piano, guitar, wood, steel, clarinet, drum |
| B (DEPTH) | damping of the wet signal, dark .. open |
| DEL + B | dry/wet |

The first version of this unit convolved the sound with those responses: 4096
taps in sixteen frequency domain partitions, a 512 point FFT every 256 samples,
603 KB of SDRAM for the spectra and 5.3 ms of latency. It worked and it was, by
a wide margin, the most expensive thing here. The network is the same model
without the finite response in it: eleven resonators at most, four delay lines in
a feedback delay network, two allpasses, one damping low pass — 46 KB of SDRAM
and no FFT, measured at 28 ns/frame against 47 for the convolution.

The one audible difference is the sustain. An 85 ms response stops 85 ms after
the sound that fed it; a network keeps ringing while it is fed, so a held note
now sits in the body instead of leaving it after a few frames. That is what a
body does, and it is the one place where this is not only cheaper but better.

The levels of the bank and the tail per body are fitted, band by band, to the
responses the old version convolved with, and the result is within 1.3 to 2.5 dB
per third-octave band (weighted by level) for six of the eight, 4 dB for the
clarinet and 7 dB for the marimba bar, whose modes sit high and leave the low end
to the tail. Fed a musical signal at full wet, all eight land within 0.4 dB of
where they were.

A switch crossfades between two engines over 85 ms, so turning the knob cannot
click. Everything is set up in the parameter callback: turning the knob builds
one engine, never while a sample is waiting.

## Stacking the units

The NTS-1 chain is oscillator → filter → amp/EG → modulation → delay → reverb,
and every unit only ever sees the audio of what comes before it. Nothing here
depends on its neighbours, so a unit can sit next to the stock effects, next to
someone else's unit, or next to another one of these.

What the mkI *does* constrain is which slots can be full at the same time:

| Combination | Works | Why |
| --- | --- | --- |
| any one of `lpg`, `reso`, `elements` | yes | there is one MOD slot, so they are alternatives |
| `noise` or `resonator` with any of them | yes | the oscillator and the effect slots are independent |
| `barber` in DELAY with a stock reverb | yes | a stock delay and a stock reverb do not collide |
| `irbody` in REVERB with a stock delay | yes | same |
| `barber` **and** `irbody` together | **no** | on the mkI a custom delay and a custom reverb are loaded into the same runtime: the SDK notes that when both are enabled only one of them can be a custom effect, and enabling both anyway has been reported to make them malfunction. Pick one, and put a stock effect in the other slot |

So a full house is one oscillator (`noise` or `resonator`), one of the three
modulators, and one of `barber` or `irbody` — with the remaining effect slot on a
stock type.

### What the numbers mean in a frame

One block is 64 frames at 48 kHz: 1.333 ms. The most expensive unit here,
`barber`, asks for 45 ns of host time per frame, which is 2.9 µs of a 1333 µs
block, or 0.2%. `bowed` with four voices is 79 ns/frame, `resonator` with four
is 71. The Cortex-M4F in the NTS-1 runs at 168 MHz and has a single precision
FPU, so the same code there costs several times what it costs on a desktop core;
even at eight times the host, the heaviest unit is around 2% of a block.

Which is why nothing here is optimised for speed at the cost of the sound: the
eight allpass stages of `barber` are what make its sweep a sweep, and cutting
them would buy idle cycles nothing needs. The one place the budget is real is
memory in the oscillator slot: `bowed` keeps four delay loops of 1024 samples,
16 KB of the 32 KB it gets.

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
  the init callback: `irbody` builds its filter coefficients there, `reso` and
  `elements` build theirs. Switching a program never builds
  anything while sound is running.
* **Suspend and resume leave silence.** After a resume nothing comes out of
  stale state (measured: 0.0000 from all of the effect units). A unit that is suspended loses
  its tail rather than replaying it later.
* **No non-finite samples.** Each unit was hammered with 20 000 random
  parameter jumps across the full range of every control, mid-stream: no NaN and
  no infinity in the output. `reso` and `elements` guard their mode banks — if a
  jump moves the modes so far under a ringing bank that it starts to run away,
  the bank is dropped instead of passing a NaN down the chain.
* **Bounded output.** Every unit ends in a soft clip, so a slot cannot hand the next
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
| `noise` | 5.8 KB | 0.4 KB | 15 ns/frame |
| `resonator` | 5.1 KB | 1.5 KB | 19 ns/frame (71 with four voices) |
| `bowed` | 7.0 KB | 16.6 KB | 24 ns/frame (79 with four voices) |
| `lpg` | 1.6 KB | 0.1 KB | 27 ns/frame |
| `reso` | 1.5 KB | 0.4 KB | 13 ns/frame |
| `elements` | 2.1 KB | 0.7 KB | 18 ns/frame |
| `barber` | 1.8 KB | 0.3 KB | 45 ns/frame |
| `irbody` | 7.0 KB | 52 KB SDRAM | 30 ns/frame |

## Installing on the NTS-1

The unit files are `*.ntkdigunit` packages. Load them with either

* [logue-cli](https://github.com/korginc/logue-sdk/tree/main/tools/logue-cli), or
* the KORG [Librarian application](https://www.korg.com/products/dj/nts_1/librarian_contents.php)
  (the "User Oscillator" / "User Effect" tabs).

After transferring, the units appear at the end of the selection list of their
slot. Custom units need NTS-1 firmware **1.02 or newer**.

Two guides live in `docs/`: [PATCHES.md](docs/PATCHES.md) says what each model
is for and where to start, [HARDWARE.md](docs/HARDWARE.md) is the listening
check list for things the host cannot answer.

## Testing

The units are also measured on a PC, against the small stand-in for the SDK in
`tests/host`. The harness builds each unit, plays it a note or a broadband
spread of tones, and records what it heard: the level in the attack and in the
tail, third-octave bands, whether any of the knobs still moves the sound and
whether random abuse produces anything but a clean signal. Those numbers are
committed under `tests/golden`, and a change that moves the sound by more than
1.5 dB in a band, or renames a dead knob, fails the build.

```sh
tests/run.sh            # compare against the goldens
tests/run.sh --golden   # rewrite them, when the change is intended
```

Two things it cannot tell you: what a unit sounds like on the instrument, and
what it costs there. The host is not a Cortex-M4F.

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
* The unit files themselves are not in the repository: `./scripts/build.sh` and
  the GitHub workflow build them with the gcc 5.4 toolchain Korg recommends for
  the mkI (which is 32 bit only, so it needs `libc6:i386`).
* What the API hands a unit is what these units use, and nothing more is
  reachable: an oscillator gets knob A (`shape`), knob B (`shift-shape`) and six
  edit parameters (hold `OSC` and turn `TYPE` to pick one, then turn the `B`
  knob) — so `noise` and `resonator` have an edit page and fill all six. A
  modfx unit gets only the two knobs (`TIME`, `DEPTH`), a delay and a reverb get
  those plus a dry/wet that lives on the shift parameter (`DEL`/`REVERB` held,
  then the `B` knob). There are no sub-menus in the format: parameter names and
  ranges are fixed in the manifest, so a deeper menu would have to be faked with
  one parameter as a page selector, which is why none of these need one.
* `irbody` is mono in the wet path, so the body is centred, and its tails are
  short by design: these are bodies, not halls — for a space, put a stock reverb
  after it. Its wet path now rings for as long as it is fed, where the
  convolution stopped after 85 ms.
* The mkI gives a modfx unit 6 KB of SRAM and a delay or reverb unit 12 KB of
  code space plus 2.4 MB of SDRAM; all units stay well inside those budgets now
  that none of them links the maths library. `irbody` uses 46 KB of that SDRAM
  for its delay lines, and is the one unit that still carries libm (about 1 KB of
  RAM and 2 KB of code): it builds its coefficients from `ir_model.h`, which uses
  `sinf`, `cosf` and `expf` to describe the objects, and that header is shared
  with the WAV generator.

## License

BSD 3-Clause, see [LICENSE](LICENSE). The Makefiles, linker scripts and unit
entry templates come from the logue SDK, Copyright (c) 2018, KORG INC., under
the same license.
