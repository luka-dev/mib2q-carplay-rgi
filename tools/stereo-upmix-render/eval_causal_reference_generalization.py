#!/usr/bin/env python3
"""Fixed terminal causal reference comparison on inspected DEV40; no tuning.

Reuse the completed SIMO evaluation's source/entry/C metric functions under its
existing isolated venv. Only the extractor changes; original input gain remains.
"""
import argparse
import csv
import gc
import json
from pathlib import Path
import time

import numpy as np
import torch
import bsrnn_reference_eval as E
import separator_causal_reference as CR
import train_causal_reference_generalization as G


class CausalMask:
    def __init__(self, net):
        self.net=net

    @torch.inference_mode()
    def apply(self, mixture, sources):
        if not torch.allclose(sources.sum(0),mixture,atol=2e-7,rtol=2e-6):
            raise RuntimeError('True source sum differs from mixture')
        n=mixture.shape[-1]
        x=G.SN.stft(torch,mixture[None].to('cuda'))
        y=self.net.separate_stft(x)
        mask=self.net.last_mask
        z=G.SN.stft(torch,sources.to('cuda'))
        sent=G.SN.istft(torch,mask*z,n).cpu()
        estimate=G.SN.istft(torch,y,n)[0].cpu()
        closure=E.fraction(sent.sum(0)-estimate,estimate)
        if closure is None or closure>1e-9 or not torch.isfinite(sent).all():
            raise RuntimeError('Causal mask/source closure failed')
        return estimate,sent,dict(final_source_sum_nse=closure,
            scope='continuous whole excerpt, causal fixed-STFT network; no 11s chunk resets/future mixing; waveform streaming latency unqualified')


def paired_sdr(candidate,control):
    old={r['song']:r for r in control['rows']}
    delta=np.asarray([r['sdr_db']-old[r['song']]['sdr_db'] for r in candidate['rows']
                      if r['sdr_db'] is not None and old[r['song']]['sdr_db'] is not None])
    if not len(delta):raise ValueError('No covered paired SDR')
    rng=np.random.default_rng(0)
    bootstrap=np.median(delta[rng.integers(len(delta),size=(2000,len(delta)))],axis=1)
    return dict(songs=len(delta),median_delta_db=float(np.median(delta)),
                ci90=np.percentile(bootstrap,[5,95]).tolist(),nonworse=int((delta>=0).sum()))


def compare_defined(new,old,kind,margin):
    if new is None or old is None:return False
    return new<=old+margin if kind=='upper' else new>=margin*old if kind=='fraction' else new>=old-margin


def main(a):
    G.configure('cuda');m=G.check_manifest(a)
    audit=json.loads((a.out/'audit.json').read_text())
    if not audit['verified'] or audit['protocol_sha256']!=G.sha(a.out/'manifest.json'):
        raise ValueError('Final CPU checkpoint audit required')
    declared=json.loads((a.runs/'direct_object_full40/manifest.json').read_text())
    ids=list(declared['song_track_ids'].values())
    if set(ids)!={r['song'] for r in m['evaluation']['dev']} or len(ids)!=40:
        raise ValueError('Previously frozen DEV40 differs')
    build=json.loads((a.tools/'direct_object_host/manifest.json').read_text())
    binaries={name:a.tools/'direct_object_host'/('upmix_'+name+'.exe') for name in ('control','direct_no_bed')}
    for name,path in binaries.items():
        if G.sha(path)!=build['builds'][name]['sha256']:raise ValueError('Frozen C binary changed')
    expected=E.gate.read(a.runs/'direct_object_full40/baseline/stems.csv')
    expected_keys={(r['song'],r['stem']) for r in expected}
    if len(expected_keys)!=231:raise ValueError('Original harness coverage differs')
    destination=a.out/'full40';destination.mkdir(exist_ok=True)
    protocol=dict(scope='previously inspected DEV40 only; causal PC package, no Test/device/car result',
        protocol_sha256=G.sha(a.out/'manifest.json'),script_sha256=G.sha(Path(__file__)),
        helper_hashes={Path(module.__file__).name:G.sha(Path(module.__file__)) for module in [E,E.ref,E.H,E.T,E.DT,E.gate]},
        c_binary_hashes={name:G.sha(p) for name,p in binaries.items()},
        c_hssr_model_sha256=G.sha(a.tools/'hssr_model.bin'),c_guard_sha256=G.sha(a.tools/'hssr_guard.bin'),
        expected_rows_sha256=G.sha(a.runs/'direct_object_full40/baseline/stems.csv'),
        input_gain='0.7/dry_mix_peak, unchanged original harness',injection_gain=1.0,
        variants='original C and fixed direct_no_bed/body0/surround0.3235, identical both models',
        selection='terminal40k only; no gain or checkpoint search; independent20 unopened')
    if a.resume:
        if json.loads((destination/'protocol.json').read_text())!=protocol:raise ValueError('Resume protocol changed')
    else:
        if (destination/'protocol.json').exists():raise ValueError('Use explicit verified resume')
        G.write(destination/'protocol.json',protocol)
    common=E.T.VARIANTS['shipped']+['--model',str(a.tools/'hssr_model.bin'),
                    '--guard-model',str(a.tools/'hssr_guard.bin'),'--mask-drive','1.0']
    result={}
    for mode in G.MODES:
        folder=destination/mode;folder.mkdir(exist_ok=True)
        ck_path=a.out/mode/'step40000.pt'
        if G.sha(ck_path)!=audit['arms'][mode]['checkpoint_sha256']:raise ValueError('Audited checkpoint changed')
        net,ck=CR.load(torch,ck_path,'cuda');frozen=CausalMask(net)
        sets={'baseline':[],'direct_no_bed_body0_cal':[]};extraction=[];inputs=[]
        for index,(song,song_id) in enumerate(declared['song_track_ids'].items(),1):
            own=folder/(song_id+'.complete.json')
            if a.resume and own.exists():
                row=json.loads(own.read_text())
                if row['checkpoint_sha256']!=G.sha(ck_path):raise ValueError('Resume checkpoint differs')
                for point,h in row['inputs'].items():
                    for suffix,key in [('.wav','mixture_sha256'),('.inj.wav','injection_sha256'),('.6ch.wav','output_sha256')]:
                        if G.sha(folder/point/(song_id+suffix))!=h[key]:raise ValueError('Resume C file changed')
                if row['extraction']['parity']['final_source_sum_nse']>1e-9:raise ValueError('Resume source closure differs')
                for point,rows in row['rowsets'].items():sets[point]+=rows
                extraction.append(row['extraction']);inputs.append(row['inputs'])
                continue
            begin=time.monotonic()
            meta,stems=E.H.load_song(str(a.moisesdb/song_id),60)
            n=min(v.shape[-1] for v in stems.values());stems={k:v[:,:n] for k,v in stems.items()}
            mixture=torch.from_numpy(sum(stems.values()));gain=.7/(float(mixture.abs().max())+1e-9)
            sources=E.roles_from_stems(stems)*gain;mixture*=gain
            estimate,sent,parity=frozen.apply(mixture,sources)
            measured=E.extraction_row(song,estimate,sent,sources)
            measured.update(track_id=song_id,samples=n,parity=parity)
            rowsets={};hashes={}
            for point,binary,extra in [('baseline','control',[]),('direct_no_bed_body0_cal','direct_no_bed',['--rear-body','0','--surround','0.3235'])]:
                rows,h=E.render_rows(folder/point,song_id,meta,stems,mixture,estimate,binaries[binary],common+extra)
                if {(r['song'],r['stem']) for r in rows}!={k for k in expected_keys if k[0]==song}:
                    raise ValueError('Original per-song stem coverage differs')
                sets[point]+=rows;rowsets[point]=rows;hashes[point]=h
            extraction.append(measured);inputs.append(hashes)
            G.write(own,dict(checkpoint_sha256=G.sha(ck_path),extraction=measured,rowsets=rowsets,inputs=hashes))
            print('FULL40',mode,index,40,song_id,round(time.monotonic()-begin,2),'seconds',flush=True)
            del stems,mixture,sources,estimate,sent;gc.collect()
        summaries={}
        for point,rows in sets.items():
            if {(r['song'],r['stem']) for r in rows}!=expected_keys or len(rows)!=231:
                raise ValueError('Full original231 row coverage differs')
            with (folder/point/'stems.csv').open('w',newline='') as f:
                writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
            # JSON null is preserved in exports; metric functions receive NaN.
            numeric=[{k:float('nan') if v is None else v for k,v in r.items()} for r in rows]
            summaries[point]=E.gate.summary(numeric)
        entries=E.transitions(frozen,a.cache,ids,folder,resume=a.resume)
        entries['scope']='40 inspected DEV songs; causal extractor, actual-mixture mask source attribution; no device or independent result'
        G.write(folder/'transitions.json',entries)
        source_summary=dict(source_tx={role:E.med([r['transmission'][role] for r in extraction]) for role in E.ROLES},
                            sdr_db=E.med([r['other_si_sdr_channel_mean_db'] for r in extraction]),
                            bands={b:E.med([r['bands'][b] for r in extraction]) for b in ['8-14k','>14k']})
        decisions={}
        for point,summary in summaries.items():
            gates=E.gate.passes(summary,summaries['baseline'],'representation40');gates.update(entries['gates'])
            decisions[point]=dict(summary=summary,gates=gates,observed_joint_gates=all(gates.values()))
        result[mode]=dict(complete=True,checkpoint_sha256=G.sha(ck_path),source_summary=source_summary,
                          extraction=extraction,variants=decisions,transitions=entries['summaries'])
        G.write(folder/'complete.json',result[mode])
        del frozen,net;gc.collect();torch.cuda.empty_cache()
    # Relative quality diagnostics never replace the absolute product gates.
    control,candidate=(result[mode] for mode in G.MODES)
    monitor={mode:json.loads((a.out/mode/'eval_step40000.json').read_text()) for mode in G.MODES}
    delta=paired_sdr(monitor['residual96x4']['metrics']['dev'],monitor['original']['metrics']['dev'])
    old,new=control['source_summary'],candidate['source_summary']
    relative=dict(dev_paired_sdr_at_least_half_db=delta['median_delta_db']>=.5,
                  dev_paired_sdr_ci_positive=delta['ci90'][0]>0,
                  eligible_nonworse=new['source_tx']['eligible']>=old['source_tx']['eligible'])
    relative.update({s+'_tx_no_more_than_001_regression':compare_defined(new['source_tx'][s],old['source_tx'][s],'upper',.01) for s in ['vocal','drums','bass']})
    for key in ['median','p10']:
        relative['entry_'+key+'_no_more_than_002_regression']=compare_defined(candidate['transitions']['entry']['duck'][key],control['transitions']['entry']['duck'][key],'lower',.02)
    for key in ['ret_pre','ret_post']:
        relative['entry_'+key+'_at_least_095_control']=compare_defined(candidate['transitions']['entry'][key]['median'],control['transitions']['entry'][key]['median'],'fraction',.95)
    direct='direct_no_bed_body0_cal'
    relative['matched_direct_rear_level_within_half_db']=abs(candidate['variants'][direct]['summary']['rear_rms_db']-control['variants'][direct]['summary']['rear_rms_db'])<=.5
    G.write(a.out/'complete.json',dict(complete=True,scope=protocol['scope'],arms=result,
          paired_dev20s_sdr=delta,relative_gates=relative,relative_quality_gate_pass=all(relative.values()),
          causal_product_gate_pass=candidate['variants'][direct]['observed_joint_gates'],
          device_independent_and_owner_ear_gates='unmeasured; reference above compute allowance',
          evaluation_protocol_sha256=G.sha(destination/'protocol.json')))
    print('CAUSAL_GENERALIZATION_FULL40_COMPLETE',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cache','initial','out','runs','tools','moisesdb'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--resume',action='store_true')
    main(p.parse_args())
