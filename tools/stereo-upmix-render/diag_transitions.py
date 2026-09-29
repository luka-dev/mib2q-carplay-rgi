#!/usr/bin/env python3
"""The transition eval (astra reviews 11 and 13).  From the DEV songs' true stems: vocal ENTRIES (RMS rises > 10 dB
between adjacent 2-s windows and lands above -40 dBFS), EXITS (the mirror) and SUSTAINED-vocal controls, <= 3 per
kind per song, refined to the nearest correctly DIRECTED 0.25-s crossing of the mid level, overlapping events
rejected; the student runs continuously from 10 s before the 12-s excerpt (4 s before / 8 s after the event).
v2 = SOURCE ATTRIBUTION: the predicted mask is applied to the true vocal and eligible STFTs of the same segment
(never the net on isolated stems), per 0.5 s on reanalysed audio:
  leak(t)      = transmitted vocal energy / vocal energy        (defined when the vocal frame is above -55 dBFS)
  retention(t) = transmitted eligible energy / eligible energy  (defined when the eligible frame is above -55 dBFS)
Reports median / p90 (p10 for the ducking ratio), the 0-2 s peak separately from the 4-8 s steady state, settling =
first post-onset time from which THREE consecutive bins stay within 1.5x steady + 0.01 (else censored), worst cases.
CPU when CUDA_VISIBLE_DEVICES=-1.     diag_transitions.py CACHE CKPT [CKPT ...] [--songs N]"""
import argparse, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN, sep_net as SN, role_confidence as RC

SR = SN.SR; W = 2 * SR; PRE, POST, WARM = 4 * SR, 8 * SR, 10 * SR; FR = SR // 2       # 0.5-s frames
FLOOR_DB = -55.0


def rms_db(x, n):
    k = x.shape[1] // n
    return 10 * np.log10(np.mean(x[:, :k * n].reshape(2, k, n) ** 2, axis=(0, 2)) + 1e-12)


def events(voc):
    """[(kind, onset_sample)] from the vocal stem: entries, exits, sustained controls, <= 3 each, refined, non-overlapping."""
    v = rms_db(voc, W); out = {"entry": [], "exit": [], "sustained": []}
    for k in range(1, len(v)):
        if v[k] - v[k - 1] > 10 and v[k] > -40: out["entry"].append(k * W)
        if v[k - 1] - v[k] > 10 and v[k - 1] > -40: out["exit"].append(k * W)
    k = 1
    while k + 6 < len(v):
        if v[k:k + 6].min() > -40 and np.abs(np.diff(v[k - 1:k + 6])).max() < 6: out["sustained"].append((k + 2) * W); k += 6
        else: k += 1
    evs = [(kind, refine(voc, s, kind)) for kind, lst in out.items() for s in lst]
    evs = sorted([(k, s) for k, s in evs if s is not None], key=lambda e: e[1])
    kept = []
    for k, s in evs:                                              # reject overlapping excerpts (keep the earlier)
        if all(abs(s - s2) >= PRE + POST for _, s2 in kept): kept.append((k, s))
    rng = np.random.default_rng(0); final = []
    for kind in out:
        lst = [e for e in kept if e[0] == kind]
        final += [lst[i] for i in sorted(rng.choice(len(lst), 3, replace=False))] if len(lst) > 3 else lst
    return final


def refine(voc, onset, kind):
    """entries/exits: the nearest correctly DIRECTED 0.25-s crossing of the mid level around the coarse onset; None if there is none."""
    if kind == "sustained": return onset
    q = SR // 4; a = max(0, onset - W); b = min(voc.shape[1], onset + W)
    lv = rms_db(voc[:, a:b], q); mid = 0.5 * (lv.max() + lv.min())
    cross = [i for i in range(1, len(lv)) if ((lv[i - 1] < mid <= lv[i]) if kind == "entry" else (lv[i - 1] >= mid > lv[i]))]
    if not cross: return None
    i = min(cross, key=lambda i: abs(a + i * q - onset))
    return a + i * q


def per_frame(torch, num, den, nf, floor_db):
    """[T] frame energies of two reanalysed STFTs -> per 0.5-s ratio (nan where the denominator frame is below the floor)."""
    fpf = FR / SN.HOP; t = np.arange(num.shape[1]); fi = np.minimum((t / fpf).astype(int), nf - 1)
    out = np.full(nf, np.nan)
    n2 = (num.abs() ** 2).sum((0, 2, 3)).cpu().numpy(); d2 = (den.abs() ** 2).sum((0, 2, 3)).cpu().numpy()
    for f in range(nf):
        m = fi == f; d = d2[m].sum()
        # the STFT energy of a -55 dBFS 0.5-s stereo frame at 2048/512 with a Hann window: ~ (10^(-5.5))^2 * n * sum(w^2)/hop
        if d > 10 ** (floor_db / 10) * m.sum() * 2 * 768: out[f] = n2[m].sum() / d
    return out


def settle(leak, t_axis, steady):
    post = np.where(t_axis >= 0)[0]
    for i in post:
        if i + 3 <= len(leak) and np.all(np.isfinite(leak[i:i + 3])) and np.all(leak[i:i + 3] <= 1.5 * steady + 0.01): return t_axis[i]
    return np.nan                                                # censored


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("cache"); ap.add_argument("ckpts", nargs="+"); ap.add_argument("--songs", type=int, default=20)
    ap.add_argument("--role-map", choices=("soft_static", "soft_ema80"))
    ap.add_argument("--matrix-zero-cross", action="store_true")
    a = ap.parse_args()
    if a.role_map and len(a.ckpts) != 1:
        ap.error("--role-map requires exactly one four-role checkpoint")
    if a.matrix_zero_cross and (len(a.ckpts) != 1 or a.role_map):
        ap.error("--matrix-zero-cross requires exactly one matrix2 checkpoint and no role map")
    torch = SN._torch()
    split = MN.load_split(a.cache)
    songs = []
    for p in [os.path.join(a.cache, t + ".npy") for t in split["dev"]][:a.songs]:
        N = MN.npy_header(p)[0][2]
        w = MN.expand_slots(MN.read_window(p, 0, N)).astype(np.float32) / 32768
        evs = [(k, s) for k, s in events(w[0] + w[1]) if s - PRE - WARM >= 0 and s + POST <= N]
        songs.append((p, w, evs))
    print(f"{len(songs)} songs, events: " + ", ".join(f"{k} {sum(1 for _, _, e in songs for kk, _ in e if kk == k)}" for k in ("entry", "exit", "sustained")))
    nf = (PRE + POST) // FR; t_axis = np.arange(nf) * 0.5 - 4.0; pre, post = t_axis < 0, t_axis >= 0
    for ck in a.ckpts:
        _, net, dev = SN.load(ck)
        print(f"\n##### {ck}" + (f" role-map={a.role_map}" if a.role_map else "")
              + (" matrix-zero-cross" if a.matrix_zero_cross else ""))
        rows = {"entry": [], "exit": [], "sustained": []}
        for p, w, evs in songs:
            for kind, s in evs:
                a0, a1 = s - PRE - WARM, s + POST
                seg = torch.as_tensor(w[:, :, a0:a1], device=dev)
                mix = seg.sum(0)[None]; elig = seg[list(SN.ELIGIBLE_SLOTS)].sum(0)[None]; voc = (seg[0] + seg[1])[None]
                with torch.no_grad():
                    net.separate_stft(SN.stft(torch, mix))
                    if a.matrix_zero_cross:
                        if net.mask_mode != "matrix2":
                            raise ValueError("--matrix-zero-cross requires a matrix2 checkpoint")
                        diagonal, cross = net.last_matrix
                        net.last_matrix = (diagonal, torch.zeros_like(cross))
                    M = (RC.mask_from_logits(torch, net.role_logits, a.role_map)[0]
                         if a.role_map else net.last_mask)
                    L = a1 - a0; cutf = M.shape[1] - SN.stft(torch, mix[:, :, WARM:]).shape[1]     # frames of warm-up to drop
                    A = lambda Z: SN.stft(torch, SN.istft(torch, Z, L))[:, cutf:]
                    E = SN.stft(torch, elig); V = SN.stft(torch, voc)
                    filtered = lambda Z: M * Z if a.role_map else net.apply_last_filter(Z)
                    leak = per_frame(torch, A(filtered(V)), A(V), nf, FLOOR_DB); ret = per_frame(torch, A(filtered(E)), A(E), nf, FLOOR_DB)
                r = dict(song=os.path.basename(p)[:8], t=s / SR, ret_pre=np.nanmean(ret[pre]), ret_post=np.nanmean(ret[post]),
                         leak_pre=np.nanmean(leak[pre]), leak_0_2=np.nanmean(leak[(t_axis >= 0) & (t_axis < 2)]),
                         peak_0_2=np.nanmax(leak[(t_axis >= 0) & (t_axis < 2)]) if np.isfinite(leak[(t_axis >= 0) & (t_axis < 2)]).any() else np.nan,
                         leak_steady=np.nanmean(leak[t_axis >= 4]))
                r["duck"] = r["ret_post"] / r["ret_pre"] if r["ret_pre"] > 0 else np.nan
                if kind == "entry": r["settle"] = settle(leak, t_axis, r["leak_steady"]) if np.isfinite(r["leak_steady"]) else np.nan
                rows[kind].append(r)
        for kind, rs in rows.items():
            if not rs: continue
            f = lambda key: np.array([x.get(key, np.nan) for x in rs], dtype=float)
            q = lambda v, lo=50, hi=90: f"{np.nanpercentile(v, lo):.3f}/{np.nanpercentile(v, hi):.3f}" if np.isfinite(v).any() else "n/a"
            line = (f"{kind:9s} n={len(rs):2d}  retention pre {q(f('ret_pre'))} post {q(f('ret_post'))} post/pre med {np.nanmedian(f('duck')):.3f} p10 {np.nanpercentile(f('duck'), 10):.3f} p90 {np.nanpercentile(f('duck'), 90):.3f}"
                    f" | vocal leak pre {q(f('leak_pre'))} 0-2 s {q(f('leak_0_2'))} peak0-2 {q(f('peak_0_2'))} steady4-8 {q(f('leak_steady'))}")
            if kind == "entry":
                st = f("settle"); line += f" | settle s med {np.nanmedian(st):.2f} p90 {np.nanpercentile(st, 90):.2f} censored {int(np.sum(~np.isfinite(st)))}/{len(st)}"
            print(line + "   (median/p90)")
            if kind == "entry":
                worst = sorted(rs, key=lambda x: -np.nan_to_num(x["peak_0_2"]))[:3]
                print("          worst 0-2 s peak leak: " + "; ".join(f"{x['song']} @{x['t']:.0f}s peak {x['peak_0_2']:.2f} mean0-2 {x['leak_0_2']:.2f} ret {x['ret_pre']:.2f}->{x['ret_post']:.2f}" for x in worst))
                worst = sorted(rs, key=lambda x: np.nan_to_num(x["duck"], nan=9))[:3]
                print("          worst ducking:         " + "; ".join(f"{x['song']} @{x['t']:.0f}s ret {x['ret_pre']:.2f}->{x['ret_post']:.2f} leak0-2 {x['leak_0_2']:.2f}" for x in worst))


if __name__ == "__main__":
    main()
