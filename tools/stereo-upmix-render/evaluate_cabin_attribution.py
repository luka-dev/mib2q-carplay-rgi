#!/usr/bin/env python3
"""Attribute rear/source output using controls from the MIXTURE, not stem reruns.

This measures the pre-guard, pre-gain DSP and a two-stem teacher, not ground-truth
separation or driver acoustics. It fails if frozen-transfer reconstruction does
not match the actual C mixture output. Host-only instrumentation is required.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import numpy as np
import soundfile as sf
from prepare_hssr_native_teacher import POLICY, digest, output_pair, valid_pair


def apply_transfers(audio, matrices, analysis, synthesis):
    """512/128 WOLA, exact captured windows and 512-sample C pipeline delay."""
    padded = np.pad(audio, ((384, 512), (0, 0)))
    result = np.zeros((len(audio) + 640, 4), dtype=np.float64)
    for hop, matrix in enumerate(matrices):
        start = hop * 128
        spectrum = np.fft.rfft(padded[start:start+512] * analysis[:, None], axis=0)
        transformed = np.einsum('boi,bi->bo', matrix.reshape(257, 4, 2), spectrum)
        chunk = np.fft.irfft(transformed, n=512, axis=0) * synthesis[:, None]
        result[start+128:start+640] += chunk
    result[:512] = 0
    return result[:len(audio)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--model', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--teacher-root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--limit', type=int, default=0)
    a = p.parse_args()
    items = json.loads(a.manifest.read_text())['items']
    if a.limit: items = items[:a.limit]
    if a.output.exists(): raise ValueError('use a fresh report path')
    rows = []
    for item in items:
        teacher = a.teacher_root/item['id']
        identity = json.loads((teacher/'teacher-provenance.json').read_text())['identity']
        if (identity['policy'] != POLICY or identity['source_sha256'] != digest(item['stereo'])
                or not valid_pair(teacher, identity)):
            raise ValueError('unverified teacher')
        pcm, rate = sf.read(item['stereo'], dtype='int16', always_2d=True)
        if rate != 48000 or pcm.shape[1] != 2: raise ValueError('expected 48k stereo')
        mix = pcm.astype(np.float64)/32768
        vocal, _ = sf.read(output_pair(teacher)[0], always_2d=True)
        instrumental, _ = sf.read(output_pair(teacher)[1], always_2d=True)
        if vocal.shape != mix.shape or np.max(abs(mix-vocal-instrumental)) > 2e-6:
            raise ValueError('teacher does not reconstruct exact S16 input')
        with tempfile.TemporaryDirectory(prefix='hssr-attribution-') as tmp:
            d = Path(tmp); pcm.astype('<i2').tofile(d/'input.s16')
            subprocess.run([str(a.capture.resolve()), str(a.model.resolve()),
                str(d/'input.s16'), str(d/'transfers'), str(d/'output')], check=True)
            raw = np.fromfile(d/'transfers', dtype='<f4')
            actual = np.fromfile(d/'output', dtype='<f4').reshape(-1, 4)
        analysis, synthesis = raw[:512], raw[512:1024]
        matrices = raw[1024:].reshape(-1, 257, 8)
        if len(matrices) != len(mix)//128: raise ValueError('missing hops')
        reconstructed = apply_transfers(mix, matrices, analysis, synthesis)
        # ±2 rear bound is nonlinear. Reject rather than misattribute clipping.
        if np.max(abs(reconstructed[:, :2])) >= 2: raise ValueError('rear clamp active')
        error = float(np.max(abs(reconstructed-actual)))
        if error > 2e-5: raise ValueError(f'C reconstruction mismatch {error}')
        vr = apply_transfers(vocal, matrices, analysis, synthesis)
        ir = apply_transfers(instrumental, matrices, analysis, synthesis)
        additivity = float(np.max(abs(vr+ir-reconstructed)))
        if additivity > 2e-6: raise ValueError('stem response additivity failed')
        ve, ie = np.sum(vr*vr, axis=0), np.sum(ir*ir, axis=0)
        cross = 2*np.sum(vr*ir, axis=0)
        row = dict(id=item['id'], c_reconstruction_max_error=error,
            stem_response_additivity_max_error=additivity,
            vocal_energy=ve.tolist(), instrumental_energy=ie.tolist(),
            cross_energy=cross.tolist(), mixture_energy=np.sum(actual**2, axis=0).tolist(),
            vocal_rear_transfer_db=float(10*np.log10((ve[:2].sum()+1e-20)/(np.sum(vocal*vocal)+1e-20))),
            instrumental_rear_transfer_db=float(10*np.log10((ie[:2].sum()+1e-20)/(np.sum(instrumental*instrumental)+1e-20))))
        rows.append(row)
        print(json.dumps(row), flush=True)
    report = dict(schema=1, mode='cabin-mixture-conditioned-sync-control-pre-guard',
        channels=['rear-left','rear-right','source-left','source-right'],
        caveats=['Teacher stems are estimates, not ground truth.',
                 'Cross energy is explicit; stem energies are not additive percentages.',
                 'No final Sound Check/limiter, asynchronous deadline jitter or speaker filters.'],
        capture_sha256=digest(a.capture), model_sha256=digest(a.model),
        manifest_sha256=digest(a.manifest), items=rows)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(report, indent=2)+'\n')
    return 0

if __name__ == '__main__': raise SystemExit(main())
