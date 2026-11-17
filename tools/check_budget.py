#!/usr/bin/env python3
"""Fail if a unit no longer fits the memory its slot gives it.

The linker would refuse a unit that overflows, but only for the sizes the
prologue knows about, and only at link time of that unit: this checks them all,
in one place, with the numbers a slot actually hands out, and keeps the .sdram
section (which lives in the reverb's 2.4 MB, not in the 12 KB) out of the sum.

    tools/check_budget.py <size-tool> [elf ...]
"""
import glob
import json
import os
import subprocess
import sys

# What the NTS-1 gives a unit, from the platform's linker scripts.
LIMIT = {"osc": 32 * 1024, "modfx": 6 * 1024, "delfx": 12 * 1024, "revfx": 12 * 1024}

size_tool = sys.argv[1]
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
    state = "ok" if used <= limit else "OVER"
    print("%-10s %6d / %6d bytes of %s memory  %s" %
          (unit, used, limit, manifest["module"], state))
    if used > limit:
        problems.append("%s is %d bytes over its %d byte %s budget"
                        % (unit, used - limit, limit, manifest["module"]))

for p in problems:
    print("   " + p)
sys.exit(1 if problems else 0)
