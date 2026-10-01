#!/usr/bin/env python3
"""CPU upper64 initialization/count/learning-path audit, original TRAIN4 only.

No new efficacy training, DEV, gain selection, C/int8 export or unit contact.
One disposable synthetic optimizer update tests the added features' gradients;
it never changes the initialized candidate or the saved parent checkpoint.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES'] = '-1'
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
from unittest.mock import patch

import numpy as np
import torch
from torch import nn
import unified_spectral_upper_decoder as D
import unified_upmix_stream as S
import unified_upmix_audio as A
import sep_net as SN

PARENT_SHA = 'ff072970aef01749f44afc0c8bc010c542eb7e2384cdf88d8b7473b6a3854da2'
FIXTURE_SHA = '47dc5768a7ff38100a7d7cfbfa209fd5ea95e43101382d6e5cbf7819400e7233'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def nse(value, reference):
    return float((value-reference).abs().double().square().sum()/reference.abs().double().square().sum().clamp_min(1e-12))


def common_exact(parent, candidate):
    other = candidate.state_dict()
    return all(torch.equal(value, other[key]) for key, value in parent.state_dict().items() if not key.startswith('dec.'))


def state_errors(value, reference):
    if isinstance(reference, dict):
        return {key: state_errors(value[key], item) for key, item in reference.items()}
    if isinstance(reference, list):
        return [state_errors(x, y) for x, y in zip(value, reference)]
    return float((value-reference).abs().max())


def largest(value):
    if isinstance(value, dict):
        return max(largest(item) for item in value.values())
    if isinstance(value, list):
        return max(largest(item) for item in value)
    return value


def count_dense(net, x):
    total = [0]
    def observe(module, args, output):
        value = args[0]
        if isinstance(module, nn.Linear):
            total[0] += value.numel()*module.out_features
        elif isinstance(module, nn.Conv1d):
            total[0] += output.numel()*(module.in_channels//module.groups)*module.kernel_size[0]
        elif isinstance(module, nn.GRU):
            batch, frames = value.shape[:2]
            total[0] += batch*frames*3*module.hidden_size*(module.input_size+module.hidden_size)
    handles = [m.register_forward_hook(observe) for m in net.modules() if isinstance(m, (nn.Linear, nn.Conv1d, nn.GRU))]
    try:
        with torch.inference_mode():
            net.forward_stft(x, with_scene=True)
    finally:
        for handle in handles:
            handle.remove()
    return total[0]/(x.shape[0]*x.shape[1])


def main(a):
    if a.out.exists():
        raise FileExistsError('Preserve prior structural evidence')
    torch.set_num_threads(2)
    if sha(a.checkpoint) != PARENT_SHA or sha(a.fixture) != FIXTURE_SHA:
        raise ValueError('Original fitted20k checkpoint/TRAIN4 waveform required')
    checkpoint = torch.load(a.checkpoint, map_location='cpu', weights_only=True)
    if checkpoint['architecture'] != D.V.ARCHITECTURE or checkpoint['mode'] != 'spectral' or checkpoint['step'] != 20000:
        raise ValueError('Expected fixed fitted spectral20k parent')
    parent = D.V.SpectralContextNet(**checkpoint['config'])
    parent.load_state_dict(checkpoint['state'], strict=True)
    parent.eval()
    raw = a.fixture.read_bytes()
    if struct.unpack_from('<8sIII', raw) != (b'UMXPC001', 4, 384000, 2) or len(raw) != 20+4*384000*2*4:
        raise ValueError('Original four TRAIN8s mixture layout differs')
    wave = torch.from_numpy(np.frombuffer(raw, dtype='<f4', offset=20).copy().reshape(4, 384000, 2)).transpose(1, 2)
    rng = torch.random.get_rng_state().clone()
    # All-device manual_seed would change a future caller's CUDA RNG even
    # though fork_rng(devices=[]) restores only CPU. This audit forbids it.
    with patch.object(torch, 'manual_seed', side_effect=RuntimeError('All-device seed prohibited')):
        candidate = D.widen(parent).eval()
    rng_unchanged = torch.equal(rng, torch.random.get_rng_state())
    repeated = D.widen(parent).eval()
    initialization_deterministic = all(torch.equal(value, repeated.state_dict()[key]) for key, value in candidate.state_dict().items())
    del repeated
    a.out.mkdir(parents=True)
    protocol = dict(checkpoint_sha256=PARENT_SHA, fixture_sha256=FIXTURE_SHA,
                    script_sha256=sha(__file__),
                    helpers={Path(m.__file__).name: sha(m.__file__) for m in (D, D.V, D.V.U, S, A, SN)},
                    torch_version=torch.__version__, initialization_seed_scope='CPU default generator only; all-device manual_seed forbidden',
                    config=candidate.config, high_bands=candidate.high_bands,
                    high_bins=sum(candidate.bands[i][1]-candidate.bands[i][0] for i in candidate.high_bands),
                    gates='initial all-source/scene/wave NSE<=1e-10, common state/low decoder/reset exact; chunk<=1e-9/future<=1e-12; observed full/selected dense exact; each added branch receives output then input gradients',
                    scope='actual fitted parent, original TRAIN mixtures; initialize13 upper decoder bands24->64 only; no efficacy training or DEV',
                    limits='old C/int8 port does not accept this architecture; excludes FFT/norm/nonlinear/DSP/scheduling costs; no device/latency/brightness/stability/promotion claim')
    (a.out/'protocol.json').write_text(json.dumps(protocol, indent=2)+'\n')
    with torch.inference_mode():
        spectra = S.analyze(wave)
        baseline, baseline_state = parent.forward_stft(spectra)
        prediction, state = candidate.forward_stft(spectra)
        preservation = {key: nse(value, baseline[key]) for key, value in prediction.items()}
        waveform = nse(S.synthesize(prediction['source_stft'][:, :, 0], 384000),
                       S.synthesize(baseline['source_stft'][:, :, 0], 384000))
        unchanged_state = largest(state_errors(state, baseline_state)) == 0
        del baseline, prediction, state, baseline_state
        selected = D.selected_graph(candidate).eval()
        sample = spectra[:1, :131]
        full, full_state = candidate.forward_stft(sample)
        only, only_state = selected.forward_stft(sample)
        selection = {key: nse(value[:, :, 0], full[key][:, :, 0]) if key in ('source_masks', 'source_stft') else nse(value, full[key])
                     for key, value in only.items()}
        selection_state = largest(state_errors(only_state, full_state))
        selected_aux_zero = bool((only['source_masks'][:, :, 1:] == 0).all())
        del full, only, full_state, only_state
        observed_full = count_dense(candidate, spectra[:1, :3])
        observed_selected = count_dense(selected, spectra[:1, :3])
    low_exact = all(torch.equal(value, candidate.dec[i].state_dict()[key]) for i in range(48) if i not in candidate.high_bands
                    for key, value in parent.dec[i].state_dict().items())
    candidate_before = {key: value.clone() for key, value in candidate.state_dict().items()}
    # Disposable proof: incoming new features are excited, outgoing gradients
    # are nonzero at the preserving start, incoming gradients follow one update.
    probe = copy.deepcopy(candidate)
    for parameter in probe.parameters():
        parameter.requires_grad_(False)
    for parameter in probe.dec.parameters():
        parameter.requires_grad_(True)
    optimizer = torch.optim.SGD(probe.dec.parameters(), lr=1e-3)
    probe.eval()  # cuDNN not used: CPU GRU, no dropout and same inference graph.
    train_input = spectra[:1, :21].clone()  # Ordinary tensor for autograd saves.
    output, _ = probe.forward_stft(train_input, with_scene=False)
    output['source_stft'].abs().square().mean().backward()
    first_grad = {str(i): float(probe.dec[i][2].weight.grad[:, 24:].abs().sum()) for i in probe.high_bands}
    optimizer.step(); optimizer.zero_grad(set_to_none=True)
    output, _ = probe.forward_stft(train_input, with_scene=False)
    output['source_stft'].abs().square().mean().backward()
    second_grad = {str(i): float(probe.dec[i][0].weight.grad[24:].abs().sum()) for i in probe.high_bands}
    with torch.inference_mode():
        full, _ = probe.forward_stft(sample)
        pieces = []; state = None
        for lo, hi in [(0, 1), (1, 7), (7, 128), (128, 129), (129, 131)]:
            part, state = probe.forward_stft(sample[:, lo:hi], state)
            pieces.append(part)
        chunks = {key: nse(torch.cat([part[key] for part in pieces], 1), value) for key, value in full.items()}
        perturbed = sample.clone(); perturbed[:, 65:] *= complex(-3, .7)
        future, _ = probe.forward_stft(perturbed)
        prefixes = {key: nse(future[key][:, :65], value[:, :65]) for key, value in full.items()}
        suffix = nse(future['source_stft'][:, 65:], full['source_stft'][:, 65:])
        reset, _ = probe.forward_stft(sample)
        reset_exact = all(torch.equal(value, reset[key]) for key, value in full.items())
        silence, _ = probe.forward_stft(torch.zeros_like(sample))
        silence_finite = all(bool(torch.isfinite(value).all()) for value in silence.values())
    cost = candidate.cost()
    gates = dict(shared_backbone_scene_exact=common_exact(parent, candidate), low_mid_decoder_exact=low_exact,
                 state_exact=unchanged_state, initial_function_preserved=max(preservation.values()) <= 1e-10,
                 initial_waveform_preserved=waveform <= 1e-10, rng_unchanged=rng_unchanged,
                 initialization_deterministic=initialization_deterministic,
                 all_device_seed_avoided=True,
                 selected_source_scene_parity=max(selection.values()) <= 1e-10,
                 selected_state_exact=selection_state == 0, selected_aux_zero=selected_aux_zero,
                 observed_full_dense_exact=observed_full == sum(cost['all_source_dense_macs_per_frame'].values()),
                 observed_selected_dense_exact=observed_selected == sum(cost['eligible_scene_dense_macs_per_frame'].values()),
                 selected_dense_under150M=cost['eligible_scene_dense_macs_s'] <= 150e6,
                 all_added_output_gradients=all(np.isfinite(value) and value > 0 for value in first_grad.values()),
                 all_added_input_gradients_after_update=all(np.isfinite(value) and value > 0 for value in second_grad.values()),
                 probe_does_not_change_candidate=all(torch.equal(value, candidate.state_dict()[key]) for key, value in candidate_before.items()),
                 active_added_branch_chunk=max(chunks.values()) <= 1e-9,
                 active_added_branch_future=max(prefixes.values()) <= 1e-12,
                 future_suffix_sensitive=suffix > 1e-9, reset_exact=reset_exact, silence_finite=silence_finite,
                 parent_file_unchanged=sha(a.checkpoint) == PARENT_SHA)
    result = dict(complete=True, passed=all(gates.values()), gates=gates, cost=cost,
                  initial_prediction_nse=preservation, waveform_nse=waveform,
                  selected_prediction_nse=selection, selected_state_max_abs=selection_state,
                  added_output_gradient_sums=first_grad, added_input_gradient_sums=second_grad,
                  chunk_nse=chunks, future_prefix_nse=prefixes, future_suffix_nse=suffix,
                  observed_full_dense_per_frame=observed_full, observed_selected_dense_per_frame=observed_selected,
                  efficacy_training_started=False, initialized_candidate_updated=False,
                  disposable_synthetic_probe_optimizer_steps=1, acoustic_or_product_pass=False,
                  protocol_sha256=sha(a.out/'protocol.json'), limits=protocol['limits'])
    (a.out/'complete.json').write_text(json.dumps(result, indent=2, allow_nan=False)+'\n')
    if not result['passed']:
        raise ValueError('Structural upper decoder audit failed; original evidence retained')
    print('UPPER_DECODER_STRUCTURE_PASS', json.dumps(dict(gates=gates,
          selected_dense_macs_s=cost['eligible_scene_dense_macs_s'], efficacy_training_started=False)), flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('checkpoint', 'fixture', 'out'):
        p.add_argument('--'+name, type=Path, required=True)
    main(p.parse_args())
