import copy
import unittest
import numpy as np
from amp_end_to_end_metrics import measure,paired_delta


class EndToEndMetricsTests(unittest.TestCase):
    def test_identity_scaled_power_and_tail_are_separate(self):
        t=np.arange(4800)/48000;x=np.sin(2*np.pi*100*t)*.1
        y=np.tile(x[:,None],(1,16));pcm=np.zeros((4800,6))
        a=measure(pcm,y,4000,800);b=measure(pcm,y*.5,4000,800)
        self.assertAlmostEqual(paired_delta(b,a)['front woofers'][1],-6.02059991328,places=8)
        self.assertEqual(a['outputs']['finite_tail']['frames'],800)
        self.assertEqual(a['analysis']['programme_window_frames'],[0,4000])
        self.assertTrue(a['flags']['digital_c_is_zero'])

    def test_overs_preserved_in_tail_and_whole_clip(self):
        y=np.zeros((100,16));y[90,14]=2
        result=measure(np.zeros((100,6)),y,80,20)
        self.assertTrue(result['flags']['electrical_output_exceeds_0dbfs'])
        self.assertEqual(result['outputs']['whole_clip']['frames_above_0dbfs'],1)
        self.assertEqual(result['outputs']['programme']['frames_above_0dbfs'],0)
        self.assertAlmostEqual(result['outputs']['finite_tail']['peak_dbfs'],6.02059991328)

    def test_group_powers_are_not_coherent_speaker_sum(self):
        y=np.zeros((4800,16));x=np.sin(2*np.pi*80*np.arange(4800)/48000)*.1
        y[:,12]=x;y[:,13]=-x
        result=measure(np.zeros((4800,6)),y,4800,0)
        self.assertGreater(result['groups']['front woofers']['power_dbfs'][1],-30)
        self.assertIsNone(result['outputs']['finite_tail']['peak_dbfs'])

    def test_exact_c_and_near_rails_without_clipping_claim(self):
        x=np.zeros((32,6));x[0,2]=1/32768;x[1,0]=-1;x[2,1]=32767/32768
        r=measure(x,np.zeros((32,16)),32,0)
        self.assertEqual(r['pcm6']['digital_c_nonzero_samples'],1)
        self.assertEqual(r['pcm6']['samples_at_or_above_s16_rail'][:2],[1,1])
        self.assertFalse(r['flags']['digital_c_is_zero'])
        self.assertFalse(r['sound_quality_or_vehicle_acceptance_established'])

    def test_invalid_lengths_nonfinite_and_complex_rejected(self):
        for pcm,out,n,tail in ((np.zeros((3,6)),np.zeros((4,16)),3,0),
            (np.zeros((3,6)),np.zeros((3,16)),True,2),
            (np.zeros((3,6)),np.zeros((3,16)),3,-1),
            (np.zeros((3,6)),np.full((3,16),np.nan),3,0),
            (np.zeros((3,6)),np.zeros((3,16),complex),3,0)):
            with self.assertRaises(ValueError):measure(pcm,out,n,tail)

    def test_unmatched_comparison_windows_rejected(self):
        r=measure(np.zeros((32,6)),np.zeros((32,16)),32,0)
        other=copy.deepcopy(r);other['analysis']['programme_window_frames']=[1,33]
        with self.assertRaises(ValueError):paired_delta(r,other)


if __name__=='__main__':unittest.main()
