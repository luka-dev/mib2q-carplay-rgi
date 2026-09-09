#!/usr/bin/env python3
"""Actual C render regression; digital metrics, not seat/acoustic predictions."""
import argparse
import json
from pathlib import Path
import subprocess
import numpy as np
import soundfile as sf
from evaluate_float_candidate import score,fingerprint,cabin

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--renderer',type=Path,required=True)
    p.add_argument('--model',type=Path,required=True)
    p.add_argument('--manifest',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--legacy-reference',type=Path)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    matrix=cabin.linear_matrix(cabin.load_config(cabin.DEFAULT_CONFIG))
    variants={'v11-float':['--float-render','--lfe','assist'],
        'quiet-bass-only':['--quiet-lfe','--lfe','assist'],
        'quiet-plus1':['--quiet-lfe','--lfe','assist','--soundcheck-target','-16','--soundcheck-params','-18','-1.8125','-1.78125'],
        'quiet-minus9':['--quiet-lfe','--lfe','assist','--soundcheck-target','-16','--soundcheck-params','-6.5','-1','.96875'],
        'cabin-assist':['--cabin-render','--lfe','assist'],
        'cabin-no-assist':['--cabin-render','--lfe','off'],
        'cabin-plus1':['--cabin-render','--lfe','assist','--soundcheck-target','-16','--soundcheck-params','-18','-1.8125','-1.78125'],
        'cabin-minus9':['--cabin-render','--lfe','assist','--soundcheck-target','-16','--soundcheck-params','-6.5','-1','.96875']}
    rows=[];compatibility=[]
    for item in json.loads(a.manifest.read_text())['items']:
        if a.legacy_reference:
            outputs=[]
            for label,renderer in [('current',a.renderer),('reference',a.legacy_reference)]:
                target=a.output/item['id']/f'legacy-{label}.wav';target.parent.mkdir(exist_ok=True)
                subprocess.run([str(renderer.resolve()),'--mode','hssr-universal','--model',str(a.model.resolve()),
                    '--surround','.25','--lfe','assist',item['stereo'],str(target)],check=True,capture_output=True)
                outputs.append(fingerprint(target))
            if outputs[0]!=outputs[1]:raise RuntimeError(f'legacy regression: {item["id"]}')
            compatibility.append(dict(id=item['id'],byte_exact=True,output_sha256=outputs[0]))
        for name,flags in variants.items():
            target=a.output/item['id']/f'{name}.wav';target.parent.mkdir(exist_ok=True)
            run=subprocess.run([str(a.renderer.resolve()),'--mode','hssr-universal','--model',str(a.model.resolve()),'--surround','.25',*flags,item['stereo'],str(target)],check=True,capture_output=True,text=True)
            metrics=score(target,matrix);audio,rate=sf.read(target,dtype='float64')
            if not np.isfinite(audio).all():raise RuntimeError('nonfinite PCM')
            spectral=np.abs(np.fft.rfft(audio,axis=0))**2
            f=np.fft.rfftfreq(len(audio),1/rate)
            energy=np.mean(audio**2,axis=0)
            front_body=float(np.sum(spectral[(f>=250)&(f<4000),:2]))
            rear_body=float(np.sum(spectral[(f>=250)&(f<4000),4:6]))
            if any(ch['preclip_samples'] for ch in metrics['input_matrix_buses']['channels'].values()):
                raise RuntimeError('known matrix overload')
            rows.append(dict(id=item['id'],variant=name,energy=energy.tolist(),
                front_body=front_body,rear_body=rear_body,diagnostics=run.stderr,**metrics))
        print(item['id'],flush=True)
    groups={}
    for name in variants:
        rs=[r for r in rows if r['variant']==name]
        groups[name]=dict(mean_channel_energy=np.mean([r['energy'] for r in rs],axis=0).tolist(),
            summed_front_body=sum(r['front_body'] for r in rs),summed_rear_body=sum(r['rear_body'] for r in rs))
    report=dict(kind='cabin-v13-digital-render',model_sha256=fingerprint(a.model),
        renderer_sha256=fingerprint(a.renderer),groups=groups,rows=rows,
        source_manifest_sha256=fingerprint(a.manifest),legacy_compatibility=compatibility,
        cabin_config_sha256=fingerprint(cabin.DEFAULT_CONFIG),
        legacy_reference_sha256=fingerprint(a.legacy_reference) if a.legacy_reference else None,
        limitations=['No physical cabin response; no source-separation quality claim from channel energy.',
                    'Existing seven-song set is a development regression, not fresh blind test.'])
    (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(groups,indent=2))

if __name__=='__main__':main()
