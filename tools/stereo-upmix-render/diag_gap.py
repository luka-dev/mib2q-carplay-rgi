#!/usr/bin/env python3
"""Where do the dB between a separator student and the IRM live?  v2 (astra review 13 §6): SOURCE ATTRIBUTION -
the predicted mask M is frozen and applied separately to the true eligible / vocal / protected STFTs, and every
ratio is measured on REANALYSED audio (istft -> stft), the same path as the evaluation.  On the 12 DEV excerpts
of sep_net.evaluate():
  SI-SDR of the student, of its magnitude with the mixture phase, of the IRM;
  recall  = transmitted eligible energy / eligible energy   (whole excerpt; and on eligible-dominant cells)
  vleak   = transmitted vocal energy / vocal energy         (whole excerpt; and on vocal-owned cells)
  pleak   = transmitted protected energy / protected energy
  band    = transmitted eligible fraction per band group, dB (energy-weighted), beside the cell-weighted bias
  coverage of the dominant-cell supports.  CPU when CUDA_VISIBLE_DEVICES=-1.
  diag_gap.py CACHE CKPT [CKPT ...]"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN, sep_net as SN

GROUPS = ((0, 11), (11, 43), (43, 128), (128, 342), (342, 598), (598, 1025))      # <250, 250-1k, 1-3k, 3-8k, 8-14k, >14k Hz
NAMES = ("<250", "250-1k", "1-3k", "3-8k", "8-14k", ">14k")


def main():
    cache, ckpts = sys.argv[1], sys.argv[2:]
    torch = SN._torch()
    split = MN.load_split(cache)
    paths = [os.path.join(cache, t + ".npy") for t in split["dev"]][:12]
    for ck in ckpts:
        _, net, dev = SN.load(ck)
        print(f"\n##### {ck}  ({net.macs * SN.FPS / 1e6:.0f} M MAC/s)")
        print("song          mix    student  mag-only  irm   | recall all/edom | vleak all/vown | pleak | cover edom vown")
        med = {}
        tx = np.zeros(len(GROUPS)); te = np.zeros(len(GROUPS)); bias = np.zeros(len(GROUPS)); bcount = np.zeros(len(GROUPS))
        for p in paths:
            N = MN.npy_header(p)[0][2]; n = min(int(20 * SN.SR), N); s = MN.dev_excerpt(p, n) if N > n else 0
            w = torch.as_tensor(MN.expand_slots(MN.read_window(p, s, n)).astype(np.float32) / 32768, device=dev)
            mix = w.sum(0)[None]; elig = w[list(SN.ELIGIBLE_SLOTS)].sum(0)[None]; voc = (w[0] + w[1])[None]; prot = mix - elig
            with torch.no_grad():
                X = SN.stft(torch, mix); E = SN.stft(torch, elig); V = SN.stft(torch, voc); P = X - E
                Y = net.separate_stft(X); M = net.last_mask
                A = lambda Z: SN.stft(torch, SN.istft(torch, Z, n))                 # reanalysed
                y = SN.istft(torch, Y, n)
                ymag = SN.istft(torch, Y.abs() * X / (X.abs() + 1e-7), n)
                irm = (E.abs() / (X.abs() + 1e-7)).clamp(0.0, 1.0); yirm = SN.istft(torch, X * irm, n)
                Ye, Yv, Yp = A(M * E), A(M * V), A(M * P)                           # the mask's transmission of each true source
                E2, V2, P2 = A(E).abs() ** 2, A(V).abs() ** 2, A(P).abs() ** 2
                Ye2, Yv2, Yp2 = Ye.abs() ** 2, Yv.abs() ** 2, Yp.abs() ** 2
                edom = E2 > P2; vown = V2 > 10 * (A(X).abs() ** 2 - V2).clamp(min=0) + 1e-9
                sd = lambda a: float(SN.si_sdr(torch, a, elig).mean())
                r = dict(mix=sd(mix), st=sd(y), mag=sd(ymag), irm=sd(yirm),
                         rec=float(Ye2.sum() / E2.sum()), rec_d=float((Ye2 * edom).sum() / (E2 * edom).sum().clamp(min=1e-9)),
                         vl=float(Yv2.sum() / V2.sum().clamp(min=1e-9)), vl_o=float((Yv2 * vown).sum() / (V2 * vown).sum().clamp(min=1e-9)),
                         pl=float(Yp2.sum() / P2.sum()), c_e=float(edom.float().mean()), c_v=float(vown.float().mean()))
                for gi, (a, b) in enumerate(GROUPS):
                    tx[gi] += float(Ye2[:, :, a:b].sum()); te[gi] += float(E2[:, :, a:b].sum())
                    m = edom[:, :, a:b]
                    db = 20 * torch.log10((Ye[:, :, a:b].abs() + 1e-4) / (A(E)[:, :, a:b].abs() + 1e-4))
                    bias[gi] += float(db[m].sum()); bcount[gi] += float(m.sum())
            for k, v in r.items(): med.setdefault(k, []).append(v)
            print(f"{os.path.basename(p)[:12]} {r['mix']:+6.2f} {r['st']:+7.2f} {r['mag']:+8.2f} {r['irm']:+7.2f} | {r['rec']:5.2f} {r['rec_d']:5.2f}      | {r['vl']:5.3f} {r['vl_o']:5.3f}    | {r['pl']:5.3f} | {r['c_e']:5.2f} {r['c_v']:5.2f}", flush=True)
        m = {k: float(np.median(v)) for k, v in med.items()}
        print(f"MEDIAN       {m['mix']:+6.2f} {m['st']:+7.2f} {m['mag']:+8.2f} {m['irm']:+7.2f} | {m['rec']:5.2f} {m['rec_d']:5.2f}      | {m['vl']:5.3f} {m['vl_o']:5.3f}    | {m['pl']:5.3f} | {m['c_e']:5.2f} {m['c_v']:5.2f}")
        print("band: transmitted eligible fraction (energy-weighted, dB) / cell-weighted bias on eligible-dominant cells (dB): " +
              "  ".join(f"{nm} {10 * np.log10(tx[i] / max(te[i], 1e-12)):+.1f}/{bias[i] / max(bcount[i], 1):+.1f}" for i, nm in enumerate(NAMES)))


if __name__ == "__main__":
    main()
