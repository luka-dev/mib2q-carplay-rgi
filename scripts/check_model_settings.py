#!/usr/bin/env python3
"""Every knob a models/*/settings.json records must match the shipped startup script.

usage: check_model_settings.py deploy/smartphone_integrator/carplay_startup.sh models/*/settings.json
A knob with value null must be unset (not exported) there.  Exit 1 on any drift.
"""
import json
import re
import sys

startup = sys.argv[1]
exported = {}
for line in open(startup):
    m = re.match(r'\s*export\s+(AUDIO_[A-Z0-9_]+)=(.*)$', line)
    if m:
        exported[m.group(1)] = m.group(2).strip().strip('"')

drift = []
for path in sys.argv[2:]:
    for key, knob in json.load(open(path))["knobs"].items():
        want = knob["value"]
        have = exported.get(key)
        if want != have:
            drift.append("%s: %s records %r, %s exports %r" % (key, path, want, startup, have))
if drift:
    print("model settings drift:\n  " + "\n  ".join(drift), file=sys.stderr)
    sys.exit(1)
print("model settings match %s (%d knobs)" % (startup, sum(len(json.load(open(p))["knobs"]) for p in sys.argv[2:])))
