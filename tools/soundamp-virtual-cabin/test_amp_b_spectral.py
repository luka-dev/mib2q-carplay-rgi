import unittest
import numpy as np
from amp_b_spectral import (POWER_MIN,POWER_MAX,NORMALIZE_EPSILON,
                            clamped_power,magnitude,normalize,sum_difference)


class BSpectralHelperTests(unittest.TestCase):
    def test_constants_are_float32_words(self):
        self.assertEqual(POWER_MIN,float(np.float32(1e-10)))
        self.assertEqual(POWER_MAX,float(np.float32(1e30)))
        self.assertEqual(NORMALIZE_EPSILON,float(np.float32(1e-20)))

    def test_magnitude_and_normalize_preserve_phase_and_reconstruct(self):
        z=np.array([3+4j,-3+4j,-3-4j,3-4j,0j,1e-7j])
        mag,phase=normalize(z)
        np.testing.assert_allclose(magnitude(z)[:4],5)
        np.testing.assert_allclose(mag*phase,z,atol=1e-15)
        np.testing.assert_allclose(abs(phase[:4]),1)
        self.assertEqual(phase[4],0)
        self.assertLess(abs(phase[5]),.011)

    def test_power_clamps_are_not_audio_injection(self):
        np.testing.assert_array_equal(clamped_power([0j,1e20+1e20j]),[POWER_MIN,POWER_MAX])
        mag,phase=normalize([0j])
        self.assertGreater(mag[0],0)
        self.assertEqual((mag*phase)[0],0)

    def test_power_workspace_is_overwritten_not_used_as_input(self):
        workspace=np.array([100.,np.nan])
        mag,phase=normalize([3+4j,0j],power_workspace=workspace)
        np.testing.assert_allclose(workspace,[25,POWER_MIN])
        np.testing.assert_allclose(mag,[5,np.sqrt(POWER_MIN+NORMALIZE_EPSILON)])
        np.testing.assert_allclose(phase,[.6+.8j,0])

    def test_half_encode_unscaled_decode_is_unity(self):
        rng=np.random.default_rng(94)
        left=rng.normal(size=513)+1j*rng.normal(size=513)
        right=rng.normal(size=513)+1j*rng.normal(size=513)
        mid,side=sum_difference(left,right,half=True)
        a,b=sum_difference(mid,side,half=False)
        np.testing.assert_allclose(a,left,atol=1e-15)
        np.testing.assert_allclose(b,right,atol=1e-15)
        np.testing.assert_allclose(abs(mid)**2+abs(side)**2,
                                   .5*(abs(left)**2+abs(right)**2),atol=1e-14)

    def test_empty_and_invalid_inputs(self):
        self.assertEqual(normalize([])[0].shape,(0,))
        for bad in ([[1,2]],[np.nan],[np.inf]):
            with self.assertRaises(ValueError):magnitude(bad)
        for power in ([-1],np.ones(1,dtype=int),np.ones(2)):
            with self.assertRaises(ValueError):normalize([1j],power_workspace=power)
        with self.assertRaises(TypeError):normalize([1j],supplied_power=[10])


if __name__=='__main__':unittest.main()
