#!/usr/bin/env python3
"""Exact labelled eligible audio through the frozen NN-free C host, CPU only.

This bounds renderer/harness behavior, not causal extractability or audible
bleed in labels. Keep original full40, gain points, source coverage and errors.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES'] = '-1'
import argparse
import csv
import gc
import json
from pathlib import Path
import time
import torch
import eval_unified_upmix_generalization as C
E, B = C.E, C.B


def main(a):
    if a.out.exists():
        raise FileExistsError('Preserve previous oracle run')
    B.T.configure('cpu'); torch.set_num_threads(2)
    executable=a.tools/'unified_audio_host_v1/upmix_unified_audio.exe'
    if B.T.F.sha(executable)!=C.C_SHA:
        raise ValueError('Frozen NN-free C differs')
    declared=json.loads((a.runs/'direct_object_full40/manifest.json').read_bytes())
    expected={(r['song'],r['stem']) for r in E.gate.read(a.runs/'direct_object_full40/baseline/stems.csv')}
    if len(declared['song_track_ids'])!=40 or len(expected)!=231:
        raise ValueError('Original40/231 coverage required')
    a.out.mkdir(parents=True)
    protocol=dict(script_sha256=B.T.F.sha(Path(__file__)),c_sha256=C.C_SHA,points=C.POINTS,
                  helper_sha256=B.T.F.sha(Path(C.__file__)),nominal='nominal_040',
                  reference='exact sum of labelled raw eligible stems; no network or inferred mask',
                  input='same60s raw cuts,.7/dry_peak,eligible injection unity,no clipping/gain rescue',
                  renderer='same copied NN-free direct/body0,frontret.5,frontbass-3,air-9,attackoff,LFEgain0',
                  scope='inspectedDEV40 CPU renderer/harness capability bound only; no target fitting, model/GPU change or reserved20',
                  interpretation='A protected proxy response on this exact labelled eligible rear is not literal forbidden source content. Per-song waveform hashes retained. No gate adjustment or owner-ear claim.')
    E.write(a.out/'protocol.json',protocol);sets={name:[] for name in C.POINTS}
    for number,(song,track) in enumerate(declared['song_track_ids'].items(),1):
        started=time.monotonic();meta,stems=E.H.load_song(str(a.moisesdb/track),60)
        count=min(s.shape[-1] for s in stems.values());stems={k:v[:,:count] for k,v in stems.items()}
        mix=torch.from_numpy(sum(stems.values()));gain=.7/(float(mix.abs().max())+1e-9)
        eligible=E.roles_from_stems(stems)[0]*gain;mix*=gain
        own={};hashes={}
        for name,surround in C.POINTS.items():
            folder=a.out/name;folder.mkdir(exist_ok=True)
            rows,h=C.render(folder,track,meta,stems,mix,eligible,executable,surround)
            if {(r['song'],r['stem']) for r in rows}!={r for r in expected if r[0]==song}:
                raise ValueError('Oracle per-song stem coverage differs')
            sets[name]+=rows;own[name]=rows;hashes[name]=h
        E.write(a.out/(track+'.complete.json'),dict(song=song,rows=own,hashes=hashes,eligible_peak=float(eligible.abs().max())))
        print('UNIFIED_RENDERER_ORACLE',number,40,'seconds',time.monotonic()-started,flush=True)
        del stems,mix,eligible
        gc.collect()
    variants={}
    for name,rows in sets.items():
        if len(rows)!=231 or {(r['song'],r['stem']) for r in rows}!=expected:
            raise ValueError('Oracle full231 coverage differs')
        with (a.out/name/'stems.csv').open('w',encoding='utf-8',newline='') as f:
            writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
        summary=E.gate.summary([{k:float('nan') if v is None else v for k,v in r.items()} for r in rows])
        gates=E.gate.passes(summary,summary,'full40');gates.pop('rear_rms_within_half_db',None)
        variants[name]=dict(summary=summary,gates=gates,observed_render_gates_pass=all(gates.values()))
    E.write(a.out/'complete.json',dict(complete=True,protocol_sha256=B.T.F.sha(a.out/'protocol.json'),variants=variants,
                                     limits=protocol['scope']+'; '+protocol['interpretation']))
    print('UNIFIED_RENDERER_ORACLE_COMPLETE',json.dumps({k:dict(shares=v['summary']['shares'],swings=len(v['summary']['swings']),gates=v['gates']) for k,v in variants.items()}),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('tools','runs','moisesdb','out'):
        p.add_argument('--'+name,type=Path,required=True)
    main(p.parse_args())
