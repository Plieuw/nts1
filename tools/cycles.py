#!/usr/bin/env python3
"""Cycle cost of a stretch of compiled ARM code, for the Cortex-M4 in the NTS-1.

There is no instrument here, so the number has to come out of the binary: this
reads the disassembly of a range of addresses and applies the Cortex-M4 pipeline
model to every instruction on it.

    tools/cycles.py <elf> --range 0x20000322-0x2000032c ...

The ranges are read off the disassembly, which is the part that needs a person:
for bowed the per-sample path is the loop test, the voice setup, the epilogue,
and per active voice the held/active blocks plus the callee. The numbers in the
README come from running this over the ELF that CI attaches to its artifact.

Conditional branches are priced as taken, which is the pessimistic side: it is
right for the loop backs and the fall-through of a rarely taken branch costs a
cycle less than counted.

The model (Cortex-M4 TRM, instruction timings):

    1 cycle     almost everything: MOV/ADD/SUB/CMP/MUL/MLA/LSL, and the FPU's
                VMUL/VADD/VSUB/VFMA/VNMUL/VMOV/VCMP, which are single cycle
    2 cycles    every load and store, including VLDR and VSTR: the access is
                one cycle and the register write-back another
    3 cycles    a call (BL) and the return, and a taken branch that leaves the
                straight line; 2 for a taken branch that stays in the loop

The code itself runs from SRAM, which has no wait states, so the model does not
need a flash accelerator assumption. What it cannot see is pipelining: the M4 can
issue an independent instruction while a load completes, so a straight sum over
loads is an upper bound. That is the right direction for the question this
answers — does a block still fit.
"""
import argparse
import re
import subprocess
import sys

LOAD = ("ldr", "str", "ldrb", "strb", "ldrh", "strh", "ldrd", "strd", "vldr",
        "vstr", "ldmia", "ldmdb", "stmia", "stmdb", "pop", "push")


def cost(mnemonic, taken):
    m = mnemonic.lower()
    if m.startswith(("bl", "blx", "bx")):
        return 3
    if m.startswith("b"):
        return 3 if taken else 1
    if m in LOAD or m.startswith(("ldr", "str", "vldr", "vstr")):
        return 2
    return 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--function", default="_hook_cycle")
    ap.add_argument("--objdump", default="arm-none-eabi-objdump")
    ap.add_argument("--range", action="append", default=[],
                    help="START-END in hex, END exclusive; repeatable")
    ap.add_argument("--label", default="")
    args = ap.parse_args()

    out = subprocess.run([args.objdump, "-d", "--no-show-raw-insn", args.elf],
                         capture_output=True, text=True).stdout
    # the symbol's instructions only
    start = out.index("<%s>:" % args.function)
    end = out.find("\n\n", start)
    body = out[start:end if end > 0 else len(out)]

    insns = []
    for line in body.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\t(\S+)\s*(.*)", line)
        if m:
            insns.append((int(m.group(1), 16), m.group(2), m.group(3).strip()))

    ranges = []
    for r in args.range:
        a, b = r.split("-")
        ranges.append((int(a, 16), int(b, 16)))

    total = 0
    counted = 0
    for lo, hi in ranges:
        cycles = 0
        n = 0
        for addr, mn, ops in insns:
            if not (lo <= addr < hi):
                continue
            n += 1
            taken = mn.lower().startswith("b") or mn.lower() in ("cbz", "cbnz")
            cycles += cost(mn, taken)
        counted += n
        total += cycles
        print("  %08x-%08x  %3d instructions  %4d cycles" % (lo, hi, n, cycles))
    print("%s: %d instructions, %d cycles" % (args.label or args.function, counted, total))
    return total


if __name__ == "__main__":
    sys.exit(0 if main() >= 0 else 1)
