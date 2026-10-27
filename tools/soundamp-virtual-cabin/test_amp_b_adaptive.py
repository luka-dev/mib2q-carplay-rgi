import unittest
import numpy as np
from scipy.signal import lfilter
from amp_b_adaptive import (band_sum,expand_bands,frequency_smooth,level_gain,
    BandLevelMaskReference,BandTransientMaskReference,FrameVectorSmoother,POWER_TO_GAIN_FACTOR,
    spectral_ramp,spectral_subtract)


class BAdaptiveTests(unittest.TestCase):
    def test_bands_are_sums_with_half_open_boundaries(self):
        np.testing.assert_array_equal(band_sum([1,2,3,4,5],[0,2,2,5]),[3,0,12])
        np.testing.assert_array_equal(expand_bands([3,99,12],[0,2,2,5],5),[3,3,12,12,12])
        # Equal per-bin energy does not mean equal energy for unequal widths.
        np.testing.assert_array_equal(band_sum(np.ones(5),[0,2,5]),[2,3])

    def test_ramp_retains_firmware_low_bin_zero_quirk(self):
        np.testing.assert_array_equal(spectral_ramp(bins=7,start=2,end=6,low_bin=2,high_bin=6),
                                      [2,2,3,4,5,6,6])
        np.testing.assert_array_equal(spectral_ramp(bins=6,start=2,end=6,low_bin=0,high_bin=4),
                                      [2,3,4,5,6,6])
        for lo,hi in ((2,2),(-1,3),(0,6)):
            with self.assertRaises(ValueError):spectral_ramp(bins=6,start=0,end=1,low_bin=lo,high_bin=hi)

    def test_subtraction_piecewise_gain_and_disabled_passthrough(self):
        kw=dict(start=2,end=2,low_bin=0,high_bin=2,negative_gain=.25)
        left,right=spectral_subtract([1,1,1],[1,2,3],[0,4,1],enabled=True,**kw)
        np.testing.assert_array_equal(left,[-.25,0,1])
        np.testing.assert_array_equal(right,[-.5,2,-.25])
        left,right=spectral_subtract([1,1,1],[1,2,3],[0,4,1],enabled=False,**kw)
        np.testing.assert_array_equal(left,[1,2,3]);np.testing.assert_array_equal(right,[0,4,1])

    def test_frequency_smoothing_against_independent_iir_with_endpoint_state(self):
        x=np.random.default_rng(98).normal(size=513);g,h=.3,.6
        a,_=lfilter([g],[1,-h],x,zi=[h*x[0]])
        b,_=lfilter([g],[1,-h],a[::-1],zi=[h*a[-1]])
        np.testing.assert_allclose(frequency_smooth(x,g,h),b[::-1],atol=1e-15)
        np.testing.assert_array_equal(frequency_smooth(x,1,99),x)
        np.testing.assert_allclose(frequency_smooth([2],g,h),[2*(g+h)**2])

    def test_level_gain_uses_point_one_not_c4d17_point_zero_five(self):
        self.assertEqual(POWER_TO_GAIN_FACTOR,float(np.float32(.1)))
        y=level_gain([10,0,-2],[0,0,0],[10,0,-2],slope=0,threshold_db=0,minimum=.01,maximum=1)
        np.testing.assert_allclose(y,[10**(-10*POWER_TO_GAIN_FACTOR),1,1])
        self.assertLess(y[0],.101)
        y=level_gain([100,0],[0,0],[100,0],slope=0,threshold_db=0,minimum=.2,maximum=.8)
        np.testing.assert_array_equal(y,[.2,.8])

    def test_full_mask_two_updates_against_scalar_band_equations(self):
        p=[1,0,.6,.4,.2,.8,.25,.75,.8,.2,1,0,.5,3,1,.01]
        power=np.array([2.,3.]);db=np.array([1.,2.]);gain=np.array([.5,.8])
        c=BandLevelMaskReference(bins=5,edges=[0,2,5],params=p,
            power_history=power,db_history=db,gain_history=gain)
        for x in ([2,2,2,2,2],[.5,1,4,2,1]):
            x=np.array(x,dtype=float);expected=[]
            for j,(a,b) in enumerate([(0,2),(2,5)]):
                power[j]=.6*sum(v*v for v in x[a:b])+.4*power[j]
                fast=10*np.log10(1+power[j]);db[j]=.2*fast+.8*db[j]
                excess=fast-db[j]-3
                raw=10**((-.5*excess if excess>0 else 0)*float(np.float32(.1)))
                raw=min(1,max(.01,raw))
                gain[j]=(.25*raw+.75*gain[j]) if gain[j]<raw else (.8*raw+.2*gain[j])
                expected.extend([gain[j]]*(b-a))
            np.testing.assert_allclose(c.process(x,enabled=True),expected,atol=1e-15)
        np.testing.assert_allclose(c.power,power)
        np.testing.assert_allclose(c.db,db)
        np.testing.assert_allclose(c.gain,gain)

    def test_disabled_mask_is_unity_without_history_updates(self):
        p=[0]*16;p[14]=1
        c=BandLevelMaskReference(bins=3,edges=[0,3],params=p,
                                power_history=[5],db_history=[6],gain_history=[7])
        np.testing.assert_array_equal(c.process([1,2,3],enabled=False),[1,1,1])
        np.testing.assert_array_equal([c.power[0],c.db[0],c.gain[0]],[5,6,7])

    def test_level_parameter_order_is_maximum_then_minimum(self):
        p=np.zeros(16);p[[2,4,6,8,10]]=1;p[14:16]=[.8,.2]
        c=BandLevelMaskReference(bins=2,edges=[0,1,2],params=p,
            power_history=[0,0],db_history=[0,0],gain_history=[0,0])
        np.testing.assert_array_equal(c.process([0,100],enabled=True),[.8,.8])
        p[4]=0
        c=BandLevelMaskReference(bins=2,edges=[0,1,2],params=p,
            power_history=[0,0],db_history=[0,0],gain_history=[0,0])
        np.testing.assert_array_equal(c.process([0,100],enabled=True),[.8,.2])
        p[14:16]=[.2,.8]
        with self.assertRaisesRegex(ValueError,'p15 minimum'):
            BandLevelMaskReference(bins=2,edges=[0,1,2],params=p,
                power_history=[0,0],db_history=[0,0],gain_history=[0,0])

    def test_vector_smoother_first_call_bypass_and_reset(self):
        c=FrameVectorSmoother(size=2,history_gain=.7,input_gain=.3)
        np.testing.assert_array_equal(c.process([100,200],enabled=False),[100,200])
        self.assertTrue(c.first)
        np.testing.assert_array_equal(c.process([2,4],enabled=True),[2,4])
        np.testing.assert_allclose(c.process([4,8],enabled=True),[2.6,5.2])
        c.process([900,900],enabled=False)
        np.testing.assert_allclose(c.history,[2.6,5.2])
        c.reset();np.testing.assert_array_equal(c.process([1,3],enabled=True),[1,3])

    def test_reject_incomplete_band_map_or_missing_history(self):
        for e in ([1,3],[0,2],[0,3,2,3],[0.,3.]):
            with self.assertRaises(ValueError):band_sum([1,2,3],e)
        with self.assertRaises(TypeError):
            BandLevelMaskReference(bins=3,edges=[0,3],params=[0]*16)

    def test_transient_74_band_squared_ratio_and_distinct_coefficient_order(self):
        p=np.zeros(88);p[2:76]=np.linspace(.1,.9,74)
        p[76:]=[.01,1,.25,.6,.75,.4,.2,.8,.9,.1,1,0]
        level=np.ones(74);gain=np.full(74,.5)
        c=BandTransientMaskReference(bins=148,edges=np.arange(0,149,2),params=p,
                                    level_history=level,gain_history=gain)
        for value in (2.,.1,3.):
            x=np.full(148,value);expected=[]
            for j in range(74):
                current=value*2  # not squared and not averaged
                level[j]=(.25*current+.75*level[j]) if level[j]<current else (.6*current+.4*level[j])
                raw=((p[2+j]*(current-level[j])+level[j])/current)**2 if level[j]<current else 1.
                gain[j]=(.2*raw+.8*gain[j]) if gain[j]<raw else (.9*raw+.1*gain[j])
                expected.extend([np.clip(gain[j],.01,1)]*2)
            np.testing.assert_allclose(c.process(x),expected,atol=1e-15)

    def test_transient_final_clamp_is_after_spatial_smoothing(self):
        p=np.zeros(88);p[2:76]=1;p[76:]=[.1,.8,1,1,0,0,1,0,1,0,.5,.7]
        c=BandTransientMaskReference(bins=74,edges=np.arange(75),params=p,
                                    level_history=np.zeros(74),gain_history=np.zeros(74))
        # Constant raw mask1 -> non-unity endpoint recursion -> output ceiling .8.
        np.testing.assert_array_equal(c.process(np.ones(74)),np.full(74,.8))

    def test_transient_zero_input_does_not_divide_zero_and_band_count_is_exact(self):
        p=np.zeros(88);p[76:]=[0,1,1,1,0,0,1,0,1,0,1,0]
        c=BandTransientMaskReference(bins=74,edges=np.arange(75),params=p,
                                    level_history=np.zeros(74),gain_history=np.zeros(74))
        np.testing.assert_array_equal(c.process(np.zeros(74)),np.ones(74))
        with self.assertRaises(ValueError):
            BandTransientMaskReference(bins=3,edges=[0,3],params=p,
                                       level_history=np.zeros(74),gain_history=np.zeros(74))


if __name__=='__main__':unittest.main()
