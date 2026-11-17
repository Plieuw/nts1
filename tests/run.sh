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
for u in noise resonator lpg reso elements barber irbody; do
  case $u in
    noise|resonator)         kind=0 ;;
    lpg|reso|elements)       kind=1 ;;
    barber)                  kind=2 ;;
    irbody)                  kind=3 ;;
  esac
  "$CXX" -O2 -std=c++11 -o "$OUT/$u" tests/harness.cpp \
    -Itests/host -I"units/$u/src" -I"units/$u/inc" \
    -DUNITKIND=$kind -DUNITFILE="\"$PWD/units/$u/src/$u.cpp\""

  # The knobs do not all run 0..1023 and there are two, four or six of them:
  # take both from the manifest the unit ships with.
  # A modulation effect declares no edit parameters on the NTS-1 but still has
  # its two knobs at run time, so it gets two.
  case $u in resonator) PERMODEL="--permodel 17" ;; *) PERMODEL="" ;; esac
  KNOBS=$(python3 -c "
import json
module = json.load(open('units/$u/manifest.json'))['header']['module']
n = len(json.load(open('units/$u/manifest.json'))['header']['params'])
print(8 if module == 'osc' else (n if n else 2))")
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

python3 tests/lint_manifests.py || status=1
exit $status
