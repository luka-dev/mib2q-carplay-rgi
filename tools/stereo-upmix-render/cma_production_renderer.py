#!/usr/bin/env python3
"""Bounded CMA-ES pilot on Track A's frozen mask through the real C renderer.

This is an exploratory six-song DEV screen, not a controller redesign or
product validation. It tests whether static product knobs can escape the
instrument/protected-stem trade-off before a more expensive controller search.
No package installation or GPU inference is required.
"""

import argparse
import csv
import json
import math
from pathlib import Path

import numpy as np

import evaluate_oracle_renderer_probe as evaluate
import upmix_behavior_tests as behavior
import upmix_stem_tests as harness


SONGS = (
    "Holy Magick - Below",             # keys, guitar, bass, drums
    "Holy Magick - Take Another Line", # keys, piano, guitar, protected stems
    "Holy Magick - Wake Up So",        # one of the three string songs
    "James Graydon - Our Love",        # strings, piano, guitar
    "Juniper - God Knows",             # strings, exposed voice
    "Modified Freq - Real",           # high vocal leakage
)
PARAMS = (
    ("mask_drive_prod", "--mask-drive-prod", 0.5, 1.8, 1.5),
    ("vocal_gate", "--vocal-gate", 0.2, 0.9, 0.5),
    ("surround", "--surround", 0.25, 0.5, 0.4),
    ("front_retention", "--front-retention", 0.4, 0.8, 0.5),
    ("rear_air_db", "--rear-air", -40.0, -6.0, -9.0),
    ("rear_body_db", "--rear-body", -10.0, 0.0, -4.0),
    ("mask_gamma", "--mask-gamma", 0.6, 1.0, 1.0),
    ("mask_treble_db", "--mask-treble", -6.0, 3.0, 0.0),
    ("front_bass_db", "--front-bass", -6.0, 0.0, -3.0),
    ("front_treble_db", "--front-treble", -6.0, 0.0, 0.0),
)
INSTRUMENTS = {"guitar": 0.15, "other_keys": 0.15, "piano": 0.10, "bowed_strings": 0.14}
PROTECTED = {"vocals": 0.02, "bass": 0.06, "drums": 0.08}


def summarize(rows):
    return evaluate.summary(rows)


def objective(s, reference_rms):
    shares = s["shares"]
    missing = sum(max(0.0, 1.0 - (shares[k] or 0.0) / limit) for k, limit in INSTRUMENTS.items())
    leaking = sum(max(0.0, (shares[k] or 0.0) / limit - 1.0) for k, limit in PROTECTED.items())
    swings = max(0, len(s["swings"]) - 1) / len(SONGS)
    rms = max(0.0, abs(s["rear_rms_db"] - reference_rms) - 0.5)
    slopes = sum(max(0.0, abs(s["slopes"][k] or 0.0) - 0.05) / 0.05
                 for k in ("guitar", "other_keys", "wind"))
    delivery = sum(min(1.5, (shares[k] or 0.0) / limit) for k, limit in INSTRUMENTS.items())
    loss = 3.0 * leaking + 2.0 * missing + 2.0 * swings + rms + slopes - 0.15 * delivery
    return float(loss), dict(leaking=leaking, instrument_shortfall=missing,
                             excess_swing_fraction=swings, rear_rms_excess_db=rms,
                             slope_excess=slopes, delivery=delivery)


def flags(x):
    return [part for value, (_, flag, lo, hi, _) in zip(x, PARAMS)
            for part in (flag, f"{lo + value * (hi - lo):.6f}")]


def config(x):
    return {name: round(lo + float(value) * (hi - lo), 6)
            for value, (name, _, lo, hi, _) in zip(x, PARAMS)}


def cma_search(a, loaded, baseline):
    n = len(PARAMS)
    mean = np.array([(base - lo) / (hi - lo) for _, _, lo, hi, base in PARAMS])
    sigma = 0.18
    covariance = np.eye(n)
    path_sigma = np.zeros(n)
    path_c = np.zeros(n)
    population = a.population
    parents = population // 2
    weights = np.log(parents + 0.5) - np.log(np.arange(1, parents + 1))
    weights /= weights.sum()
    mueff = 1.0 / np.sum(weights ** 2)
    cc = (4 + mueff / n) / (n + 4 + 2 * mueff / n)
    cs = (mueff + 2) / (n + mueff + 5)
    c1 = 2 / ((n + 1.3) ** 2 + mueff)
    cmu = min(1 - c1, 2 * (mueff - 2 + 1 / mueff) / ((n + 2) ** 2 + mueff))
    damp = 1 + 2 * max(0, math.sqrt((mueff - 1) / (n + 1)) - 1) + cs
    expected_norm = math.sqrt(n) * (1 - 1 / (4 * n) + 1 / (21 * n * n))
    rng = np.random.default_rng(a.seed)
    common = behavior.VARIANTS["cand_noair"] + ["--model", str(a.model),
              "--guard-model", str(a.guard), "--mask-model", str(a.mask_bin)]
    result_path = a.out / "search.json"
    baseline_score, baseline_parts = objective(baseline, baseline["rear_rms_db"])
    report = dict(question=__doc__.strip(), songs=list(SONGS), parameters=[p[0] for p in PARAMS],
                  baseline=dict(config=config(mean), summary=baseline, score=baseline_score,
                                parts=baseline_parts), evaluations=[], generations=[])
    best = None
    for generation in range(a.generations):
        eigenvalues, vectors = np.linalg.eigh(covariance)
        eigenvalues = np.maximum(eigenvalues, 1e-12)
        transform = vectors @ np.diag(np.sqrt(eigenvalues))
        inverse = vectors @ np.diag(1 / np.sqrt(eigenvalues)) @ vectors.T
        points = []
        for candidate in range(population):
            x = np.clip(mean + sigma * (transform @ rng.standard_normal(n)), 0, 1)
            folder = a.out / f"g{generation:02d}_c{candidate:02d}"
            folder.mkdir(parents=True, exist_ok=True)
            rows = []
            for song in SONGS:
                track_id, item = loaded[song]
                one = harness.analyse_song(str(a.render), common + flags(x), str(folder),
                                           str(a.moisesdb / track_id), 60.0, loaded=item)
                if not one:
                    raise RuntimeError(f"C renderer failed: generation {generation}, candidate {candidate}, {song}")
                rows.extend(one)
                for suffix in (".wav", ".6ch.wav"):
                    (folder / (song + suffix)).unlink(missing_ok=True)
            s = summarize(rows)
            score, parts = objective(s, baseline["rear_rms_db"])
            record = dict(generation=generation, candidate=candidate, score=score, config=config(x),
                          summary=s, parts=parts)
            report["evaluations"].append(record)
            result_path.write_text(json.dumps(report, indent=2) + "\n")
            points.append((score, x))
            if best is None or score < best[0]:
                best = (score, record)
            print(f"g{generation} c{candidate}: score {score:.3f}, swings {len(s['swings'])}/{len(SONGS)}, "
                  f"voice {s['shares']['vocals']:.3f}, guitar {s['shares']['guitar']:.3f}", flush=True)
        points.sort(key=lambda item: item[0])
        old_mean = mean.copy()
        mean = np.sum([w * x for w, (_, x) in zip(weights, points[:parents])], axis=0)
        change = (mean - old_mean) / sigma
        path_sigma = (1 - cs) * path_sigma + math.sqrt(cs * (2 - cs) * mueff) * (inverse @ change)
        hsig = float(np.linalg.norm(path_sigma) /
                     math.sqrt(1 - (1 - cs) ** (2 * (generation + 1))) / expected_norm
                     < 1.4 + 2 / (n + 1))
        path_c = (1 - cc) * path_c + hsig * math.sqrt(cc * (2 - cc) * mueff) * change
        rank_mu = sum(w * np.outer((x - old_mean) / sigma, (x - old_mean) / sigma)
                      for w, (_, x) in zip(weights, points[:parents]))
        covariance = ((1 - c1 - cmu) * covariance +
                      c1 * (np.outer(path_c, path_c) + (1 - hsig) * cc * (2 - cc) * covariance) +
                      cmu * rank_mu)
        sigma *= math.exp((cs / damp) * (np.linalg.norm(path_sigma) / expected_norm - 1))
        report["generations"].append(dict(generation=generation, best_score=points[0][0],
                                          mean=config(mean), sigma=sigma))
        result_path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"best score {best[0]:.3f}; {best[1]['config']}", flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--render", type=Path, required=True)
    p.add_argument("--model", type=Path, required=True)
    p.add_argument("--guard", type=Path, required=True)
    p.add_argument("--mask-bin", type=Path, required=True)
    p.add_argument("--moisesdb", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--baseline-csv", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--generations", type=int, default=3)
    p.add_argument("--population", type=int, default=6)
    p.add_argument("--seed", type=int, default=1610)
    a = p.parse_args()
    if a.generations < 1 or a.population < 4 or a.population % 2:
        p.error("need at least one generation and an even population >= 4")
    for path in (a.render, a.model, a.guard, a.mask_bin, a.manifest, a.baseline_csv):
        if not path.is_file():
            p.error(f"missing input: {path}")
    mapping = json.loads(a.manifest.read_text(encoding="utf-8"))["song_track_ids"]
    if not set(SONGS) <= mapping.keys():
        p.error("frozen manifest lacks a selected DEV song")
    with a.baseline_csv.open(newline="", encoding="utf-8-sig") as f:
        base_rows = [r for r in csv.DictReader(f) if r["song"] in SONGS]
    baseline = summarize(base_rows)
    if len(baseline["songs"]) != len(SONGS):
        p.error("baseline CSV lacks a selected DEV song")
    a.out.mkdir(parents=True, exist_ok=True)
    harness.INJECT = ""
    harness.INJECT_SMOOTH = 0.0
    harness.ORACLE_REF = ""
    harness.MASK_DIR = ""
    harness.TEACHER_MASK = False
    harness.RESUME = True
    loaded = {}
    for song in SONGS:
        item = harness.load_song(a.moisesdb / mapping[song], 60.0)
        if len(item[1]) < 2:
            raise ValueError(f"missing true stems for {song}")
        loaded[song] = (mapping[song], item)
        print(f"loaded {song}", flush=True)
    cma_search(a, loaded, baseline)


if __name__ == "__main__":
    main()
