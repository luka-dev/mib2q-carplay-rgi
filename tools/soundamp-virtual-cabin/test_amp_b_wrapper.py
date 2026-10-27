import unittest
import numpy as np

from amp_b_wrapper import BWrapperDoubleBufferReference, wrapper_evidence
from amp_dsp_chain import delay_channels


def passthrough(x):
    return np.column_stack((x,x[:,:3]))


def wrapper(callback=passthrough, selector=0):
    return BWrapperDoubleBufferReference(callback=callback,input_banks=np.zeros((2,512,5)),
        output_banks=np.zeros((2,512,8)),selector=selector,offset=0,
        assume_on_time_successful_callbacks=True)


def process(ref,x,size=64):
    return np.concatenate([ref.process_block(x[i:i+size]) for i in range(0,len(x),size)])


class WrapperTests(unittest.TestCase):
    def test_identity_callback_has_two_hop_latency_both_selectors(self):
        x=np.random.default_rng(501).normal(size=(4096,5))
        expected=delay_channels(passthrough(x),[1024]*8)
        for selector in (0,1):
            ref=wrapper(selector=selector)
            np.testing.assert_array_equal(process(ref,x),expected)
            self.assertEqual(ref.completed_banks,8)

    def test_scheduler_partition_invariance(self):
        x=np.random.default_rng(502).normal(size=(4096,5))
        expected=process(wrapper(),x,512)
        for size in (1,8,32,64,128,256):
            np.testing.assert_array_equal(process(wrapper(),x,size),expected)

    def test_explicit_old_output_banks_are_returned_before_new_callback(self):
        initial=np.stack((np.full((512,8),17.),np.full((512,8),23.)))
        ref=BWrapperDoubleBufferReference(callback=passthrough,input_banks=np.zeros((2,512,5)),
            output_banks=initial,selector=0,offset=0,assume_on_time_successful_callbacks=True)
        x=np.ones((1536,5));y=process(ref,x,512)
        np.testing.assert_array_equal(y[:512],17);np.testing.assert_array_equal(y[512:1024],23)
        np.testing.assert_array_equal(y[1024:],1)

    def test_callback_runs_only_after_completed_bank_and_owns_copy(self):
        calls=[]
        def callback(x):
            calls.append(x.copy());x[:]=9
            return np.ones((512,8))
        ref=wrapper(callback)
        ref.process_block(np.ones((511,5)))
        self.assertEqual(calls,[])
        np.testing.assert_array_equal(ref.process_block(np.full((1,5),2.)),0)
        self.assertEqual(len(calls),1)
        np.testing.assert_array_equal(calls[0][-1],2)
        np.testing.assert_array_equal(ref.input_banks[0,-1],2)

    def test_library_delay_adds_to_wrapper_not_replaces_it(self):
        class Delayed:
            def __init__(self):self.old=np.zeros((512,8))
            def __call__(self,x):
                result=self.old;self.old=passthrough(x);return result
        x=np.zeros((4096,5));x[0]=[1,2,3,4,5]
        y=process(wrapper(Delayed()),x)
        np.testing.assert_array_equal(y,delay_channels(passthrough(x),[1536]*8))

    def test_mode5_ampchain_delay_matches_explicit_double_buffer(self):
        from amp_dsp_chain import AmpChain
        from test_amp_common_input import upstream,b_scenario
        class Mode5Callback:
            def __init__(self,ref):self.ref=ref
            def __call__(self,x):return self.ref.process(*x[:,:3].T)
        scenario=b_scenario(1024)
        chain=AmpChain(upstream_scenario=upstream(10),first_b_scenario=scenario)
        x=np.random.default_rng(504).normal(0,.01,(4096,6))
        stages=chain.process(x)
        input5=np.zeros((len(x),5));input5[:,:3]=stages['common_input8'][:,:3]
        literal=process(wrapper(Mode5Callback(scenario['reference'])),input5)
        np.testing.assert_array_equal(literal,stages['first_b8'])

    def test_copy_addresses_match_independent_flat_memory_model(self):
        # Literal flat DM address arithmetic, no ref bank-array indexing.
        dm={};rng=np.random.default_rng(503);x=rng.normal(size=(3072,5));out=[]
        selector=offset=0
        for start in range(0,len(x),64):
            ib=0xB95EA+2560*selector;ob=0xBA9EA+4096*selector
            for lane in range(5):
                for i in range(64):dm[ib+512*lane+offset+i]=x[start+i,lane]
            out.append(np.array([[dm.get(ob+512*lane+offset+i,0) for lane in range(8)] for i in range(64)]))
            offset+=64
            if offset==512:
                for lane in range(8):
                    for i in range(512):dm[ob+512*lane+i]=dm[ib+512*(lane%5)+i]
                selector=(selector+1)&1;offset=0
        np.testing.assert_array_equal(process(wrapper(),x),np.concatenate(out))

    def test_bounds_and_explicit_callback_condition(self):
        ref=wrapper();ref.process_block(np.zeros((64,5)))
        for x in (np.zeros((449,5)),np.zeros((0,5)),np.zeros((64,3)),np.full((64,5),np.nan)):
            with self.assertRaises(ValueError):ref.process_block(x)
        with self.assertRaises(ValueError):
            BWrapperDoubleBufferReference(callback=passthrough,input_banks=np.zeros((2,512,5)),
                output_banks=np.zeros((2,512,8)),selector=0,offset=0,
                assume_on_time_successful_callbacks=False)

    def test_actual_loader_and_decoded_address_pins(self):
        evidence=wrapper_evidence()
        self.assertEqual(evidence['steady_wrapper_delay_samples'],1024)
        self.assertEqual(evidence['input_bank_words'],5*512)
        self.assertEqual(evidence['output_bank_words'],8*512)


if __name__=='__main__':unittest.main()
