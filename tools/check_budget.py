#!/usr/bin/env python3
"""Fail if a unit no longer fits the memory its slot gives it, or links libm.

The linker would refuse a unit that overflows, but only for the sizes the
prologue knows about, and only at link time of that unit: this checks them all,
in one place, with the numbers a slot actually hands out, and keeps the .sdram
section (which lives in the reverb's 2.4 MB, not in the 12 KB) out of the sum.

    tools/check_budget.py <size-tool> [elf ...]

Two things it refuses. A unit that has grown past its slot's memory — the linker
would catch that, but only when that unit is built, and only against the sizes
the prologue knows. And a unit that has picked up a symbol from the maths
library. The second one is the quiet sort of regression: one `cosf` in a loop
costs a few hundred cycles per sample, and it also drags newlib's reentrancy
struct into a slot that has 6 or 12 KB in total. `bowed` lost 4 KB of code and
half its cost to that lesson; this is so it does not have to be learned twice.
"""
import glob
import json
import os
import subprocess
import sys

# What the NTS-1 gives a unit, from the platform's linker scripts.
LIMIT = {"osc": 32 * 1024, "modfx": 6 * 1024, "delfx": 12 * 1024, "revfx": 12 * 1024}

# The maths library entries a unit can pick up, by their names in newlib. The
# leading underscores and the __kernel_/__ieee754_ prefixes are stripped before
# comparing, so cosf, _cosf and __kernel_cosf all land on "cos".
MATH = ("cos", "sin", "tan", "asin", "acos", "atan", "atan2", "exp", "log",
        "pow", "sqrt", "cbrt", "hypot", "fabs", "floor", "ceil", "fmod",
        "round", "trunc", "ldexp", "frexp", "scalbn", "remainder")
MATH_TAIL = ("", "f", "l")

# No unit gets an allowance any more: `irbody` was the last one, and it builds its
# levels and trims from the polynomials in units/common/arcmath.h now. If a unit
# ever needs one, it goes here with the reason next to it and the exact symbols,
# never as a blanket exemption.
ALLOWED = {}


def libm_symbol(name):
  n = name.lstrip("_")
  for prefix in ("kernel_", "ieee754_"):
    if n.startswith(prefix):
      n = n[len(prefix):]
  return any(n == base + tail for base in MATH for tail in MATH_TAIL)


size_tool = sys.argv[1]
nm_tool = os.path.join(os.path.dirname(size_tool), "arm-none-eabi-nm")
elvs = sys.argv[2:] or sorted(glob.glob("units/*/build/*.elf"))
problems = []
for elf in elvs:
    unit = elf.split("/")[1]
    manifest = json.load(open("units/%s/manifest.json" % unit))["header"]
    limit = LIMIT[manifest["module"]]
    out = subprocess.run([size_tool, "-A", elf], capture_output=True, text=True).stdout
    # Only what the instrument actually loads: the build carries debug sections
    # that `size -A` happily counts, and .sdram lives in the reverb's own memory.
    counted = (".text", ".rodata", ".data", ".bss", ".eh_frame", ".init_array",
               ".fini_array", ".preinit_array", ".ARM.exidx")
    used = 0
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 2 or not parts[0].startswith("."):
            continue
        if not parts[0].startswith(counted):
            continue
        used += int(parts[1])
    symbols = subprocess.run([nm_tool, elf], capture_output=True, text=True).stdout
    seen = set()
    for line in symbols.splitlines():
        parts = line.split()
        if len(parts) >= 3 and libm_symbol(parts[2]):
            seen.add(parts[2])
    allowed = ALLOWED.get(unit, set())
    extra = seen - allowed
    if not seen:
        libm = "none"
    elif extra:
        libm = "LINKS " + " ".join(sorted(extra))
    else:
        libm = "%d allowed (%s)" % (len(seen), "ir_model.h")

    state = "ok" if used <= limit else "OVER"
    print("%-10s %6d / %6d bytes of %-6s memory  %-4s  libm: %s" %
          (unit, used, limit, manifest["module"], state, libm))
    if used > limit:
        problems.append("%s is %d bytes over its %d byte %s budget"
                        % (unit, used - limit, limit, manifest["module"]))
    if extra:
        problems.append("%s links %s from the maths library, which costs RAM as "
                        "well as cycles" % (unit, " ".join(sorted(extra))))

for p in problems:
    print("   " + p)
sys.exit(1 if problems else 0)
