"""Render a Q3 mask-net checkpoint per head to WAV for LISTENING (same layout as render_q2_listen).

  python render_q3_listen.py --cache R/q3-cache-a --run R/q3-masknet-a --out R/q3-listen-render-a 747d5c98 260c431b d028d7c2
Output: <out>/epochNN/<song>/{original,model-NN-<head>,truth-NN-<head>,rear-safe,rear-safe-plus-keys,preview-stereo-crude}.wav
"""
import argparse, json, time
from pathlib import Path

import numpy as np
import soundfile as sf
import torch

from q2_source_data import CLASSES
from q3_cache import load_song
from q3_masknet import Q3MaskNet, render_song

REAR_SAFE = ('backing_vocal', 'high_percussion', 'percussion', 'strings_winds', 'other_unknown')
REAR_GAIN, PREVIEW_DELAY = 0.35, 720


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cache', type=Path, required=True)
    ap.add_argument('--run', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--device', default='mps' if torch.backends.mps.is_available() else 'cpu')
    ap.add_argument('songs', nargs='+')
    args = ap.parse_args()
    cp = torch.load(args.run / 'latest.pt', map_location='cpu', weights_only=False)
    cfg = cp['config']
    model = Q3MaskNet(cfg['bands'], cfg['width'], cfg['layers']).to(args.device)
    model.load_state_dict(cp['model']); model.eval()
    out = args.out / f"epoch{cp['epoch']:02d}"
    out.mkdir(parents=True, exist_ok=True)
    metas = {p.stem: json.load(open(p)) for p in args.cache.glob('*.json') if p.name != 'manifest.json'}
    report = dict(schema='q3-listen-render-v1', epoch=cp['epoch'], checkpoint_sha256=cp.get('sha256'), rear_safe=REAR_SAFE,
                  rear_gain=REAR_GAIN, songs={}, car_approved=False)
    for prefix in args.songs:
        song = next(s for s in metas if s.startswith(prefix))
        m = metas[song]
        name = f"{song[:8]}-{m['artist']}-{m['title']}".replace('/', '_').replace(' ', '_')
        d = out / name; d.mkdir(exist_ok=True)
        t0 = time.time()
        a = load_song(args.cache, song)
        src = render_song(model, a, torch.device(args.device))
        mix, tgt = a[:, 0], a[:, 1:]
        print(f'{name}: rendered in {time.time()-t0:.0f}s', flush=True)

        def write(fname, audio):
            sf.write(d / fname, np.clip(audio, -1, 1), 48000, subtype='PCM_24')

        write('original.wav', mix)
        pm = float((mix.astype('f8') ** 2).sum())
        heads = {}
        for k, c in enumerate(CLASSES):
            p, s = src[:, k].astype('f8'), tgt[:, k].astype('f8')
            ps, ss, es = float((p * p).sum()), float((s * s).sum()), float(((p - s) ** 2).sum())
            active = ss >= 1e-4 * pm
            heads[c] = dict(active=active, out_db_re_mix=10 * np.log10(max(ps, 1e-30) / pm),
                            nmse=(es / ss) if ss > 0 else None, out_minus_target_db=(10 * np.log10(max(ps, 1e-30) / ss) if ss > 0 else None))
            write(f'model-{k:02d}-{c}.wav', src[:, k])
            if active:
                write(f'truth-{k:02d}-{c}.wav', tgt[:, k])
            print(f"  {c:16s} " + (f"nmse={heads[c]['nmse']:.3f} out-tgt={heads[c]['out_minus_target_db']:+.1f}dB" if active else f"absent   out re mix={heads[c]['out_db_re_mix']:.1f}dB"), flush=True)
        rear = sum(src[:, CLASSES.index(c)] for c in REAR_SAFE)
        write('rear-safe.wav', rear)
        write('rear-safe-plus-keys.wav', rear + src[:, CLASSES.index('keys')])
        rear_d = np.concatenate((np.zeros((PREVIEW_DELAY, 2), 'f4'), rear[:-PREVIEW_DELAY]), 0)
        write('preview-stereo-crude.wav', mix + REAR_GAIN * rear_d)
        report['songs'][name] = dict(song_id=song, heads=heads)
        (out / 'report.json').write_text(json.dumps(report, indent=1))
    print('done', out, flush=True)


if __name__ == '__main__':
    main()
