"""Teacher labelling for the second-scene protection mask (offline, Mac MPS).

For each stereo file (mastered music, any format) or q3-cache song id:
  htdemucs_6s  -> vocals, drums, bass, guitar, piano, other
  BS-RoFormer  -> vocals (higher quality; overrides demucs vocals)
Writes a q3-compatible compact cache song: 48 kHz float16 [N,1+P,2] with mixture =
the mastered file itself and stored heads lead_vocal(=all vocals), bass, drums
(=whole kit), guitar, keys(=piano), other_unknown(=other). Heads the teachers cannot
separate (backing_vocal, other_vocal, high_percussion, percussion, strings_winds,
other_plucked) are marked present+bleed = UNKNOWN (excluded from any loss), so
`q3_cache.load_song` + Q3/Q4 trainers work unchanged. Teacher residual (mix - sum)
lands in the trainer's residual slot. Research cache; teacher weights are
development-only (licence check before any distribution).

  ~/.venvs/upmix-teachers/bin/python q4_teachers.py --out R/q4-teacher-cache-a --wav ~/Music/*.mp3
  ~/.venvs/upmix-teachers/bin/python q4_teachers.py --out R/q4-teacher-cache-a --cache R/q3-cache-a 260c431b
"""
import argparse, hashlib, json, logging, os, shutil, subprocess, sys, tempfile, time
from math import gcd
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

sys.path.insert(0, str(Path(__file__).resolve().parent))
from q2_source_data import CLASSES  # noqa: E402

SR = 48000
MODEL_DIR = Path.home() / '.cache' / 'upmix-teachers'
DEMUCS = 'htdemucs_6s.yaml'
ROFORMER = 'model_bs_roformer_ep_317_sdr_12.9755.ckpt'
KARAOKE = 'mel_band_roformer_karaoke_aufr33_viperx_sdr_10.1956.ckpt'   # lead vs backing, run on the vocals stem
HEAD_OF = dict(Vocals='lead_vocal', Bass='bass', Drums='drums', Guitar='guitar', Piano='keys', Other='other_unknown')
UNKNOWN = ('other_vocal', 'high_percussion', 'percussion', 'strings_winds', 'other_plucked')
SCHEMA = 'q4-teacher-cache-f16-v1'


def read_any(path, sr_out):
    try:
        a, sr = sf.read(path, dtype='float64', always_2d=True)
    except Exception:
        raw = subprocess.run(['ffmpeg', '-v', 'quiet', '-i', str(path), '-f', 'f64le', '-ac', '2', '-ar', str(sr_out), '-'],
                             capture_output=True, check=True).stdout
        return np.frombuffer(raw, 'f8').reshape(-1, 2).copy()
    if a.shape[1] == 1:
        a = np.repeat(a, 2, 1)
    if sr != sr_out:
        g = gcd(sr_out, sr)
        a = resample_poly(a, sr_out // g, sr // g, axis=0, padtype='constant')
    return a


def separators(work):
    from audio_separator.separator import Separator
    dem = Separator(model_file_dir=str(MODEL_DIR), output_dir=str(work), output_format='WAV', log_level=logging.WARNING,
                    demucs_params={'shifts': 0, 'overlap': 0.25})
    dem.load_model(model_filename=DEMUCS)
    rof = Separator(model_file_dir=str(MODEL_DIR), output_dir=str(work), output_format='WAV', log_level=logging.WARNING,
                    mdxc_params={'overlap': 2, 'batch_size': 4})
    rof.load_model(model_filename=ROFORMER)
    kar = Separator(model_file_dir=str(MODEL_DIR), output_dir=str(work), output_format='WAV', log_level=logging.WARNING,
                    mdxc_params={'overlap': 2, 'batch_size': 4})
    kar.load_model(model_filename=KARAOKE)
    return dem, rof, kar


def label_song(dem, rof, kar, mix48, name, out, meta_extra, work):
    """mix48 float32/64 [N,2] at 48k -> writes out/<name>.npy + .json; returns stats."""
    n = len(mix48)
    for f in work.glob('*.wav'):
        f.unlink()
    src = work / 'in.wav'
    sf.write(src, np.clip(mix48, -1, 1), SR, subtype='PCM_24')   # teachers resample internally
    stems = {}
    for sep in (dem, rof):
        for o in sep.separate(str(src)):
            tag = o[o.find('(') + 1:o.find(')')] if '(' in o else ''
            head = HEAD_OF.get(tag)
            if head is None:
                continue
            path = Path(o) if os.path.isabs(o) else work / o
            a, sr = sf.read(path, dtype='float64', always_2d=True)
            if sr != SR:
                g = gcd(SR, sr); a = resample_poly(a, SR // g, sr // g, axis=0, padtype='constant')
            a = a[:n]
            if len(a) < n:
                a = np.concatenate((a, np.zeros((n - len(a), 2))), 0)
            stems[head] = a                       # roformer 'Vocals' (second pass) overrides demucs vocals
    # karaoke on the vocals stem: 'Vocals' output = lead, remainder = backing
    voc = stems['lead_vocal']
    sf.write(work / 'voc.wav', np.clip(voc, -1, 1), SR, subtype='PCM_24')
    for o in kar.separate(str(work / 'voc.wav')):
        if '(Vocals)' in o:
            path = Path(o) if os.path.isabs(o) else work / o
            a, sr = sf.read(path, dtype='float64', always_2d=True)
            if sr != SR:
                g = gcd(SR, sr); a = resample_poly(a, SR // g, sr // g, axis=0, padtype='constant')
            a = a[:n]
            if len(a) < n:
                a = np.concatenate((a, np.zeros((n - len(a), 2))), 0)
            stems['lead_vocal'] = a
            stems['backing_vocal'] = voc - a
    stored = [k for k, c in enumerate(CLASSES) if c in stems]
    targets = np.stack([stems[CLASSES[k]] for k in stored], 1)          # [N,P,2]
    packed = np.concatenate((np.asarray(mix48, 'f8')[:, None], targets), 1).astype('f2')
    assert np.isfinite(packed).all()
    residual = np.asarray(mix48, 'f8') - targets.sum(1)
    present = [c in stems or c in UNKNOWN for c in CLASSES]
    bleed = [c in UNKNOWN for c in CLASSES]
    tmp = out / f'.{name}.tmp'
    with open(tmp, 'wb') as f:
        np.save(f, packed)
    os.replace(tmp, out / f'{name}.npy')
    info = dict(schema=SCHEMA, song_id=name, sample_rate=SR, frames=n, layout='[N,1+P,2]: 0=mixture (mastered), then stored_heads',
                stored_heads=stored, classes=list(CLASSES), heads_present=present, heads_bleed=bleed,
                heads_clean=[p and not b for p, b in zip(present, bleed)], detection_known=[c in stems for c in CLASSES],
                strict_admitted=False, exclusion_reasons=['teacher-labelled'], partition='TEACHER', unmapped_tracks=0,
                teachers=dict(demucs=DEMUCS, vocals=ROFORMER, karaoke=KARAOKE), residual_db_re_mix=float(10 * np.log10((residual ** 2).mean() / ((np.asarray(mix48) ** 2).mean() + 1e-30) + 1e-30)),
                peak_abs=float(np.abs(packed).max()), sha256=hashlib.sha256(packed.tobytes()).hexdigest(), bytes=int(packed.nbytes),
                test_pcm_accessed=False, car_approved=False, **meta_extra)
    (out / f'{name}.json').write_text(json.dumps(info, indent=1))
    return info


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--cache', type=Path, default=None)
    ap.add_argument('--wav', type=Path, nargs='*', default=[])
    ap.add_argument('--seconds', type=float, default=None)
    ap.add_argument('songs', nargs='*')
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='q4t-'))
    dem, rof, kar = separators(work)
    jobs = []
    if args.cache:
        from q3_cache import load_song
        for pre in args.songs:
            sid = next(p.stem for p in args.cache.glob('*.json') if p.stem.startswith(pre))
            jobs.append((sid, ('cache', sid)))
    for w in args.wav:
        jobs.append(('lib-' + hashlib.sha256(str(w.resolve()).encode()).hexdigest()[:12], ('wav', w)))
    for name, (kind, src) in jobs:
        if (args.out / f'{name}.json').exists():
            print(name, 'exists', flush=True); continue
        t0 = time.time()
        if kind == 'cache':
            mix = load_song(args.cache, src)[:, 0].astype('f8'); extra = dict(source='moisesdb-mix', moises_song_id=src)
        else:
            mix = read_any(src, SR); extra = dict(source='library', path=str(src), title=src.stem)
        if args.seconds:
            mix = mix[:int(args.seconds * SR)]
        info = label_song(dem, rof, kar, mix, name, args.out, extra, work)
        print(f"{name} {extra.get('title', src)[:40]!s:40s} {len(mix)/SR/60:.1f} min in {time.time()-t0:.0f}s, residual {info['residual_db_re_mix']:+.1f} dB", flush=True)
    shutil.rmtree(work, ignore_errors=True)
    print('done', args.out)


if __name__ == '__main__':
    main()
