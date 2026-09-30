#!/usr/bin/env python3
"""Matched fresh encoders, identical fixed trained decoder, original TRAIN4 only.

Preparation requires the frozen capacity full40 pair to finish and the expanded
arm to fail absolute acoustic gates. No DEV/holdout in this fitting comparison.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import time
import numpy as np
import torch
import train_unified_upmix_fit as T
import train_unified_upmix_generalization as B
import expand_unified_upmix as X
import unified_spectral_context_net as V
S=B.S
MODES=('control','spectral')
STEPS=20000
SNAPSHOTS=(0,1000,4000,10000,20000)
PARENT_SHA='1b0cb33f1b3faac1c0f655a7c0f40cebe55640b7a60a254346edf0a762ce9aa6'


def write(p,value):p.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
def hashes():
    modules=(T,B,X,V,S,T.MN,T.SN,T.P,T.F,T.U,B.G,B.H)
    return {Path(m.__file__).name:T.F.sha(Path(m.__file__)) for m in modules}|{Path(__file__).name:T.F.sha(Path(__file__))}


def initialise(parent,mode,seed=1):
    if mode not in MODES:raise ValueError('Unknown arm')
    common={k:parent.config[k] for k in ('log_mean','log_std')}
    if {k:parent.config[k] for k in ('width','depth','local','global_width','decoder')}!={'width':24,'depth':3,'local':24,'global_width':24,'decoder':24}:raise ValueError('Exact expanded24 decoder required')
    torch.manual_seed(seed)
    net=(T.U.UnifiedUpmixNet(width=24,depth=3,local=24,global_width=24,decoder=24,**common) if mode=='control' else V.SpectralContextNet(**common))
    net.dec.load_state_dict(parent.dec.state_dict(),strict=True)
    net.scene.load_state_dict(parent.scene.state_dict(),strict=True)
    for module in (net.dec,net.scene):
        for p in module.parameters():p.requires_grad_(False)
    return net


def load(path,device):
    ck=torch.load(path,map_location=device,weights_only=True)
    cls={'control':T.U.UnifiedUpmixNet,'spectral':V.SpectralContextNet}
    mode=ck['mode']
    if ck['architecture']!=(T.U.ARCHITECTURE if mode=='control' else V.ARCHITECTURE):raise ValueError('Checkpoint architecture mismatch')
    net=cls[mode](**ck['config']).to(device);net.load_state_dict(ck['state'],strict=True)
    for module in (net.dec,net.scene):
        for p in module.parameters():p.requires_grad_(False)
    return net,ck


def fixtures(a,device):
    arrays,original=T.P.data(a.cache,a.fixtures)
    raw=torch.from_numpy(arrays).to(device)
    roles=torch.stack([raw[:,6],raw[:,0]+raw[:,1],raw[:,2:5].sum(1),raw[:,5]],1)
    return roles,S.analyze(roles.sum(1)),B.spectra(roles),original


def prepare(a):
    if a.out.exists():raise FileExistsError('Preserve prior fitting attempt')
    T.configure('cpu')
    final=json.loads((a.parent/'full40/complete.json').read_bytes())
    audit=json.loads((a.parent/'audit.json').read_bytes())
    manifest=json.loads((a.parent/'manifest.json').read_bytes())
    if not final['complete'] or not audit['verified'] or audit['smoke']:raise ValueError('Completed capacity acoustic/numeric pair required')
    if final['arms']['expanded']['nominal_observed_gates_pass']:raise ValueError('Existing candidate passes; qualify it before another encoder')
    path=a.parent/'expanded/step20000.pt'
    if T.F.sha(path)!=PARENT_SHA or final['arms']['expanded']['checkpoint_sha256']!=PARENT_SHA or audit['arms']['expanded']['checkpoint_sha256']!=PARENT_SHA:raise ValueError('Predeclared fixed parent differs')
    parent,_=T.load(path,'cpu');roles,_,_,original=fixtures(a,'cpu')
    if not {r['song'] for r in original['windows']}<=set(manifest['train_inventory']):raise ValueError('Fitting crops intersect reserved audio')
    costs={};initials={}
    for mode in MODES:
        net=initialise(parent,mode)
        costs[mode]=X.selected_cost(net) if mode=='control' else net.cost()
        selected=costs[mode]['eligible_plus_all_scene_dense_macs_s'] if mode=='control' else costs[mode]['eligible_scene_dense_macs_s']
        if selected>150e6:raise ValueError('Selected dense count exceeds150M')
        if T.P.tensors_sha(net.dec)!=T.P.tensors_sha(parent.dec):raise ValueError('Fixed decoder differs')
        initials[mode]=net
    protocol=dict(version='spectral-context-fresh-encoder-fit-v1',steps=STEPS,snapshots=SNAPSHOTS,seed=1,sources=hashes(),
        parent_sha256=PARENT_SHA,parent_full40_sha256=T.F.sha(a.parent/'full40/complete.json'),parent_audit_sha256=T.F.sha(a.parent/'audit.json'),
        original_fixture_manifest_sha256=T.F.sha(a.fixtures/'manifest.json'),split_sha256=T.F.sha(a.cache/'split.json'),windows=original['windows'],
        role_tensor_sha256=hashlib.sha256(roles.numpy().tobytes()).hexdigest(),
        normalizers=manifest['normalizers'],auxiliary_weight=manifest['auxiliary_weight'],
        fixed_decoder_sha256=T.P.tensors_sha(parent.dec),fixed_scene_sha256=T.P.tensors_sha(parent.scene),cost=costs,
        architecture={mode:net.config for mode,net in initials.items()},
        objective='unchanged physical spectral+multires four-source loss and original fixed coefficient/normalizers; no gain/remix/teacher/loss change',
        intervention='fresh24x3 time/frequency RNN control versus fresh48x2 grouped-time/current-frame spectral mixer; same frozen trained24 decoder in both; no incumbent warm start',
        optimizer='fresh AdamW lr3e-4,wd1e-4,clip5; float32 TF32off; all four original8s TRAIN crops every step, no augmentation or random windows',
        selection='fixed terminal20k, diagnostics only at0/1k/4k/10k; no DEV/holdout/early acoustic stop or broader run from a smoke',
        gates='eligible paired crop median SI-SDR gain>=.5dB, candidate median>=7dB, all eligible crop deltas>=-.3dB; each present protected source median>=0dB and>=control-.3dB; eligible loss<=1.05control; strict fixed-decoder/numeric/chunk/future audit',
        scope='TRAIN encoder-fitting package only; not causal generalization, renderer/int8/device/listening promotion; current scene dormant; different fresh initial functions are part of graph package')
    a.out.mkdir(parents=True);write(a.out/'protocol.json',protocol)
    for mode,net in initials.items():
        torch.save(dict(architecture=T.U.ARCHITECTURE if mode=='control' else V.ARCHITECTURE,mode=mode,config=net.config,state=net.state_dict(),protocol_sha256=T.F.sha(a.out/'protocol.json')),a.out/(mode+'_initial.pt'))
    write(a.out/'prepared.json',dict(complete=True,initial_sha256={mode:T.F.sha(a.out/(mode+'_initial.pt')) for mode in MODES},protocol_sha256=T.F.sha(a.out/'protocol.json')))
    print('SPECTRAL_CONTEXT_TRAIN4_PREPARED',flush=True)


def check(a):
    p=json.loads((a.out/'protocol.json').read_bytes());ready=json.loads((a.out/'prepared.json').read_bytes())
    if p['sources']!=hashes() or p['split_sha256']!=T.F.sha(a.cache/'split.json') or p['original_fixture_manifest_sha256']!=T.F.sha(a.fixtures/'manifest.json') or p['parent_full40_sha256']!=T.F.sha(a.parent/'full40/complete.json') or p['parent_audit_sha256']!=T.F.sha(a.parent/'audit.json'):raise ValueError('Frozen input/source changed')
    if ready['protocol_sha256']!=T.F.sha(a.out/'protocol.json'):raise ValueError('Protocol changed')
    for mode in MODES:
        if ready['initial_sha256'][mode]!=T.F.sha(a.out/(mode+'_initial.pt')):raise ValueError('Initial changed')
    return p


def evaluate(net,roles,protocol):
    rows=[]
    with torch.inference_mode():
        for i,record in enumerate(protocol['windows']):
            row,_=B.eval_row(net,roles[i:i+1],record|dict(warm_samples=0));rows.append(row)
    return dict(rows=rows,loss={s:float(np.median([r['loss'][s] for r in rows])) for s in T.U.SOURCES},
        sdr_db={s:float(np.median([r['sdr_db'][s] for r in rows if r['sdr_db'][s] is not None])) if any(r['sdr_db'][s] is not None for r in rows) else None for s in T.U.SOURCES})


def train(a):
    T.configure(a.device);p=check(a);cap=20 if a.smoke else STEPS
    folder=a.out/(a.mode+('_smoke' if a.smoke else ''));folder.mkdir(exist_ok=a.resume)
    net,ck=load(folder/'last.pt' if a.resume else a.out/(a.mode+'_initial.pt'),a.device)
    if ck['mode']!=a.mode or ck['protocol_sha256']!=T.F.sha(a.out/'protocol.json'):raise ValueError('Wrong initial/resume')
    if a.resume and ck['smoke']!=a.smoke:raise ValueError('Resume smoke semantics differ')
    roles,mix,refs,_=fixtures(a,a.device)
    if hashlib.sha256(roles.cpu().numpy().tobytes()).hexdigest()!=p['role_tensor_sha256']:raise ValueError('Role fixture changed')
    params=[v for v in net.parameters() if v.requires_grad];opt=torch.optim.AdamW(params,lr=3e-4,weight_decay=1e-4)
    start=ck.get('step',0);history=ck.get('history',[]);prior=ck.get('seconds',0.)
    if a.resume:opt.load_state_dict(ck['optimizer'])
    begin=time.monotonic()
    def save(step):
        if T.P.tensors_sha(net.dec)!=p['fixed_decoder_sha256'] or T.P.tensors_sha(net.scene)!=p['fixed_scene_sha256']:raise ValueError('Frozen decoder/scene changed')
        saved=dict(architecture=ck['architecture'],config=net.config,state={k:v.detach().cpu() for k,v in net.state_dict().items()},
            mode=a.mode,smoke=a.smoke,step=step,optimizer=opt.state_dict(),history=history,seconds=prior+time.monotonic()-begin,protocol_sha256=T.F.sha(a.out/'protocol.json'))
        B.G.save(folder/'last.pt',saved)
        if step in SNAPSHOTS or step==cap:B.G.save(folder/f'step{step}.pt',saved)
    def snapshot(step):
        net.eval();metrics=evaluate(net,roles,p);net.train()
        write(folder/f'eval_step{step}.json',dict(step=step,metrics=metrics,checkpoint_sha256=T.F.sha(folder/f'step{step}.pt'),protocol_sha256=T.F.sha(a.out/'protocol.json')))
    if not start and not a.smoke:save(0);snapshot(0)
    for step in range(start+1,cap+1):
        net.train();opt.zero_grad(set_to_none=True);pred,_=net.forward_stft(mix,with_scene=False)
        loss=B.objective(pred['source_stft'],refs,roles,0)
        loss+=p['auxiliary_weight']*sum(B.objective(pred['source_stft'],refs,roles,i,p['normalizers'][i]) for i in range(1,4))/3
        if not bool(torch.isfinite(loss)):raise ValueError('Nonfinite loss')
        loss.backward();grad=float(torch.nn.utils.clip_grad_norm_(params,5.))
        if not np.isfinite(grad):raise ValueError('Nonfinite gradient')
        opt.step()
        if step%250==0 or step==cap:
            history.append(dict(step=step,loss=float(loss.detach()),gradient_norm=grad,seconds=prior+time.monotonic()-begin))
            save(step);write(folder/'progress.json',dict(rows=history));print('SPECTRAL_ENCODER_FIT',a.mode,step,json.dumps(history[-1]),flush=True)
        if step in SNAPSHOTS or step==cap:snapshot(step)
    net.eval()
    with torch.inference_mode():
        pred,_=net.forward_stft(mix,with_scene=False)
        torch.save(dict(input_stft=mix.cpu(),waveform=B.waves(pred['source_stft'],roles.shape[-1]).cpu(),checkpoint_sha256=T.F.sha(folder/f'step{cap}.pt')),folder/'device_replay.pt')
    write(folder/'train_complete.json',dict(complete=True,mode=a.mode,smoke=a.smoke,step=cap,checkpoint_sha256=T.F.sha(folder/f'step{cap}.pt'),protocol_sha256=T.F.sha(a.out/'protocol.json')))


def audit(a):
    T.configure('cpu');p=check(a);cap=20 if a.smoke else STEPS
    roles,mix,_,_=fixtures(a,'cpu');arms={};numerical={}
    for mode in MODES:
        folder=a.out/(mode+('_smoke' if a.smoke else ''));path=folder/f'step{cap}.pt'
        final=json.loads((folder/'train_complete.json').read_bytes());net,ck=load(path,'cpu');net.eval()
        if not final['complete'] or final['mode']!=mode or final['step']!=cap or final['smoke']!=a.smoke or final['checkpoint_sha256']!=T.F.sha(path) or ck['protocol_sha256']!=T.F.sha(a.out/'protocol.json'):raise ValueError('Saved terminal provenance differs')
        if T.P.tensors_sha(net.dec)!=p['fixed_decoder_sha256'] or T.P.tensors_sha(net.scene)!=p['fixed_scene_sha256']:raise ValueError('Saved fixed head differs')
        saved=torch.load(folder/'device_replay.pt',map_location='cpu',weights_only=True)
        if saved['checkpoint_sha256']!=T.F.sha(path):raise ValueError('Replay checkpoint differs')
        with torch.inference_mode():
            native,_=net.forward_stft(mix,with_scene=False)
            canonical,_=net.forward_stft(saved['input_stft'],with_scene=False)
            native_wave=B.waves(native['source_stft'],roles.shape[-1]);canonical_wave=B.waves(canonical['source_stft'],roles.shape[-1])
        nse=lambda y,r:float((y-r).abs().double().square().sum()/r.abs().double().square().sum().clamp_min(1e-12))
        input_nse=nse(mix,saved['input_stft']);errors={}
        if input_nse>1e-12:raise ValueError('Native input FFT parity failed')
        for i,source in enumerate(T.U.SOURCES):
            reference=saved['waveform'][:,i]
            e=dict(same_input_waveform_nse=nse(canonical_wave[:,i],reference),native_waveform_nse=nse(native_wave[:,i],reference),max_pcm16_lsb=float((native_wave[:,i]-reference).abs().max()*32768))
            if e['same_input_waveform_nse']>1e-9 or e['native_waveform_nse']>1e-7 or e['max_pcm16_lsb']>4:raise ValueError('Saved CPU/device waveform parity failed')
            errors[source]=e
        metrics=evaluate(net,roles,p)
        saved_metrics=json.loads((folder/f'eval_step{cap}.json').read_bytes())['metrics']
        for old,new in zip(saved_metrics['rows'],metrics['rows']):
            for source in T.U.SOURCES:
                if abs(old['loss'][source]-new['loss'][source])>1e-4:raise ValueError('Acoustic loss parity failed')
                x,y=old['sdr_db'][source],new['sdr_db'][source]
                if (x is None)!=(y is None) or (x is not None and abs(x-y)>.05):raise ValueError('Acoustic SDR parity failed')
        cause=T.temporal_audit(net,mix)
        stream=S.Stream(net);parts=[];begin=0;n=roles.shape[-1]
        with torch.inference_mode():
            for end in (1,337,1024,16001,n):parts.append(stream.push(roles[:1].sum(1)[...,begin:end]));begin=end
            parts.append(stream.finish())
        chunk=torch.cat(parts,-1)[...,S.DELAY:];chunk_error=nse(chunk,native_wave[:1,0])
        if chunk_error>1e-9:raise ValueError('Real PCM grouped-state chunk replay failed')
        arms[mode]=metrics|dict(checkpoint_sha256=T.F.sha(path))
        numerical[mode]=dict(input_nse=input_nse,source_errors=errors,causality=cause,waveform_chunk_nse=chunk_error)
    old,new=arms['control'],arms['spectral'];delta=[r['sdr_db']['eligible']-c['sdr_db']['eligible'] for c,r in zip(old['rows'],new['rows'])]
    gates=dict(eligible_paired_gain_05db=float(np.median(delta))>=.5,eligible_median_at_least7db=new['sdr_db']['eligible']>=7,
        each_eligible_crop_nonworse_03db=min(delta)>=-.3,eligible_median_crop_loss_nonworse_5pct=new['loss']['eligible']<=1.05*old['loss']['eligible'])
    for source in T.U.SOURCES[1:]:
        x,y=new['sdr_db'][source],old['sdr_db'][source]
        gates[source+'_reconstruction_nonworse_03db']=x is not None and y is not None and x>=y-.3
        gates[source+'_reconstruction_at_least0db']=x is not None and x>=0
    write(a.out/('smoke_audit.json' if a.smoke else 'complete.json'),dict(complete=True,audit_verified=True,smoke=a.smoke,
        fitting_gate_pass=False if a.smoke else all(gates.values()),gates=gates,paired_crop_sdr_deltas_db=delta,arms=arms,numerical=numerical,
        protocol_sha256=T.F.sha(a.out/'protocol.json'),scope='same fixed TRAIN4 decoder, fresh matched encoder fitting; smoke gates never promote; no DEV/C/int8/device/listening proof'))
    print('SPECTRAL_CONTEXT_SAVED_NUMERIC_AUDIT_PASS',json.dumps(gates),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=['prepare','train','audit'])
    for field in ('cache','fixtures','parent','out'):parser.add_argument('--'+field,type=Path,required=True)
    parser.add_argument('--mode',choices=MODES);parser.add_argument('--device',choices=['cpu','cuda'],default='cuda')
    parser.add_argument('--smoke',action='store_true');parser.add_argument('--resume',action='store_true')
    args=parser.parse_args()
    if args.action=='train' and args.mode is None:parser.error('train requires --mode')
    {'prepare':prepare,'train':train,'audit':audit}[args.action](args)
