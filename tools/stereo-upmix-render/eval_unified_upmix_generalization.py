#!/usr/bin/env python3
"""Terminal unified source models: full inspected DEV40, real NN-free C.

The fixed nominal surround gain is.40; .3235 and.50 are diagnostic operating
points, not per-song gain fitting. Scene heads are not used or claimed trained.
"""
import argparse
import csv
import gc
import json
from pathlib import Path
import subprocess
import time
import numpy as np
import torch
import bsrnn_reference_eval as E
import train_unified_upmix_generalization as B

C_SHA='6bc42871556e8bc1df94f13e1417d8f78c0c9be10d170e9044e73a8787184aa2'
POINTS={'reference_03235':.3235,'nominal_040':.40,'diagnostic_050':.50}


class UnifiedMask:
    def __init__(self,net):self.net=net.eval()

    @torch.inference_mode()
    def apply(self,mixture,sources):
        if not torch.allclose(sources.sum(0),mixture,atol=2e-7,rtol=2e-6):raise ValueError('True source sum differs')
        n=mixture.shape[-1];x=B.S.analyze(mixture[None].to('cuda'))
        pred,_=self.net.forward_stft(x,with_scene=False)
        refs=B.spectra(sources[None].to('cuda'))
        sent=B.waves(pred['source_masks'][:,:,0,None]*refs,n)[0].cpu()
        estimate=B.S.synthesize(pred['source_stft'][:,:,0],n)[0].cpu()
        closure=E.fraction(sent.sum(0)-estimate,estimate)
        if closure is None or closure>1e-9 or not torch.isfinite(sent).all():raise ValueError('Physical source attribution failed')
        return estimate,sent,dict(final_source_sum_nse=closure,
            scope='whole waveform past-only analysis; continuous neural state; physical endpoints; fixed delay compensated for offline aligned C input; not a device integration claim')


def render(out,track,meta,stems,mix,estimate,executable,gain):
    out.mkdir(exist_ok=True)
    pcm,inj,dst=[out/(track+s) for s in ('.wav','.inj.wav','.6ch.wav')]
    if not torch.isfinite(estimate).all() or float(estimate.abs().max())>=1:raise ValueError('C injection would clip; no gain correction allowed')
    E.T.write_wav(str(pcm),mix.numpy());E.T.write_wav(str(inj),estimate.numpy())
    if dst.exists():dst.unlink()
    args=['--mode','hssr-universal','--lfe','assist','--lfe-gain','0','--surround',str(gain),
          '--front-bass','-3','--front-retention','.5','--bed-delays','--bed-attack','off',
          '--rear-air','-9','--rear-body','0','--mask-drive','1','--rear-audio',str(inj),str(pcm),str(dst)]
    result=subprocess.run([str(executable),*args],capture_output=True,text=True)
    if result.returncode or 'old_neural_invocations=0, old_blobs=0' not in result.stderr:
        raise RuntimeError('NN-free copied C execution contract failed: '+result.stderr)
    (out/(track+'.render.log')).write_text(result.stderr)
    output=E.T.read_wav6(str(dst))
    if output.shape!=(6,mix.shape[-1]):raise ValueError('C channel/duration differs')
    return E.rows_from_output(meta,stems,output),dict(mixture_sha256=B.T.F.sha(pcm),injection_sha256=B.T.F.sha(inj),
        output_sha256=B.T.F.sha(dst),injection_peak=float(estimate.abs().max()),old_neural_invocations=0)


def main(a):
    B.T.configure('cuda');m=B.check(a)
    audit=json.loads((a.out/'audit.json').read_bytes())
    if not audit['verified'] or audit['smoke'] or audit['protocol_sha256']!=B.T.F.sha(a.out/'manifest.json'):
        raise ValueError('Verified terminal broad pair required')
    executable=a.tools/'unified_audio_host_v1/upmix_unified_audio.exe'
    if B.T.F.sha(executable)!=C_SHA:raise ValueError('Frozen NN-free C host changed')
    declared=json.loads((a.runs/'direct_object_full40/manifest.json').read_bytes())
    ids=list(declared['song_track_ids'].values())
    if len(ids)!=40 or set(ids)!={r['song'] for r in m['evaluation']['dev']}:raise ValueError('Full DEV40 differs')
    expected={(r['song'],r['stem']) for r in E.gate.read(a.runs/'direct_object_full40/baseline/stems.csv')}
    if len(expected)!=231:raise ValueError('Original stem coverage differs')
    baseline=json.loads((a.runs/'tracka_matched40_2026_09_30/complete.json').read_bytes())
    destination=a.out/'full40';destination.mkdir(exist_ok=True)
    protocol=dict(source_manifest_sha256=B.T.F.sha(a.out/'manifest.json'),audit_sha256=B.T.F.sha(a.out/'audit.json'),
        source_hashes=B.hashes(),script_sha256=B.T.F.sha(__file__),
        helpers={Path(mod.__file__).name:B.T.F.sha(mod.__file__) for mod in (E,E.ref,E.H,E.T,E.DT,E.gate)},
        c_sha256=C_SHA,points=POINTS,nominal='nominal_040',input_gain='0.7/dry mixture peak; injection unity; no front trim or per-song normalization',
        renderer='copied direct no-bed/body0; frontretention.5, frontbass-3, air-9, attackoff, LFEnosemanticassist; oldNNcalls0',
        selection='terminal40k; same predeclared operating points in both arms; nominal.40 fixed before DEV; no checkpoint/gain best-of search',
        limits='inspected DEV40 only; native48-band scene head dormant; no semantic scene/C ABI port, holdout, device or ears')
    if a.resume:
        if json.loads((destination/'protocol.json').read_bytes())!=protocol:raise ValueError('Resume evaluator protocol differs')
    elif (destination/'protocol.json').exists():raise FileExistsError('Explicit verified resume required')
    else:E.write(destination/'protocol.json',protocol)
    results={}
    for mode in B.MODES:
        checkpoint=a.out/mode/'step40000.pt'
        if B.T.F.sha(checkpoint)!=audit['arms'][mode]['checkpoint_sha256']:raise ValueError('Audited checkpoint changed')
        net,_=B.T.load(checkpoint,'cuda');frozen=UnifiedMask(net);folder=destination/mode;folder.mkdir(exist_ok=True)
        rowsets={name:[] for name in POINTS};extractions=[]
        for index,(song,track) in enumerate(declared['song_track_ids'].items(),1):
            marker=folder/(track+'.complete.json')
            if a.resume and marker.exists():
                saved=json.loads(marker.read_bytes())
                if saved['checkpoint_sha256']!=B.T.F.sha(checkpoint):raise ValueError('Saved per-song checkpoint differs')
                for name,h in saved['hashes'].items():
                    for suffix,key in [('.wav','mixture_sha256'),('.inj.wav','injection_sha256'),('.6ch.wav','output_sha256')]:
                        if B.T.F.sha(folder/name/(track+suffix))!=h[key]:raise ValueError('Saved C waveform changed')
                for name,rows in saved['rows'].items():rowsets[name]+=rows
                extractions.append(saved['extraction']);continue
            begin=time.monotonic();meta,stems=E.H.load_song(str(a.moisesdb/track),60)
            n=min(v.shape[-1] for v in stems.values());stems={k:v[:,:n] for k,v in stems.items()}
            mix=torch.from_numpy(sum(stems.values()));gain=.7/(float(mix.abs().max())+1e-9)
            roles=E.roles_from_stems(stems)*gain;mix*=gain
            estimate,sent,parity=frozen.apply(mix,roles);extraction=E.extraction_row(song,estimate,sent,roles)
            extraction.update(track_id=track,parity=parity,samples=n);own={};hashes={}
            for name,surround in POINTS.items():
                rows,h=render(folder/name,track,meta,stems,mix,estimate,executable,surround)
                if {(r['song'],r['stem']) for r in rows}!={k for k in expected if k[0]==song}:raise ValueError('Per-song stem coverage differs')
                own[name]=rows;hashes[name]=h;rowsets[name]+=rows
            extractions.append(extraction)
            E.write(marker,dict(checkpoint_sha256=B.T.F.sha(checkpoint),extraction=extraction,rows=own,hashes=hashes))
            print('UNIFIED_FULL40',mode,index,40,'seconds',time.monotonic()-begin,flush=True)
            del stems,mix,roles,estimate,sent;gc.collect()
        variants={}
        for name,rows in rowsets.items():
            if len(rows)!=231 or {(r['song'],r['stem']) for r in rows}!=expected:raise ValueError('Full231 stem coverage differs')
            with (folder/name/'stems.csv').open('w',newline='',encoding='utf-8') as f:
                writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
            numeric=[{k:float('nan') if v is None else v for k,v in r.items()} for r in rows]
            summary=E.gate.summary(numeric);gates=E.gate.passes(summary,summary,'full40');gates.pop('rear_rms_within_half_db',None)
            variants[name]=dict(summary=summary,gates=gates,observed_render_gates_pass=all(gates.values()),
                                rear_rms_delta_current_tracka_db=summary['rear_rms_db']-baseline['summary']['rear_rms_db'])
        transitions=E.transitions(frozen,a.cache,ids,folder,resume=a.resume)
        transitions['scope']='source-attributed physical causal extractor, whole actual-mixture waveform; nominal renderer must be separately checked for additional scene state'
        E.write(folder/'transitions.json',transitions)
        source_summary=dict(source_tx={s:E.med([r['transmission'][s] for r in extractions]) for s in E.ROLES},
            sdr_db=E.med([r['other_si_sdr_channel_mean_db'] for r in extractions]),
            bands={b:E.med([r['bands'][b] for r in extractions]) for b in ('8-14k','>14k')})
        nominal_gates=variants['nominal_040']['gates']|transitions['gates']
        result=dict(complete=True,checkpoint_sha256=B.T.F.sha(checkpoint),source_summary=source_summary,
                    extraction=extractions,variants=variants,transitions=transitions['summaries'],
                    nominal_observed_gates=nominal_gates,nominal_observed_gates_pass=all(nominal_gates.values()))
        E.write(folder/'complete.json',result);results[mode]=result
        del frozen,net;gc.collect();torch.cuda.empty_cache()
    old,new=(results[mode] for mode in B.MODES)
    monitor={mode:json.loads((a.out/mode/'eval_step40000.json').read_bytes())['metrics']['dev'] for mode in B.MODES}
    controls={r['song']:r for r in monitor['eligible_only']['rows']}
    delta=np.asarray([r['sdr_db']['eligible']-controls[r['song']]['sdr_db']['eligible']
                      for r in monitor['joint_audio']['rows'] if r['sdr_db']['eligible'] is not None and controls[r['song']]['sdr_db']['eligible'] is not None])
    if len(delta)!=40:raise ValueError('Full40 paired eligible source coverage required')
    old_nominal=old['variants']['nominal_040']['summary'];new_nominal=new['variants']['nominal_040']['summary']
    relative=dict(eligible_dev_sdr_nonworse_03db=float(np.median(delta))>=-.3,
                  eligible_tx_at_least_90pct_control=new['source_summary']['source_tx']['eligible']>=.9*old['source_summary']['source_tx']['eligible'],
                  any_stem_swings_nonworse=len(new_nominal['swings'])<=len(old_nominal['swings']))
    for source in ('vocal','drums','bass'):
        x,y=new['source_summary']['source_tx'][source],old['source_summary']['source_tx'][source]
        relative[source+'_tx_no_more_than_001_regression']=x is not None and y is not None and x<=y+.01
    for statistic in ('median','p10'):
        x,y=new['transitions']['entry']['duck'][statistic],old['transitions']['entry']['duck'][statistic]
        relative['entry_'+statistic+'_nonworse']=x is not None and y is not None and x>=y
    E.write(destination/'complete.json',dict(complete=True,arms=results,relative_gates=relative,
        relative_gates_pass=all(relative.values()),paired_dev_sdr_median_delta_db=float(np.median(delta)),
        protocol_sha256=B.T.F.sha(destination/'protocol.json'),
        device_or_product_pass=False,limits=protocol['limits']))
    print('UNIFIED_FULL40_COMPLETE',json.dumps({k:dict(source=v['source_summary'],nominal=v['variants']['nominal_040']['summary'],gates=v['nominal_observed_gates']) for k,v in results.items()}),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cache','initial','partition','monitor','out','tools','runs','moisesdb'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--resume',action='store_true');main(p.parse_args())
