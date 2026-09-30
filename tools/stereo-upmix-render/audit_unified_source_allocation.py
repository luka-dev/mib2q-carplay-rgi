#!/usr/bin/env python3
"""Frozen TRAIN12 frequency/source allocation diagnostic; no GPU or DEV.

Operator response to true components measures sensitivity, not literal audible
content in a cancelling source sum. Retain actual reconstruction error too.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES'] = '-1'
import argparse
import gc
import json
import math
from pathlib import Path
import numpy as np
import torch
import train_unified_upmix_generalization as B
T, S = B.T, B.S
BANDS = (('0-300', 0, 13), ('300-8k', 13, 342),
         ('8-14k', 342, 598), ('14-24k', 598, 1025))


def energy(x):
    return x.abs().double().square().sum()


def main(a):
    if a.result.exists():
        raise FileExistsError('Preserve prior allocation diagnostic')
    T.configure('cpu'); torch.set_num_threads(2)
    m = B.check(a); root = a.out/'joint_audio'
    terminal = json.loads((root/'train_complete.json').read_bytes())
    path = root/'step40000.pt'
    if not terminal['complete'] or terminal['step'] != 40000 or T.F.sha(path) != terminal['checkpoint_sha256']:
        raise ValueError('Fixed terminal joint40k only')
    net, _ = T.load(path, 'cpu'); net.eval(); a.result.mkdir(parents=True)
    protocol = dict(checkpoint_sha256=T.F.sha(path), script_sha256=T.F.sha(Path(__file__)),
                    manifest_sha256=T.F.sha(a.out/'manifest.json'),
                    scope='original TRAIN12, fixed terminal, actual summed PCM analyzed once; no model/data/DEV/GPU/renderer change',
                    frames='only past-only FFT windows entirely inside original scored20s cut; no zero-start or flush edges',
                    matrix='row inferred output, column true input source; squared response of frozen inferred mask to each true component / true component spectral power',
                    limits='Operator responses are sensitivity and need not sum as energies. Cancellation means not literal audible component content. Actual source reconstruction errors accompany every matrix. No promotion gate.')
    T.F.write_json(a.result/'protocol.json', protocol); rows = []
    with torch.inference_mode():
        for number, record in enumerate(m['evaluation']['train'], 1):
            roles = B.read_roles(a, record, 'cpu'); x = S.analyze(roles.sum(1)); refs = B.spectra(roles)
            pred, _ = net.forward_stft(x, with_scene=False)
            lo = math.ceil((record['warm_samples']+S.DELAY)/T.SN.HOP)
            hi = roles.shape[-1]//T.SN.HOP
            if hi <= lo:
                raise ValueError('No complete interior scored frames')
            x, refs, masks = x[:, lo:hi], refs[:, lo:hi], pred['source_masks'][:, lo:hi]
            mix_closure = float(energy(refs.sum(2)-x)/energy(x))
            if mix_closure > 1e-10:
                raise ValueError('True source spectrum closure failed')
            total = energy(refs); bands = {}
            for name, start, end in BANDS:
                r = refs[:, :, :, start:end]; mask = masks[:, :, :, start:end]
                xx = x[:, :, start:end]; power = [energy(r[:, :, i]) for i in range(4)]
                present = [p > total*1e-8 for p in power]
                response = {}; reconstruction = {}; fractions = {}
                for i, output in enumerate(T.U.SOURCES):
                    response[output] = {source: float(energy(mask[:, :, i]*r[:, :, j])/power[j])
                                        if present[j] else None for j, source in enumerate(T.U.SOURCES)}
                    reconstruction[output] = float(energy(mask[:, :, i]*xx-r[:, :, i])/power[i]) if present[i] else None
                    fractions[output] = float(power[i]/total)
                bands[name] = dict(operator_response=response, source_reconstruction_nse=reconstruction,
                                   target_power_fraction_all_sources_all_bands=fractions,
                                   summed_source_nse=float(energy(mask.sum(2)*xx-xx)/energy(xx).clamp_min(1e-12)))
            rows.append(dict(song=record['song'], scored_frames=hi-lo, true_spectral_closure_nse=mix_closure, bands=bands))
            T.F.write_json(a.result/'progress.json', dict(rows=rows))
            print('TRAIN_SOURCE_ALLOCATION', number, 12, flush=True)
            del roles, x, refs, pred, masks, r, mask, xx
            gc.collect()
    def med(values):
        present = [v for v in values if v is not None]
        return float(np.median(present)) if present else None
    summary = {name: dict(operator_response={output: {source: med([row['bands'][name]['operator_response'][output][source] for row in rows])
                                                     for source in T.U.SOURCES} for output in T.U.SOURCES},
                          source_reconstruction_nse={s: med([row['bands'][name]['source_reconstruction_nse'][s] for row in rows]) for s in T.U.SOURCES},
                          target_power_fraction_all_sources_all_bands={s: med([row['bands'][name]['target_power_fraction_all_sources_all_bands'][s] for row in rows]) for s in T.U.SOURCES},
                          summed_source_nse=med([row['bands'][name]['summed_source_nse'] for row in rows])) for name, _, _ in BANDS}
    if T.F.sha(path) != terminal['checkpoint_sha256']:
        raise ValueError('Terminal checkpoint changed')
    T.F.write_json(a.result/'complete.json', dict(complete=True, protocol_sha256=T.F.sha(a.result/'protocol.json'), rows=rows, summary=summary, limits=protocol['limits']))
    print('TRAIN_SOURCE_ALLOCATION_COMPLETE', json.dumps(summary), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('cache', 'initial', 'partition', 'monitor', 'out', 'result'):
        parser.add_argument('--'+name, type=Path, required=True)
    main(parser.parse_args())
