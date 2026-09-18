"""Synthetic math/state tests; no music fit, role or vehicle approval."""
import copy
import inspect
import unittest

import numpy as np
import torch

from q2_local_guidance_model import Q2Config,Q2LocalGuidanceModel,ORACLE_TOKEN
from q2_power_guidance_model import Q2PowerGuidanceModel,normalized_fraction_odds

torch.set_num_threads(2)
C=Q2Config(classes=('lead','support','other'),width=12,band_hidden=24)
P=(.6,.3,.1)

def audio(n=19):
    x=torch.randn(1,n,2,513,2)*.1;x[...,0,1]=0.;x[...,-1,1]=0.;return x

def model(mode='direct_power',prior=P):return Q2PowerGuidanceModel(C,prior=prior,carrier_mode=mode)

def oracle(m,x,g,state=None):
    return m.forward_oracle_train(x,g,state,partition='train',diagnostic_token=ORACLE_TOKEN)


class PowerGuidanceTests(unittest.TestCase):
    def test_initialization_same_core_rng_and_prior_carrier(self):
        torch.manual_seed(921);base=Q2LocalGuidanceModel(C);rng=torch.get_rng_state().clone()
        torch.manual_seed(921);film=model('film_control');self.assertTrue(torch.equal(torch.get_rng_state(),rng))
        torch.manual_seed(921);direct=model('direct_power');self.assertTrue(torch.equal(torch.get_rng_state(),rng))
        for (n,a),(n2,b) in zip(film.named_parameters(),direct.named_parameters()):
            self.assertEqual(n,n2);self.assertTrue(torch.equal(a,b),n)
            if n not in ('mask_bias','descriptor_head.weight','descriptor_head.bias'):self.assertTrue(torch.equal(a,dict(base.named_parameters())[n]),n)
        indices=torch.arange(3)*7+1;other=[i for i in range(21) if i not in indices]
        self.assertTrue(torch.equal(base.descriptor_head.weight[other],direct.descriptor_head.weight[other]))
        self.assertTrue(torch.equal(base.descriptor_head.bias[other],direct.descriptor_head.bias[other]))
        x=audio();expected=x[:,:,None]*torch.tensor(P)[None,None,:,None,None,None]
        for guidance in ('predicted','disabled'):
            a,_=film(x,guidance_mode=guidance);b,_=direct(x,guidance_mode=guidance)
            torch.testing.assert_close(a['sources_ri'],expected,rtol=3e-6,atol=3e-7)
            torch.testing.assert_close(b['sources_ri'],expected,rtol=3e-6,atol=3e-7)
            if guidance=='disabled':
                for key in a:self.assertEqual(a[key].detach().numpy().tobytes(),b[key].detach().numpy().tobytes(),key)
        self.assertNotEqual(film._state_weight_sha(),direct._state_weight_sha())
        self.assertEqual(direct.specification()['additional_trainable_parameters'],0)

    def test_endpoints_normalized_odds_and_finite_derivative(self):
        f=torch.tensor([.25,.5,.75],dtype=torch.float32)[None,None,:,None].requires_grad_()
        w=normalized_fraction_odds(f);torch.testing.assert_close(w[0,0,:,0],torch.tensor([1.,3.,9.])/13)
        (w*torch.tensor([1.,2.,4.])[None,None,:,None]).sum().backward();self.assertTrue(torch.isfinite(f.grad).all());self.assertGreater(float(f.grad.abs().sum()),0)
        for values,expected in [([0.,0.,0.],[0.,0.,0.]),([1.,1.,.5],[.5,.5,0.]),([1.,0.,0.],[1.,0.,0.])]:
            v=torch.tensor(values)[None,None,:,None];self.assertTrue(torch.equal(normalized_fraction_odds(v)[0,0,:,0],torch.tensor(expected)))
            predicted=normalized_fraction_odds(v,predicted=True);self.assertTrue(torch.isfinite(predicted).all())
            torch.testing.assert_close(predicted.sum(2),torch.ones_like(predicted.sum(2)))
        with self.assertRaises(ValueError):normalized_fraction_odds(torch.tensor([float('nan')])[None,None,:,None])

    def test_oracle_matches_independent_held_analytic_witness(self):
        torch.manual_seed(4);m=model();x=audio(17);g=torch.rand(1,17,3,32,6)
        g[:,4:8,:,0,1]=torch.tensor([1.,0.,0.]);g[:,8:12,:,1,1]=0.
        out,_=oracle(m,x,g)
        raw=g.detach().numpy();held=np.zeros((1,17,3,32),dtype=np.float64)
        for t in range(17):
            if t>=3:
                end=(t+1)//4*4;held[:,t]=raw[:,end-4:end,...,1].mean(1)
        exclusive=held>=1.;count=exclusive.sum(2,keepdims=True)
        odds=held/np.where(held<1,1-held,1);total=odds.sum(2,keepdims=True)
        w=np.where(count>0,exclusive/np.maximum(count,1),odds/np.where(total>0,total,1));w[:,:3]=np.asarray(P)[None,None,:,None]
        widths=np.diff(m.band_edges);expanded=np.repeat(w,widths,axis=-1)
        expected=x[:,:,None]*torch.tensor(expanded,dtype=torch.float32)[:,:,:,None,:,None]
        torch.testing.assert_close(out['sources_ri'],expected,rtol=3e-6,atol=3e-7)
        self.assertFalse(out['refinement_ri'].any())
        self.assertTrue(torch.equal(out['power_carrier_weights'][:,:3],m.power_prior[None,None,:,None].expand(1,3,3,32)))

    def test_disabled_scalar_bias_submodel_and_original_grounding(self):
        m=model();x=audio();gains=torch.tensor([-1.7,0.,3.2])
        with torch.no_grad():m.mask_bias.zero_();m.mask_bias[:,0]=gains;m.mask_bias[:,2]=gains
        out,_=m(x,guidance_mode='disabled')
        expected=x[:,:,None]*gains[None,None,:,None,None,None]
        self.assertTrue(torch.equal(out['sources_ri'],expected))
        for mode in ('film_control','direct_power'):
            m=model(mode)
            with torch.no_grad():m.mask_head.normal_(0,.1);m.refinement_head.normal_(0,.1);m.descriptor_head.weight.normal_(0,.1)
            z=torch.zeros_like(x);o,_=m(z)
            self.assertFalse(o['sources_ri'].any());self.assertFalse(o['extraction_ri'].any());self.assertFalse(o['refinement_ri'].any())
            o['sources_ri'].square().sum().backward()
            self.assertTrue(all(p.grad is None or torch.isfinite(p.grad).all() for p in m.parameters()))

    def test_arbitrary_chunks_reload_and_mode_prior_binding(self):
        torch.manual_seed(52);m=model();x=audio(23)
        with torch.no_grad():
            m.mask_head.normal_(0,.03);m.refinement_head.normal_(0,.01);m.descriptor_head.weight.normal_(0,.04)
        whole,_=m(x);state=None;parts=[]
        for a,b in [(0,1),(1,3),(3,8),(8,9),(9,16),(16,23)]:
            out,state=m(x[:,a:b],state);parts.append(out['sources_ri'])
        torch.testing.assert_close(torch.cat(parts,1),whole['sources_ri'],rtol=3e-6,atol=3e-7)
        first,s=m(x[:,:7]);payload=m.state_to_payload(s);fresh=model();fresh.load_state_dict(copy.deepcopy(m.state_dict()))
        restored=fresh.state_from_payload(payload);a,_=m(x[:,7:],s);b,_=fresh(x[:,7:],restored)
        self.assertTrue(torch.equal(a['sources_ri'],b['sources_ri']))
        with self.assertRaisesRegex(ValueError,'mode/prior'):model('film_control').load_state_dict(m.state_dict())
        with self.assertRaisesRegex(ValueError,'mode/prior'):model(prior=(.5,.3,.2)).load_state_dict(m.state_dict())
        with self.assertRaisesRegex(ValueError,'mode/prior'):model('film_control').state_from_payload(payload)
        with self.assertRaises(ValueError):m(x[:,7:],s,guidance_mode='disabled')
        m.load_state_dict(m.state_dict())
        with self.assertRaises(ValueError):m(x[:,7:],s)
        with self.assertRaises(AttributeError):m.carrier_mode='film_control'
        with torch.no_grad():m.power_prior[0]+=.01
        with self.assertRaisesRegex(ValueError,'binding'):m(x)

    def test_direct_and_inherited_branch_gradients_and_oracle_refusal(self):
        torch.manual_seed(88);m=model();x=audio(13);target=torch.randn(1,13,3,2,513,2)
        out,_=m(x);(out['sources_ri']-target).square().mean().backward()
        rows=torch.arange(3)*7+1;self.assertGreater(float(m.descriptor_head.bias.grad[rows].abs().sum()),0.)
        m.zero_grad(set_to_none=True)
        with torch.no_grad():m.mask_head.normal_(0,.03);m.refinement_head.normal_(0,.01);m.descriptor_head.weight.normal_(0,.03)
        out,_=m(x);loss=(out['sources_ri']-target).square().mean()+.01*out['descriptors'].square().mean();loss.backward()
        for name,param in m.named_parameters():
            self.assertIsNotNone(param.grad,name);self.assertTrue(torch.isfinite(param.grad).all(),name)
            self.assertGreater(float(param.grad.abs().sum()),0.,name)
        raw=torch.zeros(1,13,3,32,6)
        for partition in ('DEV','TEST','validation','heldout'):
            with self.assertRaises(ValueError):m.forward_oracle_train(x,raw,partition=partition,diagnostic_token=ORACLE_TOKEN)
        m.eval()
        with self.assertRaises(ValueError):oracle(m,x,raw)
        self.assertNotIn('raw_guidance',inspect.signature(m.forward).parameters)
        with self.assertRaises(TypeError):m(x,raw_guidance=raw)

    def test_oracle_chunk_parity_and_invalid_prior(self):
        torch.manual_seed(9);m=model();x=audio(19);g=torch.rand(1,19,3,32,6);full,_=oracle(m,x,g);parts=[];state=None
        for a,b in [(0,2),(2,3),(3,7),(7,12),(12,19)]:
            out,state=oracle(m,x[:,a:b],g[:,a:b],state);parts.append(out['sources_ri'])
        torch.testing.assert_close(torch.cat(parts,1),full['sources_ri'],rtol=3e-6,atol=3e-7)
        for prior in ((.5,.5,0.),(.5,.2,.2),(-.1,.6,.5),(float('nan'),.5,.5)):
            with self.assertRaises(ValueError):model(prior=prior)
        with self.assertRaises(ValueError):Q2PowerGuidanceModel(C,prior=P,carrier_mode='direct_power',prior_partition='DEV')

    def test_full_reference_twelve_head_initialization(self):
        c=Q2Config();p=tuple([1/12]*12);torch.manual_seed(71)
        m=Q2PowerGuidanceModel(c,prior=p,carrier_mode='direct_power');x=audio(9)
        out,_=m(x);expected=x[:,:,None]*m.power_prior[None,None,:,None,None,None]
        torch.testing.assert_close(out['sources_ri'],expected,rtol=3e-6,atol=3e-7)
        self.assertEqual(out['power_carrier_weights'].shape,(1,9,12,32))
        self.assertFalse(m.specification()['car_approved'])
        for bad in (None,[],torch.zeros(1,2),torch.zeros(0,9,2,513,2)):
            with self.assertRaises(ValueError):m(bad)


if __name__=='__main__':unittest.main()
