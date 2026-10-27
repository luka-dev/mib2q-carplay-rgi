#!/usr/bin/env python3
"""Dump the harness's per-stem 60 s excerpts of MoisesDB songs as one npz per song (float16),
so the same cells can be re-rendered and re-measured on the Mac.  Usage:
  python dump_stems.py MOISESDB OUT SECONDS song_id [song_id ...]"""
import os, sys, json, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import upmix_stem_tests as U
moises, out, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
os.makedirs(out, exist_ok=True)
for sid in sys.argv[4:]:
    meta, stems = U.load_song(os.path.join(moises, sid), secs)
    n = min(x.shape[1] for x in stems.values())
    stems = {k: v[:, :n] for k, v in stems.items()}
    mix = sum(stems.values()); k = 0.7 / (np.abs(mix).max() + 1e-9)
    np.savez(os.path.join(out, sid + ".npz"), k=np.float64(k), names=np.array(list(stems)),
             meta=json.dumps(meta), **{"stem_" + s: (v * k).astype(np.float16) for s, v in stems.items()})
    print(sid, meta.get("artist"), "-", meta.get("song"), list(stems), n, flush=True)
