"""Hash-bound fixed TRAIN output reachability screen; no network training."""
import argparse
import json
import math
import os
from pathlib import Path
import time

import numpy as np
import soundfile as sf
import torch
from torch import nn

from scene_joint_net import Config, JointSceneNet, analysis
from scene_operator_oracle import construct, raw_to_output, rear_nmse, refine
from fit_scene_fixed_nas import measure
from train_scene_joint_nas import now, sha, write


class TableHead(nn.Module):
    def __init__(self, raw, limit):
        super().__init__()
        self.table = torch.atanh(raw/limit).reshape(-1, 513, 20).transpose(1, 2)

    def forward(self, features):
        return self.table


def main():
    p = argparse.ArgumentParser()
    for n in ('anchor', 'catalog', 'audio-root', 'out'):
        p.add_argument('--'+n, type=Path, required=True)
    a = p.parse_args()
    if a.out.exists():
        raise ValueError('Fresh oracle output required')
    a.out.mkdir(parents=True)
    start = time.monotonic()
    try:
        write(a.out/'status.json', dict(state='preparing', pid=os.getpid(), updated_utc=now()))
        protocol = json.loads((a.anchor/'protocol.json').read_text())
        assert sha(a.catalog) == protocol['catalog_sha256']
        assert sha(Path(__file__).with_name('scene_joint_net.py')) == protocol['source_hashes']['scene_joint_net.py']
        assert sha(Path(__file__).with_name('train_scene_joint_nas.py')) == protocol['source_hashes']['train_scene_joint_nas.py']
        catalog = json.loads(a.catalog.read_text())
        assert catalog['pair_domain'] == 'known_downmix_of_same_target'
        row = next(r for r in catalog['rows'] if r['id'] == protocol['song'])
        assert row['split'] == protocol['partition'] == 'train'
        assert row['provenance'] == 'authored_multichannel'
        assert row['source_order'] == ['FL', 'FR', 'FC', 'LFE', 'SL', 'SR']
        source = (a.audio_root/row['path']).resolve(strict=True)
        assert source.is_relative_to(a.audio_root.resolve())
        assert sha(source) == row['sha256'] == protocol['audio_sha256']
        info = sf.info(source)
        assert (info.frames, info.samplerate, info.channels) == (row['frames'], 48000, 6)
        with sf.SoundFile(source) as stream:
            stream.seek(protocol['start_sample'])
            six = stream.read(protocol['length_samples'], dtype='float32', always_2d=True)
        assert six.shape == (protocol['length_samples'], 6) and np.isfinite(six).all()
        assert np.abs(six[:, 3]).max() <= 1e-7
        torch.set_num_threads(3); torch.set_num_interop_threads(1)
        target_pcm = torch.from_numpy(six[:, [0, 1, 2, 4, 5]].copy())*row['common_gain']
        net = JointSceneNet(Config(**protocol['specification']['config']))
        mixture = target_pcm[:, :2]+math.sqrt(.5)*(target_pcm[:, 2:3]+target_pcm[:, 3:])
        x, target = analysis(mixture), analysis(target_pcm)
        warm = protocol['warm_frames']
        denominator = torch.tensor(protocol['normalization_power'])
        frozen = dict(schema='scene-output-oracle-v1', started_utc=now(),
                      anchor_protocol_sha256=sha(a.anchor/'protocol.json'),
                      catalog_sha256=sha(a.catalog), audio_sha256=row['sha256'],
                      song=row['id'], partition='train', start_sample=protocol['start_sample'],
                      length_samples=protocol['length_samples'], common_gain=row['common_gain'],
                      warm_frames=warm, normalization_power=protocol['normalization_power'],
                      bound=net.config.coefficient_limit, interior_fraction=1-1e-5,
                      same_stock_projection=True, dev_audio_opened=False, effect_gain=0.,
                      privileged_target=True, causal_inference=False, network_training=False,
                      pass_rule='BOTH rear NMSE<=.10 and dry PCM relative null RMS<=1e-5',
                      source_hashes={n:sha(Path(__file__).with_name(n)) for n in
                       ['scene_joint_net.py', 'scene_operator_oracle.py', 'run_scene_operator_oracle_nas.py',
                        'fit_scene_fixed_nas.py', 'train_scene_joint_nas.py']})
        write(a.out/'protocol.json', frozen)
        unbounded = construct(x, target, net, bounded=False)
        raw = construct(x, target, net)
        before = rear_nmse(raw_to_output(raw, x, net), target, warm)
        history = []
        if not bool((before <= .10).all()):
            write(a.out/'status.json', dict(state='refining', pid=os.getpid(), updated_utc=now()))
            raw, history = refine(raw, x, target, net, warm, steps=1000)
        net.operator_head = TableHead(raw, net.config.coefficient_limit)
        metrics = measure(net, x, target, denominator, warm)
        bounded_output = raw_to_output(raw, x, net)
        actual, _ = net(x)
        equivalence = float((actual['base']-bounded_output).abs().max())
        assert equivalence < 1e-4 and raw.abs().max() < net.config.coefficient_limit
        assert torch.isfinite(torch.atanh(raw/net.config.coefficient_limit)).all()
        feasible = bool((torch.tensor(metrics['nmse'][3:]) <= .10).all()) and metrics['dry_pcm_null_relative_rms'] <= 1e-5
        certified_impossible = bool(history and history[-1]['tangent_lower_bound'] > .1001)
        np.save(a.out/'raw_coefficients.npy', raw.numpy(), allow_pickle=False)
        report = dict(complete=True, metrics=metrics, constructed_rear_nmse=before.tolist(),
                      unbounded_rear_nmse=rear_nmse(raw_to_output(unbounded, x, net), target, warm).tolist(),
                      unbounded_max_raw=float(unbounded.abs().max()), bounded_max_raw=float(raw.abs().max()),
                      clipped_raw_fraction=float((unbounded.abs() > frozen['bound']*frozen['interior_fraction']).float().mean()),
                      finite_head_logit_max=float(torch.atanh(raw/net.config.coefficient_limit).abs().max()),
                      stock_forward_max_difference=equivalence, refinement_history=history,
                      fitting_reachable=feasible, all_channel_nmse_below_point1=all(v <= .1 for v in metrics['nmse']),
                      certified_rear_gate_impossible=certified_impossible,
                      decision='reachable' if feasible else ('unreachable' if certified_impossible else 'inconclusive'),
                      coefficients_sha256=sha(a.out/'raw_coefficients.npy'), elapsed_seconds=time.monotonic()-start,
                      causal_or_product_qualified=False)
        write(a.out/'report.json', report)
        write(a.out/'status.json', dict(state='complete', pid=os.getpid(), updated_utc=now()))
        print(json.dumps(report), flush=True)
    except BaseException as exc:
        write(a.out/'status.json', dict(state='failed', pid=os.getpid(), error=str(exc), updated_utc=now()))
        raise


if __name__ == '__main__':
    main()
