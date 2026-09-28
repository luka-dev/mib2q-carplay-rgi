#!/usr/bin/env python3
"""review 15: weights for the auxiliary losses (band-balanced L1, attribution) so each contributes ~15 % of the
baseline bundle's PARAMETER-gradient L2 norm at random init on real batches (CPU, CUDA_VISIBLE_DEVICES=-1).
  diag_auxgrad.py CACHE TEACHER_CACHE"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN, sep_net as SN

TARGET = 0.15


def main():
    cache, tcache = sys.argv[1], sys.argv[2]
    torch = SN._torch(); torch.manual_seed(1); rng = np.random.default_rng(1)
    split = MN.load_split(cache)
    truth = [os.path.join(cache, t + ".npy") for t in split["train"]]
    pseudo = [os.path.join(tcache, f) for f in sorted(os.listdir(tcache)) if f.endswith(".npy") and os.path.exists(os.path.join(tcache, f[:-4] + ".json"))]
    net = SN.BandSepNet(torch)
    batches = [SN.windows(torch, (truth, pseudo), 8.0, 4, rng, with_vocal=True) for _ in range(6)]
    with torch.no_grad():
        lm = torch.cat([torch.log(SN.stft(torch, b[0]).abs() + 1e-5).flatten() for b in batches[:4]])
        SN.STATS["mean"], SN.STATS["std"] = float(lm.mean()), float(lm.std() + 1e-3)
    ratios = {"bandbal": [], "attr": []}
    for mix, elig, voc in batches:
        X, Tt, V = SN.stft(torch, mix), SN.stft(torch, elig), SN.stft(torch, voc)
        def pgrad(fn):
            for p in net.parameters(): p.grad = None
            Est = net.separate_stft(X); fn(Est).backward()
            return float(torch.sqrt(sum((p.grad ** 2).sum() for p in net.parameters() if p.grad is not None)))
        base = pgrad(lambda Est: SN.loss_audio(torch, Est, Tt) + SN.multires_loss(torch, SN.istft(torch, Est, mix.shape[-1]), elig))
        bb = pgrad(lambda Est: SN.loss_bandbal(torch, Est, Tt, X))
        at = pgrad(lambda Est: SN.loss_attr(torch, net.last_mask, V, Tt))
        ratios["bandbal"].append(base / bb); ratios["attr"].append(base / at)
        print(f"param-grad L2: baseline {base:.4g} | bandbal@1 {bb:.4g} | attr@1 {at:.4g}", flush=True)
    for k, v in ratios.items():
        print(f"{k}: weight for {TARGET:.0%} of the baseline gradient = {TARGET * np.median(v):.4f}  (median base/aux {np.median(v):.3f}, range {min(v):.3f}..{max(v):.3f})")


if __name__ == "__main__":
    main()
