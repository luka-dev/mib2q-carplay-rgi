#!/usr/bin/env python3
"""Local native-teacher/QAT pilot. No vehicle access or automatic promotion."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--wait-existing-teacher',action='store_true')
    a=p.parse_args();out=a.output.resolve()
    if out.exists():raise ValueError('use a new candidate directory')
    out.mkdir(parents=True)
    source=ROOT/'build/reference-audio/v13-training-source/manifest.json'
    teacher=ROOT/'build/reference-audio/v13-expanded-teacher'
    cache=out/'cache';status=out/'pipeline-status.json'
    def phase(name,**extra):
        status.write_text(json.dumps(dict(phase=name,installed=False,new_model_selected=False,**extra),indent=2)+'\n')
        print(name,flush=True)
    def run(tool,*args):
        subprocess.run([sys.executable,str(HERE/tool),*map(str,args)],cwd=ROOT,check=True)
    try:
        if a.wait_existing_teacher:
            phase('waiting-for-existing-native-teacher')
            deadline=time.monotonic()+7200
            expected={i['id'] for i in json.loads(source.read_text())['items']}
            while True:
                summary=teacher/'roformer-manifest.json'
                if summary.exists():
                    items=json.loads(summary.read_text())['items']
                    if {i['id'] for i in items if i['ready']}==expected:break
                if time.monotonic()>deadline:raise TimeoutError('native teacher did not finish')
                time.sleep(15)
        phase('verifying-native-teacher')
        run('prepare_hssr_native_teacher.py','--manifest',source,'--teacher-root',teacher,
            '--model-dir',ROOT/'build/hssr-teacher-models')
        phase('building-provenance-bound-caches')
        run('prepare_hssr_separator_supervision.py','--manifest',source,'--teacher-root',teacher,
            '--output',cache,'--require-native-teacher','--target-policy','selection-v11',
            '--include-vocal-negatives','--include-instrumental-positives','--counterfactual-mixes',
            '--input-gains-db',0,-6,-12)
        phase('training-routing-head-pilot')
        baseline=ROOT/'models/hssr-selection-v11/hssr_model.bin'
        run('train_hssr.py','--cache-only','--extra-cache-root',cache,'--init-model',baseline,
            '--output',out,'--runtime-aware','--routing-head-only','--sequence',192,'--burn-in',64,
            '--control-delay',1,'--device','cpu','--cpu-threads',2,'--loss-profile','selection-v11',
            '--epochs',4,'--max-train-batches',128,'--batch-size',32,'--learning-rate',.00001)
        phase('continuous-exported-C-validation')
        oracle=out/'hssr_validate'
        subprocess.run(['cc','-std=c99','-O2','-Ihook/audio',str(HERE/'bench/hssr_validate.c'),
            'hook/audio/hssr_dsp.c','hook/audio/hssr_neon.c','-lm','-o',str(oracle)],cwd=ROOT,check=True)
        run('evaluate_hssr_continuous_c.py','--oracle',oracle,'--cache-root',cache,
            '--models',baseline,out/'hssr_model.bin','--output',out/'continuous-c-validation.json')
        phase('pilot-complete-needs-render-attribution-and-listening-review')
    except Exception as exc:
        phase('failed',error=str(exc));raise
    return 0

if __name__=='__main__':raise SystemExit(main())
