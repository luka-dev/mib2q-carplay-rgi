#!/usr/bin/env python3
"""Curated OnAir stems -> MoisesDB-compatible seven-slot truth caches at 48 kHz.

Only mapped stems enter a synthetic mixture. The mastered WAV is used to audit
timing, never as a training target or to force the selected stem sum to close.
"""
import argparse
import json
import math
import os
from pathlib import Path

import numpy as np

from mask_net import GROUPS, SR


def _wav(path):
    import soundfile as sf
    from scipy.signal import resample_poly

    x, rate = sf.read(path, dtype="float32", always_2d=True)
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    if x.shape[1] != 2:
        raise ValueError(f"expected mono/stereo: {path}")
    x = x.T
    if rate != SR:
        factor = math.gcd(rate, SR)
        x = resample_poly(x, SR // factor, rate // factor, axis=1).astype(np.float32)
    return x, rate


def _master_alignment(master, selected):
    """Diagnostic lag and correlation; no shift or mastering residual is trained."""
    from scipy.signal import correlate

    n = min(master.shape[1], selected.shape[1], 60 * SR)
    if n < SR:
        return None, None
    stride = 16
    a = master[:, :n].mean(axis=0)[::stride]
    b = selected[:, :n].mean(axis=0)[::stride]
    a = a - a.mean()
    b = b - b.mean()
    cross = correlate(a, b, mode="full", method="fft")
    centre = len(b) - 1
    radius = int(0.5 * SR / stride)
    segment = cross[centre - radius:centre + radius + 1]
    lag = int(np.argmax(segment)) - radius
    if lag >= 0:
        aa, bb = a[lag:], b[:len(b) - lag]
    else:
        aa, bb = a[:len(a) + lag], b[-lag:]
    corr = float(np.dot(aa, bb) / (np.linalg.norm(aa) * np.linalg.norm(bb) + 1e-12))
    return lag * stride * 1000 / SR, corr


def _audit_track(root, track):
    import soundfile as sf

    folder = root / track["directory"]
    if not folder.is_dir():
        raise FileNotFoundError(folder)
    files = [track["mix"]] + [name for names in track["sources"].values() for name in names]
    if len(files) != len(set(files)):
        raise ValueError(f"duplicate source or mix: {track['id']}")
    if set(track["sources"]) - set(GROUPS):
        raise ValueError(f"unknown cache slot: {track['id']}")
    info = []
    for name in files:
        path = folder / name
        if not path.is_file():
            raise FileNotFoundError(path)
        wav = sf.info(path)
        info.append((name, wav.samplerate, wav.channels, wav.frames / wav.samplerate))
    stem_durations = [v[3] for v in info[1:]]
    spread = max(stem_durations) - min(stem_durations)
    return info, spread


def _cache_track(root, out, track):
    folder = root / track["directory"]
    parts = []
    original_rates = {}
    for group, names in track["sources"].items():
        for name in names:
            x, rate = _wav(folder / name)
            parts.append((GROUPS.index(group), name, x))
            original_rates[name] = rate
    n = min(x.shape[1] for _, _, x in parts)
    if max(x.shape[1] for _, _, x in parts) - n > int(0.15 * SR):
        raise ValueError(f"source length mismatch >150 ms: {track['id']}")
    stack = np.zeros((len(GROUPS), 2, n), np.float32)
    for group, _, x in parts:
        stack[group] += x[:, :n]
    selected = stack.sum(axis=0)
    master, _ = _wav(folder / track["mix"])
    lag_ms, corr = _master_alignment(master, selected)
    if corr is None or corr < 0.35:
        raise ValueError(f"master/stem timing audit failed: {track['id']} corr={corr}")
    peak = float(np.abs(selected).max()) + 1e-9
    stack *= 0.7 / peak
    cached = (np.clip(stack, -1, 1) * 32767).astype(np.int16)
    path = out / (track["id"] + ".npy")
    with open(str(path) + ".new", "wb") as f:
        np.save(f, cached)
    os.replace(str(path) + ".new", path)
    meta = {
        "dataset": "OnAir-Music-Dataset-v4", "track": track["directory"],
        "mix": track["mix"], "present": [bool(np.any(cached[g])) for g in range(len(GROUPS))],
        "labels": [{"file": name, "group": GROUPS[group]} for group, name, _ in parts],
        "original_rates": original_rates, "seconds": n / SR,
        "master_lag_ms": lag_ms, "master_correlation": corr,
        "normalization_peak_before": peak,
    }
    with open(out / (track["id"] + ".json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    return meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("--map", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--audit-only", action="store_true")
    a = ap.parse_args()
    root, out = Path(a.source), Path(a.out)
    manifest = json.loads(Path(a.map).read_text(encoding="utf-8"))
    tracks = manifest["tracks"]
    if len({t["id"] for t in tracks}) != len(tracks):
        raise ValueError("duplicate OnAir cache id")
    for track in tracks:
        info, spread = _audit_track(root, track)
        print(f"AUDIT {track['id']} stems={len(info)-1} length_spread={spread:.3f}s ",
              ", ".join(f"{rate}Hz/{channels}ch" for _, rate, channels, _ in info), flush=True)
        if spread > 0.15:
            raise ValueError(f"source length mismatch >150 ms: {track['id']}")
    if a.audit_only:
        return
    out.mkdir(parents=True, exist_ok=True)
    split = {}
    for track in tracks:
        meta = _cache_track(root, out, track)
        split[track["id"]] = "train"
        print(f"CACHED {track['id']} {meta['seconds']:.1f}s lag={meta['master_lag_ms']:.1f}ms "
              f"corr={meta['master_correlation']:.3f}", flush=True)
    with open(out / "split.json", "w", encoding="utf-8") as f:
        json.dump({"split": split, "groups": GROUPS, "sr": SR,
                   "source": str(root), "mapping": str(Path(a.map))}, f, indent=2)
    print(f"DONE {len(split)} TRAIN tracks; no DEV/TEST tracks", flush=True)


if __name__ == "__main__":
    main()
