import unittest
import numpy as np
from scipy.signal import lfilter
from amp_b_decorrelator import Allpass, NestedAllpass, FixedDelay, BTwoChannelTailReference


class BDecorrelatorTests(unittest.TestCase):
    def test_single_allpass_independent_transfer(self):
        x=np.random.default_rng(92).normal(size=2301)
        b=np.zeros(18);a=b.copy();b[0]=.6;b[-1]=1;a[0]=1;a[-1]=.6
        c=Allpass(17,.6)
        actual=np.r_[c.process(x[:13]),c.process(x[13:512]),c.process(x[512:])]
        np.testing.assert_allclose(actual,lfilter(b,a,x),atol=1e-14)

    def test_nested_allpass_independent_polynomial(self):
        d1,d2,g1,g2=11,23,.3,-.5
        b=np.zeros(d1+d2+1);a=b.copy()
        b[0]=g2;b[d1]+=g1*g2;b[d2]+=g1;b[-1]+=1
        a[0]=1;a[d1]+=g1;a[d2]+=g1*g2;a[-1]+=g2
        x=np.random.default_rng(93).normal(size=2301)
        c=NestedAllpass(d1,g1,d2,g2)
        actual=np.r_[c.process(x[:29]),c.process(x[29:])]
        np.testing.assert_allclose(actual,lfilter(b,a,x),atol=1e-14)
        series=Allpass(d2,g2).process(Allpass(d1,g1).process(x))
        self.assertGreater(np.max(abs(series-actual)),.1)

    def test_allpass_flat_magnitude_but_coherent_sum_not_flat(self):
        x=np.zeros(32768);x[0]=1
        for c in (Allpass(31,.6),NestedAllpass(13,.4,37,-.5)):
            y=c.process(x);h=np.fft.rfft(y)
            np.testing.assert_allclose(abs(h),1,atol=2e-14)
            self.assertGreater(np.ptp(abs(1+h)),1.5)

    def test_fixed_delay_and_empty_audio(self):
        x=np.arange(53.)
        d=FixedDelay(17)
        np.testing.assert_array_equal(np.r_[d.process(x[:11]),d.process(x[11:])],np.r_[np.zeros(17),x[:-17]])
        np.testing.assert_array_equal(FixedDelay(0).process(x),x)
        self.assertEqual(Allpass(1,.2).process([]).shape,(0,))

    def test_complete_fixed_tail_neutral_and_gain_delay_order(self):
        c=BTwoChannelTailReference(common_delay=3,channel_delays=[2,4],pre_eq_gain=.5,
            eq_records=[],nested=[None,None],single=[None,None],final_gain=.7)
        x=np.zeros((64,2));x[0]=1
        y=c.process(x);expected=np.zeros_like(x);expected[5,0]=.35;expected[7,1]=.35
        np.testing.assert_allclose(y,expected,atol=1e-16)
        self.assertEqual(c.process(x[:0]).shape,(0,2))

    def test_invalid_feedback_or_delay_rejected(self):
        for delay,gain in ((0,.5),(3,1),(3,float('nan'))):
            with self.assertRaises(ValueError):Allpass(delay,gain)

    def test_nested_zero_requests_use_one_addressed_cell_not_bypass(self):
        x=np.random.default_rng(96).normal(size=1301)
        for requested1,requested2 in ((0,96),(48,0),(0,0)):
            d1,d2=max(1,requested1),max(1,requested2);g1,g2=0,-.3
            b=np.zeros(d1+d2+1);a=b.copy()
            b[0]=g2;b[d1]+=g1*g2;b[d2]+=g1;b[-1]+=1
            a[0]=1;a[d1]+=g1;a[d2]+=g1*g2;a[-1]+=g2
            ref=NestedAllpass(requested1,g1,requested2,g2,allow_zero_delay_cells=True)
            y=np.r_[ref.process(x[:31]),ref.process(x[31:])]
            np.testing.assert_allclose(y,lfilter(b,a,x),atol=1e-14)
            self.assertGreater(np.max(abs(y-x)),.1)
        with self.assertRaises(ValueError):NestedAllpass(0,0,96,-.3)
        with self.assertRaises(ValueError):NestedAllpass(-1,0,96,-.3,allow_zero_delay_cells=True)

    def test_full_tail_active_filters_against_independent_transfer(self):
        x=np.random.default_rng(95).normal(size=(2501,2))
        cfg=dict(common_delay=3,channel_delays=[2,4],pre_eq_gain=.8,
            eq_records=[[.2,.1,.05,.3,-.1]],
            nested=[dict(delay1=7,gain1=.3,delay2=11,gain2=-.4)]*2,
            single=[dict(delay=13,gain=.2)]*2,final_gain=.9)
        c=BTwoChannelTailReference(**cfg)
        y=np.r_[c.process(x[:129]),c.process(x[129:513]),c.process(x[513:])]
        expected=np.empty_like(x)
        for ch,delay in enumerate([5,7]):
            z=lfilter([0]*delay+[.8],[1],x[:,ch])
            z=lfilter([.2,.1,.05],[1,-.3,.1],z)
            b=np.zeros(19);a=b.copy()
            b[[0,7,11,18]]=[-.4,-.12,.3,1]
            a[[0,7,11,18]]=[1,.3,-.12,-.4]
            z=lfilter(b,a,z)
            b=np.zeros(14);a=b.copy();b[[0,13]]=[.2,1];a[[0,13]]=[1,.2]
            expected[:,ch]=.9*lfilter(b,a,z)
        np.testing.assert_allclose(y,expected,atol=1e-14)


if __name__=='__main__':unittest.main()
