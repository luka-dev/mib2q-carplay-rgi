#!/usr/bin/env python3
"""Non-deployable per-source persistent membership with separate fast protection.

MoisesDB's isolated stems supply oracle admission and protection cues. Each
eligible stem has its own causal activity latch; protected stems affect the
mask immediately. The C renderer still routes the original stereo mixture.
This tests the controller structure, not whether a cheap estimator exists.
"""
import argparse
import json
from pathlib import Path
import re

import numpy as np

import oracle_cue_mask as oracle
from persistent_rear_membership import write_mask

SR = 48000
CONTROL_DT = oracle.HOP * oracle.CONTROL / SR
FREQS = np.fft.rfftfreq(oracle.N_FFT, 1 / SR)
ADMISSION_BAND = (FREQS >= 300) & (FREQS < 4000)
PROTECTED = {"vocals", "bass", "drums"}


def latch(power, on_db=-42, off_db=-50, on_dwell=.3, off_dwell=1.5,
          cue_tau=.25, ramp=.15):
    """Causal source activity latch; returns a slow membership envelope."""
    if off_db >= on_db:
        raise ValueError("off threshold must be below on threshold")
    on_power, off_power = 10 ** (on_db / 10), 10 ** (off_db / 10)
    alpha = np.exp(-CONTROL_DT / cue_tau)
    step = min(1.0, CONTROL_DT / ramp)
    smoothed = float(power[0]) if len(power) else 0
    state = smoothed >= on_power
    membership = 1.0 if state else 0.0
    above = below = 0.0
    out = np.empty(len(power), np.float32)
    changes = 0
    for i, sample in enumerate(power):
        smoothed = alpha * smoothed + (1 - alpha) * float(sample)
        if state:
            below = below + CONTROL_DT if smoothed < off_power else 0.0
            if below >= off_dwell:
                state = False
                changes += 1
                below = above = 0.0
        else:
            above = above + CONTROL_DT if smoothed > on_power else 0.0
            if above >= on_dwell:
                state = True
                changes += 1
                below = above = 0.0
        membership += step * (float(state) - membership)
        out[i] = membership
    return out, changes


def one_song(track, out, a):
    meta, stems = oracle.harness.load_song(track, 60.0)
    n = min(x.shape[1] for x in stems.values())
    mix = sum(x[:, :n] for x in stems.values())
    scale = 0.7 / (np.max(np.abs(mix)) + 1e-9)
    powers = {}
    controls = None
    for name, audio in stems.items():
        raw = oracle.stem_power(audio[:, :n] * scale)
        count = len(raw) // oracle.CONTROL
        controls = count if controls is None else min(controls, count)
        powers[name] = raw[:count * oracle.CONTROL]
    powers = {name: p[:controls * oracle.CONTROL] for name, p in powers.items()}
    shape = next(iter(powers.values())).shape
    eligible = np.zeros(shape, np.float32)
    admitted = np.zeros(shape, np.float32)
    protected = np.zeros(shape, np.float32)
    states = {}
    for name, p in powers.items():
        if name in PROTECTED:
            weight = {"vocals": a.vocal_weight, "drums": a.drum_weight,
                      "bass": a.bass_weight}[name]
            protected += weight * p
        else:
            # Positive-frequency power is about half the time-domain power.
            cue_frames = p.reshape(controls, oracle.CONTROL, -1).mean(axis=1)
            cue_power = (2 * cue_frames[:, ADMISSION_BAND].sum(axis=1)).astype(np.float32)
            member, changes = ((np.ones(controls, np.float32), 0)
                               if a.membership_always_on else
                               latch(cue_power, a.on_db, a.off_db, a.on_dwell,
                                     a.off_dwell, a.cue_tau, a.ramp))
            eligible += p
            admitted += p * np.repeat(member, oracle.CONTROL)[:, None]
            states[name] = dict(changes=changes,
                                open_fraction=round(float(member.mean()), 4),
                                energy_admitted=round(float(np.sum(cue_power * member) /
                                                            (np.sum(cue_power) + 1e-12)), 4))
    denominator = eligible + protected + oracle.POWER_FLOOR
    # Match oracle_cue_mask exactly: divide per STFT frame, average each four
    # ratios into one C control, then exponentiate to an amplitude mask.
    bins = oracle.N_FFT // 2 + 1
    fast = np.clip(eligible / denominator, 0, 1).reshape(
        controls, oracle.CONTROL, bins).mean(axis=1) ** a.gamma
    member = np.clip(admitted / denominator, 0, 1).reshape(
        controls, oracle.CONTROL, bins).mean(axis=1) ** a.gamma
    eligible_control = eligible.reshape(controls, oracle.CONTROL, bins).mean(axis=1)
    loss = 1 - float(np.sum(eligible_control * member ** 2) /
                     (np.sum(eligible_control * fast ** 2) + 1e-12))
    tag = re.sub(r'[\\/:*?"<>|]', "_", f"{meta.get('artist', '?')} - {meta.get('song', '?')}")[:60]
    write_mask(out / (tag + ".msk"), 1 - member)
    return dict(song=tag, controls=controls, eligible_energy_loss=round(loss, 4),
                state_changes=sum(s["changes"] for s in states.values()), sources=states)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--moisesdb", type=Path, required=True)
    p.add_argument("--split-file", type=Path, required=True)
    p.add_argument("--split", default="dev")
    p.add_argument("--songs", type=int, default=20)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--vocal-weight", type=float, default=1)
    p.add_argument("--drum-weight", type=float, default=1)
    p.add_argument("--bass-weight", type=float, default=1)
    p.add_argument("--gamma", type=float, default=.5)
    p.add_argument("--on-db", type=float, default=-42)
    p.add_argument("--off-db", type=float, default=-50)
    p.add_argument("--on-dwell", type=float, default=.3)
    p.add_argument("--off-dwell", type=float, default=1.5)
    p.add_argument("--cue-tau", type=float, default=.25)
    p.add_argument("--ramp", type=float, default=.15)
    p.add_argument("--membership-always-on", action="store_true",
                   help="parity control: must reproduce oracle_cue_mask at matching weights")
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    if min(a.vocal_weight, a.drum_weight, a.bass_weight, a.gamma,
           a.on_dwell, a.off_dwell, a.cue_tau, a.ramp) <= 0:
        p.error("weights, gamma, dwell, time constants and ramp must be positive")
    if a.off_db >= a.on_db:
        p.error("off-db must be below on-db")
    a.out.mkdir(parents=True, exist_ok=True)
    split = json.loads(a.split_file.read_text())["split"]
    tracks = sorted(t for t in a.moisesdb.iterdir() if t.is_dir() and split.get(t.name) == a.split)
    rng = np.random.default_rng(a.seed)
    picked = [tracks[i] for i in rng.permutation(len(tracks))[:a.songs]]
    rows = []
    for track in picked:
        row = one_song(track, a.out, a)
        rows.append(row)
        print(f"{len(rows)}/{len(picked)} {row['song']} loss={row['eligible_energy_loss']:.3f} "
              f"changes={row['state_changes']}", flush=True)
    (a.out / "report.json").write_text(json.dumps(dict(config=vars(a), songs=rows),
                                                  indent=2, default=str))


if __name__ == "__main__":
    main()
