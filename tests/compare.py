#!/usr/bin/env python3
"""Compare one measure run against its golden, with tolerances that catch a
change in the sound rather than the last decimal."""
import sys

BAND_TOL = 1.5      # dB per third-octave band
LEVEL_TOL = 0.15    # relative, for rms and peak
KNOB_MOVES = 2.0    # dB a knob that used to move must still move
RELEASE_TOL = 10.0  # dB a released note may ring louder than it was measured
TAIL_DB = -60.0     # ten seconds on, the tail with the knobs in the middle
TAIL_DB_TOP = -12.0  # ... and with the knobs at the top, where a bank rings longest
BOOT_RMS = 0.01     # audible with every parameter at zero, as a fresh program sends
BOOT_DB = -25.0     # an effect has to change the signal at those values, not pass it


def load(path):
    m = {}
    for line in open(path):
        parts = line.split()
        m[parts[0]] = [float(x) for x in parts[1:]]
    return m


def main(golden, current):
    g, c = load(golden), load(current)
    problems = []
    for key, gv in g.items():
        cv = c.get(key)
        if cv is None:
            problems.append("%s missing" % key)
            continue
        if key.startswith("bands"):
            worst = max((abs(a - b) for a, b in zip(gv, cv)), default=0)
            if worst > BAND_TOL:
                problems.append("spectrum moved %.1f dB (limit %.1f)" % (worst, BAND_TOL))
        elif key in ("rms", "rms_tail", "rms_late", "peak"):
            rel = abs(cv[0] - gv[0]) / max(gv[0], 1e-9)
            if rel > LEVEL_TOL:
                problems.append("%s %.4f, was %.4f" % (key, cv[0], gv[0]))
        elif key in ("nan", "fuzznan"):
            if cv[0] != 0:
                problems.append("%s %d" % (key, int(cv[0])))
        elif key in ("corr", "corr_hi"):
            if abs(cv[0] - gv[0]) > 0.04:
                problems.append("channel correlation %.3f, was %.3f" % (cv[0], gv[0]))
        elif key == "release_db":
            if cv[0] > gv[0] + RELEASE_TOL:
                problems.append(
                    "300 ms after note-off the unit is %.1f dB below the held level, was %.1f"
                    % (cv[0], gv[0]))
        elif key in ("tail_db", "tail_db_top"):
            limit = TAIL_DB if key == "tail_db" else TAIL_DB_TOP
            if cv[0] > limit:
                problems.append(
                    "%s %.1f dB ten seconds after the input stopped (limit %.0f)"
                    % (key, cv[0], limit))
        elif key == "boot_rms":
            if cv[0] < BOOT_RMS:
                problems.append(
                    "silent with every parameter at zero (%.4f, needs %.2f): a "
                    "freshly installed unit must sound like its own start values"
                    % (cv[0], BOOT_RMS))
        elif key == "boot_db":
            if cv[0] < BOOT_DB:
                problems.append(
                    "does nothing with every parameter at zero (%.1f dB, needs "
                    "%.0f): a fresh program sends zeroes, not the unit's values"
                    % (cv[0], BOOT_DB))
        elif key == "shift_db":
            # The level of the difference between the two ends of the delay
            # page's third knob, against the signal at the low end: a knob that
            # does nothing measures -300 dB.
            if cv[0] < -45.0:
                problems.append(
                    "the shift knob changes the signal by only %.1f dB (needs -45)"
                    % cv[0])
        elif key == "fuzzpeak":
            if cv[0] > 0.99:
                problems.append("fuzz peak %.3f clips" % cv[0])
        elif key.startswith("knob") and gv[0] > 3.0:
            if cv[0] < KNOB_MOVES:
                problems.append("%s only %.1f dB (was %.1f)" % (key, cv[0], gv[0]))
    if problems:
        for p in problems:
            print("   " + p)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
