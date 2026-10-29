"""Independent firmware-pinned regression of real first-B spectral semantics.

Local loader/CodeFlash only. Mathematical expectations use explicit scalar
equations and actual stored maps/coefficient banks, not a live-state claim.
"""
import hashlib
from pathlib import Path
import struct
import sys
import unittest

import numpy as np

from amp_b_adaptive import band_sum, expand_bands, BandLevelMaskReference, BandTransientMaskReference
from amp_b_spatial import fixed_mean
from amp_b_transform import forward_real, inverse_real

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from extract_amp_sharc_app import BITREV_SHA256, SOURCE_SHA256, parse_blocks
from audit_amp_output_bindings import loader_dm
from inspect_amp_visa import load_shortwords, decode


class RealSpectralLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = (ROOT / 'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        source = (ROOT.parents[1] / 'Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin').read_bytes()
        if hashlib.sha256(raw).hexdigest() != BITREV_SHA256 or hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
            raise AssertionError('audited firmware hashes required')
        cls.dm = loader_dm(raw, parse_blocks(raw))
        cls.mem = load_shortwords(raw)
        cls.edges = np.array([cls.dm[cls.dm[0xb9097] + i] for i in range(cls.dm[0xb9098] + 1)])
        cls.header = np.array(struct.unpack_from('<245f', source, 0x12000c))
        values = np.array([struct.unpack('<f', struct.pack('<I', cls.dm[0xe4754 + i]))[0] for i in range(512)])
        cls.twiddles = values[::2] + 1j * values[1::2]

    def test_direct_decoded_map_and_vector_size_pins(self):
        for address, instruction in {
            0x131763: 'R0=DM(0xb9098)', 0x13176c: 'R0=DM(0xb9097)',
            0x12f150: 'R12=0x4a', 0x12e6f4: 'R12=0x100', 0x12e706: 'R12=0x100',
            0x130324: 'R2=0x9', 0x13032d: 'R8=0x33',
        }.items():
            self.assertEqual(decode(self.mem, address)[1], instruction)
        self.assertEqual(self.dm[0xb9097], 0xb904c)
        self.assertEqual(self.dm[0xb9098], 74)
        self.assertEqual(len(self.edges), 75)
        np.testing.assert_array_equal(self.edges[:40], np.arange(40))
        np.testing.assert_array_equal(self.edges[-5:], [311, 347, 387, 431, 513])

    def test_real_map_band_sums_and_expansion_against_manual_bins(self):
        values = np.arange(513, dtype=float) + .25
        expected = [sum(values[k] for k in range(a, b)) for a, b in zip(self.edges[:-1], self.edges[1:])]
        np.testing.assert_array_equal(band_sum(values, self.edges), expected)
        np.testing.assert_array_equal(band_sum(np.ones(513), self.edges), np.diff(self.edges))
        self.assertEqual(expected[-1], sum(values[431:513]))
        mask = np.arange(74, dtype=float) + .125
        expanded = expand_bands(mask, self.edges, 513)
        for band, (start, end) in enumerate(zip(self.edges[:-1], self.edges[1:])):
            np.testing.assert_array_equal(expanded[start:end], np.full(end-start, mask[band]))
        self.assertEqual(expanded[512], mask[73])  # Nyquist belongs to final82-bin band.

    def test_fixed_mean_is_equal_band_weight_not_equal_bin_weight(self):
        coherence = np.arange(74, dtype=float) / 74
        actual = fixed_mean(coherence, previous=0, direct=1, feedback=0)
        expected = sum(coherence[9:60]) / 51
        self.assertAlmostEqual(actual, expected, places=14)
        bin_weighted = np.average(coherence[9:60], weights=np.diff(self.edges)[9:60])
        self.assertGreater(abs(actual-bin_weighted), .05)
        self.assertEqual((self.edges[9], self.edges[60]), (9, 113))
        self.assertEqual((self.edges[9]*48000/1024, self.edges[60]*48000/1024), (421.875, 5296.875))

    def test_level_clamp_caller_callee_pins_are_reverse_of_transient(self):
        pins = {
            0x12fa2e: 'R2=DM(+14,I5)', 0x12fa30: 'DM(M6,I7)=R2',
            0x12fa32: 'R2=DM(+15,I5)', 0x12fa36: 'DM(I7,M7)=R2',
            0x12f8e0: 'R8=DM(+5,I6)', 0x12f8e2: 'R4=DM(+4,I6)',
            0x12bb6b: 'F0=MAX(F4,F0)', 0x12bb70: 'F1=MIN(F8,F0) || R0=DM(I2,M4)',
            0x12f13d: 'I3=MODIFY(I4,+79)',
            0x12f215: 'R4=DM(-3,I3)', 0x12f217: 'R8=DM(-2,I3)',
        }
        for address, instruction in pins.items():
            self.assertEqual(decode(self.mem, address)[1], instruction)
        np.testing.assert_array_equal(self.header[64:66], [1, .5])
        for offset in (69, 157):
            self.assertLess(self.header[offset+76], self.header[offset+77])
            self.assertAlmostEqual(self.header[offset+76], .07943282)
            self.assertEqual(self.header[offset+77], 1)

    def test_actual_b01_level_bank_against_manual_temporal_math(self):
        p = self.header[50:66]
        model = BandLevelMaskReference(bins=513, edges=self.edges, params=p,
            power_history=np.zeros(74), db_history=np.zeros(74), gain_history=np.zeros(74))
        power = np.zeros(74); slow = np.zeros(74); gain = np.zeros(74)
        for amplitude in (0., 1., 100., .01):
            spectrum = np.full(513, amplitude)
            expected = []
            for i, (start, end) in enumerate(zip(self.edges[:-1], self.edges[1:])):
                power[i] = p[2]*sum(v*v for v in spectrum[start:end])+p[3]*power[i]
                fast = 10*np.log10(1+power[i])
                slow[i] = p[4]*fast+p[5]*slow[i]
                excursion = fast-slow[i]-p[13]
                target = p[12]*excursion+slow[i]+p[13] if excursion > 0 else fast
                raw = min(p[14], max(p[15], 10**((target-fast)*float(np.float32(.1)))))
                gain[i] = p[6]*raw+p[7]*gain[i] if gain[i] < raw else p[8]*raw+p[9]*gain[i]
                expected.extend([gain[i]] * (end-start))
            # Independent explicit forward/reverse recurrence, not imported smoother.
            previous = expected[0]
            for i in range(513):
                expected[i] = p[10]*expected[i]+p[11]*previous; previous = expected[i]
            previous = expected[-1]
            for i in reversed(range(513)):
                expected[i] = p[10]*expected[i]+p[11]*previous; previous = expected[i]
            np.testing.assert_allclose(model.process(spectrum, enabled=True), expected, rtol=1e-13, atol=1e-15)
        np.testing.assert_allclose(model.power, power)
        np.testing.assert_allclose(model.db, slow)
        np.testing.assert_allclose(model.gain, gain)

    def test_actual_transient_bank_accepts_ascending_bounds(self):
        for offset in (69, 157):
            p = self.header[offset:offset+88]
            model = BandTransientMaskReference(bins=513, edges=self.edges, params=p,
                level_history=np.zeros(74), gain_history=np.zeros(74))
            output = model.process(np.zeros(513))
            self.assertTrue(np.isfinite(output).all())
            self.assertGreaterEqual(output.min(), p[76])
            self.assertLessEqual(output.max(), p[77])

    def test_actual_loader_twiddles_are_used_not_ignored_or_replaced(self):
        ideal = np.exp(-2j*np.pi*np.arange(256)/1024)
        error = abs(self.twiddles-ideal).max()
        self.assertGreater(error, 1e-8)
        self.assertLess(error, 1e-7)
        signal = np.random.default_rng(73).normal(size=1024)
        custom = forward_real(signal, twiddles=self.twiddles)
        analytic = forward_real(signal)
        self.assertGreater(abs(custom-analytic).max(), 1e-7)
        np.testing.assert_allclose(inverse_real(custom, twiddles=self.twiddles), signal, rtol=0, atol=7e-7)
        # DC/Nyquist scales are independent of interior twiddle quantization.
        constant = forward_real(np.ones(1024), twiddles=self.twiddles)
        self.assertEqual(constant[0], 1024)
        self.assertEqual(constant[-1], 0)


if __name__ == '__main__':
    unittest.main()
