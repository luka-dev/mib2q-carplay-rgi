#!/usr/bin/env python3
"""astra 11, before sep8: (1) floor coverage - the fraction of the eligible stem's energy that lies BELOW the
relative floor, per band group, on training windows (if a substantial share of the eligible treble is under
the floor, lower it); (2) the scale factor for the new log-magnitude bundle = the old bundle's output-gradient
norm / the new bundle's, on the same random-init net and minibatches (so the unchanged 0.5 complex-L1 keeps
its relative influence).  CPU (CUDA_VISIBLE_DEVICES=-1).
  diag_lossfloor.py CACHE TEACHER_CACHE"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN, sep_net as SN

GROUPS = ((0, 11), (11, 43), (43, 128), (128, 342), (342, 598), (598, 1025))
NAMES = ("<250", "250-1k", "1-3k", "3-8k", "8-14k", ">14k")


def main():
    cache, tcache = sys.argv[1], sys.argv[2]
    torch = SN._torch(); torch.manual_seed(1); rng = np.random.default_rng(1)
    split = MN.load_split(cache)
    truth = [os.path.join(cache, t + ".npy") for t in split["train"]]
    pseudo = [os.path.join(tcache, f) for f in sorted(os.listdir(tcache)) if f.endswith(".npy") and os.path.exists(os.path.join(tcache, f[:-4] + ".json"))]
    tracks = (truth, pseudo)
    net = SN.BandSepNet(torch)
    batches = [SN.windows(torch, tracks, 8.0, 4, rng) for _ in range(4)]
    with torch.no_grad():
        lm = torch.cat([torch.log(SN.stft(torch, b[0]).abs() + 1e-5).flatten() for b in batches])
        SN.STATS["mean"], SN.STATS["std"] = float(lm.mean()), float(lm.std() + 1e-3)
    # (1) coverage
    for rel in (1e-3, 3e-3, 1e-2):
        below = np.zeros(len(GROUPS)); tot = np.zeros(len(GROUPS))
        for mix, elig in batches:
            X = SN.stft(torch, mix); E = SN.stft(torch, elig)
            fl = rel * X.abs().mean(dim=(1, 2), keepdim=True)
            e2 = E.abs() ** 2; under = (E.abs() < fl)
            for g, (a, b) in enumerate(GROUPS):
                below[g] += float((e2[:, :, a:b, :] * under[:, :, a:b, :]).sum()); tot[g] += float(e2[:, :, a:b, :].sum())
        print(f"floor {rel:.0e} ({20 * np.log10(rel):+.0f} dB re mean|X|): eligible energy below the floor  " +
              "  ".join(f"{n} {100 * below[g] / max(tot[g], 1e-12):.1f}%" for g, n in enumerate(NAMES)), flush=True)
    # (2) gradient-norm calibration on the model's own output
    ratios = []
    for mix, elig in batches:
        X = SN.stft(torch, mix); Tt = SN.stft(torch, elig)
        with torch.no_grad():
            Est0 = net.separate_stft(X)
        for rel in (3e-3,):
            Est = Est0.clone().requires_grad_(True)
            old = (Est.abs() - Tt.abs()).abs().mean() + SN.multires_loss(torch, SN.istft(torch, Est, mix.shape[-1]), elig)
            g_old = torch.autograd.grad(old, Est)[0]
            Est = Est0.clone().requires_grad_(True)
            fl = rel * X.abs().mean(dim=(1, 2), keepdim=True)
            new = ((torch.log(Est.abs() + fl) - torch.log(Tt.abs() + fl)).abs().mean()
                   + SN.multires_loss(torch, SN.istft(torch, Est, mix.shape[-1]), elig, mix, rel))
            g_new = torch.autograd.grad(new, Est)[0]
            Est = Est0.clone().requires_grad_(True)
            cplx = 0.5 * (Est - Tt).abs().mean(); g_c = torch.autograd.grad(cplx, Est)[0]
            r = float(g_old.abs().sum() / g_new.abs().sum())
            ratios.append(r)
            print(f"batch: old bundle {float(old):.4f} |grad| {float(g_old.abs().sum()):.4g} | new (floor {rel:.0e}) {float(new):.4f} |grad| {float(g_new.abs().sum()):.4g} "
                  f"| complex term |grad| {float(g_c.abs().sum()):.4g} | scale old/new {r:.4f}", flush=True)
    print(f"SCALE (median old/new) {np.median(ratios):.4f}  -> --loss-scale {np.median(ratios):.3f}")


if __name__ == "__main__":
    main()
