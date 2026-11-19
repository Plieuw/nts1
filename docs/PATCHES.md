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

## irbody: which body

`violin` for a boxy midrange, `cello` for the same a size up, `piano` for a large
soundboard, `guitar` for a bright box, `wood` for a knock, `steel` for a plate,
`clarinet` for a tube, `drum` for a shell. The A knob is a continuum: park it
between two names to hear the in-between body. `B` is damping, SHIFT is the mix.

## Combinations that work

* `bowed` (string) into `lpg` with the gate slow: a pad that opens with the note.
* `noise` (velvet) into `reso`: a wind instrument with a moving pitch.
* `resonator` (bell) into `barber` into a stock reverb: the bell smears.
* `bowed` (metal) into `elements`: a bowed sheet that changes shape as it rings.
* `resonator` (woodblock) into `irbody` (wood): percussion with a body.
