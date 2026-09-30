#!/usr/bin/env python3
"""CPU-only current Track A replay on the exact inspected DEV40x60s harness.

No oracle injection, no GPU, no unit change and no new selection. Startup knobs
include the owner's actual -9dB rear-air setting (older screens used -12dB).
"""
import os
os.environ['CUDA_VISIBLE_DEVICES']='-1'
import argparse
import csv
import json
from pathlib import Path
import subprocess
import time
import numpy as np
import torch
import bsrnn_reference_eval as E

EXPECTED={
 'upmix_render_mask_v21a.exe':'cd1025fc080ed066f6c6174c66d39b9622ad95f1e5d3a7e095aab5c8b914236a',
 'hssr_model.bin':'96abe57d9ee703f2e39c0231de97580c303ce896ce044b7450c14240002b0a0e',
 'hssr_guard.bin':'ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae',
 'hssr_mask_tracka_v21a.bin':'4170a4131e1017d631d7e1faa5b1c017152fef8678b3f0fa54b4524f54f569d0',
}


def main(a):
    torch.set_num_threads(2)
    if torch.cuda.is_available():
        raise RuntimeError('This baseline must have no visible GPU')
    for name,h in EXPECTED.items():
        if E.ref.sha(a.tools/name)!=h:raise ValueError('Frozen Track A file changed: '+name)
    source=a.runs/'direct_object_full40/manifest.json'
    if E.ref.sha(source)!='c0407503233c3f5e0526f8b88a3617e87e962b3763b966276206abc979dafee4':
        raise ValueError('Frozen original DEV40 changed')
    m=json.loads(source.read_text())
    expected={(r['song'],r['stem']) for r in E.gate.read(a.runs/'direct_object_full40/baseline/stems.csv')}
    if len(m['song_track_ids'])!=40 or len(expected)!=231:raise ValueError('Coverage changed')
    knobs=E.T.VARIANTS['shipped']+['--rear-air','-9','--vocal-gate','.5',
               '--model',str(a.tools/'hssr_model.bin'),'--guard-model',str(a.tools/'hssr_guard.bin'),
               '--mask-model',str(a.tools/'hssr_mask_tracka_v21a.bin'),'--mask-drive-prod','1.5',
               '--mask-treble','0','--mask-gamma','1']
    protocol=dict(scope='current Track A CPU host replay; inspected DEV40 only; no live unit or independent test',
                  script_sha256=E.ref.sha(Path(__file__)),files=EXPECTED,
                  helpers={Path(x.__file__).name:E.ref.sha(Path(x.__file__)) for x in (E,E.ref,E.T,E.H,E.gate)},
                  source_manifest_sha256=E.ref.sha(source),expected_rows=231,knobs=knobs,
                  gain='0.7/dry mixture peak; same input gain and excerpts as SIMO/causal/unified comparisons',
                  audio_injection=False,scene='all three Track A models; synchronous copied host, not async device timing',
                  selection='all40, no new tuning; rear-air -9 from current startup, earlier production20 used -12')
    if a.resume:
        if json.loads((a.out/'protocol.json').read_text())!=protocol:raise ValueError('Resume protocol differs')
    else:
        a.out.mkdir(parents=True,exist_ok=False);E.write(a.out/'protocol.json',protocol)
    rows=[]
    for i,(song,track) in enumerate(m['song_track_ids'].items(),1):
        marker=a.out/(track+'.complete.json')
        pcm=a.out/(track+'.wav');dst=a.out/(track+'.6ch.wav')
        if a.resume and marker.exists():
            r=json.loads(marker.read_text())
            if E.ref.sha(pcm)!=r['mixture_sha256'] or E.ref.sha(dst)!=r['output_sha256']:raise ValueError('Resume PCM differs')
            own=r['rows']
        else:
            begin=time.monotonic()
            meta,stems=E.H.load_song(str(a.moisesdb/track),60)
            n=min(x.shape[-1] for x in stems.values());stems={k:v[:,:n] for k,v in stems.items()}
            mix=sum(stems.values());gain=.7/(float(np.abs(mix).max())+1e-9)
            E.T.write_wav(str(pcm),mix*gain)
            if dst.exists():dst.unlink()
            p=subprocess.run([str(a.tools/'upmix_render_mask_v21a.exe'),*knobs,str(pcm),str(dst)],capture_output=True,text=True)
            if p.returncode:raise RuntimeError(p.stderr)
            (a.out/(track+'.render.log')).write_text(p.stderr)
            output=E.T.read_wav6(str(dst))
            if output.shape!=(6,n):raise ValueError('Duration/channel coverage differs')
            own=E.rows_from_output(meta,stems,output)
            E.write(marker,dict(rows=own,mixture_sha256=E.ref.sha(pcm),output_sha256=E.ref.sha(dst),
                                gain=gain,seconds=time.monotonic()-begin))
        if {(r['song'],r['stem']) for r in own}!={k for k in expected if k[0]==song}:raise ValueError('Per-song source coverage differs')
        rows+=own
        print('TRACK_A_MATCHED40',i,40,track,flush=True)
    if {(r['song'],r['stem']) for r in rows}!=expected:raise ValueError('Terminal full coverage differs')
    with (a.out/'stems.csv').open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
    summary=E.gate.summary(rows)
    gates=E.gate.passes(summary,summary,'full40');gates.pop('rear_rms_within_half_db')
    E.write(a.out/'complete.json',dict(complete=True,protocol_sha256=E.ref.sha(a.out/'protocol.json'),
          stems_sha256=E.ref.sha(a.out/'stems.csv'),summary=summary,observed_render_gates=gates,
          limits='No attributed vocal-entry/exit, independent20, live timing, car listening or candidate comparison in this baseline'))
    print('TRACK_A_MATCHED40_COMPLETE',json.dumps(E.clean(summary)),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('tools','runs','moisesdb','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--resume',action='store_true');main(p.parse_args())
