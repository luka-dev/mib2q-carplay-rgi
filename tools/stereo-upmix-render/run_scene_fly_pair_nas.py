"""Finite serialized topology screen; existing matched GRU curve is the anchor."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from train_scene_joint_nas import now, sha, write


def active_jobs(runtime, exclude=()):
    jobs = []
    for p in Path('/proc').iterdir():
        if not p.name.isdigit() or int(p.name) in exclude:
            continue
        try:
            args = (p/'cmdline').read_bytes().decode().split('\0')
        except (OSError, UnicodeDecodeError):
            continue
        if args and 'python' in Path(args[0]).name:
            scripts = [a for a in args[1:] if a.startswith(str(runtime)) and a.endswith('.py')]
            if scripts:
                jobs.append(dict(pid=int(p.name), scripts=[Path(a).name for a in scripts]))
    return jobs


def main():
    parser = argparse.ArgumentParser()
    for name in ('root', 'parent', 'catalog', 'audio-root', 'anchor'):
        parser.add_argument('--'+name, type=Path, required=True)
    a = parser.parse_args()
    root, code = a.root.resolve(), Path(__file__).parent.resolve()
    if (root/'study_protocol.json').exists():
        raise ValueError('Fresh study root required')
    runtime = root.parent
    own = os.getpid()
    lease = runtime/'.scene_fly_tenant'
    fd = os.open(lease, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    token = dict(pid=own, root=str(root), started_utc=now())
    os.write(fd, json.dumps(token).encode()); os.close(fd)
    try:
        if active_jobs(runtime, (own,)):
            raise RuntimeError('Another upmix Python tenant is active')
        graph = code/'scene_flywire_783_64.json'
        source_hashes = {p.name: sha(p) for p in code.glob('*.py')}
        anchor_protocol = json.loads((a.anchor/'protocol.json').read_text())
        anchor_monitor = json.loads((a.anchor/'monitor04096.json').read_text())
        parent = json.loads((a.parent/'protocol.json').read_text())
        if anchor_monitor['step'] != 4096 or anchor_protocol['head_variant'] != 'mlp12':
            raise ValueError('Completed MLP12/GRU matched anchor required')
        for name in ('scene_joint_net.py', 'scene_joint_head_net.py', 'train_scene_joint_nas.py'):
            if source_hashes[name] != anchor_protocol['source_hashes'][name]:
                raise ValueError('Anchor body/head/loss source changed')
        study = dict(schema='scene-fly-topology-screen-v1', started_utc=now(), pid=own,
                     variants=['fly', 'shuffled'], steps_each=4096, threads=3,
                     seed=parent['seed'], graph_sha256=sha(graph), source_hashes=source_hashes,
                     anchor_protocol_sha256=sha(a.anchor/'protocol.json'),
                     anchor_monitor_sha256=sha(a.anchor/'monitor04096.json'),
                     anchor_metrics=anchor_monitor['metrics'],
                     topology_signal_rule='Fly rear NMSE at least5% lower than shuffled in BOTH '
                                          'channels; FL/FR/C no more than5% higher than shuffled',
                     practical_anchor_rule='Fly BOTH rear NMSE lower than GRU; FL/FR/C '
                                           'no more than5% higher than GRU at matched4096',
                     fixed_crop_gate='BOTH rear NMSE<=.10 and dry PCM relative null RMS<=1e-5',
                     cost_rule='both reservoirs use identical parameters and actual dense kernels; '
                               'all variants below111.12M estimated dense neural MAC/s; GRU '
                               'has more learned recurrence parameters, not a capacity-matched ablation',
                     scope='one seed, one .699s TRAIN crop; no DEV/heldout/device/source-role/car claim',
                     no_auto_extension=True)
        write(root/'study_protocol.json', study)
        for variant in study['variants']:
            if (root/'STOP').exists():
                raise RuntimeError('Owned study STOP marker observed')
            if active_jobs(runtime, (own,)):
                raise RuntimeError('Another upmix Python tenant became active')
            command = [sys.executable, str(code/'fit_scene_fixed_nas.py'),
                       '--parent', str(a.parent), '--catalog', str(a.catalog),
                       '--audio-root', str(a.audio_root), '--out', str(root/variant),
                       '--steps', '4096', '--threads', '3', '--head', 'mlp12',
                       '--global-core', variant, '--graph', str(graph)]
            with (root/(variant+'.log')).open('wb') as log:
                child = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                         stdin=subprocess.DEVNULL)
                write(root/'status.json', dict(state='training', active_variant=variant,
                                              child_pid=child.pid, supervisor_pid=own, updated_utc=now()))
                if child.wait() != 0:
                    raise RuntimeError('Child failed: '+variant)
            report = json.loads((root/variant/'report.json').read_text())
            if not report['complete'] or report['terminal_step'] != 4096:
                raise RuntimeError('Incomplete child report')
        fly = json.loads((root/'fly/report.json').read_text())['terminal']['metrics']
        shuffled = json.loads((root/'shuffled/report.json').read_text())['terminal']['metrics']
        anchor = study['anchor_metrics']
        signal = (all(fly['nmse'][k] <= .95*shuffled['nmse'][k] for k in (3, 4))
                  and all(fly['nmse'][k] <= 1.05*shuffled['nmse'][k] for k in (0, 1, 2)))
        practical = (all(fly['nmse'][k] < anchor['nmse'][k] for k in (3, 4))
                     and all(fly['nmse'][k] <= 1.05*anchor['nmse'][k] for k in (0, 1, 2)))
        conserved = all(m['dry_pcm_null_relative_rms'] <= 1e-5 for m in (fly, shuffled))
        write(root/'comparison.json', dict(complete=True, metrics=dict(gru=anchor, fly=fly,
              shuffled=shuffled), topology_signal_pass=signal and conserved,
              practical_anchor_pass=practical and conserved, conservation_pass=conserved,
              fixed_crop_pass=all(fly['nmse'][k] <= .1 for k in (3, 4)) and conserved,
              promotion_or_generalization=False))
        write(root/'status.json', dict(state='complete', supervisor_pid=own, updated_utc=now()))
    except BaseException as exc:
        write(root/'status.json', dict(state='failed', supervisor_pid=own, updated_utc=now(), error=str(exc)))
        raise
    finally:
        if lease.exists() and json.loads(lease.read_text()) == token:
            lease.unlink()


if __name__ == '__main__':
    main()
