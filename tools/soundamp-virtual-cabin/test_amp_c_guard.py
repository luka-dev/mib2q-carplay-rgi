"""Compile actual C table/guard, compare all routes to extracted firmware audit."""
import ctypes
import itertools
from pathlib import Path
import subprocess
import tempfile
import unittest
import numpy as np
from amp_dsp_chain import AmpChain, ROOT


class CGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='amp-c-guard-')
        lib = Path(cls.temp.name) / 'guard.so'
        subprocess.run(['cc', '-std=c99', '-O2', '-shared', '-fPIC',
                        '-DSPA_STEREO_UPMIX_TESTING', '-I'+str(ROOT/'hook/audio'),
                        *[str(ROOT/'hook/audio'/f) for f in ('stereo_upmix.c', 'scene_engine_math.c', 'scene_clusters.c',
                                                              'scene_engine.c', 'scene_mask.c')],
                        '-lm', '-o', str(lib)], check=True, capture_output=True)
        cls.c = ctypes.CDLL(str(lib))
        cls.c.spa_stereo_upmix_input_matrix.restype = ctypes.POINTER(ctypes.c_float)
        cls.guard = cls.c.spa_stereo_upmix_test_guard_s16
        cls.guard.argtypes = [ctypes.POINTER(ctypes.c_int16), ctypes.c_int]
        cls.guard.restype = None

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_all_48_coefficients_match_extracted_output_rows(self):
        c = np.ctypeslib.as_array(self.c.spa_stereo_upmix_input_matrix(), shape=(48,)).reshape(6, 8)
        np.testing.assert_allclose(c, AmpChain().M.T, rtol=1e-7, atol=1e-10)

    def test_all_corners_and_random_frames_against_independent_matrix(self):
        corners = np.array(list(itertools.product((-32768, 0, 32767), repeat=6)), dtype=np.int16)
        random = np.random.default_rng(1909).integers(-32768, 32768, size=(20000, 6), dtype=np.int16)
        original = np.concatenate((corners, random))
        m = AmpChain().M
        for adjust_lfe in (0, 1):
            out = original.copy()
            for frame in out:
                self.guard(frame.ctypes.data_as(ctypes.POINTER(ctypes.c_int16)), adjust_lfe)
            self.assertLessEqual(np.max(abs(out.astype(float) @ m.T)), 32275.01)
            self.assertTrue(np.all(out[:, 3].astype(float)*original[:, 3] >= 0))
            self.assertTrue(np.all(abs(out[:, 3].astype(int)) <= abs(original[:, 3].astype(int))))
            np.testing.assert_array_equal(out[original[:, 3] == 0, 3], 0)

    def test_noop_when_all_routes_have_headroom(self):
        x = np.array([2000, -1500, 100, 50, 1000, -900], dtype=np.int16)
        before = x.copy()
        self.guard(x.ctypes.data_as(ctypes.POINTER(ctypes.c_int16)), 1)
        np.testing.assert_array_equal(x, before)


if __name__ == '__main__':
    unittest.main()
