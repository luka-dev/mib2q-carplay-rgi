#!/usr/bin/env python3
"""Compare two `stereo_upmix_bench --mask-selftest` outputs (host scalar vs QEMU NEON)."""
import re
import sys


def load(path):
    rows = {}
    for line in open(path, errors="replace"):
        m = re.match(r"mask_selftest control=(\d+) (eligibility|presence)((?: -?\d+\.\d+)+)", line.strip())
        if m:
            rows[(int(m.group(1)), m.group(2))] = [float(v) for v in m.group(3).split()]
    return rows


a, b = load(sys.argv[1]), load(sys.argv[2])
keys = sorted(set(a) & set(b))
if not keys:
    sys.exit("no matching rows")
worst = 0.0
for key in keys:
    x, y = a[key], b[key]
    if len(x) != len(y):
        sys.exit("row length mismatch at %r" % (key,))
    d = max(abs(p - q) for p, q in zip(x, y))
    worst = max(worst, d)
print("%d rows compared (%d controls), missing in one side: %d" % (
    len(keys), len({k[0] for k in keys}), len(set(a) ^ set(b))))
print("max |delta| = %.6f" % worst)
tolerance = float(sys.argv[3]) if len(sys.argv) > 3 else 1e-3
sys.exit(0 if worst <= tolerance else 1)
