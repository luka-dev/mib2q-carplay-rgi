#!/usr/bin/env python3
"""Own the full FLOAT integrity/base/specialist/audit workflow to terminal report.

No training or public uploads. Logs go to files, not an undrained interactive pipe.
"""
import argparse
import fcntl
import json
import os
import signal
import shutil
from pathlib import Path
import subprocess
import sys
import time
from run_v14_complete_audit import atomic,BASE,HERE,ROOT
from disk_budget_guard import DiskBudgetGuard

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--resume',action='store_true')
    p.add_argument('--preserve-all-audio',action='store_true')
    p.add_argument('--pause-free-gib',type=float,default=22)
    p.add_argument('--resume-free-gib',type=float,default=27)
    a=p.parse_args();guard=DiskBudgetGuard(a.pause_free_gib,a.resume_free_gib)
    out=BASE/'v15-workflow';out.mkdir(exist_ok=True)
    lock=(out/'workflow.lock').open('a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    state=dict(state='waiting-float-source',pid=os.getpid(),training_started=False)
    def interrupted(signum,frame):raise RuntimeError(f'workflow interrupted by signal {signum}')
    signal.signal(signal.SIGTERM,interrupted)
    def phase(name,**extra):state.update(state=name,updated=time.time(),**extra);atomic(out/'status.json',state)
    env=dict(os.environ,PYTORCH_ENABLE_MPS_FALLBACK='1',HF_HUB_DISABLE_PROGRESS_BARS='1')
    manifest=BASE/'v15-full-float-source/manifest.json';integrity=manifest.parent/'integrity-audit.json'
    teacher=None;audit=None
    def signal_workers(sig):
        for child in (teacher,audit):
            if child is not None and child.poll() is None:
                try:
                    if os.getpgid(child.pid)!=child.pid:raise RuntimeError('worker process group changed')
                    os.killpg(child.pid,sig)
                except ProcessLookupError:pass
    def disk_check():
        free=shutil.disk_usage(out).free/1024**3
        transition=guard.observe(free)
        if transition=='pause':signal_workers(signal.SIGSTOP)
        elif transition=='resume':signal_workers(signal.SIGCONT)
        disk=dict(state='paused-low-disk' if guard.paused else 'monitoring',updated=time.time(),
            free_gib=free,pause_free_gib=guard.pause_gib,resume_free_gib=guard.resume_gib,
            interval_seconds=5,automatic_deletion=False,preserve_all_new_audio=a.preserve_all_audio)
        atomic(out/'disk-status.json',disk)
        return free
    with (out/'integrity.log').open('a') as ilog,(out/'teacher.log').open('a') as tlog,(out/'audit.log').open('a') as alog:
        try:
            phase('waiting-float-source');deadline=time.monotonic()+1800
            while not manifest.exists():
                if time.monotonic()>deadline:raise TimeoutError('FLOAT source manifest missing')
                time.sleep(10)
            if not integrity.exists():
                phase('verifying-all-float-sources')
                subprocess.run([sys.executable,str(HERE/'audit_full_source_integrity.py'),'--manifest',str(manifest),
                    '--output',str(integrity)],env=env,stdout=ilog,stderr=subprocess.STDOUT,check=True)
            check=json.loads(integrity.read_text())
            if not check['technical_pass'] or not check['integer_clipping_avoided']:raise ValueError('FLOAT source integrity failed')
            while True:
                free=disk_check()
                if not guard.paused:break
                phase('waiting-disk-before-launch',free_gib=free)
                time.sleep(5)
            teacher=subprocess.Popen([sys.executable,str(HERE/'prepare_hssr_native_teacher.py'),
                '--manifest',str(manifest),'--teacher-root',str(BASE/'v15-full-float-teacher'),
                '--model-dir',str(ROOT/'build/hssr-teacher-models')],env=env,stdout=tlog,stderr=subprocess.STDOUT,start_new_session=True)
            audit=subprocess.Popen([sys.executable,str(HERE/'run_v14_complete_audit.py'),*(['--resume'] if a.resume else []),
                *(['--preserve-all-audio'] if a.preserve_all_audio else [])],
                env=env,stdout=alog,stderr=subprocess.STDOUT,start_new_session=True)
            phase('running-all-587-clips',teacher_pid=teacher.pid,audit_pid=audit.pid)
            while True:
                tc=teacher.poll();ac=audit.poll()
                if tc is not None and tc!=0:raise RuntimeError(f'base teacher failed: {tc}; inspect teacher.log')
                if ac is not None and ac!=0:raise RuntimeError(f'full audit failed: {ac}; inspect audit.log')
                if tc==0 and ac==0:break
                free=disk_check()
                phase('paused-low-disk' if guard.paused else 'running-all-587-clips',free_gib=free,
                    preserve_all_new_audio=a.preserve_all_audio)
                time.sleep(5)
            report=json.loads((BASE/'v15-complete-audit/dataset-report.json').read_text())
            if not report['technical_audit_complete'] or report['audited']!=587:raise ValueError('incomplete final audit')
            phase('complete-needs-semantic-review',report=str(BASE/'v15-complete-audit/dataset-report.md'))
            atomic(out/'disk-status.json',dict(state='complete',updated=time.time(),automatic_deletion=False))
            return 0
        except Exception as exc:
            signal_workers(signal.SIGTERM)
            signal_workers(signal.SIGCONT)
            phase('failed-needs-inspection',error=str(exc));raise

if __name__=='__main__':raise SystemExit(main())
