#!/usr/bin/env python3
"""Fixed40k broad comparison of original and spectral encoders after TRAIN fit.

Requires the completed audited fitting pair to pass before preparing any run.
Both train156true-stem songs, unfreeze their common decoder, freeze scene;
DEV40 is evaluated only at the fixed terminal endpoint; reserved20 stays unread.
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
import train_spectral_context_fit as FT
T,G,H,S=B.T,B.G,B.H,B.S
spectra,waves,objective,read_roles,eval_row,evaluate=(B.spectra,B.waves,B.objective,B.read_roles,B.eval_row,B.evaluate)
STEPS=40000
BASE_OFFSET=0
SNAPSHOTS=(0,1000,4000,10000,20000,30000,40000)
MODES=FT.MODES


def hashes():
    names=('eval_spectral_context_generalization.py','bsrnn_reference_eval.py','bsrnn_reference_infer.py','diag_transitions.py','evaluate_oracle_renderer_probe.py','upmix_behavior_tests.py','upmix_stem_tests.py','oracle_renderer_sweep.py')
    helpers={name:T.F.sha(Path(__file__).with_name(name)) for name in names}
    return B.hashes() | FT.hashes() | helpers | {Path(__file__).name:T.F.sha(Path(__file__))}


def load(path,device):
    net,ck=FT.load(path,device)
    # The fitting-only decoder freeze ends equally in both broad arms.
    for p in net.dec.parameters():p.requires_grad_(True)
    for p in net.scene.parameters():p.requires_grad_(False)
    return net,ck


def lr_at(step):
    if step<=2000:return 3e-4*step/2000
    return 3e-5+(3e-4-3e-5)*.5*(1+math.cos(math.pi*(step-2000)/(STEPS-2000)))


def prepare(a):
    if a.out.exists():raise FileExistsError('Preserve prior broad pair')
    T.configure('cpu')
    fit=json.loads((a.fit/'complete.json').read_bytes())
    fp=json.loads((a.fit/'protocol.json').read_bytes())
    parent=json.loads((a.parent/'manifest.json').read_bytes())
    partition=json.loads(a.partition.read_bytes())
    parent_complete=json.loads((a.parent/'full40/complete.json').read_bytes())
    parent_c_protocol=json.loads((a.parent/'full40/protocol.json').read_bytes())
    if not parent_complete['complete'] or fp['parent_full40_sha256']!=T.F.sha(a.parent/'full40/complete.json') or fp['parent_audit_sha256']!=T.F.sha(a.parent/'audit.json') or parent_c_protocol['source_manifest_sha256']!=T.F.sha(a.parent/'manifest.json'):raise ValueError('Fitting-parent inventory provenance differs')
    if not fit['complete'] or not fit['audit_verified'] or fit['smoke'] or not fit['fitting_gate_pass'] or not all(fit['gates'].values()):raise ValueError('Passed audited terminal20k encoder fit required')
    if fit['protocol_sha256']!=T.F.sha(a.fit/'protocol.json') or fp['sources']!=FT.hashes():raise ValueError('Fitting protocol/source changed')
    if partition['split_sha256']!=parent['split_sha256'] or parent['split_sha256']!=T.F.sha(a.cache/'split.json') or parent['partition_sha256']!=T.F.sha(a.partition):raise ValueError('Frozen original partition differs')
    ids=partition['train'];sealed=partition['holdout']
    if len(ids)!=156 or len(set(ids))!=156 or len(sealed)!=20 or len(set(sealed))!=20 or set(ids)&set(sealed) or set(ids)!=set(parent['train_inventory']):raise ValueError('Original156/20 inventory required')
    inventory=parent['train_inventory']
    for song,r in inventory.items():
        p=a.cache/(song+'.npy');shape,dtype,offset=T.MN.npy_header(str(p))
        if shape!=(7,2,r['samples']) or dtype!=np.dtype('int16') or offset!=r['offset'] or p.stat().st_size!=r['bytes'] or T.F.sha(p.with_suffix('.json'))!=r['metadata_sha256']:raise ValueError('True TRAIN cache inventory changed')
    train_eval=parent['evaluation']['train'];dev_eval=parent['evaluation']['dev']
    if len(train_eval)!=12 or len(dev_eval)!=40 or len({r['song'] for r in dev_eval})!=40 or not {r['song'] for r in train_eval}<=set(ids) or {r['song'] for r in dev_eval}&(set(ids)|set(sealed)):raise ValueError('TRAIN12/DEV40/sealed20 overlap or coverage differs')
    # Read only TRAIN crops in preparation; fixed DEV descriptors are metadata.
    for r in train_eval:
        raw=T.MN.read_window(str(a.cache/(r['song']+'.npy')),r['start_sample'],r['samples'])
        if hashlib.sha256(raw.tobytes()).hexdigest()!=r['crop_sha256']:raise ValueError('Frozen TRAIN monitor crop changed')
    replay=parent['replay']
    if replay['song'] not in inventory:raise ValueError('Replay must be TRAIN')
    read_roles(a,replay,'cpu')
    if fp['normalizers']!=parent['normalizers'] or fp['auxiliary_weight']!=parent['auxiliary_weight']:raise ValueError('Original physical objective changed')
    initials={};costs={};scenes={};decoders={};configs={}
    for mode in MODES:
        path=a.fit/mode/'step20000.pt';net,ck=load(path,'cpu')
        if ck['mode']!=mode or ck['step']!=20000 or ck['smoke'] or ck['protocol_sha256']!=T.F.sha(a.fit/'protocol.json') or T.F.sha(path)!=fit['arms'][mode]['checkpoint_sha256']:raise ValueError('Fixed audited fitting terminal differs')
        initials[mode]=path;configs[mode]=net.config;scenes[mode]=T.P.tensors_sha(net.scene);decoders[mode]=T.P.tensors_sha(net.dec)
        costs[mode]=FT.X.selected_cost(net) if mode=='control' else net.cost()
        selected=costs[mode]['eligible_plus_all_scene_dense_macs_s'] if mode=='control' else costs[mode]['eligible_scene_dense_macs_s']
        if selected>150e6:raise ValueError('Selected dense count exceeds150M')
    if len(set(scenes.values()))!=1 or len(set(decoders.values()))!=1 or next(iter(scenes.values()))!=fp['fixed_scene_sha256'] or next(iter(decoders.values()))!=fp['fixed_decoder_sha256']:raise ValueError('Common fitted decoder/scene provenance differs')
    manifest=dict(version='spectral-context-broad-v1',sources=hashes(),steps=STEPS,snapshots=list(SNAPSHOTS),seed=G.SEED,batch=G.BATCH,seconds=G.SECONDS,
        fitting_audit_sha256=T.F.sha(a.fit/'complete.json'),fitting_protocol_sha256=T.F.sha(a.fit/'protocol.json'),parent_manifest_sha256=T.F.sha(a.parent/'manifest.json'),
        partition_sha256=T.F.sha(a.partition),split_sha256=parent['split_sha256'],train_inventory=inventory,sealed_holdout_ids=sealed,
        evaluation=dict(train=train_eval,dev=dev_eval),replay=replay,initial_sha256={mode:T.F.sha(path) for mode,path in initials.items()},
        architecture=configs,cost=costs,scene_frozen_sha256=scenes,common_initial_decoder_sha256=decoders,normalizers=fp['normalizers'],auxiliary_weight=fp['auxiliary_weight'],
        draw_step_offset=BASE_OFFSET,optimizer='fresh AdamW wd1e-4/clip5 float32 TF32off,2k warmup3e-4/cosine3e-5 at40k both',
        data='same deterministic G.Reader draws1..40000, uniform156trueTRAIN, uniform8s crops, independent7slot +/-6dB gain, exact sum; no remix/new corpus/teacher',
        objective='same physical spectral+multires joint4source objective and original fixed coefficient/normalizers; common decoder UNFROZEN in both, scene frozen',
        intervention='original24x3 versus spectral48x2 architecture-and-encoder-fitting package; each starts from own fixed20k fitting terminal with identical decoder/scene; no incumbent warm start',
        selection='fixed terminal40k, TRAIN12 intermediate diagnostics only; DEV40 first read at terminal snapshot; original three real-C gains .3235/.40/.50 with .40 nominal; reserved20 unread',
        relative_gates='paired DEV20s eligible SDR>=control+.3dB, >=21/40nonworse; every nominal C instrument median>=control-.01; protected C medians<=control+.005; swings and entry median/p10 nonworse; all absolute13gates mandatory',
        source_attribution='all source component sensitivities reported as diagnostics, not a literal audible leakage gate; real-C protected proxies and unchanged attributed entry gates retained',
        numeric_gates=parent['numeric_gates'],
        scope_limits='Original inspected DEV40 only. Common decoder adapts in both, so result is graph+fitting package, not isolated width effect. Scene dormant; no new-graph C/int8/device/85ms/independent20/owner-ears proof. Reserved20 candidate-independent, not globally pristine.')
    a.out.mkdir(parents=True);G.write(a.out/'manifest.json',manifest)
    for mode,path in initials.items():shutil.copyfile(path,a.out/(mode+'_initial.pt'))
    print('SPECTRAL_BROAD_PREPARED156TRAIN/FIXED40k',flush=True)


def check(a):
    m=json.loads((a.out/'manifest.json').read_bytes())
    for key,path in [('fitting_audit_sha256',a.fit/'complete.json'),('fitting_protocol_sha256',a.fit/'protocol.json'),('parent_manifest_sha256',a.parent/'manifest.json'),('partition_sha256',a.partition),('split_sha256',a.cache/'split.json')]:
        if m[key]!=T.F.sha(path):raise ValueError('Frozen broad input changed: '+key)
    if m['sources']!=hashes():raise ValueError('Frozen source bytes changed')
    for mode in MODES:
        if m['initial_sha256'][mode]!=T.F.sha(a.out/(mode+'_initial.pt')) or m['initial_sha256'][mode]!=T.F.sha(a.fit/mode/'step20000.pt'):raise ValueError('Audited fitting initialization changed')
    return m


def snapshot(net,a,m,folder,step,smoke=False):
    subsets={'train':m['evaluation']['train'][:2]} if smoke else ({'train':m['evaluation']['train']} if step<STEPS else m['evaluation'])
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
    net,ck=load(folder/'last.pt' if a.resume else a.out/(a.mode+'_initial.pt'),'cuda')
    if ck['mode']!=a.mode:raise ValueError('Wrong broad arm initialization')
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
        state=dict(architecture=ck['architecture'],config=net.config,state=net.state_dict(),optimizer=opt.state_dict(),
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
                    print('SPECTRAL_BROAD',a.mode,step,'loss',row['loss'],'songs',len(counts),'seconds',elapsed,flush=True)
                if step in SNAPSHOTS or step==cap:snapshot(net,a,m,folder,step,a.smoke)
                del roles,refs,pred,loss
    finally:reader.close()
    G.write(folder/'train_complete.json',dict(complete=True,mode=a.mode,smoke=a.smoke,step=cap,last=history[-1],
        checkpoint_sha256=T.F.sha(folder/f'step{cap}.pt'),protocol_sha256=T.F.sha(a.out/'manifest.json')))


def audit(a):
    T.configure('cpu');m=check(a);step=20 if a.smoke else STEPS;result={};digests=[]
    for mode in MODES:
        folder=a.out/(mode+('_smoke' if a.smoke else ''));path=folder/f'step{step}.pt'
        final=json.loads((folder/'train_complete.json').read_bytes());net,ck=load(path,'cpu')
        if not final['complete'] or final['step']!=step or final['mode']!=mode or final['smoke']!=a.smoke or final['checkpoint_sha256']!=T.F.sha(path) or ck['protocol_sha256']!=T.F.sha(a.out/'manifest.json'):
            raise ValueError('Final checkpoint/protocol differs')
        if set(ck['song_counts'])-set(m['train_inventory']) or sum(ck['song_counts'].values())!=step*G.BATCH or (not a.smoke and len(ck['song_counts'])!=156):
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
            if (row['sdr_db'][s] is None)!=(saved['row']['sdr_db'][s] is None):raise ValueError('Present-source CPU/device SDR coverage differs')
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
    print('SPECTRAL_BROAD_CPU_AUDIT_PASS',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','train','audit'))
    for name in ('cache','fit','partition','parent','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--mode',choices=MODES);p.add_argument('--smoke',action='store_true');p.add_argument('--resume',action='store_true')
    a=p.parse_args();{'prepare':prepare,'train':train,'audit':audit}[a.action](a)
