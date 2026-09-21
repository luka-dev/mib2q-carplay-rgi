import unittest
import numpy as np
from scipy.signal import sosfilt
from amp_b_filters import LibraryFilterCascade, coefficient_step, stable_section


class BFilterTests(unittest.TestCase):
    def test_stability_guard_strict_boundaries(self):
        for a1,a2,wanted in [(0,0,True),(.5,.25,True),(1,0,False),(0,1,False),(0,-1,False),(.6,-.5,False)]:
            self.assertEqual(stable_section([1,0,0,-a1,-a2]),wanted)

    def test_fixed_bank_matches_independent_scipy_and_partitioning(self):
        records=np.array([[.4,.2,.1,.6,-.2],[.7,-.1,.03,.2,-.1],[1,.1,.03,.4,-.2]])
        x=np.random.default_rng(73).normal(size=(1007,3))
        expected=sosfilt(np.column_stack([records[:,:3],np.ones(3),-records[:,3:]]),x,axis=0)
        c=LibraryFilterCascade(records,3)
        out=np.r_[c.process(x[:19]),c.process(x[19:])]
        np.testing.assert_allclose(out,expected,atol=1e-14)

    def test_odd_first_order_optimization_drops_b2_but_pairs_do_not(self):
        rec=[1,.1,.7,.2,0]
        x=np.zeros((20,1));x[0]=1
        first=LibraryFilterCascade([rec],1).process(x)
        pair=LibraryFilterCascade([rec,[1,0,0,0,0]],1).process(x)
        np.testing.assert_allclose(first,sosfilt([[1,.1,0,1,-.2,0]],x,axis=0))
        np.testing.assert_allclose(pair,sosfilt([[1,.1,.7,1,-.2,0]],x,axis=0))
        self.assertGreater(abs(pair[2,0]-first[2,0]),.6)

    def test_zero_sections_and_empty_audio(self):
        x=np.ones((4,2))
        np.testing.assert_array_equal(LibraryFilterCascade([],2).process(x),x)
        self.assertEqual(LibraryFilterCascade([[1,0,0,0,0]],2).process(x[:0]).shape,(0,2))

    def test_interpolation_step_is_supplied_not_invented(self):
        self.assertEqual(coefficient_step(1,2,.2),(1.2,.2))
        self.assertEqual(coefficient_step(1,1.1,.2),(1.1,0))
        self.assertEqual(coefficient_step(1,2,0),(1,0))
        self.assertEqual(coefficient_step(1,2,-.2),(.8,-.2))


if __name__=='__main__':unittest.main()
