#!/usr/bin/env python3
"""Real-music behavioural tests for the HSSR upmix, driven by MoisesDB stems.

A stem is the ground truth for one instrument: for every song the mix is
rendered once, then each stem in turn plays the role of the "lead" and
upmix_behavior_tests.rear_share() reports how much of THAT instrument's energy
lands in the rears over time.  On top of the synthetic metrics it measures
the level dependence directly: the regression of the stem's rear share on its
level relative to everything else (mechanism 1 of the 2026-09-25 findings).

    python3 tools/stereo-upmix-render/upmix_stem_tests.py --render BIN \
        --model models/hssr-q6d-moises-a/hssr_model.bin \
        --guard models/hssr-selection-v11/hssr_guard.bin \
        --moisesdb /Volumes/private/Audi/upmix-training-2026-09-11/external/moisesdb/moisesdb_v0.1 \
        --songs 24 --out DIR
"""
import argparse
import csv
import json
import os
import re
import subprocess
import sys

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import upmix_behavior_tests as T  # noqa: E402

SRC_SR = 44100
EXCERPT_S = 60.0
STEMS = ("vocals", "guitar", "piano", "other_keys", "bass", "drums", "other", "wind", "bowed_strings", "percussion")
# The 2026-09-25 finding: every stem's body stays front while its top goes rear,
# so the rear share is also reported per band (of the stem's own cells).
BANDS = [(300, 1000), (1000, 2500), (2500, 4000), (4000, 6000), (6000, 10000), (10000, 16000)]
BAND_KEYS = [f"rear_{lo // 1000 if lo >= 1000 else lo}_{hi // 1000}k" for lo, hi in BANDS]


def load_song(track_dir, excerpt_s):
    """-> (meta, {stemName: stereo float32 @48k of the loudest-vocal excerpt})."""
    meta = json.load(open(os.path.join(track_dir, "data.json")))
    files = {}
    for stem in meta["stems"]:
        paths = [os.path.join(track_dir, stem["stemName"], t["id"] + ".wav") for t in stem["tracks"]]
        files[stem["stemName"]] = [p for p in paths if os.path.exists(p)]
    if not files:
        return meta, {}
    # Window choice: the 60 s where the vocal (else the whole mix) is loudest.
    probe = files.get("vocals") or sum(files.values(), [])
    info = sf.info(probe[0])
    n = info.frames
    win, hop = int(excerpt_s * SRC_SR), int(5 * SRC_SR)
    if n <= win:
        start = 0
    else:
        env = np.zeros((n - win) // hop + 1)
        for p in probe:
            x, _ = sf.read(p, dtype="float32", always_2d=True)
            x = x[:n]
            e = np.cumsum((x ** 2).sum(1))
            idx = np.arange(0, n - win, hop)[:len(env)]
            env[:len(idx)] += e[idx + win - 1] - e[idx]
        start = int(np.argmax(env)) * hop
    stems = {}
    for name, paths in files.items():
        acc = None
        for p in paths:
            x, sr = sf.read(p, dtype="float32", start=start, frames=win, always_2d=True)
            assert sr == SRC_SR, (p, sr)
            if x.shape[1] == 1:
                x = np.repeat(x, 2, 1)
            acc = x if acc is None else acc[:len(x)] + x[:len(acc)]
        if acc is not None and len(acc) > SRC_SR:
            stems[name] = resample_poly(acc.T, 160, 147, axis=1).astype(np.float32)
    return meta, stems


def band_shares(lead, rest, out6):
    """Rear share of the lead's own cells per BANDS (whole excerpt)."""
    fb = np.fft.rfftfreq(T.N_FFT, 1 / T.SR)
    dl = T.stft_pow(lead[0]) + T.stft_pow(lead[1]); dr = T.stft_pow(rest[0]) + T.stft_pow(rest[1])
    front = sum(T.stft_pow(out6[c]) for c in (0, 1, 2)); rear = sum(T.stft_pow(out6[c]) for c in (4, 5))
    n = min(len(dl), len(front))
    own = dl[:n] > 10 * (dr[:n] + 1e-12)
    out = []
    for lo, hi in BANDS:
        sel = own & (fb >= lo) & (fb < hi)
        f = front[:n][sel].sum(); r = rear[:n][sel].sum()
        out.append(r / (f + r) if f + r > 0 else np.nan)
    return out


def level_dependence(lead, rest, share, dt):
    """Regress the lead's rear share on its level vs the rest, per 53 ms frame."""
    pl = T.stft_pow(lead[0]) + T.stft_pow(lead[1])
    pr = T.stft_pow(rest[0]) + T.stft_pow(rest[1])
    lo = int(300 * T.N_FFT / T.SR)
    per = 5
    n = min(len(pl), len(share) * per)
    ratio = []
    for s in range(0, n - per, per):
        a = pl[s:s + per, lo:].sum(); b = pr[s:s + per, lo:].sum()
        ratio.append(10 * np.log10((a + 1e-9) / (b + 1e-9)))
    ratio = np.array(ratio[:len(share)]); sh = share[:len(ratio)]
    ok = ~np.isnan(sh) & (ratio > -30)
    if ok.sum() < 20:
        return np.nan, np.nan, np.nan
    slope, _ = np.polyfit(ratio[ok], sh[ok], 1)
    corr = np.corrcoef(ratio[ok], sh[ok])[0, 1]
    return slope * 6, corr, float(np.median(ratio[ok]))  # share change per +6 dB


_MASK = {}


def protect_mask_args(src, out, tag, model_path):
    """Phase 0 of the source-separation plan: run mask_net on the excerpt and
    hand the renderer the per-control protect mask (needs a -DSPA_HSSR_TEST_MASK
    build).  The net is loaded once per process."""
    import mask_net as MN
    if "net" not in _MASK:
        torch, net, roles, dev = MN.load_model(model_path)
        _MASK.update(torch=torch, net=net, roles=roles, dev=dev)
    import wave
    with wave.open(src, "rb") as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, 2).T.astype(np.float32) / 32768
    prot = MN.protect_mask(_MASK["torch"], _MASK["net"], _MASK["roles"], _MASK["dev"], x)
    path = os.path.join(out, tag + ".msk")
    MN.write_mask(path, prot)
    return ["--protect-mask", path]


def analyse_song(render, args, out, track_dir, excerpt_s, mask_model=None):
    meta, stems = load_song(track_dir, excerpt_s)
    if len(stems) < 2:
        return []
    n = min(x.shape[1] for x in stems.values())
    stems = {k: v[:, :n] for k, v in stems.items()}
    mix = sum(stems.values())
    k = 0.7 / (np.abs(mix).max() + 1e-9)
    tag = re.sub(r'[\\/:*?"<>|]', "_", f"{meta.get('artist', '?')} - {meta.get('song', '?')}")[:60]
    src = os.path.join(out, f"{tag}.wav")
    dst = os.path.join(out, f"{tag}.6ch.wav")
    T.write_wav(src, mix * k)
    extra = protect_mask_args(src, out, tag, mask_model) if mask_model else []
    subprocess.run([render] + args + extra + [src, dst], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    out6 = T.read_wav6(dst)
    on = np.ones(n, bool)
    rows = []
    for name, stem in stems.items():
        lead = stem * k; rest = (mix - stem) * k
        if (lead ** 2).sum() < 1e-3 * (mix ** 2).sum():
            continue                                   # stem effectively silent here
        share, w, dt = T.rear_share(lead, rest, out6)
        m = T.metrics(share, w, dt, on)
        slope6, corr, ratio_db = level_dependence(lead, rest, share, dt)
        bands = dict(zip(BAND_KEYS, band_shares(lead, rest, out6)))
        rows.append({"song": tag, "genre": meta.get("genre", "?"), "stem": name, **bands,
                     "median_rear": m["median_rear"], "steady_std": m["steady_std"],
                     "range_1s": m["range_1s"], "worst_half_s": m["worst_half_s"],
                     "slope_per_6dB": slope6, "level_corr": corr, "level_db": ratio_db,
                     "frames": int(np.sum(~np.isnan(share)))})
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--guard")
    ap.add_argument("--moisesdb", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--songs", type=int, default=24)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--seconds", type=float, default=EXCERPT_S)
    ap.add_argument("--variant", default="shipped", help="knob set from upmix_behavior_tests.VARIANTS")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--mask-model", help="mask_net.pt: render with its protect mask (test-mask build)")
    ap.add_argument("--split-file", help="masknet_cache/split.json")
    ap.add_argument("--split", default="dev", help="with --split-file: which split to take songs from")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    args = T.VARIANTS[a.variant] + ["--model", a.model] + (["--guard-model", a.guard] if a.guard else [])
    tracks = sorted(d for d in os.listdir(a.moisesdb) if os.path.isdir(os.path.join(a.moisesdb, d)))
    if a.split_file:
        wanted = {t for t, v in json.load(open(a.split_file))["split"].items() if v == a.split}
        tracks = [t for t in tracks if t in wanted]
    rng = np.random.default_rng(a.seed)
    picked = [os.path.join(a.moisesdb, t) for t in rng.permutation(tracks)[:a.songs]]

    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(a.jobs) as ex:
        results = list(ex.map(lambda d: analyse_song(a.render, args, a.out, d, a.seconds, a.mask_model), picked))
    rows = [r for rs in results for r in rs]
    keys = ["song", "genre", "stem", "median_rear", "steady_std", "range_1s", "worst_half_s",
            "slope_per_6dB", "level_corr", "level_db", "frames"] + BAND_KEYS
    with open(os.path.join(a.out, "stems.csv"), "w", newline="") as fh:
        wr = csv.DictWriter(fh, keys); wr.writeheader(); wr.writerows(rows)

    print(f"{len(picked)} songs, variant {a.variant}" + (f" + protect mask {a.mask_model}" if a.mask_model else "") + "\n")
    print(f"{'stem':14s} {'n':>3s} {'rear med':>9s} {'rear IQR':>12s} {'std med':>8s} {'range1s med':>11s} "
          f"{'slope/6dB med':>13s} {'corr med':>9s}  flags")
    for stem in STEMS:
        rs = [r for r in rows if r["stem"] == stem]
        if not rs:
            continue
        med = lambda key: np.nanmedian([r[key] for r in rs])
        q = np.nanpercentile([r["median_rear"] for r in rs], [25, 75])
        flags = []
        if stem == "vocals":
            leak = sum(1 for r in rs if r["median_rear"] > 0.15)
            flags.append(f"{leak}/{len(rs)} songs vocal >15 % rear")
        swing = sum(1 for r in rs if r["range_1s"] > 0.30)
        if swing:
            flags.append(f"{swing}/{len(rs)} songs swing >0.30 within 60 s")
        lvl = sum(1 for r in rs if abs(r["slope_per_6dB"]) > 0.10 and abs(r["level_corr"]) > 0.4)
        if lvl:
            flags.append(f"{lvl}/{len(rs)} songs depth follows level")
        print(f"{stem:14s} {len(rs):3d} {med('median_rear'):9.2f} {q[0]:5.2f}..{q[1]:5.2f} {med('steady_std'):8.3f} "
              f"{med('range_1s'):11.2f} {med('slope_per_6dB'):+13.3f} {med('level_corr'):+9.2f}  " + "; ".join(flags))
    print("\nRear share by band (median over songs): the depth split inside one instrument")
    print(f"{'stem':14s}" + "".join(f"{lo / 1000:g}-{hi / 1000:g}k".rjust(9) for lo, hi in BANDS))
    for stem in STEMS:
        rs = [r for r in rows if r["stem"] == stem]
        if len(rs) >= 3:
            print(f"{stem:14s}" + "".join(f"{np.nanmedian([r[k] for r in rs]):9.2f}" for k in BAND_KEYS))
    print("\nWorst songs (range_1s = back-and-forth within the excerpt, 1 s medians):")
    for r in sorted(rows, key=lambda r: -r["range_1s"])[:12]:
        print(f"  {r['range_1s']:.2f}  rear {r['median_rear']:.2f}  slope/6dB {r['slope_per_6dB']:+.2f} corr {r['level_corr']:+.2f}  "
              f"{r['stem']:10s} {r['song']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
