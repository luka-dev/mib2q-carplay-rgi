#!/usr/bin/env python3
"""Continuous exported-C control validation; no recurrent resets inside a clip."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import numpy as np
import torch
from train_hssr import OUTPUTS, loss_components
from prepare_hssr_native_teacher import digest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--oracle',type=Path,required=True)
    p.add_argument('--cache-root',type=Path,required=True)
    p.add_argument('--models',type=Path,nargs='+',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--burn-in',type=int,default=64)
    p.add_argument('--control-delay',type=int,choices=[0,1],default=1)
    a=p.parse_args();torch.set_num_threads(2)
    manifest=json.loads((a.cache_root/'manifest.json').read_text())
    # Only unmodified mixtures. Augmented stems/counterfactuals remain training
    # diagnostics; all three input levels of each validation crop are retained.
    rows=[r for r in manifest['items'] if r['split']=='validation' and not r.get('counterfactual')]
    if not rows: raise ValueError('no validation mixtures')
    results=[]
    for model in a.models:
        for row in rows:
            with np.load(row['cache']) as cache:
                features=cache['features'];targets=cache['targets']
            with tempfile.TemporaryDirectory(prefix='hssr-continuous-') as tmp:
                d=Path(tmp);features.astype('<f4').tofile(d/'x')
                subprocess.run([str(a.oracle.resolve()),str(model.resolve()),'--features',str(d/'x'),str(d/'y')],check=True)
                raw=np.fromfile(d/'y',dtype='<f4').reshape(-1,OUTPUTS)
            if raw.shape!=targets.shape or not np.isfinite(raw).all():
                raise ValueError('bad C control output')
            end=len(raw)-a.control_delay
            prediction=torch.from_numpy(raw[a.burn_in:end])[None]
            target=torch.from_numpy(targets[a.burn_in+a.control_delay:])[None]
            values=loss_components(prediction,target,'selection-v11',True)
            result=dict(model=str(model),model_sha256=digest(model),id=row['id'],
                input_gain_db=row['input_gain_db'],source_sha256=row['source_sha256'],
                frames=len(raw),metrics={k:float(v) for k,v in values.items()})
            results.append(result)
        print(f'C continuous complete {model}: {len(rows)} mixtures',flush=True)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(dict(kind='continuous-exported-C-control',
        burn_in=a.burn_in,control_delay=a.control_delay,oracle_sha256=digest(a.oracle),
        cache_manifest_sha256=digest(a.cache_root/'manifest.json'),rows=results,
        limitations=['Pseudo-label control validation, not audible separation or cabin quality.',
                    'One fixed control-period lag, not live worker deadline jitter.']),indent=2)+'\n')
    return 0

if __name__=='__main__':raise SystemExit(main())
