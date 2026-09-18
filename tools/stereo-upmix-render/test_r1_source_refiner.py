"""CPU structural/gradient qualification; no music-quality or ARM timing claim."""
from dataclasses import FrozenInstanceError, replace
import unittest

import torch

from r1_source_refiner import (ARCHITECTURE, CONDITIONING, CONTRACT,
    RefinerConfig, RefinerState, SourceRefiner)


def frames(config, batch=2, steps=9):
    value = torch.complex(torch.randn(batch, steps, 2, config.bins),
                          torch.randn(batch, steps, 2, config.bins))*.03
    value[..., 0] = value[..., 0].real.clone()
    value[..., -1] = value[..., -1].real.clone()
    return value


def streamed(model, x, e, sizes=(1, 3, 2, 7, 97)):
    parts = []; state = None; start = 0
    for size in sizes:
        end = min(x.shape[1], start+size)
        if end > start:
            out, state = model(x[:, start:end], e[:, start:end], state)
            parts.append(out)
        start = end
    if start != x.shape[1]:
        raise ValueError('Test chunk schedule did not cover all frames')
    return torch.cat(parts, 1), state


class R1SourceRefinerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.threads = torch.get_num_threads()
        torch.set_num_threads(2)

    @classmethod
    def tearDownClass(cls):
        torch.set_num_threads(cls.threads)

    def setUp(self):
        torch.manual_seed(5151)
        self.config = RefinerConfig(fft_size=256)

    def learned(self, config=None):
        model = SourceRefiner(config or self.config)
        with torch.no_grad(): model.residual_head.weight.normal_(0, .17)
        return model

    def test_same_initial_weights_rng_and_capacity_in_both_conditioning_arms(self):
        models = []; states = []
        for mode in CONDITIONING:
            torch.manual_seed(5151)
            models.append(SourceRefiner(replace(self.config, conditioning=mode)))
            states.append(torch.get_rng_state().clone())
        self.assertTrue(torch.equal(states[0], states[1]))
        self.assertEqual(list(models[0].state_dict()), list(models[1].state_dict()))
        for name, value in models[0].state_dict().items():
            self.assertTrue(torch.equal(value, models[1].state_dict()[name]), name)
        for model in models:
            report = model.specification()
            self.assertEqual(report['parameters'], 144)
            self.assertEqual(report['parameters'], sum(p.numel() for p in model.parameters()))
            self.assertEqual(report['dense_macs_per_frame'], 144*self.config.bins)
            self.assertEqual(report['state_tensor_bytes_per_source_batch_item'], 2*8*self.config.bins*4)
            self.assertTrue(report['dense_macs_are_not_arm_instructions_or_cycles'])
            self.assertFalse(report['target_runtime_verified'])
            self.assertFalse(report['native_export_supported'])
            self.assertFalse(report['model_quality_approved'])
            self.assertFalse(report['car_approved'])
        production = SourceRefiner(RefinerConfig(sample_rate=48000)).specification()
        self.assertEqual(production['dense_macs_per_second'], 13_851_000.)
        small = SourceRefiner(replace(self.config, width=4)).specification()
        self.assertEqual(small['parameters'], 72)

    def test_zero_head_exact_numerical_identity_complex_stereo_and_learning_gradient(self):
        x, e = frames(self.config), frames(self.config)
        originals = [v.clone() for v in (x, e)]
        for mode in CONDITIONING:
            model = SourceRefiner(replace(self.config, conditioning=mode))
            out, state = model(x, e)
            self.assertTrue(torch.equal(out, e))
            self.assertTrue(torch.equal(streamed(model, x, e)[0], e))
            target = e + x*.2
            loss = (out-target).abs().square().mean()
            loss.backward()
            self.assertGreater(float(model.residual_head.weight.grad.abs().sum()), 0.)
            for name, value in model.named_parameters():
                self.assertIsNotNone(value.grad, name)
                self.assertTrue(torch.isfinite(value.grad).all(), name)
            # Zero head intentionally blocks backbone gradient on update0;
            # after learning the head, the backbone receives finite gradients.
            optimizer = torch.optim.AdamW(model.parameters(), lr=.01, weight_decay=0.)
            optimizer.step(); optimizer.zero_grad(set_to_none=True)
            next_out, _ = model(x, e)
            self.assertFalse(torch.equal(next_out, e))
            ((next_out-target).abs().square().mean()).backward()
            self.assertGreater(float(model.input_projection.weight.grad.abs().sum()), 0.)
            self.assertTrue(torch.isfinite(state.history).all())
        for before, after in zip(originals, (x, e)):
            self.assertTrue(torch.equal(before, after))

    def test_direct_complex_residual_can_fill_bin_empty_in_both_current_inputs(self):
        model = SourceRefiner(self.config)
        with torch.no_grad():
            for parameter in model.parameters(): parameter.zero_()
            # Explicit representational probe, not claimed trained weights.
            model.input_projection.weight[0, 4, 0] = 1.  # real X left
            model.input_projection.weight[1, 6, 0] = 1.  # imaginary X left
            model.frequency.weight[0, 0, 2] = 1.        # following frequency bin
            model.frequency.weight[1, 0, 2] = 1.
            model.temporal[0, 0] = model.temporal[1, 0] = 1.
            model.residual_head.weight[0, 0, 0] = 1.
            model.residual_head.weight[2, 1, 0] = 1.
        x = torch.zeros(1, 1, 2, self.config.bins, dtype=torch.complex64)
        e = torch.zeros_like(x); x[0, 0, 0, 11] = 1+2j
        out, _ = model(x, e)
        self.assertEqual(x[0, 0, 0, 10], 0.)
        self.assertEqual(e[0, 0, 0, 10], 0.)
        self.assertEqual(out[0, 0, 0, 10], 1+2j)
        self.assertFalse(out[:, :, 1].any())
        self.assertFalse(out[..., 0].imag.any()); self.assertFalse(out[..., -1].imag.any())
        # Same full-bin residual can use the actual second past frame, not
        # just a scalar current-frame gate or an unused state placeholder.
        with torch.no_grad():
            model.temporal.zero_()
            model.temporal[0, 2] = model.temporal[1, 2] = 1.
        sequence = torch.cat((x, x*0, x*0), 1)
        past, _ = model(sequence, torch.zeros_like(sequence))
        self.assertFalse(past[:, :2].any())
        self.assertEqual(past[0, 2, 0, 10], 1+2j)
        pieces, _ = streamed(model, sequence, torch.zeros_like(sequence), (1, 1, 1))
        self.assertTrue(torch.equal(past, pieces))

    def test_estimate_only_has_no_original_side_channel_or_original_gradient(self):
        model = self.learned(replace(self.config, conditioning='estimate_only'))
        x = frames(self.config).requires_grad_(); e = frames(self.config).requires_grad_()
        first, state = model(x, e)
        other, other_state = model(x.detach()*17.+frames(self.config)*6., e)
        self.assertTrue(torch.equal(first, other))
        self.assertTrue(torch.equal(state.history, other_state.history))
        first.abs().square().mean().backward()
        self.assertIsNone(x.grad)
        self.assertIsNotNone(e.grad)
        self.assertTrue(torch.isfinite(e.grad).all())
        conditioned = self.learned(); conditioned.load_state_dict(model.state_dict())
        x2 = x.detach().clone().requires_grad_()
        mixed, _ = conditioned(x2, e.detach())
        self.assertFalse(torch.equal(first, mixed))
        mixed.abs().square().mean().backward()
        self.assertGreater(float(x2.grad.abs().sum()), 0.)

    def test_chunk_bptt_values_history_gradients_and_detach_ownership(self):
        first = self.learned(); second = self.learned(); second.load_state_dict(first.state_dict())
        x, e = frames(self.config, steps=17), frames(self.config, steps=17)
        target = e+x*.13
        full, full_state = first(x, e)
        partial, partial_state = streamed(second, x, e)
        torch.testing.assert_close(full, partial, rtol=1e-6, atol=2e-8)
        torch.testing.assert_close(full_state.history, partial_state.history, rtol=1e-6, atol=2e-8)
        self.assertEqual(full_state.frames, 17); self.assertEqual(partial_state.frames, 17)
        (full-target).abs().square().mean().backward()
        (partial-target).abs().square().mean().backward()
        for (name, a), (_, b) in zip(first.named_parameters(), second.named_parameters()):
            torch.testing.assert_close(a.grad, b.grad, rtol=2e-5, atol=2e-9, msg=name)
        detached = full_state.detach()
        self.assertFalse(detached.history.requires_grad)
        self.assertNotEqual(detached.history.data_ptr(), full_state.history.data_ptr())
        detached.history.zero_()
        self.assertTrue(torch.any(full_state.history))
        original_next, _ = first(x[:, :2], e[:, :2], full_state)
        detached_next, _ = first(x[:, :2], e[:, :2], full_state.detach())
        self.assertTrue(torch.equal(original_next, detached_next))

    def test_future_and_other_batch_do_not_change_past_or_neighboring_batch(self):
        model = self.learned()
        x, e = frames(self.config, steps=17), frames(self.config, steps=17)
        full, _ = model(x, e)
        changed_x, changed_e = x.clone(), e.clone()
        changed_x[:, 7:] *= -9.; changed_e[:, 7:] *= 13.
        changed, _ = model(changed_x, changed_e)
        self.assertTrue(torch.equal(full[:, :7], changed[:, :7]))
        changed_x, changed_e = x.clone(), e.clone()
        changed_x[1] *= -3.; changed_e[1] *= 7.
        separate, _ = model(changed_x, changed_e)
        self.assertTrue(torch.equal(full[0], separate[0]))
        reset, _ = model(x, e, None)
        self.assertTrue(torch.equal(full, reset))

    def test_silence_tiny_values_and_after_music_tail_remain_finite_and_bounded_history(self):
        model = self.learned()
        zero = torch.zeros(2, 7, 2, self.config.bins, dtype=torch.complex64)
        out, state = model(zero, zero)
        self.assertTrue(torch.equal(out, zero)); self.assertFalse(state.history.any())
        loss = out.abs().square().sum(); loss.backward()
        for value in model.parameters():
            self.assertIsNotNone(value.grad)
            self.assertTrue(torch.isfinite(value.grad).all())
        tiny = frames(self.config, steps=7)*1e-25
        tiny_out, tiny_state = model(tiny, tiny)
        self.assertTrue(torch.isfinite(tiny_out).all()); self.assertTrue(torch.isfinite(tiny_state.history).all())
        _, musical = model(frames(self.config), frames(self.config))
        tail, next_state = model(zero, zero, musical)
        self.assertTrue(torch.isfinite(tail).all())
        self.assertFalse(tail[:, self.config.past_frames:].any())
        self.assertFalse(next_state.history.any())
        # E=0 is not hard-coded absence: X plus prior source context can matter.
        nonzero, _ = model(frames(self.config, steps=7), zero)
        self.assertTrue(nonzero.any())

    def test_common_positive_gain_homogeneity_and_exact_endpoint_constraints(self):
        model = self.learned()
        x, e = frames(self.config), frames(self.config)
        out, state = model(x, e)
        for gain in (.25, 2., 1.7):
            scaled, scaled_state = model(x*gain, e*gain)
            torch.testing.assert_close(scaled, out*gain, rtol=2e-6, atol=3e-8)
            torch.testing.assert_close(scaled_state.history, state.history*gain, rtol=2e-6, atol=3e-8)
        self.assertFalse(out[..., 0].imag.any()); self.assertFalse(out[..., -1].imag.any())

    def test_state_validation_reload_invalidation_and_rejected_suffix_immutability(self):
        model = self.learned(); x, e = frames(self.config), frames(self.config)
        _, state = model(x, e)
        previous = state.history.detach().clone()
        stranger = self.learned()
        invalid_states = [RefinerState(state.owner, state.history[:, :, :, :-1], state.frames),
            RefinerState(state.owner, state.history.double(), state.frames),
            RefinerState(state.owner, state.history*float('nan'), state.frames),
            RefinerState(state.owner, state.history, True),
            RefinerState(state.owner, state.history, 0)]
        for invalid in invalid_states:
            with self.assertRaises(ValueError): model(x, e, invalid)
        with self.assertRaises(ValueError): stranger(x, e, state)
        bad = x.clone(); bad[0, -1, 0, 5] = complex(float('nan'), 0.)
        with self.assertRaises(ValueError): model(bad, e, state)
        self.assertTrue(torch.equal(previous, state.history))
        saved = {key: value.clone() for key, value in model.state_dict().items()}
        expected, _ = model(x, e)
        model.load_state_dict(saved)
        with self.assertRaises(ValueError): model(x, e, state)
        self.assertTrue(torch.equal(expected, model(x, e)[0]))
        _, fresh = model(x, e)
        with self.assertRaises(RuntimeError): model.load_state_dict({})
        with self.assertRaises(ValueError): model(x, e, fresh)

    def test_bad_inputs_config_and_old_model_shape_are_rejected(self):
        model = self.learned(); x, e = frames(self.config), frames(self.config)
        bad_endpoint = x.clone(); bad_endpoint[..., 0] += 1j
        for bad in (x[:, :0], x[:, :, :1], x[:, :, :, :-1], x.to(torch.complex128), x.real,
                    bad_endpoint, x*float('inf')):
            with self.assertRaises(ValueError): model(bad, e)
        with self.assertRaises(ValueError): model(x, e[:, :-1])
        with self.assertRaises(ValueError): model(x, torch.cat((e, e, e), 2))
        with self.assertRaises(ValueError): SourceRefiner({'width': 8})
        for changes in (dict(width=3), dict(width=16), dict(width=True), dict(sample_rate=32000),
                        dict(fft_size=255), dict(fft_size=True), dict(conditioning='x_only'), dict(conditioning=True)):
            with self.assertRaises(ValueError): RefinerConfig(**changes)
        with self.assertRaises(FrozenInstanceError): self.config.width = 4
        with self.assertRaises(ValueError): model.double()(x, e)
        self.assertEqual(CONTRACT['architecture'], ARCHITECTURE)
        self.assertFalse(CONTRACT['pretrained_weights_bundled'])


if __name__ == '__main__':
    unittest.main()
