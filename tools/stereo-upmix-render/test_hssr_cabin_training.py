import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import numpy as np
import soundfile as sf
import torch
from train_hssr import HssrNetwork, export_model, import_model, FEATURES, OUTPUTS
from train_hssr import freeze_except_routing_head, BANDS, loss_components
from hssr_runtime_training import activation, supervised_prediction
from prepare_hssr_native_teacher import restore_pair, valid_pair, output_pair, digest, POLICY
from evaluate_cabin_attribution import apply_transfers

ROOT=Path(__file__).resolve().parents[2]

class CabinTrainingTest(unittest.TestCase):
    def test_head_only_preserves_semantics_and_recurrence(self):
        torch.manual_seed(17);m=HssrNetwork();m.runtime_aware=True
        freeze_except_routing_head(m)
        before={k:v.detach().clone() for k,v in m.state_dict().items()}
        optimizer=torch.optim.AdamW((p for p in m.parameters() if p.requires_grad),lr=1e-4,weight_decay=0)
        x=torch.randn(2,8,FEATURES);target=torch.zeros(2,8,OUTPUTS)
        values=loss_components(m(x),target,'selection-v11',True)
        values['total'].backward();optimizer.step()
        allowed=torch.zeros(OUTPUTS,dtype=torch.bool)
        allowed[6:BANDS*8:8]=True;allowed[7:BANDS*8:8]=True
        for k,v in m.state_dict().items():
            if k.startswith('head.'):
                self.assertTrue(torch.equal(v[~allowed],before[k][~allowed]))
            else:self.assertTrue(torch.equal(v,before[k]))
        self.assertFalse(torch.equal(m.head.weight[allowed],before['head.weight'][allowed]))

    def test_frozen_wola_transfer_delay_and_additivity(self):
        rng=np.random.default_rng(13)
        a=rng.normal(0,.1,(4096,2));b=rng.normal(0,.1,(4096,2))
        window=np.sin(np.pi*(np.arange(512)+.5)/512)
        synthesis=window/np.tile((window**2).reshape(4,128).sum(axis=0),4)
        matrices=np.zeros((32,257,4,2))
        matrices[:,:,0,0]=1;matrices[:,:,1,1]=1
        matrices[:,:,2,0]=.3;matrices[:,:,3,1]=.6
        matrices=matrices.reshape(32,257,8)
        result=apply_transfers(a+b,matrices,window,synthesis)
        expected=np.concatenate([np.zeros((512,2)),a+b])[:4096]
        np.testing.assert_allclose(result[:,:2],expected,atol=1e-14)
        np.testing.assert_allclose(result,apply_transfers(a,matrices,window,synthesis)+
            apply_transfers(b,matrices,window,synthesis),atol=1e-14)

    def test_native_shared_gain_and_float_provenance(self):
        t=np.arange(4800)/48000
        mix=np.stack([.8*np.sin(2*np.pi*440*t)]*2,axis=1).astype('f4')
        tn=np.arange(4410)/44100
        vocal=np.stack([.2*np.sin(2*np.pi*440*tn)]*2,axis=1).astype('f4')
        v,i,error=restore_pair(mix,vocal,.5)
        self.assertLess(error,1e-7)
        self.assertAlmostEqual(float(np.max(v)),.4,places=2)
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);identity=dict(policy=POLICY,frames=len(mix),source_sha256='source')
            paths=output_pair(d)
            for p,x in zip(paths,(v,i)):sf.write(p,x,48000,subtype='FLOAT')
            (d/'teacher-provenance.json').write_text(json.dumps(dict(identity=identity,output_sha256={p.name:digest(p) for p in paths})))
            self.assertTrue(valid_pair(d,identity))
            self.assertFalse(valid_pair(d,dict(identity,source_sha256='changed')))
            sf.write(paths[0],v*.5,48000,subtype='FLOAT')
            self.assertFalse(valid_pair(d,identity))

    def test_rounding_and_burnin_gradients(self):
        x=torch.tensor([-.5/127,.5/127,2.])
        torch.testing.assert_close(activation(x,127),torch.tensor([-1/127,1/127,1.]))
        torch.manual_seed(7);m=HssrNetwork();m.runtime_aware=True
        x=torch.randn(1,12,FEATURES,requires_grad=True);y=torch.zeros(1,12,OUTPUTS)
        p,target=supervised_prediction(m,x,y,4,1)
        self.assertEqual(p.shape,(1,7,OUTPUTS));self.assertEqual(target.shape,p.shape)
        p.square().mean().backward()
        self.assertEqual(float(x.grad[:,:4].abs().sum()),0)
        self.assertGreater(float(x.grad[:,4:].abs().sum()),0)

    def test_runtime_emulator_matches_exported_c(self):
        torch.manual_seed(11);torch.set_num_threads(2)
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);exe=d/'validate'
            subprocess.run(['cc','-O2','-std=c99','-I'+str(ROOT/'hook/audio'),
                str(ROOT/'tools/stereo-upmix-render/bench/hssr_validate.c'),
                str(ROOT/'hook/audio/hssr_dsp.c'),str(ROOT/'hook/audio/hssr_neon.c'),'-lm','-o',str(exe)],check=True)
            model_path=ROOT/'models/hssr-selection-v11/hssr_model.bin'
            m,mean,inv=import_model(model_path);m.runtime_aware=True
            # Real validation features cover large normalized dynamic ranges.
            cache=next((ROOT/'build/reference-audio/v12-separator-cache/validation').glob('*instrumental-positive.npz'))
            raw=np.load(cache)['features'][:128].astype('f4');raw.tofile(d/'x')
            subprocess.run([str(exe),str(model_path),'--features',str(d/'x'),str(d/'y')],check=True)
            actual=np.fromfile(d/'y',dtype='f4').reshape(-1,OUTPUTS)
            with torch.no_grad():expected=m(torch.from_numpy(np.clip((raw-mean)*inv,-6,6))[None])[0].numpy()
            mae=float(np.mean(abs(actual-expected)))
            self.assertLess(mae,.003)
            self.assertLess(float(np.quantile(abs(actual-expected),.99)),.02)
            print(f'custom runtime parity logit MAE={mae:.8g}')

if __name__=='__main__':unittest.main()
