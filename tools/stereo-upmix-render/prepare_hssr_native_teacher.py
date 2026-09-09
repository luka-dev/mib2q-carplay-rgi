#!/usr/bin/env python3
"""Native-rate, shared-gain float teacher. Never overwrites stale pairs."""
from __future__ import annotations
import argparse
import hashlib
import importlib.metadata
import json
import logging
import shutil
from pathlib import Path
import time
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

MODEL = 'model_bs_roformer_ep_317_sdr_12.9755.ckpt'
SUFFIX = 'model_bs_roformer_ep_317_sdr_12.wav'
POLICY = 'native44100-shared-gain-float48-mixture-consistent-v2'

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def output_pair(directory):
    return tuple(directory/f'stereo_({stem})_{SUFFIX}' for stem in ('Vocals','Instrumental'))

def valid_pair(directory, identity):
    try:
        record=json.loads((directory/'teacher-provenance.json').read_text())
        return record['identity']==identity and all(
            sf.info(p).samplerate==48000 and sf.info(p).channels==2 and
            sf.info(p).subtype=='FLOAT' and sf.info(p).frames==identity['frames'] and
            digest(p)==record['output_sha256'][p.name] for p in output_pair(directory))
    except (RuntimeError,OSError,ValueError,KeyError):
        return False

def restore_pair(mixture, native_vocal, common_gain):
    if (not np.isfinite(common_gain) or common_gain<=0 or
            native_vocal.ndim!=2 or native_vocal.shape[1]!=2 or
            not np.isfinite(native_vocal).all()):
        raise ValueError('invalid teacher waveform/gain')
    vocal=resample_poly(native_vocal.astype(np.float64),160,147,axis=0)
    if len(vocal)<len(mixture):
        raise ValueError('truncated teacher waveform')
    vocal=(vocal[:len(mixture)]/common_gain).astype(np.float32)
    # Complement preserves the original 48k mix, including resampling residual.
    # Additivity does not establish source-separation quality.
    instrumental=(mixture.astype(np.float64)-vocal).astype(np.float32)
    residual=float(np.max(np.abs(mixture.astype(np.float64)-vocal-instrumental)))
    return vocal,instrumental,residual

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest',type=Path,required=True)
    p.add_argument('--teacher-root',type=Path,required=True)
    p.add_argument('--model-dir',type=Path,required=True)
    p.add_argument('--limit',type=int,default=0)
    p.add_argument('--minimum-free-gib',type=float,default=15)
    a=p.parse_args()
    items=json.loads(a.manifest.read_text())['items']
    common=dict(policy=POLICY,model_sha256=digest(a.model_dir/MODEL),
        config_sha256=digest((a.model_dir/MODEL).with_suffix('.yaml')),
        audio_separator=importlib.metadata.version('audio-separator'),
        torch=importlib.metadata.version('torch'))
    identities={};pending=[]
    for item in items:
        source=Path(item['stereo']);info=sf.info(source)
        if info.samplerate!=48000 or info.channels!=2 or not info.frames:
            raise ValueError(f'invalid source {source}')
        identity=dict(common,source_sha256=digest(source),frames=info.frames)
        identities[item['id']]=identity;directory=a.teacher_root/item['id']
        if not valid_pair(directory,identity):
            if any(path.exists() for path in output_pair(directory)):
                raise ValueError(f'stale pair: {directory}; use a new output root')
            pending.append(item)
    if a.limit: pending=pending[:a.limit]
    print(f'teacher pending={len(pending)} total={len(items)} policy={POLICY}',flush=True)
    if pending:
        from audio_separator.separator import Separator
        separator=Separator(log_level=logging.WARNING,model_file_dir=str(a.model_dir),sample_rate=44100)
        separator.load_model(MODEL);engine=separator.model_instance
        if int(engine.model_data_cfgdict.audio.sample_rate)!=44100:
            raise ValueError('unexpected native rate')
        for item in pending:
            required=identities[item['id']]['frames']*2*4*2+a.minimum_free_gib*1024**3
            a.teacher_root.mkdir(parents=True,exist_ok=True)
            if shutil.disk_usage(a.teacher_root).free<required:raise OSError('native teacher disk reserve reached')
            started=time.monotonic();mixture,_=sf.read(item['stereo'],dtype='float32')
            native=resample_poly(mixture,147,160,axis=0).astype(np.float32)
            gain=min(1.0,.9/max(float(np.max(np.abs(native))),1e-12))
            # Bypass separate/write_audio and their independent normalization.
            result=engine.demix(mix=(native*gain).T,override_model_segment_size=False)
            native_vocal=np.asarray(result['Vocals']).T
            if native_vocal.shape!=native.shape:
                raise ValueError('teacher alignment mismatch')
            v,i,residual=restore_pair(mixture,native_vocal,gain)
            if residual>2e-6 or not np.isfinite(i).all():
                raise ValueError('teacher reconstruction failed')
            directory=a.teacher_root/item['id'];directory.mkdir(parents=True,exist_ok=True)
            paths=output_pair(directory)
            for path,samples in zip(paths,(v,i)):sf.write(path,samples,48000,subtype='FLOAT')
            roundtrip=resample_poly(native,160,147,axis=0)[:len(mixture)]
            record=dict(identity=identities[item['id']],common_gain=gain,
                reconstruction_max_error=residual,
                resampling_residual_rms=float(np.sqrt(np.mean((mixture-roundtrip)**2))),
                output_sha256={p.name:digest(p) for p in paths})
            (directory/'teacher-provenance.json').write_text(json.dumps(record,indent=2)+'\n')
            print(f'ready {item["id"]} seconds={time.monotonic()-started:.1f} residual={residual:.2g}',flush=True)
    a.teacher_root.mkdir(parents=True,exist_ok=True)
    summary=dict(schema=2,policy=POLICY,source_manifest=str(a.manifest.resolve()),
        items=[dict(id=i['id'],ready=valid_pair(a.teacher_root/i['id'],identities[i['id']])) for i in items])
    (a.teacher_root/'roformer-manifest.json').write_text(json.dumps(summary,indent=2)+'\n')
    return 0

if __name__=='__main__':raise SystemExit(main())
