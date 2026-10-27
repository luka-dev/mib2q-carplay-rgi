#!/usr/bin/env python3
"""Real-stem evaluation of scene_upmix.py vs the classical baselines on the local AlbumDB cache.

Per song (~/Desktop/AUDI_2/upmix-data/q4-albumdb-cache-a/albumdb-NN.{json,npy}): the mix is the SUM
OF THE STORED HEADS (not the master, so attribution is exact), loudest 60 s, peak 0.7.  Every
condition renders it to 6 ch; per head: median rear share in the cells the head owns (>= 10 dB,
> 300 Hz; upmix_behavior_tests.rear_share + metrics), relative swing = range_1s / median, and for
guitar/keys a behind-ness proxy = median over frames of the per-frame median over owned cells of
10 log10(rear/front).  Plus rear RMS dBFS and render seconds.

    python3 scene_albumdb_eval.py --songs 10 --jobs 9 [--seconds 60] [--out-dir DIR] [--conds matrix,pad,...]

Writes scene_albumdb_eval.md (table + command line) and scene_albumdb_eval.json into --out-dir.
"""
import argparse
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import upmix_behavior_tests as bt  # noqa: E402

SR = 48000
DATA = os.path.expanduser("~/Desktop/AUDI_2/upmix-data/q4-albumdb-cache-a")
PY = sys.executable
CONDS = {
    "matrix": [PY, "baseline_upmix.py", "--mode", "matrix"],
    "pad": [PY, "baseline_upmix.py", "--mode", "pad"],
    "a0": [PY, "scene_upmix.py", "--alpha", "0"],
    "a0.5": [PY, "scene_upmix.py", "--alpha", "0.5"],
    "a0.7": [PY, "scene_upmix.py", "--alpha", "0.7"],
    "a0.9": [PY, "scene_upmix.py", "--alpha", "0.9"],
    "a0.7-nocl": [PY, "scene_upmix.py", "--alpha", "0.7", "--no-cluster"],
    "a0.7-n512": [PY, "scene_upmix.py", "--alpha", "0.7", "--n-fft", "512"],
    "a0.7-wide": [PY, "scene_upmix.py", "--alpha", "0.7", "--wide-pair"],
    "a0.7-s15": [PY, "scene_upmix.py", "--alpha", "0.7", "--sigma-min", "0.15"],
    "a0.7-nomass": [PY, "scene_upmix.py", "--alpha", "0.7", "--no-member-mass"],
    "a0.7-fix": [PY, "scene_upmix.py", "--alpha", "0.7", "--sigma-min", "0.15", "--no-member-mass"],
    "a0.7-fullband": [PY, "scene_upmix.py", "--alpha", "0.7", "--side-lp-hz", "0"],
    # semantic stage with the ORACLE instrument mask from the heads (ceiling of "instruments behind");
    # a condition whose name starts with "inst" gets --instrument-mask <song>.elig.npy
    "inst-oracle": [PY, "scene_upmix.py", "--alpha", "0.7"],
    "inst-oracle-a09": [PY, "scene_upmix.py", "--alpha", "0.7", "--inst-alpha", "0.9"],
    "inst-oracle-s15": [PY, "scene_upmix.py", "--alpha", "0.7", "--sigma-min", "0.15"],
    "instfine-oracle": [PY, "scene_upmix.py", "--alpha", "0.7", "--inst-mask-nfft", "2048", "--inst-mask-hop", "512"],   # the same oracle on a 23 Hz grid
    "inst-oracle-noduck": [PY, "scene_upmix.py", "--alpha", "0.7", "--no-duck-from-mask"],
    "inst-oracle-duck1": [PY, "scene_upmix.py", "--alpha", "0.7", "--duck-depth", "1.0"],
    "inst-duckonly": [PY, "scene_upmix.py", "--alpha", "0.7", "--inst-alpha", "0.0"],                # the oracle protect mask as the duck, no semantic transfer
}
PROTECT_CLASSES = (0, 1, 2, 3, 4)       # lead / backing / other vocal, bass, drums: never behind (MoisesDB ORACLE_PROTECT = vocals, bass, drums)


def oracle_inst_mask(heads, cls, protect=PROTECT_CLASSES, n=512, hop=128, avg=4):
    """Eligibility [Tc, 257] on the hook's 512/128 grid averaged over 4 hops: instrument power / total power per cell."""
    win = np.hanning(n)
    def spec(x):
        idx = np.arange(0, x.shape[1] - n, hop)
        return sum(np.abs(np.fft.rfft(np.stack([x[c][i:i + n] * win for i in idx]), axis=1)) ** 2 for c in range(2))
    tot = None; inst = None
    for h, k in zip(heads, cls):
        p = spec(np.asarray(h, dtype=np.float64))
        tot = p if tot is None else tot + p
        if k not in protect:
            inst = p if inst is None else inst + p
    if inst is None:
        inst = np.zeros_like(tot)
    e = inst / (tot + 1e-12); Tc = e.shape[0] // avg * avg
    return e[:Tc].reshape(Tc // avg, avg, -1).mean(1).astype(np.float32)
CLASSES = ["lead_vocal", "backing_vocal", "other_vocal", "bass", "drums", "high_percussion", "percussion",
           "guitar", "keys", "strings_winds", "other_plucked", "other_unknown"]
SHORT = {0: "voc", 1: "bvoc", 2: "ovoc", 3: "bass", 4: "drums", 5: "hperc", 6: "perc", 7: "gtr", 8: "keys",
         9: "str", 10: "oplk", 11: "oth"}
TABLE_CLASSES = [0, 1, 3, 4, 5, 7, 8]
BEHIND_CLASSES = [7, 8]


# ---------------------------------------------------------------- data
def load_song(nn, seconds):
    """-> (meta, heads [P, 2, n] float32 of the loudest `seconds`, peak-normalised mix to 0.7)."""
    meta = json.load(open(f"{DATA}/albumdb-{nn}.json"))
    a = np.load(f"{DATA}/albumdb-{nn}.npy", mmap_mode="r")
    n = a.shape[0]
    blk = SR
    nb = n // blk
    e = np.zeros(nb)
    for s in range(0, nb, 20):                       # per-second mix energy, chunked read
        c = np.asarray(a[s * blk:min(s + 20, nb) * blk, 1:, :], dtype=np.float32).sum(1)
        e[s:s + len(c) // blk] = (c[: len(c) // blk * blk] ** 2).reshape(-1, blk, 2).sum((1, 2))
    win = min(seconds, nb)
    cs = np.r_[0, np.cumsum(e)]
    start = int(np.argmax(cs[win:] - cs[:-win])) * blk
    heads = np.asarray(a[start:start + win * blk, 1:, :], dtype=np.float32).transpose(1, 2, 0)  # [P,2,n]
    mix = heads.sum(0)
    g = 0.7 / max(float(np.abs(mix).max()), 1e-9)
    return meta, heads * g, start / SR


# ---------------------------------------------------------------- metrics
def head_cells(dl, dr):
    own = dl > 10 * (dr + 1e-12)
    own[:, : int(300 * bt.N_FFT / SR)] = False
    return own


def share_and_behind(own, front, rear):
    """Mirror of bt.rear_share on precomputed powers (+ per-frame behind-ness dB); checked against it once."""
    n = min(len(own), len(front))
    own, front, rear = own[:n], front[:n], rear[:n]
    per = 5
    shares, weight, behind = [], [], []
    for s in range(0, n - per, per):
        o = own[s:s + per]
        f = (front[s:s + per] * o).sum(); r = (rear[s:s + per] * o).sum()
        shares.append(r / (f + r) if f + r > 0 else np.nan)
        weight.append(o.sum())
        if o.sum():
            fr = front[s:s + per][o]; rr = rear[s:s + per][o]
            behind.append(np.median(10 * np.log10((rr + 1e-14) / (fr + 1e-14))))
        else:
            behind.append(np.nan)
    return np.array(shares), np.array(weight), per * bt.HOP / SR, np.array(behind)


def eval_render(heads, pows, owns, out6, check=False):
    """-> per-head dict(median, range_1s, rel_swing, behind_db) + rear_rms_db."""
    front = sum(bt.stft_pow(out6[c]) for c in (0, 1, 2))
    rear = sum(bt.stft_pow(out6[c]) for c in (4, 5))
    on = np.ones(heads.shape[2])
    res = {}
    for k in range(heads.shape[0]):
        share, weight, dt, behind = share_and_behind(owns[k], front, rear)
        if check and k == 0:
            s2, w2, _ = bt.rear_share(heads[0], heads[1:].sum(0), out6)
            # float32 rounding of mix - head vs the sum of the others flips a few threshold cells
            bad = ~np.isclose(np.nan_to_num(s2), np.nan_to_num(share), atol=1e-3)
            assert bad.mean() < 0.02 and (w2 != weight).mean() < 0.05, (bad.sum(), (w2 != weight).sum())
        m = bt.metrics(share, weight, dt, on)
        act = (weight > 20) & ~np.isnan(behind)
        res[k] = {"median": m["median_rear"], "range_1s": m["range_1s"],
                  "rel_swing": m["range_1s"] / m["median_rear"] if m["median_rear"] > 0 else np.nan,
                  "behind_db": float(np.nanmedian(behind[act])) if act.any() else np.nan,
                  "worst_half_s": m["worst_half_s"]}
    res["rear_rms_db"] = float(20 * np.log10(np.sqrt((out6[4:6] ** 2).mean()) + 1e-12))
    return res


def run_cond(name, mix_wav, out_dir, mask_path=None):
    out = os.path.join(out_dir, f"{os.path.basename(mix_wav)[:-4]}-{name}.wav")
    t0 = time.time()
    extra = []
    if name.startswith("instfine"):
        extra = ["--instrument-mask", mask_path.replace(".elig.npy", ".eligfine.npy")]
    elif name.startswith("inst"):
        extra = ["--instrument-mask", mask_path]
    subprocess.run(CONDS[name] + extra + [mix_wav, out], cwd=HERE, check=True, capture_output=True)
    return out, time.time() - t0


def eval_song(nn, conds, seconds, out_dir, jobs):
    meta, heads, t_start = load_song(nn, seconds)
    cls = meta["stored_heads"]
    mix_wav = os.path.join(out_dir, f"albumdb-{nn}-mix.wav")
    bt.write_wav(mix_wav, heads.sum(0))
    mix = heads.sum(0)
    pows = [bt.stft_pow(h[0]) + bt.stft_pow(h[1]) for h in heads]
    # rest = mix - head (coherent sum), exactly as bt.rear_share does it
    owns = [head_cells(p, bt.stft_pow((mix - h)[0]) + bt.stft_pow((mix - h)[1])) for p, h in zip(pows, heads)]
    mask_path = None
    if any(c.startswith("inst") for c in conds):
        mask_path = os.path.join(out_dir, f"albumdb-{nn}.elig.npy")
        np.save(mask_path, oracle_inst_mask(heads, cls))
        if any(c.startswith("instfine") for c in conds):
            np.save(mask_path.replace(".elig.npy", ".eligfine.npy"), oracle_inst_mask(heads, cls, n=2048, hop=512, avg=1))
    with ThreadPoolExecutor(jobs) as ex:
        rendered = list(ex.map(lambda c: run_cond(c, mix_wav, out_dir, mask_path), conds))
    song = {"title": meta["title"], "artist": meta["artist"], "start_s": t_start, "classes": cls, "conds": {}}
    for c, (out, secs) in zip(conds, rendered):
        r = eval_render(heads, pows, owns, bt.read_wav6(out), check=(c == conds[0]))
        song["conds"][c] = {"secs": secs, "rear_rms_db": r["rear_rms_db"],
                            "heads": {str(cls[k]): r[k] for k in range(len(cls))}}
        os.remove(out)
    return song


# ---------------------------------------------------------------- aggregate
def fmt(v, d=2):
    return "-" if v is None or (isinstance(v, float) and np.isnan(v)) else f"{v:.{d}f}"


def aggregate(songs, conds):
    rows = []
    for c in conds:
        row = {"cond": c}
        for k in TABLE_CLASSES:
            vals = [s["conds"][c]["heads"][str(k)]["median"] for s in songs if str(k) in s["conds"][c]["heads"]]
            row[SHORT[k]] = float(np.nanmedian(vals)) if vals else np.nan
        sw = [s["conds"][c]["heads"][str(k)]["rel_swing"] for s in songs for k in BEHIND_CLASSES
              if str(k) in s["conds"][c]["heads"]]
        row["rel_swing_gk"] = float(np.nanmedian(sw)) if sw else np.nan
        row["swinging"] = sum(any(h["range_1s"] > 0.30 for h in s["conds"][c]["heads"].values()) for s in songs)
        for k in BEHIND_CLASSES:
            vals = [s["conds"][c]["heads"][str(k)]["behind_db"] for s in songs if str(k) in s["conds"][c]["heads"]]
            row[f"behind_{SHORT[k]}"] = float(np.nanmedian(vals)) if vals else np.nan
        row["rear_rms_db"] = float(np.median([s["conds"][c]["rear_rms_db"] for s in songs]))
        row["secs"] = float(np.median([s["conds"][c]["secs"] for s in songs]))
        rows.append(row)
    return rows


def table(rows, n_songs):
    cols = ["cond"] + [SHORT[k] for k in TABLE_CLASSES] + ["rel_swing_gk", "swinging", "behind_gtr", "behind_keys",
                                                           "rear_rms_db", "secs"]
    hdr = ["cond", "voc", "bvoc", "bass", "drums", "hperc", "gtr", "keys", "swing g+k", f"swinging/{n_songs}",
           "behind gtr dB", "behind keys dB", "rear RMS dBFS", "s/render"]
    lines = ["| " + " | ".join(hdr) + " |", "|" + "---|" * len(hdr)]
    for r in rows:
        cells = [r["cond"]] + [fmt(r[c]) for c in cols[1:8]] + [fmt(r["rel_swing_gk"]), str(r["swinging"]),
                                                                fmt(r["behind_gtr"], 1), fmt(r["behind_keys"], 1),
                                                                fmt(r["rear_rms_db"], 1), fmt(r["secs"], 0)]
        lines.append("| " + " | ".join(cells) + " |")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--songs", type=int, default=10)
    ap.add_argument("--jobs", type=int, default=len(CONDS))
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--conds", default=",".join(CONDS))
    ap.add_argument("--out-dir", default=os.environ.get("TMPDIR", "/tmp"))
    a = ap.parse_args()
    conds = a.conds.split(",")
    os.makedirs(a.out_dir, exist_ok=True)
    songs = []
    for i in range(1, a.songs + 1):
        t0 = time.time()
        s = eval_song(f"{i:02d}", conds, a.seconds, a.out_dir, a.jobs)
        songs.append(s)
        print(f"[{i:02d}] {s['title'][:28]:28s} start {s['start_s']:5.0f}s  " +
              "  ".join(f"{c}:gtr={fmt(s['conds'][c]['heads'].get('7', {}).get('median', np.nan))}"
                        f"/voc={fmt(s['conds'][c]['heads'].get('0', {}).get('median', np.nan))}" for c in conds) +
              f"  ({time.time() - t0:.0f}s)", flush=True)
    rows = aggregate(songs, conds)
    md = table(rows, len(songs))
    cmd = "python3 " + " ".join(sys.argv)
    print(md)
    json.dump({"cmd": cmd, "songs": songs, "rows": rows}, open(os.path.join(a.out_dir, "scene_albumdb_eval.json"), "w"),
              indent=1, default=float)
    with open(os.path.join(a.out_dir, "scene_albumdb_eval.md"), "w") as f:
        f.write(f"# scene_upmix.py vs baselines on AlbumDB ({len(songs)} songs, loudest {a.seconds} s, mix = sum of heads)\n\n"
                f"Command: `{cmd}`\n\nColumns: median rear share per class (over songs of per-song median over owned "
                f"cells); swing g+k = median of range_1s/median for guitar+keys; swinging = songs with any head "
                f"range_1s > 0.30; behind = median over frames of per-frame median over owned cells of "
                f"10 log10(rear/front).\n\n{md}\n")


if __name__ == "__main__":
    main()
