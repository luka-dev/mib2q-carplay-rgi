"""Original-only base/power PCM reference tests, synthetic data only."""
import inspect
import json
import unittest
from unittest import mock

import torch

from q2_local_guidance_model import Q2Config,Q2LocalGuidanceModel
from q2_power_guidance_model import Q2PowerGuidanceModel
from q2_local_guidance_audio import analyze,synthesize,META_SHAPES,TIMELINE_KEYS,render_audio
from q2_pcm_stream import Q2OriginalPCMStream,render_original_audio,DELAY


def model(kind='base',rate=48000,k=3):
    torch.manual_seed(6419)
    config=Q2Config(sample_rate=rate,width=12,band_hidden=24,
                    classes=tuple('source_'+str(i) for i in range(k)))
    result=Q2LocalGuidanceModel(config) if kind=='base' else Q2PowerGuidanceModel(
        config,prior=torch.ones(k)/k,carrier_mode=kind)
    with torch.no_grad():
        for name,p in result.named_parameters():
            if name in ('mask_head','refinement_head','hint_film.weight','context_film.weight'):
                p.copy_(torch.randn_like(p)*.025)
        if kind=='base':
            result.mask_bias[:,0]=torch.linspace(.1,.7,k)
            result.mask_bias[:,2]=torch.linspace(.1,.7,k)
    return result.eval()


def wave(n):return torch.randn(n,2,generator=torch.Generator().manual_seed(n+7741))*.07


class Q2OriginalPCMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.threads=torch.get_num_threads();torch.set_num_threads(2)
    @classmethod
    def tearDownClass(cls):torch.set_num_threads(cls.threads)

    def exact(self,a,b):
        self.assertEqual(a.dtype,b.dtype);self.assertEqual(a.shape,b.shape)
        self.assertEqual(a.contiguous().numpy().tobytes(),b.contiguous().numpy().tobytes())

    def stream(self,m,x,chunks,mode='predicted'):
        stream=Q2OriginalPCMStream(m,mode);blocks=[];position=step=0
        while position<len(x):
            n=min(chunks[step%len(chunks)],len(x)-position)
            blocks.append(stream.process(x[position:position+n]));position+=n;step+=1
        blocks.append(stream.flush())
        return stream,blocks

    def test_all_three_models_rates_and_modes_match_full_vectorized_reference(self):
        x=wave(2309)
        for kind in ('base','film_control','direct_power'):
            for rate in (44100,48000):
                m=model(kind,rate)
                for mode in ('predicted','disabled'):
                    with self.subTest(kind=kind,rate=rate,mode=mode):
                        original=x.clone();flags=[p.requires_grad for p in m.parameters()]
                        with torch.no_grad():
                            analysis=analyze(x,m.config)
                            outputs,_=m(analysis.frames_ri,guidance_mode=mode)
                            expected=synthesize(outputs['sources_ri'],len(x),m.config)
                        result=render_original_audio(m,x,mode,chunk_samples=517)
                        torch.testing.assert_close(result['sources'],expected,rtol=3e-5,atol=3e-7)
                        self.assertTrue((result['sources'].square().sum((0,2))>0).all())
                        self.exact(result['delayed_original'],x)
                        self.exact(result['residual'],x-result['sources'].sum(1))
                        self.exact(x,original)
                        self.assertEqual(flags,[p.requires_grad for p in m.parameters()])
                        self.assertTrue(all(p.grad is None for p in m.parameters()))
                        self.assertFalse(result['sources'].requires_grad)
                        self.assertEqual(result['model_metadata']['model_type'],type(m).__name__)

    def test_external_chunks_exact_and_old_audio_dsp_reused(self):
        x=wave(4099);m=model('direct_power')
        _,reference=self.stream(m,x,[len(x)])
        for chunks in ([1,17,255,4093],[257,3,9,511]):
            _,actual=self.stream(m,x,chunks)
            for name in ('sources','delayed_original','residual',*META_SHAPES,'updated',*TIMELINE_KEYS):
                self.exact(torch.cat([b[name] for b in reference]),torch.cat([b[name] for b in actual]))
        old=render_audio(m,x,chunk_samples=4093)
        new=render_original_audio(m,x,chunk_samples=4093)
        self.exact(old['sources'],new['sources']);self.exact(old['residual'],new['residual'])

    def test_exact_original_delay_timeline_flush_and_short_lengths(self):
        m=model()
        for n in (1,255,256,257,1024,1049):
            x=wave(n);stream,blocks=self.stream(m,x,[17,255,3])
            self.assertEqual(stream.received,n);self.assertEqual(stream.emitted,n+DELAY)
            self.assertEqual(len(blocks[-1]['sources']),DELAY)
            self.assertTrue(blocks[-1]['timeline']['flush'])
            dry=torch.cat([b['delayed_original'] for b in blocks])
            self.exact(dry,torch.cat((torch.zeros(DELAY,2),x)))
            self.assertFalse(torch.cat([b['sources'] for b in blocks])[:DELAY].any())
            source_pieces=[];target_pieces=[];last=0
            for block in blocks:
                t=block['timeline'];json.dumps(t,allow_nan=False)
                self.assertEqual(t['output_start'],last);last=t['output_stop']
                self.assertEqual(t['original_start'],t['output_start']-DELAY)
                lo,hi=t['valid_output_slice']
                self.assertEqual(hi-lo,t['valid_original_stop']-t['valid_original_start'])
                source_pieces.append(block['sources'][lo:hi])
                target_pieces.append(x[t['valid_original_start']:t['valid_original_stop']])
            self.assertEqual(len(torch.cat(source_pieces)),n)
            self.exact(torch.cat(target_pieces),x)
            with self.assertRaises(ValueError):stream.flush()
            with self.assertRaises(ValueError):stream.process(torch.empty(0,2))

    def test_twelve_nonzero_heads_one_input_and_no_oracle_target_argument(self):
        m=model('direct_power',k=12);x=wave(777)
        self.assertEqual(list(inspect.signature(Q2OriginalPCMStream.process).parameters),['self','original'])
        with mock.patch.object(m,'forward_oracle_train',side_effect=AssertionError('oracle called')):
            with mock.patch.object(m,'forward',wraps=m.forward) as spy:
                result=render_original_audio(m,x,chunk_samples=111)
                for call in spy.call_args_list:
                    args,kwargs=call
                    self.assertEqual(len(args),2)
                    self.assertEqual(args[0].shape,(1,1,2,513,2))
                    self.assertEqual(kwargs,dict(guidance_mode='predicted'))
        self.assertTrue((result['sources'].square().sum((0,2))>0).all())
        self.assertEqual(result['source_role_status'],'UNKNOWN')
        with self.assertRaises(TypeError):Q2OriginalPCMStream(m).process(x,targets=x)
        with self.assertRaises(ValueError):Q2OriginalPCMStream(m,'oracle_train')

    def test_reset_binding_mode_and_initial_silence(self):
        m=model('film_control');x=wave(721);digest='a'*64
        stream=Q2OriginalPCMStream(m,checkpoint_sha256=digest)
        first=stream.process(x);stream.flush();stream.reset(checkpoint_sha256=digest)
        again=stream.process(x)
        self.exact(first['sources'],again['sources']);self.exact(first['delayed_original'],again['delayed_original'])
        self.assertEqual(again['checkpoint_sha256'],digest)
        stream.reset(guidance_mode='disabled')
        self.assertEqual(stream.guidance_mode,'disabled')
        self.assertIsNone(stream.process(x)['checkpoint_sha256'])
        stream.reset();zero=stream.process(torch.zeros(5003,2));tail=stream.flush()
        self.assertFalse(zero['sources'].any());self.assertFalse(tail['sources'].any())
        self.assertFalse(zero['delayed_original'].any());self.assertFalse(tail['delayed_original'].any())
        empty=Q2OriginalPCMStream(m)
        self.assertEqual(empty.process(torch.empty(0,2))['sources'].shape,(0,3,2))
        self.assertEqual(empty.flush()['sources'].shape,(0,3,2))
        self.assertTrue(empty.ended)

    def test_future_suffix_changes_cannot_change_prior_output(self):
        m=model('direct_power');x=wave(5003);a=Q2OriginalPCMStream(m);b=Q2OriginalPCMStream(m)
        pa,pb=a.process(x[:1701]),b.process(x[:1701]);saved=pa['sources'].clone()
        for name in ('sources','delayed_original','residual',*META_SHAPES,'updated'):
            self.exact(pa[name],pb[name])
        a.process(x[1701:]);b.process(-7*x[1701:]);a.flush();b.flush()
        self.exact(pa['sources'],saved)

    def test_invalid_input_device_precision_training_rate_capacity_and_subclass(self):
        m=model();stream=Q2OriginalPCMStream(m,max_chunk_samples=1024)
        stream.process(wave(111));old_dry=stream._dry.clone();old=stream._snapshot()
        for invalid in (torch.zeros(2,3),torch.zeros(4,2,dtype=torch.float64),
            torch.tensor([[float('nan'),0.]]),torch.zeros(1,2,device='meta'),torch.zeros(1025,2)):
            with self.assertRaises(ValueError):stream.process(invalid)
            self.assertIs(stream._snapshot()[0],old[0]);self.assertEqual(stream.received,111)
            self.exact(stream._dry,old_dry)
        with self.assertRaises(ValueError):Q2OriginalPCMStream(model().train())
        with self.assertRaises(ValueError):Q2OriginalPCMStream(model().double())
        with self.assertRaises(ValueError):Q2OriginalPCMStream(model().to('meta'))
        with self.assertRaises(ValueError):Q2OriginalPCMStream(model(),sample_rate=44100)
        with self.assertRaises(ValueError):Q2OriginalPCMStream(model(),checkpoint_sha256='wrong')
        class Foreign(Q2LocalGuidanceModel):pass
        with self.assertRaises(ValueError):Q2OriginalPCMStream(Foreign(m.config).eval())
        for bad in (0,65537,True):
            with self.assertRaises(ValueError):Q2OriginalPCMStream(m,max_chunk_samples=bad)

    def test_model_successful_failed_reload_parameter_and_flag_mutation_require_reset(self):
        for kind in ('base','direct_power'):
            m=model(kind);stream=Q2OriginalPCMStream(m);stream.process(wave(111))
            m.load_state_dict(m.state_dict())
            with self.assertRaises(ValueError):stream.process(torch.empty(0,2))
            self.assertEqual(stream.received,111);stream.reset()
            with self.assertRaises(Exception):m.load_state_dict({})
            with self.assertRaises(ValueError):stream.process(torch.empty(0,2))
            stream.reset()
            with torch.no_grad():m.mask_bias.add_(.01)
            with self.assertRaises(ValueError):stream.process(torch.empty(0,2))
            stream.reset();m.mask_bias.requires_grad_(False)
            with self.assertRaises(ValueError):stream.process(torch.empty(0,2))
            stream.reset();self.assertFalse(m.mask_bias.requires_grad)
            stream.process(wave(513));self.assertFalse(m.mask_bias.requires_grad)

    def test_bad_model_numeric_suffix_and_added_residual_failure_roll_back_both_states(self):
        m=model();stream=Q2OriginalPCMStream(m);x=wave(517)
        stream.process(x[:111]);old=stream._snapshot();dry=stream._dry.clone()
        # Large finite audio is not itself forbidden/clipped; use a value which
        # genuinely overflows the original FFT, not an invented amplitude cap.
        with self.assertRaises(ValueError):stream.process(torch.full((1024,2),torch.finfo(torch.float32).max))
        self.assertIs(stream._snapshot()[0],old[0]);self.assertEqual(stream.received,111);self.exact(stream._dry,dry)
        original_push=stream._stream.push
        def overflow(waveform):
            block=original_push(waveform);block['sources'].fill_(torch.finfo(torch.float32).max)
            return block
        with mock.patch.object(stream._stream,'push',side_effect=overflow):
            with self.assertRaisesRegex(ValueError,'residual'):stream.process(x[111:])
        self.assertIs(stream._snapshot()[0],old[0]);self.assertEqual(stream.received,111);self.exact(stream._dry,dry)
        good=Q2OriginalPCMStream(m);good.process(x[:111])
        self.exact(stream.process(x[111:])['sources'],good.process(x[111:])['sources'])
        old=stream._snapshot();dry=stream._dry.clone();original_finish=stream._stream.finish
        def bad_finish():
            block=original_finish();block['sources'].fill_(torch.finfo(torch.float32).max)
            return block
        with mock.patch.object(stream._stream,'finish',side_effect=bad_finish):
            with self.assertRaisesRegex(ValueError,'residual'):stream.flush()
        self.assertFalse(stream.ended);self.assertIs(stream._snapshot()[0],old[0]);self.exact(stream._dry,dry)
        self.exact(stream.flush()['sources'],good.flush()['sources'])


if __name__=='__main__':unittest.main()
