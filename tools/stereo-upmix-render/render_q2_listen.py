"""Render a trained Q2 checkpoint per identity head to WAV for LISTENING.

Not an evaluator, not a release artifact. Uses the frozen CPU streaming
reference (q2_pcm_stream.render_original_audio) unchanged; the song reader
replicates q2_source_data.SourceReader arithmetic (unit-gain stem sum,
shared resample_poly 160/147 constant pad) with a relocated source root, because
the frozen reader hard-binds the NAS-local /mnt path and a 10 s crop cap.

Usage (Mac, NAS over SMB):
  python render_q2_listen.py --owner /Volumes/private/.../runtime/q2-full-source-direct-predicted-a \
      --index /Volumes/private/.../runtime/q2-source-data-a/index.json \
      --out /Volumes/private/.../runtime/q2-listen-render-a 747d5c98 260c431b d028d7c2
"""
import argparse, hashlib, json, sys, time
from pathlib import Path

import numpy as np
import soundfile as sf
import torch
from scipy.signal import resample_poly

import train_q2_full_source as training
from q2_source_data import CLASSES, source_class
from q2_pcm_stream import render_original_audio

NAS = '/mnt/shared/users/luka/Audi/upmix-training-2026-09-11'
REAR_SAFE = ('backing_vocal', 'high_percussion', 'percussion', 'strings_winds', 'other_unknown')
REAR_GAIN = 0.35          # same scalar as the frozen common-output chain
PREVIEW_DELAY = 720       # 15 ms @48k crude headphone preview only


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def relocate(path, base):
    return Path(str(path).replace(NAS, str(base)))


def load_model(owner):
    plan = json.load(open(owner / 'plan.json'))
    report = json.load(open(owner / 'report.json'))
    pointer = json.load(open(owner / 'latest.json'))
    cp_path = owner / 'checkpoints' / pointer['path']
    cp = torch.load(cp_path, map_location='cpu', weights_only=False)
    model = training.create_model(training._config(plan['config']), plan['architecture'],
                                  plan['carrier_prior']).eval()
    model.load_state_dict(cp['model'])
    assert model._state_weight_sha() == report['final_sha256'], 'weights differ from sealed final'
    return model, dict(owner=str(owner), checkpoint=pointer['path'], checkpoint_sha256=sha(cp_path),
                       weight_sha256=report['final_sha256'], plan_sha256=sha(owner / 'plan.json'),
                       arm=plan['arm'], architecture=plan['architecture'])


def read_song(row, source_root, seconds=None):
    n = row['frames'] if seconds is None else min(row['frames'], int(seconds * 44100))
    mix = np.zeros((n, 2), 'f8')
    targets = np.zeros((n, len(CLASSES), 2), 'f8')
    for t in row['tracks']:
        a, rate = sf.read(source_root / t['path'], dtype='float64', always_2d=True, frames=n)
        assert rate == 44100 and a.shape[1] == 2
        m = len(a)
        mix[:m] += a
        k = source_class(t)
        if k is not None:
            targets[:m, k] += a
    packed = np.concatenate((mix[:, None], targets), 1)
    packed = resample_poly(packed, 160, 147, axis=0, padtype='constant').astype('f4')
    return packed[:, 0], packed[:, 1:]


def db(x):
    return float(10 * np.log10(max(x, 1e-30)))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--owner', type=Path, required=True)
    ap.add_argument('--index', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--seconds', type=float, default=None, help='excerpt length for a smoke run')
    ap.add_argument('--threads', type=int, default=4)
    ap.add_argument('songs', nargs='+', help='song_id prefixes')
    args = ap.parse_args()
    torch.set_num_threads(args.threads)

    index = json.load(open(args.index))
    base = Path(str(args.index).split('/runtime/')[0])
    source_root = relocate(index['source_root'], base)
    rows = {r['song_id']: r for r in index['rows']}
    model, binding = load_model(args.owner)
    print('model', binding, flush=True)
    args.out.mkdir(parents=True, exist_ok=True)
    report = dict(schema='q2-listen-render-v1', purpose='listening only; not an evaluator or release artifact',
                  model=binding, index_sha256=sha(args.index), rear_safe=REAR_SAFE, rear_gain=REAR_GAIN,
                  preview='dry + rear_gain*rear_safe delayed 15 ms, crude headphone preview, not the car',
                  excerpt_seconds=args.seconds, songs={}, car_approved=False)

    for prefix in args.songs:
        song_id = next(s for s in rows if s.startswith(prefix))
        row = rows[song_id]
        assert row['partition'] != 'TEST' and not row['exclusion_reasons'], 'DEV/TRAIN admitted only'
        name = f"{song_id[:8]}-{row['artist']}-{row['title']}".replace('/', '_').replace(' ', '_')
        d = args.out / name
        d.mkdir(exist_ok=True)
        t0 = time.time()
        mix, targets = read_song(row, source_root, args.seconds)
        print(f'{name}: read {len(mix)/48000:.1f}s in {time.time()-t0:.1f}s', flush=True)
        t0 = time.time()
        r = render_original_audio(model, torch.from_numpy(mix), 'predicted',
                                  checkpoint_sha256=binding['checkpoint_sha256'])
        src = r['sources'].numpy()            # [N,12,2], aligned to delayed original
        dry = r['delayed_original'].numpy()    # [N,2]
        print(f'{name}: rendered in {time.time()-t0:.1f}s', flush=True)
        # render_original_audio crops the 1024-sample startup: sources/dry are INPUT-aligned
        assert np.allclose(dry, mix, atol=1e-6), 'delayed_original is expected to be input-aligned after cropping'
        tgt = targets

        def write(fname, audio):
            sf.write(d / fname, np.clip(audio, -1, 1), 48000, subtype='PCM_24')

        write('original.wav', dry)
        heads = {}
        for k, c in enumerate(CLASSES):
            p, s = src[:, k].astype('f8'), tgt[:, k].astype('f8')
            ps, ss, es = float((p * p).sum()), float((s * s).sum()), float(((p - s) ** 2).sum())
            active = ss >= 1e-4 * float((dry.astype('f8') ** 2).sum())
            heads[c] = dict(active=active, out_db_re_mix=db(ps / max(float((dry.astype('f8') ** 2).sum()), 1e-30)),
                            nmse=(es / ss) if ss > 0 else None, out_minus_target_db=(db(ps / ss) if ss > 0 else None))
            write(f'model-{k:02d}-{c}.wav', src[:, k])
            if active:
                write(f'truth-{k:02d}-{c}.wav', tgt[:, k])
        rear = sum(src[:, CLASSES.index(c)] for c in REAR_SAFE)
        write('rear-safe.wav', rear)
        write('rear-safe-plus-keys.wav', rear + src[:, CLASSES.index('keys')])
        rear_d = np.concatenate((np.zeros((PREVIEW_DELAY, 2), 'f4'), rear[:-PREVIEW_DELAY]), 0)
        write('preview-stereo-crude.wav', dry + REAR_GAIN * rear_d)
        report['songs'][name] = dict(song_id=song_id, artist=row['artist'], title=row['title'], genre=row['genre'],
                                     samples=int(len(mix)), heads=heads)
        for c, h in heads.items():
            if h['active']:
                print(f"  {c:16s} nmse={h['nmse']:.3f} out-tgt={h['out_minus_target_db']:+.1f}dB", flush=True)
            else:
                print(f"  {c:16s} absent   out re mix={h['out_db_re_mix']:.1f}dB", flush=True)
        (args.out / 'report.json').write_text(json.dumps(report, indent=1))
    print('done', args.out, flush=True)


if __name__ == '__main__':
    main()
