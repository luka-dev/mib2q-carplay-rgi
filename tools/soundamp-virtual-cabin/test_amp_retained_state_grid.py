import tempfile
from pathlib import Path
import unittest

import numpy as np

from amp_test_scenarios import make_test_scenario,scenario_manifest
from audit_amp_retained_state_grid import probes,levels,evaluate_context,run,auxiliary,make_chain,STATIC_SCENARIOS


class RetainedStateGridTests(unittest.TestCase):
    def test_probe_reproducibility_separate_inputs_and_tail(self):
        a=probes(4096);b=probes(4096)
        for x,y in zip(a,b):
            np.testing.assert_array_equal(x,y)
            np.testing.assert_array_equal(x[-1024:],0)
            self.assertGreater(np.max(abs(x)),0)
        self.assertEqual(a[0].shape,(4096,6));self.assertEqual(a[2].shape,(4096,3))
        for n in (2048,4097,4096.,True):
            with self.assertRaises(ValueError):probes(n)

    def test_real_eight_axis_context_and_boundary_reset(self):
        got=evaluate_context(bank=7,pointer='default',rounding='nearest',frames=4096)
        self.assertEqual(len(got['rows']),8)
        self.assertEqual(len({tuple(row[k] for k in ('adaptive_enable','complex_aux_enabled',
                          'second_b_aux_enabled')) for row in got['rows']}),8)
        self.assertTrue(got['checks']['caller_state_unchanged'])
        for item in got['checks']['extreme_replays']:
            self.assertTrue(item['reset_exact']);self.assertEqual(item['boundary_injection_max_absolute_error'],0)
        self.assertFalse(got['scenario']['actual_vehicle_state_proven'])
        self.assertGreater(max(got['rows'][0]['stages']['first_b10']['peak_linear']),0)

    def test_factory_numeric_selection_is_opt_in_and_explicit(self):
        with self.assertRaises(ValueError):make_test_scenario(input_eq_bank=7,first_b_numerics='firmware_library_nearest')
        for mode in (None,'auto','firmware_library',True):
            with self.assertRaises(ValueError):make_test_scenario(input_eq_bank=7,first_b_numerics=mode)
        old=make_test_scenario(input_eq_bank=7,first_b_spectral='retained_calibration')
        self.assertIsNone(old['first_b_scenario']['reference'].spectral.complex_twiddles)
        for rounding in ('nearest','floor'):
            got=make_test_scenario(input_eq_bank=7,first_b_spectral='retained_calibration',
                                  first_b_numerics='firmware_library_'+rounding)
            manifest=scenario_manifest(got)
            self.assertEqual(manifest['spectral_binding']['level_math']['fix_rounding'],rounding)
            self.assertFalse(manifest['spectral_binding']['complex_fft']['hardware_bit_exact'])

    def test_cli_axes_separate_aux_from_adaptation(self):
        for name,(rounding,probe,adaptive) in STATIC_SCENARIOS.items():
            aux,metadata=auxiliary(name,3072,1024)
            chain,manifest=make_chain(name,7)
            self.assertEqual(manifest['adaptive_enable'],adaptive)
            self.assertEqual(manifest['first_b_numerical_reference'],'firmware_library_'+rounding)
            self.assertEqual(bool(np.any(aux['complex_auxiliary'])),probe)
            self.assertEqual(bool(np.any(aux['second_b_auxiliary'])),probe)
            self.assertFalse(metadata['music_donors'])
        with self.assertRaises(ValueError):make_chain('full_static_typo',7)
        with self.assertRaises(ValueError):auxiliary('full_static_typo',3072,1024)

    def test_report_refuses_overwrite_and_bad_axes_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):run(out=Path(directory))
            for banks in ((),(7,7),(16,),(True,)):
                with self.assertRaises(ValueError):run(out=Path(directory)/'new',banks=banks)
            self.assertFalse((Path(directory)/'new').exists())
        with self.assertRaises(ValueError):levels(np.full((4096,16),np.nan))


if __name__=='__main__':unittest.main()
