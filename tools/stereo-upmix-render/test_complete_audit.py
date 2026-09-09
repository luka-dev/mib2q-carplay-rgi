import unittest
import numpy as np
from audit_full_source_integrity import rough_fingerprint
from run_v14_complete_audit import window_states

class CompleteAuditTests(unittest.TestCase):
    def test_fingerprint_finite_reproducible(self):
        t=np.arange(96000)/48000
        x=(np.sin(2*np.pi*440*t)*np.exp(-5*(t%.25))*10000).astype(np.int16)
        stereo=np.column_stack([x,x]);a=rough_fingerprint(stereo)
        self.assertTrue(np.isfinite(a).all());self.assertEqual(len(a),24*128)
        np.testing.assert_array_equal(a,rough_fingerprint(stereo))
    def test_silence_fingerprint_finite(self):
        self.assertTrue(np.isfinite(rough_fingerprint(np.zeros((96000,2),dtype=np.int16))).all())
    def test_supported_never_approved(self):
        view={'clap':{'cosine':{'hi_hat':.7,'noise':.1}},'ast':{'focus':{'Hi-hat':.5}}}
        stem=dict(metrics={'level_vs_mix_db':-10},windows=[dict(offset_seconds=0,seconds=10,
            views={'shared_level':view,'diagnostic_level':view})])
        result=window_states({'stems':{'hh':stem}})['hh'][0]
        self.assertEqual(result['state'],'cross-model-supported-candidate-not-approved')
        self.assertFalse(result['training_approved'])
    def test_short_tail_abstains(self):
        stem=dict(metrics={'level_vs_mix_db':-10},windows=[dict(offset_seconds=20,seconds=.2,
            short_context_warning='short',views={})])
        self.assertEqual(window_states({'stems':{'hh':stem}})['hh'][0]['state'],'short-context-abstain')

if __name__=='__main__':unittest.main()
