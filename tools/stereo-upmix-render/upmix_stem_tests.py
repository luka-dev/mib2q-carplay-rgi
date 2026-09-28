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
        if MASK_ELIGIBLE:                                  # routing-time choice of which roles may go rear
            roles.eligible = [roles.names.index(n) for n in MASK_ELIGIBLE.split("+")]
        _MASK.update(torch=torch, net=net, roles=roles, dev=dev)
    import wave
    with wave.open(src, "rb") as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, 2).T.astype(np.float32) / 32768
    prot = MN.protect_mask(_MASK["torch"], _MASK["net"], _MASK["roles"], _MASK["dev"], x)
    elig = np.clip(1 - prot, 0, 1)
    if MASK_THRESH > 0:
        elig = np.clip((elig - MASK_THRESH) / (1 - MASK_THRESH), 0, 1)
    if MASK_SHARPEN != 1.0:
        elig = elig ** MASK_SHARPEN
    if ORACLE_AMP:                                   # drive mode wants an amplitude gain: sqrt of the eligibility
        elig = np.sqrt(elig)
    prot = (1 - elig).astype(np.float32)
    path = os.path.join(out, tag + ".msk")
    MN.write_mask(path, prot)
    return ["--protect-mask", path]


ORACLE_PROTECT = ("vocals", "bass", "drums")   # MoisesDB stem names never allowed rear (drums whole: conservative)
ORACLE_REF = ""      # --oracle-ref DIR: renders of the oracle condition; rear reconstruction error against them (astra review 6)


def rear_error(out6, tag):
    """Oracle-referenced rear reconstruction error: the candidate's Ls+Rs against the oracle's from the same
    renderer, latency-aligned by cross-correlation (+-50 ms), normalised squared error broadband and in the
    treble (2-16 kHz), at fixed gain and after one global positive gain fit (level vs waveform/image mismatch).
    A difference-signal wash scores badly here while rear share cannot tell it from a routed source."""
    path = os.path.join(ORACLE_REF, f"{tag}.6ch.wav")
    if not os.path.exists(path):
        return {}
    ref = T.read_wav6(path)
    n = min(ref.shape[1], out6.shape[1])
    a = out6[4, :n] + out6[5, :n]; b = ref[4, :n] + ref[5, :n]
    m = int(0.05 * T.SR); L = 1 << int(np.ceil(np.log2(n + m)))
    xc = np.fft.irfft(np.fft.rfft(a, L) * np.conj(np.fft.rfft(b, L)), L)
    lags = np.concatenate([xc[:m + 1], xc[-m:]]); k = int(np.argmax(lags)); lag = k if k <= m else k - (2 * m + 1)
    if lag > 0: a, b = a[lag:], b[:n - lag]
    elif lag < 0: a, b = a[:n + lag], b[-lag:]
    A = np.fft.rfft(a); B = np.fft.rfft(b); freqs = np.fft.rfftfreq(len(a), 1 / T.SR)
    out = {}
    for band, sel in (("rear_nse", freqs >= 200), ("rear_nse_treble", (freqs >= 2000) & (freqs < 16000))):
        Ab, Bb = A[sel], B[sel]
        ref_e = float(np.sum(np.abs(Bb) ** 2)) + 1e-12
        out[band] = float(np.sum(np.abs(Ab - Bb) ** 2) / ref_e)
        g = max(0.0, float(np.real(np.vdot(Ab, Bb)) / (np.sum(np.abs(Ab) ** 2) + 1e-12)))
        out[band + "_gainfit"] = float(np.sum(np.abs(g * Ab - Bb) ** 2) / ref_e)
    out["rear_lag_ms"] = lag * 1000.0 / T.SR
    return out


ORACLE_AMP = False   # --oracle-amp: write 1 - sqrt(eligibility) so the drive mode's (1 - protect) is an AMPLITUDE gain
MASK_THRESH = 0.0    # --mask-thresh t: eligibility below t is 0, above rescaled (a conservative operating point)
MASK_SHARPEN = 1.0   # --mask-sharpen k: eligibility ** k (k > 1 = only confident cells go rear)
MASK_ELIGIBLE = ""   # --mask-eligible other  (default: the model's other+cymbals)
RESUME = False
INJECT_SMOOTH = 0.0    # --inject-smooth MS: slow per-band gain envelope (fast attack, this release) on the injected estimate


def smooth_envelope(elig, mix, release_ms, n_fft=2048, hop=512, bands=32):
    """Stabiliser for a separator's estimate: per band, the estimate's energy relative to the mixture's is
    held with a fast attack and a slow release, and the estimate is re-scaled to that held ratio.  The
    spectral shape stays the estimate's; only its level trajectory per band is slowed down."""
    import sep_net as SN
    torch = SN._torch()
    E = SN.stft(torch, torch.as_tensor(elig.astype(np.float32))[None])[0]        # [T,1025,2]
    X = SN.stft(torch, torch.as_tensor(mix.astype(np.float32))[None])[0]
    edges = np.unique(np.round(np.geomspace(1, E.shape[1] - 1, bands + 1)).astype(int))
    pe = (E.abs() ** 2).sum(-1); px = (X.abs() ** 2).sum(-1) + 1e-9               # [T,1025]
    alpha = float(np.exp(-hop / 48000 / (release_ms / 1000)))
    gain = torch.ones_like(pe)
    for a, b in zip(edges[:-1], edges[1:]):
        r = (pe[:, a:b].sum(-1) / px[:, a:b].sum(-1)).clamp(1e-4, 4.0)           # the estimate's share of the mixture per band
        held = r.clone()
        for t in range(1, len(r)):
            held[t] = max(float(r[t]), float(held[t - 1]) * alpha) if r[t] < held[t - 1] else float(r[t])
        gain[:, a:b] = (held / r).sqrt()[:, None]                                  # re-scale the estimate to the held share
    out = SN.istft(torch, (E * gain[..., None])[None], elig.shape[1])[0]
    return out.numpy()
TEACHER_MASK = False   # --teacher-mask: the mask from htdemucs's stems of the excerpt (jobs must be 1: one GPU model)
INJECT = ""            # --inject oracle|teacher: that source's eligible AUDIO drives the object path (needs --render-extra "--mask-drive G")
MASK_DIR = ""          # --mask-dir: precomputed HSSRMSK0 masks, for offline controller experiments


def oracle_mask_args(stems, k, out, tag):
    """Upper bound of the whole approach: the protect mask from the REAL stems at
    the DSP's geometry.  If even this gives a poor rear through the DSP, the mask
    is not the bottleneck."""
    import mask_net as MN
    torch = MN._torch()
    P = {}
    for name, x in stems.items():
        X = MN.stft(torch, torch.as_tensor(x * k))                       # [2, T, 257]
        P[name] = (X.real ** 2 + X.imag ** 2).sum(0)
    tot = sum(P.values()) + 1e-9
    elig = sum(v for n, v in P.items() if n not in ORACLE_PROTECT) / tot  # [T, 257]
    T = (elig.shape[0] // MN.CONTROL) * MN.CONTROL
    elig = elig[:T].reshape(T // MN.CONTROL, MN.CONTROL, MN.N_BINS).mean(1)
    if ORACLE_AMP:
        elig = elig.sqrt()
    prot = (1 - elig).numpy().astype(np.float32)
    path = os.path.join(out, tag + ".oracle.msk")
    MN.write_mask(path, prot)
    return ["--protect-mask", path]


def analyse_song(render, args, out, track_dir, excerpt_s, mask_model=None, oracle=False):
    # per-song result cache: the D: volume (WinBtrfs) stalls CloseHandle under load, so a run can be
    # killed by its runner and resumed (--resume) without redoing the songs that finished
    cache = os.path.join(out, os.path.basename(track_dir) + ".rows.json")
    if RESUME and os.path.exists(cache):
        return json.load(open(cache))
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
    if INJECT:                                         # interface test (astra 8): the rear-eligible AUDIO through the object path
        if INJECT == "oracle":
            elig = sum(v for nme, v in stems.items() if nme not in ORACLE_PROTECT) * k
        elif INJECT == "irm":                              # astra 9: the optimal BOUNDED REAL mask (ideal ratio mask, mixture phase) at 2048/512
            import sep_net as SN
            torch = SN._torch()
            e = torch.as_tensor((sum(v for nme, v in stems.items() if nme not in ORACLE_PROTECT) * k).astype(np.float32))[None]
            m = torch.as_tensor((mix * k).astype(np.float32))[None]
            E, X = SN.stft(torch, e), SN.stft(torch, m)
            irm = (E.abs() / (X.abs() + 1e-7)).clamp(0.0, 1.0)
            est = SN.istft(torch, X * irm, m.shape[-1])[0]
            sdr = float(SN.si_sdr(torch, est, e[0]).mean()); sdr_mix = float(SN.si_sdr(torch, m[0], e[0]).mean())
            print(f"  IRM {tag}: si_sdr {sdr:.2f} dB (mixture {sdr_mix:.2f})", flush=True)
            elig = est.numpy()
        elif INJECT.startswith("student:"):                # the separator student's eligible estimate (sep_net.py)
            import sep_net as SN
            if "student" not in _MASK:
                _MASK["student"] = SN.load(INJECT[len("student:"):])
            elig = SN.separate_array(_MASK["student"], (mix * k).astype(np.float32))
        elif INJECT == "teacher2":                       # BS-RoFormer vocals + demucs drums/bass
            import teacher_label as TL
            if "sep2" not in _MASK:
                _MASK["sep2"] = TL.separator2()
            elig = TL.separate_array2(_MASK["sep2"], (mix * k).astype(np.float32))["other"]
        else:
            import teacher_label as TL
            if "sep" not in _MASK:
                _MASK["sep"] = TL.separator()
            elig = TL.separate_array(_MASK["sep"], (mix * k).astype(np.float32))["other"]
        if INJECT_SMOOTH > 0:
            elig = smooth_envelope(elig, mix * k, INJECT_SMOOTH)
        inj = os.path.join(out, f"{tag}.inj.wav")
        T.write_wav(inj, elig)
        extra = ["--rear-audio", inj]
    elif TEACHER_MASK:                                 # htdemucs's own mask through the DSP: the ceiling of the distillation route
        import teacher_label as TL
        if "sep" not in _MASK:
            _MASK["sep"] = TL.separator()
        est = TL.separate_array(_MASK["sep"], (mix * k).astype(np.float32))   # vocals / drums / bass / other, already scaled
        extra = oracle_mask_args(est, 1.0, out, tag + ".teacher")
    elif MASK_DIR:
        mask_path = os.path.join(MASK_DIR, tag + ".msk")
        if not os.path.isfile(mask_path):
            raise FileNotFoundError(mask_path)
        extra = ["--protect-mask", mask_path]
    else:
        extra = protect_mask_args(src, out, tag, mask_model) if mask_model else \
            oracle_mask_args(stems, k, out, tag) if oracle else []
    try:
        subprocess.run(render.split() + args + extra + [src, dst], check=True,   # a command line (no spaces in our paths)
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        out6 = T.read_wav6(dst)
    except (subprocess.CalledProcessError, OSError, ValueError) as e:   # one bad song (D: stall) must not kill the table
        print(f"  SKIP {tag}: {e}", flush=True)
        return []
    print(f"  rendered {tag}", flush=True)
    on = np.ones(n, bool)
    nse = rear_error(out6, tag) if ORACLE_REF else {}
    nse["rear_rms_db"] = float(20 * np.log10(np.sqrt(((out6[4] ** 2 + out6[5] ** 2) / 2).mean()) + 1e-9))   # for level matching across conditions
    rows = []
    for name, stem in stems.items():
        lead = stem * k; rest = (mix - stem) * k
        if (lead ** 2).sum() < 1e-3 * (mix ** 2).sum():
            continue                                   # stem effectively silent here
        share, w, dt = T.rear_share(lead, rest, out6)
        m = T.metrics(share, w, dt, on)
        slope6, corr, ratio_db = level_dependence(lead, rest, share, dt)
        bands = dict(zip(BAND_KEYS, band_shares(lead, rest, out6)))
        rows.append({"song": tag, "genre": meta.get("genre", "?"), "stem": name, **bands, **nse,
                     "median_rear": m["median_rear"], "steady_std": m["steady_std"],
                     "range_1s": m["range_1s"], "worst_half_s": m["worst_half_s"],
                     "slope_per_6dB": slope6, "level_corr": corr, "level_db": ratio_db,
                     "frames": int(np.sum(~np.isnan(share)))})
    json.dump(rows, open(cache, "w"))
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
    ap.add_argument("--mask-dir", help="precomputed HSSRMSK0 files named <song>.msk (test-mask build)")
    ap.add_argument("--oracle-mask", action="store_true", help="protect mask from the real stems (upper bound)")
    ap.add_argument("--render-extra", default="", help="extra renderer flags, e.g. '--mask-drive 1.0'")
    ap.add_argument("--oracle-amp", action="store_true", help="mask (oracle or model) as amplitude gain (sqrt of the eligibility) for --mask-drive")
    ap.add_argument("--mask-thresh", type=float, default=0.0)
    ap.add_argument("--mask-sharpen", type=float, default=1.0)
    ap.add_argument("--mask-eligible", default="", help="roles allowed rear at routing time, e.g. 'other' (default other+cymbals)")
    ap.add_argument("--mask-presence", type=float, default=0.0, help="full nets: e * (1 - alpha * band vocal presence) at routing")
    ap.add_argument("--resume", action="store_true", help="reuse per-song results already in --out")
    ap.add_argument("--inject-smooth", type=float, default=0.0, help="ms of release for the per-band gain envelope on the injected estimate")
    ap.add_argument("--teacher-mask", action="store_true", help="mask from htdemucs's stems (ceiling of the distillation route)")
    ap.add_argument("--oracle-ref", default="", help="dir with the oracle condition's *.6ch.wav renders: rear reconstruction error vs them")
    ap.add_argument("--inject", default="", help="interface test: eligible audio through the object path: oracle | teacher | teacher2 | student:CKPT")
    ap.add_argument("--split-file", help="masknet_cache/split.json")
    ap.add_argument("--split", default="dev", help="with --split-file: which split to take songs from")
    a = ap.parse_args()
    global ORACLE_AMP, MASK_THRESH, MASK_SHARPEN, MASK_ELIGIBLE, RESUME, TEACHER_MASK, ORACLE_REF, INJECT, INJECT_SMOOTH, MASK_DIR
    ORACLE_AMP = a.oracle_amp; RESUME = a.resume; TEACHER_MASK = a.teacher_mask; ORACLE_REF = a.oracle_ref; INJECT = a.inject
    INJECT_SMOOTH = a.inject_smooth
    MASK_DIR = a.mask_dir or ""
    if MASK_DIR and (a.inject or a.teacher_mask or a.mask_model or a.oracle_mask):
        ap.error("--mask-dir cannot be combined with another mask or injection source")
    MASK_THRESH, MASK_SHARPEN, MASK_ELIGIBLE = a.mask_thresh, a.mask_sharpen, a.mask_eligible
    import mask_net as MN; MN.PRESENCE_ALPHA = a.mask_presence
    os.makedirs(a.out, exist_ok=True)
    args = T.VARIANTS[a.variant] + ["--model", a.model] + (["--guard-model", a.guard] if a.guard else []) + a.render_extra.split()
    tracks = sorted(d for d in os.listdir(a.moisesdb) if os.path.isdir(os.path.join(a.moisesdb, d)))
    if a.split_file:
        wanted = {t for t, v in json.load(open(a.split_file))["split"].items() if v == a.split}
        tracks = [t for t in tracks if t in wanted]
    rng = np.random.default_rng(a.seed)
    picked = [os.path.join(a.moisesdb, t) for t in rng.permutation(tracks)[:a.songs]]

    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(a.jobs) as ex:
        results = list(ex.map(lambda d: analyse_song(a.render, args, a.out, d, a.seconds, a.mask_model, a.oracle_mask), picked))
    rows = [r for rs in results for r in rs]
    keys = ["song", "genre", "stem", "median_rear", "steady_std", "range_1s", "worst_half_s",
            "slope_per_6dB", "level_corr", "level_db", "frames"] + BAND_KEYS
    keys += ["rear_rms_db"]
    per = {r["song"]: r["rear_rms_db"] for r in rows if "rear_rms_db" in r}
    if per:
        print(f"rear level (Ls/Rs RMS, {len(per)} songs): median {np.median(list(per.values())):.1f} dBFS")
    if ORACLE_REF:
        keys += ["rear_nse", "rear_nse_gainfit", "rear_nse_treble", "rear_nse_treble_gainfit", "rear_lag_ms"]
        per_song = {r["song"]: r for r in rows if "rear_nse" in r}
        if per_song:
            v = list(per_song.values())
            print(f"rear reconstruction error vs the oracle ({len(v)} songs, median): broadband {np.median([r['rear_nse'] for r in v]):.2f} "
                  f"(gain-fit {np.median([r['rear_nse_gainfit'] for r in v]):.2f}), treble {np.median([r['rear_nse_treble'] for r in v]):.2f} "
                  f"(gain-fit {np.median([r['rear_nse_treble_gainfit'] for r in v]):.2f}), lag med {np.median([r['rear_lag_ms'] for r in v]):.1f} ms")
    with open(os.path.join(a.out, "stems.csv"), "w", newline="") as fh:
        wr = csv.DictWriter(fh, keys); wr.writeheader(); wr.writerows(rows)

    print(f"{len(picked)} songs, variant {a.variant}" + (f" + protect mask {a.mask_model}" if a.mask_model else "")
          + (" + ORACLE protect mask from the real stems" if a.oracle_mask else "") + (f" {a.render_extra}" if a.render_extra else "")
          + (f" thresh {a.mask_thresh} sharpen {a.mask_sharpen}" if (a.mask_thresh or a.mask_sharpen != 1.0) else "") + "\n")
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
