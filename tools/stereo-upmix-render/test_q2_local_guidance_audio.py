"""Synthetic CPU PCM contracts; no datasets, device audio, fitting or routing."""
from dataclasses import dataclass
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import torch
from torch import nn

from q2_local_guidance_audio import (Q2PCMStream, render_audio, analyze, synthesize,
    FFT, HOP, DELAY, META_SHAPES, TIMELINE_KEYS)


@dataclass(frozen=True)
class MockState:
    owner: object
    frames: int
    previous: torch.Tensor

    def detach(self):
        return MockState(self.owner,self.frames,self.previous.detach().clone())


class MockModel(nn.Module):
    def __init__(self, rate=48000, k=12, history=False):
        super().__init__()
        self.config=SimpleNamespace(sample_rate=rate,fft_size=FFT,hop=HOP,bins=513,bands=32,
                                    classes=tuple('source_'+str(i) for i in range(k)))
        self.dummy=nn.Parameter(torch.zeros(()))
        self.generation=0;self._state_owner=object();self.history=history
        self.fault_after=None;self.bad_endpoint=False;self.bad_phase=False;self.eval()

    def reload(self):
        self.generation+=1;self._state_owner=object()

    def forward(self, frames, state=None, guidance_mode='predicted'):
        if state is None:
            state=MockState(self._state_owner,0,torch.zeros_like(frames[:,0]))
        if state.owner is not self._state_owner:
            raise ValueError('Foreign state')
        k=len(self.config.classes);gains=torch.linspace(-.5,.75,k,dtype=torch.float32)
        sources=[];logs=[];updates=[];previous=state.previous
        for index in range(frames.shape[1]):
            value=frames[:,index]
            combined=value+.125*previous if self.history else value
            source=combined[:,None]*gains[None,:,None,None,None]
            if guidance_mode=='disabled':source=source*.5
            source=source.clone()
            if self.fault_after is not None and state.frames+index>=self.fault_after:
                source[0,0,0,4,0]=float('nan')
            if self.bad_endpoint:source[0,0,0,0,1]=1.
            sources.append(source)
            logs.append(value[...,0].mean((1,2))[:,None,None].expand(-1,k,32).clone())
            updates.append(torch.full((frames.shape[0],),False if self.bad_phase else
                                     (state.frames+index+1)%4==0,dtype=torch.bool))
            previous=value.clone()
        logits=torch.stack(logs,1)
        return dict(sources_ri=torch.stack(sources,1),activity_logits=logits,
            activity=torch.sigmoid(logits),descriptors=logits.new_zeros((*logits.shape,5)),
            log_error_scale=logits.new_zeros(logits.shape),updated=torch.stack(updates,1)), \
            MockState(self._state_owner,state.frames+frames.shape[1],previous)


class Q2AudioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.threads=torch.get_num_threads();torch.set_num_threads(2)

    @classmethod
    def tearDownClass(cls):torch.set_num_threads(cls.threads)

    def wave(self,n,scale=.1):
        return torch.randn(n,2,generator=torch.Generator().manual_seed(8191+n))*scale

    def exact(self,a,b):
        self.assertEqual(a.dtype,b.dtype);self.assertEqual(a.shape,b.shape)
        self.assertEqual(a.detach().contiguous().numpy().tobytes(),b.detach().contiguous().numpy().tobytes())

    def stream(self,model,wave,chunks):
        stream=Q2PCMStream(model);blocks=[];pos=index=0
        while pos<len(wave):
            n=min(chunks[index%len(chunks)],len(wave)-pos)
            block=stream.push(wave[pos:pos+n]);blocks.append(block)
            self.assertEqual((block['sample_start'],block['sample_stop']),(pos,pos+n))
            pos+=n;index+=1
        blocks.append(stream.finish())
        return {name:torch.cat([b[name] for b in blocks]) for name in
                ('sources',*META_SHAPES,'updated',*TIMELINE_KEYS)}

    def test_known_stereo_sources_classes_no_gain_clipping_both_rates(self):
        for rate in (44100,48000):
            for k in (1,3,12):
                model=MockModel(rate,k)
                for n in (1,255,256,257,1024):
                    wave=self.wave(n,1.4);copy=wave.clone();result=render_audio(model,wave)
                    expected=wave[:,None]*torch.linspace(-.5,.75,k)[None,:,None]
                    torch.testing.assert_close(result['sources'],expected,rtol=2e-6,atol=1e-6)
                    self.exact(result['residual'],wave-result['sources'].sum(1));self.exact(wave,copy)
                    self.assertEqual(result['delay_seconds'],1024/rate)
                    self.assertEqual(result['role_status'],'UNKNOWN')
                    self.assertTrue(model.dummy.requires_grad);self.assertIsNone(model.dummy.grad)
                    self.assertFalse(result['sources'].requires_grad)
        self.assertGreater(float(render_audio(MockModel(),torch.full((1024,2),8.))['sources'].abs().max()),1.)

    def test_external_chunks_exact_metadata_four_hop_phase_and_reset(self):
        wave=self.wave(5003);model=MockModel(history=True)
        reference=self.stream(model,wave,[5003])
        self.assertFalse(reference['sources'][:DELAY].any())
        self.assertEqual(len(reference['sources']),len(wave)+DELAY)
        for chunks in ([1,17,255,4093],[128],[257,3,9,511]):
            actual=self.stream(model,wave,chunks)
            for name in reference:self.exact(actual[name],reference[name])
        self.exact(reference['updated'],(reference['frame_indices']+1)%4==0)
        aligned=render_audio(model,wave)
        self.exact(aligned['sources'],reference['sources'][DELAY:DELAY+len(wave)])
        stream=Q2PCMStream(model);first=stream.push(wave)['sources'];stream.finish();stream.reset()
        self.exact(first,stream.push(wave)['sources'])

    def test_one_full_stereo_fft_and_batched_K_inverse_per_hop(self):
        original_fft,original_ifft=torch.fft.rfft,torch.fft.irfft
        calls=[];inverse=[]
        def fft(*args,**kwargs):calls.append(args[0].shape);return original_fft(*args,**kwargs)
        def ifft(*args,**kwargs):inverse.append(args[0].shape);return original_ifft(*args,**kwargs)
        with patch.object(torch.fft,'rfft',side_effect=fft),patch.object(torch.fft,'irfft',side_effect=ifft):
            result=render_audio(MockModel(),self.wave(1801),chunk_samples=7)
        count=(1801+255)//256+3
        self.assertEqual(calls,[torch.Size([2,1024])]*count)
        self.assertEqual(inverse,[torch.Size([12,2,513])]*count)
        self.assertEqual(result['frame_starts'].tolist(),[i*256-768 for i in range(count)])
        self.exact(result['frame_full_support'],(result['frame_starts']>=0)&(result['frame_ends']<=1801))
        self.assertTrue(result['frame_uses_right_padding'].any())

    def test_no_future_dependence_silence_tails_disabled_and_no_input_mutation(self):
        model=MockModel(history=True);wave=self.wave(2057)
        a,b=Q2PCMStream(model),Q2PCMStream(model)
        pa,pb=a.push(wave[:1301]),b.push(wave[:1301]);saved=pa['sources'].clone()
        for name in ('sources',*META_SHAPES,'updated'):self.exact(pa[name],pb[name])
        a.push(wave[1301:]);b.push(-3*wave[1301:]);a.finish();b.finish()
        self.exact(pa['sources'],saved)
        predicted=render_audio(model,wave);disabled=render_audio(model,wave,'disabled')
        self.exact(predicted['activity'],disabled['activity'])
        self.assertFalse(torch.equal(predicted['sources'],disabled['sources']))
        self.assertFalse(render_audio(model,torch.zeros(5003,2))['sources'].any())
        impulse=torch.zeros(5003,2);impulse[2047]=torch.tensor([1.,-.5])
        tail=render_audio(model,impulse)['sources']
        self.assertTrue(tail[2048:].any());self.assertFalse(tail[4096:].any())

    def test_empty_invalid_input_rate_device_and_model_mutation_transaction(self):
        model=MockModel();stream=Q2PCMStream(model);empty=torch.empty(0,2)
        self.assertEqual(stream.push(empty)['sources'].shape,(0,12,2))
        self.assertEqual(stream.finish()['sources'].shape,(0,12,2))
        with self.assertRaises(ValueError):stream.push(empty)
        with self.assertRaises(ValueError):stream.finish()
        stream.reset();stream.push(self.wave(111))
        for invalid in (torch.zeros(3,1),torch.zeros(3,2,dtype=torch.float64),
                        torch.tensor([[float('nan'),0.]]),torch.tensor([[float('inf'),0.]]),
                        torch.zeros(3,2,device='meta')):
            with self.assertRaises(ValueError):stream.push(invalid)
            self.assertEqual((stream.received,stream._work.fill),(111,111))
        for rate in (True,44100):
            with self.assertRaises(ValueError):Q2PCMStream(model,sample_rate=rate)
        with self.assertRaises(ValueError):Q2PCMStream(MockModel(rate=32000))
        with self.assertRaises(ValueError):Q2PCMStream(MockModel().to('meta'))
        with self.assertRaises(ValueError):Q2PCMStream(model,'oracle_train')
        stream.guidance_mode='disabled'
        with self.assertRaises(ValueError):stream.push(empty)
        self.assertEqual(stream.received,111)
        stream.reset();stream.push(self.wave(111))
        model.config=SimpleNamespace(**vars(model.config))
        with self.assertRaises(ValueError):stream.push(empty)
        stream.reset()
        model.reload()
        with self.assertRaises(ValueError):stream.push(empty)
        stream.reset()
        with torch.no_grad():model.dummy.add_(1)
        with self.assertRaises(ValueError):stream.push(empty)
        stream.reset();model.dummy.requires_grad_(False)
        with self.assertRaises(ValueError):stream.push(empty)
        model.dummy.requires_grad_(True);stream.reset();model.train()
        with self.assertRaises(ValueError):stream.push(empty)

    def test_numeric_bad_endpoint_bad_phase_and_overflow_do_not_commit(self):
        model=MockModel(history=True);stream=Q2PCMStream(model);wave=self.wave(2057)
        for attr,value in (('fault_after',1),('bad_endpoint',True),('bad_phase',True)):
            setattr(model,attr,value)
            with self.assertRaises(ValueError):stream.push(wave)
            self.assertEqual((stream.received,stream.emitted,stream._work.frames),(0,0,0))
            self.assertFalse(stream._work.history.any())
            setattr(model,attr,None if attr=='fault_after' else False)
        with self.assertRaisesRegex(ValueError,'overflow'):
            stream.push(torch.full((1024,2),torch.finfo(torch.float32).max))
        self.assertEqual(stream.received,0)

    def test_batch_analysis_synthesis_alignment_all_K_and_differentiable(self):
        for rate in (44100,48000):
            model=MockModel(rate);wave=self.wave(5003).requires_grad_()
            pack=analyze(wave,model.config)
            self.assertEqual(pack.frames_ri.shape,(1,23,2,513,2))
            result,_=model(pack.frames_ri)
            output=synthesize(result['sources_ri'],len(wave),model.config)
            expected=wave[:,None]*torch.linspace(-.5,.75,12)[None,:,None]
            torch.testing.assert_close(output,expected,rtol=2e-6,atol=1e-7)
            output.square().mean().backward()
            self.assertTrue(torch.isfinite(wave.grad).all());self.assertTrue(wave.grad.any())
            empty=analyze(torch.empty(0,2),model.config)
            self.assertEqual(empty.frames_ri.shape,(1,0,2,513,2))
            self.assertEqual(synthesize(torch.empty(1,0,12,2,513,2),0,model.config).shape,(0,12,2))
            bad=result['sources_ri'].detach().clone();bad[...,0,1]=1
            with self.assertRaises(ValueError):synthesize(bad,len(wave),model.config)
            with self.assertRaises(ValueError):synthesize(result['sources_ri'],len(wave)+256,model.config)

    def test_actual_all12_nonzero_model_pcm_exact_chunks_batch_tolerance_both_rates(self):
        from q2_local_guidance_model import Q2Config,Q2LocalGuidanceModel
        with torch.random.fork_rng(devices=[]),torch.no_grad():
            for rate in (44100,48000):
                torch.manual_seed(4129);model=Q2LocalGuidanceModel(Q2Config(sample_rate=rate)).eval()
                for parameter in model.parameters():parameter.uniform_(-.04,.04)
                wave=self.wave(5003);state={name:value.clone() for name,value in model.state_dict().items()}
                whole=render_audio(model,wave,chunk_samples=len(wave));chunks=render_audio(model,wave)
                for name in ('sources','residual',*META_SHAPES,'updated',*TIMELINE_KEYS):
                    self.exact(whole[name],chunks[name])
                irregular=self.stream(model,wave,[1,7,255,1023,3])
                self.exact(whole['sources'],irregular['sources'][DELAY:DELAY+len(wave)])
                self.assertTrue(whole['sources'].square().sum((0,2)).gt(0).all())
                self.assertFalse(render_audio(model,torch.zeros(2049,2))['sources'].any())
                pack=analyze(wave,model.config);output,_=model(pack.frames_ri)
                batch=synthesize(output['sources_ri'],len(wave),model.config)
                torch.testing.assert_close(whole['sources'],batch,rtol=3e-5,atol=3e-7)
                for name in META_SHAPES:
                    torch.testing.assert_close(whole[name],output[name][0],rtol=3e-5,atol=3e-7)
                for name,value in model.state_dict().items():self.exact(value,state[name])
                self.assertTrue(all(value.requires_grad and value.grad is None for value in model.parameters()))

    def test_actual_valid_and_rejected_checkpoint_load_require_reset(self):
        from q2_local_guidance_model import Q2LocalGuidanceModel
        model=Q2LocalGuidanceModel().eval();stream=Q2PCMStream(model);wave=self.wave(111)
        stream.push(wave);saved={k:v.clone() for k,v in model.state_dict().items()}
        model.load_state_dict(saved)
        with self.assertRaises(ValueError):stream.push(wave)
        stream.reset();stream.push(wave)
        bad={k:v.clone() for k,v in saved.items()};bad['mask_head'][0,0,0]=float('nan')
        with self.assertRaises(ValueError):model.load_state_dict(bad)
        with self.assertRaises(ValueError):stream.finish()
        self.assertEqual(stream.received,111)
        stream.reset();self.assertFalse(stream.push(self.wave(1200))['sources'].any())


if __name__=='__main__':unittest.main()
