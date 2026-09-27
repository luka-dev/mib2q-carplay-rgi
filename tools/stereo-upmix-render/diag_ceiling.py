#!/usr/bin/env python3
"""astra review 10, E: the references through the EXACT evaluation path of sep_net.evaluate():
the mixture itself, the identity mask through stft/istft, and the bounded real mask (IRM, mixture
phase) -- median SI-SDR and vocal leak over the same 12 DEV excerpts.  CPU is fine."""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN, sep_net as SN

cache = sys.argv[1]
torch = SN._torch(); dev = "cpu"
split = MN.load_split(cache)
rows = {"mixture direct": [], "identity mask": [], "irm": []}; leaks = []
for p in [os.path.join(cache, t + ".npy") for t in split["dev"]][:12]:
    N = MN.npy_header(p)[0][2]; n = min(int(20 * SN.SR), N)
    s = MN.dev_excerpt(p, n) if N > n else 0
    w = torch.as_tensor(MN.expand_slots(MN.read_window(p, s, n)).astype(np.float32) / 32768)
    mix = w.sum(0)[None]; elig = w[list(SN.ELIGIBLE_SLOTS)].sum(0)[None]; voc = (w[0] + w[1])[None]
    X = SN.stft(torch, mix); E = SN.stft(torch, elig)
    irm = (E.abs() / (X.abs() + 1e-7)).clamp(0.0, 1.0)
    ests = {"mixture direct": mix, "identity mask": SN.istft(torch, X, n), "irm": SN.istft(torch, X * irm, n)}
    for k, e in ests.items():
        rows[k].append(float(SN.si_sdr(torch, e, elig).mean()))
    Xv = SN.stft(torch, voc); Xe = SN.stft(torch, ests["irm"])
    vcell = (Xv.abs() ** 2) > 10 * ((X - Xv).abs() ** 2 + 1e-9)
    leaks.append(float(((Xe.abs() ** 2) * vcell).sum() / ((Xv.abs() ** 2) * vcell).sum().clamp(min=1e-9)))
    print(os.path.basename(p)[:12], " ".join(f"{k} {v[-1]:+.2f}" for k, v in rows.items()), f"irm leak {leaks[-1]:.3f}", flush=True)
print("MEDIAN " + " | ".join(f"{k} {np.median(v):+.2f} dB" for k, v in rows.items()) + f" | irm leak {np.median(leaks):.3f}")
