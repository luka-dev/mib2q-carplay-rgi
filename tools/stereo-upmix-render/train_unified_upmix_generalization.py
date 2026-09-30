#!/usr/bin/env python3
"""Matched source-supervision study, real causal waveform path, terminal40k.

Both arms start from the SAME audited joint20k weights. Only the protected audio
objectives differ. Scene heads are frozen; the candidate holdout stays unread.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import time
import numpy as np
import torch
import train_unified_upmix_fit as T
import train_causal_reference_generalization as G
import unified_upmix_audio as H
import unified_upmix_stream as S

STEPS = 40000
SNAPSHOTS = (0, 1000, 4000, 10000, 20000, 30000, 40000)
MODES = T.MODES


def hashes():
    return T.hashes() | {Path(m.__file__).name:T.F.sha(Path(m.__file__)) for m in (G,H,S)} | {Path(__file__).name:T.F.sha(Path(__file__))}


def spectra(roles):
    b, k, _, n = roles.shape
    flat = S.analyze(roles.reshape(b*k,2,n))
    return flat.reshape(b,k,flat.shape[1],T.SN.N_BINS,2).permute(0,2,1,3,4)


def waves(pred, samples):
    b, frames, k, bins, channels = pred.shape
    flat = pred.permute(0,2,1,3,4).reshape(b*k,frames,bins,channels)
    return S.synthesize(flat,samples).reshape(b,k,channels,samples)


def objective(pred, refs, roles, i, normalize=1.):
    selected = pred[:,:,i] * normalize
    return T.SN.loss_audio(torch,selected,refs[:,:,i]*normalize) + T.SN.multires_loss(
        torch,S.synthesize(selected,roles.shape[-1]),roles[:,i]*normalize)


def prepare(a):
    T.configure('cpu')
    if a.out.exists():
        raise FileExistsError('Preserve previous run')
    parent = json.loads((a.initial/'complete.json').read_bytes())
    if not parent['audit_verified'] or not parent['fitting_gate_pass']:
        raise ValueError('Passed terminal20k fitting gate required')
    path = a.initial/'joint_audio/last.pt'
    if T.F.sha(path) != parent['arms']['joint_audio']['checkpoint_sha256']:
        raise ValueError('Audited joint20k changed')
    net,ck = T.load(path,'cpu')
    if ck['step'] != 20000 or ck['mode'] != 'joint_audio':
        raise ValueError('Wrong common warm start')
    partition = json.loads(a.partition.read_bytes())
    old = json.loads(a.monitor.read_bytes())
    if partition['split_sha256'] != old['split_sha256'] or old['split_sha256'] != T.F.sha(a.cache/'split.json'):
        raise ValueError('Original split changed')
    ids = partition['train']
    if len(ids)!=156 or len(partition['holdout'])!=20 or set(ids)&set(partition['holdout']):
        raise ValueError('156/20 partition differs')
    inventory = {s:old['train_inventory'][s] for s in ids}
    for s,r in inventory.items():
        p = a.cache/(s+'.npy')
        shape,dtype,offset = T.MN.npy_header(str(p))
        if shape!=(7,2,r['samples']) or dtype!=np.dtype('int16') or offset!=r['offset'] or p.stat().st_size!=r['bytes'] or T.F.sha(p.with_suffix('.json'))!=r['metadata_sha256']:
            raise ValueError('Training cache/header/metadata changed')
    train_eval = old['evaluation']['train']
    if not {r['song'] for r in train_eval} <= set(ids):
        raise ValueError('TRAIN monitor intersects sealed holdout')
    for r in train_eval+old['evaluation']['dev']:
        raw=T.MN.read_window(str(a.cache/(r['song']+'.npy')),r['start_sample'],r['samples'])
        if hashlib.sha256(raw.tobytes()).hexdigest()!=r['crop_sha256']:
            raise ValueError('Fixed evaluation crop changed')
    replay = dict(train_eval[0]); replay.update(samples=4*T.SN.SR,warm_samples=0,score_samples=4*T.SN.SR)
    raw=T.MN.read_window(str(a.cache/(replay['song']+'.npy')),replay['start_sample'],replay['samples'])
    replay['crop_sha256']=hashlib.sha256(raw.tobytes()).hexdigest()
    prepared=json.loads((a.initial/'prepared.json').read_bytes())
    manifest=dict(version='unified-broad-source-v1',sources=hashes(),steps=STEPS,seed=G.SEED,batch=G.BATCH,seconds=G.SECONDS,
        initial_sha256=T.F.sha(path),parent_audit_sha256=T.F.sha(a.initial/'complete.json'),
        parent_protocol_sha256=T.F.sha(a.initial/'protocol.json'),partition_sha256=T.F.sha(a.partition),
        monitor_manifest_sha256=T.F.sha(a.monitor),split_sha256=partition['split_sha256'],
        train_inventory=inventory,evaluation=dict(train=train_eval,dev=old['evaluation']['dev']),replay=replay,
        architecture=net.config,cost=net.cost(),normalizers=prepared['normalizers'],auxiliary_weight=prepared['auxiliary_weight'],
        scene_frozen_sha256=T.P.tensors_sha(net.scene),snapshots=list(SNAPSHOTS),
        optimizer='fresh AdamW wd1e-4/clip5, float32 TF32 off;2k warmup to3e-4, cosine to3e-5 at40k',
        data='uniform156 real TRAIN songs, uniform8s crops, independent7-slot +/-6dB gains, exact sum; same G.Reader sequence in both arms; no remix/teacher/new corpus',
        inference='past-only zero-start Hann2048/512, physical inverseFFT endpoints, carried neural state; TRAIN compensates fixed1536-sample output delay',
        objective='same spectral+multires audio objective and four-crop fixed normalizers/coefficient; eligible_only omits protected3 terms; both start with identical trained4-source decoder',
        selection='fixed terminal40k both; intermediate DEV reports diagnostic only; no best-DEV selection or gain tuning; candidate-holdout20 never loaded',
        relative_gates='joint eligible median SDR>=control-.3dB; eligible attributedTX>=.9control; each protectedTX<=control+.01; source/C entry and swings nonworse; absolute full40 remains required',
        limits='Different156TRAIN inventory from older176TRAIN reference, so old/new is package comparison. Four-crop warm start used centered fitting; broad path is causal. Scene dormant. No device/int8/car proof. Reserved20 are candidate-independent, not globally pristine.')
    a.out.mkdir(parents=True)
    G.write(a.out/'manifest.json',manifest)
    shutil.copyfile(path,a.out/'initial.pt')
    shutil.copyfile(a.partition,a.out/'partition.json')
    print('UNIFIED_BROAD_PREPARED156TRAIN/40DEV/sealed20',flush=True)


def check(a):
    m=json.loads((a.out/'manifest.json').read_bytes())
    for key,path in [('split_sha256',a.cache/'split.json'),('partition_sha256',a.partition),('monitor_manifest_sha256',a.monitor),
                     ('parent_audit_sha256',a.initial/'complete.json'),('parent_protocol_sha256',a.initial/'protocol.json')]:
        if m[key]!=T.F.sha(path):raise ValueError('Frozen input changed: '+key)
    if m['sources']!=hashes() or m['initial_sha256']!=T.F.sha(a.out/'initial.pt') or m['initial_sha256']!=T.F.sha(a.initial/'joint_audio/last.pt'):
        raise ValueError('Sources/common initialization changed')
    return m


def read_roles(a,record,device):
    raw=T.MN.read_window(str(a.cache/(record['song']+'.npy')),record['start_sample'],record['samples'])
    if hashlib.sha256(raw.tobytes()).hexdigest()!=record['crop_sha256']:
        raise ValueError('Monitor/replay signal changed')
    w=torch.from_numpy(raw.astype(np.float32)/32768).to(device)
    return torch.stack([w[6],w[0]+w[1],w[2:5].sum(0),w[5]])[None]


@torch.inference_mode()
def eval_row(net,roles,record):
    net.eval();refs=spectra(roles);pred,_=net.forward_stft(refs.sum(2),with_scene=False)
    n=roles.shape[-1];cut=record['warm_samples'];source_waves=waves(pred['source_stft'],n)
    attributed=waves(pred['source_masks'][:,:,0,None]*refs,n)
    closure=float((attributed.sum(1)-source_waves[:,0]).square().sum()/source_waves[:,0].square().sum().clamp_min(1e-12))
    if closure>1e-9:raise ValueError('Source attribution closure failed')
    loss={};sdr={};tx={};target=roles[...,cut:];wave=source_waves[...,cut:];sent=attributed[...,cut:]
    present=target.square().sum((0,2,3))>target.square().sum()*1e-6
    for i,s in enumerate(T.U.SOURCES):
        y=T.SN.stft(torch,wave[:,i]);r=T.SN.stft(torch,target[:,i])
        loss[s]=float(T.SN.loss_audio(torch,y,r)+T.SN.multires_loss(torch,wave[:,i],target[:,i]))
        sdr[s]=float(T.SN.si_sdr(torch,wave[:,i],target[:,i]).mean()) if bool(present[i]) else None
        den=target[:,i].double().square().sum()
        tx[s]=float(sent[:,i].double().square().sum()/den) if bool(present[i]) else None
    e=T.SN.stft(torch,target[:,0]);z=T.SN.stft(torch,sent[:,0]);y=T.SN.stft(torch,wave[:,0]);bands={}
    for name,lo,hi in [('0-300',0,13),('300-8k',13,342),('8-14k',342,598),('14-24k',598,1025)]:
        den=e[:,:,lo:hi].abs().double().square().sum()
        bands[name]=dict(eligible_tx=float(z[:,:,lo:hi].abs().double().square().sum()/den) if float(den)>1e-9 else None,
                        error_nse=float((y[:,:,lo:hi]-e[:,:,lo:hi]).abs().double().square().sum()/den) if float(den)>1e-9 else None)
    if not np.isfinite(list(loss.values())).all():raise ValueError('Nonfinite evaluation')
    return dict(song=record['song'],loss=loss,sdr_db=sdr,source_tx=tx,bands=bands,closure_nse=closure),source_waves


def evaluate(net,a,records,device):
    rows=[eval_row(net,read_roles(a,r,device),r)[0] for r in records]
    return dict(rows=rows,summary=dict(
        loss={s:G.median([r['loss'][s] for r in rows]) for s in T.U.SOURCES},
        sdr_db={s:G.median([r['sdr_db'][s] for r in rows]) for s in T.U.SOURCES},
        source_tx={s:G.median([r['source_tx'][s] for r in rows]) for s in T.U.SOURCES},
        bands={b:{k:G.median([r['bands'][b][k] for r in rows]) for k in rows[0]['bands'][b]} for b in rows[0]['bands']}))


def snapshot(net,a,m,folder,step,smoke=False):
    subsets={'train':m['evaluation']['train'][:2]} if smoke else m['evaluation']
    result=dict(step=step,mode=a.mode,metrics={k:evaluate(net,a,v,'cuda') for k,v in subsets.items()},
                checkpoint_sha256=T.F.sha(folder/f'step{step}.pt'),protocol_sha256=T.F.sha(a.out/'manifest.json'),
                scene_sha256=T.P.tensors_sha(net.scene))
    G.write(folder/f'eval_step{step}.json',result)
    if step==(20 if smoke else STEPS):
        roles=read_roles(a,m['replay'],'cuda');row,wave=eval_row(net,roles,m['replay'])
        torch.save(dict(waveform=wave.cpu(),row=row,checkpoint_sha256=result['checkpoint_sha256'],
                        crop_sha256=m['replay']['crop_sha256']),folder/'cuda_replay.pt')
    print('EVAL',a.mode,step,json.dumps({k:v['summary'] for k,v in result['metrics'].items()}),flush=True)


def train(a):
    T.configure('cuda');m=check(a);folder=a.out/(a.mode+('_smoke' if a.smoke else ''))
    folder.mkdir(exist_ok=a.resume)
    net,ck=T.load(folder/'last.pt' if a.resume else a.out/'initial.pt','cuda')
    for p in net.scene.parameters():p.requires_grad_(False)
    opt=torch.optim.AdamW([p for p in net.parameters() if p.requires_grad],lr=G.lr_at(1),weight_decay=1e-4)
    step0=0;history=[];counts=Counter();exposure=np.zeros(4,np.int64);descriptors=[];digest=hashlib.sha256();prior=0.
    if a.resume:
        if ck['protocol_sha256']!=T.F.sha(a.out/'manifest.json') or ck['mode']!=a.mode or ck['smoke']!=a.smoke:
            raise ValueError('Resume semantics differ')
        opt.load_state_dict(ck['optimizer']);step0=ck['step'];history=ck['history'];counts.update(ck['song_counts'])
        exposure=np.array(ck['audible_source_windows']);descriptors=ck['descriptor_hashes'];prior=ck['seconds']
        for h in descriptors:digest.update(bytes.fromhex(h))
    def save(step,elapsed):
        if T.P.tensors_sha(net.scene)!=m['scene_frozen_sha256']:raise ValueError('Dormant scene changed')
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
            pending=producer.submit(reader.batch,step0+1)
            for step in range(step0+1,cap+1):
                arrays,draws,energies=pending.result()
                if step<cap:pending=producer.submit(reader.batch,step+1)
                h=hashlib.sha256(json.dumps(draws,sort_keys=True).encode()).hexdigest();descriptors.append(h);digest.update(bytes.fromhex(h))
                counts.update(r['song'] for r in draws);exposure+=(energies>10**(-55/10)).sum(0)
                roles=torch.from_numpy(arrays).to('cuda');refs=spectra(roles);net.train();opt.zero_grad(set_to_none=True)
                for group in opt.param_groups:group['lr']=G.lr_at(step)
                pred,_=net.forward_stft(refs.sum(2),with_scene=False)
                loss=objective(pred['source_stft'],refs,roles,0)
                if a.mode=='joint_audio':
                    loss+=m['auxiliary_weight']*sum(objective(pred['source_stft'],refs,roles,i,m['normalizers'][i]) for i in range(1,4))/3
                if not bool(torch.isfinite(loss)):raise ValueError('Nonfinite training loss')
                loss.backward();grad=float(torch.nn.utils.clip_grad_norm_([p for p in net.parameters() if p.requires_grad],5.))
                if not np.isfinite(grad):raise ValueError('Nonfinite gradient')
                decoder_grad=sum(float(p.grad.square().sum()) for p in net.dec.parameters() if p.grad is not None)**.5
                opt.step()
                if step%250==0 or step==cap:
                    elapsed=prior+time.monotonic()-begin
                    row=dict(step=step,seconds=elapsed,loss=float(loss),grad_norm=grad,decoder_grad_norm=decoder_grad,
                             lr=G.lr_at(step),distinct_train_songs=len(counts),windows=step*G.BATCH,
                             audible_source_windows=exposure.tolist(),sample_sequence_sha256=digest.hexdigest())
                    history.append(row);G.write(folder/'progress.json',dict(rows=history,song_counts=dict(counts)))
                    save(step,elapsed)
                    print('UNIFIED_BROAD',a.mode,step,'loss',row['loss'],'songs',len(counts),'seconds',elapsed,flush=True)
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
        if T.P.tensors_sha(net.scene)!=m['scene_frozen_sha256']:raise ValueError('Scene unexpectedly trained')
        saved=torch.load(folder/'cuda_replay.pt',map_location='cpu',weights_only=True)
        if saved['checkpoint_sha256']!=T.F.sha(path) or saved['crop_sha256']!=m['replay']['crop_sha256']:
            raise ValueError('GPU saved waveform/input provenance differs')
        roles=read_roles(a,m['replay'],'cpu');row,wave=eval_row(net,roles,m['replay']);errors={}
        for i,s in enumerate(T.U.SOURCES):
            y=saved['waveform'][:,i];errors[s]=float((y-wave[:,i]).double().square().sum()/y.double().square().sum().clamp_min(1e-12))
        if max(errors.values())>1e-9:raise ValueError('Saved all-output CPU/CUDA waveform parity failed')
        cause=T.temporal_audit(net,spectra(roles).sum(2))
        stream=S.Stream(net);n=roles.shape[-1];mix=roles.sum(1);parts=[];start=0
        with torch.inference_mode():
            for end in (1,337,1024,16001,n):parts.append(stream.push(mix[...,start:end]));start=end
            parts.append(stream.finish());chunk=torch.cat(parts,-1)[...,S.DELAY:]
        stream_error=float((chunk-wave[:,0]).square().sum()/wave[:,0].square().sum().clamp_min(1e-12))
        if stream_error>1e-9:raise ValueError('Real waveform chunk-state replay failed')
        result[mode]=dict(checkpoint_sha256=T.F.sha(path),waveform_cpu_cuda_nse=errors,source_row=row,
                          neural_causality=cause,real_waveform_chunk_nse=stream_error,
                          distinct_train_songs=len(ck['song_counts']),sealed_holdout_audio_unread=True)
        digests.append(final['last']['sample_sequence_sha256'])
    if len(set(digests))!=1:raise ValueError('Matched draws differ')
    G.write(a.out/('smoke_audit.json' if a.smoke else 'audit.json'),dict(verified=True,smoke=a.smoke,
        arms=result,matched_sample_sequence_sha256=digests[0],protocol_sha256=T.F.sha(a.out/'manifest.json'),
        scope='saved source models and real past-only PCM audit; no scene/C/device/holdout/car quality promotion'))
    print('UNIFIED_BROAD_CPU_AUDIT_PASS',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','train','audit'))
    for name in ('cache','initial','partition','monitor','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--mode',choices=MODES);p.add_argument('--smoke',action='store_true');p.add_argument('--resume',action='store_true')
    a=p.parse_args();{'prepare':prepare,'train':train,'audit':audit}[a.action](a)
