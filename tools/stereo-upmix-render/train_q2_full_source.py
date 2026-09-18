"""Explicit prepare/run/resume for bounded full-song Q2 TRAIN research.

One architecture/guidance arm per immutable owner. No DEV optimizer, oracle on
DEV, TEST reader, automatic launch, native export or quality approval. Only one
bounded PCM context and one small BPTT graph are resident at a time.
"""
from contextlib import contextmanager
from dataclasses import asdict
import argparse
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import resource
import shutil
import sys
import tempfile
import time

import torch

import q2_source_data as data
from q2_local_guidance_model import Q2Config,Q2LocalGuidanceModel
from q2_stream_training import (StreamChunk,plan_index,prepare_chunk,advance,
                               cursor_to_payload,cursor_from_payload)
from q2_training_core import objective,OBJECTIVE
from release_acceptance import load_freeze

SCHEMA='q2-full-source-train-v1'
CALIBRATION='q2-full-source-calibration-v1'
MAX_JOBS=100_000
CODE=('train_q2_full_source.py','q2_stream_training.py',
      'q2_training_core.py','q2_local_guidance_model.py','q2_source_data.py',
      'r1_refiner_audio.py','r1_source_refiner.py','release_acceptance.py')


def require(value,message):
    if not value:raise ValueError(message)


def cpu_copy(value):
    if isinstance(value,torch.Tensor):return value.detach().cpu().clone()
    if isinstance(value,dict):return {k:cpu_copy(v) for k,v in value.items()}
    if isinstance(value,list):return [cpu_copy(v) for v in value]
    if isinstance(value,tuple):return tuple(cpu_copy(v) for v in value)
    return value


def create_model(config,architecture,prior):
    """Explicit fixed factory; no dependency on the evolving short-pilot runner."""
    if architecture=='film_zero':return Q2LocalGuidanceModel(config)
    require(architecture in ('film_control','direct_power'),'Unknown architecture')
    from q2_power_guidance_model import Q2PowerGuidanceModel
    return Q2PowerGuidanceModel(config,prior=tuple(prior),carrier_mode=architecture,prior_partition='TRAIN')


def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def encoded(value):return (json.dumps(value,sort_keys=True,indent=2,allow_nan=False)+'\n').encode()


def fresh_json(path,value):
    with Path(path).open('xb') as stream:stream.write(encoded(value))


def publish(path,body):
    """Atomic no-overwrite publication; interrupted .partial files are retained."""
    path=Path(path)
    descriptor,name=tempfile.mkstemp(prefix=path.name+'.partial-',dir=path.parent)
    with os.fdopen(descriptor,'wb') as stream:
        stream.write(body);stream.flush();os.fsync(stream.fileno())
    # Hard-link publication is atomic and refuses an existing immutable name.
    os.link(name,path)
    os.unlink(name)


def atomic_json(path,value):
    path=Path(path);temporary=path.with_suffix(path.suffix+'.tmp')
    with temporary.open('wb') as stream:
        stream.write(encoded(value));stream.flush();os.fsync(stream.fileno())
    os.replace(temporary,path)


def environment():
    import numpy as np
    import scipy
    import soundfile as sf
    from scipy.signal import _signaltools,_upfirdn,_upfirdn_apply
    return dict(torch=str(torch.__version__),python=sys.version,platform=platform.platform(),
                cpu_threads=torch.get_num_threads(),interop_threads=torch.get_num_interop_threads(),
                numpy=np.__version__,scipy=scipy.__version__,soundfile=sf.__version__,
                libsndfile=sf.__libsndfile_version__,
                resample_dependencies={Path(m.__file__).name:digest(m.__file__)
                    for m in (_signaltools,_upfirdn,_upfirdn_apply)},
                threads={k:os.environ.get(k) for k in
                    ('OPENBLAS_NUM_THREADS','OMP_NUM_THREADS','MKL_NUM_THREADS')})


@contextmanager
def writer(owner):
    raw=Path(owner).absolute();require(not raw.is_symlink(),'No symlink owner')
    owner=raw.resolve();require(owner.parent.is_dir(),'Existing explicit parent required')
    lock=owner.parent/('.q2-full-source-'+hashlib.sha256(str(owner).encode()).hexdigest()[:24]+'.lock')
    with lock.open('a+b') as stream:
        try:fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError as exc:raise ValueError('Another writer owns this study') from exc
        try:yield owner
        finally:fcntl.flock(stream,fcntl.LOCK_UN)


def _config(value):
    value=dict(value);value['classes']=tuple(value['classes']);return Q2Config(**value)


def schedule(stream_plan,epochs,chunk_frames,seed):
    estimated=epochs*sum(math.ceil((r['frame_stop']-r['frame_start'])/chunk_frames)
                          for song in stream_plan['songs'] for r in song['chunks'])
    require(estimated<=MAX_JOBS and estimated*400+len(encoded(stream_plan))<64*1024**2,
            'Schedule metadata allocation cap; split into separately declared studies')
    jobs=[]
    for epoch in range(epochs):
        songs=list(range(len(stream_plan['songs'])));random.Random(seed+epoch).shuffle(songs)
        for song in songs:
            for record in stream_plan['songs'][song]['chunks']:
                size=record['frame_stop']-record['frame_start']
                for start in range(0,size,chunk_frames):
                    stop=min(size,start+chunk_frames);a=record['frame_start']+start;b=record['frame_start']+stop
                    valid=max(0,min(b,record['song_frames']//256)-max(a,3))
                    jobs.append(dict(epoch=epoch,song=song,chunk=record['index'],start=start,stop=stop,
                                     global_frame_start=a,global_frame_stop=b,loss_frames=valid))
    return jobs


def load_calibration(path,expected_sha256,index_sha256,stream_plan,config):
    path=Path(path);require(path.is_file() and not path.is_symlink() and path.stat().st_size<=16*1024**2,'Bounded calibration file required')
    require(digest(path)==expected_sha256,'Calibration body SHA differs')
    value=json.loads(path.read_text());songs={v['song_id']:v for v in stream_plan['songs']}
    require(value.get('schema')==CALIBRATION and value.get('complete') is True and value.get('partition')=='TRAIN'
            and value.get('index_sha256')==index_sha256 and tuple(value.get('classes',()))==config.classes
            and value.get('target_rate')==config.sample_rate
            and set(value.get('song_intervals',{}))==set(songs),'Full admitted TRAIN calibration binding required')
    require(all(value['song_intervals'][sid]==[[0,row['song_frames']]] for sid,row in songs.items()),
            'Calibration must cover all full TRAIN timelines once, not four-pilot cores')
    require(value.get('definition')=='pooled full TRAIN PCM source squared sums, each sample once, floor probability1e-6 then renormalize'
            and value.get('test_pcm_accessed') is False,'Explicit TRAIN-only calibration definition required')
    sums=torch.tensor(value['source_power_sums'],dtype=torch.float64)
    require(sums.shape==(len(config.classes),) and bool(torch.isfinite(sums).all())
            and bool((sums>=0).all()) and float(sums.sum())>0,'Finite positive TRAIN calibration powers required')
    prior=(sums/sums.sum()).clamp_min(1e-6);prior=prior/prior.sum()
    claimed=torch.tensor(value['carrier_prior'],dtype=torch.float64)
    require(claimed.shape==prior.shape and bool(torch.isfinite(claimed).all())
            and bool(torch.allclose(claimed,prior,rtol=1e-12,atol=1e-15)), 'Calibration prior does not match bound powers')
    for name in ('source_proof_sha256','calibration_code_sha256','cache_log_sha256'):
        body=value.get(name);require(isinstance(body,str) and len(body)==64 and all(v in '0123456789abcdef' for v in body),
                                     'Calibration provenance SHA required: '+name)
    for relative,body in calibration_artifacts(value).items():
        item=Path(relative)
        require(not item.is_absolute() and '..' not in item.parts,'Calibration proof path outside owner')
        proof=path.parent/item
        require(proof.resolve().is_relative_to(path.parent.resolve()) and proof.is_file() and not proof.is_symlink()
                and proof.stat().st_size<=64*1024**2
                and digest(proof)==body,'Calibration proof/code body differs')
    return value


def calibration_artifacts(value):
    return {'source-proofs.json':value['source_proof_sha256'],
            'cache-log.json':value['cache_log_sha256'],
            value['calibration_code_path']:value['calibration_code_sha256']}


def prepare(index_path,owner,*,index_sha256,freeze_owner,architecture='film_zero',arm='predicted',
            calibration_path=None,calibration_sha256=None,epochs=1,chunk_frames=128,seed=91621,
            learning_rate=.001,profile='reference',sample_rate=48000,device='cpu',
            core_hops=None,past_hops=None,checkpoint_every=64,max_updates=None,
            max_seconds=1800.,min_free_gib=50.,max_memory_gib=4.,max_output_mib=4096.):
    """Freeze metadata/code/resources only. Does not construct a model/optimizer/read PCM."""
    with writer(owner) as owner:
        require(not owner.exists(),'Fresh owner required')
        require(architecture in ('film_zero','film_control','direct_power') and arm in ('predicted','disabled','oracle_train'), 'Explicit architecture/guidance arm required')
        require(device in ('cpu','mps') and profile in ('reference','student'),'Explicit backend/profile required')
        require(type(epochs)is int and 1<=epochs<=100 and type(chunk_frames)is int and 1<=chunk_frames<=128
                and type(seed)is int and 0<=seed<2**32 and type(checkpoint_every)is int and checkpoint_every>0,'Bounded schedule required')
        for name,value in dict(learning_rate=learning_rate,max_seconds=max_seconds,max_memory_gib=max_memory_gib,max_output_mib=max_output_mib).items():
            require(type(value)in(int,float) and math.isfinite(value) and value>0,'Finite positive '+name)
        require(type(min_free_gib)in(int,float) and math.isfinite(min_free_gib) and min_free_gib>=0,'Finite disk reserve required')
        require(shutil.disk_usage(owner.parent).free>=min_free_gib*1024**3,'Disk reserve before prepare')
        index_path=Path(index_path).resolve();index=data.load_index(index_path,expected_sha256=index_sha256)
        _,freeze=load_freeze(freeze_owner)
        width,hidden=(24,48) if profile=='reference' else (12,24)
        config=Q2Config(sample_rate=sample_rate,width=width,band_hidden=hidden,classes=tuple(index['classes']))
        stream_plan=plan_index(index,partition='TRAIN',config=config,core_hops=core_hops,past_hops=past_hops)
        require(stream_plan['song_count']>0,'No admitted TRAIN songs')
        jobs=schedule(stream_plan,epochs,chunk_frames,seed)
        available_updates=sum(v['loss_frames']>0 for v in jobs)
        max_updates=available_updates if max_updates is None else max_updates
        require(type(max_updates)is int and 1<=max_updates<=available_updates,'Fixed update ceiling must fit schedule')
        calibration=None
        if calibration_path is not None:
            calibration=load_calibration(calibration_path,calibration_sha256,index_sha256,stream_plan,config)
        require(architecture=='film_zero' or calibration is not None,'Power architectures require full TRAIN calibration, not a pilot prior')
        names=CODE+(('q2_power_guidance_model.py',) if architecture!='film_zero' else ())
        source={name:digest(Path(__file__).with_name(name)) for name in names}
        plan=dict(schema=SCHEMA,config=asdict(config),index_path=str(index_path),index_sha256=index_sha256,
            freeze_owner=str(Path(freeze_owner).resolve()),protocol_sha256=freeze['protocol_sha256'],
            metric_specification_sha256=freeze['metric_specification_sha256'],source_sha256=source,
            architecture=architecture,arm=arm,carrier_prior=None if calibration is None else calibration['carrier_prior'],
            calibration_sha256=calibration_sha256 if calibration is not None else None,
            epochs=epochs,chunk_frames=chunk_frames,seed=seed,profile=profile,device=device,
            execution=environment(),objective=OBJECTIVE,normalization='owned_complete_window_chunk',
            optimizer=dict(name='Adam',lr=learning_rate,betas=[.9,.999],eps=1e-8,weight_decay=0.,
                           amsgrad=False,foreach=False,fused=False,clip_norm=5.,clip_norm_type=2.,
                           clip_foreach=False,clip_error_if_nonfinite=True),
            stream_plan=stream_plan,schedule=jobs,available_updates=available_updates,max_updates=max_updates,
            checkpoint_every=checkpoint_every,
            limits=dict(max_cumulative_seconds=max_seconds,min_free_gib=min_free_gib,
                        max_memory_gib=max_memory_gib,max_output_mib=max_output_mib),
            recurrent_history='sequential TBPTT: retain detached pre-update state under new weights; not recomputed full history',
            inference='original only; oracle_train is explicit TRAIN-only diagnostic',
            dev_optimizer_updates=0,test_pcm_accessed=False,quality_approved=False,car_approved=False)
        require(len(encoded(plan))<64*1024**2,'Bounded plan size required')
        projected=len(encoded(plan))+index_path.stat().st_size+16384
        projected+=sum(Path(__file__).with_name(name).stat().st_size for name in source)
        if calibration is not None:
            projected+=Path(calibration_path).stat().st_size
            projected+=sum((Path(calibration_path).parent/p).stat().st_size for p in calibration_artifacts(calibration))
        require(projected<=max_output_mib*1024**2,'Projected prepare output owner cap')
        require(shutil.disk_usage(owner.parent).free-projected>=min_free_gib*1024**3,'Projected prepare disk reserve')
        owner.mkdir(mode=0o700);(owner/'source').mkdir();(owner/'checkpoints').mkdir();(owner/'history').mkdir()
        for name,body in source.items():
            shutil.copyfile(Path(__file__).with_name(name),owner/'source'/name)
            require(digest(owner/'source'/name)==body,'Code snapshot body differs')
        shutil.copyfile(index_path,owner/'index.json')
        require(digest(owner/'index.json')==index_sha256,'Index snapshot body differs')
        if calibration is not None:
            shutil.copyfile(calibration_path,owner/'calibration.json')
            require(digest(owner/'calibration.json')==calibration_sha256,'Calibration snapshot differs')
            for relative,body in calibration_artifacts(calibration).items():
                target=owner/relative;target.parent.mkdir(parents=True,exist_ok=True)
                require(not target.exists(),'Calibration snapshot name collision')
                shutil.copyfile(Path(calibration_path).parent/relative,target)
                require(digest(target)==body,'Calibration proof snapshot differs')
        fresh_json(owner/'plan.json',plan)
        fresh_json(owner/'plan-seal.json',dict(schema=SCHEMA,plan_sha256=digest(owner/'plan.json')))
        return dict(state='prepared',plan_sha256=digest(owner/'plan.json'),songs=stream_plan['song_count'],
                    jobs=len(jobs),max_updates=max_updates,optimizer_updates=0,pcm_decoded=False)


class Budget:
    def __init__(self,owner,plan,plan_sha):
        self.owner,self.plan,self.plan_sha=owner,plan,plan_sha
        self.started=time.monotonic();self.base=0.
        self.rescan()
        path=owner/'work-budget.json'
        if path.exists():
            entry=json.loads(path.read_text());require(entry['plan_sha256']==plan_sha,'Budget plan binding differs')
            self.base=entry['elapsed_seconds']
            require(type(self.base)in(int,float) and math.isfinite(self.base) and self.base>=0,'Invalid persisted budget')
    def elapsed(self):return self.base+time.monotonic()-self.started
    def rescan(self):
        self.sizes={str(p):p.stat().st_size for p in self.owner.rglob('*') if p.is_file()}
        self.bytes=sum(self.sizes.values())
    def changed(self,path):
        key=str(path);size=Path(path).stat().st_size
        self.bytes+=size-self.sizes.get(key,0);self.sizes[key]=size
    def record(self):
        value=self.elapsed();path=self.owner/'work-budget.json'
        atomic_json(path,dict(plan_sha256=self.plan_sha,elapsed_seconds=value));self.changed(path);return value
    def reason(self):
        elapsed=self.record();limits=self.plan['limits']
        if (self.owner/'STOP').exists():return 'STOP'
        if elapsed>=limits['max_cumulative_seconds']:return 'cumulative_walltime'
        if shutil.disk_usage(self.owner).free<limits['min_free_gib']*1024**3:return 'disk_reserve'
        rss=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform=='darwin' else 1024)
        gpu=torch.mps.driver_allocated_memory() if self.plan['device']=='mps' else 0
        if rss+gpu>limits['max_memory_gib']*1024**3:return 'conservative_RSS_plus_MPS_memory'
        if self.bytes>limits['max_output_mib']*1024**2:return 'output_owner_cap'
        return None
    def write_room(self,additional):
        require(self.bytes+additional<=self.plan['limits']['max_output_mib']*1024**2,'Output owner cap')
        require(shutil.disk_usage(self.owner).free-additional>=self.plan['limits']['min_free_gib']*1024**3,'Projected output disk reserve')


def bound(owner):
    seal=json.loads((owner/'plan-seal.json').read_text())
    require(seal['schema']==SCHEMA and seal['plan_sha256']==digest(owner/'plan.json'),'Frozen plan body differs')
    plan=json.loads((owner/'plan.json').read_text());require(plan['schema']==SCHEMA,'Unknown full training plan')
    require(plan['execution']==environment(),'Execution context changed; no silent backend substitute')
    for name,body in plan['source_sha256'].items():
        require(Path(name).name==name and digest(Path(__file__).with_name(name))==body
                and digest(owner/'source'/name)==body,'Frozen code/source copy changed')
    require(digest(owner/'index.json')==plan['index_sha256'],'Index snapshot changed')
    data.load_index(plan['index_path'],expected_sha256=plan['index_sha256'])
    _,freeze=load_freeze(plan['freeze_owner'])
    require(freeze['protocol_sha256']==plan['protocol_sha256'] and freeze['metric_specification_sha256']==plan['metric_specification_sha256'],'Acceptance freeze changed')
    if plan['calibration_sha256'] is not None:
        load_calibration(owner/'calibration.json',plan['calibration_sha256'],plan['index_sha256'],plan['stream_plan'],_config(plan['config']))
    require(schedule(plan['stream_plan'],plan['epochs'],plan['chunk_frames'],plan['seed'])==plan['schedule'],'Schedule differs from plan')
    return plan,digest(owner/'plan.json')


def checkpoint_read(owner,pointer,plan_sha):
    name=pointer.get('path');require(isinstance(name,str) and name.startswith('checkpoint-') and name.endswith('.pt')
        and name[11:-3].isdigit() and Path(name).name==name,'Invalid checkpoint pointer path')
    path=owner/'checkpoints'/name
    require(path.is_file() and not path.is_symlink() and path.stat().st_size<=64*1024**2
            and digest(path)==pointer['sha256'],'Checkpoint body differs')
    value=torch.load(path,map_location='cpu',weights_only=True)
    require(value['schema']==SCHEMA and value['plan_sha256']==plan_sha
            and name==f"checkpoint-{value['next_job']:08d}.pt",'Checkpoint cursor/plan differs')
    return value


def recover_pointer(owner,plan_sha):
    """Recover a completed atomically published checkpoint after pointer crash."""
    latest=owner/'latest.json';pointer=json.loads(latest.read_text()) if latest.exists() else None
    current=-1
    if pointer is not None:current=checkpoint_read(owner,pointer,plan_sha)['next_job']
    paths=sorted((owner/'checkpoints').glob('checkpoint-????????.pt'),reverse=True)
    if paths:
        path=paths[0];candidate=dict(path=path.name,sha256=digest(path))
        value=checkpoint_read(owner,candidate,plan_sha)
        if value['next_job']>current:
            _history(owner,value['next_job'],value['history_head_sha256'])
            candidate['next_job']=value['next_job'];atomic_json(latest,candidate);pointer=candidate
    return pointer


def _history(owner,count,head):
    previous=None;values=[]
    for index in range(count):
        path=owner/'history'/f'job-{index:08d}.json'
        require(path.is_file() and not path.is_symlink(),'Missing committed history')
        row=json.loads(path.read_text());require(row['job_index']==index and row['previous_sha256']==previous,'History chain differs')
        previous=digest(path);values.append(row)
    require(previous==head,'Checkpoint history head differs')
    return values


def _source_lookup(reader,stream_plan):
    return {song['song_id']+'/'+track['id']:track for song in stream_plan['songs']
            for track in reader.rows[song['song_id']]['tracks']}


def run(owner,*,resume=False,stop_after_jobs=None):
    """Explicit bounded TRAIN optimizer execution; never called by prepare."""
    with writer(owner) as owner:
        require(owner.is_dir(),'Prepared owner required')
        plan,plan_sha=bound(owner);budget=Budget(owner,plan,plan_sha)
        require(stop_after_jobs is None or type(stop_after_jobs)is int and stop_after_jobs>=0,'Nonnegative qualification interruption required')
        latest=owner/'latest.json'
        require(resume or not latest.exists() and not any((owner/'checkpoints').glob('checkpoint-*.pt')),
                'Use explicit resume for an existing checkpoint')
        checkpoint=None
        if resume:recover_pointer(owner,plan_sha)
        if latest.exists():checkpoint=checkpoint_read(owner,json.loads(latest.read_text()),plan_sha)
        if checkpoint is not None:
            require(0<=checkpoint['next_job']<=len(plan['schedule']),'Checkpoint cursor outside schedule')
            committed=_history(owner,checkpoint['next_job'],checkpoint['history_head_sha256'])
            require(type(checkpoint['work_seconds'])in(int,float) and math.isfinite(checkpoint['work_seconds'])
                    and checkpoint['work_seconds']>=0,'Invalid checkpoint time budget')
            require(type(checkpoint['optimizer_updates'])is int and checkpoint['optimizer_updates']>=0
                    and checkpoint['optimizer_updates']==sum(row['loss_frames']>0 for row in committed),
                    'Checkpoint optimizer exposure differs from committed history')
            budget.base=max(budget.base,checkpoint['work_seconds'])
        if (owner/'report.json').exists():
            report_seal=json.loads((owner/'report-seal.json').read_text())
            require(report_seal['sha256']==digest(owner/'report.json'),'Completed report body differs')
            report=json.loads((owner/'report.json').read_text())
            require(checkpoint is not None and report['plan_sha256']==plan_sha
                    and report['checkpoint_sha256']==digest(owner/'checkpoints'/json.loads(latest.read_text())['path']),
                    'Completed report checkpoint differs')
            return report
        reason=budget.reason()
        if reason or stop_after_jobs==0:
            job_index=0 if checkpoint is None else checkpoint['next_job']
            job=plan['schedule'][job_index] if job_index<len(plan['schedule']) else None
            status_path=owner/'status.json'
            atomic_json(status_path,dict(schema=SCHEMA,plan_sha256=plan_sha,state='paused',
                reason=reason or 'qualification_interruption',next_job=job_index,
                optimizer_updates=0 if checkpoint is None else checkpoint['optimizer_updates'],
                epoch=None if job is None else job['epoch'],
                song_id=None if job is None else plan['stream_plan']['songs'][job['song']]['song_id'],
                chunk=None if job is None else job['chunk'],global_frame=None if job is None else job['global_frame_start']))
            budget.changed(status_path);budget.record()
            return dict(state='paused',reason=reason or 'qualification_interruption',optimizer_updates_this_run=0)
        device=plan['device'];require(device!='mps' or torch.backends.mps.is_available(),'MPS unavailable; no silent fallback')
        try:reader=data.SourceReader(plan['index_path'],expected_sha256=plan['index_sha256'],max_crop_seconds=10.)
        finally:budget.record()
        config=_config(plan['config']);torch.manual_seed(plan['seed'])
        if device=='mps':torch.mps.manual_seed(plan['seed'])
        try:model=create_model(config,plan['architecture'],plan['carrier_prior']).to(device).train()
        finally:budget.record()
        opt=plan['optimizer'];optimizer=torch.optim.Adam(model.parameters(),lr=opt['lr'],betas=tuple(opt['betas']),eps=opt['eps'],weight_decay=0.,amsgrad=False,foreach=False,fused=False)
        initial_sha=model._state_weight_sha();cursor=None;next_job=0;updates=0;head=None;source_ledger={};read_events=[]
        if checkpoint is not None:
            model.load_state_dict(checkpoint['model']);optimizer.load_state_dict(checkpoint['optimizer'])
            cursor=None if checkpoint['cursor'] is None else cursor_from_payload(model,checkpoint['cursor'])
            next_job=checkpoint['next_job'];updates=checkpoint['optimizer_updates'];head=checkpoint['history_head_sha256']
            source_ledger=checkpoint['source_ledger'];read_events=checkpoint['read_events'];initial_sha=checkpoint['initial_sha256']
            torch.set_rng_state(checkpoint['torch_rng'])
            if device=='mps':torch.mps.set_rng_state(checkpoint['mps_rng'])
        lookup=_source_lookup(reader,plan['stream_plan'])
        # Re-hash previously used source bodies on resume; a fresh process must
        # not trust the prior process's cached file signatures.
        for key,body in source_ledger.items():
            require(key in lookup and lookup[key]['sha256']==body,'Committed source identity differs')
            reason=budget.reason()
            if reason:raise ValueError('Stopped during source re-verification: '+reason)
            try:reader._verify(data.contained(reader.root,lookup[key]['path']),lookup[key])
            finally:budget.record()
        starting_updates=updates;executed=0;case=None;case_key=None

        def status(state,reason=None):
            job=plan['schedule'][next_job] if next_job<len(plan['schedule']) else None
            row=dict(schema=SCHEMA,plan_sha256=plan_sha,state=state,reason=reason,next_job=next_job,
                optimizer_updates=updates,elapsed_seconds=budget.elapsed(),
                epoch=None if job is None else job['epoch'],
                song_id=None if job is None else plan['stream_plan']['songs'][job['song']]['song_id'],
                chunk=None if job is None else job['chunk'],
                global_frame=None if job is None else job['global_frame_start'])
            path=owner/'status.json';atomic_json(path,row);budget.changed(path)

        def commit():
            nonlocal checkpoint
            budget.rescan()
            for name,body in plan['source_sha256'].items():
                require(digest(Path(__file__).with_name(name))==body and digest(owner/'source'/name)==body,
                        'Frozen code changed before checkpoint')
            elapsed=budget.record();name=f'checkpoint-{next_job:08d}.pt';path=owner/'checkpoints'/name
            if path.exists():
                previous=json.loads(latest.read_text());require(previous['path']==name and digest(path)==previous['sha256'],'Immutable checkpoint collision')
                return previous
            value=dict(schema=SCHEMA,plan_sha256=plan_sha,next_job=next_job,optimizer_updates=updates,
                model=cpu_copy(model.state_dict()),optimizer=cpu_copy(optimizer.state_dict()),
                cursor=None if cursor is None else cursor_to_payload(model,cursor),initial_sha256=initial_sha,
                torch_rng=torch.get_rng_state(),mps_rng=torch.mps.get_rng_state().cpu() if device=='mps' else None,
                history_head_sha256=head,source_ledger=source_ledger,read_events=read_events,
                work_seconds=elapsed,recurrent_history=plan['recurrent_history'])
            import io
            buffer=io.BytesIO();torch.save(value,buffer);body=buffer.getvalue()
            budget.write_room(len(body)+4096)
            publish(path,body);budget.changed(path)
            pointer=dict(path=name,sha256=digest(path),next_job=next_job)
            atomic_json(latest,pointer);budget.changed(latest);checkpoint=value;return pointer

        try:
            status('running')
            while next_job<len(plan['schedule']):
                reason=budget.reason()
                if reason or (stop_after_jobs is not None and executed>=stop_after_jobs):
                    commit();status('paused',reason or 'qualification_interruption');return dict(state='paused',reason=reason or 'qualification_interruption',next_job=next_job,
                        optimizer_updates=updates,optimizer_updates_this_run=updates-starting_updates)
                if updates>=plan['max_updates'] and plan['schedule'][next_job]['loss_frames']:
                    reason='fixed_update_budget';break
                job=plan['schedule'][next_job];song=plan['stream_plan']['songs'][job['song']]
                chunk=StreamChunk(**song['chunks'][job['chunk']]);key=(job['epoch'],job['song'],job['chunk'])
                if key!=case_key:
                    case=None
                    case=prepare_chunk(reader,chunk,config,device=device);case_key=key
                    prov=case['source_provenance']
                    for proof in prov['source_proofs']:
                        source_ledger[chunk.song_id+'/'+proof['source_id']]=proof['sha256']
                    read_events.append(dict(epoch=job['epoch'],song_id=chunk.song_id,chunk=chunk.index,
                        read_interval=[chunk.read_start,chunk.read_stop],core_interval=[chunk.core_start,chunk.core_stop],
                        provenance_sha256=hashlib.sha256(encoded(prov)).hexdigest()))
                    reason=budget.reason()
                    if reason:commit();status('paused',reason);return dict(state='paused',reason=reason,next_job=next_job,optimizer_updates_this_run=updates-starting_updates)
                if chunk.index==0 and job['start']==0:cursor=None
                optimizer.zero_grad(set_to_none=True)
                output,cursor=advance(model,case,cursor,arm=plan['arm'],start=job['start'],stop=job['stop'])
                metrics=None;norm=None
                require(int(case['valid'][job['start']:job['stop']].sum())==job['loss_frames'],'Planned loss coverage differs')
                if job['loss_frames']:
                    loss,values=objective(output,case,job['start'],job['stop']);loss.backward()
                    require(all(p.grad is None or bool(torch.isfinite(p.grad).all()) for p in model.parameters()),'Nonfinite gradient')
                    norm=torch.nn.utils.clip_grad_norm_(model.parameters(),opt['clip_norm'],norm_type=opt['clip_norm_type'],
                                                       foreach=False,error_if_nonfinite=True)
                    require(bool(torch.isfinite(norm)),'Nonfinite gradient norm')
                    optimizer.step();updates+=1;metrics={k:float(v) for k,v in values.items()};norm=float(norm)
                cursor=cursor.detach();del output
                row=dict(job_index=next_job,previous_sha256=head,song_id=chunk.song_id,**job,
                    optimizer_updates=updates,metrics=metrics,preclip_gradient_norm=norm)
                path=owner/'history'/f'job-{next_job:08d}.json';body=encoded(row);budget.write_room(len(body)+4096)
                if path.exists():require(path.read_bytes()==body,'Uncommitted history replay differs; no overwrite')
                else:publish(path,body)
                budget.changed(path)
                head=digest(path);next_job+=1;executed+=1
                if next_job%plan['checkpoint_every']==0:commit()
                status('running')
            else:reason='full_schedule_completed'
            pointer=commit();reader_proof=reader.verify_sources();bound(owner)
            history=_history(owner,next_job,head)
            report=dict(schema=SCHEMA,state='completed' if next_job==len(plan['schedule']) else 'budget_exhausted',
                reason=reason,plan_sha256=plan_sha,checkpoint_sha256=pointer['sha256'],
                initial_sha256=initial_sha,final_sha256=model._state_weight_sha(),next_job=next_job,
                planned_jobs=len(plan['schedule']),optimizer_updates=updates,
                forwarded_frames=sum(v['global_frame_stop']-v['global_frame_start'] for v in history),
                optimized_loss_frames=sum(v['loss_frames'] for v in history),
                complete_schedule=next_job==len(plan['schedule']),source_verification=reader_proof,
                source_ledger_sha256=hashlib.sha256(encoded(source_ledger)).hexdigest(),
                read_events=len(read_events),elapsed_seconds=budget.record(),
                dev_optimizer_updates=0,test_pcm_accessed=False,quality_approved=False,car_approved=False)
            budget.write_room(len(encoded(report))+4096);fresh_json(owner/'report.json',report)
            fresh_json(owner/'report-seal.json',dict(sha256=digest(owner/'report.json')))
            status(report['state'],reason);return report
        finally:
            budget.record()


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('prepare');p.add_argument('--index',required=True);p.add_argument('--index-sha256',required=True)
    p.add_argument('--owner',required=True);p.add_argument('--freeze-owner',required=True)
    p.add_argument('--architecture',choices=('film_zero','film_control','direct_power'),default='film_zero')
    p.add_argument('--arm',choices=('predicted','disabled','oracle_train'),default='predicted')
    p.add_argument('--calibration');p.add_argument('--calibration-sha256')
    for name,typ,default in (('epochs',int,1),('chunk-frames',int,128),('seed',int,91621),('learning-rate',float,.001),
        ('sample-rate',int,48000),('checkpoint-every',int,64),('max-updates',int,None),('max-seconds',float,1800.),
        ('min-free-gib',float,50.),('max-memory-gib',float,4.),('max-output-mib',float,4096.)):
        p.add_argument('--'+name,type=typ,default=default)
    p.add_argument('--profile',choices=('reference','student'),default='reference');p.add_argument('--device',choices=('cpu','mps'),default='cpu')
    for command in ('run','resume'):
        p=sub.add_parser(command);p.add_argument('--owner',required=True);p.add_argument('--stop-after-jobs',type=int)
    parser.add_argument('--threads',type=int,default=2)
    args=vars(parser.parse_args());torch.set_num_threads(args.pop('threads'));command=args.pop('command')
    if command=='prepare':
        args['index_path']=args.pop('index');args['calibration_path']=args.pop('calibration');result=prepare(**args)
    else:result=run(args['owner'],resume=command=='resume',stop_after_jobs=args['stop_after_jobs'])
    print(json.dumps(result,allow_nan=False))


if __name__=='__main__':main()
