#!/usr/bin/env python3
"""After the full base teacher, audit one non-silent mid-song core per source.

Bounded first pass, not full-corpus semantic validation. Never starts training.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from teacher_audit_policy import coverage_report

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'build/reference-audio'

def main():
    manifest=BASE/'v14-full-source/manifest.json'
    data=json.loads(manifest.read_text());coverage=coverage_report(data)
    groups={}
    for row in data['items']:groups.setdefault(row['source_sha256'],[]).append(row)
    selected=[]
    for rows in groups.values():
        candidates=[r for r in rows if not r['silence_candidate']] or rows
        selected.append(min(candidates,key=lambda r:abs(r['core_start_seconds']+r['core_frames']/96000-r['source_duration_seconds']/2)))
    status=BASE/'v14-specialist-queue.json'
    def publish(state,**extra):
        record=dict(state=state,updated=time.time(),coverage=coverage,
            selected_ids=[r['id'] for r in selected],policy='one-mid-song-core-per-source-review-only',
            all_specialist_clips_validated=False,training_started=False,**extra)
        temporary=status.with_suffix('.tmp');temporary.write_text(json.dumps(record,indent=2)+'\n');temporary.replace(status)
    publish('waiting-for-full-base-teacher')
    deadline=time.monotonic()+12*3600
    base=BASE/'v14-full-teacher';summary=base/'roformer-manifest.json'
    while not summary.exists():
        if time.monotonic()>deadline:publish('base-timeout-needs-inspection');return 1
        time.sleep(30)
    ready=json.loads(summary.read_text())['items']
    if len(ready)!=len(data['items']) or not all(r['ready'] for r in ready):
        publish('base-incomplete-needs-inspection');return 1
    output=BASE/'v14-specialist-audit'
    if any((output/r['id']).exists() for r in selected):
        publish('existing-target-needs-inspection');return 1
    estimate=sum(r['frames'] for r in selected)*2*4*15
    free=shutil.disk_usage(BASE).free
    if free<estimate+15*1024**3:
        publish('disk-budget-blocked',required_bytes=estimate+15*1024**3,free_bytes=free);return 1
    publish('running-specialists',estimated_output_bytes=estimate)
    env=dict(os.environ,PYTORCH_ENABLE_MPS_FALLBACK='1')
    with (BASE/'v14-specialist-queue.log').open('a') as log:
        result=subprocess.run([sys.executable,str(ROOT/'tools/stereo-upmix-render/prepare_specialist_teacher_audit.py'),
            '--manifest',str(manifest),'--teacher-root',str(base),'--output',str(output),
            '--model-dir',str(ROOT/'build/hssr-teacher-models'),'--ids',*[r['id'] for r in selected]],
            env=env,stdout=log,stderr=subprocess.STDOUT)
    publish('ready-for-human-review' if result.returncode==0 else 'specialist-failed-needs-inspection',exit_code=result.returncode)
    return result.returncode

if __name__=='__main__':raise SystemExit(main())
