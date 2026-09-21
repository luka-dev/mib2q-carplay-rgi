#!/usr/bin/env python3
"""Explicit electronic/acoustic bridge; no invented cabin calibration.

most_inputs: six measured end-to-end responses, NEVER apply AmpChain again.
post_amp_config_outputs: sixteen responses after the modeled config outputs;
requires an explicit AmpChain and explicit electronic zero-padding duration.
Declared measurement provenance is not independent hardware validation.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import oaconvolve

from amp_dsp_chain import AmpChain, IN, SR
from amp_music_report import fingerprint, load_upstream_scenario, read_audio

DOMAINS = {'most_inputs': IN, 'post_amp_config_outputs': [f'cfg{i:02d}' for i in range(16)]}


def _nonempty(value):
    return (isinstance(value, str) and bool(value.strip())) or (isinstance(value, dict) and bool(value))


def load_transfer(path):
    """Hash-check every stereo FIR, preserving gain, sign and relative timing."""
    path = Path(path).resolve()
    manifest = json.loads(path.read_text())
    if not isinstance(manifest, dict):
        raise ValueError('manifest must be an object')
    domain = manifest.get('domain')
    if (type(manifest.get('schema_version')) is not int or manifest['schema_version'] != 1
            or manifest.get('sample_rate') != SR or not isinstance(domain, str) or domain not in DOMAINS):
        raise ValueError('schema1, 48000 Hz and explicit supported acoustic domain required')
    channels = DOMAINS[domain]
    if (manifest.get('channels') != channels or not isinstance(manifest.get('responses'), dict)
            or set(manifest['responses']) != set(channels)):
        raise ValueError('every domain channel is required in exact order; no inferred fan-out')
    provenance = manifest.get('provenance', {})
    if not isinstance(provenance, dict) or provenance.get('kind') not in ('measured', 'synthetic') or not _nonempty(provenance.get('description')):
        raise ValueError('explicit measured/synthetic provenance and description required')
    capture = manifest.get('capture', {})
    if not isinstance(capture, dict) or not all(_nonempty(capture.get(k)) for k in ('seat', 'state', 'microphone_calibration', 'level_reference')):
        raise ValueError('capture seat/state/microphone_calibration/level_reference required')
    if 'global_ir_gain_db' in manifest:
        raise ValueError('global normalization is not supported; supply explicit per-response gain_db')
    irs, sources = {}, {}
    for name in channels:
        record = manifest['responses'][name]
        if not isinstance(record, dict) or not isinstance(record.get('file'), str) or not record['file'].strip():
            raise ValueError(f'{name}: response file record required')
        file = (path.parent / record['file']).resolve()
        sha = hashlib.sha256(file.read_bytes()).hexdigest()
        if record.get('sha256') != sha:
            raise ValueError(f'{name}: response hash mismatch')
        ir, rate = sf.read(file, always_2d=True, dtype='float64')
        gain = record.get('gain_db')
        if (rate != SR or ir.shape[1] != 2 or len(ir) < 1 or not np.isfinite(ir).all()
                or isinstance(gain, bool) or not isinstance(gain, (int, float))
                or not np.isfinite(gain) or abs(gain) > 120):
            raise ValueError(f'{name}: finite nonempty 48k stereo IR and explicit gain_db within +/-120 required')
        irs[name] = ir * 10**(gain/20)
        sources[name] = dict(path=str(file), sha256=sha, frames=len(ir), gain_db=gain)
    if not any(np.any(v) for v in irs.values()):
        raise ValueError('all acoustic responses are silent')
    metadata = dict(domain=domain, channels=list(channels), sample_rate=SR,
                    provenance=provenance, capture=capture, manifest=fingerprint(path),
                    responses=sources, response_hashes_verified=True, responses_validated=True,
                    hardware_verified=False,
                    stage_order=['upmix_pcm'] + ([] if domain == 'most_inputs' else ['amp_chain']) + ['measured_brir'],
                    boundary='declared provenance only; no live state, physical mapping or measurement accuracy proof; fixed linear acoustic transfer')
    return irs, metadata


def validate_manifest(path):
    return load_transfer(path)[1]


def convolve_outputs(audio, responses):
    """Coherent sum at each ear, full FIR tails, no normalization/clipping."""
    if np.iscomplexobj(audio) or any(np.iscomplexobj(h) for h in responses):
        raise ValueError('real audio and responses required')
    x = np.asarray(audio, dtype=float)
    if x.ndim != 2 or x.shape[1] != len(responses) or not len(x) or not np.isfinite(x).all():
        raise ValueError('finite nonempty frames x response-count audio required')
    impulse = [np.asarray(h, dtype=float) for h in responses]
    if any(h.ndim != 2 or h.shape[1] != 2 or not len(h) or not np.isfinite(h).all() for h in impulse):
        raise ValueError('finite nonempty stereo FIRs required')
    out = np.zeros((len(x)+max(map(len, impulse))-1, 2))
    for channel, h in enumerate(impulse):
        for ear in range(2):
            y = oaconvolve(x[:, channel], h[:, ear], mode='full')
            out[:len(y), ear] += y
    if not np.isfinite(out).all():
        raise ValueError('acoustic convolution overflow')
    return out


def render(x6, manifest_path, *, chain=None, electronic_tail_frames=None, stage_inputs=None):
    """Caller explicitly supplies all non-default electronic scenarios.

    stage_inputs may contain first_b, second_b, second_b_auxiliary, complex_auxiliary; supplied
    boundary arrays must include the requested padded electronic tail.
    Finite zero padding does not prove that an IIR/adaptive tail has decayed.
    """
    if np.iscomplexobj(x6):
        raise ValueError('real PCM6 required')
    x = np.asarray(x6, dtype=float)
    if x.ndim != 2 or x.shape[1] != 6 or not len(x) or not np.isfinite(x).all():
        raise ValueError('finite nonempty PCM6 required')
    irs, metadata = load_transfer(manifest_path)
    supplied = {} if stage_inputs is None else dict(stage_inputs)
    if set(supplied) - {'first_b', 'second_b', 'second_b_auxiliary', 'complex_auxiliary'}:
        raise ValueError('unknown stage input')
    if metadata['domain'] == 'most_inputs':
        if chain is not None or electronic_tail_frames is not None or supplied:
            raise ValueError('MOST end-to-end IR already includes DSP: no AmpChain or electronic tail allowed')
        electrical = x
        model = None
    else:
        if not isinstance(chain, AmpChain):
            raise ValueError('post-amp responses require an explicit AmpChain')
        if type(electronic_tail_frames) is not int or not 0 <= electronic_tail_frames <= 60*SR:
            raise ValueError('explicit electronic_tail_frames in 0..2880000 required')
        electrical = chain.process(np.pad(x, ((0, electronic_tail_frames), (0, 0))),
                                   capture_stages=False, **supplied)['out']
        model = chain.metadata()
    out = convolve_outputs(electrical, [irs[name] for name in metadata['channels']])
    report = dict(model=model, acoustics=metadata, input_frames=len(x), output_frames=len(out),
                  electronic_tail_frames=electronic_tail_frames,
                  acoustic_tail_frames=max(len(h) for h in irs.values())-1,
                  electronic_tail_fully_settled_proven=False,
                  boundary_inputs={name: {'shape':list(np.asarray(value).shape),
                    'sha256':hashlib.sha256(np.asarray(value, dtype='<f8').tobytes()).hexdigest()}
                    for name,value in supplied.items()},
                  peak_dbfs=float(20*np.log10(max(float(np.max(abs(out))), 1e-15))),
                  normalization='none; absolute scale depends on declared IR level reference; not calibrated SPL',
                  fidelity='synthetic fixture only' if metadata['provenance']['kind']=='synthetic' else
                           'declared measured transfer; state/mapping/linearity/measurement accuracy not independently verified')
    return out, report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--electronics-hypothesis', action='store_true',
                   help='explicitly accept B0/C4D17 identity/omitted runtime stages; metadata retains warnings')
    p.add_argument('--upstream-scenario', type=Path)
    p.add_argument('--electronic-tail-frames', type=int)
    a = p.parse_args()
    report_path = a.output.with_suffix(a.output.suffix+'.report.json')
    if a.output.exists() or report_path.exists():
        p.error('output and sidecar must not already exist')
    if a.upstream_scenario and not a.electronics_hypothesis:
        p.error('--upstream-scenario requires explicit --electronics-hypothesis')
    chain = AmpChain(upstream_scenario=load_upstream_scenario(a.upstream_scenario)
                     if a.upstream_scenario else None) if a.electronics_hypothesis else None
    y, report = render(read_audio(a.input, 6), a.manifest, chain=chain,
                       electronic_tail_frames=a.electronic_tail_frames)
    if np.max(abs(y)) > np.finfo(np.float32).max:
        raise ValueError('audio cannot be represented in float32 WAV')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    # Float WAV preserves over-range samples; never silently normalize or clip.
    sf.write(a.output, y, SR, subtype='FLOAT')
    reread = read_audio(a.output, 2)
    if not np.allclose(reread, y, rtol=1e-7, atol=1e-12):
        raise ValueError('saved float WAV differs beyond float32 rounding')
    report.update(input=fingerprint(a.input), output=fingerprint(a.output),
                  saved_peak_dbfs=float(20*np.log10(max(float(np.max(abs(reread))), 1e-15))),
                  complete=True)
    report_path.write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print('saved', a.output, report['fidelity'])


if __name__ == '__main__':
    main()
