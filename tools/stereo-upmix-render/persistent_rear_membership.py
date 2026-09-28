#!/usr/bin/env python3
"""Offline causal membership/protection prototype for the real C renderer.

The separator's rear-audio estimate is evidence that an eligible stream is
present. It is never used as rear audio. Slow hysteretic membership multiplies
a separate fast protection mask; protection can close immediately at a vocal
entry even while membership remains. Output is HSSRMSK0 for --protect-mask.

This global membership is a deliberately small first prototype, not source
identity tracking. The oracle fast mask is an upper bound and cannot ship.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import stft

SR = 48000
N_FFT = 512
HOP = 128
CONTROL_HOPS = 4
CONTROL_S = HOP * CONTROL_HOPS / SR
N_BINS = N_FFT // 2 + 1


def read_mask(path):
    with open(path, "rb") as f:
        header = f.read(16)
        if header[:8] != b"HSSRMSK0":
            raise ValueError(f"{path}: wrong mask magic")
        frames, bins = np.frombuffer(header[8:], "<u4")
        if bins != N_BINS or frames == 0:
            raise ValueError(f"{path}: wrong mask geometry")
        data = np.frombuffer(f.read(), "<f4")
    if len(data) != frames * bins:
        raise ValueError(f"{path}: short mask")
    return data.reshape(frames, bins).copy()


def write_mask(path, protect):
    if protect.ndim != 2 or protect.shape[1] != N_BINS:
        raise ValueError("wrong mask shape")
    with open(path, "wb") as f:
        f.write(b"HSSRMSK0")
        f.write(np.asarray(protect.shape, dtype="<u4").tobytes())
        f.write(np.asarray(protect, dtype="<f4").tobytes())


def read_stereo(path):
    x, sr = sf.read(path, dtype="float32", always_2d=True)
    if sr != SR or x.shape[1] != 2:
        raise ValueError(f"{path}: expected 48-kHz stereo")
    return x.T


def control_power(path, controls):
    x = read_stereo(path)
    _, _, spec = stft(x, fs=SR, nperseg=N_FFT, noverlap=N_FFT - HOP,
                      boundary="zeros", padded=True, axis=-1)
    energy = np.sum(np.abs(spec) ** 2, axis=0).T  # [STFT frame, bin]
    wanted = controls * CONTROL_HOPS
    if len(energy) < wanted:
        raise ValueError(f"{path}: shorter than {controls} controls")
    return energy[:wanted].reshape(controls, CONTROL_HOPS, N_BINS).mean(axis=1)


def admission_cue(mix_power, estimate_power, lo_hz=300.0, hi_hz=4000.0):
    a = max(0, int(np.ceil(lo_hz * N_FFT / SR)))
    b = min(N_BINS, int(np.ceil(hi_hz * N_FFT / SR)))
    numerator = estimate_power[:, a:b].sum(axis=1)
    denominator = mix_power[:, a:b].sum(axis=1)
    return np.clip(np.sqrt(numerator / np.maximum(denominator, 1e-9)), 0, 1)


def membership(cue, on, off, attack_s, release_s, admit_s, drop_s, ramp_s):
    if not (0 <= off < on <= 1):
        raise ValueError("need 0 <= off < on <= 1")
    smooth = 0.0
    admitted = False
    above = below = 0
    gain = 0.0
    target_step = min(1.0, CONTROL_S / max(ramp_s, CONTROL_S))
    values = np.empty(len(cue), np.float32)
    changes = []
    for t, c in enumerate(cue):
        tau = attack_s if c > smooth else release_s
        alpha = np.exp(-CONTROL_S / max(tau, CONTROL_S))
        smooth = alpha * smooth + (1 - alpha) * float(c)
        above = above + 1 if smooth >= on else 0
        below = below + 1 if smooth <= off else 0
        if not admitted and above * CONTROL_S >= admit_s:
            admitted = True
            below = 0
            changes.append(dict(t_s=round(t * CONTROL_S, 3), state="admitted"))
        elif admitted and below * CONTROL_S >= drop_s:
            admitted = False
            above = 0
            changes.append(dict(t_s=round(t * CONTROL_S, 3), state="released"))
        gain = min(1.0, gain + target_step) if admitted else max(0.0, gain - target_step)
        values[t] = gain
    return values, changes


def one_song(mix_path, estimate_path, fast_path, output_path, cfg, true_eligible_path=None):
    protect = read_mask(fast_path)
    n = len(protect)
    mix = control_power(mix_path, n)
    estimate = control_power(estimate_path, n)
    cue = admission_cue(mix, estimate, cfg.lo_hz, cfg.hi_hz)
    member, changes = membership(cue, cfg.on, cfg.off, cfg.attack_s, cfg.release_s,
                                 cfg.admit_s, cfg.drop_s, cfg.ramp_s)
    fast = np.clip(1 - protect, 0, 1)
    if cfg.fast_is_energy:
        fast = np.sqrt(fast)
    out = fast * member[:, None]
    write_mask(output_path, 1 - out)
    weighted = mix * fast * fast
    total = float(weighted.sum())
    lost = float((weighted * (1 - member[:, None] ** 2)).sum() / max(total, 1e-12))
    true_loss = None
    if true_eligible_path is not None:
        true_power = control_power(true_eligible_path, n)
        true_weighted = true_power * fast * fast
        true_loss = float((true_weighted * (1 - member[:, None] ** 2)).sum() /
                          max(float(true_weighted.sum()), 1e-12))
    return dict(song=mix_path.stem, controls=n, admission_fraction=float(np.mean(member > 0.5)),
                membership_changes=len(changes), changes=changes,
                estimated_eligible_audio_loss=lost,
                true_eligible_audio_loss=true_loss,
                note="loss is relative to the same fast mask without membership")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--screen", type=Path, required=True, help="upmix_stem_tests screen with .wav and .inj.wav")
    p.add_argument("--fast-mask-dir", type=Path, required=True)
    p.add_argument("--true-eligible-screen", type=Path, help="oracle .inj.wav files for true eligible-audio loss")
    p.add_argument("--fast-suffix", default=".oracle.msk")
    p.add_argument("--fast-is-energy", action="store_true", help="sqrt the input eligibility like production")
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--lo-hz", type=float, default=300)
    p.add_argument("--hi-hz", type=float, default=4000)
    p.add_argument("--on", type=float, default=0.55)
    p.add_argument("--off", type=float, default=0.35)
    p.add_argument("--attack-s", type=float, default=0.8)
    p.add_argument("--release-s", type=float, default=2.0)
    p.add_argument("--admit-s", type=float, default=0.5)
    p.add_argument("--drop-s", type=float, default=1.0)
    p.add_argument("--ramp-s", type=float, default=0.2)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    rows = []
    for mix in sorted(a.screen.glob("*.wav")):
        if mix.name.endswith(".inj.wav") or mix.name.endswith(".6ch.wav"):
            continue
        estimate = a.screen / (mix.stem + ".inj.wav")
        fast = a.fast_mask_dir / (mix.stem + a.fast_suffix)
        if not (estimate.exists() and fast.exists()):
            continue
        true_eligible = None
        if a.true_eligible_screen:
            true_eligible = a.true_eligible_screen / (mix.stem + ".inj.wav")
            if not true_eligible.exists():
                raise FileNotFoundError(true_eligible)
        row = one_song(mix, estimate, fast, a.out / (mix.stem + ".msk"), a, true_eligible)
        rows.append(row)
        print(f"{row['song']}: changes {row['membership_changes']}, loss {row['estimated_eligible_audio_loss']:.3f}", flush=True)
    if not rows:
        raise ValueError("no matching source, estimate, and fast masks")
    (a.out / "report.json").write_text(json.dumps(dict(config=vars(a), songs=rows), indent=2, default=str))
    print(f"wrote {len(rows)} masks to {a.out}", flush=True)


if __name__ == "__main__":
    main()
