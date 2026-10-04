"""Read-only channel-gradient geometry at the retained MLP12 terminal weights."""
import argparse
import json
import math
import os
from pathlib import Path
import sys

import numpy as np
import soundfile as sf
import torch


def main():
    p = argparse.ArgumentParser()
    for n in ('code', 'anchor', 'catalog', 'audio-root', 'out'):
        p.add_argument('--'+n, type=Path, required=True)
    a = p.parse_args()
    sys.path.insert(0, str(a.code))
    from scene_joint_net import Config, analysis
    from scene_joint_head_net import JointSceneHeadNet
    from train_scene_joint_nas import sha, write, now
    from run_scene_fly_pair_nas import active_jobs
    assert not a.out.exists() and not active_jobs(a.anchor.parent.parent, (os.getpid(),))
    protocol = json.loads((a.anchor/'protocol.json').read_text())
    report = json.loads((a.anchor/'report.json').read_text())
    assert report['complete'] and report['terminal_step'] == 8192
    assert protocol['specification']['architecture'] == 'joint-scene-band-global-mlp12-v1'
    assert sha(a.catalog) == protocol['catalog_sha256']
    for n in ('scene_joint_net.py', 'scene_joint_head_net.py', 'train_scene_joint_nas.py'):
        assert sha(a.code/n) == protocol['source_hashes'][n]
    row = next(r for r in json.loads(a.catalog.read_text())['rows'] if r['id'] == protocol['song'])
    assert row['split'] == protocol['partition'] == 'train'
    assert row['source_order'] == ['FL', 'FR', 'FC', 'LFE', 'SL', 'SR']
    source = (a.audio_root/row['path']).resolve(strict=True)
    assert source.is_relative_to(a.audio_root.resolve()) and sha(source) == protocol['audio_sha256']
    with sf.SoundFile(source) as stream:
        stream.seek(protocol['start_sample'])
        six = stream.read(protocol['length_samples'], dtype='float32', always_2d=True)
    assert six.shape == (protocol['length_samples'], 6) and np.isfinite(six).all()
    assert np.abs(six[:, 3]).max() <= 1e-7
    torch.set_num_threads(3); torch.set_num_interop_threads(1)
    net = JointSceneHeadNet(Config(**protocol['specification']['config']))
    checkpoint = a.anchor/'terminal.pt'
    assert sha(checkpoint) == report['terminal_checkpoint_sha256']
    saved = torch.load(checkpoint, map_location='cpu', weights_only=True)
    assert saved['step'] == 8192 and saved['protocol_sha256'] == sha(a.anchor/'protocol.json')
    net.load_state_dict(saved['model'], strict=True)
    target_pcm = torch.from_numpy(six[:, [0, 1, 2, 4, 5]].copy())*row['common_gain']
    mixture = target_pcm[:, :2]+math.sqrt(.5)*(target_pcm[:, 2:3]+target_pcm[:, 3:])
    x, target = analysis(mixture), analysis(target_pcm)
    prediction, _ = net(x)
    pred, truth = prediction['base'][:, protocol['warm_frames']:], target[:, protocol['warm_frames']:]
    squared = (pred-truth).abs().square().mean((0, 1, 2))
    magnitude = (pred.abs()-truth.abs()).abs().mean((0, 1, 2))
    original_power = torch.tensor(protocol['normalization_power'])
    crop_power = truth.abs().square().mean((0, 1, 2))
    assert (crop_power > 1e-8).all()
    original_terms = squared/original_power+.05*magnitude/original_power.sqrt()
    assert abs(float(original_terms.mean().detach())-report['terminal']['metrics']['loss']) < 1e-6
    balanced_terms = squared/crop_power+.05*magnitude/crop_power.sqrt()
    nmse = squared/crop_power
    torch.testing.assert_close(nmse, torch.tensor(report['terminal']['metrics']['nmse']), atol=1e-6, rtol=1e-5)
    parameters = list(net.parameters())
    def gradient(value):
        parts = torch.autograd.grad(value, parameters, retain_graph=True)
        result = torch.cat([part.flatten() for part in parts])
        assert torch.isfinite(result).all()
        return result
    original = torch.stack([gradient(term) for term in original_terms])
    gate_gradients = torch.stack([gradient(term) for term in nmse])
    total = original.mean(0)
    balanced = gradient(balanced_terms.mean())
    norm = original.norm(dim=1)
    cosine = original@original.T/(norm[:, None]*norm[None, :]).clamp_min(1e-20)
    # Derivative of each NMSE along UNIT negative total-gradient direction.
    # This is local SGD geometry, not an AdamW trajectory or a parameter step.
    slope_original = -(gate_gradients@total)/total.norm().clamp_min(1e-20)
    slope_balanced = -(gate_gradients@balanced)/balanced.norm().clamp_min(1e-20)
    a.out.mkdir()
    result = dict(complete=True, captured_utc=now(), anchor_checkpoint_sha256=sha(checkpoint),
                  anchor_protocol_sha256=sha(a.anchor/'protocol.json'),
                  catalog_sha256=sha(a.catalog), audio_sha256=sha(source),
                  channels=['FL', 'FR', 'C', 'Ls', 'Rs'], nmse=nmse.detach().tolist(),
                  original_terms=original_terms.detach().tolist(), balanced_terms=balanced_terms.detach().tolist(),
                  original_channel_gradient_norm_before_common_mean=norm.tolist(),
                  original_channel_gradient_cosine=cosine.tolist(),
                  original_total_gradient_norm=float(total.norm()), balanced_total_gradient_norm=float(balanced.norm()),
                  original_balanced_total_gradient_cosine=float(torch.dot(total, balanced)/(total.norm()*balanced.norm())),
                  nmse_slope_along_unit_original_descent=slope_original.tolist(),
                  nmse_slope_along_unit_balanced_descent=slope_balanced.tolist(),
                  balanced_definition='per-crop TRAIN power-normalized complex MSE + .05 magnitude MAE; hypothetical objective only',
                  slope_interpretation='negative local directional derivative means NMSE decreases for infinitesimal plain gradient descent; not AdamW or a training outcome',
                  optimizer_steps=0, weights_changed=False, dev_or_heldout_opened=False,
                  source_hashes={n:sha(a.code/n) for n in ('scene_joint_net.py', 'scene_joint_head_net.py', 'train_scene_joint_nas.py')},
                  auditor_sha256=sha(Path(__file__)))
    write(a.out/'report.json', result)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
