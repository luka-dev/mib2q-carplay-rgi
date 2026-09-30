#!/usr/bin/env python3
"""Verify the owner's requested scheduler reorder, without neural training.

The original checkpoint is immutable evidence; both terminal horizons stay40k.
Run on CPU after the verified previous worker tree has exited and before restart.
"""
import argparse
import hashlib
import json
from pathlib import Path

import torch
import separator_causal_reference as CR
import train_causal_reference_generalization as G


def main(a):
    G.configure('cpu')
    manifest=json.loads((a.out/'manifest.json').read_text())
    if manifest['sources']!=G.source_hashes():
        raise ValueError('Frozen model/training sources changed')
    archive=a.out/'priority_pause/original_last.pt'
    active=a.out/'original/last.pt'
    if G.sha(archive)!=G.sha(active):
        raise ValueError('Saved pause archive differs from resumable checkpoint')
    net,checkpoint=CR.load(torch,archive,'cpu')
    step=checkpoint['step']
    if (checkpoint['mode']!='original' or not 20000<=step<40000 or step%250 or
            checkpoint['protocol_sha256']!=G.sha(a.out/'manifest.json') or
            checkpoint['stats']!=manifest['stats']):
        raise ValueError('Invalid frozen control checkpoint semantics')
    if not all(bool(torch.isfinite(t).all()) for t in net.modules.state_dict().values()):
        raise ValueError('Nonfinite paused weights')
    if len(checkpoint['descriptor_hashes'])!=step or sum(checkpoint['song_counts'].values())!=step*G.BATCH:
        raise ValueError('Pause exposure or sampler history differs')
    sequence=hashlib.sha256()
    for item in checkpoint['descriptor_hashes']:sequence.update(bytes.fromhex(item))
    last=checkpoint['history'][-1]
    if last['step']!=step or sequence.hexdigest()!=last['sample_sequence_sha256']:
        raise ValueError('Saved sampling sequence differs')
    optimizer=checkpoint['optimizer']
    if not optimizer['state'] or not optimizer['param_groups']:
        raise ValueError('Fresh restart would lose the trained optimizer')
    if any(abs(group['lr']-G.lr_at(step))>1e-12 for group in optimizer['param_groups']):
        raise ValueError('Saved fixed40k scheduler position differs')
    for state in optimizer['state'].values():
        for value in state.values():
            if torch.is_tensor(value) and not bool(torch.isfinite(value).all()):
                raise ValueError('Nonfinite saved optimizer')
    result=dict(verified=True,scope='owner-requested execution order change; no acoustic early-stop decision',
        protocol_sha256=G.sha(a.out/'manifest.json'),paused_checkpoint_sha256=G.sha(archive),
        paused_original_step=step,saved_optimizer=True,saved_sampling_sequence_sha256=sequence.hexdigest(),
        saved_train_windows=step*G.BATCH,decoder_sha256=G.P.tensors_sha(net.dec),
        next_arm='residual96x4',next_arm_steps=40000,then_resume_original_to=40000,
        unchanged='models, data, stats, loss, step schedule, sampling, checkpoints and terminal gates',
        early_comparison='declared20k checkpoint in both arms; informational only, not a new selection gate',
        audit_script_sha256=G.sha(Path(__file__)))
    if (a.out/'priority_amendment.json').exists():
        raise FileExistsError('Preserve existing scheduling amendment')
    G.write(a.out/'priority_amendment.json',result)
    print(json.dumps(result),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True)
    main(p.parse_args())
