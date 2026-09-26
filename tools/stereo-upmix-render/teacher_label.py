#!/usr/bin/env python3
"""Teacher (HTDemucs) labels for the mask net — Phase 2 of
docs/audio/UPMIX_SOURCE_SEPARATION_PLAN.md — and the check the Codex review
demanded first: how often a teacher-confident "rear-eligible" cell is really
vocal / bass / drum core (false rear permission), measured against real stems.

  validate  run htdemucs_ft on MoisesDB DEV mixes (from the mask-net cache),
            compare its ideal-ratio masks at the car's STFT with the truth,
            report false rear permission and coverage per confidence threshold
  separate  run the teacher over a song folder (mp3 via ffmpeg), write
            [vocals, master - vocals] as 48 kHz int16 .npy + json (a compact
            two-slot cache: the sum IS the real master, and protection-only
            supervision needs nothing but the vocal share); mask_net expands
            it to the seven-slot layout when it loads a window

Teacher stems: drums / bass / other / vocals.  They cannot label lead vs
backing or drum core vs cymbals (Codex): teacher songs only ever supervise the
four-way merge, and only where the teacher is confident.
"""
import argparse
import json
import os
import subprocess
import sys
import time

import numpy as np

# Windows consoles default to cp1252: never let a song title kill a run.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN  # noqa: E402

TEACHER_STEMS = ("drums", "bass", "other", "vocals")
# truth roles (cache v2) -> teacher stems
TRUTH_TO_TEACHER = {"lead_vocal": "vocals", "other_vocal": "vocals", "drum_core": "drums", "cymbals": "drums",
                    "percussion": "drums", "bass": "bass", "other": "other"}


def separator(model="htdemucs_ft"):
    import torch
    from demucs.api import Separator
    return Separator(model=model, device="cuda" if torch.cuda.is_available() else "cpu", progress=False)


def separate_array(sep, x48):
    """x48 [2, n] float32 at 48 kHz -> dict stem -> [2, n] float32 at 48 kHz."""
    import torch
    import torchaudio
    src = torch.as_tensor(x48)
    x44 = torchaudio.functional.resample(src, MN.SR, sep.samplerate)
    _, stems = sep.separate_tensor(x44, sep.samplerate)
    out = {}
    for k, v in stems.items():
        y = torchaudio.functional.resample(v.cpu(), sep.samplerate, MN.SR).numpy()
        out[k] = y[:, :x48.shape[1]] if y.shape[1] >= x48.shape[1] else np.pad(y, ((0, 0), (0, x48.shape[1] - y.shape[1])))
    return out


def validate(a):
    import torch
    sep = separator(a.model)
    split = MN.load_split(a.cache)
    tracks = split[a.split][:a.songs]
    band_hz = np.fft.rfftfreq(MN.N_FFT, 1 / MN.SR)
    n = int(a.seconds * MN.SR)
    thresholds = (0.6, 0.7, 0.8, 0.9, 0.95)
    rows = []
    t0 = time.time()
    for i, t in enumerate(tracks):
        w = np.load(os.path.join(a.cache, t + ".npy"), mmap_mode="r")
        if w.shape[2] <= n:
            continue
        v = np.asarray(w[0]).astype(np.float32) + np.asarray(w[1]).astype(np.float32)
        e = np.cumsum((v ** 2).sum(0)); idx = np.arange(0, w.shape[2] - n, 5 * MN.SR)
        s = idx[int(np.argmax(e[idx + n - 1] - e[idx]))] if len(idx) else 0
        groups = np.asarray(w[:, :, s:s + n]).astype(np.float32) / 32768        # [7,2,n]
        mix = groups.sum(0)
        est = separate_array(sep, mix)
        # truth power per teacher stem at the car's STFT
        G = MN.stft(torch, torch.as_tensor(groups))                             # [7,2,T,257]
        P = (G.real ** 2 + G.imag ** 2).sum(1)                                  # [7,T,257]
        truth = {k: sum(P[MN.GROUPS.index(g)] for g, kk in TRUTH_TO_TEACHER.items() if kk == k) for k in TEACHER_STEMS}
        E = MN.stft(torch, torch.as_tensor(np.stack([est[k] for k in TEACHER_STEMS])))
        Pe = (E.real ** 2 + E.imag ** 2).sum(1)                                 # [4,T,257]
        tot_e = Pe.sum(0) + 1e-9
        tot_t = sum(truth.values()) + 1e-9
        band_ok = torch.as_tensor(band_hz >= 220)                               # the DSP never rears below 220 Hz
        m_t = {k: Pe[j] / tot_e for j, k in enumerate(TEACHER_STEMS)}
        m_truth = {k: truth[k] / tot_t for k in TEACHER_STEMS}
        protected_truth = m_truth["vocals"] + m_truth["bass"] + m_truth["drums"]   # drums whole: conservative
        for th in thresholds:
            conf = (m_t["other"] > th) & band_ok                                # teacher says: confidently rear-eligible
            cell_e = tot_t * conf
            false_perm = float((protected_truth * cell_e).sum() / (cell_e.sum() + 1e-9))
            vocal_perm = float((m_truth["vocals"] * cell_e).sum() / (cell_e.sum() + 1e-9))
            coverage = float(cell_e.sum() / (tot_t * band_ok).sum())
            other_recall = float((m_truth["other"] * cell_e).sum() / ((m_truth["other"] * tot_t * band_ok).sum() + 1e-9))
            # the other direction (protection): cells the teacher calls confidently VOCAL
            vconf = (m_t["vocals"] > th) & band_ok
            vcell = tot_t * vconf
            vocal_precision = float((m_truth["vocals"] * vcell).sum() / (vcell.sum() + 1e-9))
            vocal_recall = float((m_truth["vocals"] * vcell).sum() / ((m_truth["vocals"] * tot_t * band_ok).sum() + 1e-9))
            rows.append({"song": t, "th": th, "false_perm": false_perm, "vocal_perm": vocal_perm,
                         "coverage": coverage, "other_recall": other_recall,
                         "vocal_precision": vocal_precision, "vocal_recall": vocal_recall})
        # vocal-only agreement, the V12/V15 failure axis: vocal energy the teacher puts in other
        vt = float((m_t["other"] * truth["vocals"] * band_ok).sum() / ((truth["vocals"] * band_ok).sum() + 1e-9))
        rows.append({"song": t, "th": -1, "vocal_in_teacher_other": vt})
        print(f"  [{i + 1}/{len(tracks)}] {t[:8]} vocal energy in teacher 'other': {vt:.3f}  ({time.time() - t0:.0f}s)", flush=True)
    os.makedirs(a.out, exist_ok=True)
    json.dump(rows, open(os.path.join(a.out, "validate.json"), "w"))
    print(f"\nhtdemucs_ft vs real stems, {len(tracks)} {a.split} songs, {a.seconds:.0f} s loudest-vocal excerpts, cells >= 220 Hz")
    print(f"{'other>th':>9s} {'coverage':>9s} {'false rear perm':>16s} {'of which vocal':>15s} {'other recall':>13s}   (medians; false perm = protected energy inside teacher-confident rear cells)")
    for th in thresholds:
        rs = [r for r in rows if r["th"] == th]
        print(f"{th:9.2f} {np.median([r['coverage'] for r in rs]):9.3f} {np.median([r['false_perm'] for r in rs]):16.3f} "
              f"{np.median([r['vocal_perm'] for r in rs]):15.3f} {np.median([r['other_recall'] for r in rs]):13.3f}")
    print(f"\nprotection direction: cells the teacher calls vocal with share > th")
    print(f"{'vocal>th':>9s} {'precision':>10s} {'recall':>8s}   (precision = truly vocal energy inside them; recall = share of all vocal energy they cover)")
    for th in thresholds:
        rs = [r for r in rows if r["th"] == th]
        print(f"{th:9.2f} {np.median([r['vocal_precision'] for r in rs]):10.3f} {np.median([r['vocal_recall'] for r in rs]):8.3f}")
    vt = [r["vocal_in_teacher_other"] for r in rows if r["th"] == -1]
    print(f"\nvocal energy the teacher leaves in 'other' (median / p90): {np.median(vt):.3f} / {np.percentile(vt, 90):.3f}")


def separate(a):
    sep = separator(a.model)
    os.makedirs(a.out, exist_ok=True)
    files = [os.path.join(r, f) for r, _, fs in os.walk(a.music) for f in fs
             if f.lower().endswith((".mp3", ".m4a", ".flac", ".wav"))]
    rng = np.random.default_rng(a.seed)
    picked = [files[i] for i in rng.permutation(len(files))[:a.songs or len(files)]]
    names = []
    t0 = time.time()
    for i, path in enumerate(picked):
        tag = f"t{i:05d}"
        out = os.path.join(a.out, tag + ".npy")
        if os.path.exists(out):
            continue
        tmp = os.path.join(a.out, tag + ".src.wav")
        subprocess.run([a.ffmpeg, "-v", "error", "-y", "-i", path, "-vn", "-ac", "2", "-ar", str(MN.SR),
                        "-sample_fmt", "s16", tmp], check=True)
        import wave
        with wave.open(tmp, "rb") as w:
            x = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, 2).T.astype(np.float32) / 32768
        os.remove(tmp)
        x = x / (np.abs(x).max() + 1e-9) * 0.7
        est = separate_array(sep, x)
        if a.stems == 4:                                                      # [4, 2, n]: vocals, drums, bass, other (other = master - the rest)
            stack = np.stack([est["vocals"], est["drums"], est["bass"], x - est["vocals"] - est["drums"] - est["bass"]])
            layout = "vocals_drums_bass_other"
        else:
            stack = np.stack([est["vocals"], x - est["vocals"]])             # [2, 2, n]: vocals, the rest of the master
            layout = "vocals_rest"
        np.save(out, (np.clip(stack, -1, 1) * 32767).astype(np.int16))
        json.dump({"teacher": a.model, "source": os.path.basename(path), "layout": layout},
                  open(os.path.join(a.out, tag + ".json"), "w"))
        names.append((tag, path))
        print(f"  [{i + 1}/{len(picked)}] {tag} {os.path.basename(path)[:50]} ({time.time() - t0:.0f}s)", flush=True)
    with open(os.path.join(a.out, "names.txt"), "a", encoding="utf-8") as fh:
        for tag, path in names:
            fh.write(f"{tag}\t{path}\n")


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    v = sub.add_parser("validate"); v.add_argument("--cache", required=True); v.add_argument("--out", required=True)
    v.add_argument("--split", default="dev"); v.add_argument("--songs", type=int, default=40); v.add_argument("--seconds", type=float, default=60.0)
    v.add_argument("--model", default="htdemucs_ft")
    s = sub.add_parser("separate"); s.add_argument("--music", required=True); s.add_argument("--out", required=True)
    s.add_argument("--ffmpeg", default="ffmpeg"); s.add_argument("--songs", type=int, default=0, help="0 = the whole folder"); s.add_argument("--seed", type=int, default=3)
    s.add_argument("--model", default="htdemucs_ft"); s.add_argument("--stems", type=int, default=2, choices=(2, 4))
    a = ap.parse_args()
    {"validate": validate, "separate": separate}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
