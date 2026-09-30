#!/usr/bin/env python3
"""TRAIN-only shared decoder bottleneck task-gradient conflict diagnostic.

No updates. Exact broad objective/coefficient on first scored8s of TRAIN12.
Encoder and terminal affine are fixed; gradients target shared bottlenecks.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES'] = '-1'
import argparse
import gc
import json
from pathlib import Path
import numpy as np
import torch
import train_unified_upmix_generalization as B
T, S = B.T, B.S


def stats(pairs):
    e2 = sum(float(e.double().square().sum()) for e, a in pairs)
    a2 = sum(float(a.double().square().sum()) for e, a in pairs)
    dot = sum(float((e.double()*a.double()).sum()) for e, a in pairs)
    return dict(eligible_gradient_norm=e2**.5, weighted_protected_gradient_norm=a2**.5,
                cosine=dot/(e2*a2)**.5 if e2*a2 > 1e-24 else None,
                protected_to_eligible_norm_ratio=(a2/e2)**.5 if e2 > 1e-24 else None,
                combined_descent_opposes_eligible=(e2+dot < 0) if e2 > 1e-24 else None)


def main(a):
    if a.result.exists():
        raise FileExistsError('Preserve previous gradient audit')
    T.configure('cpu'); torch.set_num_threads(2); m = B.check(a)
    root = a.out/'joint_audio'; path = root/'step40000.pt'
    terminal = json.loads((root/'train_complete.json').read_bytes())
    if not terminal['complete'] or T.F.sha(path) != terminal['checkpoint_sha256']:
        raise ValueError('Fixed joint terminal required')
    net, _ = T.load(path, 'cpu'); net.eval()
    for p in net.parameters():
        p.requires_grad_(False)
    parameters = []
    for decoder in net.dec:
        for p in decoder[0].parameters():
            p.requires_grad_(True); parameters.append(p)
    a.result.mkdir(parents=True)
    protocol = dict(checkpoint_sha256=T.F.sha(path), script_sha256=T.F.sha(Path(__file__)),
                    manifest_sha256=T.F.sha(a.out/'manifest.json'),
                    scope='first8s of original scoredTRAIN12 cuts, zero-start as actual training, no updates/GPU/DEV',
                    objective=m['objective'], auxiliary_weight=m['auxiliary_weight'], normalizers=m['normalizers'],
                    parameters='only first shared32-to16 affine weight/bias in each band decoder',
                    band_groups='frequency centre of original decoder band; whole parameter gradient, not isolated bin loss',
                    interpretation='Negative cosine is task conflict. e_dot(e+weighted_aux)<0 predicts first-order eligible loss increase for this bottleneck-only combined step. Not actual multi-step dynamics, an encoder proof, or a split-branch acoustic result.')
    T.F.write_json(a.result/'protocol.json', protocol); rows = []
    for number, record in enumerate(m['evaluation']['train'], 1):
        print('TRAIN_DECODER_GRADIENT_BEGIN', number, flush=True)
        full = B.read_roles(a, record, 'cpu')
        roles = full[..., record['warm_samples']:record['warm_samples']+8*T.SN.SR].clone()
        del full
        refs = B.spectra(roles); pred, _ = net.forward_stft(S.analyze(roles.sum(1)), with_scene=False)
        print('TRAIN_DECODER_GRADIENT_FORWARD', number, flush=True)
        eligible = B.objective(pred['source_stft'], refs, roles, 0)
        auxiliary = m['auxiliary_weight']*sum(B.objective(pred['source_stft'], refs, roles, i, m['normalizers'][i]) for i in range(1,4))/3
        print('TRAIN_DECODER_GRADIENT_OBJECTIVES', number, flush=True)
        ge = torch.autograd.grad(eligible, parameters, retain_graph=True)
        print('TRAIN_DECODER_GRADIENT_ELIGIBLE', number, flush=True)
        ga = torch.autograd.grad(auxiliary, parameters)
        print('TRAIN_DECODER_GRADIENT_PROTECTED', number, flush=True)
        bands = []; groups = {key: [] for key in ('0-300', '300-8k', '8-14k', '14-24k', 'all')}
        for index, (lo, hi) in enumerate(net.bands):
            hz = ((lo+hi)/2)*T.SN.SR/T.SN.N_FFT
            key = '0-300' if hz < 300 else '300-8k' if hz < 8000 else '8-14k' if hz < 14000 else '14-24k'
            pairs = list(zip(ge[2*index:2*index+2], ga[2*index:2*index+2]))
            bands.append(dict(band=index, bins=[lo,hi], centre_hz=hz, group=key, **stats(pairs)))
            groups[key].extend(pairs); groups['all'].extend(pairs)
        row = dict(song=record['song'], eligible_loss=float(eligible.detach()), weighted_protected_loss=float(auxiliary.detach()),
                   bands=bands, groups={key:stats(pairs) for key,pairs in groups.items()})
        rows.append(row); T.F.write_json(a.result/'progress.json', dict(rows=rows))
        print('TRAIN_DECODER_GRADIENT', number, 12, json.dumps(row['groups']), flush=True)
        del roles, refs, pred, eligible, auxiliary, ge, ga, groups, pairs
        gc.collect()
    def med(values):
        values = [v for v in values if v is not None]
        return float(np.median(values)) if values else None
    summary = {key:dict(median_cosine=med([r['groups'][key]['cosine'] for r in rows]),
                        median_protected_to_eligible_norm_ratio=med([r['groups'][key]['protected_to_eligible_norm_ratio'] for r in rows]),
                        songs_combined_descent_opposes_eligible=sum(r['groups'][key]['combined_descent_opposes_eligible'] is True for r in rows),
                        eligible_reversal_band_cases=sum(b['combined_descent_opposes_eligible'] is True for r in rows for b in r['bands'] if key == 'all' or b['group'] == key),
                        band_cases=sum(1 for r in rows for b in r['bands'] if key == 'all' or b['group'] == key)) for key in rows[0]['groups']}
    if T.F.sha(path) != terminal['checkpoint_sha256']:
        raise ValueError('Frozen checkpoint changed')
    T.F.write_json(a.result/'complete.json', dict(complete=True, protocol_sha256=T.F.sha(a.result/'protocol.json'), rows=rows, summary=summary, limits=protocol['interpretation']))
    print('TRAIN_DECODER_GRADIENT_COMPLETE', json.dumps(summary), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('cache','initial','partition','monitor','out','result'):
        parser.add_argument('--'+name, type=Path, required=True)
    main(parser.parse_args())
