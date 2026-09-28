#!/usr/bin/env python3
"""Make a blind rear-only headphone diagnostic from cached owner songs.

This is not an in-car, cabin, or 5.1 listening test. The C renderer receives
the same stereo mix and either cached teacher "other" or student eligible audio
as its --rear-audio input. Only Ls/Rs are exported for checking rear content,
vocal leakage, and continuity. Teacher and student clips in a pair keep their
relative gain; one shared playback gain prevents clipping on headphones.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import subprocess

import numpy as np
import soundfile as sf

import sep_net

SR = 48000
CLIPS = (
    ("01", "t00084", 33, 57, "Audioslave - Show Me How to Live; vocal entry"),
    ("03", "t00435", 59, 83, "The Animals - House of the Rising Sun; vocal exit"),
    ("05", "t00105", 126, 150, "2CELLOS - Smells Like Teen Spirit; instrumental"),
    ("09", "t00028", 194, 218, "Bishop Briggs - The Way I Do; exposed vocal"),
)


def read_rear(path):
    x, sr = sf.read(path, dtype="float32", always_2d=True)
    if sr != SR or x.shape[1] != 6:
        raise ValueError(f"renderer output must be 48 kHz, six-channel: {path}")
    return x[:, 4:6]


def wav(path, x):
    sf.write(path, x, SR, subtype="PCM_16")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cache", type=Path, required=True)
    p.add_argument("--student", type=Path, required=True)
    p.add_argument("--renderer", type=Path, required=True)
    p.add_argument("--model", type=Path, required=True)
    p.add_argument("--guard", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--seed", type=int, default=1609)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    clips = a.out / "clips"
    work = a.out / "work"
    clips.mkdir(exist_ok=True)
    matched = a.out / "clips_level_matched"
    matched.mkdir(exist_ok=True)
    work.mkdir(exist_ok=True)
    torch = sep_net._torch()
    torch.set_num_threads(2)
    loaded = sep_net.load(str(a.student))
    rng = random.Random(a.seed)
    manifest = []
    key = {}
    for slot, cache_id, start, end, title in CLIPS:
        stems = np.load(a.cache / (cache_id + ".npy"), mmap_mode="r")
        if stems.shape[:2] != (4, 2) or stems.shape[2] < end * SR:
            raise ValueError(f"bad or too-short teacher cache {cache_id}: {stems.shape}")
        # Give the renderer 10 s of history, then export exactly the chosen 24 s.
        begin = (start - 10) * SR
        stop = end * SR
        crop = np.asarray(stems[:, :, begin:stop], dtype=np.float32) / 32768
        mix = crop.sum(axis=0)
        gain = 0.7 / (np.max(np.abs(mix)) + 1e-9)
        mix *= gain
        teacher = crop[3] * gain
        student = sep_net.separate_array(loaded, mix.astype(np.float32))
        mix_path = work / f"{slot}_mix.wav"
        wav(mix_path, mix.T)
        rear = {}
        for name, eligible in (("teacher", teacher), ("student", student)):
            eligible_path = work / f"{slot}_{name}_eligible.wav"
            out6 = work / f"{slot}_{name}_6ch.wav"
            wav(eligible_path, eligible.T)
            out6.unlink(missing_ok=True)  # renderer refuses to replace its output
            cmd = [str(a.renderer), "--mode", "hssr-universal", "--lfe", "assist",
                   "--lfe-gain", "0", "--surround", "0.40", "--front-bass", "-3",
                   "--front-retention", "0.5", "--bed-delays", "--bed-attack", "off",
                   "--rear-air", "-12", "--mask-drive", "1.0",
                   "--model", str(a.model), "--guard-model", str(a.guard),
                   "--rear-audio", str(eligible_path), str(mix_path), str(out6)]
            subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
            rear[name] = read_rear(out6)[10 * SR:].copy()
        shared_gain = min(1.0, 0.8 / (max(float(np.max(np.abs(x))) for x in rear.values()) + 1e-9))
        rms = {name: float(np.sqrt(np.mean(x ** 2))) for name, x in rear.items()}
        quiet_rms = min(rms.values())
        labels = ["A", "B"]
        rng.shuffle(labels)
        for label, name in zip(labels, ("teacher", "student")):
            path = clips / f"{slot}_{label}.wav"
            wav(path, rear[name] * shared_gain)
            # Equal-RMS copy for leak/timbre judgements. Only attenuate the
            # louder rear, so level matching can never clip or amplify noise.
            wav(matched / path.name, rear[name] * shared_gain * quiet_rms / (rms[name] + 1e-9))
            key[f"{slot}_{label}"] = name
            manifest.append(dict(slot=slot, file=path.name, title=title,
                                 rear_rms_db=round(float(20 * np.log10(np.sqrt(np.mean(rear[name] ** 2)) + 1e-9)), 2),
                                 sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        print(f"{slot}: {title}", flush=True)
    (a.out / "private_key.json").write_text(json.dumps(key, indent=2))
    (a.out / "manifest.json").write_text(json.dumps(dict(
        warning="Rear-only headphones; no front image, B&O amp, or cabin. Not a car verdict. clips keeps true rear level; clips_level_matched equalises pair RMS for leak/timbre checks.",
        student=str(a.student), teacher_cache=str(a.cache), clips=manifest), indent=2))


if __name__ == "__main__":
    main()
