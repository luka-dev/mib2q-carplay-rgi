#!/usr/bin/env python3
"""Cache three owner-authorized Cambridge multitracks as TRAIN-only truth.

The synthetic training mixture is the exact sum of explicitly mapped tracks.
There is no released mix in these ZIPs to use as an independent closure check.
The owner will add the permission record to the repository separately.
"""

import argparse
import hashlib
import io
import json
import math
import os
import re
import zipfile
from pathlib import Path

import numpy as np

from mask_net import GROUPS, SR


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(4 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def members(zf, spec):
    files = {}
    for name in zf.namelist():
        if not name.lower().endswith(".wav"):
            continue
        match = re.fullmatch(r"(\d{2})_[^/]+\.wav", name.rsplit("/", 1)[-1], re.I)
        if not match:
            raise ValueError(f"unexpected WAV name: {name}")
        index = int(match.group(1))
        if index in files:
            raise ValueError(f"duplicate track index {index}")
        files[index] = name
    groups = spec["groups"]
    if set(groups) != set(GROUPS):
        raise ValueError("group map differs from seven-slot cache")
    assigned = [i for indices in groups.values() for i in indices]
    excluded = spec["exclude"]
    if len(assigned + excluded) != len(set(assigned + excluded)):
        raise ValueError("duplicate/overlapping track assignment")
    if set(assigned + excluded) != set(files):
        raise ValueError(f"unaccounted WAV indices: {set(files) ^ set(assigned + excluded)}")
    if not groups["lead_vocal"] or not groups["other"]:
        raise ValueError("trial requires lead vocal and eligible instruments")
    return files


def decode(zf, name):
    import soundfile as sf
    from scipy.signal import resample_poly

    raw = zf.read(name)
    with sf.SoundFile(io.BytesIO(raw)) as stream:
        rate = stream.samplerate
        x = stream.read(dtype="float32", always_2d=True)
    del raw
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    if x.shape[1] != 2:
        raise ValueError(f"expected mono/stereo source: {name}")
    x = x.T
    if rate != SR:
        gcd = math.gcd(rate, SR)
        x = resample_poly(x, SR // gcd, rate // gcd, axis=1).astype(np.float32)
    return x, rate


def cache_one(root, out, track_id, spec, *, resume=False):
    archive = root / (track_id + ".zip.part")
    if archive.stat().st_size != spec["zip_bytes"]:
        raise ValueError(f"incomplete ZIP: {track_id}")
    audio_path = out / (track_id + ".npy")
    meta_path = out / (track_id + ".json")
    archive_sha = sha256(archive)
    if resume and audio_path.exists() and meta_path.exists():
        meta = json.loads(meta_path.read_text(encoding="utf-8"))
        if meta.get("archive_sha256") != archive_sha:
            raise ValueError(f"cached archive hash changed: {track_id}")
        existing = np.load(audio_path, mmap_mode="r")
        if existing.dtype != np.int16 or existing.shape[:2] != (len(GROUPS), 2):
            raise ValueError(f"cached audio shape changed: {track_id}")
        return meta
    if audio_path.exists() or meta_path.exists():
        raise FileExistsError(f"partial or unrequested overwrite: {track_id}")

    with zipfile.ZipFile(archive) as zf:
        files = members(zf, spec)
        failed = zf.testzip()
        if failed is not None:
            raise ValueError(f"ZIP CRC failed: {track_id}: {failed}")
        parts = []
        rates = {}
        lengths = {}
        for group in GROUPS:
            for index in spec["groups"][group]:
                name = files[index]
                x, rate = decode(zf, name)
                parts.append((group, index, name, x))
                rates[str(index)] = rate
                lengths[str(index)] = x.shape[1]
    # Cambridge's multitracks are placed together at the same timeline origin.
    # Individual bounced tracks can stop as soon as their last event ends; e.g.
    # one Ajay backing-vocal file is 28 s while the main session is 267 s.
    # Preserve those later instrumental passages by padding only the tails.
    n = max(lengths.values())
    if n < 60 * SR:
        raise ValueError(f"source shorter than 60 s: {track_id}")
    stack = np.zeros((len(GROUPS), 2, n), np.float32)
    for group, _, _, x in parts:
        stack[GROUPS.index(group), :, :x.shape[1]] += x
    selected = stack.sum(axis=0)
    peak_mix = float(np.abs(selected).max())
    peak_stem = float(np.abs(stack).max())
    if peak_mix < 1e-5:
        raise ValueError(f"silent selected mixture: {track_id}")
    scale = min(.7 / peak_mix, .99 / max(peak_stem, 1e-9))
    cached = np.rint(np.clip(stack * scale, -1, 1) * 32767).astype(np.int16)
    new_audio = out / (track_id + ".npy.new")
    new_meta = out / (track_id + ".json.new")
    try:
        with new_audio.open("wb") as stream:
            np.save(stream, cached)
        meta = dict(dataset="Cambridge multitrack owner-authorized trial", cache_split="train",
                    artist=spec["artist"], title=spec["title"], seconds=n / SR,
                    archive_sha256=archive_sha, groups=GROUPS,
                    present=[bool(spec["groups"][group]) for group in GROUPS],
                    labels=[dict(index=index, file=name, group=group)
                            for group, index, name, _ in parts],
                    excluded_indices=spec["exclude"], original_rates=rates,
                    source_lengths=lengths, aligned_at_zero_assumed=True,
                    tail_padded_to_samples=n, normalization_peak_before=peak_mix,
                    normalization_scale=scale, synthetic_mix=True)
        new_meta.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
        os.replace(new_audio, audio_path)
        os.replace(new_meta, meta_path)
        return meta
    finally:
        new_audio.unlink(missing_ok=True)
        new_meta.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--map", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--only", help="cache one completed archive without publishing split.json")
    args = parser.parse_args()
    spec = json.loads(args.map.read_text(encoding="utf-8"))
    args.out.mkdir(parents=True, exist_ok=True)
    if args.only:
        if args.only not in spec["tracks"]:
            parser.error("--only ID is absent from the mapping")
        meta = cache_one(args.source, args.out, args.only, spec["tracks"][args.only], resume=args.resume)
        print(f"CACHED {args.only} {meta['seconds']:.1f}s; split.json not published", flush=True)
        return
    split = {}
    for track_id, track in spec["tracks"].items():
        meta = cache_one(args.source, args.out, track_id, track, resume=args.resume)
        print(f"CACHED {track_id} {meta['seconds']:.1f}s {len(meta['labels'])} mapped WAVs", flush=True)
        split[track_id] = "train"
    split_path = args.out / "split.json"
    payload = dict(split=split, groups=GROUPS, sr=SR,
                   source="Cambridge multitrack owner-authorized trial; TRAIN only")
    if split_path.exists():
        if not args.resume or json.loads(split_path.read_text(encoding="utf-8")) != payload:
            raise ValueError("existing split.json differs")
    else:
        temporary = args.out / "split.json.new"
        temporary.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        os.replace(temporary, split_path)
    print(f"DONE {len(split)} TRAIN-only tracks", flush=True)


if __name__ == "__main__":
    main()
