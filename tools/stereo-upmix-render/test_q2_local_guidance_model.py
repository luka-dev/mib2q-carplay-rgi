"""Synthetic Q2 numerical/causal qualification; not source or car approval."""
from dataclasses import replace
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

import torch
from torch.nn import functional as F

from q2_local_guidance_model import (Q2Config,Q2LocalGuidanceModel,Q2State,CLASSES,
                                     ORACLE_TOKEN,supervised_guidance_targets)


def config(**kwargs):
    return Q2Config(width=12,band_hidden=24,classes=('lead_vocal','backing_vocal','guitar'),**kwargs)


def spectra(batch=1,steps=19):
    x=torch.randn(batch,steps,2,513,2)*.3
    x[...,0,1]=0;x[...,-1,1]=0
    return x


def nontrivial():
    torch.manual_seed(1234);model=Q2LocalGuidanceModel(config())
    with torch.no_grad():
        model.mask_head.uniform_(-.1,.1);model.refinement_head.uniform_(-.1,.1)
    return model


class Q2ModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.threads=torch.get_num_threads();torch.set_num_threads(2)

    @classmethod
    def tearDownClass(cls):torch.set_num_threads(cls.threads)

    def close(self,a,b):
        if a.dtype==torch.bool:self.assertTrue(torch.equal(a,b))
        else:torch.testing.assert_close(a,b,rtol=3e-5,atol=3e-6)

    def test_config_shapes_initial_zero_and_accounted_reference_and_student(self):
        for width,hidden in ((24,48),(12,24)):
            model=Q2LocalGuidanceModel(Q2Config(width=width,band_hidden=hidden))
            out,state=model(spectra(1,9));spec=model.specification()
            self.assertEqual(out['sources_ri'].shape,(1,9,12,2,513,2))
            self.assertEqual(out['descriptors'].shape,(1,9,12,32,5))
            self.assertEqual(out['activity_logits'].shape,(1,9,12,32))
            self.assertEqual(out['updated'].nonzero().tolist(),[[0,3],[0,7]])
            self.assertFalse(out['sources_ri'].any())
            self.assertEqual(spec['parameters'],sum(p.numel() for p in model.parameters()))
            fields=(state.previous_log_power,state.previous_power_valid,*state.encoder_histories,state.block_sum,
                    state.band_hidden,state.held_context,state.held_raw_guidance,state.oracle_sum,state.held_oracle)
            self.assertEqual(spec['state_bytes_per_batch'],sum(t.numel()*t.element_size() for t in fields))
            self.assertEqual(spec['dense_mac_per_frame'],sum(spec['dense_mac_fast'].values())
                             +sum(spec['dense_mac_per_control'].values())/4)
            self.assertEqual(spec['direct_audio_receptive_frames'],31)
            self.assertFalse(spec['car_approved']);self.assertEqual(spec['role_status'],'UNKNOWN')
            self.assertIsNone(spec['total_mask_amplitude_bound'])
            self.assertEqual(tuple(spec['classes']),CLASSES)
            self.assertTrue(all(p.dtype==torch.float32 for p in model.parameters()))
        for kwargs in (dict(width=8),dict(band_hidden=24),dict(bands=16),dict(update_hops=2),
                       dict(sample_rate=32000),dict(classes=['lead']),dict(classes=('lead','lead'))):
            with self.assertRaises(ValueError):Q2Config(**kwargs)

    def test_static_coefficient_exact_positive_negative_scalar_and_zero_silence(self):
        model=Q2LocalGuidanceModel(config());x=spectra(2,13)
        gains=torch.tensor([.35,-3.25,4.])
        with torch.no_grad():
            model.mask_bias[:,0]=gains;model.mask_bias[:,2]=gains
            out,state=model(x)
            expected=x[:,:,None]*gains[None,None,:,None,None,None]
            self.assertTrue(torch.equal(out['sources_ri'],expected))
            self.assertTrue(torch.equal(out['extraction_ri'],expected))
            self.assertFalse(out['refinement_ri'].any())
            silent,_=model(torch.zeros_like(x))
            self.assertFalse(silent['sources_ri'].any())
            # Bias cannot emit a signal even after real input: direct carrier0,
            # and the deliberately zero residual head does not synthesize tail.
            tail,_=model(torch.zeros_like(x),state)
            self.assertFalse(tail['sources_ri'].any())
            state=None;pieces=[];pos=0
            for size in (1,2,5,5):
                chunk,state=model(x[:,pos:pos+size],state);pos+=size;pieces.append(chunk['sources_ri'])
            self.assertTrue(torch.equal(torch.cat(pieces,1),expected))
            reloaded=Q2LocalGuidanceModel(model.config);reloaded.load_state_dict(model.state_dict())
            replay,_=reloaded(x);self.assertTrue(torch.equal(replay['sources_ri'],expected))
        model.zero_grad(set_to_none=True)
        out,_=model(x);out['sources_ri'].square().mean().backward()
        self.assertTrue(torch.isfinite(model.mask_bias.grad).all())
        self.assertTrue((model.mask_bias.grad[:,(0,2)].abs()>0).all())

    def test_v1_preservation_rng_core_initial_and_nonzero_zero_bias_parity(self):
        root=Path(__file__).resolve().parents[2]
        snapshot=root/'output/q2-guidance-v1-preserved.v1Gk5w/q2_local_guidance_model.py'
        # The actual immutable local v1 artifact is optional in a small remote
        # source bundle. Its SHA is fixed, never silently replaced by live v2.
        if not snapshot.exists():self.skipTest('Preserved v1 body not included in this source-only environment')
        import hashlib
        self.assertEqual(hashlib.sha256(snapshot.read_bytes()).hexdigest(),
                         '64e21e66fd7f63e48f3158ee0febe44b13e13b7fc4cbb511ec3782b1b3afc064')
        spec=importlib.util.spec_from_file_location('_q2_preserved_v1_test',snapshot)
        old=importlib.util.module_from_spec(spec);sys.modules[spec.name]=old
        exec(compile(snapshot.read_bytes(),str(snapshot),'exec'),old.__dict__)
        torch.manual_seed(5312)
        v1=old.Q2LocalGuidanceModel(old.Q2Config(width=12,band_hidden=24,classes=config().classes))
        rng=torch.get_rng_state().clone()
        torch.manual_seed(5312);v2=Q2LocalGuidanceModel(config())
        self.assertTrue(torch.equal(rng,torch.get_rng_state()))
        for name,value in v1.state_dict().items():self.assertTrue(torch.equal(value,v2.state_dict()[name]),name)
        self.assertFalse(v2.mask_bias.any())
        with torch.no_grad():
            for name,value in v1.named_parameters():
                value.uniform_(-.07,.07);v2.state_dict()[name].copy_(value)
        x=spectra(1,13)
        a,_=v1(x);b,_=v2(x)
        for key in a:self.assertTrue(torch.equal(a[key],b[key]),key)
        loss1=a['sources_ri'].square().mean()+a['descriptors'].square().mean()
        loss2=b['sources_ri'].square().mean()+b['descriptors'].square().mean()
        loss1.backward();loss2.backward()
        newer=dict(v2.named_parameters())
        for name,value in v1.named_parameters():
            self.assertTrue(torch.equal(value.grad,newer[name].grad),name)

    def test_chunks_every_phase_batch_and_future_suffix(self):
        model=nontrivial();x=spectra(2,23)
        with torch.no_grad():
            whole,ws=model(x)
            for sizes in ((1,)*23,(2,3,7,1,10),(4,8,11)):
                state=None;pos=0;parts={key:[] for key in whole}
                for size in sizes:
                    result,state=model(x[:,pos:pos+size],state);pos+=size
                    for key in parts:parts[key].append(result[key])
                for key in parts:self.close(whole[key],torch.cat(parts[key],1))
                self.assertEqual(state.frames,23)
                for name in ('block_sum','band_hidden','held_context','held_raw_guidance','previous_log_power'):
                    self.close(getattr(ws,name),getattr(state,name))
                for a,b in zip(ws.encoder_histories,state.encoder_histories):self.close(a,b)
            for cut in (1,2,3,4,5,6,7,8,15,22):
                changed=x.clone();changed[:,cut:]=spectra(2,23-cut)*4
                candidate,_=model(changed)
                for key in whole:self.assertTrue(torch.equal(whole[key][:,:cut],candidate[key][:,:cut]),key)
            for index in range(2):
                single,_=model(x[index:index+1])
                for key in whole:self.close(single[key],whole[key][index:index+1])

    def test_disabled_keeps_context_detector_and_never_hard_gates_sources(self):
        model=nontrivial();x=spectra()
        with torch.no_grad():
            guided,gs=model(x);disabled,ds=model(x,guidance_mode='disabled')
            for key in ('activity_logits','activity','descriptors','log_error_scale','band_features','updated'):
                self.assertTrue(torch.equal(guided[key],disabled[key]))
            for name in ('block_sum','band_hidden','held_context','held_raw_guidance'):
                self.assertTrue(torch.equal(getattr(gs,name),getattr(ds,name)))
            self.assertGreater(float((guided['sources_ri']-disabled['sources_ri']).abs().max()),0)
            # Learned absence is information, not permission to zero a decoder.
            model.descriptor_head.weight.zero_();model.descriptor_head.bias.reshape(3,7)[:,0]=-100
            result,_=model(x)
            self.assertFalse(result['activity'][:,3:].any());self.assertTrue(result['sources_ri'][:,3:].any())
            self.assertTrue((result['guidance_scale']>=.5).all());self.assertTrue((result['guidance_scale']<=1.5).all())

    def test_residual_recovers_empty_current_bin_from_actual_past_neighbor(self):
        model=Q2LocalGuidanceModel(config())
        with torch.no_grad():
            for value in model.parameters():value.zero_()
            model.frequency_encoder.weight[0,0,4]=1
            for kernel in model.temporal_kernels:kernel[0,0]=1
            model.audio_mix.weight[0,0,0]=1
            model.refinement_head[0,0,0]=1
            x=torch.zeros(1,33,2,513,2);x[0,0,0,100,0]=2
            out,_=model(x)
            self.assertEqual(float(out['refinement_ri'][0,2,0,0,98,0]),2)
            self.assertEqual(float(out['refinement_ri'][0,30,0,0,98,0]),2)
            self.assertFalse(out['refinement_ri'][:,31:].any())
            self.assertEqual(float(x[0,2,0,98,0]),0)
            self.assertFalse(out['extraction_ri'].any())
            self.assertFalse(out['sources_ri'][...,0,1].any());self.assertFalse(out['sources_ri'][...,-1,1].any())
            state=None;pieces=[];start=0
            for size in (5,7,10,11):
                part,state=model(x[:,start:start+size],state);start+=size;pieces.append(part['sources_ri'])
            self.assertTrue(torch.equal(out['sources_ri'],torch.cat(pieces,1)))

    def test_source_local_targets_power_flux_spatial_known_and_block_alignment(self):
        c=config();x=torch.zeros(1,9,2,513,2);x[...,100,0]=10
        s=torch.stack((x*.5,torch.zeros_like(x),x*2),2)
        target=supervised_guidance_targets(x,s,c)
        band=next(i for i,(a,b) in enumerate(zip(Q2LocalGuidanceModel(c).band_edges[:-1],
                                                Q2LocalGuidanceModel(c).band_edges[1:])) if a<=100<b)
        raw=target['raw_guidance'];self.close(raw[0,:,0,band,1],torch.full((9,),.2))
        self.close(raw[0,:,2,band,1],torch.full((9,),.8))
        self.assertEqual(float(raw[0,0,0,band,2]),1)
        self.assertFalse(raw[0,1:,0,band,2].any())
        self.assertFalse(target['descriptor_known'][0,:,1,:,2:].any())
        self.assertTrue(target['activity_known'].all()) # dataset known mask still mandatory externally
        self.assertFalse(target['block_guidance'][:,:3].any())
        self.assertEqual(float(target['block_guidance'][0,3,0,band,2]),.25)
        self.assertEqual(target['updated'].nonzero().tolist(),[[0,3],[0,7]])
        # Destructive cancellation X=0 does not invent relative power1 or mark a
        # real reference source inactive. It remains target-only information.
        cancelled=supervised_guidance_targets(x*0,s,c)
        self.assertEqual(float(cancelled['raw_guidance'][0,0,0,band,0]),1)
        for value in target.values():self.assertTrue(torch.isfinite(value).all())

    def test_oracle_is_separate_train_only_causal_and_target_detached(self):
        model=nontrivial();x=spectra(1,13);s=x[:,:,None].expand(-1,-1,3,-1,-1,-1)*.2
        guide=supervised_guidance_targets(x,s,model.config)['raw_guidance'].requires_grad_()
        with self.assertRaises(TypeError):model(x,raw_guidance=guide)
        with self.assertRaises(ValueError):model(x,guidance_mode='oracle_train')
        for partition,token in (('assessment',ORACLE_TOKEN),('train','wrong')):
            with self.assertRaises(ValueError):model.forward_oracle_train(x,guide,partition=partition,diagnostic_token=token)
        model.eval()
        with self.assertRaises(ValueError):model.forward_oracle_train(x,guide,partition='train',diagnostic_token=ORACLE_TOKEN)
        model.train();full,_=model.forward_oracle_train(x,guide,partition='train',diagnostic_token=ORACLE_TOKEN)
        ordinary,_=model(x)
        self.assertTrue(torch.equal(full['sources_ri'][:,:3],ordinary['sources_ri'][:,:3]))
        full['sources_ri'].square().mean().backward();self.assertIsNone(guide.grad)
        with torch.no_grad():
            state=None;rows=[];pos=0
            for size in (1,2,5,1,4):
                out,state=model.forward_oracle_train(x[:,pos:pos+size],guide[:,pos:pos+size],state,
                                                     partition='train',diagnostic_token=ORACLE_TOKEN)
                pos+=size;rows.append(out['sources_ri'])
            self.close(full['sources_ri'],torch.cat(rows,1))
            changed=guide.detach().clone();changed[:,5:,:, :,0]=1-changed[:,5:,:,:,0]
            other,_=model.forward_oracle_train(x,changed,partition='train',diagnostic_token=ORACLE_TOKEN)
            self.assertTrue(torch.equal(full['sources_ri'][:,:7],other['sources_ri'][:,:7]))

    def test_zero_history_finite_backward_tiny_and_common_gain(self):
        model=nontrivial();zero=torch.zeros(1,9,2,513,2,requires_grad=True)
        out,state=model(zero)
        self.assertFalse(out['sources_ri'].any())
        loss=out['sources_ri'].square().mean()+out['descriptors'].square().mean()+out['activity_logits'].square().mean()
        loss.backward();self.assertTrue(torch.isfinite(zero.grad).all())
        self.assertTrue(all(p.grad is None or torch.isfinite(p.grad).all() for p in model.parameters()))
        with torch.no_grad():
            x=spectra(1,13);a,_=model(x)
            for gain in (.25,3,1e-20):
                b,_=model(x*gain)
                torch.testing.assert_close(b['sources_ri']/gain,a['sources_ri'],rtol=2e-4,atol=2e-5)
                self.assertTrue(all(torch.isfinite(v).all() for v in b.values()))
            for count in (1,3,7):
                continuation,state=model(torch.zeros(1,count,2,513,2),state)
                self.assertFalse(continuation['sources_ri'].any())

    def test_all_branches_gradients_and_short_synthetic_update(self):
        torch.manual_seed(45);model=Q2LocalGuidanceModel(config());x=spectra(2,13)
        target=x[:,:,None]*torch.tensor([.1,.2,.3])[None,None,:,None,None,None]
        truth=supervised_guidance_targets(x,target,model.config)['block_guidance']
        optimizer=torch.optim.Adam(model.parameters(),lr=.002)
        seen={name:False for name,_ in model.named_parameters()}
        before={name:p.detach().clone() for name,p in model.named_parameters()}
        for _ in range(5):
            optimizer.zero_grad(set_to_none=True);out,_=model(x);updated=out['updated']
            loss=(out['sources_ri']-target).square().mean()
            loss=loss+F.binary_cross_entropy_with_logits(out['activity_logits'][updated],truth[...,0][updated])
            loss=loss+(out['descriptors'][updated]-truth[...,1:][updated]).square().mean()
            loss=loss+(out['log_error_scale'][updated]-.2).square().mean()
            loss.backward()
            for name,p in model.named_parameters():
                self.assertIsNotNone(p.grad,name);self.assertTrue(torch.isfinite(p.grad).all(),name)
                seen[name]|=bool(p.grad.abs().max()>0)
            optimizer.step()
        self.assertTrue(all(seen.values()),[name for name,v in seen.items() if not v])
        self.assertTrue(all(not torch.equal(before[name],p) for name,p in model.named_parameters()))

    def test_owned_detached_state_serialization_exact_weights_and_reload(self):
        model=nontrivial();x=spectra(1,12).requires_grad_();_,state=model(x[:,:5])
        detached=state.detach();self.assertFalse(detached.encoder_histories[0].requires_grad)
        self.assertNotEqual(detached.encoder_histories[0].data_ptr(),state.encoder_histories[0].data_ptr())
        out,_=model(x[:,5:],detached);out['sources_ri'].sum().backward()
        self.assertFalse(x.grad[:,:5].any());self.assertTrue(x.grad[:,5:].any())
        payload=model.state_to_payload(detached);other=Q2LocalGuidanceModel(model.config)
        other.load_state_dict(model.state_dict());rebound=other.state_from_payload(payload)
        with torch.no_grad():
            a,_=model(x[:,5:],detached);b,_=other(x[:,5:],rebound)
            for key in a:self.assertTrue(torch.equal(a[key],b[key]),key)
        corrupt=copy.deepcopy(payload);corrupt['tensors']['band_hidden'].fill_(float('nan'))
        with self.assertRaises(ValueError):other.state_from_payload(corrupt)
        with torch.no_grad():other.mask_head.add_(.01)
        with self.assertRaises(ValueError):other.state_from_payload(payload)
        generation=model.generation;weights={name:p.clone() for name,p in model.state_dict().items()}
        broken={name:p.clone() for name,p in weights.items()};broken['mask_head'].flatten()[0]=float('nan')
        with self.assertRaises(ValueError):model.load_state_dict(broken)
        self.assertEqual(model.generation,generation+1)
        self.assertTrue(all(torch.equal(value,model.state_dict()[name]) for name,value in weights.items()))
        with self.assertRaises(ValueError):model(x[:,5:],detached)
        model.load_state_dict(weights);model(x[:,5:])

    def test_invalid_inputs_state_and_suffix_are_transactional(self):
        model=nontrivial();x=spectra();_,state=model(x[:,:5]);saved=state.detach()
        for bad in (x.double(),x[:, :0],x.reshape(-1),x.clone()):
            if bad.shape==x.shape and bad.dtype==x.dtype:bad[:, -1,0,200,0]=float('nan')
            with self.assertRaises(ValueError):model(bad,state)
        endpoint=x.clone();endpoint[...,0,1]=.1
        with self.assertRaises(ValueError):model(endpoint,state)
        for badstate in (replace(state,owner=object()),replace(state,frames=-1),
                         replace(state,block_sum=state.block_sum.double()),replace(state,mode='disabled')):
            with self.assertRaises(ValueError):model(x,badstate)
        self.assertTrue(torch.equal(state.block_sum,saved.block_sum))
        for a,b in zip(state.encoder_histories,saved.encoder_histories):self.assertTrue(torch.equal(a,b))

    @unittest.skipUnless(torch.backends.mps.is_available(),'MPS unavailable; CPU tests do not claim MPS qualification')
    def test_actual_mps_float32_forward_backward_and_cpu_numeric_smoke(self):
        cpu=nontrivial();mps=Q2LocalGuidanceModel(cpu.config).to('mps');mps.load_state_dict(cpu.state_dict())
        x=spectra(1,9);reference,_=cpu(x)
        device_x=x.to('mps').requires_grad_();candidate,_=mps(device_x)
        loss=candidate['sources_ri'].square().mean()+candidate['descriptors'].square().mean()
        loss.backward();torch.mps.synchronize()
        self.assertTrue(torch.isfinite(device_x.grad).all())
        self.assertTrue(all(p.grad is None or torch.isfinite(p.grad).all() for p in mps.parameters()))
        torch.testing.assert_close(candidate['sources_ri'].cpu(),reference['sources_ri'],rtol=3e-4,atol=3e-5)


if __name__=='__main__':unittest.main()
