import unittest
import numpy as np
from scipy.signal import lfilter
from amp_b_output import BSixChannelTailReference, BFourChannelTailReference, BOutputFinishReference


def config(delays=(0,0), gain=1., records=(), final=1.):
    return dict(channel_delays=list(delays),pre_eq_gain=gain,eq_records=list(records),
                single=[None,None],final_gain=final)


class BOutputTests(unittest.TestCase):
    def test_six_fanout_copies_before_individual_pair_processing(self):
        pairs=[config((2,3),2,final=.5),config((4,5),3,final=.25),config((6,7),4,final=.125)]
        c=BSixChannelTailReference(pre_gain=.6,common_delay=5,nested=[[None,None]]*2,
                                  pair_configs=pairs,final_gain=.7)
        x=np.zeros((37,2));x[0]=[1,-2]
        actual=np.r_[c.process(x[:8]),c.process(x[8:])]
        expected=np.zeros((37,6))
        for p in range(3):
            for ch in range(2):
                expected[7+2*p+ch,2*p+ch]=[1,-2][ch]*.6*.7*[1,.75,.5][p]
        np.testing.assert_allclose(actual,expected,atol=1e-15)

    def test_six_nested_stages_precede_all_three_pair_filters(self):
        x=np.random.default_rng(96).normal(size=(1501,2))
        nested=dict(delay1=3,gain1=.2,delay2=7,gain2=-.3)
        pairs=[config(records=[[.4,.1,.02,.3,-.1]])]*3
        c=BSixChannelTailReference(pre_gain=.6,common_delay=2,nested=[[nested,nested]]*2,
                                  pair_configs=pairs,final_gain=.7)
        actual=np.r_[c.process(x[:37]),c.process(x[37:])]
        b=np.zeros(11);a=b.copy();b[[0,3,7,10]]=[-.3,-.06,.2,1];a[[0,3,7,10]]=[1,.2,-.06,-.3]
        z=lfilter([0,0,.6],[1],x,axis=0)
        z=lfilter(b,a,lfilter(b,a,z,axis=0),axis=0)
        z=.7*lfilter([.4,.1,.02],[1,-.3,.1],z,axis=0)
        np.testing.assert_allclose(actual,np.tile(z,(1,3)),atol=1e-14)

    def test_four_fanout_independent_delays_and_filters(self):
        x=np.zeros((20,2));x[0]=[1,2]
        c=BFourChannelTailReference(common_delay=3,
            pair_configs=[config((1,2),2),config((4,5),3)],final_gain=.5)
        expected=np.zeros((20,4));expected[4,0]=1;expected[5,1]=2
        expected[7,2]=1.5;expected[8,3]=3
        np.testing.assert_array_equal(c.process(x),expected)

    def test_merge_routes_only_first_four_six_lanes(self):
        c=BOutputFinishReference(six_gain=2,four_gain=3,two_gain=4,
                                six_eq=[],two_eq=[],final_eq=[],final_gain=.5)
        a=np.arange(6.)[None,:];b=np.ones((1,4));d=np.ones((1,2))
        y=c.process(a,b,d)
        np.testing.assert_array_equal(y,[[1.5,2.5,3.5,4.5,4,5,2,2]])
        np.testing.assert_array_equal(a,np.arange(6.)[None,:])

    def test_finish_filter_order_and_state_continuity(self):
        rng=np.random.default_rng(97)
        a=rng.normal(size=(1025,6));b=rng.normal(size=(1025,4));d=rng.normal(size=(1025,2))
        rec=[.4,.1,.02,.3,-.1];rec2=[.7,.2,.1,.2,-.05]
        c=BOutputFinishReference(six_gain=.2,four_gain=.3,two_gain=.4,
            six_eq=[rec],two_eq=[rec2],final_eq=[rec],final_gain=.5)
        actual=np.r_[c.process(a[:53],b[:53],d[:53]),c.process(a[53:],b[53:],d[53:])]
        z=a*.2;z[:,:4]+=b*.3
        z=np.column_stack([lfilter(rec[:3],[1,-rec[3],-rec[4]],z,axis=0),
                           lfilter(rec2[:3],[1,-rec2[3],-rec2[4]],d*.4,axis=0)])
        expected=.5*lfilter(rec[:3],[1,-rec[3],-rec[4]],z,axis=0)
        np.testing.assert_allclose(actual,expected,atol=1e-14)

    def test_merge_has_no_implicit_normalization_or_clipping(self):
        def finish():return BOutputFinishReference(six_gain=1,four_gain=1,two_gain=1,
                                    six_eq=[],two_eq=[],final_eq=[],final_gain=1)
        six=np.ones((3,6));four=np.ones((3,4));two=np.ones((3,2))
        np.testing.assert_array_equal(finish().process(six,four,two),np.tile([2,2,2,2,1,1,1,1],(3,1)))
        np.testing.assert_array_equal(finish().process(six,-four,two),np.tile([0,0,0,0,1,1,1,1],(3,1)))

    def test_empty_and_reject_invented_four_channel_allpass(self):
        c=BFourChannelTailReference(common_delay=0,pair_configs=[config()]*2,final_gain=1)
        self.assertEqual(c.process(np.zeros((0,2))).shape,(0,4))
        bad=config();bad['single'][0]=dict(delay=3,gain=.2)
        with self.assertRaises(ValueError):
            BFourChannelTailReference(common_delay=0,pair_configs=[bad,config()],final_gain=1)


if __name__=='__main__':unittest.main()
