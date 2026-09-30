#!/usr/bin/env python3
"""Predeclared matched10k extension with physically valid inverse real FFT.

Preserves the original10k run and optimizer states. No architecture, new data,
DEV or scene activation. Additional training and the mandatory FFT correction
are not interpreted as isolated effects of training length.
"""
import argparse
import json
from pathlib import Path
import shutil
import time
from types import SimpleNamespace
import numpy as np
import torch
import train_unified_upmix_fit as T
import unified_upmix_audio as H
import audit_unified_upmix_fit as A

STEPS=20000


def hashes():
    return T.hashes()|{Path(H.__file__).name:T.F.sha(Path(H.__file__)),Path(A.__file__).name:T.F.sha(Path(A.__file__)),Path(__file__).name:T.F.sha(Path(__file__))}


def prepare(a):
    T.configure('cpu')
    if a.out.exists():raise FileExistsError('Preserve prior extension')
    parent=SimpleNamespace(cache=a.cache,initial=a.initial,out=a.parent)
    protocol,prepared=T.check(parent)
    audit=json.loads((a.parent/'audited.json').read_text())
    if not audit['audit_verified'] or audit['fitting_gate_pass'] or audit['protocol_sha256']!=T.F.sha(a.parent/'protocol.json'):
        raise ValueError('Verified failing10k pair required')
    if {k for k,v in audit['gates'].items() if not v}!={'eligible_median_at_least_7db'}:
        raise ValueError('Extension rationale differs')
    initial={}
    for mode in T.MODES:
        p=a.parent/mode/'last.pt'
        if T.F.sha(p)!=audit['arms'][mode]['checkpoint_sha256']:raise ValueError('Parent checkpoint differs')
        ck=torch.load(p,map_location='cpu',weights_only=True)
        if ck['step']!=10000 or ck['mode']!=mode:raise ValueError('Parent state differs')
        initial[mode]=T.F.sha(p)
    manifest=dict(source_hashes=hashes(),parent_protocol_sha256=T.F.sha(a.parent/'protocol.json'),
                  parent_audit_sha256=T.F.sha(a.parent/'audited.json'),parent_prepared_sha256=T.F.sha(a.parent/'prepared.json'),
                  split_sha256=T.F.sha(a.cache/'split.json'),fixture_manifest_sha256=T.F.sha(a.initial/'manifest.json'),
                  initial_checkpoints=initial,steps=STEPS,added_steps=10000,optimizer='preserved AdamW states; constant3e-4, wd1e-4, clip5, TF32 off',
                  targets='same exact4 natural TRAIN8s crops, sources and fixed TRAIN coefficient',
                  synthesis='explicit real DC/Nyquist endpoints before every inverse real FFT; historical spectral loss retained',
                  selection='terminal20k both; original10k retained as failed gate; no DEV/scene/data/architecture change',
                  rationale='joint6.9138dB misses7dB absolute by.086; all relative/protected fitting gates pass; bounded extension, not broad promotion',
                  gates=protocol['gates'],scope='TRAIN fitting only; correction and additional steps confounded across phases')
    a.out.mkdir(parents=True)
    T.F.write_json(a.out/'protocol.json',manifest)
    for mode in T.MODES:shutil.copyfile(a.parent/mode/'last.pt',a.out/('initial_'+mode+'.pt'))
    T.F.write_json(a.out/'prepared.json',prepared)
    print('UNIFIED_FIT20K_EXTENSION_PREPARED',flush=True)


def check(a):
    m=json.loads((a.out/'protocol.json').read_text());p=json.loads((a.out/'prepared.json').read_text())
    if m['source_hashes']!=hashes() or m['split_sha256']!=T.F.sha(a.cache/'split.json') or m['fixture_manifest_sha256']!=T.F.sha(a.initial/'manifest.json'):
        raise ValueError('Frozen extension sources/fixtures differ')
    if m['parent_protocol_sha256']!=T.F.sha(a.parent/'protocol.json') or m['parent_audit_sha256']!=T.F.sha(a.parent/'audited.json'):
        raise ValueError('Original phase differs')
    if m['parent_prepared_sha256']!=T.F.sha(a.out/'prepared.json'):
        raise ValueError('Fixed source normalization/coefficient differs')
    for mode,h in m['initial_checkpoints'].items():
        if T.F.sha(a.out/('initial_'+mode+'.pt'))!=h or T.F.sha(a.parent/mode/'last.pt')!=h:
            raise ValueError('Original/copy checkpoint differs')
    return m,p


def role_loss(pred,refs,roles,i,normalize=1.):
    return H.objective(pred[:,:,i]*normalize,refs[:,:,i]*normalize,roles[:,i]*normalize,roles.shape[-1])


def evaluate(net,x,refs,roles):
    net.eval();losses={};sdr={};tx={}
    with torch.no_grad():
        pred,_=net.forward_stft(x,with_scene=False)
        for i,s in enumerate(T.U.SOURCES):
            losses[s]=float(role_loss(pred['source_stft'],refs,roles,i))
            w=H.istft_centered(pred['source_stft'][:,:,i],roles.shape[-1])
            sdr[s]=T.SN.si_sdr(torch,w,roles[:,i]).mean(-1).tolist()
            sent=T.SN.stft(torch,H.istft_centered(pred['source_masks'][:,:,0]*refs[:,:,i],roles.shape[-1]))
            ref=T.SN.stft(torch,H.istft_centered(refs[:,:,i],roles.shape[-1]))
            den=ref.abs().square().sum((1,2,3));num=sent.abs().square().sum((1,2,3))
            tx[s]=[float(n/d) if float(d)>1e-9 else None for n,d in zip(num,den)]
        power=roles.square().sum((2,3));present=power>power.sum(1,keepdim=True).clamp_min(1e-12)*1e-6
    net.train()
    if not np.isfinite(list(losses.values())).all() or not np.isfinite(list(sdr.values())).all():raise ValueError('Nonfinite evaluation')
    return dict(loss=losses,sdr_db=sdr,eligible_source_tx=tx,source_present={s:present[:,i].tolist() for i,s in enumerate(T.U.SOURCES)})


def train(a):
    T.configure('cuda');m,p=check(a)
    folder=a.out/a.mode
    if folder.exists() and not a.resume:raise FileExistsError('Verified resume required')
    folder.mkdir(exist_ok=True)
    path=folder/'last.pt' if a.resume else a.out/('initial_'+a.mode+'.pt')
    net,ck=T.load(path,'cuda')
    if ck['mode']!=a.mode or (a.resume and ck['protocol_sha256']!=T.F.sha(a.out/'protocol.json')):raise ValueError('Resume semantics differ')
    for v in net.scene.parameters():v.requires_grad_(False)
    scene=T.P.tensors_sha(net.scene)
    opt=torch.optim.AdamW([v for v in net.parameters() if v.requires_grad],lr=3e-4,weight_decay=1e-4)
    opt.load_state_dict(ck['optimizer'])
    start=ck['step'];rows=list(ck['rows']);elapsed=ck['seconds']
    if not 10000<=start<=STEPS:raise ValueError('Wrong extension start')
    roles,refs,x,_=T.fixtures(a,'cuda');begin=time.monotonic()
    for step in range(start,STEPS+1):
        if step>start:
            opt.zero_grad(set_to_none=True)
            pred,_=net.forward_stft(x,with_scene=False)
            loss=role_loss(pred['source_stft'],refs,roles,0)
            if a.mode=='joint_audio':
                loss+=p['auxiliary_weight']*sum(role_loss(pred['source_stft'],refs,roles,i,p['normalizers'][i]) for i in range(1,4))/3
            if not bool(torch.isfinite(loss)):raise ValueError('Nonfinite loss')
            loss.backward();grad=float(torch.nn.utils.clip_grad_norm_([v for v in net.parameters() if v.requires_grad],5))
            if not np.isfinite(grad):raise ValueError('Nonfinite gradient')
            opt.step()
        if step%250==0:
            row=evaluate(net,x,refs,roles);row.update(step=step,seconds=elapsed+time.monotonic()-begin)
            if not rows or rows[-1]['step']!=step:rows.append(row)
            if T.P.tensors_sha(net.scene)!=scene:raise ValueError('Dormant scene changed')
            T.F.write_json(folder/'progress.json',dict(mode=a.mode,rows=rows))
            temporary=folder/'last.pt.new'
            torch.save(dict(architecture=T.U.ARCHITECTURE,config=net.config,state=net.state_dict(),optimizer=opt.state_dict(),
                       step=step,mode=a.mode,rows=rows,seconds=row['seconds'],protocol_sha256=T.F.sha(a.out/'protocol.json')),temporary)
            temporary.replace(folder/'last.pt')
            print('EXTENSION',a.mode,step,'loss',row['loss']['eligible'],'eligibleSDR',float(np.median(row['sdr_db']['eligible'])),'seconds',row['seconds'],flush=True)
    T.F.write_json(folder/'complete.json',dict(complete=True,step=STEPS,mode=a.mode,checkpoint_sha256=T.F.sha(folder/'last.pt'),
                         scene_frozen_sha256=scene,final=rows[-1]))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['prepare','train','audit'])
    for name in ['cache','initial','parent','out']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--mode',choices=T.MODES);p.add_argument('--resume',action='store_true');a=p.parse_args()
    if a.action=='audit':A.verify(a,steps=STEPS,checker=check,filename='complete.json',scope='TRAIN-only20k pair; corrected real FFT; no DEV/scene/device/car verdict')
    else:{'prepare':prepare,'train':train}[a.action](a)
