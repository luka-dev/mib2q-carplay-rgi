#!/usr/bin/env python3
"""Where do the ~5.5 dB between a separator student and the IRM ceiling live?  On the 12 DEV excerpts of
sep_net.evaluate(): SI-SDR of the student, of its MAGNITUDE with the mixture phase (is the complex mask's
phase worth anything?), and of the IRM; recall of eligible-dominant cells and leak into protected-dominant
cells; per-band-group log-magnitude bias on eligible-dominant cells (negative = conservative); SI-SDR per
2-s segment split into vocal-entry segments and the rest.  CPU when CUDA_VISIBLE_DEVICES=-1 (PowerShell drops an empty variable).
  diag_gap.py CACHE CKPT [CKPT ...]"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN, sep_net as SN

GROUPS = ((0, 11), (11, 43), (43, 128), (128, 342), (342, 598), (598, 1025))      # bins: <250, 250-1k, 1-3k, 3-8k, 8-14k, >14k Hz
SEG = 2 * SN.SR


def seg_sdr(torch, est, ref):
    n = est.shape[-1] // SEG * SEG
    e = est[..., :n].reshape(2, -1, SEG).transpose(0, 1); r = ref[..., :n].reshape(2, -1, SEG).transpose(0, 1)   # [S,2,SEG]
    return SN.si_sdr(torch, e, r).mean(-1).cpu().numpy()


def main():
    cache, ckpts = sys.argv[1], sys.argv[2:]
    torch = SN._torch()
    split = MN.load_split(cache)
    paths = [os.path.join(cache, t + ".npy") for t in split["dev"]][:12]
    for ck in ckpts:
        _, net, dev = SN.load(ck)
        print(f"\n##### {ck}  ({net.macs * SN.FPS / 1e6:.0f} M MAC/s)")
        print("song          mix    student  mag-only  irm   | recall  leak  | irm recall leak | vocal leak")
        med = {k: [] for k in ("mix", "st", "mag", "irm", "rec", "leak", "irec", "ileak", "vleak")}
        bias = np.zeros((len(GROUPS), 2)); bcount = np.zeros(len(GROUPS))
        entry_st, entry_irm, rest_st, rest_irm = [], [], [], []
        for p in paths:
            N = MN.npy_header(p)[0][2]; n = min(int(20 * SN.SR), N); s = MN.dev_excerpt(p, n) if N > n else 0
            w = torch.as_tensor(MN.expand_slots(MN.read_window(p, s, n)).astype(np.float32) / 32768, device=dev)
            mix = w.sum(0)[None]; elig = w[list(SN.ELIGIBLE_SLOTS)].sum(0)[None]; voc = (w[0] + w[1])[None]
            with torch.no_grad():
                X = SN.stft(torch, mix); E = SN.stft(torch, elig); V = SN.stft(torch, voc)
                Y = net.separate_stft(X)
                y = SN.istft(torch, Y, n)
                ymag = SN.istft(torch, Y.abs() * X / (X.abs() + 1e-7), n)
                irm = (E.abs() / (X.abs() + 1e-7)).clamp(0.0, 1.0); Yi = X * irm; yirm = SN.istft(torch, Yi, n)
                P = X - E
                edom = (E.abs() ** 2) > (P.abs() ** 2)
                sd = lambda a: float(SN.si_sdr(torch, a, elig).mean())
                r = dict(mix=sd(mix), st=sd(y), mag=sd(ymag), irm=sd(yirm),
                         rec=float((Y.abs() ** 2 * edom).sum() / (E.abs() ** 2 * edom).sum()),
                         leak=float((Y.abs() ** 2 * ~edom).sum() / (P.abs() ** 2 * ~edom).sum()),
                         irec=float((Yi.abs() ** 2 * edom).sum() / (E.abs() ** 2 * edom).sum()),
                         ileak=float((Yi.abs() ** 2 * ~edom).sum() / (P.abs() ** 2 * ~edom).sum()))
                vcell = (V.abs() ** 2) > 10 * ((X - V).abs() ** 2 + 1e-9)
                r["vleak"] = float(((Y.abs() ** 2) * vcell).sum() / ((V.abs() ** 2) * vcell).sum().clamp(min=1e-9))
                # per-band-group magnitude bias on eligible-dominant cells (dB)
                db = 20 * torch.log10((Y.abs() + 1e-4) / (E.abs() + 1e-4))
                for gi, (a, b) in enumerate(GROUPS):
                    m = edom[:, :, a:b, :]
                    if m.any():
                        bias[gi, 0] += float(db[:, :, a:b, :][m].sum()); bias[gi, 1] += float(db[:, :, a:b, :][m].abs().sum()); bcount[gi] += float(m.sum())
                # segments: vocal entries (vocal RMS rises > 10 dB over the previous 2 s and is above -40 dBFS)
                vs = voc[0, :, :n // SEG * SEG].reshape(2, -1, SEG).pow(2).mean((0, 2)).clamp(min=1e-12).log10().mul(10).cpu().numpy()
                ss, si = seg_sdr(torch, y[0], elig[0]), seg_sdr(torch, yirm[0], elig[0])
                for k in range(1, len(vs)):
                    (entry_st if (vs[k] - vs[k - 1] > 10 and vs[k] > -40) else rest_st).append(ss[k])
                    (entry_irm if (vs[k] - vs[k - 1] > 10 and vs[k] > -40) else rest_irm).append(si[k])
            for k, v in r.items(): med[k].append(v)
            print(f"{os.path.basename(p)[:12]} {r['mix']:+6.2f} {r['st']:+7.2f} {r['mag']:+8.2f} {r['irm']:+7.2f} | {r['rec']:5.2f} {r['leak']:5.2f} | {r['irec']:6.2f} {r['ileak']:5.2f}     | {r['vleak']:.3f}", flush=True)
        m = {k: float(np.median(v)) for k, v in med.items()}
        print(f"MEDIAN       {m['mix']:+6.2f} {m['st']:+7.2f} {m['mag']:+8.2f} {m['irm']:+7.2f} | {m['rec']:5.2f} {m['leak']:5.2f} | {m['irec']:6.2f} {m['ileak']:5.2f}     | {m['vleak']:.3f}")
        names = ("<250", "250-1k", "1-3k", "3-8k", "8-14k", ">14k")
        print("band bias on eligible-dominant cells (dB, student/truth; mean, mean |.|): " +
              "  ".join(f"{nm} {bias[i, 0] / max(bcount[i], 1):+.1f}/{bias[i, 1] / max(bcount[i], 1):.1f}" for i, nm in enumerate(names)))
        print(f"2-s segments: vocal entries n={len(entry_st)} student {np.median(entry_st):+.2f} irm {np.median(entry_irm):+.2f} | "
              f"rest n={len(rest_st)} student {np.median(rest_st):+.2f} irm {np.median(rest_irm):+.2f} | "
              f"student p10/p90 {np.percentile(rest_st + entry_st, 10):+.2f}/{np.percentile(rest_st + entry_st, 90):+.2f}")


if __name__ == "__main__":
    main()
