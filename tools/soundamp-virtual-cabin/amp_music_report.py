#!/usr/bin/env python3
"""Stereo -> C upmix -> known J525 electronics; conditional electrical levels.

The dry reference is L/R-only PCM6 in BANK 1, never stock stereo/bank 0.
Variant: name:surround:front_bass_db:lfe_gain_db. Reports retain model
assumptions; group powers are arithmetic means, not an acoustic speaker sum.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

import numpy as np
import soundfile as sf
from scipy.signal import welch

from amp_dsp_chain import AmpChain, SR

BANDS = [(20,60),(60,120),(120,250),(250,500),(500,2000),(2000,8000),(8000,20000)]
GROUPS = {'front mid/treble':[0,1], 'rear doors':[8,9], 'centre mid':[10],
          'centre tweeter':[11], 'front woofers':[12,13], 'subwoofer':[14,15]}
UPMIX_LATENCY = 512


def db(power):
    return 10*np.log10(np.maximum(power,1e-30))


def band_powers(samples):
    """Linear mean-square band powers, Welch PSD integrated over frequency."""
    samples = np.asarray(samples,dtype='f8')
    if len(samples) < 2:
        raise ValueError('analysis window needs at least two samples')
    n = min(32768,len(samples))
    f,p = welch(samples,SR,nperseg=n,noverlap=n//2,detrend=False,axis=0,scaling='density')
    return np.array([p[(f>=lo)&(f<hi)].sum(axis=0)*(f[1]-f[0]) for lo,hi in BANDS])


def read_audio(path, channels):
    x,sr = sf.read(path,dtype='float64',always_2d=True)
    if sr != SR or x.shape[1] != channels or not np.isfinite(x).all():
        raise ValueError(f'{path}: expected finite {SR} Hz/{channels}-channel audio')
    return x


def measure(chain, pcm6, reference_power, start, end, source_delay):
    # Reset at clip start, not the crop boundary; retain only final output.
    out = chain.process(pcm6,capture_stages=False)['out']
    return measure_output(chain, out, reference_power, start, end, source_delay)


def measure_output(chain, out, reference_power, start, end, source_delay):
    """Measure already-rendered config-order output; preserve normal alignment."""
    out = np.asarray(out, dtype=float)
    if out.ndim != 2 or out.shape[1] != 16 or not np.isfinite(out).all():
        raise ValueError('finite frames x16 output required')
    powers = []; peaks = []
    for channel, delay in enumerate(chain.total_a_delay):
        lo,hi = start+source_delay+delay,end+source_delay+delay
        if hi > len(out):
            raise ValueError('clip is too short for the window plus model/upmix delay')
        window = out[lo:hi,channel]
        powers.append(band_powers(window))
        peaks.append(float(20*np.log10(max(np.max(np.abs(window)),1e-15))))
    powers = np.asarray(powers)
    return {'peak_out_dbfs':peaks,
            'bands_re_dry_L':(db(powers)-db(reference_power)).tolist(),
            'groups':{group:{'power_dbfs':db(powers[idx].mean(axis=0)).tolist(),
                              'bands_re_dry_L':(db(powers[idx].mean(axis=0))-db(reference_power)).tolist(),
                              'peak_out_dbfs':max(peaks[o] for o in idx)}
                      for group,idx in GROUPS.items()}}


def parse_variant(value):
    fields=value.split(':')
    if len(fields)!=4 or not re.fullmatch(r'[A-Za-z0-9_-]+',fields[0]) or fields[0]=='dry':
        raise ValueError('variant must be name:surround:front_bass_db:lfe_gain_db; name dry is reserved')
    surround,front_bass,lfe=map(float,fields[1:])
    if not (0<=surround<=.5 and -12<=front_bass<=0 and -24<=lfe<=12):
        raise ValueError('variant bounds: surround 0..0.5, front bass -12..0, LFE -24..12')
    return fields[0],surround,front_bass,lfe


def fingerprint(path):
    return {'path':str(path),'sha256':hashlib.sha256(Path(path).read_bytes()).hexdigest()}


def load_upstream_scenario(path):
    """Relative audit paths resolve against the scenario file, not shell cwd."""
    path=Path(path).resolve()
    scenario=json.loads(path.read_text())
    audit_path=Path(scenario['audit_path'])
    if not audit_path.is_absolute():scenario['audit_path']=str(path.parent/audit_path)
    return scenario


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--renderer');ap.add_argument('--model');ap.add_argument('--guard')
    ap.add_argument('--rendered-dir',type=Path,help='reuse NAME-VARIANT.wav; settings are labels, not re-rendered')
    ap.add_argument('--upstream-scenario',type=Path,
                    help='explicit settled common input controls and physical mapping hypothesis as JSON; B remains zero in this CLI')
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--variant',action='append',default=[])
    ap.add_argument('--start',type=float,default=5);ap.add_argument('--end',type=float,default=55)
    ap.add_argument('wavs',nargs='+',type=Path)
    a=ap.parse_args()
    try:
        variants=[parse_variant(v) for v in a.variant]
    except ValueError as error:
        ap.error(str(error))
    if len({v[0] for v in variants}) != len(variants):
        ap.error('variant names must be unique')
    if len({p.stem for p in a.wavs}) != len(a.wavs):
        ap.error('input stems must be unique')
    if not (np.isfinite(a.start) and np.isfinite(a.end) and 0<=a.start<a.end):
        ap.error('require finite 0 <= start < end')
    if variants and not a.rendered_dir and not all((a.renderer,a.model,a.guard)):
        ap.error('rendering requires --renderer, --model and --guard')
    a.out.mkdir(parents=True,exist_ok=True)
    chain=AmpChain(upstream_scenario=None if a.upstream_scenario is None else load_upstream_scenario(a.upstream_scenario))
    results={};files={};songs=[]
    start,end=round(a.start*SR),round(a.end*SR)
    for wav in a.wavs:
        x=read_audio(wav,2)
        if end>len(x):
            raise ValueError(f'{wav}: shorter than requested source window')
        ref=band_powers(x[start:end,0])
        dry=np.zeros((len(x),6));dry[:,:2]=x
        key=f'{wav.stem}:dry'
        results[key]=measure(chain,dry,ref,start,end,0)
        files[key]=fingerprint(wav)
        del dry
        for name,surround,fb,lfe in variants:
            rendered=(a.rendered_dir or a.out)/f'{wav.stem}-{name}.wav'
            if not a.rendered_dir:
                if rendered.resolve()==wav.resolve():
                    raise ValueError('refusing to overwrite input')
                cmd=[a.renderer,'--mode','hssr-universal','--model',a.model,'--guard-model',a.guard,
                     '--lfe','assist','--surround',str(surround),'--front-bass',str(fb),'--lfe-gain',str(lfe),
                     str(wav),str(rendered)]
                run=subprocess.run(cmd,capture_output=True,text=True,check=True)
                rendered.with_suffix('.log').write_text(run.stderr)
            pcm6=read_audio(rendered,6)
            key=f'{wav.stem}:{name}'
            results[key]=measure(chain,pcm6,ref,start,end,UPMIX_LATENCY)
            files[key]=fingerprint(rendered)
            del pcm6
        songs.append(wav.stem)
        print(wav.stem,'done',flush=True)
    names=['dry']+[v[0] for v in variants]
    summary={}
    for group in GROUPS:
        summary[group]={}
        for name in names:
            rows=[results[f'{song}:{name}']['groups'][group] for song in songs]
            baseline=[results[f'{song}:dry']['groups'][group] for song in songs]
            summary[group][name]={
                'mean_bands_re_dry_L':np.mean([r['bands_re_dry_L'] for r in rows],axis=0).tolist(),
                'mean_delta_vs_dry_bank1':np.mean([np.array(r['power_dbfs'])-b['power_dbfs'] for r,b in zip(rows,baseline)],axis=0).tolist(),
                'max_model_peak_dbfs':max(r['peak_out_dbfs'] for r in rows)}
    report={'model':chain.metadata(),'upstream_scenario_file':None if a.upstream_scenario is None else fingerprint(a.upstream_scenario),
            'bands':BANDS,'group_channels':GROUPS,'results':results,'summary':summary,
            'analysis':{'reference':'dry L/R-only PCM6 BANK1, not stock stereo',
                        'window_source_seconds':[a.start,a.end], 'upmix_latency_frames':UPMIX_LATENCY,
                        'alignment':'per-output known A-path delay plus upmix latency',
                        'power':'Welch 32768/50%; linear mean within groups; mean paired dB across tracks',
                        'peaks':'model sample peaks relative to input full scale, not hardware overload verdict',
                        'render_mode':'existing WAVs; variant settings are labels' if a.rendered_dir else 'C renderer, S16 path, no Sound Check gain'},
            'variants':variants,'files':files}
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('\nMean band dB re dry input L:',BANDS)
    for group,rows in summary.items():
        for name,r in rows.items():
            print(f'{group:18s} {name:18s} peak {r["max_model_peak_dbfs"]:6.1f} | '+
                  ' '.join(f'{v:6.1f}' for v in r['mean_bands_re_dry_L']))
    print('\nConditional electrical model; no hardware clipping or acoustic verdict.\n'+'\n'.join(chain.warnings))


if __name__=='__main__':
    main()
