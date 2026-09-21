import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

import numpy as np

from amp_b_math import BMathReference,load_math_tables,ROOT,LOADER
from amp_b_adaptive import BandLevelMaskReference,level_gain,POWER_TO_GAIN_FACTOR

sys.path.insert(0,str(ROOT/'tools'))
from inspect_amp_visa import compute,load_shortwords,listing
from extract_amp_sharc_app import SOURCE_SHA256


class FirmwareLibraryMathTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.nearest=BMathReference(fix_rounding='nearest')
        cls.floor=BMathReference(fix_rounding='floor')

    def test_scaled_conversion_and_abs_opcodes_match_adi_tables(self):
        for code,wanted in ((0x0da223,'F2=FLOAT R2 BY R3'),
                            (0x0d9814,'R8=FIX F1 BY R4'),
                            (0x0dd814,'R8=TRUNC F1 BY R4'),
                            (0x0306a0,'R6=ABS R10')):
            self.assertEqual(compute(code),wanted)
        mem=load_shortwords(LOADER.read_bytes())
        self.assertFalse(any('UNKNOWN' in line for line in listing(mem,0x1257e6,0x125949)))

    def test_log_rational_positive_normal_domain_and_branch_boundaries(self):
        threshold=self.nearest.tables['log'][0]*2
        x=np.concatenate((np.geomspace(np.finfo(np.float32).tiny,np.finfo(np.float32).max,10001),
                          [1.,2.,.5,np.nextafter(threshold,0),threshold,np.nextafter(threshold,2)]))
        np.testing.assert_allclose(self.nearest.natural_log(x),np.log(x),rtol=0,atol=3e-9)
        np.testing.assert_allclose(self.nearest.log10(x),np.log10(x),rtol=0,atol=9.1e-7)
        self.assertEqual(self.nearest.log10(1.),0)
        self.assertEqual(self.nearest.natural_log(1.),0)
        # Stored log10 factor is not silently replaced with ideal 1/log(10).
        self.assertGreater(abs(self.nearest.log10(10.)-1),1e-9)

    def test_pow_tables_and_polynomial_against_independent_power(self):
        exponents=np.linspace(-37,37,20001)
        for reference in (self.nearest,self.floor):
            result=reference.pow10(exponents)
            np.testing.assert_allclose(result,10.**exponents,rtol=1.1e-7,atol=0)
            self.assertEqual(reference.pow10(0.),1)
            trace=reference.pow10_trace(exponents)
            self.assertEqual(trace['base_anchor_index'],12)
            self.assertEqual(trace['base_log2_high'],3.3125)
            self.assertTrue(np.all(trace['residual']<=0))
            self.assertTrue(np.all(trace['residual']>=-1/16))
            self.assertTrue(np.all((trace['reconstruction_index']>=1)&(trace['reconstruction_index']<=17)))
            self.assertAlmostEqual(trace['base_log2_high']+trace['base_log2_low'],np.log2(10),places=9)

    def test_explicit_fix_rounding_is_not_truncation_toward_zero(self):
        x=np.array([-.03125,.03125,.09375,-.09375])
        np.testing.assert_array_equal(self.nearest._fix16(x),[0,0,2,-2])
        np.testing.assert_array_equal(self.floor._fix16(x),[-1,0,1,-2])
        np.testing.assert_array_equal(self.nearest.pow10_trace(x)['exponent_sixteenth'],[0,0,.125,-.125])
        np.testing.assert_array_equal(self.floor.pow10_trace(x)['exponent_sixteenth'],[-.0625,0,.0625,-.125])

    def test_outside_supported_domains_fail_not_silent_fallback(self):
        for x in (0.,-1.,np.inf,np.nan,np.finfo(np.float32).tiny/2,1e39,1j):
            with self.subTest(log=x),self.assertRaises(ValueError):self.nearest.log10(x)
        for x in (-37.001,37.001,np.inf,np.nan,1j):
            with self.subTest(pow=x),self.assertRaises(ValueError):self.nearest.pow10(x)
        for mode in (None,'truncate','auto',True):
            with self.assertRaises(ValueError):BMathReference(fix_rounding=mode)

    def test_scalar_vector_shape_and_copyable_provenance(self):
        x=np.array([[.1,1.],[2.,10.]])
        self.assertEqual(self.nearest.log10(x).shape,x.shape)
        self.assertEqual(self.nearest.pow10(x).shape,x.shape)
        reference=copy.deepcopy(self.nearest)
        np.testing.assert_array_equal(reference.pow10(x),self.nearest.pow10(x))
        metadata=reference.metadata();json.dumps(metadata,allow_nan=False)
        self.assertFalse(metadata['hardware_bit_exact'])
        self.assertFalse(metadata['fix_rounding_live_verified'])
        metadata['tables'].clear()
        self.assertEqual(len(reference.metadata()['tables']),4)

    def test_corrupt_loader_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'bad.ldr';raw=bytearray(LOADER.read_bytes());raw[-1]^=1
            path.write_bytes(raw)
            with self.assertRaisesRegex(ValueError,'exact audited'):load_math_tables(path)

    def test_actual_b01_bank_opt_in_and_unchanged_default(self):
        path=ROOT.parents[1]/'Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin'
        raw=path.read_bytes();self.assertEqual(hashlib.sha256(raw).hexdigest(),SOURCE_SHA256)
        params=list(struct.unpack_from('<16f',raw,0x12000c+4*50))
        arguments=dict(bins=2,edges=[0,1,2],params=params,power_history=[0,0],
                       db_history=[0,0],gain_history=[0,0])
        default=BandLevelMaskReference(**arguments)
        explicit=BandLevelMaskReference(**arguments,numeric_mode='host_math')
        actual=BandLevelMaskReference(**arguments,numeric_mode='firmware_library',fix_rounding='nearest')
        self.assertIsNone(default.math_reference)
        for x in ([1,100],[1,100],[10,1000]):
            ordinary=default.process(x,enabled=True)
            np.testing.assert_array_equal(ordinary,explicit.process(x,enabled=True))
            derived=actual.process(x,enabled=True)
            self.assertTrue(np.isfinite(derived).all())
            np.testing.assert_allclose(derived,ordinary,rtol=2e-6,atol=1e-10)
        self.assertEqual(actual.math_reference.metadata()['fix_rounding'],'nearest')
        with self.assertRaises(ValueError):BandLevelMaskReference(**arguments,numeric_mode='firmware_library')
        with self.assertRaises(ValueError):BandLevelMaskReference(**arguments,fix_rounding='nearest')
        with self.assertRaises(ValueError):BandLevelMaskReference(**arguments,numeric_mode='unknown')

    def test_level_gain_uses_actual_pow10_opt_in_before_clamp(self):
        fast=np.array([0.,10.,100.]);slope=.6
        expected=self.nearest.pow10((slope-1)*fast*POWER_TO_GAIN_FACTOR)
        got=level_gain(fast,np.zeros(3),fast,slope=slope,threshold_db=0,
                       minimum=.5,maximum=1,math_reference=self.nearest)
        np.testing.assert_array_equal(got,np.clip(expected,.5,1))


if __name__=='__main__':unittest.main()
