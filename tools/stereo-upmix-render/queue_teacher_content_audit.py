#!/usr/bin/env python3
"""Bounded local diagnostic follow-up to the 50-song specialist review queue."""
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'build/reference-audio'

def main():
    plan=BASE/'v14-specialist-queue.json'
    if not plan.exists():raise ValueError('specialist queue plan missing')
    ids=json.loads(plan.read_text())['selected_ids']
    if len(ids)!=len(set(ids)) or not ids:raise ValueError('invalid plan')
    output=BASE/'v14-independent-full50'
    output.mkdir(exist_ok=False)
    state=dict(state='waiting-for-specialist-clips',selected_ids=ids,completed_ids=[],
        human_listening_performed=False,training_approved=False)
    def publish():
        state['updated']=time.time();tmp=output/'status.tmp'
        tmp.write_text(json.dumps(state,indent=2)+'\n');tmp.replace(output/'status.json')
    publish();deadline=time.monotonic()+12*3600
    while not all((BASE/'v14-specialist-audit'/i/'audit.json').exists() for i in ids):
        upstream=json.loads(plan.read_text())
        if upstream['state'] in ['disk-budget-blocked','existing-target-needs-inspection','base-timeout-needs-inspection','base-incomplete-needs-inspection','specialist-failed-needs-inspection']:
            state['state']='upstream-needs-inspection';publish();return 1
        if time.monotonic()>deadline:state['state']='timeout-needs-inspection';publish();return 1
        time.sleep(30)
    # Five clips per process bound in-memory audio views; do not retain all 50.
    for start in range(0,len(ids),5):
        group=ids[start:start+5];state['state']='analyzing-audio';state['current_ids']=group;publish()
        with (output/'run.log').open('a') as log:
            result=subprocess.run([sys.executable,str(ROOT/'tools/stereo-upmix-render/audit_teacher_audio_content.py'),
                '--output',str(output/f'batch-{start//5:02d}'),'--ids',*group],stdout=log,stderr=subprocess.STDOUT)
        if result.returncode:state['state']='failed-needs-inspection';publish();return result.returncode
        state['completed_ids'].extend(group);publish()
    state['state']='diagnostics-ready-not-human-validated';publish();return 0

if __name__=='__main__':raise SystemExit(main())
