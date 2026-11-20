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

Six custom units for the **KORG Nu:Tekt NTS-1 (mkI)**, built with the
[logue SDK](https://github.com/korginc/logue-sdk) v1.1-0: three oscillators, two
modulation effects and a space in the delay slot. The reverb slot still ends the
chain on the instrument's own effects; everything here either makes the sound,
shapes it, or puts it in a room.

| Unit | Slot | What it does | File |
| --- | --- | --- | --- |
| `noise` | OSC | Eight noise algorithms, from white to tuned resonance | `noise.ntkdigunit` |
| `resonator` | OSC | 17 model modal resonator, played from the keyboard | `resonator.ntkdigunit` |
| `bowed` | OSC | Bowed string, sustains while the key is down | `bowed.ntkdigunit` |
| `reso` | MOD | Twelve mode resonator with beating pairs, Rings in spirit | `reso.ntkdigunit` |
| `wavefolder` | MOD | Five fold wavefolder, triangle to sine curve | `wavefolder.ntkdigunit` |
| `reflect` | DEL | A space built from four short delay lines and its own reflections | `reflect.ntkdigunit` |

The NTS-1 has one slot per type, so `noise`, `resonator` and `bowed` are
alternatives for the oscillator slot, `reso` and `wavefolder` are alternatives
for the modulation slot, and `reflect` is the delay slot. That is the whole
repository: three oscillators, two shapers and one room. There is no user slot
for an envelope generator on the mkI — the MOD slot only accepts audio
modulation effects — and the reverb slot is left to the instrument's own
effects.

### Where each unit sits

The NTS-1 chain is fixed: oscillator, filter, amp/EG, then the three effect
slots — modulation, delay, reverb, in that order. Every unit is in the slot
where that chain does the most for it.

| Slot | Comes after | Unit | Why there |
| --- | --- | --- | --- |
| OSC | — | `noise` | the only slot that makes sound, and it needs no note to do it |
| OSC | — | `resonator` | the only slot that is handed the note and sits inside the amp envelope, which is what makes a struck object playable |
| OSC | — | `bowed` | the only slot that is handed the note and keeps sounding for as long as the key is down; nothing later in the chain can make a struck sound sustain |
| MOD | amp/EG | `reso` | it turns the oscillator into an instrument, so everything time based should come after it |
| MOD | amp/EG | `wavefolder` | it folds what the amp envelope hands it, so a plucked note opens up as it decays and a held one sits at a steady fold: the shaper wants the shaped signal, and that is this slot |
| DEL | MOD | `reflect` | a space has to come after the shaper, and this is the slot with the 12 KB and the memory a tail of a second needs. It keeps the dry signal, so a stock reverb can still end the chain |

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
with a sine at the same pitch. A key that arrives while the previous one is
still down is a slur: nothing is restarted and the tuned algorithms walk to the
new pitch over 32 ms, the same time for any interval. White, pink and brown stay close to their usual
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
| 0 % | mallet | soft head, no high end, 14 ms thump | `resonator` membrane and bowl |
| 25 % | stick | hard full-band click, gone in 3 ms | `resonator` marimba, `reso` at a short ring |
| 50 % | pluck | bright edge over a woody body, 28 ms | `resonator` piano and guitar |
| 75 % | ping | narrow ring tuned to the note, 140 ms | `reso` with a long ring, `bowed` |
| 100 % | knock | knuckle on wood, 5 kHz and down, slow | `wavefolder` after it, or a low thump under anything |

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

`reso` is the same kind of engine with the pitch set by hand, because a
modulation effect gets no note. Here the note is available, so this one is played
from the keyboard: a strike goes into a bank of modes and the amp envelope decides how
long you hear it ring. A key that arrives while the previous one is still down
is a slur and not a second strike: the bank is retuned along a straight line to
the new note in 32 ms, keeping its phase, which is a body being driven at a new
pitch rather than a second body.

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

The string follows the pitch the synth reports, block by block: a bend, and the
`Tune` knob, retune the loop the way a string is retuned, by sliding the period
per sample rather than jumping the read pointer at a block edge. From C4 to G4
the period goes 184 to 123 samples; the largest sample step during that bend is
0.031, against 0.034 for the same note standing still, so nothing clicks. Only
when the pitch moves: a note nobody bends costs one compare per block.

A key that arrives while the previous one is still down is a slur: the loop is
not re-seeded and the bow does not restart, only the period walks to the new
note, in a straight line over 32 ms. Measured C4 → G4: 184, 173, 150, 133, 121
samples at 0, 10, 20 and 30 ms, ending on 123 where G4 is 122.4, with the level
the same as before the slur.

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

### reso — modulation effect (modal resonator)

Twelve tuned resonators in near pairs, so the modes beat against each other.
Feed it clicks, noise or plucks and it becomes a struck bar, a bell or a body;
feed it sustained sound and it turns into a tuned drone.

| Control | Function |
| --- | --- |
| A (TIME) | fundamental, 40 Hz .. 2 kHz |
| B (DEPTH) | ring time, 0.1 s .. 5 s |

### wavefolder — modulation effect (five fold wavefolder)

A folder turns a signal back on itself at full scale instead of clipping it:
what a clipper would flatten becomes another trip up the curve, and the
harmonics that adds are the effect. This one folds at most five times — the
fifth reflection is the last, and past it the curve rides the cap — so the knob
walks from a clean signal into five reflections instead of into a fuzz.

| Control | Function |
| --- | --- |
| A (TIME) | fold: drive into the curve, 1x .. 5x |
| B (DEPTH) | shape: triangle fold .. sine fold |

The drive ends where the cap begins. It used to run to 8x, which put a full
scale signal at the fifth fold from 77% of the knob on: the last quarter of the
turn did nothing (and on a pluck that peaks at 0.9, the last sixth). With 5x at
the top, every equal turn adds one reflection and the top of the knob is exactly
the fifth one.

The two curves are the ones folders are usually built from. A triangle fold is
piecewise straight, so every reflection is a corner and the top of the knob is
hard and buzzy; a sine fold is the same walk with round corners. The B knob is
the one in between, so it moves the edge of the harmonics rather than how many
of them there are: at the middle of the drive and on the 0.9 peak pluck, the two
ends of it land 2.0 dB apart on the curve alone and 0.4 dB apart once the level
comparator has evened them out, the sine end being the louder of the two.

Drive is gain as well as fold, and a knob that is really a volume control is not
what this is for. What the level does as the drive goes up is not 1/drive,
though: a folded signal is bounded, so its RMS stops growing once the input is
folded, while the clean end still tracks the drive. Measured with
`tools/levels.cpp --pluck --peak 0.9` — a pluck that peaks at 0.9, which is where
a loud patch sits — the curve on its own is +1.7 dB at the clean end, +6.4 dB at
the middle of the knob and +7.3 dB at the top. So the gain is 1.7 dB down
throughout plus 1.0 to 1.9 of compensation that saturates with the drive.

That is a fit to one input level, and a folder's level depends on the level it is
fed: at 5x, a pluck that peaks at 0.9 comes out where it went in, and the same
pluck 20 dB quieter comes out 9 dB louder, because the quiet one is barely folded
and its drive is just gain. A second mechanism closes that. Every block is
compared with the block it was given and a gain is moved by the difference —
15 ms to come down, 200 ms to come back up, never more than 10 dB either way, and
taken whole on the first block of a note so a note never starts on the gain of the
one before it. The unit therefore hands on the level it was given whatever that
level is: measured at five input peaks from -20 dB to full scale and at the
bottom, middle and top of FOLD, the whole set lands between −1.5 and +0.6 dB of
the dry level, where without it the same fifteen measurements spanned 10 dB
(+8.8 dB for the quietest patch at the top of the knob, −0.9 dB for the loudest).
It costs a multiply and two accumulations a sample.

Which is the point of the comparator: the timbre follows the drive and the level
does not, so a patch does not change how loud it is when the effect is switched
in, however quietly it happens to play.

The curve keeps no state and has no tail: it is a function of the sample going
through it, so digital silence in is exactly digital silence out and the block is
skipped (the knob ramps still run, so a knob moved over silence lands where it
should). The one thing that carries over between blocks is the comparator's gain,
which is a gain and not a sound. Both knobs move as 20 ms slopes spread across
the samples of the block, because the drive is a gain into a curve and a gain that
steps between parameter messages is a click on anything sustained. It is a small
unit for a curve, and no maths library.

### reflect — delay effect (a space from short delay lines)

Not an impulse response and not a reverb in the usual sense: four short delay
lines, 3.8 to 9.4 ms at full size, that feed each other back, with four wall
reflections read off the input in front of them and two allpasses to smear what
goes in. The response is the train of reflections those lines actually produce —
nothing is convolved and no response is stored anywhere in the unit — and it
changes with the size knob the way a room of a different size does.

| Control | Function |
| --- | --- |
| A (TIME) | size: the lines and the walls scale from 0.35x to 1x |
| B (DEPTH) | decay: 0.62 to 0.98 of the reflection coming back per pass |
| DEL + B knob | dry/wet |

The walls are four taps between half a millisecond and two, with alternating
signs, so the first thing heard is a set of discrete reflections instead of one
comb. The room is four lines damped at 5 kHz and fed back through a Householder
matrix, so every line receives a little of all four: that is what turns four
echoes into a tail. The two allpasses in front of the network are what make the
first reflection a smear rather than four clicks.

Two things the measurements say:

* **The B knob is not a volume control.** A network with more feedback stores
  more energy, so the wet path is scaled by (1 - feedback) and referenced to the
  middle of the knob. On the 0.9 peak pluck it reads +1.3 dB at the middle of the
  knobs, +1.3 dB with size at its bottom, +1.4 dB at its top, +0.3 dB with decay
  at its bottom and +1.0 dB at its top: the whole travel of both knobs is inside
  1.2 dB. Full wet reads +3.7 dB, which is a room at full mix, and 100% dry
  reads -0.1 dB.
* **The size knob glides the read distance, not the length.** Turning a delay
  length in steps is a click; moving it as a slope is the pitch glide a tape
  delay makes, which is the sound a delay line is supposed to make when it is
  moved. Everything else the knobs do is a gain and takes the same 30 ms glide,
  so nothing steps.

The two channels are two points of the same room rather than two rooms: left
weighs the four lines equally, right gives the even ones a plus and the odd ones
a minus. That is a rotation, so it costs no level, and it is why the pair is
wide — 0.22 correlation on the harness source and 0.42 above 2 kHz — without the
sides being out of phase at any frequency. A delay between the channels would
cancel in mono; a different weighting of the same reflections does not.

It is in the delay slot because that is the slot with the memory a space needs,
and because it adds the wet to the dry instead of replacing it: a stock reverb
can still end the chain after it. The four lines, the two diffusers and the input
history are 6524 bytes of SRAM, and the unit is 8352 of the delay slot's 12288
bytes in the gcc 5.4 build, with no maths library.

## Stacking the units

The NTS-1 chain is oscillator → filter → amp/EG → modulation → delay → reverb,
and every unit only ever sees the audio of what comes before it. Nothing here
depends on its neighbours, so a unit can sit next to the stock effects, next to
someone else's unit, or next to another one of these.

What the mkI *does* constrain is which slots can be full at the same time:

| Combination | Works | Why |
| --- | --- | --- |
| any one of `reso`, `wavefolder` | yes | there is one MOD slot, so they are alternatives |
| `reflect` | yes | the delay slot is a slot of its own |
| any oscillator with either shaper and `reflect` | yes | oscillator, modulation and delay are three independent slots |
| `reflect` with the instrument's own reverb | yes | it is a custom *reverb* that would share a runtime with a custom delay; the stock reverb does not |

So a full house here is one of the three oscillators, one of the two shapers and
`reflect` in the delay slot, with the reverb left to the instrument.

### What the numbers mean in a frame

One block is 64 frames at 48 kHz: 1.333 ms, and the bench strikes a note every
half second so an oscillator is timed while it is sounding and not after it has
decayed. The most expensive unit here, `reflect`, asks for 39 ns of host time per frame,
which is 2.5 µs of a 1333 µs block, or 0.19%. Then `reso` 22 ns/frame,
`resonator` 20, `wavefolder` 16, `bowed` 12 and `noise` 9. The
Cortex-M4F in the
NTS-1 runs at 168 MHz and has a single precision
FPU, so the same code there costs several times what it costs on a desktop core;
even at eight times the host, the heaviest unit is around 2% of a block.

Which is why nothing here is optimised for speed at the cost of the sound. The
one place the budget is real is memory in the oscillator slot: `bowed` keeps one
delay loop of 1024 samples and `resonator` one bank of modes, 4 KB and 0.4 KB of
the 32 KB they get.

### Levels

A patch should not jump when the unit changes, so the levels are fitted to the
SDK's own reference rather than to each other. `tools/levels.cpp` is the
measurement: it takes the same `--ranges` as the harness, `--peak` scales the
source to the level a patch actually reaches, `--settle` runs the unit before the
source starts so a knob ramp has arrived, and `--pluck` plays
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
  of its knobs. With one plucked note `reso` reads +1.1 dB and `reflect` +1.3 dB,
  the latter staying inside 1.2 dB of that over the whole travel of its size and
  decay   knobs (and reading +3.7 dB at full wet, which is a room at full mix).
  `wavefolder` is the one that needed more than a fit: its drive is gain as well
  as fold, so it is fitted on a pluck that peaks at 0.9 and then held there by a
  level comparator, which is what keeps a quieter patch from coming out louder
  (see its section: −1.5 to +0.6 dB over five input levels and the whole knob
travel).

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
  the init callback: `reso` builds its filter coefficients there. Switching a
  program never builds anything while sound is running.
* **Suspend and resume leave silence.** After a resume nothing comes out of
  stale state (measured: 0.0000 from all of the effect units). A unit that is suspended loses
  its tail rather than replaying it later.
* **No non-finite samples.** Each unit was hammered with 20 000 random
  parameter jumps across the full range of every control, mid-stream: no NaN and
  no infinity in the output. `reso` guards its mode bank — if a jump moves the
  modes so far under a ringing bank that it starts to run away, the bank is
  dropped instead of passing a NaN down the chain.
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
  those limits.

Measured with `tools/bench.cpp`: the same workload in every unit, nanoseconds per
audio frame on this host (x86, gcc 10.3, 20 s of audio, minimum of three
passes). One 64 frame block at 48 kHz is 1.333 ms, and these are a comparison
between units, not an M4 prediction: the gap is about two orders of magnitude.
The one figure that is not from this host is `bowed`'s 777 cycles per frame,
counted on the M4 from its own image.

**Busy** is a note held (a spread of tones for the effects); **idle** (`--idle`)
is the same unit with nothing to do — no key down, or digital silence in, which
is what the effect slots are handed between notes.

| Unit | Text + data | SRAM | Busy | Idle |
| --- | --- | --- | --- | --- |
| `noise` | 5.8 KB | 0.4 KB | 8.9 ns/frame | 8.4 ns |
| `resonator` | 5.0 KB | 0.4 KB | 20 ns/frame | 0.4 ns |
| `bowed` | 2.7 KB | 4.1 KB | 12 ns/frame | 1.6 ns |
| `reso` | 2.1 KB | 0.4 KB | 22 ns/frame | 2.6 ns |
| `wavefolder` | 1.3 KB | none | 16 ns/frame | 5.0 ns |
| `reflect` | 1.8 KB | 6.4 KB | 39 ns/frame | 1.9 ns |

An effect slot is handed digital silence between notes, and every unit here skips
a block whose input is all zeroes once its own state has run down: that is not an
approximation, every term of the sum is zero. It is where most of an effect's
cost goes — `reso` is 22 ns a frame with a note and 2.6 without one — and
`wavefolder`, which keeps no state at all, still writes the zeroes at 2.8 ns
rather than running the curve over them.

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
./scripts/build.sh noise reso     # just these
./scripts/build.sh --setup        # only fetch the SDK and toolchain
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

A named set is a file in `packs/`, one unit per line, and builds with
`./scripts/build.sh --pack live`: the five files the instrument gets, every
time, instead of a hand-picked list.

`.github/workflows/build.yml` builds all units on every push and uploads them as
an artifact. Cutting a release is pushing a tag: the same workflow builds the
units with gcc 5.4 and attaches the five packages to a release, creating it with
the commit list as its notes if it does not exist yet. A tag with a suffix
(`v0.2.0-beta`) makes a prerelease, a release published by hand gets its packages
the same way, and a second run for a tag that already has a release replaces the
packages on it rather than making another.

## Memory budgets

The build checks that every unit still fits the memory its slot gives it — 32 KB
for an oscillator, 6 KB for a modulation effect, 12 KB for a delay — and fails if
one grew past it.
`tools/check_budget.py` prints one line per unit:

```
noise        6680 /  32768 bytes of osc    memory  ok    libm: none
wavefolder   1292 /   6144 bytes of modfx  memory  ok    libm: none
reflect      8352 /  12288 bytes of delfx  memory  ok    libm: none
```

The same tool runs `nm` over every unit file and fails if one of them links the
maths library. That is the quiet sort of regression: one `cosf` in a loop costs a
few hundred cycles per sample, and it drags newlib's reentrancy struct into a
slot that has 6 or 32 KB in total. `bowed` lost 4 KB of code and half its cost to
that lesson, and nothing here has needed an allowance since: the polynomials in
`units/common/arcmath.h` cover the exponentials, square roots and sines.

`tools/cycles.py` is the other half of that: it prices a range of any unit's
disassembly with the Cortex-M4's instruction timings, which is where `bowed`'s
cycles come from. The ELFs are in the CI artifact for exactly that.

What each unit file links, from `nm` over the ELFs CI builds:

| Unit | Symbols from outside the unit | Undefined |
| --- | --- | --- |
| `bowed` | none | none |
| `noise` | `___osc_mcu_hash_veneer`, `___osc_white_veneer` | none |
| `reflect` | none | none |
| `reso` | none | none |
| `resonator` | `___osc_mcu_hash_veneer` | none |
| `wavefolder` | none | none |

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
  modfx unit gets only the two knobs (`TIME`, `DEPTH`). There are no sub-menus in
  the format: parameter names and
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
* The mkI gives an oscillator slot 32 KB, a modfx slot 6 KB and a delfx slot
  12 KB of code space; all six units stay inside those budgets, and none of them
  links the maths library.

## License

BSD 3-Clause, see [LICENSE](LICENSE). The Makefiles, linker scripts and unit
entry templates come from the logue SDK, Copyright (c) 2018, KORG INC., under
the same license.
