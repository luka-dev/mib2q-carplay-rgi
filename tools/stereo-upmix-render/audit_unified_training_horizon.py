#!/usr/bin/env python3
"""TRAIN-only fixed-checkpoint state/8s horizon mismatch diagnostic.

Eight-second resets are diagnostic, not a deployable processing strategy. Tests
use only the frozen TRAIN12 monitor. No model, DEV or live training changes.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES']='-1'
import argparse
import gc
import json
from pathlib import Path
import numpy as np
import torch
import train_unified_upmix_generalization as B
T=B.T
S=B.S


@torch.inference_mode()
def mask_chunks(net,x,reset):
    selected=[];state=None
    for chunk in x.split(750,1):
        if reset=='hidden' and state is not None:state={'level':state['level']}
        elif reset=='all':state=None
        pred,state=net.forward_stft(chunk,state,with_scene=False)
        selected.append(pred['source_masks'][:,:,0].clone())
        del pred
    return torch.cat(selected,1)


def metrics(estimate,sent,roles,cut):
    target=roles[...,cut:];y=estimate[...,cut:];attributed=sent[...,cut:]
    powers=target.double().square().sum((0,2,3));present=powers>powers.sum()*1e-6
    closure=float((sent.sum(1)-estimate).double().square().sum()/estimate.double().square().sum().clamp_min(1e-12))
    if closure>1e-9:raise ValueError('Attribution closure failed')
    tx={s:float(attributed[:,i].double().square().sum()/powers[i]) if present[i] else None for i,s in enumerate(T.U.SOURCES)}
    return dict(eligible_sdr_db=float(T.SN.si_sdr(torch,y,target[:,0]).mean()),source_tx=tx,closure_nse=closure)


@torch.inference_mode()
def row(net,roles,record):
    n=roles.shape[-1];x=S.analyze(roles.sum(1));refs=B.spectra(roles)
    variants={}
    # Continuous actual transform with changed neural state policy only.
    for tag,reset in [('continuous','none'),('hidden_reset_8s','hidden'),('all_neural_reset_8s','all')]:
        mask=mask_chunks(net,x,reset)
        estimate=S.synthesize(mask*x,n);sent=B.waves(mask[:,:,None]*refs,n)
        variants[tag]=metrics(estimate,sent,roles,record['warm_samples'])
        del mask,estimate,sent
    del x,refs
    # This last case also repeats zero-start analysis, as in TRAIN. Offline
    # compensation at chunk edges is diagnostic, and must never be deployed.
    estimates=[];attributions=[]
    for lo in range(0,n,8*T.SN.SR):
        part=roles[...,lo:lo+8*T.SN.SR];nn=part.shape[-1]
        x=S.analyze(part.sum(1));refs=B.spectra(part)
        pred,_=net.forward_stft(x,with_scene=False);mask=pred['source_masks'][:,:,0]
        estimates.append(S.synthesize(mask*x,nn));attributions.append(B.waves(mask[:,:,None]*refs,nn))
        del pred,mask,x,refs
    variants['independent_zero_start_8s']=metrics(torch.cat(estimates,-1),torch.cat(attributions,-1),roles,record['warm_samples'])
    return variants


def main(a):
    if a.result.exists():raise FileExistsError('Preserve prior horizon diagnostic')
    T.configure('cpu');torch.set_num_threads(2);manifest=B.check(a)
    folder=a.out/a.mode;terminal=json.loads((folder/'train_complete.json').read_bytes());checkpoint=folder/'step40000.pt'
    if not terminal['complete'] or terminal['step']!=40000 or T.F.sha(checkpoint)!=terminal['checkpoint_sha256']:raise ValueError('Fixed terminal40k required')
    net,ck=T.load(checkpoint,'cpu');net.eval();a.result.mkdir(parents=True)
    protocol=dict(mode=a.mode,checkpoint_sha256=T.F.sha(checkpoint),manifest_sha256=T.F.sha(a.out/'manifest.json'),script_sha256=T.F.sha(Path(__file__)),
        scope='frozen TRAIN12 only, exact20s scored cuts with original up-to10s warmup, fixed40k weights; no DEV or fitting/selection',
        cases='same continuous past-only spectra: full carry, time/global reset8s with level retained, all-neural reset8s; final case also repeats zero-start transform independently8s',
        interpretation='>=.5dB paired median SDR increase or>=.03 eligibleTX gain with<=.01 vocalTX increase is a diagnostic signal, not an acoustic promotion; resets can introduce audible discontinuities')
    T.F.write_json(a.result/'protocol.json',protocol);rows=[]
    for i,record in enumerate(manifest['evaluation']['train'],1):
        roles=B.read_roles(a,record,'cpu');variants=row(net,roles,record);rows.append(dict(song=record['song'],variants=variants))
        T.F.write_json(a.result/'progress.json',dict(rows=rows,protocol_sha256=T.F.sha(a.result/'protocol.json')))
        print('TRAIN_HORIZON',i,12,json.dumps(variants),flush=True)
        del roles;gc.collect()
    median=lambda xs:float(np.median([v for v in xs if v is not None]))
    summaries={tag:dict(eligible_sdr_db=median([r['variants'][tag]['eligible_sdr_db'] for r in rows]),
        source_tx={s:median([r['variants'][tag]['source_tx'][s] for r in rows]) for s in T.U.SOURCES}) for tag in rows[0]['variants']}
    comparisons={}
    for tag in list(summaries)[1:]:
        deltas=[r['variants'][tag]['eligible_sdr_db']-r['variants']['continuous']['eligible_sdr_db'] for r in rows]
        tx_gain=summaries[tag]['source_tx']['eligible']-summaries['continuous']['source_tx']['eligible']
        vocal_delta=summaries[tag]['source_tx']['vocal']-summaries['continuous']['source_tx']['vocal']
        comparisons[tag]=dict(paired_median_sdr_delta_db=median(deltas),eligible_tx_delta=tx_gain,vocal_tx_delta=vocal_delta,
                             diagnostic_improvement_signal=(median(deltas)>=.5 or tx_gain>=.03) and vocal_delta<=.01)
    if T.F.sha(checkpoint)!=terminal['checkpoint_sha256']:raise ValueError('Terminal checkpoint changed')
    T.F.write_json(a.result/'complete.json',dict(complete=True,rows=rows,summaries=summaries,comparisons=comparisons,
        protocol_sha256=T.F.sha(a.result/'protocol.json'),limits='TRAIN state-distribution diagnostic only. Not proof of a universal capacity limit or a deployable reset policy. No unseen20, rear/C/device/ears.'))
    print('TRAIN_HORIZON_COMPLETE',json.dumps(dict(summary=summaries,comparisons=comparisons)),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cache','initial','partition','monitor','out','result'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--mode',choices=B.MODES,required=True);main(p.parse_args())
