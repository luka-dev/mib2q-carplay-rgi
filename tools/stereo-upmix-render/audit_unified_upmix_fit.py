#!/usr/bin/env python3
"""Post-run audit amendment: task losses plus all-output CPU/CUDA parity.

The original audit fails on dormant control-head loss roundoff. Training,
checkpoints and all acoustic gates are unchanged. This file never trains.
"""
import argparse
import json
from pathlib import Path
import numpy as np
import torch
import train_unified_upmix_fit as T


def verify(a, steps=T.STEPS, checker=T.check, filename='audited.json', scope='TRAIN-only; original10k pair; no broad/scene/DEV/device/car verdict'):
    T.configure('cuda');protocol,prepared=checker(a)
    roles,refs,x,_=T.fixtures(a,'cpu')
    out=a.out/filename
    if out.exists():raise FileExistsError('Preserve previous audit')
    result={}
    for mode in T.MODES:
        marker=json.loads((a.out/mode/'complete.json').read_text())
        path=a.out/mode/'last.pt'
        if not marker['complete'] or marker['step']!=steps or T.F.sha(path)!=marker['checkpoint_sha256']:
            raise ValueError('Final checkpoint differs')
        cpu_net,ck=T.load(path,'cpu');cpu_net.eval()
        if ck['mode']!=mode or ck['step']!=steps or ck['protocol_sha256']!=T.F.sha(a.out/'protocol.json'):
            raise ValueError('Final semantics differ')
        cpu=T.evaluate(cpu_net,x,refs,roles)
        cpu_net.eval()
        gpu_net,_=T.load(path,'cuda');gpu_net.eval()
        with torch.inference_mode():
            cpu_pred,_=cpu_net.forward_stft(x,with_scene=False)
            gpu_pred,_=gpu_net.forward_stft(x.to('cuda'),with_scene=False)
            nse={}
            for i,s in enumerate(T.U.SOURCES):
                p=cpu_pred['source_stft'][:,:,i]
                q=gpu_pred['source_stft'][:,:,i].cpu()
                spectral=float((p-q).abs().double().square().sum()/q.abs().double().square().sum().clamp_min(1e-12))
                w=T.SN.istft(torch,p,roles.shape[-1])
                z=T.SN.istft(torch,gpu_pred['source_stft'][:,:,i],roles.shape[-1]).cpu()
                waveform=float((w-z).double().square().sum()/z.double().square().sum().clamp_min(1e-12))
                if max(spectral,waveform)>1e-9:raise ValueError('All-output CPU/CUDA signal parity failed')
                nse[s]=dict(spectral_nse=spectral,waveform_nse=waveform)
        trained=T.U.SOURCES if mode=='joint_audio' else ('eligible',)
        errors={s:abs(cpu['loss'][s]-marker['final']['loss'][s]) for s in T.U.SOURCES}
        if max(errors[s] for s in trained)>1e-4:raise ValueError('Trained task CPU loss replay failed')
        sdr_error=float(np.max(np.abs(np.asarray(list(cpu['sdr_db'].values()))-np.asarray(list(marker['final']['sdr_db'].values())))))
        if sdr_error>.05:raise ValueError('All-output SDR replay failed')
        if T.P.tensors_sha(cpu_net.scene)!=marker['scene_frozen_sha256']:raise ValueError('Dormant scene differs')
        result[mode]=dict(checkpoint_sha256=T.F.sha(path),cpu=cpu,trained_tasks=list(trained),
                          loss_errors=errors,sdr_error=sdr_error,all_output_cpu_cuda_parity=nse,
                          temporal_audit=T.temporal_audit(cpu_net,x))
        del gpu_net,cpu_pred,gpu_pred;torch.cuda.empty_cache()
    old,new=[result[m]['cpu'] for m in T.MODES]
    delta=np.asarray(new['sdr_db']['eligible'])-np.asarray(old['sdr_db']['eligible'])
    gates=dict(eligible_loss_nonworse_5pct=new['loss']['eligible']<=1.05*old['loss']['eligible'],
               eligible_median_no_more_than_03db_regression=np.median(new['sdr_db']['eligible'])>=np.median(old['sdr_db']['eligible'])-.3,
               eligible_median_at_least_7db=np.median(new['sdr_db']['eligible'])>=7,
               no_eligible_crop_regression_over_1db=bool((delta>=-1).all()))
    for s in T.U.SOURCES[1:]:
        valid=np.asarray(new['source_present'][s])
        gates[s+'_median_sdr_nonnegative']=bool(valid.any() and np.median(np.asarray(new['sdr_db'][s])[valid])>=0)
    T.F.write_json(out,dict(complete=True,audit_verified=True,arms=result,gates={k:bool(v) for k,v in gates.items()},
         fitting_gate_pass=bool(all(gates.values())),protocol_sha256=T.F.sha(a.out/'protocol.json'),
         auditor_sha256=T.F.sha(Path(__file__)),amendment='Only trained-task losses use original1e-4 replay gate; dormant control outputs additionally require CPU/CUDA spectral and waveform NSE<=1e-9. Original failed runner record retained. Acoustic gates unchanged.',
         scope=scope))
    print('UNIFIED_FIT_AUDITED',json.dumps({k:bool(v) for k,v in gates.items()}),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['cache','initial','out']:p.add_argument('--'+name,type=Path,required=True)
    verify(p.parse_args())
