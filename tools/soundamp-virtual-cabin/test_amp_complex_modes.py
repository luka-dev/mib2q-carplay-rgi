import unittest
import numpy as np
from amp_complex_modes import CalibrationSequence, ComplexLifecycle, CalibrationNoiseCore, select_stage_mask


class ModeTests(unittest.TestCase):
    def test_mask_precedence_and_no_sideband_read_for_other_modes(self):
        for sideband in (0,1,2,3):
            for mode in range(16):
                mask,inhibit=select_stage_mask(mode,sideband)
                if mode&4:
                    self.assertEqual(mask,0xdff75e5 if sideband&1 else 0xfff7fe5)
                    self.assertEqual(inhibit,sideband&1)
                else:
                    self.assertEqual(mask,0xf4ba if mode&2 else 1 if mode&1 else 2)
                    self.assertEqual(inhibit,0)
        mask,_=select_stage_mask(2,0)
        self.assertEqual([i for i in range(28) if mask>>i&1],[1,3,4,5,7,10,12,13,14,15])

    def test_disabled_and_binding_only_call_do_not_clear_output(self):
        c=ComplexLifecycle()
        self.assertEqual(c.begin_block(master_enable=0,mode_word=4,sideband_word=0)['action'],'no_write')
        self.assertFalse(c.initialized)
        self.assertEqual(c.begin_block(master_enable=1,mode_word=4,sideband_word=0)['reason'],'bind_buffers_only')
        self.assertEqual(c.begin_block(master_enable=1,mode_word=4,sideband_word=0)['action'],'full_mode')
        c.begin_block(master_enable=0,mode_word=4,sideband_word=0)
        self.assertTrue(c.initialized)

    def test_configure_wait_measure_advance_and_last_block(self):
        c=CalibrationSequence(channel_mask=1|512,wait_samples=128,measure_samples=128)
        r=c.begin_block(64)
        self.assertTrue(r['reset_fir_history']);self.assertFalse(r['reset_fir_coefficients'])
        for _ in range(2):self.assertEqual(c.begin_block(64)['reason'],'wait')
        for _ in range(2):self.assertEqual(c.begin_block(64)['gate_bits'],1)
        self.assertEqual(c.begin_block(64)['reference_gate_bits'],512)
        for _ in range(2):self.assertEqual(c.begin_block(64)['reason'],'wait')
        for _ in range(2):self.assertEqual(c.begin_block(64)['gate_bits'],512)
        self.assertEqual(c.begin_block(64)['gate_bits'],512) # completion fall-through
        self.assertTrue(c.complete)
        self.assertEqual(c.begin_block(64)['reason'],'complete')

    def test_empty_channel_mask_finishes_without_inventing_gate(self):
        c=CalibrationSequence(0,128,128)
        self.assertEqual(c.begin_block(64)['gate_bits'],0)
        self.assertTrue(c.complete)

    def test_noise_modular_core_matches_independent_schrage_recurrence(self):
        seed=1;shuffle=[0]*32;previous=0;expected=[]
        for _ in range(1024):
            quotient=seed//127773
            seed=16807*(seed-quotient*127773)-2836*quotient
            if seed<0:seed+=2147483647
            index=min(31,previous//67108864)
            previous,shuffle[index]=shuffle[index],seed
            sample=2*previous
            if sample>=2147483648:sample-=4294967296
            expected.append(sample/2147483648)
        c=CalibrationNoiseCore(seed=1,shuffle=[0]*32,previous=0,alusat=False)
        actual=np.r_[c.process(63),c.process(961)]
        np.testing.assert_array_equal(actual,expected)
        self.assertEqual(c.seed,seed)
        self.assertEqual(c.shuffle,shuffle)

    def test_noise_alu_saturation_is_explicit(self):
        args=dict(seed=1,shuffle=[2147483646]*32,previous=0)
        a=CalibrationNoiseCore(**args,alusat=False).process(1)[0]
        b=CalibrationNoiseCore(**args,alusat=True).process(1)[0]
        self.assertLess(a,0);self.assertGreater(b,.99)


if __name__=='__main__':unittest.main()
