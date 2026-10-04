"""One bounded explicit-history trial, with predeclared matched terminal comparison."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from run_scene_fly_pair_nas import active_jobs
from train_scene_joint_nas import now, sha, write


def main():
    p = argparse.ArgumentParser()
    for name in ('root', 'parent', 'catalog', 'audio-root', 'anchor', 'oracle'):
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args()
    root, code = a.root.resolve(), Path(__file__).parent.resolve()
    if (root/'study_protocol.json').exists():
        raise ValueError('Fresh study root required')
    lease = root.parent/'.scene_history16_tenant'
    token = dict(pid=os.getpid(), root=str(root), started_utc=now())
    fd = os.open(lease, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    os.write(fd, json.dumps(token).encode()); os.close(fd)
    try:
        assert not active_jobs(root.parent, (os.getpid(),))
        oracle = json.loads((a.oracle/'report.json').read_text())
        assert oracle['complete'] and oracle['fitting_reachable']
        anchor = json.loads((a.anchor/'protocol.json').read_text())
        terminal = json.loads((a.anchor/'report.json').read_text())
        assert terminal['complete'] and terminal['terminal_step'] == 8192
        assert anchor['head_variant'] == 'mlp12'
        assert anchor.get('global_core', 'gru') == 'gru'
        assert anchor['specification']['architecture'] == 'joint-scene-band-global-mlp12-v1'
        for n in ('scene_joint_net.py', 'scene_joint_head_net.py', 'train_scene_joint_nas.py'):
            assert sha(code/n) == anchor['source_hashes'][n]
        study = dict(schema='scene-encoded-history16-screen-v1', started_utc=now(), steps=8192, threads=3,
                     oracle_report_sha256=sha(a.oracle/'report.json'),
                     anchor_protocol_sha256=sha(a.anchor/'protocol.json'),
                     anchor_report_sha256=sha(a.anchor/'report.json'),
                     anchor_metrics=terminal['terminal']['metrics'],
                     source_hashes={f.name:sha(f) for f in code.glob('*.py')},
                     comparison_rule='terminal8192 BOTH rear NMSE at least5% lower than MLP12; '
                                     'FL/FR/C no more than5% higher; dry relative null RMS<=1e-5',
                     fixed_crop_gate='BOTH rear NMSE<=.10 and dry relative null RMS<=1e-5',
                     scope='one fixed .699s TRAIN crop and one seed; no generalization/device/car claim',
                     no_auto_extension=True)
        write(root/'study_protocol.json', study)
        env = os.environ.copy()
        env.update(OMP_NUM_THREADS='3', MKL_NUM_THREADS='3', PYTHONUNBUFFERED='1')
        with (root/'checks.log').open('wb') as log:
            subprocess.run([sys.executable, str(code/'test_scene_history_net.py')],
                           stdout=log, stderr=subprocess.STDOUT, env=env, check=True)
        base = [sys.executable, str(code/'fit_scene_fixed_nas.py'), '--parent', str(a.parent),
                '--catalog', str(a.catalog), '--audio-root', str(a.audio_root),
                '--head', 'mlp12', '--history16', '--threads', '3']
        for name, steps in (('smoke', 20), ('run', 8192)):
            if (root/'STOP').exists():
                raise RuntimeError('Owned study STOP marker observed')
            assert not active_jobs(root.parent, (os.getpid(),))
            with (root/(name+'.log')).open('wb') as log:
                child = subprocess.Popen(base+['--out', str(root/name), '--steps', str(steps)],
                                         stdout=log, stderr=subprocess.STDOUT,
                                         stdin=subprocess.DEVNULL, env=env)
                write(root/'status.json', dict(state=name, supervisor_pid=os.getpid(),
                                              child_pid=child.pid, updated_utc=now()))
                if child.wait() != 0:
                    raise RuntimeError('Child failed: '+name)
            result = json.loads((root/name/'report.json').read_text())
            assert result['complete'] and result['terminal_step'] == steps
        current = result['terminal']['metrics']
        reference = study['anchor_metrics']
        conserved = current['dry_pcm_null_relative_rms'] <= 1e-5
        signal = (all(current['nmse'][k] <= .95*reference['nmse'][k] for k in (3, 4))
                  and all(current['nmse'][k] <= 1.05*reference['nmse'][k] for k in (0, 1, 2)))
        write(root/'comparison.json', dict(complete=True, candidate=current, anchor=reference,
              nmse_relative_reduction=[1-v/w for v,w in zip(current['nmse'], reference['nmse'])],
              comparison_pass=signal and conserved, conservation_pass=conserved,
              fixed_crop_pass=result['terminal_pass'], promotion_or_generalization=False))
        write(root/'status.json', dict(state='complete', supervisor_pid=os.getpid(), updated_utc=now()))
    except BaseException as exc:
        write(root/'status.json', dict(state='failed', supervisor_pid=os.getpid(), updated_utc=now(), error=str(exc)))
        raise
    finally:
        if lease.exists() and json.loads(lease.read_text()) == token:
            lease.unlink()


if __name__ == '__main__':
    main()
