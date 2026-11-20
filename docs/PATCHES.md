# Choosing a sound

The units are instruments, not presets, so this is a map rather than a patch
bank: what each model is for, and where to start. `Edit` numbers count from 1 on
the display unless noted.

## resonator: which of the seventeen

| Model | Sounds like | Start with | Reach for it when |
| --- | --- | --- | --- |
| string | a plucked low string, long | `Strike` 30, `Damp` 20, B 60 | you want a bass line that rings |
| membrane | a drum skin with pitch | `Strike` 70, `Pos` 25 | toms, taiko, a kick with tone |
| bell | inharmonic, high partials loud | `Strike` 60, B 30 | the note should be a colour, not a pitch |
| kalimba | short tine, mostly attack | `Strike` 50, B 70 | plucks that sit in a busy mix |
| glass | thin, brittle, long upper band | `Strike` 75, B 80 | high, glassy arpeggios |
| marimba | wooden bar with a tuned second mode | `Strike` 45, `Pos` 40 | mallet melodies |
| xylophone | brighter and shorter than marimba | `Strike` 60, `Pos` 30 | fast, dry lines |
| vibraphone | long, soft, even partials | `Strike` 25, B 45 | long notes that bloom |
| piano | stretched partials, dies from the top | `Strike` 55, `Damp` 35 | pads with a keyboard feel |
| steel drum | two strong modes, a bright skirt | `Strike` 65, `Pos` 20 | Caribbean melody |
| gong | very long, inharmonic, low | `A` high (long), `Strike` 40 | single hits that keep going |
| bowl | long, round, almost a chord | `A` 70, B 40 | drones, singing bowl |
| triangle | very short, very bright | `Strike` 80, B 90 | accents |
| woodblock | almost all attack | `Strike` 70, `Damp` 60 | percussion, clicks |
| log drum | short, hollow, a clear pitch | `Strike` 60, `Pos` 60 | tribal patterns |
| tabla | short with a bend at the start | `Strike` 75, `Pos` 35 | hand drums |
| reed | narrow, buzzy, sustains oddly | `A` 60, B 30 | a drone under everything |

`Strike` is the hammer, `Pos` is where it hits (a mode with a node there stays
quiet), `Damp` closes a low pass on the body from 24 kHz down to 500 Hz, `A` is
the decay time and `B` is brightness.

## bowed: which of the five

| Model | Sounds like | Bow | Rosin | Attack |
| --- | --- | --- | --- | --- |
| string | a bowed cello, clear pitch | 60 | 45 | 35 |
| metal | a bowed cymbal or saw blade | 45 | 25 | 70 |
| pipe | a breathy flute, noisy | 35 | 75 | 15 |
| hum | a voice without words | 50 | 55 | 50 |
| glass | a bowed wine glass, thin | 30 | 20 | 80 |

Low `Bow` and slow `Attack` is a soft swell; high `Bow` with `Rosin` near 0 is a
clean, almost synthetic sustained tone. The LFO on shape moves the bow.

## noise: which algorithm

For a bed: `pink`. For a texture that moves: `velvet` with `Grain` low. For
percussion: `crackle` or `step` with a short `Trans`. For a pitched noise
instrument: `tuned` with `Keytrk` at 100.

## reflect: small room, big room

A is the size and B is how long the reflections keep coming back, so the two
ends of the knobs are the two obvious sounds and everything between moves
smoothly.

| Want | A (size) | B (decay) | DEL + B (mix) |
| --- | --- | --- | --- |
| a box around the note | 20 | 30 | 35 |
| a small room, audible | 45 | 55 | 45 |
| a long, dark space under a pad | 70 | 80 | 60 |
| just the walls: a slap | 30 | 0 | 55 |
| barely there, on a dry bass | 60 | 40 | 20 |

With B at 0 it is four walls and no tail, which is the setting to reach for when
the sound is already busy. With B high the unit keeps the dry signal, so the
note stays in front of the room instead of dissolving into it.

## Combinations that work

* `noise` (velvet) into `reso`: a wind instrument with a moving pitch.
* `resonator` (bell) into `wavefolder`: harmonics on top of the strike, and the
  bell keeps ringing under them.
* `bowed` (metal) into `wavefolder` with FOLD low: a string with an edge it does
  not have on its own.
* `noise` (pluck) into `reso` into `wavefolder`: a plucked string whose body
  opens up as it decays.
* `resonator` (woodblock or kalimba) into `reflect` with A low and B low: the
  click gets its own room, which is what makes a small tuned block sound placed
  rather than pasted.
* `bowed` into `reflect` with A at 70 and B at 80: a sustained note inside a
  space, still with a stock reverb after it.
* `noise` (white) into `reflect` with the mix high and B near 1: a noise wash
  that decays through the room's own reflection train instead of a filter.
