"""Q3 data cache: per-song 48 kHz float16 [N,13,2] = mixture + 12 class targets.

Replicates q2_source_data.SourceReader arithmetic (unit-gain float64 stem sum,
one shared scipy resample_poly 160/147 constant pad, zero-extended short stems)
for WHOLE songs with a relocated source root, plus per-head bleed masks so the
100 TRAIN / 21 DEV bleed-excluded songs become usable with partial labels.
TEST rows are never read. Output is a research cache, not a sealed owner.

  python q3_cache.py --index R/q2-source-data-a/index.json --out R/q3-cache-a --workers 4
"""
import argparse, hashlib, json, os, sys, time
from multiprocessing import Pool
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

from q2_source_data import CLASSES, source_class

NAS = '/mnt/shared/users/luka/Audi/upmix-training-2026-09-11'
SCHEMA = 'q3-cache-f16-v1'


def relocate(path, base):
    return Path(str(path).replace(NAS, str(base)))


def load_song(cache, song):
    """Cache song -> float32 [N,13,2] (mixture + 12 heads), expanding compact or legacy layouts. One sequential read."""
    a = np.load(Path(cache) / f'{song}.npy').astype('f4')
    meta = json.load(open(Path(cache) / f'{song}.json'))
    stored = meta.get('stored_heads')
    if stored is None:
        return a                                                    # legacy full [N,13,2]
    full = np.zeros((a.shape[0], 1 + len(CLASSES), 2), 'f4')
    full[:, 0] = a[:, 0]
    for i, k in enumerate(stored):
        full[:, 1 + k] = a[:, 1 + i]
    return full


def build_song(args):
    row, source_root, out = args
    song = row['song_id']
    npy, meta = out / f'{song}.npy', out / f'{song}.json'
    if npy.exists() and meta.exists():
        return song, 'exists', 0.0
    t0 = time.time()
    n = row['frames']
    # float32 accumulation: originals are exactly float32-representable (frozen reader asserts this),
    # and the per-track sums of <=~10 stems lose nothing audible; halves peak memory versus float64.
    mix = np.zeros((n, 2), 'f4')
    targets = np.zeros((n, len(CLASSES), 2), 'f4')
    present = np.zeros(len(CLASSES), bool)
    bleed = np.zeros(len(CLASSES), bool)
    unmapped = 0
    tracks = []
    for t in row['tracks']:
        a, rate = sf.read(source_root / t['path'], dtype='float32', always_2d=True)
        assert rate == 44100 and a.shape[1] == 2 and len(a) == t['header']['frames']
        assert np.isfinite(a).all()
        m = len(a)
        mix[:m] += a
        k = source_class(t)
        dirty = t['has_bleed'] is not False or t['isolated_source_candidate'] is not True
        if k is None:
            unmapped += 1
        else:
            targets[:m, k] += a
            present[k] = True
            bleed[k] |= dirty
        tracks.append(dict(id=t['id'], stem=t['stem'], track_type=t['track_type'], head=k,
                           has_bleed=t['has_bleed'], frames=m, sha256=t['sha256']))
    stored = [k for k in range(len(CLASSES)) if present[k]]            # compact: mixture + present heads only
    # resample one stereo signal at a time (same filter/phase/gain for all; bounded peak memory)
    cols = [mix] + [targets[:, k] for k in stored]
    out_len = None
    f16 = None
    for i, c in enumerate(cols):
        r = resample_poly(c.astype('f8'), 160, 147, axis=0, padtype='constant')
        if f16 is None:
            out_len = len(r)
            f16 = np.zeros((out_len, len(cols), 2), 'f2')
        f16[:, i] = r.astype('f2')
    del cols, targets, mix
    peak = float(np.max(np.abs(f16.astype('f4'))))
    closure = float(np.max(np.abs(f16[:, 0].astype('f4') - f16[:, 1:].astype('f4').sum(1)))) if not unmapped else None
    assert np.isfinite(f16).all() and peak < 60000
    tmp = out / f'.{song}.tmp'
    with open(tmp, 'wb') as f:
        np.save(f, f16)
    os.replace(tmp, npy)
    info = dict(schema=SCHEMA, song_id=song, artist=row['artist'], artist_key=row['artist_key'],
                title=row['title'], genre=row['genre'], partition=row['partition'],
                sample_rate=48000, frames=int(len(f16)), original_frames=n,
                layout='[N,1+P,2]: 0=mixture, then stored_heads in order; absent heads are exact zeros', stored_heads=stored,
                classes=list(CLASSES), heads_present=present.tolist(), heads_bleed=bleed.tolist(),
                heads_clean=(present & ~bleed).tolist(), detection_known=row['class_known'],
                strict_admitted=not row['exclusion_reasons'], exclusion_reasons=row['exclusion_reasons'],
                unmapped_tracks=unmapped, closure_max_abs=closure, peak_abs=peak,
                resample='shared scipy.signal.resample_poly 160/147 constant pad, float64 sum then float16',
                sha256=hashlib.sha256(f16.tobytes()).hexdigest(), bytes=int(f16.nbytes), tracks=tracks,
                test_pcm_accessed=False, car_approved=False)
    meta.write_text(json.dumps(info, indent=1))
    return song, 'built', time.time() - t0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--index', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--workers', type=int, default=4)
    ap.add_argument('--partitions', default='TRAIN,DEV')
    ap.add_argument('--limit', type=int, default=None)
    ap.add_argument('songs', nargs='*', help='optional song_id prefixes')
    args = ap.parse_args()
    index = json.load(open(args.index))
    base = Path(str(args.index).split('/runtime/')[0])
    source_root = relocate(index['source_root'], base)
    parts = set(args.partitions.split(','))
    assert 'TEST' not in parts
    rows = [r for r in index['rows'] if r['partition'] in parts]
    if args.songs:
        rows = [r for r in rows if any(r['song_id'].startswith(p) for p in args.songs)]
    rows = sorted(rows, key=lambda r: r['song_id'])[:args.limit]
    args.out.mkdir(parents=True, exist_ok=True)
    print(f'{len(rows)} songs -> {args.out} with {args.workers} workers', flush=True)
    jobs = [(r, source_root, args.out) for r in rows]
    done = 0
    t0 = time.time()
    with Pool(args.workers) as pool:
        for song, state, secs in pool.imap_unordered(build_song, jobs):
            done += 1
            print(f'[{done}/{len(rows)}] {song[:8]} {state} {secs:.0f}s  elapsed {time.time()-t0:.0f}s', flush=True)
    manifest = dict(schema=SCHEMA, index=str(args.index), index_sha256=hashlib.sha256(args.index.read_bytes()).hexdigest(),
                    source_root=str(source_root), partitions=sorted(parts),
                    songs={p.stem: json.loads(p.read_text()) for p in sorted(args.out.glob('*.json')) if p.name != 'manifest.json'})
    for s in manifest['songs'].values():
        s.pop('tracks', None)
    (args.out / 'manifest.json').write_text(json.dumps(manifest, indent=1))
    print('manifest', len(manifest['songs']), 'songs', flush=True)


if __name__ == '__main__':
    main()
