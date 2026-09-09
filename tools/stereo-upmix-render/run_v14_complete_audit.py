#!/usr/bin/env python3
"""Complete every full-song core with bounded scratch/retention and durable reports.

No training. No deletion of pre-existing audio. Only this process's temporary
directory is cleaned. Resume uses completed per-clip checkpoints; interrupted
incomplete runs stay on disk for inspection.
"""
import argparse
from collections import Counter
from contextlib import nullcontext
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import numpy as np
import soundfile as sf
from prepare_hssr_native_teacher import digest,valid_pair,POLICY
from teacher_audit_policy import coverage_report

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
BASE=ROOT/'build/reference-audio'

def atomic(path,record):
    temporary=path.with_suffix(path.suffix+'.tmp');temporary.write_text(json.dumps(record,indent=2,ensure_ascii=False)+'\n')
    temporary.replace(path)

def role_risks(clip):
    flags=[];s=clip['stems'];back=s['backing']['metrics'];lead=s['lead']['metrics']
    if back['rms_dbfs']-lead['rms_dbfs']>6:
        flags.append('backing-dominates-lead-role-unverified')
    if back['side_energy_fraction']<.03 and back['level_vs_mix_db']>-20:
        flags.append('substantial-narrow-vocal-residual-not-automatic-rear')
    if s['hh']['metrics']['level_vs_mix_db']<-30:flags.append('quiet-hh-do-not-normalize-into-positive-label')
    if s['piano']['metrics']['level_vs_mix_db']<-40:flags.append('quiet-piano-identity-unverified')
    for name,stem in s.items():
        if stem['metrics']['mono_retained_energy_fraction']<.1 and stem['metrics']['level_vs_mix_db']>-45:
            flags.append('mono-audit-loses-side-content:'+name)
        for w in stem['windows']:
            if not w['views']:continue
            raw=w['views']['shared_level']['clap']['cosine'];diag=w['views']['diagnostic_level']['clap']['cosine']
            if max(raw,key=raw.get)!=max(diag,key=diag.get) and name in ['hh','piano','backing']:
                flags.append('gain-sensitive-content-description:'+name)
            if name=='piano' and diag['processed_vocal']>diag['piano']+.1 and stem['metrics']['level_vs_mix_db']>-40:
                flags.append('piano-vs-processed-vocal-disagreement')
    if 'lead_from_mix' in s and s['lead_from_mix']['metrics']['rms_dbfs']>-55:
        delta=s['lead_from_mix']['metrics']['rms_dbfs']-lead['rms_dbfs']
        if abs(delta)>6:flags.append('karaoke-input-path-sensitive')
    return sorted(set(flags))

def window_states(clip):
    """Triage labels only; scores are deliberately NOT translated to probabilities."""
    result={}
    for name,stem in clip['stems'].items():
        windows=[]
        for w in stem['windows']:
            state='unresolved-content-and-role'
            if w.get('short_context_warning'):state='short-context-abstain'
            elif not w['views']:state='near-silence-abstain'
            elif stem['metrics']['level_vs_mix_db']<-40:state='low-level-residual-abstain'
            elif name in ['hh','kick','snare','guitar','piano','bass']:
                key={'hh':'hi_hat'}.get(name,name);label={'hh':'Hi-hat','kick':'Bass drum','snare':'Snare drum',
                    'guitar':'Guitar','piano':'Piano','bass':'Bass guitar'}[name]
                views=w['views'];support=[]
                for view in ['shared_level','diagnostic_level']:
                    scores=views[view]['clap']['cosine'];ranking=sorted(scores,key=scores.get,reverse=True)
                    # Uncalibrated heuristic for review priority, not a quality/admission threshold.
                    support.append(key in ranking[:2] and views[view]['ast']['focus'].get(label,0)>.1)
                if all(support):state='cross-model-supported-candidate-not-approved'
            windows.append(dict(offset_seconds=w['offset_seconds'],seconds=w['seconds'],state=state,training_approved=False))
        result[name]=windows
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=BASE/'v15-complete-audit')
    p.add_argument('--source-manifest',type=Path,default=BASE/'v15-full-float-source/manifest.json')
    p.add_argument('--teacher-root',type=Path,default=BASE/'v15-full-float-teacher')
    p.add_argument('--integrity-report',type=Path,default=BASE/'v15-full-float-source/integrity-audit.json')
    p.add_argument('--batch-size',type=int,default=5);p.add_argument('--reserve-gib',type=float,default=15)
    p.add_argument('--preserve-all-audio',action='store_true',
        help='Keep every new batch directory, including incomplete computations; hardlink all audition stems')
    p.add_argument('--resume',action='store_true');a=p.parse_args()
    if not 1<=a.batch_size<=5:raise ValueError('batch size must be 1..5')
    if a.output.exists() and not a.resume:raise ValueError('output exists; inspect and use --resume')
    a.output.mkdir(parents=True,exist_ok=True)
    lock=(a.output/'runner.lock').open('a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    manifest=a.source_manifest;data=json.loads(manifest.read_text());coverage=coverage_report(data)
    if not data.get('integer_clipping_avoided'):raise ValueError('full audit requires unclipped FLOAT source corpus')
    manifest_hash=digest(manifest);items=data['items'];byid={r['id']:r for r in items}
    teacher=a.teacher_root;models=ROOT/'build/hssr-teacher-models'
    for sub in ['clips','batches','scratch','retained']:(a.output/sub).mkdir(exist_ok=True)
    plan=a.output/'plan.json'
    if plan.exists():
        previous_plan=json.loads(plan.read_text())
        if previous_plan['manifest_sha256']!=manifest_hash:raise ValueError('manifest changed since plan')
        if previous_plan.get('preserve_all_audio') and not a.preserve_all_audio:
            raise ValueError('preservation policy cannot be downgraded on resume')
    groups={}
    for r in items:groups.setdefault(r['source_sha256'],[]).append(r)
    retained=set()
    for rows in groups.values():
        candidates=[r for r in rows if not r['silence_candidate']] or rows
        retained.add(min(candidates,key=lambda r:abs(r['core_start_seconds']+r['core_frames']/96000-r['source_duration_seconds']/2))['id'])
    atomic(plan,dict(manifest=str(manifest),manifest_sha256=manifest_hash,coverage=coverage,
        retained_ids=sorted(retained),policy='all-cores-both-karaoke-inputs-two-audio-models-review-only',
        preserve_all_audio=a.preserve_all_audio,
        temporary_audio_policy=('All new scratch and outputs retained, including incomplete batches; audition WAVs hardlinked'
            if a.preserve_all_audio else 'Only owned scratch is removed after metrics/provenance checkpoint; original/full-source/base audio retained'),
        training_approved=False))
    status=dict(state='starting',pid=os.getpid(),total_clips=len(items),total_songs=len(groups),training_started=False)
    def records():return [json.loads(p.read_text()) for p in (a.output/'clips').glob('*.json')]
    def publish(state,**extra):
        done=records();status.update(state=state,updated=time.time(),completed_clips=len(done),
            fully_audited_songs=sum(all(any(d['id']==r['id'] for d in done) for r in rows) for rows in groups.values()),**extra)
        atomic(a.output/'status.json',status)
    def command(script,args,batchdir):
        with (batchdir/'run.log').open('a') as log:
            log.write('\nRUN '+script+'\n');log.flush()
            env=dict(os.environ,PYTORCH_ENABLE_MPS_FALLBACK='1',HF_HUB_DISABLE_PROGRESS_BARS='1')
            subprocess.run([sys.executable,str(HERE/script),*map(str,args)],cwd=ROOT,env=env,
                stdout=log,stderr=subprocess.STDOUT,check=True)
    def completed_report(state):
        done=records();counts=Counter(flag for r in done for flag in r['risk_flags'])
        states=Counter(w['state'] for r in done for windows in r['window_states'].values() for w in windows)
        per_song=[dict(title=rows[0]['title'],partition=rows[0]['partition'],total=len(rows),
            audited=sum(r['id'] in {d['id'] for d in done} for r in rows),
            risky=sum(bool(d['risk_flags']) for d in done if d['source_sha256']==h)) for h,rows in groups.items()]
        report=dict(state=state,coverage=coverage,audited=len(done),per_song=per_song,risk_counts=dict(counts),
            preserve_all_new_audio=a.preserve_all_audio,
            historical_audited_without_retained_audio=sum(not r['retained_audio'] for r in done),
            window_triage_counts=dict(states),technical_audit_complete=len(done)==len(items),
            semantic_correctness_guaranteed=False,training_approved=False,
            conclusion='Automatic audit is not clean-reference validation. Review-only candidates and unresolved roles remain excluded from automatic training.')
        atomic(a.output/'dataset-report.json',report)
        lines=['# Full FLOAT dataset audit','',f'State: {state}. Completed {len(done)}/{len(items)} clips.',
            '', 'No automatic training approval. Semantic purity/role is not guaranteed by AST/CLAP.',
            '', '| Song | Partition | Audited / total | Clips with risk flags |','|---|---|---:|---:|']
        lines.extend(f"| {r['title']} | {r['partition']} | {r['audited']} / {r['total']} | {r['risky']} |" for r in per_song)
        lines.extend(['','## Risk counts','',*['- '+k+': '+str(v) for k,v in counts.most_common()]])
        (a.output/'dataset-report.md').write_text('\n'.join(lines)+'\n')
    started=time.monotonic();attempt=len(list((a.output/'batches').iterdir()))
    try:
        integrity=a.integrity_report
        while not integrity.exists():
            publish('waiting-source-integrity')
            if time.monotonic()-started>1200:raise TimeoutError('source integrity report absent')
            time.sleep(15)
        source_check=json.loads(integrity.read_text())
        if not source_check['technical_pass'] or not source_check.get('integer_clipping_avoided') or source_check['manifest_sha256']!=manifest_hash:raise ValueError('source integrity failed')
        while True:
            done={r['id'] for r in records()};pending=[r for r in items if r['id'] not in done]
            pending.sort(key=lambda row:row['id'] not in retained)
            if not pending:break
            free=shutil.disk_usage(a.output).free
            if free<(a.reserve_gib+3)*1024**3:raise OSError('disk reserve reached; no data removed')
            ready=[]
            for row in pending:
                prov=teacher/row['id']/'teacher-provenance.json'
                if not prov.exists():continue
                rec=json.loads(prov.read_text());identity=rec['identity']
                if identity['policy']!=POLICY or identity['source_sha256']!=row['stereo_sha256']:
                    raise ValueError('base identity mismatch')
                if not valid_pair(prov.parent,identity):raise ValueError('base pair validation failed')
                ready.append(row)
                if len(ready)==a.batch_size:break
            if not ready:
                publish('waiting-base-teacher');completed_report('waiting-base-teacher')
                if time.monotonic()-started>24*3600:raise TimeoutError('full pipeline deadline')
                time.sleep(30);continue
            batchdir=a.output/'batches'/f'{attempt:04d}';attempt+=1;batchdir.mkdir()
            ids=[r['id'] for r in ready];publish('specialist-separation',current_ids=ids)
            context=(nullcontext(tempfile.mkdtemp(prefix='preserved-v15-',dir=a.output/'scratch'))
                if a.preserve_all_audio else tempfile.TemporaryDirectory(prefix='owned-v14-',dir=a.output/'scratch'))
            with context as scratch_name:
                scratch=Path(scratch_name);special=scratch/'specialists';direct=scratch/'direct'
                command('prepare_specialist_teacher_audit.py',['--manifest',manifest,'--teacher-root',teacher,
                    '--output',special,'--model-dir',models,'--ids',*ids],batchdir)
                publish('karaoke-input-ablation',current_ids=ids)
                command('audit_karaoke_input_ablation.py',['--manifest',manifest,'--teacher-root',teacher,
                    '--specialists',special,'--output',direct,'--ids',*ids],batchdir)
                publish('independent-audio-content-audit',current_ids=ids)
                command('audit_teacher_audio_content.py',['--manifests',manifest,'--base-teacher',teacher,
                    '--full-teacher',teacher,'--specialists',special,'--direct-karaoke-root',direct,
                    '--output',batchdir/'content','--ids',*ids],batchdir)
                analysis=json.loads((batchdir/'content/results.json').read_text())
                if {c['id'] for c in analysis['clips']}!=set(ids):raise ValueError('incomplete audio audit')
                for clip in analysis['clips']:
                    ident=clip['id'];row=byid[ident]
                    for stem in clip['stems'].values():
                        if abs(sum(w['seconds'] for w in stem['windows'])-row['core_frames']/48000)>1e-6:
                            raise ValueError('audio audit did not cover the complete core')
                    s=json.loads((special/ident/'audit.json').read_text());d=json.loads((direct/ident/'audit.json').read_text())
                    retention=None
                    if a.preserve_all_audio or ident in retained:
                        dest=a.output/'retained'/ident
                        if not dest.exists():dest.mkdir()
                        combined={**s['stems'],**d['stems']};paths={}
                        for name,stem in combined.items():
                            target=dest/(name+'.wav')
                            if target.exists() and digest(target)!=stem['sha256']:raise ValueError('retained target differs; inspect before resume')
                            if not target.exists():
                                if a.preserve_all_audio:os.link(stem['path'],target)
                                else:shutil.copy2(stem['path'],target)
                            if digest(target)!=stem['sha256']:raise ValueError('retention copy mismatch')
                            paths[name]=dict(path=str(target.resolve()),sha256=stem['sha256'])
                        retention=dict(source_sha256=s['source_sha256'],stems=paths,qa=s['qa'])
                        atomic(dest/'retention.json',retention)
                    # Persist complete teacher provenance and model diagnostics before scratch cleanup.
                    record=dict(id=ident,title=row['title'],source_sha256=row['source_sha256'],
                        stereo_sha256=row['stereo_sha256'],partition=row['partition'],core_frames=row['core_frames'],
                        technical_pass=True,base_teacher_sha256=digest(teacher/ident/'teacher-provenance.json'),
                        specialist=s,direct_karaoke=d,analysis=clip,risk_flags=role_risks(clip),
                        window_states=window_states(clip),retained_audio=retention is not None,
                        unretained_audio=('All batch audio retained at original scratch paths' if a.preserve_all_audio
                            else 'Recompute from preserved source, model hashes and batch recipe; original scratch paths expire'),
                        training_approved=False,human_review='unreviewed')
                    atomic(a.output/'clips'/(ident+'.json'),record)
            publish('batch-complete',current_ids=[]);completed_report('in-progress')
        publish('complete-needs-semantic-review',current_ids=[]);completed_report('complete-needs-semantic-review')
        return 0
    except Exception as exc:
        publish('failed-needs-inspection',error=str(exc));completed_report('failed-needs-inspection');raise

if __name__=='__main__':raise SystemExit(main())
