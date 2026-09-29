#!/usr/bin/env python3
"""Split the existing MoisesDB eligible cache slot into instrument-family sidecars.

The six sidecar slots must sum *exactly* to the already validated seven-slot
cache's `other` slot. Each source family retains its true-stem identity for
later controlled remix; DEV and TEST songs are never written here.
"""
import argparse
import json
import os
from pathlib import Path

import numpy as np

import mask_net as MN

GROUPS = ("guitar", "keys", "piano", "strings", "winds", "other")
ROLE = {"guitar": 0, "other_keys": 1, "piano": 2, "bowed_strings": 3, "wind": 4}
PROTECTED = {"vocals", "drums", "percussion", "bass"}


def _one(uid, raw_root, cache, out):
    import soundfile as sf
    from scipy.signal import resample_poly

    metadata = json.loads((raw_root / uid / "data.json").read_text(encoding="utf-8"))
    parts = [None] * len(GROUPS)
    labels = []
    for stem in metadata["stems"]:
        name = stem["stemName"]
        if name in PROTECTED:
            continue
        gi = ROLE.get(name, len(GROUPS) - 1)
        for item in stem["tracks"]:
            path = raw_root / uid / name / (item["id"] + ".wav")
            if not path.is_file():
                raise FileNotFoundError(path)
            wav, sr = sf.read(path, dtype="float32", always_2d=True)
            if sr != 44100 or wav.shape[1] != 2:
                raise ValueError(f"expected 44.1-kHz stereo: {uid} {name} {item['id']}")
            x = wav.T
            if parts[gi] is None:
                parts[gi] = x.copy()
            else:
                n = min(parts[gi].shape[1], x.shape[1])
                parts[gi] = parts[gi][:, :n] + x[:, :n]
            labels.append({"stem": name, "type": item.get("trackType"), "group": GROUPS[gi]})
    active = [x for x in parts if x is not None]
    if not active:
        raise ValueError(f"no eligible stems: {uid}")
    n = min(x.shape[1] for x in active)
    stack = np.zeros((len(GROUPS), 2, n), np.float32)
    for gi, x in enumerate(parts):
        if x is not None:
            stack[gi] = x[:, :n]
    stack = resample_poly(stack, 160, 147, axis=2).astype(np.float32)
    source = cache / (uid + ".npy")
    cached_n = MN.npy_header(source)[0][2]
    if stack.shape[2] < cached_n:
        raise ValueError(f"sidecar shorter than original eligible cache: {uid}")
    stack = stack[:, :, :cached_n]
    original = MN.read_window(str(source), 0, cached_n, slots=[6])[0]
    raw_sum = stack.sum(axis=0)
    sample = slice(None, None, 32)
    r = raw_sum[:, sample].astype(np.float64)
    o = original[:, sample].astype(np.float64) / 32767
    denom = float(np.sum(r * r))
    if denom < 1e-12:
        if np.any(original):
            raise ValueError(f"eligible raw/cached activity mismatch: {uid}")
        gain = 0.0
    else:
        gain = float(np.sum(r * o) / denom)
    quantized = (np.clip(stack * gain, -1, 1) * 32767).astype(np.int16)
    residual = original.astype(np.int32) - quantized.astype(np.int32).sum(axis=0)
    residual_rms = float(np.sqrt(np.mean(residual.astype(np.float64) ** 2)))
    residual_max = int(np.max(np.abs(residual)))
    if residual_rms > 3.0 or residual_max > 100:
        raise ValueError(f"eligible closure before quantization correction failed: {uid}, rms={residual_rms:.2f}, max={residual_max}")
    corrected_other = quantized[-1].astype(np.int32) + residual
    if corrected_other.min() < -32768 or corrected_other.max() > 32767:
        raise ValueError(f"eligible residual correction overflow: {uid}")
    quantized[-1] = corrected_other.astype(np.int16)
    if not np.array_equal(quantized.astype(np.int32).sum(axis=0), original.astype(np.int32)):
        raise AssertionError(f"sidecar closure failed: {uid}")
    dest = out / (uid + ".npy")
    with open(str(dest) + ".new", "wb") as f:
        np.save(f, quantized)
    os.replace(str(dest) + ".new", dest)
    with open(out / (uid + ".json"), "w", encoding="utf-8") as f:
        json.dump({"source": uid, "groups": GROUPS, "labels": labels,
                   "scale_from_existing_cache": gain, "precorrection_residual_rms_lsb": residual_rms,
                   "precorrection_residual_max_lsb": residual_max, "seconds": cached_n / MN.SR}, f, indent=2)
    return f"{uid} {cached_n / MN.SR:.1f}s labels={len(labels)} gain={gain:.5f} residual={residual_rms:.2f}/{residual_max} LSB"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--moisesdb", required=True)
    ap.add_argument("--cache", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--one", help="audit and write exactly one cached TRAIN song")
    ap.add_argument("--force", action="store_true", help="rebuild existing complete sidecars")
    a = ap.parse_args()
    raw_root, cache, out = Path(a.moisesdb), Path(a.cache), Path(a.out)
    split = MN.load_split(str(cache))
    train = split["train"]
    if a.one:
        if a.one not in train:
            raise ValueError("--one must name an existing TRAIN cache")
        train = [a.one]
    out.mkdir(parents=True, exist_ok=True)
    for idx, uid in enumerate(train, 1):
        existing = out / (uid + ".npy")
        if existing.exists() and (out / (uid + ".json")).exists() and not a.force:
            shape, dtype, _ = MN.npy_header(str(existing))
            base_n = MN.npy_header(str(cache / (uid + ".npy")))[0][2]
            if shape != (len(GROUPS), 2, base_n) or dtype != np.dtype("int16"):
                raise ValueError(f"incompatible existing sidecar: {uid}")
            print(f"[{idx}/{len(train)}] SKIP complete {uid}", flush=True)
            continue
        print(f"[{idx}/{len(train)}] {_one(uid, raw_root, cache, out)}", flush=True)
    if not a.one:
        with open(out / "split.json", "w", encoding="utf-8") as f:
            json.dump({"split": {uid: "train" for uid in train}, "groups": GROUPS,
                       "sr": MN.SR, "base_cache": str(cache)}, f, indent=2)
        print(f"DONE {len(train)} TRAIN instrument sidecars", flush=True)


if __name__ == "__main__":
    main()
