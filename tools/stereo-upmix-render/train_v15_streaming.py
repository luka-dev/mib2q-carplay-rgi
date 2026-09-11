#!/usr/bin/env python3
"""Experimental audit-gated routing distillation, bounded memory and no feature cache.

Not semantic approval: frozen GRUs/classes/global/LFE heads, no auto deployment.
Only existing development partitions are used; no heldout selection or fitting.
"""
import argparse
from collections import Counter
import fcntl
import json
import math
from pathlib import Path
import random
import shutil
import subprocess
import time

import numpy as np
import soundfile as sf
import torch
from torch.nn import functional as F
from train_hssr import (BANDS, FFT_SIZE, HOP_SIZE, CONTROL_HOPS, SAMPLE_RATE,
    stft, make_features, import_model, export_model, freeze_except_routing_head)
from prepare_hssr_separator_supervision import selection_v11_targets
from prepare_hssr_native_teacher import digest, valid_pair, output_pair, POLICY
import hssr_runtime_training as runtime
from disk_budget_guard import DiskBudgetGuard
from run_v14_complete_audit import atomic, ROOT, BASE, HERE

VOICE_KEYS=('lead_singing','harmony_singing','wordless_vocal','processed_vocal','speech')
VOICE_LABELS=('Singing','Choir','Humming','Speech','Vocal music','A capella')
ROUTING_ROWS=np.array([b*8+j for b in range(BANDS) for j in (6,7)])


def normalized_inputs(features, mean, inv):
    values=np.clip((features-mean)*inv,-6,6).astype(np.float32)
    if not np.isfinite(values).all():raise ValueError('nonfinite input features')
    return torch.from_numpy(values)


def content_gate(window, nonvocal):
    """Uncalibrated conservative experiment gate, never a purity probability."""
    if window.get('short_context_warning') or window['seconds']<1:return False
    views=window['views']
    if set(views)!={'shared_level','diagnostic_level'}:return False
    for view in views.values():
        scores=view['clap']['cosine']
        voice=max(scores[k] for k in VOICE_KEYS)
        other=max(v for k,v in scores.items() if k not in VOICE_KEYS and k!='music_mix')
        ast=max(view['ast']['focus'].get(k,0) for k in VOICE_LABELS)
        if not all(math.isfinite(v) for v in (voice,other,ast)):return False
        if nonvocal:
            if not (ast<.1 and other>voice+.05):return False
        elif not (ast>.1 and voice>other+.05):return False
    return True


def admitted_windows(record, mode):
    if record['partition']=='heldout':return []
    name='instrumental' if mode=='mixture' else 'vocals'
    stem=record['analysis']['stems'][name]
    if stem['metrics']['mono_retained_energy_fraction']<.1:return []
    if mode=='mixture' and record['specialist']['qa']['vocal_teacher_disagreement_db']>-12:return []
    return [dict(offset_seconds=w['offset_seconds'],seconds=w['seconds'])
        for w in stem['windows'] if content_gate(w,mode=='mixture')]


def core_mask(row, select, windows):
    # Full analysis window AND next control interval must lie inside admitted core.
    starts=select*HOP_SIZE/SAMPLE_RATE-row['core_offset_frames']/SAMPLE_RATE
    ends=(select*HOP_SIZE+FFT_SIZE+CONTROL_HOPS*HOP_SIZE)/SAMPLE_RATE-row['core_offset_frames']/SAMPLE_RATE
    mask=np.zeros(len(select),dtype=bool)
    for w in windows:
        mask|=(starts>=w['offset_seconds'])&(ends<=w['offset_seconds']+w['seconds'])
    mask[:64]=False  # warmup at each contextual clip boundary
    mask[-1:]=False  # no next-control target outside the clip
    return mask


def routing_loss(logits, targets, mask):
    if not bool(mask.any()):return None
    pred=logits[:,ROUTING_ROWS].reshape(-1,BANDS,2)[mask]
    target=targets[mask]
    rear=runtime.sigmoid(pred[...,0]).clamp(1e-6,1-1e-6)
    pan=runtime.tanh(pred[...,1])
    rear_loss=F.binary_cross_entropy(rear,target[...,0],reduction='none')
    rear_loss=(rear_loss*(1+4*target[...,0])).mean()
    pan_loss=((pan-target[...,1]).square()*target[...,0]).mean()
    return rear_loss+.5*pan_loss


def assert_frozen(model, initial):
    allowed=torch.zeros(264,dtype=torch.bool);allowed[ROUTING_ROWS]=True
    for k,v in model.state_dict().items():
        current=v.detach().cpu()
        if not torch.equal(current[~allowed] if k.startswith('head.') else current,
                           initial[k][~allowed] if k.startswith('head.') else initial[k]):
            raise ValueError('frozen parameters changed: '+k)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--epochs',type=int,default=4)
    p.add_argument('--resume',action='store_true')
    p.add_argument('--smoke',action='store_true',help='bounded one train + one validation case; not a full run')
    a=p.parse_args()
    if a.epochs<1:raise ValueError('epochs must be positive')
    out=a.output.resolve()
    if out.exists() and not a.resume:raise ValueError('use a fresh output directory')
    out.mkdir(parents=True,exist_ok=True)
    lock=(out/'training.lock').open('a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    torch.set_num_threads(2);torch.manual_seed(15011)
    source=BASE/'v15-full-float-source/manifest.json';audit=BASE/'v15-complete-audit'
    teacher=BASE/'v15-full-float-teacher';baseline=ROOT/'build/hssr-selection-v11/hssr_model.bin'
    rows=json.loads(source.read_text())['items'];byid={r['id']:r for r in rows}
    report=json.loads((audit/'dataset-report.json').read_text())
    if not report['technical_audit_complete'] or report['audited']!=len(rows):raise ValueError('incomplete audit')
    bindings=dict(source_sha256=digest(source),audit_sha256=digest(audit/'dataset-report.json'),baseline_sha256=digest(baseline))
    sources={}
    for r in rows:sources.setdefault(r['source_sha256'],set()).add(r['partition'])
    if any(len(v)!=1 for v in sources.values()):raise ValueError('source partition leakage')
    state=dict(training_kind='experimental-routing-only',semantic_labels_approved=False,
        installed=False,heldout_used_for_training=False,pid=__import__('os').getpid())
    def status(phase,**extra):
        state.update(phase=phase,updated=time.time(),**extra);atomic(out/'status.json',state)
    guard=DiskBudgetGuard()
    def space():
        while True:
            free=shutil.disk_usage(out).free/2**30;guard.observe(free)
            atomic(out/'disk-status.json',dict(free_gib=free,paused=guard.paused,pause_gib=22,resume_gib=27,automatic_deletion=False))
            if not guard.paused:return
            status('paused-low-disk',free_gib=free)
            time.sleep(5)
    try:
        status('reviewing-complete-audit');space()
        cases=[];decisions=[]
        for row in rows:
            r=json.loads((audit/'clips'/(row['id']+'.json')).read_text())
            if (not r['technical_pass'] or r['stereo_sha256']!=row['stereo_sha256']
                or r['source_sha256']!=row['source_sha256'] or r['partition']!=row['partition']):raise ValueError('audit identity mismatch')
            modes={mode:admitted_windows(r,mode) for mode in ('mixture','vocal-negative')}
            decisions.append(dict(id=row['id'],partition=row['partition'],windows=modes,
                ground_truth_approved=False,excluded_reason=('heldout' if row['partition']=='heldout'
                    else 'no-supported-windows' if not any(modes.values()) else None)))
            for mode,windows in modes.items():
                if windows:cases.append(dict(id=row['id'],partition=row['partition'],mode=mode,windows=windows,
                    audit_sha256=digest(audit/'clips'/(row['id']+'.json'))))
        train=[c for c in cases if c['partition']=='train'];val=[c for c in cases if c['partition']=='validation']
        if not train or not val or not any(c['mode']=='mixture' for c in train+val):raise ValueError('insufficient candidate coverage')
        plan=dict(bindings=bindings,epochs=a.epochs,smoke=a.smoke,
            policy='experimental-two-stem-routing-gated-v1',semantic_training_approved=False,
            gates=dict(ast_vocal=.1,clap_margin=.05,max_vocal_disagreement_db=-12,both_gain_views=True),
            frozen='both GRUs, all semantic/global/LFE rows',input_gains_db=[0,-6,-12],
            temporal_policy='continuous frozen runtime GRU per contextual clip, core-only supervision, 64-control warmup, one-control target delay',
            loss_policy='routing BCE + pan; vocal-negative optimizer loss scaled0.25; validation equal mean of mixture and negative families',
            cases=cases,decisions=decisions,counts=dict(Counter(c['partition']+':'+c['mode'] for c in cases)),
            limitations=['AST/CLAP gates are uncalibrated, not semantic purity approval.',
                'No backing/HH/piano class labels used. No global head or bass-head updates.',
                'Local experiment only; export/C/listening acceptance still required.'])
        plan_path=out/'admission-plan.json'
        if a.resume and json.loads(plan_path.read_text())!=plan:raise ValueError('resume plan changed')
        if not a.resume:atomic(plan_path,plan)
        model,mean,inv=import_model(baseline);model.runtime_aware=True
        initial={k:v.detach().clone() for k,v in model.state_dict().items()}
        freeze_except_routing_head(model)
        opt=torch.optim.AdamW((p for p in model.parameters() if p.requires_grad),lr=1e-5,weight_decay=0)
        verified=set()
        def arrays(case,gain_db):
            row=byid[case['id']];directory=teacher/row['id']
            if row['id'] not in verified:
                prov=json.loads((directory/'teacher-provenance.json').read_text());identity=prov['identity']
                if (identity['policy']!=POLICY or identity['source_sha256']!=digest(Path(row['stereo']))
                    or identity['source_sha256']!=row['stereo_sha256'] or not valid_pair(directory,identity)):
                    raise ValueError('base teacher provenance mismatch')
                verified.add(row['id'])
            mix,rate=sf.read(row['stereo'],dtype='float32',always_2d=True)
            v=sf.read(output_pair(directory)[0],dtype='float32',always_2d=True)[0]
            ins=sf.read(output_pair(directory)[1],dtype='float32',always_2d=True)[0]
            if rate!=SAMPLE_RATE or mix.shape!=v.shape or v.shape!=ins.shape:raise ValueError('audio alignment')
            common=min(1.,.98/max(float(np.abs(mix).max()),float(np.abs(v).max()),1e-8))
            x=mix if case['mode']=='mixture' else v
            x=x*(common*10**(gain_db/20))
            # Model sees the actual integer PCM domain; FLOAT teachers themselves remain unclipped.
            x=np.clip(np.where(x>=0,np.floor(x*32768+.5),np.ceil(x*32768-.5)),-32768,32767).astype('f4')/32768
            spec=stft(x);select=np.arange(CONTROL_HOPS-1,len(spec),CONTROL_HOPS)
            features=make_features(x,spec)[select]
            if case['mode']=='mixture':
                targets=selection_v11_targets(stft(v),stft(ins))[select,:BANDS*8].reshape(-1,BANDS,8)[...,[6,7]]
            else:targets=np.zeros((len(select),BANDS,2),dtype='f4')
            mask=core_mask(row,select,case['windows'])
            return features,targets,mask
        def run_case(case,gain_db,training):
            space();features,targets,mask=arrays(case,gain_db)
            if not mask.any():return None
            inputs=normalized_inputs(features,mean,inv)
            target=torch.from_numpy(targets.astype(np.float32));keep=torch.from_numpy(mask)
            recurrent=None;total=0.;count=0
            for at in range(0,len(inputs),256):
                space()
                end=min(at+256,len(inputs))
                with torch.no_grad():
                    embeddings,recurrent=runtime.forward(model,inputs[None,at:end],recurrent,return_embeddings=True)
                # Only the head graph is retained; recurrent weights/state are frozen.
                prediction=F.linear(embeddings[0],runtime.weight(model.head.weight),model.head.bias)
                n=min(end,len(target)-1)-at
                if n<=0:continue
                active=keep[at:at+n]
                loss=routing_loss(prediction[:n],target[at+1:at+1+n],active)
                if loss is None:continue
                if not torch.isfinite(loss):raise ValueError('nonfinite loss')
                if training:
                    opt.zero_grad(set_to_none=True)
                    (loss*(.25 if case['mode']=='vocal-negative' else 1)).backward()
                    torch.nn.utils.clip_grad_norm_(model.head.parameters(),1.)
                    opt.step()
                weight=int(active.sum());total+=float(loss.detach())*weight;count+=weight
            return total/count if count else None
        def validation(epoch):
            totals={'mixture':[],'vocal-negative':[]}
            selected=val[:1] if a.smoke else val
            for i,c in enumerate(selected):
                status('validation',epoch=epoch,case=i+1,total_cases=len(selected),current_id=c['id'])
                with torch.no_grad():value=run_case(c,0,False)
                if value is not None:totals[c['mode']].append(value)
            means={k:float(np.mean(v)) for k,v in totals.items() if v}
            if not means:raise ValueError('no eligible validation frames')
            return dict(loss=float(np.mean(list(means.values()))),families=means)
        checkpoint=out/'last.pt';history=[];start_epoch=1;start_case=0
        if a.resume:
            saved=torch.load(checkpoint,map_location='cpu',weights_only=False)
            if saved['bindings']!=bindings:raise ValueError('checkpoint bindings changed')
            model.load_state_dict(saved['model']);opt.load_state_dict(saved['optimizer'])
            best=saved['best'];best_state=saved['best_state'];history=saved['history']
            baseline_score=saved['baseline_score'];start_epoch=saved['epoch'];start_case=saved['next_case']
        else:
            baseline_score=validation(0);best=baseline_score['loss'];best_state={k:v.detach().clone() for k,v in model.state_dict().items()}
        def save(epoch,next_case):
            space();assert_frozen(model,initial)
            temporary=checkpoint.with_suffix('.pt.tmp')
            torch.save(dict(bindings=bindings,model=model.state_dict(),optimizer=opt.state_dict(),best=best,
                best_state=best_state,history=history,baseline_score=baseline_score,epoch=epoch,next_case=next_case),temporary)
            temporary.replace(checkpoint)
        if not a.resume:save(1,0)
        for epoch in range(start_epoch,a.epochs+1):
            order=list(train);random.Random(15011+epoch).shuffle(order)
            if a.smoke:order=order[:1]
            for i in range(start_case,len(order)):
                c=order[i];gain=(0,-6,-12)[(epoch-1+i)%3]
                status('training',epoch=epoch,epochs=a.epochs,case=i+1,total_cases=len(order),current_id=c['id'],mode=c['mode'])
                value=run_case(c,gain,True)
                if i%10==0 or i+1==len(order):
                    save(epoch,i+1);print(json.dumps(dict(epoch=epoch,case=i+1,cases=len(order),loss=value)),flush=True)
            start_case=0;score=validation(epoch);history.append(dict(epoch=epoch,validation=score))
            if score['loss']<best:
                best=score['loss'];best_state={k:v.detach().clone() for k,v in model.state_dict().items()}
            save(epoch+1,0)
            atomic(out/'history.json',dict(baseline=baseline_score,history=history,best_loss=best))
        model.load_state_dict(best_state);assert_frozen(model,initial)
        exported=export_model(model,mean,inv,out/'candidate.bin')
        atomic(out/'result.json',dict(bindings=bindings,export=exported,baseline=baseline_score,history=history,
            improved_validation=best<baseline_score['loss'],smoke=a.smoke,semantic_training_approved=False,
            installed=False,acceptance='pending-exported-C-and-mixture-attribution-and-listening'))
        status('smoke-complete' if a.smoke else 'training-complete-needs-exported-C-and-listening',best_loss=best)
    except Exception as exc:
        status('failed-needs-inspection',error=str(exc));raise


if __name__=='__main__':main()
