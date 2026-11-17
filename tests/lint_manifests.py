#!/usr/bin/env python3
"""Check every manifest against the SDK's rules and against the unit's code.

The packer accepts a manifest that lies; the instrument then shows the wrong
number of knobs. These are the mistakes that have actually happened here.
"""
import glob
import json
import re
import sys

LIMITS = {"osc": 6, "modfx": 2, "delfx": 4, "revfx": 4}
problems = []

for path in sorted(glob.glob("units/*/manifest.json")):
    name = path.split("/")[1]
    m = json.load(open(path))
    h = m.get("header", m)
    module = h.get("module")
    params = h.get("params", [])
    if module not in LIMITS:
        problems.append("%s: unknown module %r" % (name, module))
        continue
    if h.get("num_param") != len(params):
        problems.append("%s: num_param %r but %d params" %
                        (name, h.get("num_param"), len(params)))
    if len(params) > LIMITS[module]:
        problems.append("%s: %d params, %s allows %d" %
                        (name, len(params), module, LIMITS[module]))
    for p in params:
        if len(p) != 4 or p[1] > p[2]:
            problems.append("%s: bad parameter descriptor %r" % (name, p))
    if h.get("dev_id", 0) == 0:
        problems.append("%s: dev_id 0, which the SDK reserves for invalid" % name)
    if not re.match(r"^\d+\.\d+-\d+$", str(h.get("version", ""))):
        problems.append("%s: version %r is not x.y-z" % (name, h.get("version")))

    # The knobs the manifest promises have to be the ones the code reads.
    src = open("units/%s/src/%s.cpp" % (name, name), errors="ignore").read()
    wanted = {"osc": 6, "modfx": 2, "delfx": 4, "revfx": 4}[module]
    if module == "osc":
        if len(set(re.findall(r"k_user_osc_param_id[1-6]", src))) != 6:
            problems.append("%s: not all six oscillator parameters are read" % name)
    elif module == "modfx":
        if len(set(re.findall(r"k_user_modfx_param_\w+", src))) != 2:
            problems.append("%s: not both modulation parameters are read" % name)
    else:
        # The third parameter of a delay or reverb slot is reserved by the API.
        for p_name in ("time", "depth", "shift_depth"):
            if "k_user_%s_param_%s" % (module, p_name) not in src:
                problems.append("%s: %s is never read" % (name, p_name))

if problems:
    for p in problems:
        print("   " + p)
    sys.exit(1)
print("manifests: ok")
