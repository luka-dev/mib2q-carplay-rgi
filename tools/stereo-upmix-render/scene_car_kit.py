#!/usr/bin/env python3
"""Blind listening kit for the car: stereo sources x renderer conditions -> headerless 6-ch S16 files
for `qsa_play` on the head unit (plus the 6-ch WAVs), file names blinded per source, key sealed in KEY.txt.

  python3 scene_car_kit.py OUT_DIR name=stereo.wav [name=stereo.wav ...]

Conditions (edit CONDS): passive matrix, scene alpha 0.7 (+12 ms rear), scene alpha 0.7 rear-lead
(front delayed 15 ms), scene alpha 0.9, scene alpha 0.7 with place width 0.15, scene alpha 0 (side layer + air only).  Also writes
`rearcheck.s16` (tone in Ls/Rs only): silence from it means the unit's SAD is not 0xfc and the
session is invalid (LANDMINES: qsa_play sets no SAD, play CarPlay once first, then pause it)."""
import os, sys, random, subprocess, numpy as np, wave
HERE = os.path.dirname(os.path.abspath(__file__)); PY = sys.executable
CONDS = {
    "matrix":        [PY, os.path.join(HERE, "baseline_upmix.py"), "--mode", "matrix"],
    "scene07":       [PY, os.path.join(HERE, "scene_upmix.py"), "--alpha", "0.7"],
    "scene07_lead":  [PY, os.path.join(HERE, "scene_upmix.py"), "--alpha", "0.7", "--rear-delay-ms", "-15"],
    "scene09":       [PY, os.path.join(HERE, "scene_upmix.py"), "--alpha", "0.9"],
    "scene07_s15":   [PY, os.path.join(HERE, "scene_upmix.py"), "--alpha", "0.7", "--sigma-min", "0.15"],
    "scene00_side":  [PY, os.path.join(HERE, "scene_upmix.py"), "--alpha", "0.0"],
}
def wav_to_s16(src, dst):
    b = open(src, "rb").read(); i = b.index(b"data") + 8
    open(dst, "wb").write(b[i:])
    return (len(b) - i) // 12
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
rng = random.Random(20261008); key = []
for arg in sys.argv[2:]:
    name, src = arg.split("=", 1)
    letters = list("ABCDEF")[:len(CONDS)]; rng.shuffle(letters)
    for (cond, cmd), letter in zip(CONDS.items(), letters):
        w6 = os.path.join(out, f"{name}_{letter}.6ch.wav")
        subprocess.run(cmd + [src, w6], check=True, capture_output=True)
        n = wav_to_s16(w6, os.path.join(out, f"{name}_{letter}.s16"))
        key.append(f"{name}_{letter} = {cond}  ({n / 48000:.1f} s)")
        print(f"{name}_{letter}.s16  {n / 48000:.1f} s", flush=True)
# rear-only check: 440 Hz tone -20 dBFS in Ls/Rs, nothing elsewhere
t = np.arange(48000 * 5) / 48000.0; tone = (0.1 * np.sin(2 * np.pi * 440 * t) * 32767).astype("<i2")
x = np.zeros((len(t), 6), "<i2"); x[:, 4] = tone; x[:, 5] = tone
open(os.path.join(out, "rearcheck.s16"), "wb").write(x.tobytes())
open(os.path.join(out, "KEY.txt"), "w").write("\n".join(key) + "\n")
print("key sealed in KEY.txt (open after scoring)")
