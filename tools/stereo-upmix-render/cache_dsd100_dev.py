#!/usr/bin/env python3
"""Cache only DSD100 Dev real stems as seven-slot Track B TRAIN truth.

The published Test audio is never opened. First validate the complete archive
with inspect_dsd100.py --crc and record its SHA-256; pass that hash here.
The training mixture is the exact sum of the four selected real-stem slots.
"""

import argparse
import io
import json
import math
import os
import re
import zipfile
from pathlib import Path

import numpy as np

from mask_net import GROUPS, SR


ROLES = {"vocals": "lead_vocal", "drums": "drum_core",
         "bass": "bass", "other": "other"}
DEV_SOURCE = re.compile(r"(?:^|/)Sources/Dev/(\d{3} - [^/]+)/([^/]+)\.wav$", re.I)
DEV_MIX = re.compile(r"(?:^|/)Mixtures/Dev/(\d{3} - [^/]+)/mixture\.wav$", re.I)
CHUNK = SR


def manifest(zf):
    tracks = {}
    for name in zf.namelist():
        source = DEV_SOURCE.search(name)
        mixture = DEV_MIX.search(name)
        if source:
            title, role = source.groups()
            role = role.lower()
            if role not in ROLES:
                raise ValueError(f"unknown DSD100 Dev source role: {name}")
            files = tracks.setdefault(title, dict(sources={}, mixture=None))
            if role in files["sources"]:
                raise ValueError(f"duplicate DSD100 Dev source: {title} {role}")
            files["sources"][role] = name
        elif mixture:
            title = mixture.group(1)
            files = tracks.setdefault(title, dict(sources={}, mixture=None))
            if files["mixture"]:
                raise ValueError(f"duplicate DSD100 Dev mixture: {title}")
            files["mixture"] = name
    if len(tracks) != 50:
        raise ValueError(f"expected 50 DSD100 Dev tracks, found {len(tracks)}")
    if len({title[:3] for title in tracks}) != len(tracks):
        raise ValueError("duplicate DSD100 Dev numeric track id")
    for title, files in tracks.items():
        if set(files["sources"]) != set(ROLES) or not files["mixture"]:
            raise ValueError(f"incomplete DSD100 Dev track: {title}")
    return tracks


def wav(zf, name, seconds=None):
    import soundfile as sf
    from scipy.signal import resample_poly

    # ZipExtFile seeking can re-inflate the entire member. BytesIO gives
    # libsndfile normal seek behavior while limiting the working set to one stem.
    raw = zf.read(name)  # ZIP member CRC checked by zipfile.
    with sf.SoundFile(io.BytesIO(raw)) as audio:
        rate = audio.samplerate
        frames = -1 if seconds is None else int(seconds * rate)
        x = audio.read(frames=frames, dtype="float32", always_2d=True)
    del raw
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    if x.shape[1] != 2:
        raise ValueError(f"expected mono/stereo: {name}")
    x = x.T
    if rate != SR:
        divisor = math.gcd(rate, SR)
        x = resample_poly(x, SR // divisor, rate // divisor, axis=1).astype(np.float32)
    return x, rate


def cache_track(zf, title, files, out, archive_sha):
    track_id = "dsd100_dev_" + title[:3]
    final_audio, final_meta = out / (track_id + ".npy"), out / (track_id + ".json")
    if final_audio.exists() or final_meta.exists():
        raise FileExistsError(f"cache files already exist for {track_id}; use --resume")
    rates, lengths, parts = {}, {}, {}
    try:
        for role in ROLES:
            audio, rate = wav(zf, files["sources"][role])
            rates[role] = rate
            lengths[role] = audio.shape[1]
            parts[role] = audio
        n = min(lengths.values())
        if max(lengths.values()) - n > int(.15 * SR):
            raise ValueError(f"source length spread >150 ms: {title} {lengths}")
        if n < 60 * SR:
            raise ValueError(f"source too short for the 60 s harness: {title} {n / SR:.1f} s")

        peak_mix = peak_source = 0.0
        for start in range(0, n, CHUNK):
            stop = min(start + CHUNK, n)
            segment = [parts[role][:, start:stop] for role in ROLES]
            mix = sum(segment)
            peak_mix = max(peak_mix, float(np.abs(mix).max()))
            peak_source = max(peak_source, *(float(np.abs(x).max()) for x in segment))
        if peak_mix < 1e-5:
            raise ValueError(f"silent DSD100 Dev track: {title}")
        scale = min(.7 / peak_mix, .99 / max(peak_source, 1e-9))

        # The mastered mix is an alignment diagnostic; it is not a target.
        master, master_rate = wav(zf, files["mixture"], seconds=60)
        audit_n = min(master.shape[1], n, 60 * SR)
        selected = sum(parts[role][:, :audit_n] for role in ROLES)
        residual = master[:, :audit_n] - selected
        rel_db = 20 * math.log10((float(np.sqrt(np.mean(residual**2))) + 1e-12) /
                                 (float(np.sqrt(np.mean(selected**2))) + 1e-12))
        del master, selected, residual
        if rel_db > -20:
            raise ValueError(f"master/stem alignment or closure failed: {title} {rel_db:.1f} dB")

        new_audio = out / (track_id + ".npy.new")
        cached = np.zeros((len(GROUPS), 2, n), np.int16)
        group_ids = {role: GROUPS.index(group) for role, group in ROLES.items()}
        for start in range(0, n, CHUNK):
            stop = min(start + CHUNK, n)
            for role, group in group_ids.items():
                x = np.asarray(parts[role][:, start:stop]) * scale
                cached[group, :, start:stop] = np.rint(np.clip(x, -1, 1) * 32767).astype(np.int16)
        with new_audio.open("wb") as f:
            np.save(f, cached)
        del cached
        meta = dict(dataset="DSD100", published_split="Dev", cache_split="train",
                    title=title, archive_sha256=archive_sha, seconds=n / SR,
                    original_rates=rates, source_lengths=lengths,
                    groups=GROUPS, present=[group in ROLES.values() for group in GROUPS],
                    labels=[dict(file=files["sources"][role], group=ROLES[role]) for role in ROLES],
                    master_rate=master_rate, master_residual_db=round(rel_db, 3),
                    normalization_peak_before=peak_mix, scale=scale)
        new_meta = out / (track_id + ".json.new")
        new_meta.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
        os.replace(new_audio, final_audio)
        os.replace(new_meta, final_meta)
        return track_id, meta
    except Exception:
        for path in (out / (track_id + ".npy.new"), out / (track_id + ".json.new"),
                     final_audio, final_meta):
            path.unlink(missing_ok=True)
        raise
    finally:
        parts.clear()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-sha256", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--audit-only", action="store_true", help="inspect Dev manifest without reading audio")
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-fA-F]{64}", args.archive_sha256):
        parser.error("--archive-sha256 must be a 64-digit SHA-256 recorded after full CRC validation")
    with zipfile.ZipFile(args.archive) as zf:
        tracks = manifest(zf)
        print(f"DSD100 Dev manifest: {len(tracks)} complete four-source tracks; Test audio unopened")
        if args.audit_only:
            return
        args.out.mkdir(parents=True, exist_ok=True)
        split_path = args.out / "split.json"
        if split_path.exists():
            old = json.loads(split_path.read_text(encoding="utf-8"))
            if not args.resume or old.get("archive_sha256") != args.archive_sha256.lower():
                raise ValueError("existing split.json must match archive SHA and --resume")
        split = {}
        for title, files in sorted(tracks.items()):
            track_id = "dsd100_dev_" + title[:3]
            audio, meta = args.out / (track_id + ".npy"), args.out / (track_id + ".json")
            if args.resume and audio.exists() and meta.exists():
                old = json.loads(meta.read_text(encoding="utf-8"))
                if old.get("archive_sha256") != args.archive_sha256.lower() or old.get("title") != title:
                    raise ValueError(f"existing cache metadata differs: {track_id}")
                stack = np.load(audio, mmap_mode="r")
                if stack.dtype != np.int16 or stack.shape[0:2] != (len(GROUPS), 2):
                    raise ValueError(f"invalid existing cache: {track_id}")
                print(f"RESUMED {track_id} {stack.shape[2] / SR:.1f}s", flush=True)
            else:
                if audio.exists() or meta.exists():
                    raise FileExistsError(f"partial or unrequested overwrite: {track_id}")
                _, result = cache_track(zf, title, files, args.out, args.archive_sha256.lower())
                print(f"CACHED {track_id} {result['seconds']:.1f}s "
                      f"master residual {result['master_residual_db']:.1f} dB", flush=True)
            split[track_id] = "train"
        payload = dict(split=split, groups=GROUPS, sr=SR, source="DSD100 Dev only",
                       archive_sha256=args.archive_sha256.lower())
        new_split = args.out / "split.json.new"
        new_split.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        os.replace(new_split, split_path)
        print(f"DONE {len(split)} TRAIN tracks; no TEST audio opened", flush=True)


if __name__ == "__main__":
    main()
