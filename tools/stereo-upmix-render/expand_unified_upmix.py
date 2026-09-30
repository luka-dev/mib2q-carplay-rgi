#!/usr/bin/env python3
"""Add shared capacity while preserving a fixed trained graph's initial outputs.

Same 2048/512 frontend, one trunk/state owner. New residual block starts as
identity; local/global/decoder features expand16->24 with zero outgoing links.
This is structural preparation, not training, acoustic or device acceptance.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import torch
from unified_upmix_net import UnifiedUpmixNet, ARCHITECTURE
import unified_upmix_net as U

PARENT_SHA = 'ea4133f8983898d0b728b7aa969875b0574092cdcbe0bce1fbf7df33f3092a48'
TARGET = dict(width=24, depth=3, local=24, global_width=24, decoder=24)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def expand(parent):
    expected = dict(width=24, depth=2, local=16, global_width=16, decoder=16)
    if {k:parent.config[k] for k in expected} != expected:
        raise ValueError('Exact audited24/2/16/16/16 graph required')
    result = UnifiedUpmixNet(**(parent.config | TARGET))
    result.enc.load_state_dict(parent.enc.state_dict())
    for before, after in zip(parent.blocks, result.blocks):
        after.load_state_dict(before.state_dict())
    with torch.no_grad():
        for projection in (result.blocks[-1].time_proj, result.blocks[-1].band_proj):
            projection.weight.zero_(); projection.bias.zero_()
        result.interface.weight[:16].copy_(parent.interface.weight)
        result.interface.bias[:16].copy_(parent.interface.bias)
        # Preserve all old reset/update/new gate rows. New hidden/input columns
        # cannot affect those rows until learning creates outgoing links.
        for name, before in parent.global_gru.named_parameters():
            after = dict(result.global_gru.named_parameters())[name]
            for gate in range(3):
                rows = slice(gate*24, gate*24+16)
                old_rows = slice(gate*16, (gate+1)*16)
                if after.ndim == 2:
                    after[rows].zero_(); after[rows, :16].copy_(before[old_rows])
                else:
                    after[rows].copy_(before[old_rows])
        inputs = torch.tensor(list(range(16))+list(range(24,40)))
        result.scene.weight.zero_()
        result.scene.weight[:, inputs] = parent.scene.weight
        result.scene.bias.copy_(parent.scene.bias)
        for before, after in zip(parent.dec, result.dec):
            after[0].weight[:16].zero_()
            after[0].weight[:16, inputs] = before[0].weight
            after[0].bias[:16].copy_(before[0].bias)
            after[2].weight.zero_()
            after[2].weight[:, :16].copy_(before[2].weight)
            after[2].bias.copy_(before[2].bias)
        result.log_mean.copy_(parent.log_mean); result.log_std.copy_(parent.log_std)
    for p in result.scene.parameters():
        p.requires_grad_(False)
    return result


def nse(x, ref):
    return float((x-ref).abs().double().square().sum()/ref.abs().double().square().sum().clamp_min(1e-12))


def selected_cost(net):
    cost = net.cost(); pieces = cost['dense_macs_per_frame'].copy()
    pieces['four_source_projection'] //= 4
    return dict(full_output=cost, eligible_plus_all_scene_dense_macs_frame=sum(pieces.values()),
                eligible_plus_all_scene_dense_macs_s=sum(pieces.values())*cost['fps'],
                limits='Counted dense graph only; current default C blob/runtime does not accept this configuration. No measured device or full-chain fit.')


def smoke(parent, result):
    torch.set_num_threads(1)
    parent.eval(); result.eval(); torch.manual_seed(7)
    x = torch.complex(torch.randn(2,21,1025,2), torch.randn(2,21,1025,2))
    x.imag[:,:,0] = 0; x.imag[:,:,-1] = 0
    with torch.inference_mode():
        reference, _ = parent.forward_stft(x)
        prediction, _ = result.forward_stft(x)
        initial = {k:nse(prediction[k], reference[k]) for k in prediction}
        parts = []; state = None
        for lo,hi in ((0,1),(1,8),(8,9),(9,21)):
            p,state = result.forward_stft(x[:,lo:hi], state); parts.append(p)
        chunk = {k:nse(torch.cat([p[k] for p in parts],1),prediction[k]) for k in prediction}
        changed = x.clone(); changed[:,10:] *= -7
        future,_ = result.forward_stft(changed)
        prefix = {k:nse(future[k][:,:10],prediction[k][:,:10]) for k in prediction}
        repeated,_ = result.forward_stft(x)
        reset = max(float((repeated[k]-prediction[k]).abs().max()) for k in prediction)
        silence,_ = result.forward_stft(torch.zeros_like(x))
        finite = all(bool(torch.isfinite(v).all()) for v in silence.values())
    if max(initial.values()) > 1e-9 or max(chunk.values()) > 1e-9 or max(prefix.values()) != 0 or reset != 0 or not finite:
        raise ValueError('Predeclared functional/carry/causality gate failed')
    # Check that function-preserving zero links do not leave new capacity dead.
    # Separate copy; synthetic objective and updates never change saved init.
    probe = copy.deepcopy(result).train()
    opt = torch.optim.SGD([p for p in probe.parameters() if p.requires_grad], lr=1e-4)
    for step in range(2):
        opt.zero_grad(set_to_none=True)
        output,_ = probe.forward_stft(x[:,:5], with_scene=False)
        output['source_stft'].abs().square().mean().backward()
        if step == 0:
            opt.step()
    norm = lambda ps:sum(float(p.grad.detach().abs().sum()) for p in ps if p.grad is not None)
    gradients = dict(added_time_gru=norm(probe.blocks[-1].time_gru.parameters()),
                     added_frequency_gru=norm(probe.blocks[-1].band_gru.parameters()),
                     added_local_rows=float(probe.interface.weight.grad[16:].abs().sum()),
                     added_global_rows=sum(float(p.grad.reshape(3,24,-1)[:,16:].abs().sum()) for p in probe.global_gru.parameters()),
                     added_decoder_features=sum(float(d[0].weight.grad[16:].abs().sum()) for d in probe.dec))
    if not all(v > 0 for v in gradients.values()):
        raise ValueError('Added capacity has no gradient after outgoing links update')
    return dict(initial_all_fields_nse=initial, carried_chunk_nse=chunk, future_prefix_nse=prefix,
                reset_max_abs=reset, silence_finite=finite, synthetic_second_step_gradient_abs_sums=gradients,
                limits='CPU graph/initialization proof only on synthetic spectra. No actual TRAIN/DEV quality, CUDA/PCM, int8/C/device/car result.')


def main(a):
    if a.out.exists():
        raise FileExistsError('Preserve previous preparation')
    if sha(a.parent) != PARENT_SHA:
        raise ValueError('Fixed joint40k parent differs')
    torch.set_num_threads(1); torch.manual_seed(1)
    checkpoint = torch.load(a.parent, map_location='cpu', weights_only=True)
    if checkpoint['architecture'] != ARCHITECTURE or checkpoint['mode'] != 'joint_audio' or checkpoint['step'] != 40000 or checkpoint['smoke']:
        raise ValueError('Fixed audited joint40k parent only')
    parent = UnifiedUpmixNet(**checkpoint['config']); parent.load_state_dict(checkpoint['state'], strict=True)
    torch.manual_seed(1); result = expand(parent)
    a.out.mkdir(parents=True)
    protocol = dict(parent_sha256=PARENT_SHA, parent_protocol_sha256=checkpoint['protocol_sha256'],
                    script_sha256=sha(__file__), base_graph_source_sha256=sha(U.__file__), seed=1,
                    original_config=parent.config, target_config=result.config, cost=selected_cost(result),
                    gates='all initial output fields NSE<=1e-9; carried chunks<=1e-9; future/reset errors0; finite silence; added modules receive gradients after two synthetic steps',
                    scope='structural CPU preparation only. Same frontend/data semantics; no GPU training, renderer, budget waiver or scene-quality claim.')
    (a.out/'protocol.json').write_text(json.dumps(protocol,indent=2)+'\n')
    torch.save(dict(architecture=ARCHITECTURE, config=result.config, state=result.state_dict(),
                    parent_sha256=PARENT_SHA, mode='capacity_expanded_initial', step=0), a.out/'expanded_initial.pt')
    audit = smoke(parent, result)
    (a.out/'complete.json').write_text(json.dumps(dict(complete=True, passed=True, audit=audit,
        protocol_sha256=sha(a.out/'protocol.json'), expanded_initial_sha256=sha(a.out/'expanded_initial.pt')),indent=2)+'\n')
    print('UNIFIED_FUNCTION_EXPANSION_PASS', json.dumps(dict(cost=protocol['cost'],audit=audit)),flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent',type=Path,required=True); parser.add_argument('--out',type=Path,required=True)
    main(parser.parse_args())
