"""Check one unit's legato measurement.

    tests/legato_check.py <unit> <file>

A slur has three properties worth testing, and none of them is a golden: the new
note is reached, the pitch was still moving on the way there (so it glides
instead of jumping), and the sound did not restart under it. A noise source has
no period to read, so for it the same thing is measured as energy at the two
notes.

    the note numbers are fixed by tests/legato.cpp: C4 -> G4.
"""
import sys

PERIOD_C4 = 48000.0 / 261.63
PERIOD_G4 = 48000.0 / 392.00

unit, path = sys.argv[1], sys.argv[2]
vals = {}
for line in open(path):
    parts = line.split()
    if len(parts) == 2:
        vals[parts[0]] = float(parts[1])

problems = []
if unit == "noise":
    # The tuned algorithm is band-passed noise: the period is not readable, but
    # the band has moved from one note to the other.
    if vals["g4_after"] < 1.5 * vals["c4_after"]:
        problems.append("no energy at G4 after the slur (%.5f vs %.5f at C4)"
                        % (vals["g4_after"], vals["c4_after"]))
else:
    if abs(vals["period_before"] - PERIOD_C4) > 0.03 * PERIOD_C4:
        problems.append("C4 is out of tune before the slur (%.1f, want %.1f)"
                        % (vals["period_before"], PERIOD_C4))
    if abs(vals["period_after"] - PERIOD_G4) > 0.03 * PERIOD_G4:
        problems.append("G4 is out of tune after the slur (%.1f, want %.1f)"
                        % (vals["period_after"], PERIOD_G4))
    if not (PERIOD_G4 + 4.0 < vals["period_10ms"] < vals["period_before"] - 4.0):
        problems.append("the pitch did not glide (%.1f at 10 ms, between %.1f and %.1f)"
                        % (vals["period_10ms"], PERIOD_G4, vals["period_before"]))

if vals["level_ratio"] < 0.5:
    problems.append("the sound restarted under the slur (level x%.2f)"
                    % vals["level_ratio"])

for p in problems:
    print("   " + p)
sys.exit(1 if problems else 0)
