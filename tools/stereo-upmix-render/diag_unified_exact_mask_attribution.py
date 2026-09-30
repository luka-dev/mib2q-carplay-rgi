#!/usr/bin/env python3
"""CPU privileged exact complex mask: validate transition attribution semantics.

Not a learned/cost-qualified separator. Retains the existing transition detector,
window selection, floor and gates; an exact target reconstruction does not imply
its frozen-mask components represent audible independent source waveforms.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import torch
import mask_net as MN
import sep_net as SN
import diag_transitions as DT
import unified_upmix_stream as S


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def clean(x):
    if isinstance(x,dict):return {k:clean(v) for k,v in x.items()}
    if isinstance(x,list):return [clean(v) for v in x]
    if isinstance(x,float) and not np.isfinite(x):return None
    return x

def write(path,x):path.write_text(json.dumps(clean(x),indent=2,allow_nan=False)+'\n')
def nse(y,r):return float((y-r).abs().double().square().sum()/r.abs().double().square().sum().clamp_min(1e-30))
def ratio(y,r):
    d=float(r.abs().double().square().sum())
    return float(y.abs().double().square().sum())/d if d>1e-9 else None

def oracle(roles):
    if roles.device.type!='cpu':raise ValueError('CPU only')
    refs=S.analyze(roles)
    mixture=refs.sum(0,keepdim=True)
    target=refs[:1]
    nonzero=mixture.abs()>0
    safe=torch.where(nonzero,mixture,torch.ones_like(mixture))
    mask=torch.where(nonzero,target/safe,torch.zeros_like(mixture))
    if not torch.isfinite(mask).all():raise ValueError('Nonfinite exact mask')
    n=roles.shape[-1]
    predicted=S.synthesize(mask*mixture,n)[0]
    attributed=S.synthesize(mask*refs,n)
    e=nse(predicted,roles[0]);closure=nse(attributed.sum(0),predicted)
    if e>1e-9 or closure>1e-9:raise ValueError('Exact target/attribution closure failed')
    return predicted,attributed,dict(target_waveform_nse=e,attribution_sum_nse=closure,
        zero_mixture_bins=int((~nonzero).sum()),mask_abs_max=float(mask.abs().max()),
        target_spectral_nse=nse(mask*mixture,target))


def frame_rows(predicted,attributed,roles):
    # Same centered reanalysis and warm-frame removal as the current evaluator.
    cut=SN.stft(torch,roles[0:1]).shape[1]-SN.stft(torch,roles[0:1,:,DT.WARM:]).shape[1]
    stft=lambda wave:SN.stft(torch,wave[None])[:,cut:]
    eligible=stft(roles[0]);vocal=stft(roles[1])
    ret=DT.per_frame(torch,stft(attributed[0]),eligible,24,DT.FLOOR_DB)
    leak=DT.per_frame(torch,stft(attributed[1]),vocal,24,DT.FLOOR_DB)
    actual=DT.per_frame(torch,stft(predicted),eligible,24,DT.FLOOR_DB)
    t=np.arange(24)*.5-4
    mean=lambda a,select:float(np.nanmean(a[select])) if np.isfinite(a[select]).any() else None
    before,after=mean(ret,t<0),mean(ret,t>=0)
    actual_before,actual_after=mean(actual,t<0),mean(actual,t>=0)
    return dict(ret_pre=before,ret_post=after,duck=after/before if before is not None and before>0 and after is not None else None,
        leak_pre=mean(leak,t<0),leak_0_2=mean(leak,(t>=0)&(t<2)),leak_steady=mean(leak,t>=4),
        actual_target_ret_pre=actual_before,actual_target_ret_post=actual_after,
        actual_target_duck=actual_after/actual_before if actual_before is not None and actual_before>0 and actual_after is not None else None)


def summarize(rows):
    result={}
    for kind in ('entry','exit','sustained'):
        selection=[r for r in rows if r['kind']==kind];data=dict(count=len(selection),song_count=len({r['song'] for r in selection}))
        for key in ('ret_pre','ret_post','duck','leak_pre','leak_0_2','leak_steady','actual_target_ret_pre','actual_target_ret_post','actual_target_duck'):
            values=[r[key] for r in selection if r[key] is not None and np.isfinite(r[key])]
            data[key]=dict(n=len(values),median=float(np.median(values)) if values else None,p10=float(np.percentile(values,10)) if values else None)
        result[kind]=data
    return result


def smoke(path):
    torch.set_num_threads(1)
    n=22*SN.SR;t=torch.arange(n,dtype=torch.float64)/SN.SR
    eligible=torch.stack([.03*torch.sin(2*torch.pi*440*t),.02*torch.sin(2*torch.pi*440*t+.3)])
    vocal=2*eligible*(t>=14)[None]
    roles=torch.stack([eligible,vocal,torch.zeros_like(eligible),torch.zeros_like(eligible)])
    pred,sent,parity=oracle(roles);row=frame_rows(pred,sent,roles)
    if not row['duck']<.6 or abs(row['actual_target_duck']-1)>1e-6:raise ValueError('Counterexample semantics failed')
    write(path,dict(verified=True,parity=parity,row=row,scope='synthetic collinear component counterexample; not real-data gate failure'))
    print('EXACT_MASK_ATTRIBUTION_SYNTHETIC',json.dumps(row),flush=True)


def run(a):
    torch.set_num_threads(1);torch.set_num_interop_threads(1)
    reference=json.loads(a.reference.read_bytes());expected={(r['song'],r['kind'],round(r['t']*SN.SR)) for r in reference['rows']}
    ids=list(dict.fromkeys(r['song'] for r in reference['rows']))
    if len(expected)!=265 or len(ids)!=40:raise ValueError('Original40/265 event coverage required')
    split=MN.load_split(str(a.cache))
    if set(ids)!=set(split['dev']):raise ValueError('Original DEV40 differs')
    sources={Path(m.__file__).name:sha(m.__file__) for m in (MN,SN,DT,S)}
    protocol=dict(version='exact-complex-mask-attribution-v1',script_sha256=sha(__file__),sources=sources,
        reference_sha256=sha(a.reference),split_sha256=sha(a.cache/'split.json'),ids=ids,
        method='float64 CPU past-only2048/512, exact complex E/X per-channel, zero only where X exactlyzero; no cap, gain, teacher or learned network',
        events='original40/265 source-attributed events; detector rechecked;22s actual mixture/10s warm/4pre8post; unchanged centered reanalysis/floor/gates',
        selection='all declared events, no cut/threshold selection',
        scope='privileged oracle diagnostic; no learnability, rear renderer, causality of label inference, device/int8, listening or product promotion')
    a.out.mkdir();write(a.out/'protocol.json',protocol)
    rows=[];seen=set()
    for index,song in enumerate(ids,1):
        file=a.cache/(song+'.npy');n=MN.npy_header(str(file))[0][2]
        vocal=MN.read_window(str(file),0,n,slots=[0,1]).astype(np.float32).sum(0)/32768
        events=[(kind,s) for kind,s in DT.events(vocal) if s-DT.PRE-DT.WARM>=0 and s+DT.POST<=n]
        del vocal
        for kind,onset in events:
            key=(song,kind,int(onset))
            if key not in expected or key in seen:raise ValueError('Detected event set differs')
            seen.add(key);begin=onset-DT.PRE-DT.WARM;length=DT.PRE+DT.WARM+DT.POST
            raw=MN.expand_slots(MN.read_window(str(file),begin,length))
            w=torch.from_numpy(raw.astype(np.float64)/32768)
            roles=torch.stack([w[6],w[0]+w[1],w[2:5].sum(0),w[5]])
            with torch.inference_mode():
                pred,sent,parity=oracle(roles);r=frame_rows(pred,sent,roles)
            r.update(song=song,kind=kind,t=onset/SN.SR,parity=parity,
                input_roles_sha256=hashlib.sha256(roles.numpy().tobytes()).hexdigest(),
                source_tx={source:ratio(sent[i],roles[i]) for i,source in enumerate(('eligible','vocal','drums','bass'))})
            rows.append(clean(r));write(a.out/'progress.json',dict(rows=rows,songs_complete=index-1))
        write(a.out/'progress.json',dict(rows=rows,songs_complete=index))
        print('EXACT_MASK_EVENTS',index,len(ids),song,len(events),flush=True)
    if seen!=expected:raise ValueError('Full event coverage differs')
    summary=summarize(rows);entry=summary['entry']['duck']
    gates=dict(entry_median=entry['median'] is not None and entry['median']>=.85,entry_p10=entry['p10'] is not None and entry['p10']>=.60)
    final=dict(complete=True,rows=rows,summaries=summary,original_entry_gates=gates,
        numerical_verified=True,max_target_waveform_nse=max(r['parity']['target_waveform_nse'] for r in rows),
        max_attribution_sum_nse=max(r['parity']['attribution_sum_nse'] for r in rows),protocol_sha256=sha(a.out/'protocol.json'),
        scope=protocol['scope'],limits='Exact masks can have very large cancelling component waveforms; attributed retention/leakage is not actual isolated audible target or protected content. Original thresholds remain unchanged.')
    write(a.out/'complete.json',final);print('EXACT_MASK_ATTRIBUTION_COMPLETE',json.dumps(summary['entry']),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--smoke-out',type=Path)
    p.add_argument('--cache',type=Path);p.add_argument('--reference',type=Path);p.add_argument('--out',type=Path)
    a=p.parse_args()
    if a.smoke_out:smoke(a.smoke_out)
    else:
        if any(x is None for x in (a.cache,a.reference,a.out)):p.error('All run paths required')
        run(a)
