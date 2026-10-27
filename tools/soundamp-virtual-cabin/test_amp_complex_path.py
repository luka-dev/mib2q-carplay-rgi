import json
import unittest
from pathlib import Path

import numpy as np
from scipy.signal import sosfilt

from amp_complex_path import ComplexFullModeReference, PowerHistory, biquad_sos, load_calibrated_fir
from amp_complex_control import AdaptiveResidual


ROOT=Path(__file__).resolve().parents[2]
PARAMETERS=ROOT/'assets/soundamp/remaining-blocks-20260919/audit.json'
IMAGE=ROOT.parents[1]/'Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin'


class ComplexPathTests(unittest.TestCase):
    def path(self, adaptive=0, sideband=0):
        p=json.loads(PARAMETERS.read_text())['complex_control_profiles'][0]
        return ComplexFullModeReference(p,np.zeros((10,512)),adaptive_enable=adaptive,
            sideband=sideband,power_x_db=[0]*5,power_y_db=[0]*5,
            power_floor_db=[-200]*5,rounding='nearest')

    def test_power_history_lanes_and_window_boundary(self):
        h=PowerHistory(2,maximum=True,sum_scale=1/8,max_scale=1/4)
        a=np.ones((4,5,2));a[:,:,0]=2
        self.assertIsNone(h.push(a))
        b=np.ones((4,5,2));b[:,:,1]=3
        x,y=h.push(b)
        np.testing.assert_array_equal(x,9)
        np.testing.assert_array_equal(y,4)
        self.assertIsNone(h.push(np.zeros_like(a)))
        x,y=h.push(np.zeros_like(a))
        np.testing.assert_array_equal(x,0);np.testing.assert_array_equal(y,0)

    def test_maximum_block_energy_not_maximum_sample(self):
        h=PowerHistory(1,maximum=True,sum_scale=1/4,max_scale=1/4)
        a=np.zeros((4,5,2));a[0]=4
        x,y=h.push(a)
        np.testing.assert_array_equal(x,4) # not peak square 16
        np.testing.assert_array_equal(y,4)

    def test_history_sum_scale(self):
        h=PowerHistory(2,maximum=False,sum_scale=1/8,max_scale=1/4)
        self.assertIsNone(h.push(np.ones((4,5,2))))
        x,y=h.push(np.ones((4,5,2))*3)
        np.testing.assert_array_equal(x,5);np.testing.assert_array_equal(y,5)

    def test_auxiliary_biquad_sign_order_against_scalar_recurrence(self):
        record=[.6,-.2,.3,.1,-.05]
        rng=np.random.default_rng(31);x=rng.normal(size=97)
        z1,z2=.4,-.2;y=[]
        for v in x:
            o=.3*v+z1
            z1,z2=.1*v+.6*o+z2,-.05*v-.2*o
            y.append(o)
        actual,_=sosfilt([biquad_sos(record)],x,zi=np.array([[.4,-.2]]))
        np.testing.assert_allclose(actual,y,atol=1e-15)

    def test_full_mode_disabled_adaptation_reconstructs_delayed_audio(self):
        rng=np.random.default_rng(30);x=rng.normal(0,.01,(64*40,10))
        c=self.path();out=[];updates=[]
        for i in range(40):
            r=c.process_block(x[i*64:(i+1)*64],np.zeros(64))
            out.append(r['audio'])
            if r['update'] is not None:updates.append(i+1)
        expected=np.zeros_like(x);expected[1023:]=x[:-1023]
        np.testing.assert_allclose(np.concatenate(out),expected,atol=1e-8)
        self.assertEqual(updates,[16,32])

    def test_sideband_skip_is_exact_delay_even_with_adaptation_requested(self):
        rng=np.random.default_rng(32);x=rng.normal(0,.01,(64*32,10))
        c=self.path(adaptive=1,sideband=1)
        y=np.concatenate([c.process_block(x[i:i+64],np.ones(64))['audio'] for i in range(0,len(x),64)])
        expected=np.zeros_like(x);expected[1023:]=x[:-1023]
        np.testing.assert_array_equal(y,expected)
        np.testing.assert_array_equal(c.control.endpoint,1)

    def test_stage12_stride16_compaction_and_distinct_auxiliary_music_sum(self):
        c=self.path();t=np.arange(64);aux=np.sin(t/7)
        p=c.profile['detector_layout']
        filt=[[1,b1,b2,1,-a1,-a2] for a2,a1,b2,b1 in p['prefilter_coefficients']]
        # Both auxiliary EQ records are unity in this extraction.
        expected=sosfilt(filt,aux*p['prefilter_scale'])
        r=c.process_block(np.zeros((64,10)),aux)
        np.testing.assert_allclose(r['detector_auxiliary'],expected[::16],atol=1e-30)
        self.assertFalse(np.allclose(expected[:4],expected[::16],atol=1e-12))
        np.testing.assert_array_equal(r['detector_music_sum'],0)
        np.testing.assert_allclose(r['residual'],expected[::16],atol=1e-30)

    def test_music_sum_uses_input_before_common_delay(self):
        c=self.path()
        x=np.zeros((64,10));x[:,0]=1
        r=c.process_block(x,np.zeros(64))
        np.testing.assert_array_equal(r['audio'],0)
        np.testing.assert_array_equal(r['detector_references'],0)
        self.assertGreater(np.max(abs(r['detector_music_sum'])),0)

    def test_new_target_affects_next_block_not_completed_window(self):
        c=self.path(adaptive=1)
        # Stub only the power-window result to isolate scheduling from FIR.
        c.history.push=lambda _: (np.full(5,.01),np.full(5,.1))
        x=np.ones((64,10))*.01
        before=c.bands.gains.copy()
        r=c.process_block(x,np.zeros(64))
        self.assertTrue((r['update']['endpoint']>1).all())
        np.testing.assert_array_equal(c.bands.gains,before)
        c.history.push=lambda _: None
        c.process_block(x,np.zeros(64))
        np.testing.assert_allclose(c.bands.gains,before+64*r['update']['steps'])

    def test_explicit_scope_and_block_size(self):
        c=self.path()
        self.assertIn('not live',c.metadata()['warning'])
        with self.assertRaises(ValueError):c.process_block(np.zeros((65,10)),np.zeros(65))

    def test_global_circular_fir_history_matches_canonical_lanes(self):
        # Literal address layout of 125F36: one 5120-word circular history;
        # heads separated by 512, cursor decremented once per sample. SIMD
        # boundary duplicates implement wrapped reads, not extra FIR taps.
        rng=np.random.default_rng(33)
        h=rng.normal(0,.001,(10,512));x=rng.normal(0,.1,(1040,10));aux=rng.normal(0,.1,1040)
        canonical=AdaptiveResidual(h,reference_gains=np.ones(10),auxiliary_gain=1,
                                   step=.002,rounding='nearest')
        flat=np.zeros(5120);head=0;weights=h.copy();expected=[]
        for row,target in zip(x,aux):
            flat[(head+np.arange(10)*512)%5120]=row
            history=flat[(head+np.arange(5120))%5120].reshape(10,512)
            error=target-np.sum(weights*history)
            weights+=.002*error*history
            expected.append(error);head=(head-1)%5120
        # Each block's aggregate power stays <.5, so step-table index=0.
        actual=np.concatenate([canonical.process(x[i:i+4],aux[i:i+4]) for i in range(0,len(x),4)])
        np.testing.assert_allclose(actual,expected,atol=1e-14)
        np.testing.assert_allclose(canonical.coefficients,weights,atol=1e-14)

    @unittest.skipUnless(IMAGE.exists(),'local audited CodeFlash required')
    def test_actual_fir_identity_and_synthetic_long_run(self):
        from audit_amp_complex_scenarios import run
        a=json.loads(PARAMETERS.read_text())
        p=a['complex_control_profiles'][0]
        with self.assertRaises(ValueError):load_calibrated_fir(IMAGE,p,'0'*64)
        bad=json.loads(json.dumps(p));bad['fir_bank_AAA60']['sha256']='0'*64
        with self.assertRaises(ValueError):load_calibrated_fir(IMAGE,bad,a['source_sha256'])
        result=run(IMAGE,PARAMETERS,seconds=1)
        for s in result['scenarios']:
            self.assertEqual(s['update_count'],48000//1024)
            self.assertEqual(s['model']['initial_fir_float32_sha256'],p['fir_bank_AAA60']['sha256'])
            self.assertTrue(np.isfinite(s['final_target_db']).all())
        # An explicit sideband skip remains neutral. Zero auxiliary is NOT a
        # bypass: the residual still includes the nonzero calibrated FIR.
        np.testing.assert_array_equal(result['scenarios'][-1]['final_target_db'],0)
        self.assertGreater(max(result['scenarios'][0]['final_target_db']),0)


if __name__=='__main__':unittest.main()
