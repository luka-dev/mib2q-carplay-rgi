import unittest
import numpy as np
from audit_teacher_audio_content import diagnostic_gain,describe_audio,overlap

class AudioContentTests(unittest.TestCase):
    def test_analysis_gain_capped_and_nonmutating(self):
        x=np.ones(48000,dtype=np.float32)*.0001;before=x.copy()
        self.assertEqual(diagnostic_gain(x),10)
        np.testing.assert_array_equal(x,before)
        self.assertLessEqual(diagnostic_gain(np.ones(48000)*2)*2,.95)
    def test_side_only_warned_by_mono_retention(self):
        t=np.arange(48000)/48000;y=np.sin(2*np.pi*440*t)*.1
        x=np.column_stack([y,-y]);m=describe_audio(x,x)
        self.assertAlmostEqual(m['mono_retained_energy_fraction'],0)
        self.assertAlmostEqual(m['side_energy_fraction'],1)
        self.assertAlmostEqual(m['level_vs_mix_db'],0)
        self.assertGreater(m['bands']['250-1000'],.99)
    def test_overlap_not_labeled_purity(self):
        x=np.arange(100,dtype=np.float32)
        result=overlap(x,x*.2)
        self.assertAlmostEqual(result['cosine'],1)
        self.assertIn('NOT a leakage percentage',result['warning'])
    def test_silent_metrics_finite(self):
        x=np.zeros((48000,2),dtype=np.float32);m=describe_audio(x,x)
        self.assertTrue(np.isfinite(m['rms_dbfs']))
        self.assertEqual(m['mono_retained_energy_fraction'],0)

if __name__=='__main__':unittest.main()
