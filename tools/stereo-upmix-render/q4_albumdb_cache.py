"""AlbumDB -> q3-compatible compact cache: mastered stereo + true named stems.

mixture = the author's stereo MASTER (mastered, not a stem sum); targets = stems_mixed
grouped by name prefix into the 12 Q2/Q3 heads. The master is not a sample-exact sum of
the stems, so the difference lands in the trainer's residual slot. Partition DEV
(mastered-domain check for the protection mask); one artist, so never mix with TRAIN
of the same songs. Research cache only.

  python q4_albumdb_cache.py --root /Volumes/private/.../external --out R/q4-albumdb-cache-a
"""
import argparse, hashlib, json, os, re, time
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

from q2_source_data import CLASSES

SR = 48000
RULES = [  # (regex on stem file name, head)
    (r'^vox_lead', 'lead_vocal'), (r'^vox_backing|^vox_bv|^vox_harm', 'backing_vocal'), (r'^vox', 'other_vocal'),
    (r'^bass', 'bass'), (r'^drums_overhead|^drums_oh|^drums_hat|^drums_hh|^drums_ride|^drums_crash|^drums_cymb', 'high_percussion'),
    (r'^drums', 'drums'), (r'^perc|^shaker|^tamb|^clap', 'percussion'), (r'^guitar|^gtr|^acoustic', 'guitar'),
    (r'^keys|^piano|^synth|^organ|^rhodes', 'keys'), (r'^string|^violin|^cello|^brass|^horn|^sax|^flute', 'strings_winds'),
    (r'^banjo|^mandolin|^ukulele', 'other_plucked'),
]


def head_of(name):
    n = name.lower()
    for pat, head in RULES:
        if re.search(pat, n):
            return head
    return 'other_unknown'


def to48(a, sr):
    if sr == SR:
        return a
    from math import gcd
    g = gcd(SR, sr)
    return resample_poly(a, SR // g, sr // g, axis=0, padtype='constant')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=Path, required=True, help='.../external')
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    masters = sorted((args.root / 'albumdb-19683001-masters-a/masters_stereo/masters_stereo').glob('*.wav'))
    stems_root = args.root / 'albumdb-stems-19683001-b/files/stems_mixed'
    args.out.mkdir(parents=True, exist_ok=True)
    for m in masters:
        t0 = time.time()
        num = m.name[:2]
        song_dir = next(d for d in stems_root.iterdir() if d.name.startswith(num))
        name = f'albumdb-{num}'
        if (args.out / f'{name}.json').exists():
            print(name, 'exists'); continue
        mix, sr = sf.read(m, dtype='float64', always_2d=True)
        mix = to48(mix, sr)
        n = len(mix)
        targets = np.zeros((n, len(CLASSES), 2), 'f8')
        present = np.zeros(len(CLASSES), bool)
        used = {}
        for st in sorted(song_dir.glob('*.wav')):
            a, sr2 = sf.read(st, dtype='float64', always_2d=True)
            if a.shape[1] == 1:
                a = np.repeat(a, 2, 1)
            a = to48(a, sr2)[:n]
            k = CLASSES.index(head_of(st.stem))
            targets[:len(a), k] += a
            present[k] = True
            used[st.name] = CLASSES[k]
        residual = mix - targets.sum(1)
        stored = [k for k in range(len(CLASSES)) if present[k]]
        packed = np.concatenate((mix[:, None], targets[:, stored]), 1).astype('f2')
        assert np.isfinite(packed).all()
        tmp = args.out / f'.{name}.tmp'
        with open(tmp, 'wb') as f:
            np.save(f, packed)
        os.replace(tmp, args.out / f'{name}.npy')
        info = dict(schema='q4-albumdb-cache-f16-v1', song_id=name, artist='Tom McKenzie', artist_key='tom mckenzie', title=m.stem[5:],
                    genre='indie', partition='DEV', sample_rate=SR, frames=n, layout='[N,1+P,2]: 0=mastered stereo, then stored_heads',
                    stored_heads=stored, classes=list(CLASSES), heads_present=present.tolist(), heads_bleed=[False] * len(CLASSES),
                    heads_clean=present.tolist(), detection_known=[True] * len(CLASSES), strict_admitted=True, exclusion_reasons=[],
                    unmapped_tracks=0, stem_map=used, residual_db_re_mix=float(10 * np.log10((residual ** 2).mean() / ((mix ** 2).mean() + 1e-30) + 1e-30)),
                    peak_abs=float(np.abs(packed).max()), sha256=hashlib.sha256(packed.tobytes()).hexdigest(), bytes=int(packed.nbytes),
                    source='albumdb-19683001 masters_stereo + stems_mixed (CC licence, research only)', test_pcm_accessed=False, car_approved=False)
        (args.out / f'{name}.json').write_text(json.dumps(info, indent=1))
        print(f"{name} {m.stem[5:][:30]:30s} {n/SR/60:.1f} min, {len(used)} stems -> heads {[CLASSES[k] for k in stored]}, residual {info['residual_db_re_mix']:+.1f} dB, {time.time()-t0:.0f}s", flush=True)
    print('done', args.out)


if __name__ == '__main__':
    main()
