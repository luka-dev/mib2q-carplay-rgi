import tempfile
from pathlib import Path
import unittest

import numpy as np

from amp_b_wrapper import BWrapperDoubleBufferReference
from amp_b_wrapper_control import (WrapperCallbackControl,SuccessfulLibraryTransition,
                                  alternate_stereo_fanout,callback_error_status,control_evidence)


def transition(word=1,count=0):
    return SuccessfulLibraryTransition(transition_word=word,counter=count,ready_word=1,
        assume_successful_calls_and_no_intervening_writes=True)


class WrapperControlTests(unittest.TestCase):
    def test_exact_error_words_against_literal_btgl_branch(self):
        for word in (0,1,0x2000,0x4000,0x8000,0xc000,0x4001,0x8001,0xffffffff):
            expected=((word^(1<<14))==0 or (word^(1<<15))==0)
            self.assertEqual(callback_error_status(word),expected)
        with self.assertRaises(ValueError):callback_error_status(-1)

    def test_inhibit_equals_one_not_low_bit(self):
        for word in (0,1,2,3,0xffffffff):
            c=WrapperCallbackControl(3,7,9)
            kwargs={} if word==1 else {'process_status':0}
            result=c.invoke(inhibit_word=word,**kwargs)
            self.assertEqual(result['clear_output_words'],4096 if word==1 else 0)
            self.assertEqual(c.error,9 if word==1 else 0)
            self.assertEqual(c.state,3);self.assertEqual(c.busy,0)
            self.assertEqual(result['actions'][0],'busy1')
            self.assertEqual(result['actions'][-1],'busy0')

    def test_process_error_clears_bank_sets_error_three_without_changing_state(self):
        for status in (0x4000,0x8000):
            c=WrapperCallbackControl(3,0,9);r=c.invoke(inhibit_word=0,process_status=status)
            self.assertEqual(r['actions'],['busy1','process_12DE3B','clear4096','busy0'])
            self.assertEqual((c.state,c.error,c.busy),(3,3,0))
            self.assertFalse(r['input_cleared_by_callback'])

    def test_initialization_statuses_and_delayed_state_three_store(self):
        cases=[(0x4000,None,1,2),(0x8000,None,1,2),(0,0x4000,1,1),
               (0,0x8000,1,1),(0,0,3,0),(0xc000,0xc000,3,0)]
        for init,param,state,error in cases:
            c=WrapperCallbackControl(1,0,9)
            r=c.invoke(inhibit_word=1,initialize_status=init,parameter_status=param)
            self.assertEqual((c.state,c.error,c.busy),(state,error,0))
            self.assertEqual(r['clear_output_words'],4096)
            self.assertEqual(r['actions'][:3],['busy1','clear4096','initialize_12DD7D'])
            self.assertEqual('parameter_131A65_0800AFFE' in r['actions'],param is not None)

    def test_other_callback_states_clear_without_resetting_error(self):
        for state in (0,2,4,0xffffffff):
            c=WrapperCallbackControl(state,1,11);r=c.invoke(inhibit_word=0)
            self.assertEqual(r['actions'],['busy1','clear4096','busy0'])
            self.assertEqual((c.state,c.error,c.busy),(state,11,0))

    def test_unreached_missing_statuses_reject_without_mutating(self):
        for state,kwargs in [(3,{}),(1,{}),(0,{'process_status':0}),
                             (1,{'initialize_status':0x4000,'parameter_status':0})]:
            c=WrapperCallbackControl(state,7,8)
            with self.assertRaises(ValueError):c.invoke(inhibit_word=0,**kwargs)
            self.assertEqual((c.state,c.busy,c.error),(state,7,8))

    def test_inhibited_callback_does_not_erase_already_copied_or_other_bank_audio(self):
        state=WrapperCallbackControl(3,0,0)
        def callback(_):
            r=state.invoke(inhibit_word=1)
            self.assertEqual(r['clear_output_words'],4096)
            return np.zeros((512,8))
        banks=np.empty((2,512,8));banks[0]=.3;banks[1]=.7
        wrapper=BWrapperDoubleBufferReference(callback=callback,input_banks=np.zeros((2,512,5)),
            output_banks=banks,selector=0,offset=0,assume_on_time_successful_callbacks=True)
        # The steady bank helper is used only with explicitly synchronous callbacks.
        actual=np.concatenate([wrapper.process_block(np.ones((64,5))) for _ in range(24)])
        np.testing.assert_array_equal(actual[:512],.3)
        np.testing.assert_array_equal(actual[512:1024],.7)
        np.testing.assert_array_equal(actual[1024:],0)

    def test_alternate_handler_literal_four_pair_copy_has_no_bank_delay(self):
        x=np.arange(65*5.).reshape(65,5)
        expected=np.empty((65,8))
        for pair in range(4):
            for frame in range(65):
                expected[frame,2*pair]=x[frame,0]
                expected[frame,2*pair+1]=x[frame,1]
        np.testing.assert_array_equal(alternate_stereo_fanout(x),expected)
        x[:,2:]=1e6
        np.testing.assert_array_equal(alternate_stereo_fanout(x),expected)

    def test_ten_successful_transition_calls_still_run_algorithm_then_clear(self):
        c=transition();seen=[];stateful=[0]
        def algorithm(x):
            seen.append((c.counter,x.copy()));stateful[0]+=1
            return np.full((512,8),stateful[0])
        for i in range(10):
            r=c.process_bank(np.ones((512,5)),algorithm)
            np.testing.assert_array_equal(r['audio_at_12DF07'],0)
            np.testing.assert_array_equal(seen[-1][1],0)
            self.assertEqual(seen[-1][0],i+1)
            self.assertEqual(r['actions'][:4],['increment_counter','clear5_inputs',
                                              'run_supplied_algorithm','clear8_outputs'])
        self.assertEqual((c.transition,c.counter,stateful[0]),(0,0,10))
        r=c.process_bank(np.ones((512,5)),algorithm)
        np.testing.assert_array_equal(r['audio_at_12DF07'],11)
        np.testing.assert_array_equal(seen[-1][1],1)
        self.assertFalse(r['algorithm_state_reset_inferred'])

    def test_transition_comparisons_are_exact_and_counter_wraps_uint32(self):
        for word in (0,2,3,0xffffffff):
            c=transition(word,7);r=c.process_bank(np.ones((512,5)),lambda x:np.ones((512,8)))
            self.assertEqual(c.counter,7);np.testing.assert_array_equal(r['library_input'],1)
        c=transition(1,10);c.process_bank(np.ones((512,5)),lambda x:np.ones((512,8)))
        self.assertEqual((c.transition,c.counter),(1,11)) # NOT counter>=10
        c=transition(1,0xffffffff);c.process_bank(np.ones((512,5)),lambda x:np.ones((512,8)))
        self.assertEqual((c.transition,c.counter),(1,0))

    def test_successful_path_contract_and_evidence_pinning(self):
        with self.assertRaises(ValueError):SuccessfulLibraryTransition(transition_word=1,counter=0,ready_word=3,
            assume_successful_calls_and_no_intervening_writes=True)
        with self.assertRaises(ValueError):SuccessfulLibraryTransition(transition_word=1,counter=0,ready_word=1,
            assume_successful_calls_and_no_intervening_writes=False)
        report=control_evidence()
        self.assertEqual(report['pins']['0x125039'],'DM(0xb95e2)=I4')
        with tempfile.TemporaryDirectory() as directory:
            bad=Path(directory)/'bad';bad.write_bytes(b'bad loader')
            with self.assertRaises(ValueError):control_evidence(bad)


if __name__=='__main__':unittest.main()
