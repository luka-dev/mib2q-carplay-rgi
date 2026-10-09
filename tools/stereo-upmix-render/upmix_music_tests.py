#!/usr/bin/env python3
"""Stability metrics of the HSSR upmix on a folder of ordinary songs (no stems).

Every file is decoded with ffmpeg (48 kHz, 16-bit stereo), the loudest 60 s
are rendered with each knob set, and the whole-mix behaviour is measured:
rear share per band, how much that share moves over time (250 ms and 1 s),
front/rear treble vs the dry mix.  Without stems there is no "which
instrument", so this complements upmix_stem_tests.py (MoisesDB) with volume:
thousands of real masters, the user's own library.

    python upmix_music_tests.py --render upmix_render.exe --ffmpeg ffmpeg.exe \
        --music D:\\music \
        --songs 200 --out D:\\upmix\\runs\\music-2026-09-25 [--keep 8]
"""
import argparse
import csv
import os
import subprocess
import sys
import wave

import numpy as np

# Windows consoles default to cp1252: never let a song title kill a run.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import upmix_behavior_tests as T  # noqa: E402

BANDS = [(300, 1000), (1000, 2500), (2500, 4000), (4000, 6000), (6000, 10000), (10000, 16000)]
VARIANTS = {"car_0922": T.CAR_0922, "candidate": T.SHIPPED}
VARIANTS.update({k: v for k, v in T.VARIANTS.items() if k.startswith("cand_")})


def decode(ffmpeg, path, out_wav):
    subprocess.run([ffmpeg, "-v", "error", "-y", "-i", path, "-vn", "-ac", "2", "-ar", "48000",
                    "-sample_fmt", "s16", out_wav], check=True)


def read_wav2(path):
    with wave.open(path, "rb") as w:
        assert w.getnchannels() == 2 and w.getframerate() == 48000 and w.getsampwidth() == 2
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, 2).T
    return x.astype(np.float32) / 32768


def loudest(x, seconds):
    win, hop = int(seconds * T.SR), int(5 * T.SR)
    if x.shape[1] <= win:
        return x
    e = np.cumsum((x ** 2).sum(0))
    idx = np.arange(0, x.shape[1] - win, hop)
    s = idx[int(np.argmax(e[idx + win - 1] - e[idx]))]
    return x[:, s:s + win]


def measure(mix, out6):
    """Whole-mix metrics.  Shares are rear/(front+rear) energy per band."""
    fb = np.fft.rfftfreq(T.N_FFT, 1 / T.SR)
    d = T.stft_pow(mix[0]) + T.stft_pow(mix[1])
    F = sum(T.stft_pow(out6[c]) for c in (0, 1, 2))
    R = sum(T.stft_pow(out6[c]) for c in (4, 5))
    n = min(len(d), len(F))
    d, F, R = d[:n], F[:n], R[:n]
    m = {}
    for lo, hi in BANDS:
        sel = (fb >= lo) & (fb < hi)
        f, r = F[:, sel].sum(), R[:, sel].sum()
        m[f"rear_{lo}_{hi}"] = r / (f + r) if f + r > 0 else np.nan
    hi = (fb >= 2500) & (fb < 10000)
    for per, key in ((23, "flick250"), (94, "range1s")):
        sh = []
        for s in range(0, n - per, per):
            f, r = F[s:s + per][:, hi].sum(), R[s:s + per][:, hi].sum()
            sh.append(r / (f + r) if f + r > 0 else np.nan)
        sh = np.array(sh)
        m[key] = np.nanstd(sh) if key == "flick250" else (np.nanmax(sh) - np.nanmin(sh))
    treble = (fb >= 2000) & (fb < 16000)
    m["front_hi_db"] = 10 * np.log10(F[:, treble].sum() / (d[:, treble].sum() + 1e-9))
    m["rear_hi_db"] = 10 * np.log10(R[:, treble].sum() / (d[:, treble].sum() + 1e-9))
    body = (fb >= 300) & (fb < 2000)
    m["rear_mid_db"] = 10 * np.log10(R[:, body].sum() / (d[:, body].sum() + 1e-9))
    return m


def one(a, model, path, keep, index):
    # ASCII work names: the mingw renderer opens files through the ANSI fopen,
    # which cannot see Cyrillic or accented song names.  The real name goes
    # to the CSV and to a names.txt next to the kept renders.
    title = os.path.splitext(os.path.basename(path))[0]
    tag = f"{index:04d}"
    src = os.path.join(a.out, "src", tag + ".wav")
    os.makedirs(os.path.dirname(src), exist_ok=True)
    try:
        decode(a.ffmpeg, path, src)
        x = read_wav2(src)
    except Exception as e:  # noqa: BLE001 - one bad file must not stop the run
        return [{"song": title, "variant": "ERROR", "error": str(e)[:80]}]
    x = loudest(x, a.seconds)
    x = x / (np.abs(x).max() + 1e-9) * 0.7
    T.write_wav(src, x)
    rows = []
    for vn, args in VARIANTS.items():
        dst = os.path.join(a.out, "ab" if keep else "tmp", f"{tag}.{vn}.6ch.wav")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        r = subprocess.run([a.render] + args + model + [src, dst],
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        if r.returncode:
            rows.append({"song": title, "variant": "ERROR", "error": r.stderr.decode(errors="replace")[-80:]})
            continue
        out6 = T.read_wav6(dst)
        if not keep:
            os.remove(dst)
        rows.append({"song": title, "variant": vn, "file": tag, **measure(x, out6)})
    if not keep:
        os.remove(src)
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True)
    ap.add_argument("--ffmpeg", default="ffmpeg")
    ap.add_argument("--model", default="", help="passed through to the renderer; unused since the Universal renderer was retired (2026-10-09)")
    ap.add_argument("--guard")
    ap.add_argument("--music", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--songs", type=int, default=100)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--keep", type=int, default=8, help="keep the A/B renders of the first N songs")
    ap.add_argument("--jobs", type=int, default=max(2, (os.cpu_count() or 4) // 2))
    ap.add_argument("--variants", default="car_0922,candidate", help="comma list of VARIANTS keys")
    a = ap.parse_args()
    global VARIANTS
    VARIANTS = {k: VARIANTS[k] for k in a.variants.split(",")}
    os.makedirs(a.out, exist_ok=True)
    model = (["--model", a.model] if a.model else []) + (["--guard-model", a.guard] if a.guard else [])
    files = [os.path.join(r, f) for r, _, fs in os.walk(a.music) for f in fs
             if f.lower().endswith((".mp3", ".m4a", ".flac", ".wav", ".aac", ".ogg", ".opus", ".wma"))]
    rng = np.random.default_rng(a.seed)
    picked = [files[i] for i in rng.permutation(len(files))[:a.songs]]

    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(a.jobs) as ex:
        results = list(ex.map(lambda ip: one(a, model, ip[1], ip[0] < a.keep, ip[0]), enumerate(picked)))
    with open(os.path.join(a.out, "names.txt"), "w", encoding="utf-8") as fh:
        for i, pth in enumerate(picked):
            fh.write(f"{i:04d}\t{pth}\n")
    rows = [r for rs in results for r in rs]
    keys = sorted({k for r in rows for k in r}, key=lambda k: (k not in ("song", "variant", "file"), k))
    with open(os.path.join(a.out, "music.csv"), "w", newline="", encoding="utf-8") as fh:
        wr = csv.DictWriter(fh, keys); wr.writeheader(); wr.writerows(rows)
    errors = [r for r in rows if r["variant"] == "ERROR"]
    rows = [r for r in rows if r["variant"] != "ERROR"]
    print(f"{len(files)} files found, {len(picked)} picked, {len(errors)} failed to decode\n")
    metric_keys = [k for k in keys if k not in ("song", "variant", "file", "error")]
    print(f"{'variant':10s}" + "".join(f"{k[5:] if k.startswith('rear_') else k:>13s}" for k in metric_keys))
    for vn in VARIANTS:
        rs = [r for r in rows if r["variant"] == vn]
        print(f"{vn:10s}" + "".join(f"{np.nanmedian([r[k] for r in rs]):13.3f}" for k in metric_keys))
    print("\nmedians over songs.  rear_* = rear share of the whole mix in that band; flick250 = std of the "
          "2.5-10 kHz rear share at 250 ms; range1s = max-min of its 1 s medians; *_db = vs the dry mix.")
    swing = {vn: sum(1 for r in rows if r["variant"] == vn and r["range1s"] > 0.30) for vn in VARIANTS}
    print("songs with a >0.30 swing of the treble rear share within 60 s: " +
          ", ".join(f"{vn} {n}/{len(picked) - len(errors)}" for vn, n in swing.items()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
