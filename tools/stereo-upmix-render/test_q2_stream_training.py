"""Authored full-song frame/descriptor/state proofs; no corpus PCM or optimizer."""
from dataclasses import replace
import copy
import json
from pathlib import Path
import unittest
from unittest import mock

import numpy as np
import torch

from q2_local_guidance_model import Q2Config,Q2LocalGuidanceModel
from q2_training_core import prepare_case,objective
from q2_stream_training import (StreamChunk,plan_song,plan_index,prepare_chunk,advance,
    cursor_to_payload,cursor_from_payload,denominator_from_statistics,validate_chunk)


class AuthoredReader:
    """Exact native aligned arrays, with explicitly shorter source zero tails."""
    def __init__(self,config,frames=15117,partition='TRAIN'):
        self.config=config;self.calls=[];self.partition=partition
        rng=np.random.default_rng(33191)
        s=(rng.normal(size=(frames,len(config.classes),2))*.012).astype('f4')
        s[frames//2:,0]=0
        # Nonstationary source/silence boundary exercises flux and quiet masks.
        s[777:1025]=0
        self.sources=s;self.mixture=s.sum(1);self.frames=frames
        self.row=dict(song_id='authored',partition=partition,sample_rate=config.sample_rate,
            frames=frames,common_frames=frames//2,artist='synthetic',
            complete_reconstruction_candidate=True,reconstruction_known=[True]*len(config.classes),
            class_known=[True]*len(config.classes),exclusion_reasons=[])

    def read_crop(self,song_id,start_frame,frames,*,target_rate,context_frames,partition):
        if (song_id!='authored' or partition!=self.partition or partition=='TEST'
            or target_rate!=self.config.sample_rate or context_frames!=0
            or not 0<=start_frame<start_frame+frames<=self.frames):
            raise ValueError('Authored reader timeline violation')
        self.calls.append((start_frame,frames))
        return dict(id='authored',song_id='authored',partition=partition,artist='synthetic',
            sample_rate=target_rate,classes=self.config.classes,global_start_frame=start_frame,
            mixture=self.mixture[start_frame:start_frame+frames].copy(),
            targets=self.sources[start_frame:start_frame+frames].copy(),
            reconstruction_known=np.ones(len(self.config.classes),bool),
            detection_known=np.ones(len(self.config.classes),bool),core_start=0,core_stop=frames,
            provenance=dict(synthetic_only=True,source_tails_truncated=False,test_pcm_accessed=False))

    def whole(self):
        return self.read_crop('authored',0,self.frames,target_rate=self.config.sample_rate,
                              context_frames=0,partition=self.partition)


def nonzero_model(config):
    torch.manual_seed(9393)
    model=Q2LocalGuidanceModel(config)
    with torch.no_grad():
        # Make mask, residual and local guidance effects observable immediately.
        for name,p in model.named_parameters():
            if name in ('mask_head','refinement_head','hint_film.weight','context_film.weight'):
                p.copy_(torch.randn_like(p)*.027)
        model.mask_bias.fill_(.031)
    return model


class StreamTrainingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.old_threads=torch.get_num_threads();torch.set_num_threads(2)
    @classmethod
    def tearDownClass(cls):torch.set_num_threads(cls.old_threads)

    def config(self,rate=48000):
        return Q2Config(sample_rate=rate,width=12,band_hidden=24,classes=('voice','percussion'))

    def assert_state_close(self,a,b):
        self.assertEqual(a.frames,b.frames);self.assertEqual(a.mode,b.mode)
        for name in ('previous_log_power','previous_power_valid','block_sum','band_hidden',
                     'held_context','held_raw_guidance','oracle_sum','held_oracle'):
            x,y=getattr(a,name),getattr(b,name)
            if x.dtype==torch.bool:self.assertTrue(torch.equal(x,y),name)
            else:torch.testing.assert_close(x,y,rtol=3e-5,atol=3e-7,msg=name)
        for x,y in zip(a.encoder_histories,b.encoder_histories):
            torch.testing.assert_close(x,y,rtol=3e-5,atol=3e-7)

    def test_whole_song_vs_chunk_spectra_hints_ownership_all_rates_and_tail_shapes(self):
        for rate in (44100,48000):
            c=self.config(rate)
            for length in (14080,14193,15117):
                with self.subTest(rate=rate,length=length):
                    reader=AuthoredReader(c,length)
                    chunks=plan_song(reader.row,c,core_hops=11,past_hops=8)
                    whole=prepare_case(reader.whole(),c)
                    cases=[prepare_chunk(reader,v,c) for v in chunks]
                    self.assertEqual(chunks[0].core_start,0);self.assertEqual(chunks[-1].core_stop,length)
                    self.assertTrue(all(v.read_start%1024==0 for v in chunks))
                    self.assertEqual([v.index for v in chunks],list(range(len(chunks))))
                    x=torch.cat([v['original'] for v in cases],1)
                    s=torch.cat([v['targets'] for v in cases],1)
                    torch.testing.assert_close(x,whole['original'],rtol=0,atol=0)
                    torch.testing.assert_close(s,whole['targets'],rtol=0,atol=0)
                    for name in whole['truth']:
                        joined=torch.cat([v['truth'][name] for v in cases],1)
                        torch.testing.assert_close(joined,whole['truth'][name],rtol=0,atol=0,msg=name)
                    self.assertTrue(torch.equal(torch.cat([v['valid'] for v in cases]),whole['valid']))
                    self.assertTrue(torch.equal(torch.cat([v['block_valid'] for v in cases]),whole['block_valid']))
                    self.assertEqual(sum(int(v['valid'].sum()) for v in cases),length//256-3)
                    self.assertFalse(cases[0]['valid'][:3].any())
                    self.assertTrue(cases[0]['left_padding'][:3].all())
                    self.assertTrue(cases[-1]['right_padding'][-3:].all())
                    # First new frame at every seam is scored despite its FFT
                    # window beginning in the preceding requested core.
                    self.assertTrue(all(v['valid'][0] for v in cases[1:]))
                    self.assertTrue(all(int(v['frame_starts'][0])<v['core'][0] for v in cases[1:]))
                    combined=denominator_from_statistics([v['power_statistics'] for v in cases])
                    torch.testing.assert_close(combined,whole['denominator'],rtol=2e-7,atol=1e-12)
                    # No shortened-source tail truncation to common_frames.
                    self.assertGreater(chunks[-1].core_stop,reader.row['common_frames'])

    def test_continuing_outputs_final_state_guidance_modes_and_actual_objective_backward(self):
        c=self.config();reader=AuthoredReader(c,12149)
        chunks=plan_song(reader.row,c,core_hops=9,past_hops=8)
        cases=[prepare_chunk(reader,v,c) for v in chunks]
        whole=prepare_case(reader.whole(),c)
        for arm in ('predicted','disabled','oracle_train'):
            with self.subTest(arm=arm):
                model=nonzero_model(c).train()
                if arm=='oracle_train':
                    from q2_local_guidance_model import ORACLE_TOKEN
                    expected,end=model.forward_oracle_train(whole['original'],whole['truth']['raw_guidance'],
                        partition='train',diagnostic_token=ORACLE_TOKEN)
                else:expected,end=model(whole['original'],guidance_mode=arm)
                cursor=None;parts=[]
                for case in cases:
                    out,cursor=advance(model,case,cursor,arm=arm);parts.append(out)
                for key in ('sources_ri','extraction_ri','refinement_ri','activity_logits','descriptors','updated'):
                    joined=torch.cat([v[key] for v in parts],1)
                    torch.testing.assert_close(joined,expected[key],rtol=3e-5,atol=3e-7,msg=key)
                self.assert_state_close(cursor.model_state,end)
                self.assertEqual(cursor.next_frame,whole['original'].shape[1])
                self.assertEqual(cursor.next_chunk,len(chunks))
                loss,values=objective(parts[-1],cases[-1],0,cases[-1]['original'].shape[1])
                loss.backward()
                self.assertTrue(torch.isfinite(loss));self.assertTrue(all(torch.isfinite(v) for v in values.values()))
                self.assertTrue(all(p.grad is None or torch.isfinite(p.grad).all() for p in model.parameters()))

    def test_wrong_order_missing_phase_foreign_reset_and_reload_fail_closed(self):
        c=self.config();reader=AuthoredReader(c);chunks=plan_song(reader.row,c,core_hops=9,past_hops=8)
        cases=[prepare_chunk(reader,v,c) for v in chunks[:3]]
        model=nonzero_model(c)
        with self.assertRaisesRegex(ValueError,'begin'):advance(model,cases[1])
        _,cursor=advance(model,cases[0]);payload=cursor_to_payload(model,cursor)
        restored=cursor_from_payload(model,payload)
        expected,end=advance(model,cases[1],cursor)
        observed,again=advance(model,cases[1],restored)
        self.assertTrue(torch.equal(expected['sources_ri'],observed['sources_ri']))
        self.assert_state_close(end.model_state,again.model_state)
        for bad in (cases[0],cases[2]):
            with self.assertRaisesRegex(ValueError,'Missing/reordered'):advance(model,bad,cursor)
        with self.assertRaises(ValueError):advance(nonzero_model(c),cases[1],cursor)
        with self.assertRaises(ValueError):advance(model,cases[1],cursor,arm='disabled')
        wrong=replace(chunks[1],read_start=chunks[1].read_start+256)
        before=len(reader.calls)
        with self.assertRaisesRegex(ValueError,'phase'):prepare_chunk(reader,wrong,c)
        self.assertEqual(len(reader.calls),before)
        bad=copy.deepcopy(payload);bad['next_frame']+=1
        with self.assertRaises(ValueError):cursor_from_payload(model,bad)
        detached=cursor.detach();self.assertFalse(detached.model_state.band_hidden.requires_grad)
        self.assertNotEqual(detached.model_state.band_hidden.data_ptr(),cursor.model_state.band_hidden.data_ptr())
        model.load_state_dict(model.state_dict())
        with self.assertRaises(ValueError):advance(model,cases[1],cursor)

    def test_original_only_normal_forward_and_oracle_dev_refusal(self):
        c=self.config();reader=AuthoredReader(c,partition='DEV')
        chunk=plan_song(reader.row,c,core_hops=9,past_hops=8)[0]
        case=prepare_chunk(reader,chunk,c);model=nonzero_model(c).train()
        with mock.patch.object(model,'forward',wraps=model.forward) as spy:
            advance(model,case)
            args,kwargs=spy.call_args
            self.assertEqual(len(args),2);self.assertTrue(torch.equal(args[0],case['original']));self.assertIsNone(args[1])
            self.assertEqual(kwargs,{'guidance_mode':'predicted'})
        with mock.patch.object(model,'forward_oracle_train',side_effect=AssertionError('DEV target forwarded')):
            with self.assertRaisesRegex(ValueError,'Oracle'):advance(model,case,arm='oracle_train')
        bad=dict(reader.row,partition='TEST')
        with self.assertRaises(ValueError):plan_song(bad,c)

    def test_full_reference12_heads_subchunk_bptt_cursor_roundtrip_no_frame_loss(self):
        c=Q2Config();reader=AuthoredReader(c,6417)
        chunks=plan_song(reader.row,c,core_hops=11,past_hops=8)
        cases=[prepare_chunk(reader,v,c) for v in chunks]
        model=nonzero_model(c);whole=prepare_case(reader.whole(),c)
        expected,end=model(whole['original']);cursor=None;parts=[]
        for case in cases:
            size=case['original'].shape[1]
            for start in range(0,size,3):
                stop=min(size,start+3)
                output,cursor=advance(model,case,cursor,start=start,stop=stop)
                parts.append(output['sources_ri'])
                cursor=cursor_from_payload(model,cursor_to_payload(model,cursor.detach()))
                self.assertEqual(cursor.next_frame,case['stream_chunk'].frame_start+stop)
                with self.assertRaises(ValueError):advance(model,case,cursor,start=start,stop=stop)
        torch.testing.assert_close(torch.cat(parts,1),expected['sources_ri'],rtol=3e-5,atol=3e-7)
        self.assert_state_close(cursor.model_state,end)
        self.assertEqual(cursor.next_chunk,len(chunks))

    def test_future_pcm_suffix_cannot_change_prior_owned_spectra_or_model_prefix(self):
        c=self.config();a=AuthoredReader(c);b=AuthoredReader(c)
        chunks=plan_song(a.row,c,core_hops=11,past_hops=8)
        boundary=chunks[1].core_stop
        b.sources[boundary:]*=-3;b.mixture[boundary:]=b.sources[boundary:].sum(1)
        ma=nonzero_model(c);mb=nonzero_model(c);ca=cb=None
        for chunk in chunks[:2]:
            pa=prepare_chunk(a,chunk,c);pb=prepare_chunk(b,chunk,c)
            self.assertTrue(torch.equal(pa['original'],pb['original']))
            oa,ca=advance(ma,pa,ca);ob,cb=advance(mb,pb,cb)
            self.assertTrue(torch.equal(oa['sources_ri'],ob['sources_ri']))

    def test_fully_quiet_song_keeps_finite_loss_and_exact_zero_audio_without_permission(self):
        c=self.config();reader=AuthoredReader(c,frames=8319)
        reader.sources.fill(0);reader.mixture.fill(0)
        model=nonzero_model(c);cursor=None
        for chunk in plan_song(reader.row,c,core_hops=11,past_hops=8):
            case=prepare_chunk(reader,chunk,c)
            self.assertTrue(torch.equal(case['denominator'],torch.full((2,),1e-9)))
            output,cursor=advance(model,case,cursor)
            self.assertEqual(int(torch.count_nonzero(output['sources_ri'])),0)
            loss,_=objective(output,case,0,case['original'].shape[1])
            loss.backward();cursor=cursor.detach()
            self.assertTrue(torch.isfinite(loss))
            self.assertTrue(all(p.grad is None or torch.isfinite(p.grad).all() for p in model.parameters()))

    def test_planning_exposure_tiny_tail_merge_short_reads_fixed_denominator_and_invalid_data(self):
        c=self.config();reader=AuthoredReader(c,frames=9*256*3+31)
        chunks=plan_song(reader.row,c,core_hops=9,past_hops=8)
        self.assertEqual(len(chunks),3);self.assertEqual(chunks[-1].core_stop-chunks[-1].core_start,9*256+31)
        fixed=torch.tensor([.3,.7],dtype=torch.float32)
        case=prepare_chunk(reader,chunks[1],c,denominator=fixed)
        self.assertTrue(torch.equal(case['denominator'],fixed));self.assertEqual(case['normalization'],'fixed_external_song')
        fixed[0]=99;self.assertEqual(float(case['denominator'][0]),float(torch.tensor(.3)))
        for denom in (torch.tensor([0.,1.]),torch.tensor([float('nan'),1.]),torch.ones(3)):
            with self.assertRaises(ValueError):prepare_chunk(reader,chunks[1],c,denominator=denom)
        with self.assertRaises(ValueError):plan_song(reader.row,c,past_hops=7)
        with self.assertRaises(ValueError):plan_song(reader.row,c,max_seconds=.01)
        with self.assertRaises(ValueError):plan_song(dict(reader.row,frames=2047),c)
        with self.assertRaises(ValueError):denominator_from_statistics([])
        bad=AuthoredReader(c);bad.mixture[0,0]=np.nan
        with self.assertRaises(ValueError):prepare_chunk(bad,plan_song(bad.row,c)[0],c)
        index=dict(classes=list(c.classes),rows=[reader.row,
            dict(reader.row,song_id='excluded',complete_reconstruction_candidate=False,exclusion_reasons=['label unknown']),
            dict(reader.row,song_id='sealed',partition='TEST')])
        plan=plan_index(index,partition='TRAIN',config=c,core_hops=9,past_hops=8)
        self.assertEqual(plan['song_count'],1);self.assertEqual(len(plan['excluded']),1)
        song=plan['songs'][0]
        self.assertEqual(song['requested_core_union'],[[0,reader.frames]])
        self.assertEqual(song['model_visible_real_pcm_union'],[[0,reader.frames]])
        self.assertEqual(song['loss_frame_count'],reader.frames//256-3)
        self.assertFalse(plan['source_pcm_decoded']);self.assertFalse(plan['test_pcm_accessed'])

    def test_actual_index_all81train_metadata_only_no_pcm(self):
        path=Path(__file__).resolve().parents[2]/'output/q2-source-fit-input-a/proofs/index.json'
        if not path.exists():self.skipTest('Local metadata snapshot absent; no corpus access fallback')
        index=json.loads(path.read_text())
        with mock.patch('soundfile.SoundFile',side_effect=AssertionError('PCM opened during planning')):
            result=plan_index(index,partition='TRAIN',config=Q2Config())
        self.assertEqual(result['song_count'],81)
        expected={r['song_id'] for r in index['rows'] if r['partition']=='TRAIN' and r['complete_reconstruction_candidate']}
        self.assertEqual({r['song_id'] for r in result['songs']},expected)
        for song in result['songs']:
            self.assertEqual(song['requested_core_union'],[[0,song['song_frames']]])
            self.assertEqual(song['loss_frame_count'],song['song_frames']//256-3)
            records=[StreamChunk(**r) for r in song['chunks']]
            self.assertEqual(sum(r.frame_stop-r.frame_start for r in records),song['model_frame_interval'][1])
            self.assertTrue(all(r.read_stop-r.read_start<=480000 for r in records))
            self.assertTrue(all(a.frame_stop==b.frame_start for a,b in zip(records,records[1:])))


if __name__=='__main__':unittest.main()
