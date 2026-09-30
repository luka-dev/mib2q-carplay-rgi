#!/usr/bin/env python3
"""Package saved TRAIN-only C inputs/outputs for independent ARM replay."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main(a):
    if a.out.exists():raise FileExistsError('Preserve previous fixture')
    baseline=json.loads((a.reference/'complete.json').read_bytes())
    if not baseline['float_verified']:raise ValueError('Verified saved reference required')
    fixtures=['input.spectra','float.output','int8.output'];files=[(a.reference/n).open('rb') for n in fixtures]
    try:
        h=[struct.unpack('<8sIII',f.read(20)) for f in files]
        b,t=h[0][1:3]
        if h[0]!=(b'UMXSP001',b,t,4100) or any(x!=(b'UMXOT001',b,t,17552) for x in h[1:]) or b!=4 or t<a.frames:raise ValueError('Original fixture layout differs')
        for mode in ('float','int8'):
            if sha(a.reference/(mode+'.output'))!=baseline['arms'][mode]['output_sha256']:raise ValueError('Saved reference changed')
        a.out.mkdir(parents=True)
        fixture=a.out/'arm_fixture.bin'
        with fixture.open('wb') as dst:
            dst.write(struct.pack('<8sII',b'UMXQA001',b,a.frames))
            for stream in range(b):
                for frame in range(a.frames):
                    for f,x in zip(files,h):
                        count=x[3]*4;f.seek(20+(stream*t+frame)*count);data=f.read(count)
                        if len(data)!=count:raise ValueError('Truncated saved record')
                        dst.write(data)
        protocol=dict(scope='first64 or fewer actual TRAIN frames per stream, saved Windows C float/int8 outputs; scalar/NEON/state numerical execution, not acoustic validation or device timing',
            frames=a.frames,streams=b,fixture_sha256=sha(fixture),fixture_bytes=fixture.stat().st_size,
            reference_sha256=sha(a.reference/'complete.json'),inputs_sha256=sha(a.reference/'input.spectra'),
            source_estimate_nse_gates=dict(float=1e-7,int8=1e-4),scene_max_abs_gates=dict(float=1e-4,int8=.05),dot_exact_cases=24,
            stream_order='ARM frame-first interleaving versus Windows stream-first; all state caller-owned',
            selection='prefix length fixed before ARM execution; four already inspected fitting crops, no DEV or candidate selection')
        (a.out/'manifest.json').write_text(json.dumps(protocol,indent=2)+'\n')
        print('ARM_TRAIN_FIXTURE_READY',json.dumps(protocol),flush=True)
    finally:
        for f in files:f.close()


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--frames',type=int,choices=range(1,65),default=64);main(p.parse_args())
