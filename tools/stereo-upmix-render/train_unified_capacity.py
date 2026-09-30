#!/usr/bin/env python3
"""Fixed20k capacity continuation: same initial function, data and objectives.

Current joint40k is the parent, not an acoustically promoted winner. One extra
identity-initialized block and16->24 shared features are the only intervention.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import math
from pathlib import Path
import shutil
import time
from types import SimpleNamespace
import numpy as np
import torch
import train_unified_upmix_generalization as B
import expand_unified_upmix as X
T,G,H,S=B.T,B.G,B.H,B.S
spectra,waves,objective,read_roles,eval_row,evaluate=(B.spectra,B.waves,B.objective,B.read_roles,B.eval_row,B.evaluate)
STEPS=20000
BASE_OFFSET=40000
SNAPSHOTS=(0,1000,4000,10000,20000)
MODES=('control','expanded')


def hashes():
    evaluator=Path(__file__).with_name('eval_unified_capacity.py')
    return B.hashes() | {Path(X.__file__).name:T.F.sha(Path(X.__file__)),Path(__file__).name:T.F.sha(Path(__file__)),evaluator.name:T.F.sha(evaluator)}


def lr_at(step):
    if step<=2000:return 3e-4*step/2000
    return 3e-5+(3e-4-3e-5)*.5*(1+math.cos(math.pi*(step-2000)/(STEPS-2000)))


def parent_args(a):
    return SimpleNamespace(cache=a.cache,initial=a.initial,partition=a.partition,monitor=a.monitor,out=a.parent)


def prepare(a):
    if a.out.exists():raise FileExistsError('Preserve prior capacity pair')
    T.configure('cpu');m=B.check(parent_args(a))
    completed=json.loads((a.parent/'full40/complete.json').read_bytes())
    audit=json.loads((a.parent/'audit.json').read_bytes())
    if not completed['complete'] or not audit['verified'] or audit['smoke']:raise ValueError('Completed audited source/C pair required')
    if completed['arms']['joint_audio']['nominal_observed_gates_pass']:raise ValueError('Existing source candidate passes; prioritize its deployment gates')
    parent=a.parent/'joint_audio/step40000.pt'
    if T.F.sha(parent)!=X.PARENT_SHA or audit['arms']['joint_audio']['checkpoint_sha256']!=X.PARENT_SHA:raise ValueError('Frozen parent changed')
    prepared=json.loads((a.expansion/'complete.json').read_bytes())
    xp=json.loads((a.expansion/'protocol.json').read_bytes())
    expanded=a.expansion/'expanded_initial.pt'
    if not prepared['complete'] or not prepared['passed'] or prepared['protocol_sha256']!=T.F.sha(a.expansion/'protocol.json') or prepared['expanded_initial_sha256']!=T.F.sha(expanded) or xp['parent_sha256']!=X.PARENT_SHA or xp['script_sha256']!=T.F.sha(Path(X.__file__)) or xp['base_graph_source_sha256']!=T.F.sha(Path(T.U.__file__)):raise ValueError('Function-preserving preparation changed')
    initials={'control':parent,'expanded':expanded};costs={};scenes={};configs={}
    for mode,path in initials.items():
        net,_=T.load(path,'cpu');costs[mode]=X.selected_cost(net);scenes[mode]=T.P.tensors_sha(net.scene);configs[mode]=net.config
        if costs[mode]['eligible_plus_all_scene_dense_macs_s']>150e6:raise ValueError('Dense selected screen exceeds150M')
    m=m | dict(version='unified-capacity-continuation-v1',sources=hashes(),steps=STEPS,snapshots=list(SNAPSHOTS),
        parent_manifest_sha256=T.F.sha(a.parent/'manifest.json'),parent_audit_sha256=T.F.sha(a.parent/'audit.json'),
        parent_c_result_sha256=T.F.sha(a.parent/'full40/complete.json'),expansion_protocol_sha256=T.F.sha(a.expansion/'protocol.json'),
        initial_sha256={mode:T.F.sha(path) for mode,path in initials.items()},scene_frozen_sha256=scenes,cost=costs,architecture=configs,
        draw_step_offset=BASE_OFFSET,optimizer='fresh AdamW wd1e-4/clip5, float32 TF32 off;2k warmup3e-4/cosine3e-5 at20k both',
        objective='same physical spectral+multires joint4-source objective, fixed original coefficient/normalizers both; scene frozen',
        intervention='24channel2blocks/local-global-decoder16 control vs24channel3blocks/local-global-decoder24, function-preserving joint40k warm start; same2048/512 frontend',
        selection='fixed terminal20k continuation both, no4k acoustic stop/bestDEV/gain selection; all40 C/events at same predeclared operating points; reserved20 remains unread',
        relative_gates='eligible pairedDEV SDR>=control-.3dB, eligibleTX>=.9control, every protectedTX<=control+.01, C swings/entry nonworse; full absolute gates remain required',
        scope_limits='Capacity package, not proof of which added component helped. Parent joint chosen for meaningful protected training heads, not as a promoted winner. Selected dense estimate only; new C/int8/device port required. Scene dormant.')
    a.out.mkdir(parents=True);G.write(a.out/'manifest.json',m)
    for mode,path in initials.items():shutil.copyfile(path,a.out/(mode+'_initial.pt'))
    print('UNIFIED_CAPACITY_PREPARED156TRAIN/FIXED20k',flush=True)


def check(a):
    m=json.loads((a.out/'manifest.json').read_bytes());B.check(parent_args(a))
    if m['sources']!=hashes() or m['parent_manifest_sha256']!=T.F.sha(a.parent/'manifest.json') or m['parent_audit_sha256']!=T.F.sha(a.parent/'audit.json') or m['parent_c_result_sha256']!=T.F.sha(a.parent/'full40/complete.json') or m['expansion_protocol_sha256']!=T.F.sha(a.expansion/'protocol.json'):raise ValueError('Frozen capacity sources/parent changed')
    for mode in MODES:
        if m['initial_sha256'][mode]!=T.F.sha(a.out/(mode+'_initial.pt')):raise ValueError('Capacity initial checkpoint changed')
    return m


@torch.inference_mode()
def initial_audit(a):
    T.configure('cuda');m=check(a);roles=read_roles(a,m['replay'],'cuda');x=S.analyze(roles.sum(1));outputs={};waveforms={}
    for mode in MODES:
        net,_=T.load(a.out/(mode+'_initial.pt'),'cuda');net.eval()
        outputs[mode],_=net.forward_stft(x);waveforms[mode]=waves(outputs[mode]['source_stft'],roles.shape[-1])
        del net
    nse=lambda y,r:float((y-r).abs().double().square().sum()/r.abs().double().square().sum().clamp_min(1e-12))
    errors={k:nse(outputs['expanded'][k],outputs['control'][k]) for k in outputs['control']}
    wav={s:nse(waveforms['expanded'][:,i],waveforms['control'][:,i]) for i,s in enumerate(T.U.SOURCES)}
    if max(list(errors.values())+list(wav.values()))>1e-9:raise ValueError('Actual TRAIN CUDA initial-function parity failed')
    G.write(a.out/'initial_audit.json',dict(verified=True,all_field_nse=errors,source_waveform_nse=wav,
        crop_sha256=m['replay']['crop_sha256'],protocol_sha256=T.F.sha(a.out/'manifest.json'),
        scope='actual frozenTRAIN4s physical path and CUDA graph initialization equality; no DEV quality gain'))
    print('UNIFIED_CAPACITY_ACTUAL_TRAIN_INITIAL_PARITY_PASS',flush=True)


def snapshot(net,a,m,folder,step,smoke=False):
    subsets={'train':m['evaluation']['train'][:2]} if smoke else m['evaluation']
    result=dict(step=step,mode=a.mode,metrics={k:evaluate(net,a,v,'cuda') for k,v in subsets.items()},
                checkpoint_sha256=T.F.sha(folder/f'step{step}.pt'),protocol_sha256=T.F.sha(a.out/'manifest.json'),
                scene_sha256=T.P.tensors_sha(net.scene))
    G.write(folder/f'eval_step{step}.json',result)
    if step==(20 if smoke else STEPS):
        roles=read_roles(a,m['replay'],'cuda');row,wave=eval_row(net,roles,m['replay'])
        torch.save(dict(waveform=wave.cpu(),input_stft=S.analyze(roles.sum(1)).cpu(),row=row,checkpoint_sha256=result['checkpoint_sha256'],
                        crop_sha256=m['replay']['crop_sha256']),folder/'cuda_replay.pt')
    print('EVAL',a.mode,step,json.dumps({k:v['summary'] for k,v in result['metrics'].items()}),flush=True)


def train(a):
    T.configure('cuda');m=check(a);folder=a.out/(a.mode+('_smoke' if a.smoke else ''))
    folder.mkdir(exist_ok=a.resume)
    net,ck=T.load(folder/'last.pt' if a.resume else a.out/(a.mode+'_initial.pt'),'cuda')
    for p in net.scene.parameters():p.requires_grad_(False)
    opt=torch.optim.AdamW([p for p in net.parameters() if p.requires_grad],lr=lr_at(1),weight_decay=1e-4)
    step0=0;history=[];counts=Counter();exposure=np.zeros(4,np.int64);descriptors=[];digest=hashlib.sha256();prior=0.
    if a.resume:
        if ck['protocol_sha256']!=T.F.sha(a.out/'manifest.json') or ck['mode']!=a.mode or ck['smoke']!=a.smoke:
            raise ValueError('Resume semantics differ')
        opt.load_state_dict(ck['optimizer']);step0=ck['step'];history=ck['history'];counts.update(ck['song_counts'])
        exposure=np.array(ck['audible_source_windows']);descriptors=ck['descriptor_hashes'];prior=ck['seconds']
        for h in descriptors:digest.update(bytes.fromhex(h))
    def save(step,elapsed):
        if T.P.tensors_sha(net.scene)!=m['scene_frozen_sha256'][a.mode]:raise ValueError('Dormant scene changed')
        state=dict(architecture=T.U.ARCHITECTURE,config=net.config,state=net.state_dict(),optimizer=opt.state_dict(),
            mode=a.mode,smoke=a.smoke,step=step,history=history,seconds=elapsed,song_counts=dict(counts),
            audible_source_windows=exposure.tolist(),descriptor_hashes=descriptors,protocol_sha256=T.F.sha(a.out/'manifest.json'))
        G.save(folder/'last.pt',state)
        if step in SNAPSHOTS or step==cap:G.save(folder/f'step{step}.pt',state)
    cap=20 if a.smoke else STEPS
    if not step0 and not a.smoke:
        save(0,0.);snapshot(net,a,m,folder,0)
    reader=G.Reader(a.cache,m['train_inventory']);begin=time.monotonic()
    try:
        with ThreadPoolExecutor(max_workers=1) as producer:
            pending=producer.submit(reader.batch,BASE_OFFSET+step0+1)
            for step in range(step0+1,cap+1):
                arrays,draws,energies=pending.result()
                if step<cap:pending=producer.submit(reader.batch,BASE_OFFSET+step+1)
                h=hashlib.sha256(json.dumps(draws,sort_keys=True).encode()).hexdigest();descriptors.append(h);digest.update(bytes.fromhex(h))
                counts.update(r['song'] for r in draws);exposure+=(energies>10**(-55/10)).sum(0)
                roles=torch.from_numpy(arrays).to('cuda');refs=spectra(roles);net.train();opt.zero_grad(set_to_none=True)
                for group in opt.param_groups:group['lr']=lr_at(step)
                pred,_=net.forward_stft(S.analyze(roles.sum(1)),with_scene=False)
                loss=objective(pred['source_stft'],refs,roles,0)
                loss+=m['auxiliary_weight']*sum(objective(pred['source_stft'],refs,roles,i,m['normalizers'][i]) for i in range(1,4))/3
                if not bool(torch.isfinite(loss)):raise ValueError('Nonfinite training loss')
                loss.backward();grad=float(torch.nn.utils.clip_grad_norm_([p for p in net.parameters() if p.requires_grad],5.))
                if not np.isfinite(grad):raise ValueError('Nonfinite gradient')
                decoder_grad=sum(float(p.grad.square().sum()) for p in net.dec.parameters() if p.grad is not None)**.5
                opt.step()
                if step%250==0 or step==cap:
                    elapsed=prior+time.monotonic()-begin
                    row=dict(step=step,seconds=elapsed,loss=float(loss.detach()),grad_norm=grad,decoder_grad_norm=decoder_grad,
                             lr=lr_at(step),distinct_train_songs=len(counts),windows=step*G.BATCH,
                             audible_source_windows=exposure.tolist(),sample_sequence_sha256=digest.hexdigest())
                    history.append(row);G.write(folder/'progress.json',dict(rows=history,song_counts=dict(counts)))
                    save(step,elapsed)
                    print('UNIFIED_CAPACITY',a.mode,step,'loss',row['loss'],'songs',len(counts),'seconds',elapsed,flush=True)
                if step in SNAPSHOTS or step==cap:snapshot(net,a,m,folder,step,a.smoke)
                del roles,refs,pred,loss
    finally:reader.close()
    G.write(folder/'train_complete.json',dict(complete=True,mode=a.mode,smoke=a.smoke,step=cap,last=history[-1],
        checkpoint_sha256=T.F.sha(folder/f'step{cap}.pt'),protocol_sha256=T.F.sha(a.out/'manifest.json')))


def audit(a):
    T.configure('cpu');m=check(a);step=20 if a.smoke else STEPS;result={};digests=[]
    for mode in MODES:
        folder=a.out/(mode+('_smoke' if a.smoke else ''));path=folder/f'step{step}.pt'
        final=json.loads((folder/'train_complete.json').read_bytes());net,ck=T.load(path,'cpu')
        if not final['complete'] or final['step']!=step or final['mode']!=mode or final['smoke']!=a.smoke or final['checkpoint_sha256']!=T.F.sha(path) or ck['protocol_sha256']!=T.F.sha(a.out/'manifest.json'):
            raise ValueError('Final checkpoint/protocol differs')
        if set(ck['song_counts'])-set(m['train_inventory']) or sum(ck['song_counts'].values())!=step*G.BATCH:
            raise ValueError('Exposure/partition audit failed')
        if T.P.tensors_sha(net.scene)!=m['scene_frozen_sha256'][mode]:raise ValueError('Scene unexpectedly trained')
        saved=torch.load(folder/'cuda_replay.pt',map_location='cpu',weights_only=True)
        if saved['checkpoint_sha256']!=T.F.sha(path) or saved['crop_sha256']!=m['replay']['crop_sha256']:
            raise ValueError('GPU saved waveform/input provenance differs')
        roles=read_roles(a,m['replay'],'cpu');row,wave=eval_row(net,roles,m['replay']);errors={};native_errors={}
        gates=m['numeric_gates'];native_input=S.analyze(roles.sum(1));canonical=saved['input_stft']
        input_nse=float((native_input-canonical).abs().double().square().sum()/canonical.abs().double().square().sum().clamp_min(1e-12))
        if input_nse>gates['native_input_stft_nse']:raise ValueError('Native FFT input parity failed')
        with torch.inference_mode():
            canonical_pred,_=net.forward_stft(canonical,with_scene=False)
            canonical_wave=waves(canonical_pred['source_stft'],roles.shape[-1])
        for i,s in enumerate(T.U.SOURCES):
            y=saved['waveform'][:,i];den=y.double().square().sum().clamp_min(1e-12)
            errors[s]=float((y-canonical_wave[:,i]).double().square().sum()/den)
            native_errors[s]=dict(nse=float((y-wave[:,i]).double().square().sum()/den),
                max_pcm16_lsb=float((y-wave[:,i]).abs().max()*32768),loss_error=abs(row['loss'][s]-saved['row']['loss'][s]),
                sdr_error_db=abs(row['sdr_db'][s]-saved['row']['sdr_db'][s]) if row['sdr_db'][s] is not None and saved['row']['sdr_db'][s] is not None else 0.)
            e=native_errors[s]
            if e['nse']>gates['native_end_to_end_waveform_nse'] or e['max_pcm16_lsb']>gates['native_max_error_pcm16_lsb'] or e['loss_error']>gates['loss_error'] or e['sdr_error_db']>gates['sdr_error_db']:
                raise ValueError('Native end-to-end CPU/CUDA signal or acoustic parity failed')
        if max(errors.values())>gates['same_input_all_source_waveform_nse']:raise ValueError('Same-input all-output CPU/CUDA waveform parity failed')
        cause=T.temporal_audit(net,native_input)
        stream=S.Stream(net);n=roles.shape[-1];mix=roles.sum(1);parts=[];start=0
        with torch.inference_mode():
            for end in (1,337,1024,16001,n):parts.append(stream.push(mix[...,start:end]));start=end
            parts.append(stream.finish());chunk=torch.cat(parts,-1)[...,S.DELAY:]
        stream_error=float((chunk-wave[:,0]).square().sum()/wave[:,0].square().sum().clamp_min(1e-12))
        if stream_error>1e-9:raise ValueError('Real waveform chunk-state replay failed')
        result[mode]=dict(checkpoint_sha256=T.F.sha(path),same_input_waveform_cpu_cuda_nse=errors,
                          native_input_spectral_nse=input_nse,native_end_to_end_errors=native_errors,source_row=row,
                          neural_causality=cause,real_waveform_chunk_nse=stream_error,
                          distinct_train_songs=len(ck['song_counts']),sealed_holdout_audio_unread=True)
        digests.append(final['last']['sample_sequence_sha256'])
    if len(set(digests))!=1:raise ValueError('Matched draws differ')
    G.write(a.out/('smoke_audit.json' if a.smoke else 'audit.json'),dict(verified=True,smoke=a.smoke,
        arms=result,matched_sample_sequence_sha256=digests[0],protocol_sha256=T.F.sha(a.out/'manifest.json'),
        scope='saved source models and real past-only PCM audit; no scene/C/device/holdout/car quality promotion'))
    print('UNIFIED_CAPACITY_CPU_AUDIT_PASS',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','initial-audit','train','audit'))
    for name in ('cache','initial','partition','monitor','parent','expansion','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--mode',choices=MODES);p.add_argument('--smoke',action='store_true');p.add_argument('--resume',action='store_true')
    a=p.parse_args();{'prepare':prepare,'initial-audit':initial_audit,'train':train,'audit':audit}[a.action](a)
