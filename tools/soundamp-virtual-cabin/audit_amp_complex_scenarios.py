#!/usr/bin/env python3
"""Synthetic C4D17 scenarios, NOT a measurement or simulation of cabin noise.

Uses actual audited initial FIR coefficients and explicit synthetic graph11.
No vehicle access. Prints JSON; optional --output writes the same artifact.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from amp_complex_path import ComplexFullModeReference, load_calibrated_fir

ROOT=Path(__file__).resolve().parents[2]


def run(image, audit, profile_index=0, seconds=5):
    if seconds<1 or not np.isfinite(seconds):raise ValueError('at least one finite second required')
    a=json.loads(Path(audit).read_text())
    p=next(p for p in a['complex_control_profiles'] if p['profile_index']==profile_index)
    fir=load_calibrated_fir(image,p,a['source_sha256'])
    n=int(seconds*48000)//64*64
    t=np.arange(n)/48000
    # Inputs at C4D17, not MOST PCM. Do not compare these dB to music reports.
    music=np.zeros((n,10))
    music[:,0]=.01*np.sin(2*np.pi*63*t)+.01*np.sin(2*np.pi*1000*t)
    music[:,1]=.01*np.sin(2*np.pi*125*t)+.01*np.sin(2*np.pi*4000*t)
    result={'source_sha256':a['source_sha256'],'frames':n,'sample_rate':48000,
        'audit_sha256':hashlib.sha256(Path(audit).read_bytes()).hexdigest(),
        'reference_source_sha256':hashlib.sha256((Path(__file__).parent/'amp_complex_path.py').read_bytes()).hexdigest(),
        'reference_model_version':'c4d17-fixed-full-mode-reference-v3',
        'detector_sample_indices':[0,16,32,48],
        'supersedes':'v2 scenario results omitted stage12 stride16 compaction; identity-path music reports unaffected',
        'scope':'synthetic stage-boundary sensitivity test; NOT live state, cabin SPL or bass diagnosis',
        'music':'channels0/1: four .01-amplitude tones at 63/1000 and 125/4000 Hz; other channels zero',
        'power_offsets':'stored base tables ONLY; explicit zero runtime additive offsets',
        'scenarios':[]}
    for sideband,aux_gain in ((0,0),(0,.01),(0,.1),(1,.1)):
        c=ComplexFullModeReference(p,fir,adaptive_enable=1,sideband=sideband,
            power_x_db=p['power_x_base_db'],power_y_db=p['power_y_base_db'],
            power_floor_db=p['power_y_floor_base_db'],rounding='nearest')
        aux=aux_gain*np.sin(2*np.pi*63*t)
        output=np.empty_like(music);updates=[]
        for start in range(0,n,64):
            block=c.process_block(music[start:start+64],aux[start:start+64])
            output[start:start+64]=block['audio']
            if block['update'] is not None:
                u=block['update']
                updates.append({'at_frame':start+64,**{k:np.asarray(u[k]).tolist()
                                for k in ('endpoint','power_x','power_y')}})
        if not np.isfinite(output).all():raise ArithmeticError('nonfinite output')
        result['scenarios'].append({'model':c.metadata(),'auxiliary_63Hz_amplitude':aux_gain,
            'output_peak':float(abs(output).max()),'update_count':len(updates),
            'final_target_db':(20*np.log10(c.control.endpoint)).tolist(),
            'fir_changed':bool(np.any(c.residual.coefficients!=fir)),
            'updates':updates})
    return result


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('image',type=Path)
    ap.add_argument('--audit',type=Path,default=ROOT/'output/r2/soundamp/remaining-blocks-20260919/audit.json')
    ap.add_argument('--profile',type=int,choices=(0,1),default=0)
    ap.add_argument('--seconds',type=float,default=5)
    ap.add_argument('--output',type=Path)
    args=ap.parse_args()
    report=run(args.image,args.audit,args.profile,args.seconds)
    encoded=json.dumps(report,indent=2,allow_nan=False)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(encoded)
        print(json.dumps({'output':str(args.output),'scenarios':[
            {k:s[k] for k in ('auxiliary_63Hz_amplitude','output_peak','update_count','final_target_db','fir_changed')}
            | {'sideband':s['model']['sideband']} for s in report['scenarios']]},indent=2))
    else:print(encoded,end='')
