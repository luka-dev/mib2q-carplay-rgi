import copy
import unittest
import numpy as np
from amp_dsp_chain import AmpChain, delay_channels
from amp_music_report import band_powers, measure, measure_output
from audit_amp_tuning import add_candidates, full_peak, match_gains, stress_signals


class TuningTests(unittest.TestCase):
    def test_existing_measurement_is_unchanged(self):
        c=AmpChain();x=np.random.default_rng(721).normal(0,.01,(20000,6))
        ref=band_powers(x[3000:12000,0]);y=c.process(x,capture_stages=False)['out']
        self.assertEqual(measure(c,x,ref,3000,12000,512),measure_output(c,y,ref,3000,12000,512))

    def test_matched_gains_attenuation_only_and_all_peaks_bounded(self):
        mid=np.array([-24,-26,-29]);peak=np.array([5,2,-4])
        target,g=match_gains(mid,peak)
        np.testing.assert_allclose(mid+g,target)
        self.assertTrue(np.all(g<=0));self.assertLessEqual(max(peak+g),-1.2+1e-12)
        with self.assertRaises(ValueError):match_gains([np.nan],[1])

    def test_full_clip_peak_includes_samples_outside_music_window(self):
        x=np.zeros((10000,16));x[0,15]=2
        p=full_peak(x)
        self.assertAlmostEqual(p['peak_dbfs'],20*np.log10(2))
        self.assertEqual(p['frames_above_0dbfs'],1)

    def test_synthetic_b_composition_matches_direct_injection(self):
        c=AmpChain();x=np.random.default_rng(722).normal(0,.01,(6000,6))
        stages=c.process(x);b1,b2=stress_signals(stages)
        np.testing.assert_array_equal(b1[:,[2,3,8,9]],0)
        for key,b in (('first_b',b1),('second_b',b2)):
            by=c.process(np.zeros_like(x),**{key:b},capture_stages=False)['out']
            for scale,delay in ((.25,0),(-.25,0),(.25,96)):
                delayed=delay_channels(b,[delay]*b.shape[1])*scale
                direct=c.process(x,**{key:delayed},capture_stages=False)['out']
                np.testing.assert_allclose(direct,stages['out']+scale*delay_channels(by,[delay]*16),atol=1e-13)

    def test_additional_sweep_requires_same_model_windows_and_sources(self):
        base={'complete':True,'model':{'version':7},'analysis':{'window':[5,55]},
              'tracks':{'song':{'source':{'sha256':'abc'},'variants':{'a':1}}}}
        extra=copy.deepcopy(base);extra['tracks']['song']['variants']={'b':2}
        combined=copy.deepcopy(base);add_candidates(combined,extra,['b'])
        self.assertEqual(combined['tracks']['song']['variants'],{'a':1,'b':2})
        for key in ('model','analysis','tracks','hash'):
            wrong=copy.deepcopy(extra)
            if key=='hash':wrong['tracks']['song']['source']['sha256']='changed'
            else:wrong[key]={}
            with self.assertRaises(ValueError):add_candidates(copy.deepcopy(base),wrong,['b'])


if __name__=='__main__':unittest.main()
