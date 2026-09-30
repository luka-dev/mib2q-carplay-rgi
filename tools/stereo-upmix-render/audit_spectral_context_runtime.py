#!/usr/bin/env python3
"""Saved TRAIN spectral encoder: standalone C float parity and int8 state checks.

No broad acoustic, quantization quality or physical device claim. Uses only an
already inspected fitting fixture; never reads a DEV or reserved audio file.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES'] = '-1'
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import numpy as np
import torch
import train_spectral_context_fit as FT
import unified_spectral_context_net as V


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def execute(args):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr[-2000:])
    return result


def parse(path, streams, frames):
    raw = path.read_bytes()
    if struct.unpack_from('<8sIII', raw) != (b'UMXOT001', streams, frames, 17552):
        raise ValueError('Different C output shape')
    if len(raw) != 20 + streams * frames * 17552 * 4:
        raise ValueError('Different C output length')
    values = np.frombuffer(raw, dtype='<f4', offset=20).copy().reshape(streams, frames, 17552)
    if not np.isfinite(values).all():
        raise ValueError('Nonfinite C output')
    return values


def main(a):
    if a.out.exists():
        raise FileExistsError('Preserve prior spectral runtime evidence')
    torch.set_num_threads(2)
    net, ck = FT.load(a.checkpoint, 'cpu')
    net.eval()
    manifest = json.loads((a.assets / 'manifest.json').read_bytes())
    provenance = json.loads(a.fixture_manifest.read_bytes())
    if (ck['architecture'] != V.ARCHITECTURE or manifest['checkpoint_sha256'] != sha(a.checkpoint)
            or provenance['checkpoint_sha256'] != sha(a.checkpoint)
            or provenance['fixture_sha256'] != sha(a.fixture)):
        raise ValueError('Fixture/model/export provenance differs')
    for filename, record in manifest['files'].items():
        if sha(a.assets / filename) != record['sha256']:
            raise ValueError('Blob changed')
    raw = a.fixture.read_bytes()
    header = struct.unpack_from('<8sIII', raw)
    if header != (b'UMXSP001', 4, 64, 4100) or len(raw) != 20 + 4 * 64 * 4100 * 4:
        raise ValueError('Fixed four TRAIN streams / first64 frames required')
    inputs = np.frombuffer(raw, dtype='<f4', offset=20).copy().reshape(4, 64, 4100)
    x = torch.view_as_complex(torch.from_numpy(inputs.reshape(4, 64, 1025, 2, 2)).contiguous())
    a.out.mkdir(parents=True)
    runtime_dir = Path(__file__).parent / 'unified_spectral_runtime_v1'
    protocol = dict(version='spectral-context-runtime-train64-v1',
                    checkpoint_sha256=sha(a.checkpoint), fixture_sha256=sha(a.fixture),
                    fixture_manifest_sha256=sha(a.fixture_manifest), asset_manifest_sha256=sha(a.assets/'manifest.json'),
                    executable_sha256=sha(a.executable), checker_sha256=sha(a.checker), script_sha256=sha(Path(__file__)),
                    runtime_sources={p.name:sha(p) for p in sorted(runtime_dir.glob('*')) if p.is_file()},
                    selection='Four original already inspected TRAIN crops; first64 GPU-saved STFT frames, no holdout',
                    float_gate='All-source spectral reconstruction NSE<=1e-9; each scene maxabs<=1e-4; exact declared dense count',
                    state_gate='All/selected eligible+scene+full padded state bit exact with four interleaved streams; protected rows zero; repeat/reset and changed-future first32 frames exact, float and int8',
                    int8_limits='Per-row weights and dynamic activations only; int8-vs-float errors are diagnostics, not acoustic quality success',
                    scope='Standalone neural native-host numeric/selection checks; no FFT/DSP, bright rear, broad quality, Audi timing, trained scene, int8 acoustic or live hook claim')
    write(a.out/'protocol.json', protocol)
    with torch.inference_mode():
        pred, _ = net.forward_stft(x, with_scene=True)
    changed = inputs.copy()
    changed[:, 32:] *= -3
    # DC/Nyquist imaginary zero remains valid under this suffix perturbation.
    future_path = a.out/'changed_future.spectra'
    future_path.write_bytes(raw[:20] + changed.tobytes())
    results = {}; arrays = {}
    source_names = FT.T.U.SOURCES
    fields = ('family_presence', 'membership', 'family_pan', 'membership_update')
    for mode in ('float', 'int8'):
        blob = a.assets/('unified_'+mode+'.bin')
        output = a.out/(mode+'.output')
        r = execute([a.executable, blob, a.fixture, output])
        (a.out/(mode+'.log')).write_text(r.stderr)
        values = parse(output,4,64); arrays[mode] = values
        masks = torch.view_as_complex(torch.from_numpy(values[:,:,:16400].copy()).reshape(4,64,4,1025,2,2).contiguous())
        estimate = masks*x[:,:,None]
        nse = {s:float((estimate[:,:,i]-pred['source_stft'][:,:,i]).abs().double().square().sum()/pred['source_stft'][:,:,i].abs().double().square().sum().clamp_min(1e-12)) for i,s in enumerate(source_names)}
        scene = {field:float((torch.from_numpy(values[:,:,16400+i*288:16400+(i+1)*288].copy()).reshape(4,64,48,6)-pred[field]).abs().max()) for i,field in enumerate(fields)}
        performance = json.loads(r.stderr)
        repeat = a.out/(mode+'_repeat.output');execute([a.executable,blob,a.fixture,repeat])
        altered = a.out/(mode+'_changed_future.output');execute([a.executable,blob,future_path,altered])
        future = parse(altered,4,64)
        gates = dict(dense_count_exact=performance['dense_macs_per_frame']==sum(net.cost()['all_source_dense_macs_per_frame'].values()),
                     repeat_reset_exact=repeat.read_bytes()==output.read_bytes(),future_prefix_exact=np.array_equal(future[:,:32],values[:,:32]))
        if mode=='float':
            gates.update(all_source_parity=max(nse.values())<=1e-9, scene_parity=max(scene.values())<=1e-4)
        results[mode] = dict(source_estimate_nse_vs_float_torch=nse,scene_maxabs_vs_float_torch=scene,
                             performance=performance,gates=gates,output_sha256=sha(output))
    pruning = execute([a.checker,a.assets/'unified_float.bin',a.assets/'unified_int8.bin',a.fixture])
    rows = [json.loads(line.split('UNIFIED_PRUNE_CHECK ',1)[1]) for line in pruning.stdout.splitlines() if line.startswith('UNIFIED_PRUNE_CHECK ')]
    selected = sum(net.cost()['eligible_scene_dense_macs_per_frame'].values())
    pruning_pass = len(rows)==2 and all(r['passed'] and r['all_dense_macs_frame']==sum(net.cost()['all_source_dense_macs_per_frame'].values()) and r['selected_dense_macs_frame']==selected for r in rows)
    (a.out/'pruning.log').write_text(pruning.stdout)
    # Save16 reference frames per stream for independently built ARM execution.
    record = np.concatenate([inputs[:,:16],arrays['float'][:,:16],arrays['int8'][:,:16]],-1).astype('<f4')
    arm = a.out/'arm_fixture.bin';arm.write_bytes(struct.pack('<8sII',b'UMXQA001',4,16)+record.tobytes())
    write(a.out/'arm_fixture_manifest.json',dict(checkpoint_sha256=sha(a.checkpoint),fixture_sha256=sha(arm),
          original_fixture_sha256=sha(a.fixture),reference_executable_sha256=sha(a.executable),
          reference_outputs={mode:results[mode]['output_sha256'] for mode in results},
          selection='First16 frames of each inspected TRAIN64 replay; native float/int8 reference, four streams',
          scope='ARM numeric parity input only; no device timing or int8 acoustic claim'))
    passed = pruning_pass and all(all(r['gates'].values()) for r in results.values())
    result = dict(complete=True,passed=passed,arms=results,pruning=rows,pruning_pass=pruning_pass,
                  protocol_sha256=sha(a.out/'protocol.json'),arm_fixture_sha256=sha(arm),scope=protocol['scope'])
    write(a.out/'complete.json',result)
    print('SPECTRAL_RUNTIME_NATIVE_AUDIT',json.dumps(dict(passed=passed,arms=results,pruning=rows)),flush=True)
    if not passed:
        raise ValueError('Spectral runtime numeric failure retained')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('checkpoint','assets','fixture','fixture-manifest','executable','checker','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    main(parser.parse_args())
