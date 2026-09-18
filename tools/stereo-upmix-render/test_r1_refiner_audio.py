"""R1 stereo Hann/WOLA timeline, gradients and frame-state tests; offline only."""
from dataclasses import replace
import json
import unittest

import torch

from r1_refiner_audio import (AudioAnalysis, CONTRACT, analyze, score_frame_mask,
    synthesize)
from r1_source_refiner import CONDITIONING, RefinerConfig, SourceRefiner


def refined_in_frame_chunks(model, x, e):
    result = []; state = None; start = 0
    for size in (1, 3, 2, 7, 97):
        end = min(x.shape[1], start+size)
        if end > start:
            value, state = model(x[:, start:end], e[:, start:end], state)
            result.append(value)
        start = end
    if start != x.shape[1]: raise ValueError('Incomplete test frame schedule')
    return torch.cat(result, 1), state


class R1RefinerAudioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.threads = torch.get_num_threads()
        torch.set_num_threads(2)

    @classmethod
    def tearDownClass(cls):
        torch.set_num_threads(cls.threads)

    def setUp(self):
        torch.manual_seed(5154)

    def test_identity_every_returned_sample_both_rates_and_boundary_lengths(self):
        for rate in (44100, 48000):
            config = RefinerConfig(sample_rate=rate)
            for length in (1, 255, 256, 257, 1023, 1024, 1111, 4096, 4173):
                with self.subTest(rate=rate, length=length):
                    wave = torch.randn(length, 2)*.2
                    original = wave.clone()
                    analysis = analyze(wave, config)
                    out = synthesize(analysis.frames, length, config)
                    torch.testing.assert_close(out, wave, rtol=2e-6, atol=2e-7)
                    self.assertTrue(torch.equal(wave, original))
                    self.assertEqual(out.shape, wave.shape)
                    self.assertEqual(analysis.waveform_valid_range, (0, length))
                    self.assertEqual(analysis.frames.shape, (1, (length+255)//256+3, 2, 513))
                    self.assertFalse(analysis.frames[..., 0].imag.any())
                    self.assertFalse(analysis.frames[..., -1].imag.any())

    def test_periodic_hann_right_edge_indices_exact_padding_and_mask_boundaries(self):
        config = RefinerConfig(); wave = torch.arange(4357*2, dtype=torch.float32).reshape(4357, 2)*1e-5
        analysis = analyze(wave, config)
        self.assertEqual(analysis.left_padding, 768)
        self.assertEqual(analysis.right_padding, 1019)
        self.assertEqual(analysis.frame_starts[:5].tolist(), [-768, -512, -256, 0, 256])
        self.assertEqual(analysis.frame_ends[:5].tolist(), [256, 512, 768, 1024, 1280])
        window = torch.hann_window(1024, periodic=True)
        explicit_first = torch.cat((torch.zeros(768, 2), wave[:256]), 0).T*window
        torch.testing.assert_close(analysis.frames[0, 0], torch.fft.rfft(explicit_first), rtol=0, atol=0)
        score = score_frame_mask(analysis, 1024, 4096)
        expected = (analysis.frame_starts >= 1024) & (analysis.frame_ends <= 4096)
        self.assertTrue(torch.equal(score, expected))
        self.assertEqual(int(score.sum()), 9)
        self.assertFalse(analysis.full_support_mask[:3].any())
        self.assertFalse(analysis.full_support_mask[-3:].any())
        for start, end in zip(analysis.frame_starts[score], analysis.frame_ends[score]):
            self.assertGreaterEqual(int(start), 1024); self.assertLessEqual(int(end), 4096)
        # Returned coordinate tensors do not mutate the stored score geometry.
        analysis.frame_starts.zero_()
        self.assertTrue(torch.equal(score_frame_mask(analysis, 1024, 4096), expected))
        metadata = analysis.timeline()
        json.dumps(metadata, allow_nan=False)
        self.assertEqual(metadata['nominal_wola_alignment_delay_samples'], 768)
        self.assertEqual(metadata['returned_sample_start'], 0)
        self.assertEqual(metadata['returned_sample_end'], len(wave))
        self.assertFalse(metadata['pcm_streaming_implemented'])

    def test_impulses_preserve_original_core_indices_and_stereo_channels(self):
        length = 4099
        for index in (0, 255, 256, 767, 768, 1023, 2048, length-1):
            wave = torch.zeros(length, 2); wave[index, 0] = .7; wave[index, 1] = -.3
            analysis = analyze(wave)
            out = synthesize(analysis.frames, length)
            self.assertEqual(int(out[:, 0].abs().argmax()), index)
            self.assertEqual(int(out[:, 1].abs().argmax()), index)
            torch.testing.assert_close(out, wave, rtol=1e-6, atol=1e-7)

    def test_gradient_through_analysis_synthesis_matches_identity_on_all_real_samples(self):
        for length in (1, 257, 1024, 2089):
            wave = (torch.randn(length, 2)*.03).requires_grad_()
            probe = torch.randn_like(wave)
            analysis = analyze(wave)
            reconstructed = synthesize(analysis.frames, length)
            (reconstructed*probe).sum().backward()
            self.assertTrue(torch.isfinite(wave.grad).all())
            torch.testing.assert_close(wave.grad, probe, rtol=3e-6, atol=8e-7)

    def test_zero_residual_model_reconstructs_estimate_without_using_six_channel_head(self):
        x = torch.randn(3179, 2)*.1; estimate = torch.randn_like(x)*.03
        for rate in (44100, 48000):
            for mode in CONDITIONING:
                config = RefinerConfig(sample_rate=rate, conditioning=mode)
                ax, ae = analyze(x, config), analyze(estimate, config)
                model = SourceRefiner(config)
                refined, state = model(ax.frames, ae.frames)
                self.assertTrue(torch.equal(refined, ae.frames))
                output = synthesize(refined, len(x), config)
                torch.testing.assert_close(output, estimate, rtol=2e-6, atol=1e-7)
                self.assertEqual(output.shape[-1], 2)
                self.assertEqual(state.frames, ax.frames.shape[1])

    def test_nonzero_model_frame_chunks_and_waveform_gradients_match_whole(self):
        config = RefinerConfig(); first = SourceRefiner(config); second = SourceRefiner(config)
        with torch.no_grad(): first.residual_head.weight.normal_(0, .1)
        second.load_state_dict(first.state_dict())
        x = torch.randn(3333, 2)*.1; e = torch.randn_like(x)*.03; target = e+x*.03
        ax, ae = analyze(x, config), analyze(e, config)
        full_frames, full_state = first(ax.frames, ae.frames)
        split_frames, split_state = refined_in_frame_chunks(second, ax.frames, ae.frames)
        full = synthesize(full_frames, len(x), config)
        split = synthesize(split_frames, len(x), config)
        torch.testing.assert_close(full, split, rtol=1e-6, atol=2e-8)
        torch.testing.assert_close(full_state.history, split_state.history, rtol=1e-6, atol=2e-8)
        (full-target).square().mean().backward()
        (split-target).square().mean().backward()
        for (name, a), (_, b) in zip(first.named_parameters(), second.named_parameters()):
            self.assertTrue(torch.isfinite(a.grad).all())
            torch.testing.assert_close(a.grad, b.grad, rtol=3e-5, atol=2e-9, msg=name)

    def test_no_future_in_analysis_frames_and_nominal_delayed_model_dependence(self):
        config = RefinerConfig(); model = SourceRefiner(config)
        with torch.no_grad(): model.residual_head.weight.normal_(0, .13)
        x = torch.randn(6144, 2)*.1; e = torch.randn_like(x)*.03
        ax, ae = analyze(x, config), analyze(e, config)
        cut = 3072
        changed_x, changed_e = x.clone(), e.clone()
        changed_x[cut:] *= -5.; changed_e[cut:] *= 3.
        bx, be = analyze(changed_x, config), analyze(changed_e, config)
        complete_past = ax.frame_ends <= cut
        self.assertTrue(torch.equal(ax.frames[:, complete_past], bx.frames[:, complete_past]))
        self.assertTrue(torch.equal(ae.frames[:, complete_past], be.frames[:, complete_past]))
        frames_a, _ = model(ax.frames, ae.frames); frames_b, _ = model(bx.frames, be.frames)
        self.assertTrue(torch.equal(frames_a[:, complete_past], frames_b[:, complete_past]))
        audio_a = synthesize(frames_a, len(x), config); audio_b = synthesize(frames_b, len(x), config)
        # Aligned offline outputs can use the nominal WOLA lookahead/latency
        # interval. Do not call them zero-latency streaming PCM.
        unaffected = cut-(config.fft_size-config.hop)
        self.assertTrue(torch.equal(audio_a[:unaffected], audio_b[:unaffected]))

    def test_short_and_quiet_intervals_have_explicit_empty_score_domain_not_fake_pass(self):
        quiet = torch.zeros(333, 2, requires_grad=True)
        analysis = analyze(quiet)
        self.assertFalse(analysis.full_support_mask.any())
        self.assertEqual(analysis.timeline()['complete_original_support_frames'], 0)
        output = synthesize(analysis.frames, len(quiet))
        self.assertFalse(output.any())
        output.square().sum().backward()
        self.assertTrue(torch.isfinite(quiet.grad).all())
        longer = analyze(torch.zeros(4096, 2))
        self.assertFalse(score_frame_mask(longer, 1024, 1536).any())
        train = score_frame_mask(longer, 0, 2048)
        assessment = score_frame_mask(longer, 2048, 4096)
        self.assertFalse((train & assessment).any())
        self.assertTrue(train.any()); self.assertTrue(assessment.any())
        self.assertFalse(CONTRACT['pcm_streaming_implemented'])
        self.assertFalse(CONTRACT['car_approved'])

    def test_strict_inputs_shapes_padding_endpoints_and_intervals(self):
        wave = torch.randn(2049, 2)*.03; analysis = analyze(wave)
        bad = wave.clone(); bad[-1, 0] = float('nan')
        for value in (wave[:0], wave[:, :1], wave.double(), wave.to(torch.int32), bad):
            with self.assertRaises(ValueError): analyze(value)
        with self.assertRaises(ValueError): analyze(wave, RefinerConfig(fft_size=512))
        with self.assertRaises(ValueError): analyze(wave, {})
        for length in (0, -1, True, 2049.):
            with self.assertRaises(ValueError): synthesize(analysis.frames, length)
        bad_endpoint = analysis.frames.clone(); bad_endpoint[..., 0] += 1j
        bad_finite = analysis.frames.clone(); bad_finite[0, -1, 0, 5] = complex(float('inf'), 0.)
        for value in (analysis.frames[:, :-1], analysis.frames[:, :, :1], analysis.frames.real,
                      analysis.frames.to(torch.complex128), bad_endpoint, bad_finite):
            with self.assertRaises(ValueError): synthesize(value, len(wave))
        for start, end in ((-1, 2049), (0, 2050), (4, 4), (5, 4), (True, 20), (0., 2049)):
            with self.assertRaises(ValueError): score_frame_mask(analysis, start, end)
        for invalid in (replace(analysis, left_padding=0), replace(analysis, right_padding=True),
                        AudioAnalysis(analysis.frames, len(wave), RefinerConfig(fft_size=512), 768, 1023)):
            with self.assertRaises(ValueError): score_frame_mask(invalid, 0, len(wave))


if __name__ == '__main__':
    unittest.main()
