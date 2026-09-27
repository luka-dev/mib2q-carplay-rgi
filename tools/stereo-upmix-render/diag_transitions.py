#!/usr/bin/env python3
"""astra 11: the transition eval the loudest-vocal excerpts cannot give.  From the DEV songs' true stems,
find vocal ENTRIES (vocal RMS rises > 10 dB between adjacent 2-s windows and lands above -40 dBFS), vocal
EXITS (the mirror) and SUSTAINED-vocal controls (12 s above -40 dBFS with no rise), <= 3 events of each kind
per song; run the student continuously from 10 s before the 12-s excerpt (4 s before the event, 8 s after) so
its state is realistic; per 0.5 s: vocal leak (estimate energy in vocal-owned cells / vocal energy there) and
eligible retention (estimate energy in eligible-dominant cells / truth energy there).  Reports distributions
and the worst cases, not only medians.  CPU (CUDA_VISIBLE_DEVICES=-1).
  diag_transitions.py CACHE CKPT [CKPT ...] [--songs N]"""
import argparse, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN, sep_net as SN

SR = SN.SR; W = 2 * SR; PRE, POST, WARM = 4 * SR, 8 * SR, 10 * SR; FR = SR // 2       # 0.5-s frames


def rms_db(x, n):
    """RMS in dBFS per block of n samples of a [2, N] array."""
    k = x.shape[1] // n
    return 10 * np.log10(np.mean(x[:, :k * n].reshape(2, k, n) ** 2, axis=(0, 2)) + 1e-12)


def events(voc):
    """[(kind, onset_sample)] from the vocal stem: entries, exits, sustained controls, <= 3 each."""
    v = rms_db(voc, W); out = {"entry": [], "exit": [], "sustained": []}
    for k in range(1, len(v)):
        if v[k] - v[k - 1] > 10 and v[k] > -40:
            out["entry"].append(k * W)
        if v[k - 1] - v[k] > 10 and v[k - 1] > -40:
            out["exit"].append(k * W)
    k = 1
    while k + 6 < len(v):
        seg = v[k:k + 6]
        if seg.min() > -40 and np.abs(np.diff(v[k - 1:k + 6])).max() < 6:
            out["sustained"].append((k + 2) * W); k += 6
        else:
            k += 1
    rng = np.random.default_rng(0)
    return [(kind, int(s)) for kind, lst in out.items() for s in (rng.choice(lst, 3, replace=False) if len(lst) > 3 else lst)]


def refine(voc, onset, kind):
    """entries/exits: move the onset to the first 0.25-s frame around the coarse one that crosses the mid level."""
    if kind == "sustained":
        return onset
    q = SR // 4; a = max(0, onset - W); b = min(voc.shape[1], onset + W)
    lv = rms_db(voc[:, a:b], q); mid = 0.5 * (lv.max() + lv.min())
    idx = np.where((lv > mid) if kind == "entry" else (lv < mid))[0]
    return a + int(idx[0]) * q if len(idx) else onset


def frame_stats(torch, X, E, V, Y, n_frames):
    """per 0.5-s frame: leak, retention (nan where undefined)."""
    fpf = FR / SN.HOP                                            # STFT frames per 0.5 s (46.875)
    t = np.arange(X.shape[1]); fi = np.minimum((t / fpf).astype(int), n_frames - 1)
    vcell = (V.abs() ** 2) > 10 * ((X - V).abs() ** 2 + 1e-9)
    edom = (E.abs() ** 2) > ((X - E).abs() ** 2)
    leak = np.full(n_frames, np.nan); ret = np.full(n_frames, np.nan)
    Y2, V2, E2 = Y.abs() ** 2, V.abs() ** 2, E.abs() ** 2
    for f in range(n_frames):
        m = torch.as_tensor(fi == f)
        vd = float((V2[:, m] * vcell[:, m]).sum()); ed = float((E2[:, m] * edom[:, m]).sum())
        if vd > 1e-6: leak[f] = float((Y2[:, m] * vcell[:, m]).sum()) / vd
        if ed > 1e-6: ret[f] = float((Y2[:, m] * edom[:, m]).sum()) / ed
    return leak, ret


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("cache"); ap.add_argument("ckpts", nargs="+"); ap.add_argument("--songs", type=int, default=20)
    a = ap.parse_args()
    torch = SN._torch()
    split = MN.load_split(a.cache)
    paths = [os.path.join(a.cache, t + ".npy") for t in split["dev"]][:a.songs]
    songs = []
    for p in paths:
        N = MN.npy_header(p)[0][2]
        w = MN.expand_slots(MN.read_window(p, 0, N)).astype(np.float32) / 32768        # [7,2,N]
        voc = w[0] + w[1]
        evs = [(k, refine(voc, s, k)) for k, s in events(voc)]
        evs = [(k, s) for k, s in evs if s - PRE - WARM >= 0 and s + POST <= N]
        songs.append((p, w, evs))
    print(f"{len(songs)} songs, events: " + ", ".join(f"{k} {sum(1 for _, _, e in songs for kk, _ in e if kk == k)}" for k in ("entry", "exit", "sustained")))
    nf = (PRE + POST) // FR; t_axis = (np.arange(nf) * 0.5 - 4.0)
    for ck in a.ckpts:
        _, net, dev = SN.load(ck)
        print(f"\n##### {ck}")
        rows = {"entry": [], "exit": [], "sustained": []}
        for p, w, evs in songs:
            for kind, s in evs:
                a0, a1 = s - PRE - WARM, s + POST
                seg = torch.as_tensor(w[:, :, a0:a1], device=dev)
                mix = seg.sum(0)[None]; elig = seg[list(SN.ELIGIBLE_SLOTS)].sum(0)[None]; voc = (seg[0] + seg[1])[None]
                with torch.no_grad():
                    y = SN.istft(torch, net.separate_stft(SN.stft(torch, mix)), a1 - a0)
                cut = lambda x: x[:, :, WARM:]
                X = SN.stft(torch, cut(mix)); E = SN.stft(torch, cut(elig)); V = SN.stft(torch, cut(voc)); Y = SN.stft(torch, cut(y))
                leak, ret = frame_stats(torch, X, E, V, Y, nf)
                pre, post = t_axis < 0, t_axis >= 0
                r = dict(song=os.path.basename(p)[:8], t=s / SR,
                         ret_pre=np.nanmean(ret[pre]), ret_post=np.nanmean(ret[post]),
                         leak_pre=np.nanmean(leak[pre]), leak_0_2=np.nanmean(leak[(t_axis >= 0) & (t_axis < 2)]), leak_peak=np.nanmax(leak[post]) if np.isfinite(leak[post]).any() else np.nan,
                         leak_steady=np.nanmean(leak[t_axis >= 4]))
                # settling: first post-onset frame whose leak is within 1.5x of the steady value (entries only)
                if kind == "entry" and np.isfinite(r["leak_steady"]):
                    ok = np.where(post & (leak <= 1.5 * r["leak_steady"] + 0.01))[0]
                    r["settle"] = t_axis[ok[0]] if len(ok) else np.nan
                rows[kind].append(r)
        for kind, rs in rows.items():
            if not rs: continue
            f = lambda key: np.array([x.get(key, np.nan) for x in rs], dtype=float)
            q = lambda v: f"{np.nanmedian(v):.3f}/{np.nanpercentile(v, 90):.3f}" if np.isfinite(v).any() else "n/a"
            print(f"{kind:9s} n={len(rs):2d}  retention pre {q(f('ret_pre'))}  post {q(f('ret_post'))}  post/pre {q(f('ret_post') / f('ret_pre'))}  | "
                  f"leak pre {q(f('leak_pre'))}  0-2 s {q(f('leak_0_2'))}  peak {q(f('leak_peak'))}  steady {q(f('leak_steady'))}" +
                  (f"  | settle s {q(f('settle'))}" if kind == "entry" else "") + "   (median/p90)")
            if kind == "entry":
                worst = sorted(rs, key=lambda x: -np.nan_to_num(x["leak_peak"]))[:3]
                print("          worst peak leak: " + "; ".join(f"{x['song']} @{x['t']:.0f}s peak {x['leak_peak']:.2f} 0-2s {x['leak_0_2']:.2f} ret {x['ret_pre']:.2f}->{x['ret_post']:.2f}" for x in worst))
                worst = sorted(rs, key=lambda x: np.nan_to_num(x["ret_post"] / x["ret_pre"], nan=9))[:3]
                print("          worst ducking:   " + "; ".join(f"{x['song']} @{x['t']:.0f}s ret {x['ret_pre']:.2f}->{x['ret_post']:.2f} leak0-2 {x['leak_0_2']:.2f}" for x in worst))


if __name__ == "__main__":
    main()
