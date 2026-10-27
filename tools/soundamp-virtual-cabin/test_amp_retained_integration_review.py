"""Independent retained-factory wiring review; no vehicle-state assumption."""
import copy
import unittest

import numpy as np

from amp_dsp_chain import AmpChain
from amp_test_scenarios import make_test_scenario, scenario_manifest


class RetainedIntegrationReview(unittest.TestCase):
    def test_retained_branches_are_not_unity_and_baseline_default_is_unchanged(self):
        baseline=copy.deepcopy(AmpChain().metadata())
        scenario=make_test_scenario(input_eq_bank=7,first_b_spectral='retained_calibration')
        spectral=scenario['first_b_scenario']['reference'].spectral
        manifest=scenario_manifest(scenario)
        rng=np.random.default_rng(0x130771)
        spectra=rng.normal(size=(2,513))+1j*rng.normal(size=(2,513))
        branches=spectral.spectral_process(*spectra,mode=5)
        self.assertEqual(set(branches),{'pre','main','post'})
        # Independent deterministic boundary probe detects accidental reuse of
        # the old all-unity factory. Retained main may legitimately be unity.
        for key in ('pre','post'):
            self.assertTrue(np.isfinite(branches[key]).all())
            self.assertGreater(np.max(np.abs(branches[key]-spectra)),.01)
        np.testing.assert_array_equal(scenario['first_b_scenario']['reference'].spectral.spatial.params[0:4],
                                      [1.,1.,1.,0.])
        self.assertEqual(manifest['first_b_mode_control_history']['apply_order'],[[20,1],[16,5]])
        self.assertFalse(manifest['first_b_mode_control_history']['full_replay_or_transition_simulated'])
        self.assertFalse(manifest['actual_vehicle_state_proven'])
        self.assertFalse(manifest['spectral_binding']['transform_tables']['hardware_bit_exact'])
        self.assertEqual(manifest['spectral_binding']['spatial']['balance_raw_words'],
                         ['40a00001','c0600001'])
        self.assertEqual(AmpChain().metadata(),baseline)
        self.assertIsNone(baseline['first_b_scenario'])


if __name__=='__main__':unittest.main()
