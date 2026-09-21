import unittest
import numpy as np
from numpy.testing import assert_allclose
from amp_b_adaptive import BandLevelMaskReference, FrameVectorSmoother
from amp_b_spatial import SpatialFrameReference
from amp_b_pipeline import StereoTransientReference, BSpectralFrameReference, BMode2FixedReference, BMode5FixedReference, prepare_mode5, crossmix, remodulate
from amp_b_output import BSixChannelTailReference, BFourChannelTailReference, BOutputFinishReference
from amp_b_decorrelator import BTwoChannelTailReference


def transient(n=513, gain=1):
    p = np.zeros(88); p[76:78] = gain
    p[[78, 79, 82, 84, 86]] = 1
    return StereoTransientReference(bins=n, edges=np.linspace(0, n, 75, dtype=int),
                                    params=p, history=np.zeros(296))


def pipeline(**overrides):
    n = 513; edges = np.linspace(0, n, 75, dtype=int)
    lp = np.zeros(16); lp[14:16] = 1; lp[[2, 4, 6, 8, 10]] = 1
    def level():
        return BandLevelMaskReference(bins=n, edges=edges, params=lp,
            power_history=np.zeros(74), db_history=np.zeros(74), gain_history=np.zeros(74))
    sp = np.zeros(28); sp[[4, 5, 11, 13, 15, 17, 19, 22, 23, 25]] = 1
    sp[9:11] = [.3, 2e-6]
    spatial = SpatialFrameReference(bins=n, edges=edges, params=sp,
        eigen_selector=0, balance_enabled=False, gate_selector=0, low_band=0, high_band=73,
        balance_slope=1, balance_offset=0, left_history=np.zeros(74), right_history=np.zeros(74),
        cross_history=np.zeros(74, complex), diffuse_history=np.zeros(74),
        minimum_history=0, mean_history=0, gate_history=0)
    args = dict(main_transient=transient(), post_transient=transient(), level_sum=level(),
        level_difference=level(), spatial=spatial,
        smooth_left=FrameVectorSmoother(size=256, history_gain=.75, input_gain=.25),
        smooth_right=FrameVectorSmoother(size=256, history_gain=.75, input_gain=.25),
        pre_subtraction=dict(enabled=False, start=.2, end=.2, low_bin=0, high_bin=512, negative_gain=0),
        crossfeed=0, main_selector=0, post_selector=0, main_enabled=True,
        post_enabled=True, level_enabled=True, spatial_enabled=False, smoothing_enabled=False)
    args.update(overrides)
    return BSpectralFrameReference(**args)


class StereoTransientTests(unittest.TestCase):
    def model(self):
        model = transient(74)
        p = np.zeros(88); p[77] = 1; p[[80, 81, 82, 84, 86]] = 1
        for side, previous, gain in ((model.left, 1, .7), (model.right, 2, .8)):
            side.params[:] = p; side.level[:] = previous; side.gain[:] = gain
        return model

    def test_shared_average_and_left_history_only(self):
        model = self.model()
        right_before = (model.right.level.copy(), model.right.gain.copy())
        a, b, x, y = model.process(np.full(74, 3j), np.full(74, 5+2j),
                                  np.full(74, 2), np.full(74, 8), enabled=True, shared_selector=1)
        # Average5, old level1 -> (1/5)^2=.04, applied once to scalar too.
        assert_allclose(a, 3j*.04); assert_allclose(b, (5+2j)*.04)
        assert_allclose(x, 2*.04); assert_allclose(y, 8*.04)
        assert_allclose(model.left.gain, .04)
        assert_allclose((model.right.level, model.right.gain), right_before)

    def test_independent_and_nonboolean_selector(self):
        for selector in (0, 2, 3):
            model = self.model()
            a, b, x, y = model.process(np.ones(74), np.ones(74), np.full(74, 2), np.full(74, 8),
                                      enabled=True, shared_selector=selector)
            assert_allclose(a, .25); assert_allclose(b, .0625)
            assert_allclose(x, .5); assert_allclose(y, .5)

    def test_disabled_preserves_all_history_and_inputs(self):
        model = self.model(); before = model.history()
        values = [np.full(74, 2j), np.full(74, 3j), np.full(74, 2), np.full(74, 8)]
        out = model.process(*values, enabled=False, shared_selector=1)
        for a, b in zip(out, values):
            assert_allclose(a, b); self.assertFalse(np.shares_memory(a, b))
        assert_allclose(model.history(), before)

    def test_switch_from_shared_reuses_left_not_new_third_history(self):
        model = self.model()
        model.process(np.ones(74), np.ones(74), np.full(74, 2), np.full(74, 8),
                      enabled=True, shared_selector=1)
        out = model.process(np.ones(74), np.ones(74), np.full(74, 2), np.full(74, 8),
                            enabled=True, shared_selector=0)
        assert_allclose(out[0], .25); assert_allclose(out[1], .0625)

    def test_history_packing_and_invalid_shapes(self):
        model = self.model(); h = model.history()
        for sl, value in ((slice(0, 74), 1), (slice(74, 148), 2),
                          (slice(148, 222), .7), (slice(222, 296), .8)):
            assert_allclose(h[sl], value)
        with self.assertRaises(ValueError): model.process([1], [1], [1], [1], enabled=True, shared_selector=0)
        with self.assertRaises(ValueError): model.process(np.ones(74), np.ones(74), -np.ones(74), np.ones(74),
                                                          enabled=True, shared_selector=0)


class SpectralWrapperTests(unittest.TestCase):
    def spectra(self):
        rng = np.random.default_rng(1280)
        return rng.normal(size=(2, 513))+1j*rng.normal(size=(2, 513))

    def test_crossmix_preserves_sum_scales_difference_and_not_clamped(self):
        a, b = self.spectra()
        for amount in (0, .2, .5, 1, 2):
            l, r = crossmix(a, b, amount=amount)
            assert_allclose(l+r, a+b, atol=2e-15)
            assert_allclose(l-r, (1-2*amount)*(a-b), atol=3e-15)

    def test_remodulation_preserves_complex_sign_and_negative_scalar(self):
        assert_allclose(remodulate([2, -3], [1j, 2-4j]), [2j, -6+12j])

    def test_neutral_spectral_branches_all_unity(self):
        x = self.spectra()
        for mode in (2, 5):
            for out in pipeline().spectral_process(*x, mode=mode).values():
                assert_allclose(out, x, atol=2e-15)

    def test_main_transient_cannot_leak_into_pre_or_post(self):
        x = self.spectra(); model = pipeline(main_transient=transient(gain=.125))
        out = model.spectral_process(*x, mode=2)
        assert_allclose(out['main'], .125*x, atol=2e-15)
        assert_allclose(out['pre'], x, atol=2e-15)
        assert_allclose(out['post'], x, atol=2e-15)

    def test_pre_reference_is_coherent_sum_not_magnitude_average(self):
        x = np.ones(513, complex)*(3+4j)
        config = dict(enabled=True, start=.2, end=.2, low_bin=0, high_bin=512, negative_gain=0)
        coherent = pipeline(pre_subtraction=config).spectral_process(x, x, mode=2)['pre']
        assert_allclose(coherent, [.8*x, .8*x], atol=1e-12)
        opposite = pipeline(pre_subtraction=config).spectral_process(x, -x, mode=2)['pre']
        # Sum zero still has magnitude helper floor, NOT magnitude average5.
        self.assertGreater(abs(opposite[0][0]), 4.99999)

    def test_two_transient_stages_are_independent_not_serial_main_to_post(self):
        x = self.spectra()
        out = pipeline(main_transient=transient(gain=.25),
                       post_transient=transient(gain=.5)).spectral_process(*x, mode=5)
        assert_allclose(out['main'], .25*x, atol=2e-15)
        assert_allclose(out['post'], .5*x, atol=2e-15)

    def test_level_mask_is_before_crossmix_and_spatial(self):
        x = self.spectra(); model = pipeline(crossfeed=.2)
        # Shared parameter bank, separate histories: force equal gain .5.
        model.level_sum.params[14:16] = .5; model.level_difference.params[14:16] = .5
        out = model.spectral_process(*x, mode=2)
        expected = [.5*(.8*x[0]+.2*x[1]), .5*(.8*x[1]+.2*x[0])]
        assert_allclose(out['post'], expected, atol=2e-15)
        assert_allclose(out['main'], x, atol=2e-15)

    def test_magnitude_history_only_first256_bins(self):
        model = pipeline(smoothing_enabled=True)
        x = np.ones(513, complex)*(2+3j)
        first = model.spectral_process(x, x, mode=2)['post']
        assert_allclose(first, [x, x], atol=2e-15)
        second = model.spectral_process(2*x, 2*x, mode=2)['post']
        assert_allclose(second[0][:256], 1.25*x[:256], atol=2e-15)
        assert_allclose(second[0][256:], 2*x[256:], atol=2e-15)

    def test_inactive_branches_are_unwritten_and_histories_freeze(self):
        model = pipeline(smoothing_enabled=True)
        out = model.spectral_process(*self.spectra(), mode=3)
        self.assertIsNone(out['pre']); self.assertIsNone(out['post'])
        self.assertTrue(model.smooth_left.first)
        assert_allclose(model.level_sum.power, 0)
        assert_allclose(model.post_transient.history(), 0)
        self.assertGreater(np.max(model.main_transient.history()), 0)

    def test_zero_input_never_injects_control_floors_as_audio(self):
        model = pipeline(smoothing_enabled=True, spatial_enabled=True)
        for _ in range(3):
            out = model.process(np.zeros(512), np.zeros(512), mode=2)
            for branch in out.values(): assert_allclose(branch, 0, atol=0)

    def test_neutral_stream_against_input_delay_and_independent_stft(self):
        rng = np.random.default_rng(1281); x = rng.normal(size=(2, 512*5))
        model = pipeline(); output = {k:[] for k in ('pre', 'main', 'post')}
        previous = np.zeros((2, 512)); tail = np.zeros((2, 512)); reference = []
        for hop in range(6):
            block = x[:, 512*hop:512*(hop+1)] if hop < 5 else np.zeros((2, 512))
            out = model.process(*block, mode=2)
            for key in output: output[key].append(out[key])
            frames = np.concatenate((previous, block), axis=1)*model.window
            rendered = np.fft.irfft(np.fft.rfft(frames, axis=1), axis=1)*model.window
            reference.append(tail+rendered[:, :512]); tail = rendered[:, 512:]; previous = block
        expected = np.concatenate((np.zeros((2, 512)), x), axis=1)
        assert_allclose(np.concatenate(reference, axis=1), expected, atol=3e-15)
        for value in output.values(): assert_allclose(np.concatenate(value, axis=1), expected, atol=4e-15)

    def test_separate_ola_states_and_skipped_state_not_erased(self):
        hist = {'pre':np.ones((2, 512)), 'main':2*np.ones((2, 512)), 'post':3*np.ones((2, 512))}
        model = pipeline(synthesis_history=hist)
        out = model.process(np.zeros(512), np.zeros(512), mode=3)
        self.assertIsNone(out['pre']); assert_allclose(out['main'], 2)
        assert_allclose(model.synthesis_history['post'], 3)
        out = model.process(np.zeros(512), np.zeros(512), mode=2)
        assert_allclose(out['pre'], 1); assert_allclose(out['main'], 0); assert_allclose(out['post'], 3)

    def test_nonneutral_stream_gains_dont_accumulate_between_branches(self):
        model = pipeline(main_transient=transient(gain=.5), post_transient=transient(gain=.25))
        x = np.sin(2*np.pi*60*np.arange(512*3)/48000)
        output = {k:[] for k in ('pre', 'main', 'post')}
        for block in np.concatenate((x, np.zeros(512))).reshape(-1, 512):
            out = model.process(block, block, mode=2)
            for k in output: output[k].append(out[k][0])
        expected = np.concatenate((np.zeros(512), x))
        for k, gain in (('pre', 1), ('main', .5), ('post', .25)):
            assert_allclose(np.concatenate(output[k]), gain*expected, atol=2e-15)

    def test_reject_wrong_frame_and_smoother_size(self):
        model = pipeline()
        with self.assertRaises(ValueError): model.process(np.zeros(1024), np.zeros(1024), mode=2)
        with self.assertRaises(ValueError): model.process(np.zeros(512), np.zeros(512), mode=True)
        with self.assertRaises(ValueError): pipeline(smooth_left=FrameVectorSmoother(size=513, history_gain=.5, input_gain=.5))


class Mode2CompositionTests(unittest.TestCase):
    def model(self, common_delay=0):
        def config():
            return dict(channel_delays=[0, 0], pre_eq_gain=1, eq_records=[],
                        single=[None, None], final_gain=1)
        return BMode2FixedReference(
            spectral=pipeline(main_transient=transient(gain=.25), post_transient=transient(gain=.5)),
            six_tail=BSixChannelTailReference(pre_gain=1, common_delay=common_delay,
                nested=[[None, None], [None, None]], pair_configs=[config() for _ in range(3)], final_gain=1),
            four_tail=BFourChannelTailReference(common_delay=common_delay,
                pair_configs=[config(), config()], final_gain=1),
            two_tail=BTwoChannelTailReference(common_delay=common_delay, nested=[None, None], **config()),
            finish=BOutputFinishReference(six_gain=1, four_gain=1, two_gain=1,
                six_eq=[], two_eq=[], final_eq=[], final_gain=1))

    def test_branch_binding_pre_four_post_six_main_two(self):
        model = self.model(); x = np.zeros((512, 2)); x[0] = [1, -2]
        first = model.process(*x.T)
        second = model.process(np.zeros(512), np.zeros(512))
        assert_allclose(first, 0, atol=1e-15)
        expected = np.tile(x, (1, 4))*[1.5, 1.5, 1.5, 1.5, .5, .5, .25, .25]
        assert_allclose(second, expected, atol=2e-15)

    def test_full_stream_delay_and_no_silent_clipping(self):
        model = self.model(common_delay=17)
        rng = np.random.default_rng(131208); x = rng.normal(size=(1024, 2))
        padded = np.concatenate((x, np.zeros((1024, 2))))
        y = np.concatenate([model.process(*hop.T) for hop in padded.reshape(-1, 512, 2)])
        expected = np.zeros_like(y)
        expected[529:1553] = np.tile(x, (1, 4))*[1.5, 1.5, 1.5, 1.5, .5, .5, .25, .25]
        assert_allclose(y, expected, atol=4e-15)
        self.assertGreater(np.max(abs(y)), 1)

    def test_invalid_hop_does_not_advance_spectral_history(self):
        model = self.model()
        with self.assertRaises(ValueError): model.process(np.ones(513), np.ones(513))
        assert_allclose(model.spectral.analysis_history, 0)


class Mode5CompositionTests(unittest.TestCase):
    def model(self, gain):
        base = Mode2CompositionTests().model()
        return BMode5FixedReference(input_mix_gain=gain, spectral=base.spectral,
            six_tail=base.six_tail, four_tail=base.four_tail,
            two_tail=base.two_tail, finish=base.finish)

    def test_shared_addition_preserves_difference_without_half_scale(self):
        rng = np.random.default_rng(1315)
        x = rng.normal(size=(3,512)); before = x.copy()
        for gain in (0, .25, 1, -2):
            y = prepare_mode5(*x, input_mix_gain=gain)
            assert_allclose(y[0]-y[1], x[0]-x[1], atol=2e-15)
            assert_allclose(y.sum(axis=0), x[0]+x[1]+2*gain*x[2], atol=2e-15)
            self.assertFalse(np.shares_memory(y,x))
        assert_allclose(x,before,atol=0)

    def test_extra_only_reaches_all_output_branches(self):
        model = self.model(.5); z=np.zeros(512); c=z.copy(); c[0]=2
        first=model.process(z,z,c); second=model.process(z,z,z)
        assert_allclose(first,0,atol=1e-15)
        expected=np.zeros((512,8)); expected[0]=[1.5]*4+[.5]*2+[.25]*2
        assert_allclose(second,expected,atol=2e-15)

    def test_zero_extra_matches_mode2_over_multiple_frames(self):
        model=self.model(4); reference=Mode2CompositionTests().model()
        rng=np.random.default_rng(1316)
        for _ in range(4):
            a,b=rng.normal(size=(2,512))
            assert_allclose(model.process(a,b,np.zeros(512)),reference.process(a,b),atol=0)

    def test_stream_against_independent_shared_sum_and_known_branch_gains(self):
        model=self.model(.7); rng=np.random.default_rng(1317)
        x=rng.normal(size=(3,1536)); padded=np.pad(x,((0,0),(0,512)))
        y=np.concatenate([model.process(*padded[:,i:i+512]) for i in range(0,2048,512)])
        prepared=np.column_stack((x[0]+.7*x[2],x[1]+.7*x[2]))
        expected=np.zeros((2048,8))
        expected[512:]=np.tile(prepared,(1,4))*[1.5,1.5,1.5,1.5,.5,.5,.25,.25]
        assert_allclose(y,expected,atol=5e-15)
        self.assertGreater(np.max(abs(y)),1)  # no hidden limiter

    def test_invalid_extra_or_gain_rejected_before_history_advances(self):
        model=self.model(1); z=np.zeros(512)
        for bad in (np.zeros(513), np.full(512,np.nan)):
            with self.assertRaises(ValueError): model.process(z,z,bad)
        assert_allclose(model.spectral.analysis_history,0)
        for bad in (np.nan,np.inf,1j,[1]):
            with self.assertRaises(ValueError): self.model(bad)


if __name__ == '__main__': unittest.main()
