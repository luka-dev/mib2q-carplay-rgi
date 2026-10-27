import tempfile
from pathlib import Path
import unittest

import numpy as np

from amp_b_fft import load_complex_fft_table, table_fft, LOADER
from amp_b_transform import forward_real, inverse_real
from amp_b_window import load_spectral_tables


class StoredComplexFFTTests(unittest.TestCase):
    def test_factorization_against_direct_dft_and_numpy(self):
        rng = np.random.default_rng(222)
        for n in (4, 8, 16, 512):
            x = rng.normal(size=n)+1j*rng.normal(size=n)
            w = np.exp(-2j*np.pi*np.arange(n//2)/n)
            np.testing.assert_allclose(table_fft(x, w), np.fft.fft(x), atol=5e-14)
            if n <= 16:
                direct = np.exp(-2j*np.pi*np.outer(np.arange(n),np.arange(n))/n)@x
                np.testing.assert_allclose(table_fft(x, w), direct, atol=6e-14)

    def test_actual_table_stride_and_precision(self):
        tables = load_complex_fft_table()
        # Independent actual words, using recorded DM address and even entries.
        import sys
        sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
        from extract_amp_sharc_app import parse_blocks
        from audit_amp_output_bindings import loader_dm
        raw = LOADER.read_bytes();dm = loader_dm(raw,parse_blocks(raw))
        words = np.array([dm[0xe4754+4*i+j] for i in range(256) for j in (0,1)],dtype='<u4')
        f = words.view('<f4').astype(float)
        np.testing.assert_array_equal(tables['twiddles'],f[::2]+1j*f[1::2])
        error = abs(tables['twiddles']-np.exp(-2j*np.pi*np.arange(256)/512)).max()
        self.assertGreater(error,1e-8);self.assertLess(error,1e-7)
        self.assertFalse(tables['metadata']['hardware_bit_exact'])

    def test_stored_table_really_changes_transform_and_preserves_scale(self):
        tables=load_complex_fft_table();packing=load_spectral_tables()['twiddles']
        x=np.random.default_rng(223).normal(size=1024)
        opts=dict(twiddles=packing,complex_twiddles=tables['twiddles'])
        y=forward_real(x,**opts)
        self.assertGreater(abs(y-forward_real(x,twiddles=packing)).max(),1e-7)
        np.testing.assert_allclose(y,np.fft.rfft(x),atol=2e-5,rtol=0)
        np.testing.assert_allclose(inverse_real(y,**opts),x,atol=2e-6,rtol=0)
        for x in (np.ones(1024),(-1.)**np.arange(1024),np.eye(1,1024)[0]):
            np.testing.assert_allclose(forward_real(x,**opts),np.fft.rfft(x),atol=1e-12)

    def test_invalid_shapes_and_nonfinite_rejected(self):
        for x,w in [(np.zeros(3),np.zeros(1)),(np.zeros(8),np.zeros(3)),
                    (np.zeros((8,1)),np.zeros(4)),(np.zeros(8),np.full(4,np.nan))]:
            with self.assertRaises(ValueError):table_fft(x,w)

    def test_final_radix4_uses_exact_rotation_not_quarter_table_entry(self):
        x=np.array([1+2j,3-4j,5+6j,7-8j])
        #The kernel's last stage has no PM loads, even if caller table differs.
        np.testing.assert_allclose(table_fft(x,np.array([.7+.2j,.003-.98j])),np.fft.fft(x),atol=0)

    def test_wrong_source_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'wrong.bin';path.write_bytes(b'not firmware')
            with self.assertRaises(ValueError):load_complex_fft_table(path)


if __name__=='__main__':unittest.main()
