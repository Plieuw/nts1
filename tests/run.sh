#!/bin/sh
# Builds every unit against tests/host and checks it against tests/golden.
#
#   tests/run.sh            compare against the recorded metrics
#   tests/run.sh --golden   rewrite the goldens (only when a change is intended;
#                           look at the diff before committing it)
set -e
cd "$(dirname "$0")/.."
CXX=${CXX:-g++}
OUT=${OUT:-/tmp/nts1-tests}
mkdir -p "$OUT"

status=0
for u in noise resonator bowed reso wavefolder reflect; do
  case $u in
    noise|resonator|bowed)     kind=0 ;;
    reso|wavefolder)           kind=1 ;;
    reflect)                   kind=2 ;;
  esac
  "$CXX" -O2 -std=c++11 -o "$OUT/$u" tests/harness.cpp \
    -Itests/host -I"units/$u/src" -I"units/$u/inc" \
    -DUNITKIND=$kind -DUNITFILE="\"$PWD/units/$u/src/$u.cpp\""

  # The knobs do not all run 0..1023 and there are two, four or six of them:
  # take both from the manifest the unit ships with.
  # A modulation effect declares no edit parameters on the NTS-1 but still has
  # its two knobs at run time, so it gets two.
  case $u in resonator) PERMODEL="--permodel 17" ;; bowed) PERMODEL="--permodel 5" ;; *) PERMODEL="" ;; esac
  KNOBS=$(python3 -c "
import json
module = json.load(open('units/$u/manifest.json'))['header']['module']
n = len(json.load(open('units/$u/manifest.json'))['header']['params'])
# A delay unit on the mkI has time, depth and shift-depth (index 3), so it is
# swept with four: the reserved one in between does nothing.
print(8 if module == 'osc' else (n if n else (4 if module == 'delfx' else 2)))")
  RANGES=$(python3 -c "
import json
p = json.load(open('units/$u/manifest.json'))['header']['params']
# A declared parameter arrives scaled to its own range, and a bipolar one
# (-100..100) arrives as 0..200, so its raw range starts at zero.
out = []
for x in p:
    lo, hi = int(x[1]), int(x[2])
    out.append('0:%d' % (2 * hi) if lo < 0 else '%d:%d' % (lo, hi))
print(','.join(out))")
  "$OUT/$u" --knobs "$KNOBS" --ranges "$RANGES" $PERMODEL > "$OUT/$u.txt"

  if [ "$1" = "--golden" ]; then
    cp "$OUT/$u.txt" "tests/golden/$u.txt"
    echo "$u: golden written"
  elif python3 tests/compare.py "tests/golden/$u.txt" "$OUT/$u.txt"; then
    echo "$u: ok"
  else
    echo "$u: FAILED"
    status=1
  fi
done

# The legato glide: a key that arrives while the previous one is still down has
# to move the pitch instead of restarting the note.
for u in noise resonator bowed; do
  "$CXX" -O2 -std=c++11 -o "$OUT/legato_$u" tests/legato.cpp \
    -Itests/host -I"units/$u/src" \
    -DUNITKIND=0 -DUNITFILE="\"$PWD/units/$u/src/$u.cpp\"" || { status=1; continue; }
  # Twice: the key overlapping, and the key released 5 ms before the next one.
  # Only the resonator slurs in the second case: a bow that is lifted and a noise
  # band that is re-armed are new articulations by construction, but a string
  # that is still ringing a few milliseconds later is still the same string.
  ok=1
  "$OUT/legato_$u" --unit "$u" > "$OUT/legato_$u.txt" || ok=0
  python3 tests/legato_check.py "$u" "$OUT/legato_$u.txt" || ok=0
  if [ "$u" = resonator ]; then
    "$OUT/legato_$u" --unit "$u" --split > "$OUT/legato_${u}_split.txt" || ok=0
    python3 tests/legato_check.py "$u" "$OUT/legato_${u}_split.txt" || ok=0
  fi
  if [ "$ok" = 1 ]; then
    echo "legato $u: ok"
  else
    echo "legato $u: FAILED"
    status=1
  fi
done

python3 tests/lint_manifests.py || status=1
exit $status
