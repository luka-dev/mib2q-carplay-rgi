#!/usr/bin/env python3
"""CPU acoustic/checkpoint audit of the completed causal encoder pair."""
import argparse
import json
from pathlib import Path
import numpy as np
import torch
import sep_net as SN
import separator_causal_reference as CR
import separator_causal_reference_probe as P


def audit(a):
    torch.set_num_threads(4)
    result = json.loads((a.out/'complete.json').read_text())
    manifest = json.loads((a.out/'manifest.json').read_text())
    if not result['complete'] or result['steps'] != 10000 or result['smoke']:
        raise ValueError('Full paired horizon required')
    arrays, old = P.data(a.cache,a.fit)
    if manifest['windows'] != old['windows'] or manifest['model_sha256'] != P.sha(Path(CR.__file__)) or manifest['probe_sha256'] != P.sha(Path(P.__file__)):
        raise ValueError('Audit data/source differs')
    anchor = torch.load(a.fit/'fit.pt',map_location='cpu',weights_only=True)
    w = torch.from_numpy(arrays)
    x, target = SN.stft(torch,w.sum(1)), SN.stft(torch,w[:,6])
    sources = dict(eligible=target,vocal=SN.stft(torch,w[:,0]+w[:,1]),
                   drums=SN.stft(torch,w[:,2:5].sum(1)),bass=SN.stft(torch,w[:,5]))
    verified = {}
    for mode in P.MODES:
        path = a.out/mode/'fit.pt'
        if P.sha(path) != result['arms'][mode]['checkpoint_sha256']:
            raise ValueError('Checkpoint hash differs')
        net, ck = CR.load(torch,path)
        if ck['step'] != 10000 or ck['mode'] != mode:
            raise ValueError('Checkpoint mode/horizon differs')
        ref = CR.make(torch,'original')
        ref.modules.load_state_dict(anchor['state'],strict=True)
        if P.tensors_sha(net.dec) != P.tensors_sha(ref.dec) or P.tensors_sha(net.dec) != manifest['frozen_decoder_sha256']:
            raise ValueError('Saved decoder differs from trained anchor')
        if not all(bool(torch.isfinite(v).all()) for v in net.modules.state_dict().values()):
            raise ValueError('Nonfinite saved parameters')
        measured = P.evaluate(torch,net,x,target,w[:,6],sources,w.shape[-1])
        saved = result['arms'][mode]['final']
        loss_error = abs(measured['loss']-saved['loss'])
        sdr_error = float(np.max(np.abs(np.asarray(measured['sdr_db'])-saved['sdr_db'])))
        source_error = max(float(np.max(np.abs(np.asarray(measured['source_tx'][s])-saved['source_tx'][s]))) for s in sources)
        if loss_error > 1e-4 or sdr_error > .05 or source_error > .005:
            raise ValueError('CPU acoustic replay outside declared numerical tolerance')
        architecture = result['arms'][mode]['architecture']
        if sum(p.numel() for p in net.parameters()) != architecture['parameters'] or net.macs*SN.FPS != architecture['estimated_neural_macs_s']:
            raise ValueError('Parameter/nominal MAC audit differs')
        verified[mode] = dict(acoustic=measured,causality=P.causality(torch,net,x),
                             loss_error=loss_error,max_sdr_error_db=sdr_error,max_source_tx_error=source_error,
                             decoder_sha256=P.tensors_sha(net.dec))
    gates = P.promotion(verified[P.MODES[1]]['acoustic'],verified[P.MODES[0]]['acoustic'],manifest['historical'])
    if gates != result['gates']:
        raise ValueError('CPU/GPU promotion decisions differ')
    P.write_json(a.out/'audit.json',dict(verified=True,scope='CPU saved-checkpoint TRAIN audit only',
                 arms=verified,gates=gates,diagnostic_pass=all(gates.values()),
                 result_sha256=P.sha(a.out/'complete.json'),audit_script_sha256=P.sha(Path(__file__))))
    print('CPU_AUDIT_PASSED diagnostic_pass='+str(all(gates.values())),flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['cache','fit','out']:
        p.add_argument('--'+name,type=Path,required=True)
    audit(p.parse_args())
