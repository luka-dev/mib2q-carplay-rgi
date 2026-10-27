import json
from pathlib import Path
import unittest
import numpy as np
from scipy.signal import sosfilt, sosfreqz
from amp_b_upstream import BInputPathFixedReference, select_level_bank, input_delay_target
from amp_dsp_chain import design_to_sos


PARAMS = json.loads((Path(__file__).resolve().parents[2]/
    'assets/soundamp/remaining-blocks-20260919/audit.json').read_text())['input_path_profiles'][0]


def reference(bank=7, delay=0, family='graphic', gains=None):
    if gains is None: gains=[0]*(4 if family=='tone' else 5)
    return BInputPathFixedReference(PARAMS,eq_family=family,
        shared_gains_tenths_db=gains,bank_index=bank,delay_samples=delay)


class BUpstreamTests(unittest.TestCase):
    def test_selector_threshold_ties_and_fixed_point_truncation(self):
        t=PARAMS['bank_eq']['level_selector']['thresholds']
        for level,index in [(20,0),(12,0),(11,1),(0,4),(-20,8),(-30,10),(-50,13),(-60,14),(-61,15),(-128,15)]:
            self.assertEqual(select_level_bank(level*(1<<24),t),index)
        self.assertEqual(select_level_bank(int(-9.99*(1<<24)),t),5)
        # -9.99 truncates to -9, still above threshold -10, not a floor(-10).
        self.assertEqual(select_level_bank(int(-12.99*(1<<24)),t),6)

    def test_selector_invalid_values_rejected(self):
        t=PARAMS['bank_eq']['level_selector']['thresholds']
        for word in (True,0.0,1<<31,-(1<<31)-1):
            with self.assertRaises(ValueError):select_level_bank(word,t)
        with self.assertRaises(ValueError):select_level_bank(0,t[::-1])

    def test_delay_formula_request_enable_floor_and_cap(self):
        for enabled,ms,want in [(False,200,0),(True,0,0),(True,61,0),
                                (True,62,9),(True,100,1833),(True,65535,9600)]:
            self.assertEqual(input_delay_target(PARAMS,enabled=enabled,milliseconds=ms),want)
        with self.assertRaises(ValueError):input_delay_target(PARAMS,enabled=1,milliseconds=100)

    def test_neutral_bank_seven_both_shared_eq_families_are_unity(self):
        x=np.random.default_rng(81).normal(size=(1024,8))
        for family in ('graphic','tone'):
            np.testing.assert_allclose(reference(family=family).process(x),x,atol=2e-11)

    def test_bank_gain_normalization_matches_direct_sos_cascade(self):
        rng=np.random.default_rng(82);x=rng.normal(size=(2048,8))
        for index in (0,7,10,15):
            r=reference(bank=index)
            expected=x.copy()
            for rec,gain in zip(PARAMS['bank_eq']['templates'],PARAMS['bank_eq']['gains_tenths_db'][index]):
                rec=dict(rec,gain_tenths_db=gain,gain_linear=10**(gain/200))
                expected=sosfilt([design_to_sos(rec)],expected,axis=0)
            np.testing.assert_allclose(r.process(x),expected,atol=3e-10,rtol=1e-10)

    def test_every_channel_stays_independent_with_non_neutral_filters(self):
        for ch in range(8):
            x=np.zeros((128,8));x[0,ch]=1
            r=reference(bank=15,family='tone',gains=[30,-20,10,40])
            y=r.process(x)
            np.testing.assert_array_equal(np.delete(y,ch,axis=1),0)
            self.assertGreater(np.linalg.norm(y[:,ch]),0)

    def test_uniform_delay_and_stream_partition(self):
        x=np.random.default_rng(83).normal(size=(513,8))
        for d in (0,1,17,63,64,65,1000):
            full=reference(delay=d);stream=reference(delay=d)
            y=full.process(x)
            joined=np.concatenate([stream.process(x[:19]),stream.process(x[19:64]),stream.process(x[64:])])
            np.testing.assert_array_equal(y,joined)
            expected=np.zeros_like(x)
            if d<len(x):expected[d:]=x[:len(x)-d]
            np.testing.assert_allclose(y,expected,atol=2e-11)

    def test_invalid_audio_before_history_changes(self):
        r=reference(delay=10)
        for x in (np.zeros((64,6)),np.full((64,8),np.nan),np.ones((64,8),complex)):
            with self.assertRaises(ValueError):r.process(x)
            np.testing.assert_array_equal(r.delay_history,0)
        self.assertEqual(r.process(np.zeros((0,8))).shape,(0,8))

    def test_independent_delay_line_dma_ring_matches_sample_delays(self):
        # Independently follow decoded C4A54 TCB semantics, not a FIFO:
        # shared external ring, channel offsets separated by d, write then
        # read from advanced EIEP-max(d,N); consume scratch on NEXT block.
        n=64; rng=np.random.default_rng(84)
        for d in (0,1,17,63,64,65,127,128,129,1833,9600):
            count=1210 if d==129 else 161 if d==9600 else 40
            audio=rng.normal(size=(n*count,8))
            ring=np.zeros(76864); scratch=np.zeros((8,n));head=0;blocks=[]
            for block in audio.reshape(-1,n,8):
                head=(head+n)%len(ring); pos=head;out=np.empty_like(block)
                for ch in range(8):
                    if d==0:out[:,ch]=block[:,ch]
                    elif d<n:out[:,ch]=np.r_[scratch[ch,-d:],block[:-d,ch]]
                    else:out[:,ch]=scratch[ch]
                    # Hardware TCB writes current internal scratch first.
                    ring[(pos+np.arange(n))%len(ring)]=block[:,ch]
                    read=(pos+n-max(d,n)+np.arange(n))%len(ring)
                    scratch[ch]=ring[read]
                    pos=(pos-d)%len(ring)
                blocks.append(out)
            dma=np.concatenate(blocks)
            expected=np.zeros_like(audio)
            if d<len(audio):expected[d:]=audio[:len(audio)-d]
            np.testing.assert_array_equal(dma,expected)
            np.testing.assert_allclose(reference(delay=d).process(audio),dma,atol=3e-11)

    def test_response_matches_long_impulse_and_delay_phase(self):
        frequencies=np.array([30,55,120,1000,10000])
        model=reference(bank=10,delay=65,family='tone',gains=[30,-20,10,0])
        x=np.zeros((32768,8));x[0,0]=1
        impulse=model.process(x)[:,0]
        h=np.exp(-2j*np.pi*frequencies[:,None]*np.arange(len(x))/48000)@impulse
        np.testing.assert_allclose(h,model.response(frequencies)[:,0],atol=1e-10)
        flat=reference(delay=65).response(frequencies)
        expected=np.exp(-2j*np.pi*frequencies*65/48000)
        np.testing.assert_allclose(flat[:,0],expected,atol=1e-10)

    def test_explicit_scenario_required_and_reported(self):
        with self.assertRaises(TypeError):BInputPathFixedReference(PARAMS)
        self.assertFalse(reference().scenario['live_state_proven'])
        with self.assertRaises(ValueError):reference(bank=16)
        with self.assertRaises(ValueError):reference(delay=-1)
        with self.assertRaises(ValueError):reference(family='tone',gains=[0]*5)

    def test_type26_shelf_both_branches_and_identity(self):
        for gain in (-100,0,90):
            rec=dict(PARAMS['shared_eq']['tone']['templates'][0],gain_tenths_db=gain,
                     gain_linear=10**(gain/200))
            sos=design_to_sos(rec)
            _,h=sosfreqz([sos],worN=[0,np.pi])
            np.testing.assert_allclose(abs(h),[10**(gain/200),1],atol=1e-12)
            K=rec['cot_value'];G=rec['gain_linear']
            # AB850 separately gives raw numerator, denominator and inv_a0.
            b=np.array([K+G,G-K,0]) if gain>=0 else np.array([K+1,1-K,0])
            a=np.array([K+1,1-K,0]) if gain>=0 else np.array([K+1/G,1/G-K,0])
            np.testing.assert_array_equal(sos,np.r_[b/a[0],a/a[0]])

    def test_low_frequency_bank_boost_is_not_the_bass_knob(self):
        for index,wanted in [(7,0),(10,6),(15,20.5)]:
            r=reference(bank=index)
            # Reconstruct effective SOS from the packed reference's coefficients.
            c=r.bank.records[0]
            sos=np.column_stack((np.ones(3),c[:,3],c[:,2],np.ones(3),-c[:,1],-c[:,0]))
            _,h=sosfreqz(sos,worN=[55],fs=48000)
            gain=20*np.log10(abs(h[0]*r.bank.gains[0]))
            # Other peak sections have small tails at55Hz (bank15 adds .005dB).
            self.assertAlmostEqual(gain,wanted,delta=.01)


if __name__=='__main__':unittest.main()
