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
| `resonator` | OSC | 17 model modal resonator, played from the keyboard | `resonator.ntkdigunit` |
| `bowed` | OSC | Bowed string, sustains while the key is down | `bowed.ntkdigunit` |
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
| DELAY | mod | `barber` | a phaser belongs before the reverb, and it needs the shift-depth parameter that only a delay or reverb unit gets — that is where the notch count lives; a modulation effect has no such parameter |
| REVERB | delay | `irbody` | a space belongs at the end of the chain, and a reverb unit gets the same 12 KB of code, 2.4 MB of SDRAM and shift-depth mix as a delay unit |

One consequence of that chain is worth saying out loud, because it decides how
the units behave: only the OSC slot is released at all. When a key comes up, the
amp EG closes on whatever the oscillator slot produced, and the effect slots
simply see less signal. So `resonator` and `bowed` stop themselves within a few
milliseconds of note-off and leave the tail to the EG's release knob; a unit that
kept its own tail would keep playing under a knob that says it stopped.

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
| Edit 6 | `Damp`: a damper on the body, a low pass from 24 kHz down to 500 Hz |

A model is a ratio set plus the way that object behaves under the knobs. On top
of the ratios each one carries three numbers: how long it rings, how much faster
its high modes die than its low ones, and how much of the strike reaches the top.
That is what separates the two bars tuned to 1 : 4 : 10 — a xylophone is gone in
a fraction of a second with nothing but attack, a vibraphone is still ringing
when the next note comes — and it is how the wood block, the log drum and the
tabla sit next to the glass and the bell instead of sounding like shorter
versions of them. The models keep the knobs: `Strike` and `Pos` move the attack,
`Tune` moves the note, and `Damp` is a damper on the body — a one pole low pass on
the voice, from 24 kHz down to 500 Hz. It is not a second decay: a struck C6
loses 7.4 dB of RMS at full damp where C4 loses 1.7, and the ring itself is
untouched, so `B` is still the knob that decides how long the high modes live.
Before 1.11-0 the knob only changed how fast those modes died, which moved the
RMS by 0.01 dB over its whole travel at C6 — a control that did nothing you could
hear.

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

One voice, because the NTS-1 is monophonic: one note sounds at a time, so a new
note takes the bank and there is no second note for a second voice to hold. The
voice keeps the pitch it was struck at — a struck object does not glide — and
follows the keyboard and the tune knob, so a solo line still bends; a note that
jumps far enough to put the bank into a runaway drops it instead of passing a NaN
on. A released key ends the note: the voice fades out in about 4 ms and the amp EG
of the NTS-1 takes the tail from there, so the release knob is the release, and
the knob cannot be lied to by a bank that rings for half a minute underneath it.

### bowed — oscillator (a sustained note)

Everything else here answers a hit. `bowed` is the one that keeps going: a delay
loop of one period is rubbed by a bow of filtered noise plus one small push per
period, and the loop's losses decide how long the note takes to speak. When the
key comes up the bow leaves at once, whatever the Attack knob is set to, because
the amp EG of the NTS-1 comes after this slot and owns the tail. One voice: the
NTS-1 plays one note at a time, so a new key takes the delay loop and the note
before it is over, which is what a bow does to a string anyway.

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
**Size:** one delay loop of 1024 samples is 4 KB, a quarter of what the polyphonic
version held and still the largest allocation in this collection.

**Cost, on the M4.** The audio loop prices at 216 cycles per frame, plus the 14
of the `bowNoise` call it makes per sample: about 230, so a 64 frame block —
1.333 ms — is 82 us of the 180 MHz core, 6% of it. The polyphonic version was 777
for four voices (one active and three idle); those three are gone, and the loop
no longer walks a voice array at all. That 777 was counted from the gcc 5.4 image
CI builds and this 230 from the gcc 10.3 build of this change in a sandbox, so the
two are the same method but not the same compiler.

```
                       cycles/frame   per block   of the 1.333 ms block
mono loop                      216      13 824      77 us   5.8%
with bowNoise                   230      14 720      82 us   6.1%
```

Those are counted from the image, not from a PC: the audio loop is disassembled
and priced instruction by instruction with the Cortex-M4's timings (1 cycle for
the ALU and the single-precision FPU, 2 for a load or store, 3 for a call and a
taken branch). `tools/cycles.py` does the arithmetic and prints the ranges; the
model is written into its header. Two things it cannot know: the clock (the F446
tops out at 180 MHz, KORG does not publish the figure, and at 168 MHz the same
block is 7%) and the pipeline (an M4 can issue a following instruction while a
load settles, so summing over loads is an upper bound). Neither changes the
answer: it fits, with almost all of the block left for the rest of the synth.

The same loop on this host measures 10 ns per frame, a hundred and thirty times
less in wall time, and that is not a mistake: a 3.5 GHz out-of-order core with a dual-issue FPU really is that
far from 180 MHz in order. It is why the table below is a comparison between
units and this number is a promise about the instrument.

The cycle count used to be worse in a way nobody could hear: the body's two resonators
worked their coefficients out per sample with `cosf`, twice per voice, and `cosf`
also pulled newlib's sine kernels into the image. They are calculated once when
the model changes now: `nm -u` on the unit file lists nothing at all.

Note-on costs 1.7 us, and most of that is the noise the note starts
from. It is worth knowing what that does and does not allow: the unit used to
zero the delay line before filling it, which was a second pass over 4 KB for
nothing (the fill overwrites all of it anyway) and worth 3% of the note-on. What
looks like the bigger win — seeding only the first period, since that is all the
loop reads before it has written the rest — was tried and measured, and it takes
note-on to 0.4 us, but it changes the attack by 6 dB: with the rest of the line
holding an older note instead of noise, the first frames come out differently. So
the whole line is seeded, and the loop keeps its attack.

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

Four points on the B knob are the four objects themselves: 0, 341, 682 and 1023.
Within 3% of the way between two of them — ten counts — the knob plays that model
exactly, so the membrane can be found by hand and is exactly the membrane. Past
that band the morph is picked up again over the next ten counts by a smoothstep,
which leaves the band and re-enters the morph with zero slope, so the level never
steps: over the whole B sweep with one plucked note at 220 Hz the largest change
between neighbouring knob counts is 3.6 dB, against 7.6 dB for a hard edge — and
the morph alone, with no band at all, reaches 2.5 dB per count where a mode sits
on the note. Everything beyond the return — 900 of the 1024 positions — is
bit-identical to that plain morph.

The instrument is mono, so this unit renders one bank. The slot is handed a main
and a sub buffer, but an instrument with one oscillator has one signal, and the
SDK says a platform without a sub timbre may ignore the sub buffers; a host that
still arrives with a separate sub output gets the same samples there. The bank
runs two samples per step, so each mode's coefficients and its two states are
used for a pair instead of being read and written once per sample.

| Control | Function |
| --- | --- |
| A (TIME) | fundamental, 40 Hz .. 2 kHz |
| B (DEPTH) | geometry: string → membrane → plate → bell |

### barber — delay effect (barberpole phaser)

One to sixteen all-pass sections, eight notches in the audible range at the top
setting, and a second cascade half a cycle out of phase takes over where the first
wraps around, crossfaded with a raised cosine. The result never jumps, so it
rises (or falls) forever. Feedback sharpens the notches into the classic jet
sound.

The sweep covers 40 Hz .. 6.4 kHz of all-pass corner, which puts the lowest notch
at a tenth of the corner and the highest at 3.7 times it: every notch travels
7.3 octaves on its way up, 44% more than the five it used to cross. Sixteen
sections and a wider corner cost 42% of the unit's host time (33.8 → 47.9
ns/frame), which is still among the cheapest things in this collection.

| Control | Function |
| --- | --- |
| A (TIME) | rate and direction: left of centre falls, right of centre rises, further out is faster (0.02 .. 10 Hz) |
| B (DEPTH) | feedback / resonance |
| DEL + B | number of notches, one to sixteen |

Each cascade is mixed with the dry signal at the middle, which is where its
notches are deepest, and the two are then crossfaded; that sum is the output.
Mixing it with the dry signal a second time used to put a 6 dB ceiling on the
notches — a quarter of the all-pass against three quarters of the dry — and that
ceiling, not the sweep, was most of why this slot read as a phase drift rather
than a barberpole. The mix is fixed because the shifted DEPTH control is needed
for the notch count: adding or removing a section starts or empties that
section's state, so the count can be turned while it runs without a click.

**The sweep had a sign error until 1.5-0.** The first-order allpass here is
`y = x1 + c(x - y1)`, whose pole sits at `-c`, so its corner is at `fc` only when
`c` is negative and close to −1. The coefficient was `(1 - t)/(1 + t)`, which puts
the pole at −1 and mirrors the corner to `fs/2 - fc`: every notch of the cascade
sat in the top octave. Measured across 100 Hz .. 4 kHz the response was flat to
within 0.1 dB, and a steady tone moved by 0.0, 0.0 and 0.2 dB at 500, 977 and
2000 Hz — there was no comb in the band to move. `(t - 1)/(t + 1)` is the same
sweep with the pole where it belongs: six notches between 100 Hz and 4 kHz (172,
532, 938, 1436, 2126 and 3238 Hz, 20 to 34 dB deep at a still phase), and a steady
tone now swings 50 to 59 dB as they pass. That, and not the depth of the sweep,
is why this slot read as a phase drift.

At the middle of the knobs the unit is −0.11 dB against the dry signal, measured
with a plucked note.

### irbody — reverb effect (instrument bodies)

The wet signal is stereo now. Two points on a corpus do not hear the same
thing: the tail is read from the two delay lines a few samples apart and goes
through an allpass on the difference, and the modes themselves take a quarter
of a millisecond detour on the right. How wide that is depends on the
body — a wood block is wider than a drum — and the two average back to the level
the unit had before. The harness measures the correlation on the 21 ms right
after the input stops, `corr_hi` the same window above 2 kHz.

The tail is one network, not one per engine. Every body here is the guitar's
size, so the two delay lines are the same length whichever preset is chosen, and
both engines in a crossfade can stand in the same set of them: a body change
costs a second mode bank instead of a second tail, and the network's settings
are walked from one body's numbers to the other's once a block while the 85 ms
fade runs. That is the peak the instrument was paying for while a knob turned —
715 → 654 cycles a frame standing still and 1327 → 1031 with a crossfade
running, counted off the image. Standing still the unit is sample for sample
what it was; while the knob moves the two builds differ by -19 dB (correlation
0.994), because one tail carries both bodies where two used to be summed.

It is also what took the last buffer out of SDRAM: the comb lines are 2.6 KB and
they fit in the slot's 12 KB now that there is one set instead of two, so a
sample no longer waits for the external memory on the hottest path in the unit.

The A knob is a continuum. It still picks the family, but between two bodies the
damping, brightness, level and size glide from one into the next, so a violin
can be walked into a cello instead of jumping. The mode ratios stay with the
nearest body, because half of one body's partials with half of another's is
neither. Levels are fitted per body (measured on the host, within 4 dB over the
whole travel) so turning the knob does not change how loud you are.

Each body also holds exactly for 3% of the knob on either side of it (±31 counts)
instead of only inside the quarter step it lands on (±18), so the glide into the
next body has not started yet when you are near one. Over the whole A sweep that
band moves 174 of 1024 positions, by at most 1.65 dB.

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

No body is larger than a guitar. The model describes a piano soundboard and a
steel plate as bigger things, and a knob that walks all of them had no business
putting a note in a room: the comb scale and both decay constants are brought
back to the guitar's, so every preset is a body you could hold, and a piano or a
plate is that body's colour rather than a hall. Two delay lines instead of four,
no allpass on the side signal and none on the tail, which is what a small object
needs and what the slot can afford: the two lines are read four times between
them where the old network read nine.

Only the comb line that the tail stands in lives in SDRAM, 5 KB of it. The two
engines and the short delay lines are read and written once per sample, so they
sit in SRAM instead: 2.3 KB of it, with the code, out of the 12 KB the slot has.
That is a placement, not a different sound — on the host the cost is the same
before and after, and it is on the instrument that a sample would otherwise wait
on SDRAM for its own state.

A body ends when the sound does: the unit's own tail is 8 to 22 ms, so it is the
one place here where a state left in the slot could be heard after the note. It
cannot be: 20 ms of true digital silence closes the output over 10 ms and opens
it the same way. A held note, a release, and a delay tail in front of it all
keep the gate open, because the input is only exactly zero once the amp EG of the
synth has already taken the note down.

| Control | Function |
| --- | --- |
| A (TIME) | body: violin, cello, piano, guitar, wood, steel, clarinet, drum |
| B (DEPTH) | damping of the wet signal, dark .. open |
| DEL + B | dry/wet |

The first version of this unit convolved the sound with those responses: 4096
taps in sixteen frequency domain partitions, a 512 point FFT every 256 samples,
603 KB of SDRAM for the spectra and 5.3 ms of latency. It worked and it was, by
a wide margin, the most expensive thing here. The network is the same model
without the finite response in it: eleven resonators at most, two delay lines in
a feedback delay network, three allpasses, one damping low pass — 5 KB of SDRAM
and no FFT, measured at 36 ns/frame against 47 for the convolution.

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
`barber`, asks for 34 ns of host time per frame, which is 2.2 µs of a 1333 µs
block, or 0.16%. `bowed` is 10 ns/frame, `resonator` 27. The Cortex-M4F in the
NTS-1 runs at 168 MHz and has a single precision
FPU, so the same code there costs several times what it costs on a desktop core;
even at eight times the host, the heaviest unit is around 2% of a block.

Which is why nothing here is optimised for speed at the cost of the sound: the
sixteen allpass stages of `barber` are what make its sweep a sweep, and cutting
them would buy idle cycles nothing needs. The one place the budget is real is
memory in the oscillator slot: `bowed` keeps one delay loop of 1024 samples and
`resonator` one bank of modes, 4 KB and 0.4 KB of the 32 KB they get.

### Levels

A patch should not jump when the unit changes, so the levels are fitted to the
SDK's own reference rather than to each other. `tools/levels.cpp` is the
measurement: it takes the same `--ranges` as the harness, and `--pluck` plays
one note with a decay, which is what a body, a gate and a resonator answer,
instead of the forty eight simultaneous tones the goldens use.

* **An oscillator at `Level` 100 peaks at full scale.** That is what the SDK's
  own demo does: `waves` writes its wavetable straight to the output, peak 1.0,
  with a soft clip of 0.125 on top. Measured at note 60: `noise` 0.90,
  `bowed` 0.82, `resonator` 0.90. `resonator` used to peak at 0.53 — it was
  fitted when it was polyphonic, at a quarter of the level knob. `bowed` was
  already at the top: at 100 its mix sits in its own soft clip, which is why its
  RMS (0.69) is above a full-scale sine and why raising its trim there only
  added distortion.
* **An effect leaves the level of a dry note where it found it**, at the middle
  of its knobs. With one plucked note: `lpg` +0.4 dB, `reso` +1.1, `barber`
  +0.8, `elements` −0.0 and `irbody` +0.8. `elements` was 9.8 dB down and
  `irbody` 17.3 dB up — the body sat on top of the note and clipped it on every
  pluck, where a reverb should sit behind it.

The `waves` demo itself does not build against the host stand-in (its `dsp`
headers want the ARM SDK's own float helpers), so the reference is read from its
source: the table at full scale and no gain on the way out.

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
* **Tails end.** Ten seconds after the input stops, the effects are at least
  60 dB down with their knobs in the middle, and at least 12 dB down with the
  knobs at the top, where a bank rings longest (`reso` at its 5 s ring time is
  the -16 dB case). The harness prints both figures (`tail_db`, `tail_db_top`)
  for the effects — the level over the last second of a ten second render,
  against the level while the input was there — and `tests/run.sh` fails above
  those limits. A ring that never ends measures at or above the note itself:
  `elements` reached a 179 s ring time at the bell end and measured +1 dB there
  before this change. Its bell end is 2 s now, and -36 dB at the top.

Measured with `tools/bench.cpp`: the same workload in every unit, nanoseconds per
audio frame on this host (x86, gcc 10.3, 20 s of audio, minimum of three passes).
The previous build is run alternately in the same session, so a busy machine
cannot flatter either column. One 64 frame block at 48 kHz is 1.333 ms, and these
are a comparison — between units and against the version before this one — not an
M4 prediction: the gap is about two orders of magnitude, see `bowed` above. The
one figure that is not from this host is `bowed`'s 777 cycles per frame, counted
on the M4 from its own image.

**Busy** is a note held (a spread of tones for the effects); **idle** is the same
unit with nothing to do — no key down, or digital silence in, which is what the
effect slots are handed between notes.

| Unit | Text + data | SRAM | Busy | Idle |
| --- | --- | --- | --- | --- |
| `noise` | 5.8 KB | 0.4 KB | 8.6 → 8.9 ns/frame (unchanged) | 8.4 → 8.4 ns |
| `resonator` | 5.0 KB | 0.4 KB | 31.9 → 27.0 ns (**-15%**) | 1.4 → 0.4 ns (**-69%**) |
| `bowed` | 2.7 KB | 4.1 KB | 14.1 → 10.2 ns (**-27%**) | 7.3 → 1.6 ns (**-78%**) |
| `lpg` | 1.7 KB | 0.1 KB | 21.0 → 21.0 ns | 21.1 → 2.1 ns (**-90%**) |
| `reso` | 2.1 KB | 0.4 KB | 28.6 → 22.3 ns (**-22%**) | 29.5 → 2.6 ns (**-91%**) |
| `elements` | 2.6 KB | 0.4 KB | 39.2 → 16.3 ns (**-58%**) | 39.9 → 1.6 ns (**-96%**) |
| `barber` | 2.4 KB | 0.6 KB | 33.8 → 47.9 ns (16 sections) | 34.0 → 2.4 ns (**-93%**) |
| `irbody` | 5.3 KB | 4.0 KB | 42.2 → 32.3 ns (**-23%**) | 46.5 → 2.1 ns (**-95%**) |

Summed over one slot of each type — `bowed`, `elements`, `barber` and `irbody`,
which is a full house on the mkI — that is 129 → 93 ns/frame while playing, and
128 → 7.7 ns/frame with the instrument idle. Which slot types run at once is the
difference the instrument hears: four slots together used to do the same work
whether or not a note was sounding, and between notes now almost none.

What was done, and why the sound does not move:

* `reso` computes both timbres in one pass over the modes. The coefficients are
  shared by the two channels and were loaded twice; they are loaded once now,
  with the same three terms per channel in the same order.
* `elements` renders one bank instead of two, because the instrument has one
  oscillator, and steps it two samples at a time: a mode's coefficients and its
  two states are used for the pair, so the state is read and written once where
  it was read and written twice. Its ratio table is gone too — the four
  geometries are worked out when the knob moves rather than kept as four times
  sixteen floats.
* `resonator` skips the per-mode drive coefficient once the strike is over — a
  few milliseconds into the note — because that term is exactly zero after it, so
  the two paths agree sample for sample.
* Every effect slot now skips a block whose input is digital silence when its own
  state has run down. That is not an approximation: every term of the sum is
  zero, so the output is written and the DSP is not run. It is where most of the
  saving is, because the slots used to run flat out between notes. `barber` and
  `irbody` also stop advancing their own state there (the phaser's LFO phase, the
  bodies), which by then is 20 ms behind and below -180 dB.
* `barber` steps its two cascades in one pass over the stages: the same
  arithmetic, but two independent chains for the core to interleave instead of
  one at a time, and half the loop bookkeeping. `lpg` gets the same per-channel
  skip as the other effects.
* The state guards that keep a bank off the FPU's denormal path now trigger at
  -180 dB instead of -300 dB, which is what lets an idle channel be skipped
  rather than merely cleared.
* `noise` is unchanged and was the cheap one already: a noise source has to
  render every frame. `bowed` and `resonator` are one voice instead of four,
  because the NTS-1 plays one note at a time: no voice loop, no voice
  allocation, and an idle note cannot cost anything when it does not exist.

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

What it can tell you is which way a change moved the cost. `tools/bench.cpp` is
built the same way as the harness (one unit, `UNITKIND`, `UNITFILE`) and prints
nanoseconds per audio frame for a unit that is playing and for one that is idle;
the cost table above comes from it. `tools/cycles.py` is the other half: it
prices a range of a unit's own ARM code with the Cortex-M4's timings, which is
how `bowed`'s 777 cycles were counted.

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

`.github/workflows/build.yml` builds all units on every push and uploads them as
an artifact. Cutting a release is pushing a tag: the same workflow builds the
units with gcc 5.4 and attaches the eight packages to a release, creating it with
the commit list as its notes if it does not exist yet. A tag with a suffix
(`v0.2.0-beta`) makes a prerelease, a release published by hand gets its packages
the same way, and a second run for a tag that already has a release replaces the
packages on it rather than making another.

## Memory budgets

The build checks that every unit still fits the memory its slot gives it — 32 KB
for an oscillator, 6 KB for a modulation effect, 12 KB for a delay or reverb
unit, with the reverb's SDRAM counted separately — and fails if one grew past
it. `tools/check_budget.py` prints the numbers:

```
irbody       4980 /  12288 bytes of revfx memory  ok
bowed       19524 /  32768 bytes of osc memory  ok
```

That check is not ceremony: `irbody` came within 3 KB of its 12 KB while it was
getting its stereo paths, and what did not fit was libm, not the reverb.

The same tool runs `nm` over every unit file and fails if one of them links the
maths library:

```
barber       1924 /  12288 bytes of delfx  memory  ok    libm: none
irbody       4980 /  12288 bytes of revfx  memory  ok    libm: none
```

Both snippets are what CI prints, which is the gcc 5.4 the instrument is built
with; the newer gcc in a sandbox builds several units smaller, so the gate is the
stricter of the two. `irbody` used to be the tight one — 632 bytes of headroom
left, nearly all of it spent on the maths library — and taking that out is what
gave it back 6.6 KB.

`tools/cycles.py` is the other half of that: it prices a range of any unit's
disassembly with the Cortex-M4's instruction timings, which is where `bowed`'s
cycles come from. The ELFs are in the CI artifact for exactly that.

One `cosf` in a loop is not a small thing here. It costs a few hundred cycles per
sample, it pulls newlib's sine kernels in (4 KB of the oscillator slot's 32 KB,
which is what `bowed` was carrying until it stopped calling it per sample), and
it brings the reentrancy struct along, which is a kilobyte of RAM in a slot that
has 6 or 12. There are no exceptions left: `irbody` was the last unit with an
allowance, for two calls — `powf` for a level in decibels and `sqrtf` for the
midpoint between two bodies — and both are polynomials in
`units/common/arcmath.h` now. `ir_model.h`'s renderer, which is written in
`sinf`/`cosf`/`expf` for the sake of the WAV generator, is behind
`IRMODEL_RENDER` and is not compiled into the unit at all.

What each unit file links, from `nm` over the ELFs CI builds:

| Unit | Symbols from outside the unit | Undefined |
| --- | --- | --- |
| `barber` | none | none |
| `bowed` | none | none |
| `elements` | none | none |
| `irbody` | none | none |
| `lpg` | none | none |
| `noise` | `___osc_mcu_hash_veneer`, `___osc_white_veneer` | none |
| `reso` | none | none |
| `resonator` | `___osc_mcu_hash_veneer` | none |

The three veneers are the loader's stubs for SDK calls — `osc_white` and
`osc_mcu_hash` — not library code. Everything else each unit needs, it carries.

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
  those plus the shift parameter (`DEL`/`REVERB` held, then the `B` knob) — a
  dry/wet on `irbody`, the notch count on `barber`. There are no sub-menus in the format: parameter names and
  ranges are fixed in the manifest, so a deeper menu would have to be faked with
  one parameter as a page selector, which is why none of these need one.
* A freshly installed unit sounds like its own start values, not like nothing.
  The instrument hands a unit the parameter values the program has stored, and a
  program that has never held that unit sends zero for all of them — a level of
  zero is silence, so every unit was dead until a knob was moved (this is exactly
  how `noise` and `bowed` behaved on the NTS-1). Each unit now keeps what its
  `OSC_INIT` set until the program sends a parameter that is not zero; from then
  on the patch wins, zeroes included, so a knob at the bottom still means the
  bottom. A and B are latched separately, because on `bowed` A is bow pressure
  and an A that was never chosen should not pull the level down with it.
* `irbody` is mono in the wet path, so the body is centred, and its tails are
  short by design: these are bodies, not halls — for a space, put a stock reverb
  after it. Its wet path now rings for as long as it is fed, where the
  convolution stopped after 85 ms.
* The mkI gives a modfx unit 6 KB of SRAM and a delay or reverb unit 12 KB of
  code space plus 2.4 MB of SDRAM; all units stay well inside those budgets now
  that none of them links the maths library. `irbody` is 9432 of its 12288 and
  needs none of the SDRAM now: its delay lines are one shared set of 2.6 KB in
  SRAM, where two cost it 5 KB of the external memory.

## License

BSD 3-Clause, see [LICENSE](LICENSE). The Makefiles, linker scripts and unit
entry templates come from the logue SDK, Copyright (c) 2018, KORG INC., under
the same license.
