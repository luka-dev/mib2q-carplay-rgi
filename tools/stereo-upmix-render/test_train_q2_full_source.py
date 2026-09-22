"""Actual small CPU process restarts against authored native SourceReader files."""
from dataclasses import asdict
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import torch
import numpy as np

import train_q2_full_source as full
from release_acceptance import seal
from test_q2_source_data import index as authored_index

ROOT=Path(__file__).resolve().parent
REPO=ROOT.parent.parent


def fixture(root):
    path,_,_,_=authored_index(root)
    freeze=root/'freeze'
    seal(ROOT/'release_acceptance_v2.json',REPO/'docs/sources/UPMIX_RELEASE_ACCEPTANCE_V2.md',freeze)
    return path,freeze


def preparation(path,freeze,owner,**kw):
    return full.prepare(path,owner,index_sha256=full.digest(path),freeze_owner=freeze,
        profile='student',epochs=1,chunk_frames=4,core_hops=9,past_hops=8,checkpoint_every=5,
        max_seconds=120.,min_free_gib=0.,max_memory_gib=4.,max_output_mib=128.,**kw)


WORKER=r'''
import sys,json,torch
sys.path.insert(0,sys.argv[1]);torch.set_num_threads(2)
import train_q2_full_source as f
result=f.run(sys.argv[2],resume=sys.argv[3]=='resume',stop_after_jobs=None if sys.argv[4]=='none' else int(sys.argv[4]))
print(json.dumps({'result':result},allow_nan=False))
'''


def process(owner,*,resume=False,cap=None,success=True):
    result=subprocess.run([sys.executable,'-c',WORKER,str(ROOT),str(owner),'resume' if resume else 'run',
        'none' if cap is None else str(cap)],capture_output=True,text=True,timeout=140,
        env={**os.environ,'OMP_NUM_THREADS':'2','OPENBLAS_NUM_THREADS':'2','MKL_NUM_THREADS':'2'})
    if not success:return result
    if result.returncode:raise AssertionError(result.stdout+'\n'+result.stderr)
    rows=[json.loads(line)['result'] for line in result.stdout.splitlines() if line.startswith('{"result":')]
    if len(rows)!=1:raise AssertionError('Missing unique process result')
    return rows[0]


def checkpoint(owner):
    pointer=json.loads((owner/'latest.json').read_text())
    return full.checkpoint_read(owner,pointer,full.digest(owner/'plan.json'))


def authored_calibration(path,folder):
    folder.mkdir();(folder/'code').mkdir()
    reader=full.data.SourceReader(path);song=reader.rows['song-0']
    total=(song['frames']*48000+44099)//44100
    row=reader.read_crop('song-0',0,total,target_rate=48000,partition='TRAIN')
    sums=np.square(row['targets'].astype('f8')).sum((0,2))
    prior=np.maximum(sums/sums.sum(),1e-6);prior/=prior.sum()
    (folder/'source-proofs.json').write_text(json.dumps(row['provenance']['source_proofs']))
    (folder/'cache-log.json').write_text(json.dumps(dict(synthetic_only=True,interval=[0,total])))
    (folder/'code/authored.py').write_text('# Synthetic test calibration fixture only.\n')
    value=dict(schema=full.CALIBRATION,complete=True,partition='TRAIN',index_sha256=full.digest(path),
        target_rate=48000,classes=list(full.data.CLASSES),song_intervals={'song-0':[[0,total]]},
        source_power_sums=sums.tolist(),carrier_prior=prior.tolist(),test_pcm_accessed=False,
        definition='pooled full TRAIN PCM source squared sums, each sample once, floor probability1e-6 then renormalize',
        source_proof_sha256=full.digest(folder/'source-proofs.json'),cache_log_sha256=full.digest(folder/'cache-log.json'),
        calibration_code_path='code/authored.py',calibration_code_sha256=full.digest(folder/'code/authored.py'))
    target=folder/'calibration.json';target.write_text(json.dumps(value));return target


class FullSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.threads=torch.get_num_threads();torch.set_num_threads(2)
        cls.thread_env=mock.patch.dict(os.environ,
            {'OMP_NUM_THREADS':'2','OPENBLAS_NUM_THREADS':'2','MKL_NUM_THREADS':'2'})
        cls.thread_env.start()
    @classmethod
    def tearDownClass(cls):
        cls.thread_env.stop();torch.set_num_threads(cls.threads)

    def equal(self,a,b,path='root'):
        self.assertEqual(type(a),type(b),path)
        if isinstance(a,torch.Tensor):
            self.assertEqual(a.shape,b.shape,path);self.assertEqual(a.dtype,b.dtype,path)
            self.assertTrue(torch.equal(a,b),path)
        elif isinstance(a,dict):
            self.assertEqual(a.keys(),b.keys(),path)
            for key in a:self.equal(a[key],b[key],path+'.'+str(key))
        elif isinstance(a,(list,tuple)):
            self.assertEqual(len(a),len(b),path)
            for n,(x,y) in enumerate(zip(a,b)):self.equal(x,y,path+'.'+str(n))
        else:self.assertEqual(a,b,path)

    def test_prepare_is_metadata_only_sealed_train_complete_timeline(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path,freeze=fixture(root);owner=root/'prepared'
            with mock.patch.object(full,'create_model',side_effect=AssertionError('model initialized')), \
                 mock.patch.object(full.torch.optim,'Adam',side_effect=AssertionError('optimizer initialized')), \
                 mock.patch.object(full.data,'SourceReader',side_effect=AssertionError('PCM reader initialized')):
                result=preparation(path,freeze,owner)
            self.assertEqual(result['state'],'prepared');self.assertEqual(result['optimizer_updates'],0)
            plan,_=full.bound(owner);self.assertEqual(plan['stream_plan']['song_count'],1)
            self.assertTrue(all(row['partition']=='TRAIN' for row in plan['stream_plan']['songs']))
            song=plan['stream_plan']['songs'][0]
            self.assertEqual(song['requested_core_union'],[[0,song['song_frames']]])
            self.assertEqual(sum(j['loss_frames'] for j in plan['schedule']),song['loss_frame_count'])
            self.assertFalse((owner/'latest.json').exists())
            value=json.loads((owner/'plan.json').read_text());value['epochs']=2
            (owner/'plan.json').write_text(json.dumps(value))
            with self.assertRaisesRegex(ValueError,'plan body'):full.bound(owner)

    def test_audio_runtime_dependencies_bound_before_model_or_source_reads(self):
        from scipy.signal import _signaltools,_upfirdn,_upfirdn_apply
        import scipy
        import soundfile as sf
        actual=full.environment()
        self.assertEqual(actual['numpy'],np.__version__)
        self.assertEqual(actual['scipy'],scipy.__version__)
        self.assertEqual(actual['soundfile'],sf.__version__)
        self.assertEqual(actual['libsndfile'],sf.__libsndfile_version__)
        self.assertEqual(actual['resample_dependencies'],
            {Path(m.__file__).name:full.digest(m.__file__)
             for m in (_signaltools,_upfirdn,_upfirdn_apply)})
        self.assertEqual(actual['threads'],
            {k:'2' for k in ('OMP_NUM_THREADS','OPENBLAS_NUM_THREADS','MKL_NUM_THREADS')})
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path,freeze=fixture(root);owner=root/'bound';preparation(path,freeze,owner)
            variants=[]
            for key in ('numpy','scipy','soundfile','libsndfile'):
                changed=copy.deepcopy(actual);changed[key]+='-different';variants.append(changed)
            for name in actual['resample_dependencies']:
                changed=copy.deepcopy(actual);changed['resample_dependencies'][name]='0'*64;variants.append(changed)
            for key in actual['threads']:
                changed=copy.deepcopy(actual);changed['threads'][key]='3';variants.append(changed)
            for changed in variants:
                with self.subTest(context=changed), \
                     mock.patch.object(full,'environment',return_value=changed), \
                     mock.patch.object(full,'create_model',side_effect=AssertionError('Model initialized')), \
                     mock.patch.object(full.data,'SourceReader',side_effect=AssertionError('PCM reader initialized')):
                    with self.assertRaisesRegex(ValueError,'Execution context changed'):
                        full.run(owner,resume=True)
            self.assertFalse((owner/'latest.json').exists())

    def test_actual_process_resume_model_adam_state_rng_history_exact_and_checkpoints_immutable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path,freeze=fixture(root);a=root/'continuous';b=root/'resumed'
            preparation(path,freeze,a);preparation(path,freeze,b)
            ra=process(a);self.assertEqual(ra['state'],'completed')
            paused=process(b,cap=1);self.assertEqual(paused['state'],'paused')
            original={p:full.digest(p) for p in (b/'checkpoints').glob('*.pt')}
            rb=process(b,resume=True);self.assertEqual(rb['state'],'completed')
            for p,body in original.items():self.assertEqual(full.digest(p),body)
            ca,cb=checkpoint(a),checkpoint(b)
            for key in ('model','optimizer','cursor','torch_rng','mps_rng','next_job','optimizer_updates','initial_sha256','source_ledger','history_head_sha256'):
                self.equal(ca[key],cb[key],key)
            ah=full._history(a,ca['next_job'],ca['history_head_sha256'])
            bh=full._history(b,cb['next_job'],cb['history_head_sha256']);self.equal(ah,bh)
            self.assertGreater(len(cb['read_events']),len(ca['read_events'])) # honest re-read on resume
            self.assertFalse(rb['test_pcm_accessed']);self.assertFalse(rb['quality_approved'])
            self.assertEqual(rb['dev_optimizer_updates'],0)
            self.assertEqual(ra['optimized_loss_frames'],rb['optimized_loss_frames'])
            self.assertEqual(ah[-1]['loss_frames'],0) # final WOLA padding still forwarded
            self.assertEqual(cb['cursor']['next_frame'],json.loads((b/'plan.json').read_text())['stream_plan']['songs'][0]['model_frame_interval'][1])
            self.assertEqual(len(cb['source_ledger']),3)
            with mock.patch.object(full,'create_model',side_effect=AssertionError('Completed model initialized')):
                replay=full.run(b,resume=True)
            self.assertEqual(replay['checkpoint_sha256'],rb['checkpoint_sha256'])
            report=json.loads((b/'report.json').read_text());report['quality_approved']=True
            (b/'report.json').write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError,'report body'):full.run(b,resume=True)

    def test_stop_disk_budget_checkpoint_path_and_source_changes_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path,freeze=fixture(root)
            with mock.patch.object(full.shutil,'disk_usage',return_value=type('Disk',(),{'free':0})()):
                with self.assertRaisesRegex(ValueError,'Disk reserve'):
                    full.prepare(path,root/'no-space',index_sha256=full.digest(path),freeze_owner=freeze,min_free_gib=1)
            self.assertFalse((root/'no-space').exists())
            owner=root/'stop';preparation(path,freeze,owner);(owner/'STOP').touch()
            with mock.patch.object(full,'create_model',side_effect=AssertionError('Model past STOP')):
                stopped=full.run(owner)
            self.assertEqual(stopped['reason'],'STOP');self.assertFalse((owner/'latest.json').exists())
            other=root/'paused';preparation(path,freeze,other);process(other,cap=1)
            pointer=json.loads((other/'latest.json').read_text());pointer['path']='../../checkpoint-00000001.pt'
            with self.assertRaisesRegex(ValueError,'pointer'):full.checkpoint_read(other,pointer,full.digest(other/'plan.json'))
            budget=json.loads((other/'work-budget.json').read_text());budget['elapsed_seconds']=120.
            (other/'work-budget.json').write_text(json.dumps(budget))
            with mock.patch.object(full,'create_model',side_effect=AssertionError('Model past cumulative budget')):
                stopped=full.run(other,resume=True)
            self.assertEqual(stopped['reason'],'cumulative_walltime')

    def test_source_body_mutation_on_resume_rehashes_prior_inputs_before_more_steps(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path,freeze=fixture(root);owner=root/'paused';preparation(path,freeze,owner)
            process(owner,cap=1);before=checkpoint(owner)['optimizer_updates']
            native=root/'originals/song-0/song-0-source-0.wav'
            with native.open('ab') as stream:stream.write(b'changed')
            result=process(owner,resume=True,success=False)
            self.assertNotEqual(result.returncode,0);self.assertIn('source body changed',result.stderr.lower())
            self.assertEqual(checkpoint(owner)['optimizer_updates'],before)

    def test_checkpoint_published_before_pointer_recovers_and_partial_is_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path,freeze=fixture(root);owner=root/'study';preparation(path,freeze,owner)
            process(owner,cap=1)
            original=json.loads((owner/'latest.json').read_text());body=full.digest(owner/'checkpoints'/original['path'])
            # Authored process-crash point: completed immutable body exists,
            # but latest-pointer publication did not occur.
            (owner/'latest.json').unlink()
            partial=owner/'checkpoints/checkpoint-00000002.pt.partial-authored'
            partial.write_bytes(b'incomplete checkpoint attempt')
            with mock.patch.object(full,'create_model',side_effect=AssertionError('Recovery must not optimize')):
                paused=full.run(owner,resume=True,stop_after_jobs=0)
            self.assertEqual(paused['state'],'paused')
            self.assertEqual(json.loads((owner/'latest.json').read_text())['sha256'],body)
            result=process(owner,resume=True);self.assertEqual(result['state'],'completed')
            self.assertEqual(partial.read_bytes(),b'incomplete checkpoint attempt')
            self.assertEqual(full.digest(owner/'checkpoints'/original['path']),body)

    def test_schedule_allocation_guard_and_incremental_output_accounting(self):
        huge=dict(songs=[dict(chunks=[dict(frame_start=0,frame_stop=full.MAX_JOBS)])])
        with mock.patch.object(full.random,'Random',side_effect=AssertionError('Allocated oversized jobs')):
            with self.assertRaisesRegex(ValueError,'allocation cap'):full.schedule(huge,2,1,13)
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path,freeze=fixture(root);owner=root/'prepared';preparation(path,freeze,owner)
            plan,body=full.bound(owner);budget=full.Budget(owner,plan,body)
            with mock.patch.object(Path,'rglob',side_effect=AssertionError('Per-step filesystem rescan')):
                budget.write_room(100);budget.write_room(100);budget.record();budget.write_room(100)
            before=budget.bytes;new=owner/'authored-counter.bin';new.write_bytes(b'abc')
            budget.changed(new);self.assertEqual(budget.bytes,before+3)
            with self.assertRaisesRegex(ValueError,'Output owner cap'):budget.write_room(129*1024**2)

    def test_fixed_update_budget_is_explicit_not_full_epoch_and_status_is_bound(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path,freeze=fixture(root);owner=root/'budget'
            preparation(path,freeze,owner,max_updates=2)
            result=process(owner)
            self.assertEqual(result['state'],'budget_exhausted');self.assertFalse(result['complete_schedule'])
            self.assertEqual(result['optimizer_updates'],2)
            status=json.loads((owner/'status.json').read_text())
            self.assertEqual(status['plan_sha256'],full.digest(owner/'plan.json'))
            self.assertEqual(status['optimizer_updates'],2);self.assertEqual(status['state'],'budget_exhausted')
            self.assertIsInstance(status['global_frame'],int)

    def test_power_wrapper_full_train_calibration_and_owned_state_resume(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path,freeze=fixture(root);cal=authored_calibration(path,root/'calibration')
            kwargs=dict(architecture='direct_power',calibration_path=cal,calibration_sha256=full.digest(cal))
            a=root/'continuous';b=root/'resumed';preparation(path,freeze,a,**kwargs);preparation(path,freeze,b,**kwargs)
            process(a);process(b,cap=1);process(b,resume=True)
            ca,cb=checkpoint(a),checkpoint(b)
            for key in ('model','optimizer','cursor','torch_rng','mps_rng','history_head_sha256'):
                self.equal(ca[key],cb[key],key)
            self.assertEqual(ca['cursor']['model_state']['schema'],'q2-power-guidance-owned-state-v1')
            self.assertEqual(full.digest(a/'code/authored.py'),full.digest(cal.parent/'code/authored.py'))
            (a/'source-proofs.json').write_text('changed')
            with self.assertRaisesRegex(ValueError,'proof/code'):full.bound(a)
            bad=json.loads(cal.read_text());bad['song_intervals']={};cal.write_text(json.dumps(bad))
            with self.assertRaisesRegex(ValueError,'Full admitted TRAIN'):
                preparation(path,freeze,root/'bad',architecture='direct_power',calibration_path=cal,calibration_sha256=full.digest(cal))


if __name__=='__main__':unittest.main()
