#!/usr/bin/env python3
"""Build an 'oracle master' from AlbumDB stems: one instrument hard-panned, everything else dead-centre.

On such a file the structural decomposition is exact (p = -1, coherence 1 in the instrument's
cells, p = 0 elsewhere), so the renderer's transfer and protection can be judged without any
estimation error -- offline through scene_upmix.py now, and in the car by playing the stereo
file from the phone through the real hook (spec section 13.5).

    python3 scene_oracle_master.py --song 01 --stem guitar --pan -1 --seconds 60 --out DIR

Writes DIR/oracle_albumdb-01_guitar_L.wav (stereo, 48 kHz, int16) and prints the per-head rear
share / relative swing of scene_upmix.py at alpha 0, 0.7 and 0.9 plus the passive matrix.
"""
import argparse
import json
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import upmix_behavior_tests as T  # noqa: E402

SR = 48000
CACHE = os.path.expanduser("~/Desktop/AUDI_2/upmix-data/q4-albumdb-cache-a")


def load(song):
    meta = json.load(open(os.path.join(CACHE, f"albumdb-{song}.json")))
    a = np.load(os.path.join(CACHE, f"albumdb-{song}.npy"), mmap_mode="r")
    heads = {meta["classes"][h]: i + 1 for i, h in enumerate(meta["stored_heads"])}
    return meta, a, heads


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--song", default="01")
    ap.add_argument("--stem", default="guitar")
    ap.add_argument("--pan", type=float, default=-1.0, help="amplitude pan of the chosen stem, -1 = hard left")
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--out", default=os.path.join(HERE, "oracle"))
    ap.add_argument("--alphas", default="0,0.7,0.9")
    ns = ap.parse_args(argv)
    meta, a, heads = load(ns.song)
    if ns.stem not in heads:
        print("heads:", list(heads)); return 2
    n = a.shape[0]
    win = int(ns.seconds * SR)
    # loudest window by total energy over the heads
    hop = 5 * SR
    best, start = -1.0, 0
    for s in range(0, max(1, n - win), hop):
        e = float(np.abs(a[s:s + win, 1:, :].astype(np.float32)).sum())
        if e > best:
            best, start = e, s
    seg = a[start:start + win].astype(np.float64)       # [win, 1+P, 2]
    mono = {k: seg[:, i, :].mean(axis=1) for k, i in heads.items()}   # each head collapsed to mono
    gl, gr = np.sqrt((1 - ns.pan) / 2), np.sqrt((1 + ns.pan) / 2)     # constant-power pan
    stereo = {}
    for k, m in mono.items():
        if k == ns.stem:
            stereo[k] = np.stack([gl * m, gr * m])
        else:
            stereo[k] = np.stack([m, m]) / np.sqrt(2)
    mix = sum(stereo.values())
    peak = np.abs(mix).max()
    k = 0.7 / peak
    mix *= k
    for key in stereo:
        stereo[key] *= k
    os.makedirs(ns.out, exist_ok=True)
    tag = f"oracle_albumdb-{ns.song}_{ns.stem}_{'L' if ns.pan < 0 else 'R' if ns.pan > 0 else 'C'}"
    src = os.path.join(ns.out, tag + ".wav")
    T.write_wav(src, mix)
    print("wrote", src, "| heads:", ", ".join(heads), "| window starts at %.1f s" % (start / SR))
    conds = [("matrix", ["python3", os.path.join(HERE, "baseline_upmix.py"), "--mode", "matrix"])]
    for al in ns.alphas.split(","):
        conds.append((f"scene a{al}", ["python3", os.path.join(HERE, "scene_upmix.py"), "--alpha", al]))
    print(f"{'condition':12s} " + " ".join(f"{h[:8]:>14s}" for h in stereo) + "   rear_dBFS")
    for name, cmd in conds:
        dst = os.path.join(ns.out, f"{tag}.{name.replace(' ', '_')}.6ch.wav")
        subprocess.run(cmd + [src, dst], check=True, stdout=subprocess.DEVNULL)
        out6 = T.read_wav6(dst)
        cells = []
        for h, st in stereo.items():
            share, w, dt = T.rear_share(st, mix - st, out6)
            m = T.metrics(share, w, dt, np.ones(win, bool))
            med = m["median_rear"]; rel = (m["range_1s"] / med) if med and med > 0.02 else float("nan")
            cells.append(f"{med:6.3f}/{rel:5.2f}")
        rms = 20 * np.log10(np.sqrt(((out6[4] ** 2 + out6[5] ** 2) / 2).mean()) + 1e-9)
        print(f"{name:12s} " + " ".join(f"{c:>14s}" for c in cells) + f"   {rms:6.1f}")
    print("cells = median rear share / relative swing (range_1s / median)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
