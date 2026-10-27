#!/usr/bin/env python3
"""Compare explicit common-input scenarios on existing authored PCM6 clips.

B1/B2=0 and C4D17=identity hypotheses; no fresh upmix or vehicle configuration.
The first supplied scenario is the comparison reference, never labelled stock.
"""
import argparse
import json
from pathlib import Path
import numpy as np
import soundfile as sf
from amp_dsp_chain import AmpChain, SR
from amp_music_report import BANDS, GROUPS, band_powers, measure, fingerprint, load_upstream_scenario


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--scenario',type=Path,action='append',required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--start',type=float,default=2)
    p.add_argument('--end',type=float,default=10)
    p.add_argument('--seconds',type=float,default=12)
    p.add_argument('pcm6',nargs='+',type=Path)
    a=p.parse_args()
    if not all(np.isfinite(v) for v in (a.start,a.end,a.seconds)) or not 0<=a.start<a.end<a.seconds:
        p.error('require finite 0<=start<end<seconds')
    chains=[AmpChain(upstream_scenario=load_upstream_scenario(s)) for s in a.scenario]
    names=[s.stem for s in a.scenario]
    if len(set(names))!=len(names):p.error('scenario names must be unique')
    result={'scope':__doc__,'bands':BANDS,'window_seconds':[a.start,a.end],
            'level_reference':'bands_re_dry_L uses authored PCM6 channel0, not an original dry stereo source',
            'maximum_source_seconds':a.seconds,'reference_scenario':names[0],
            'scenarios':{n:dict(file=fingerprint(s),model=c.metadata()) for n,s,c in zip(names,a.scenario,chains)},
            'tracks':[]}
    start,end=round(a.start*SR),round(a.end*SR)
    for path in a.pcm6:
        x,sr=sf.read(path,frames=round(a.seconds*SR),dtype='float64',always_2d=True)
        if sr!=SR or x.shape[1]!=6 or not np.isfinite(x).all():raise ValueError('finite 48kHz PCM6 required')
        if len(x)<end:raise ValueError('track shorter than analysis window')
        power=band_powers(x[start:end,0])
        rows={name:measure(c,x,power,start,end,0) for name,c in zip(names,chains)}
        for name,row in rows.items():
            row['delta_vs_reference_scenario']={group:(np.array(row['groups'][group]['power_dbfs'])-
                rows[names[0]]['groups'][group]['power_dbfs']).tolist() for group in GROUPS}
        result['tracks'].append({'file':fingerprint(path),'frames_rendered':len(x),'results':rows})
        print(path.name,'done',flush=True)
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(json.dumps(result,indent=2)+'\n')
    print('Conditional scenario report:',a.out)


if __name__=='__main__':main()
