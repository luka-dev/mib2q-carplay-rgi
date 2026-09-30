#!/usr/bin/env python3
"""Frozen terminal TRAIN12 mixture-consistency diagnostic; no training or DEV.

Post-hoc projection is not evidence that a projection-trained model succeeds.
Targets are used for scoring only, never to construct projected operators.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES'] = '-1'
import argparse
import copy
import gc
import json
from pathlib import Path
import numpy as np
import torch
import train_unified_upmix_generalization as B
T, S = B.T, B.S
CASES = ('raw', 'uniform', 'eligible_residual')


def project(masks, case):
    if case == 'raw':
        return masks
    if case == 'uniform':
        return masks + (1 - masks.sum(2, keepdim=True)) / 4
    out = masks.clone()
    out[:, :, 0] = 1 - masks[:, :, 1:].sum(2)
    return out


def fold(net, case):
    result = copy.deepcopy(net)
    with torch.no_grad():
        for decoder, (lo, hi) in zip(result.dec, result.bands):
            linear = decoder[-1]
            w = linear.weight.reshape(hi-lo, 4, 2, 2, -1)
            bias = linear.bias.reshape(hi-lo, 4, 2, 2)
            if case == 'uniform':
                w.sub_(w.mean(1, keepdim=True))
                bias.sub_(bias.mean(1, keepdim=True))
                bias[:, :, :, 0].add_(.25)
            elif case == 'eligible_residual':
                w[:, 0] = -w[:, 1:].sum(1)
                bias[:, 0] = -bias[:, 1:].sum(1)
                bias[:, 0, :, 0].add_(1)
            else:
                raise ValueError(case)
    return result.eval()


def nse(y, ref):
    return float((y-ref).abs().double().square().sum() /
                 ref.abs().double().square().sum().clamp_min(1e-12))


@torch.inference_mode()
def score(masks, mixture, refs, roles, cut):
    count = roles.shape[-1]
    source = B.waves(masks * mixture[:, :, None], count)
    sent = B.waves(masks[:, :, 0, None] * refs, count)
    target, y = roles[..., cut:], source[..., cut:]
    power = target.double().square().sum((0, 2, 3))
    present = power > power.sum()*1e-6
    tx = {s: float(sent[:, i, :, cut:].double().square().sum()/power[i])
          if present[i] else None for i, s in enumerate(T.U.SOURCES)}
    sdr = {s: float(T.SN.si_sdr(torch, y[:, i], target[:, i]).mean())
           if present[i] else None for i, s in enumerate(T.U.SOURCES)}
    e = T.SN.stft(torch, target[:, 0])
    z = T.SN.stft(torch, sent[:, 0, :, cut:])
    bands = {}
    for name, lo, hi in [('0-300', 0, 13), ('300-8k', 13, 342),
                         ('8-14k', 342, 598), ('14-24k', 598, 1025)]:
        den = e[:, :, lo:hi].abs().double().square().sum()
        bands[name] = float(z[:, :, lo:hi].abs().double().square().sum()/den) if den > 1e-9 else None
    closure = nse(sent.sum(1), source[:, 0])
    if closure > 1e-9:
        raise ValueError('Eligible attribution closure failed')
    return dict(sdr_db=sdr, source_tx=tx, eligible_band_tx=bands,
                attributed_closure_nse=closure,
                summed_source_waveform_nse=nse(source.sum(1), roles.sum(1)))


def main(a):
    if a.result.exists():
        raise FileExistsError('Preserve previous diagnostic')
    T.configure('cpu'); torch.set_num_threads(2)
    manifest = B.check(a)
    folder = a.out/'joint_audio'
    terminal = json.loads((folder/'train_complete.json').read_bytes())
    checkpoint = folder/'step40000.pt'
    if not terminal['complete'] or terminal['step'] != 40000 or T.F.sha(checkpoint) != terminal['checkpoint_sha256']:
        raise ValueError('Fixed joint terminal40k required')
    net, _ = T.load(checkpoint, 'cpu'); net.eval()
    a.result.mkdir(parents=True)
    protocol = dict(scope='fixed terminal joint40k, frozen TRAIN12 only; no DEV/holdout/GPU/model update',
                    checkpoint_sha256=T.F.sha(checkpoint), manifest_sha256=T.F.sha(a.out/'manifest.json'),
                    script_sha256=T.F.sha(Path(__file__)), cases=list(CASES),
                    projection='uniform: m_i+(1-sum_j m_j)/4; eligible residual: 1-sum protected operators, others unchanged',
                    fold_gate_nse=1e-9, projected_sum_gate_nse=1e-10,
                    signal='paired median eligible SDR gain>=.5dB or eligibleTX gain>=.03; every protected attributedTX delta<=.01',
                    limits='TRAIN diagnostic signal only, not promotion. No oracle target in projection. No projection-training, renderer, gain, int8, device or ears result.')
    T.F.write_json(a.result/'protocol.json', protocol)
    rows = []; folded_checks = {}
    with torch.inference_mode():
        for number, record in enumerate(manifest['evaluation']['train'], 1):
            roles = B.read_roles(a, record, 'cpu')
            mixture, refs = S.analyze(roles.sum(1)), B.spectra(roles)
            prediction, _ = net.forward_stft(mixture, with_scene=False)
            masks = prediction['source_masks']; variants = {}
            if number == 1:
                prefix = mixture[:, :64]
                original, _ = net.forward_stft(prefix, with_scene=False)
                for case in CASES[1:]:
                    folded = fold(net, case)
                    replay, _ = folded.forward_stft(prefix, with_scene=False)
                    expected = project(original['source_masks'], case)
                    error = nse(replay['source_masks']*prefix[:, :, None], expected*prefix[:, :, None])
                    folded_checks[case] = dict(source_spectral_nse=error, passed=error <= 1e-9)
                    if error > 1e-9:
                        raise ValueError('Folded decoder parity failed')
                    del folded, replay, expected
            for case in CASES:
                selected = project(masks, case)
                variants[case] = score(selected, mixture, refs, roles, record['warm_samples'])
                if case != 'raw' and variants[case]['summed_source_waveform_nse'] > 1e-10:
                    raise ValueError('Projected waveform mixture closure failed')
                del selected
            rows.append(dict(song=record['song'], variants=variants))
            T.F.write_json(a.result/'progress.json', dict(rows=rows, folded_decoder_checks=folded_checks))
            print('TRAIN_CONSISTENCY', number, 12, json.dumps(variants), flush=True)
            del roles, mixture, refs, prediction, masks
            gc.collect()
    median = lambda values: float(np.median([v for v in values if v is not None]))
    summaries = {case: dict(sdr_db={s: median([r['variants'][case]['sdr_db'][s] for r in rows]) for s in T.U.SOURCES},
                           source_tx={s: median([r['variants'][case]['source_tx'][s] for r in rows]) for s in T.U.SOURCES},
                           eligible_band_tx={b: median([r['variants'][case]['eligible_band_tx'][b] for r in rows]) for b in rows[0]['variants'][case]['eligible_band_tx']}) for case in CASES}
    comparisons = {}
    for case in CASES[1:]:
        delta = median([r['variants'][case]['sdr_db']['eligible']-r['variants']['raw']['sdr_db']['eligible'] for r in rows])
        tx = {s: summaries[case]['source_tx'][s]-summaries['raw']['source_tx'][s] for s in T.U.SOURCES}
        comparisons[case] = dict(paired_median_eligible_sdr_delta_db=delta, source_tx_delta=tx,
                                diagnostic_signal=(delta >= .5 or tx['eligible'] >= .03) and all(tx[s] <= .01 for s in T.U.SOURCES[1:]))
    if T.F.sha(checkpoint) != terminal['checkpoint_sha256']:
        raise ValueError('Terminal weights changed')
    result = dict(complete=True, protocol_sha256=T.F.sha(a.result/'protocol.json'), rows=rows,
                  summaries=summaries, comparisons=comparisons, folded_decoder_checks=folded_checks,
                  limits=protocol['limits'])
    T.F.write_json(a.result/'complete.json', result)
    print('TRAIN_CONSISTENCY_COMPLETE', json.dumps(dict(summaries=summaries, comparisons=comparisons)), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('cache', 'initial', 'partition', 'monitor', 'out', 'result'):
        parser.add_argument('--'+name, type=Path, required=True)
    main(parser.parse_args())
