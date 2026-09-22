"""A/B: deployed HSSR-universal (real C renderer, V11 model) vs the azimuth prototype.

For each song: renders HSSR (6-ch S16 WAV via stereo_upmix_render --mode hssr-universal
--surround 0.25 --lfe off, i.e. the profile in the car) and writes a headphone preview
(FL/FR + 0.8*SL/SR) next to the azimuth outputs. With true stems (cache songs) it also
runs the ISOLATED-STEM PROBE both ways: each stem alone through HSSR and through the
azimuth mapping -> rear power re input power (dB). This is the probe HSSR was judged
by historically (vocals -22.9 dB mean); lower = safer. Listening only + probe numbers.

  python q5_hssr_ab.py --renderer build/stereo_upmix_render_current --model models/hssr-selection-v11/hssr_model.bin \
      --azimuth-out R/q5-azimuth-scene-a --out R/q5-hssr-ab-a --cache R/q3-cache-a --songs 747d5c98 260c431b d028d7c2 --wav ...
"""
import argparse, json, subprocess, tempfile, time
from pathlib import Path

import numpy as np
import soundfile as sf

from q2_source_data import CLASSES
from q3_cache import load_song
from q5_azimuth_scene import read_any, stft, azimuth_objects, SR

PROBE = ('lead_vocal', 'backing_vocal', 'bass', 'drums', 'high_percussion', 'guitar', 'keys')


def run_hssr(renderer, model, wav_in, wav_out, surround=0.25):
    cmd = [str(renderer), '--mode', 'hssr-universal', '--model', str(model), '--lfe', 'off', '--surround', str(surround), str(wav_in), str(wav_out)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stderr.strip()[-400:])
    six, sr = sf.read(wav_out, dtype='float32', always_2d=True)
    assert sr == SR and six.shape[1] == 6
    return six


def db(x):
    return float(10 * np.log10(max(x, 1e-30)))


def hssr_probe(renderer, model, stem, work):
    p_in, p_out = work / 'probe_in.wav', work / 'probe_out.wav'
    sf.write(p_in, np.clip(stem, -1, 1), SR, subtype='PCM_16')
    six = run_hssr(renderer, model, p_in, p_out)
    rear = six[:, 4:6].astype('f8'); front = six[:, 0:2].astype('f8')
    return dict(rear_re_in_db=db(float((rear ** 2).sum()) / float((stem.astype('f8') ** 2).sum() + 1e-30)),
                front_re_in_db=db(float((front ** 2).sum()) / float((stem.astype('f8') ** 2).sum() + 1e-30)))


def azimuth_probe(stem, p0, p1):
    X = stft(stem)
    rear_m, diff_m, _, _ = azimuth_objects(X, p0=p0, p1=p1)
    P = np.abs(X) ** 2
    return dict(rear_re_in_db=db(float((rear_m[:, None, :] ** 2 * P).sum()) / float(P.sum() + 1e-30)),
                diffuse_re_in_db=db(float((diff_m[:, None, :] ** 2 * P).sum()) / float(P.sum() + 1e-30)))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--renderer', type=Path, required=True)
    ap.add_argument('--model', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--cache', type=Path, nargs='*', default=[])
    ap.add_argument('--songs', nargs='*', default=[])
    ap.add_argument('--wav', type=Path, nargs='*', default=[])
    ap.add_argument('--p0', type=float, default=0.12)
    ap.add_argument('--p1', type=float, default=0.35)
    ap.add_argument('--seconds', type=float, default=None)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='q5ab-'))
    report = dict(schema='q5-hssr-ab-v1', hssr=dict(renderer=str(args.renderer), model=str(args.model), surround=0.25, lfe='off'),
                  azimuth=dict(p0=args.p0, p1=args.p1), songs={})
    jobs = []
    for cache in args.cache:
        metas = {p.stem: json.load(open(p)) for p in cache.glob('*.json') if p.name != 'manifest.json'}
        for pre in args.songs:
            for sid, m in metas.items():
                if sid.startswith(pre):
                    jobs.append((f"{sid[:8]}-{m.get('artist','')}-{m.get('title','')}".replace(' ', '_').replace('/', '_'), cache, sid))
    for w in args.wav:
        jobs.append((w.stem.replace(' ', '_')[:60], None, w))
    for name, cache, src in jobs:
        t0 = time.time(); d = args.out / name; d.mkdir(exist_ok=True)
        if cache is not None:
            a = load_song(cache, src); mix, stems = a[:, 0], a[:, 1:]
        else:
            mix, stems = read_any(src), None
        if args.seconds:
            mix = mix[:int(args.seconds * SR)]; stems = None if stems is None else stems[:len(mix)]
        sf.write(work / 'in.wav', np.clip(mix, -1, 1), SR, subtype='PCM_16')
        six = run_hssr(args.renderer, args.model, work / 'in.wav', d / 'hssr-6ch.wav')
        front, rear = six[:, 0:2], six[:, 4:6]
        sf.write(d / 'hssr-preview.wav', np.clip(front + 0.8 * rear, -1, 1), SR, subtype='PCM_24')
        sf.write(d / 'hssr-rear.wav', rear, SR, subtype='PCM_24')
        res = dict(hssr_rear_re_front_db=db(float((rear ** 2).mean()) / float((front ** 2).mean() + 1e-30)))
        if stems is not None:
            probe = {}
            for c in PROBE:
                s = stems[:, CLASSES.index(c)]
                if float((s ** 2).sum()) < 1e-4 * float((mix ** 2).sum()):
                    continue
                probe[c] = dict(hssr=hssr_probe(args.renderer, args.model, s, work), azimuth=azimuth_probe(s, args.p0, args.p1))
            res['isolated_stem_probe'] = probe
        report['songs'][name] = res
        line = f"{name[:34]:34s} HSSR rear/front {res['hssr_rear_re_front_db']:+.1f} dB"
        if stems is not None:
            line += ' | solo→rear (hssr/azimuth): ' + ' '.join(f"{c[:6]} {v['hssr']['rear_re_in_db']:+.0f}/{v['azimuth']['rear_re_in_db']:+.0f}" for c, v in res['isolated_stem_probe'].items())
        print(line, f'({time.time()-t0:.0f}s)', flush=True)
        (args.out / 'report.json').write_text(json.dumps(report, indent=1))
    print('done', args.out)


if __name__ == '__main__':
    main()
