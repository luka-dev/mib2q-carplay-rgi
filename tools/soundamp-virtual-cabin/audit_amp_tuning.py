#!/usr/bin/env python3
"""Local tuning experiments, not a cabin SPL predictor or production preset.

grid: nine retention/rear combinations; bass: trims/LFE around a chosen grid
point; stress: explicit common EQ and arbitrary B-boundary stress signals;
match: post-guard, front-mid-power-matched6ch comparison files with full-clip
v7 sample-peak margin. No vehicle I/O, no automatic production selection.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np
import soundfile as sf
from amp_dsp_chain import AmpChain, ROOT, SR, delay_channels
from amp_music_report import (BANDS, GROUPS, band_powers, fingerprint, load_upstream_scenario,
                              measure_output, read_audio)
from audit_amp_retention_candidates import peak_metrics

DIAGNOSTICS = ('minimum-guard-gain', 'attenuated-frames', 'total-frames', 'limiter-attacks',
               'longest-observed-run', 'reduction-db-sum', 'over3-frames', 'over6-frames', 'over10-frames')


def require_hash(record):
    if fingerprint(record['path'])['sha256'] != record['sha256']:
        raise ValueError('file changed: '+record['path'])


def full_peak(out):
    peak = float(np.max(abs(out)))
    per_frame = np.max(abs(out), axis=1)
    return {'peak_dbfs': float(20*np.log10(max(peak, 1e-15))),
            'frames_above_0dbfs': int(np.count_nonzero(per_frame > 1)),
            'frames_above_minus1dbfs': int(np.count_nonzero(per_frame > 10**(-1/20))),
            'frames': len(out), 'scope': 'whole rendered clip; sample peaks, NOT true peaks or physical overload'}


def delta(measured, dry):
    return {g: (np.array(v['power_dbfs'])-dry['groups'][g]['power_dbfs']).tolist()
            for g, v in measured['groups'].items()}


def summarize(tracks):
    names = list(next(iter(tracks.values()))['variants'])
    result = {}
    for name in names:
        rows = [t['variants'][name] for t in tracks.values()]
        means = {g: np.mean([v['delta_vs_dry_bank1'][g] for v in rows], axis=0).tolist() for g in GROUPS}
        tilts = [v['delta_vs_dry_bank1']['front woofers'][1]-v['delta_vs_dry_bank1']['front mid/treble'][4] for v in rows]
        rear_relative = [v['delta_vs_dry_bank1']['rear doors'][4]-v['delta_vs_dry_bank1']['front mid/treble'][4] for v in rows]
        dg = [v['guard'] for v in rows]
        frames = sum(v['total-frames'] for v in dg)
        result[name] = {'parameters': rows[0]['parameters'], 'mean_delta_vs_dry_bank1': means,
                        'tilt_mean_min_max_db': [float(np.mean(tilts)), min(tilts), max(tilts)],
                        'rear_relative_to_front_mid_change_db': float(np.mean(rear_relative)),
                        'whole_clip_peak_dbfs': max(v['whole_clip']['peak_dbfs'] for v in rows),
                        'guard_attenuated_fraction': sum(v['attenuated-frames'] for v in dg)/frames,
                        'guard_mean_reduction_db_all_frames': sum(v['reduction-db-sum'] for v in dg)/frames,
                        'guard_longest_observed_run_ms': max(v['longest-observed-run'] for v in dg)/48,
                        'guard_min_gain': min(v['minimum-guard-gain'] for v in dg),
                        'guard_over3_fraction': sum(v['over3-frames'] for v in dg)/frames,
                        'guard_over6_fraction': sum(v['over6-frames'] for v in dg)/frames,
                        'max_matrix_peak': max(max(v['input_metrics']['matrix_bus_peak']) for v in rows)}
    return result


def render_sweep(a, base, chain):
    if a.phase == 'grid':
        variants = {f'r{round(r*100)}_s{round(s*100)}': dict(retention=r, surround=s, front_bass=0, lfe='assist', lfe_gain=0)
                    for r in (.5, .75, 1.) for s in (.3, .35, .4)}
    else:
        variants = {name: dict(retention=a.retention, surround=a.surround, front_bass=bass, lfe=lfe, lfe_gain=gain)
                    for name, bass, lfe, gain in (('flat',0,'assist',0), ('bass1',-1,'assist',0),
                        ('bass2',-2,'assist',0), ('lfe_minus6',0,'assist',-6), ('lfe_plus6',0,'assist',6), ('lfe_off',0,'off',0))}
    pcm_dir = Path(tempfile.mkdtemp(prefix='upmix-'+a.phase+'-'))
    report = {'complete': False, 'scope': __doc__, 'phase': a.phase, 'model': chain.metadata(),
              'baseline_report': fingerprint(a.baseline), 'analysis': base['analysis'], 'bands': BANDS,
              'renderer': fingerprint(a.renderer), 'main_model': fingerprint(a.model), 'guard_model': fingerprint(a.guard),
              'source_code': [fingerprint(ROOT/p) for p in ('hook/audio/stereo_upmix.c', 'hook/audio/stereo_upmix.h',
                  'hook/audio/hssr_dsp.c', 'hook/audio/hssr_neon.c', 'hook/audio/soundcheck.c',
                  'tools/stereo-upmix-render/stereo_upmix_render.c')],
              'pcm_directory': str(pcm_dir), 'tracks': {},
              'note': 'Unity programme gain. lfe_off also selects the existing higher front gain; not pure LFE ablation.'}
    start,end = [round(t*SR) for t in base['analysis']['window_source_seconds']]
    latency = base['analysis']['upmix_latency_frames']
    for key, source in base['files'].items():
        if not key.endswith(':dry'): continue
        require_hash(source); song = key.rsplit(':',1)[0]
        stereo = read_audio(source['path'],2); ref=band_powers(stereo[start:end,0])
        rows = {}; report['tracks'][song] = dict(source=source, variants=rows)
        for name, params in variants.items():
            target=pcm_dir/(song+'-'+name+'.wav')
            cmd=[str(a.renderer.resolve()), '--float-render', '--model',str(a.model.resolve()),
                 '--guard-model',str(a.guard.resolve()), '--front-retention',str(params['retention']),
                 '--surround',str(params['surround']), '--front-bass',str(params['front_bass']),
                 '--lfe',params['lfe'], '--lfe-gain',str(params['lfe_gain']),source['path'],str(target)]
            run=subprocess.run(cmd,check=True,capture_output=True,text=True)
            (a.out/(song+'-'+name+'.log')).write_text(run.stderr)
            fields=dict(re.findall(r'([a-z0-9-]+)=([0-9.eE+-]+)',run.stderr))
            x=read_audio(target,6); metrics=peak_metrics(x,chain.M)
            if max(metrics['matrix_bus_peak'])>10**(-1/20)+8/32768 or metrics['pcm_rail_samples']:
                raise ValueError('input guard bound failed')
            out=chain.process(x,capture_stages=False)['out']
            row=measure_output(chain,out,ref,start,end,latency)
            row.update(parameters=params,file=fingerprint(target),command=cmd,input_metrics=metrics,
                       whole_clip=full_peak(out),guard={k:float(fields[k]) for k in DIAGNOSTICS})
            row['delta_vs_dry_bank1']=delta(row,base['results'][key]); rows[name]=row
            del x,out
            (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(a.phase,song,name,'done',flush=True)
        if a.phase=='bass':
            # Pure post-render ablation separates LFE summation from the
            # CLI off mode's front-gain change and a fresh guard trajectory.
            x=read_audio(rows['flat']['file']['path'],6);x[:,3]=0
            out=chain.process(x,capture_stages=False)['out']
            control=measure_output(chain,out,ref,start,end,latency)
            control.update(scope='only cached flat PCM channel3 zeroed; no new guard; not a production candidate',
                           input_metrics=peak_metrics(x,chain.M),whole_clip=full_peak(out))
            control['delta_vs_flat']={g:(np.array(v['power_dbfs'])-rows['flat']['groups'][g]['power_dbfs']).tolist()
                                     for g,v in control['groups'].items()}
            report['tracks'][song]['lfe_zero_only_control']=control
            del x,out
    report['summary']=summarize(report['tracks']);report['complete']=True
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')


def stress_signals(stages):
    """Arbitrary boundary tests, NOT reconstructed B music or calibrated bounds."""
    first=stages['ten'].copy();first[:,[2,3]]=0
    # Muted A lanes8/9 stay zero: this does not exercise every possible B output.
    second=stages['split16'].copy()
    return first,second


def match_gains(mid_db, peaks_db, ceiling=-1.2):
    """One common front-mid target; attenuation only, all full-clip peaks bound."""
    mid=np.asarray(mid_db,float);peaks=np.asarray(peaks_db,float)
    if mid.shape!=peaks.shape or not np.isfinite(mid).all() or not np.isfinite(peaks).all():
        raise ValueError('finite paired level/peak vectors required')
    target=float(np.min(mid+np.minimum(0,ceiling-peaks)))
    return target,(target-mid)


def add_candidates(grid, extra, names):
    """Merge an additional sweep in memory only, failing on mismatched inputs."""
    if not extra['complete'] or extra['model']!=grid['model'] or extra['analysis']!=grid['analysis']:
        raise ValueError('extra report differs in model/analysis')
    if set(extra['tracks'])!=set(grid['tracks']):raise ValueError('extra track set differs')
    for song,t in grid['tracks'].items():
        other=extra['tracks'][song]
        if t['source']['sha256']!=other['source']['sha256']:raise ValueError('extra source PCM differs')
        for name in names:
            if name in t['variants']:raise ValueError('duplicate candidate name')
            t['variants'][name]=other['variants'][name]


def postprocess(a, base, chain):
    grid=json.loads(a.grid.read_text())
    if not grid['complete'] or grid['model']!=chain.metadata():raise ValueError('complete matching grid required')
    if a.extra:
        add_candidates(grid,json.loads(a.extra.read_text()),a.extra_names)
        a.names+=a.extra_names
    for t in grid['tracks'].values():
        for name in a.names:require_hash(t['variants'][name]['file'])
    report={'complete':False,'scope':__doc__,'phase':a.phase,'grid':fingerprint(a.grid),
            'model':chain.metadata(),'bands':BANDS,'names':a.names,'tracks':{}}
    if a.extra:report['extra_report']=fingerprint(a.extra)
    if a.phase=='match':
        # Final comparison files are deliverables, unlike temporary sweep PCM.
        pcm_dir=a.out.resolve()/'pcm';pcm_dir.mkdir()
        report.update(pcm_directory=str(pcm_dir),method='same front-mid500..2000Hz power per track, not LUFS/SPL; post-guard attenuation; v7 whole-clip ceiling-1.2dBFS before final S16 quantization')
        start,end=[round(t*SR) for t in base['analysis']['window_source_seconds']];latency=512
        for song,t in grid['tracks'].items():
            require_hash(t['source']); stereo=read_audio(t['source']['path'],2);ref=band_powers(stereo[start:end,0])
            dry=np.zeros((len(stereo),6));dry[:,:2]=delay_channels(stereo,[latency]*2)
            dryout=chain.process(dry,capture_stages=False)['out'];drystats=measure_output(chain,dryout,ref,start,end,latency)
            names=['dry_bank1']+a.names
            mids=[drystats['groups']['front mid/treble']['power_dbfs'][4]]+[t['variants'][n]['groups']['front mid/treble']['power_dbfs'][4] for n in a.names]
            peaks=[full_peak(dryout)['peak_dbfs']]+[t['variants'][n]['whole_clip']['peak_dbfs'] for n in a.names]
            target,gains=match_gains(mids,peaks);rows={}
            for name,gain in zip(names,gains):
                x=dry if name=='dry_bank1' else read_audio(t['variants'][name]['file']['path'],6)
                path=pcm_dir/(song+'-'+name+'.wav');sf.write(path,x*10**(gain/20),SR,subtype='PCM_16')
                quantized=read_audio(path,6);y=chain.process(quantized,capture_stages=False)['out']
                stats=measure_output(chain,y,ref,start,end,latency)
                error=stats['groups']['front mid/treble']['power_dbfs'][4]-target
                pk=full_peak(y)
                if abs(error)>.02 or pk['peak_dbfs']>-1.19:raise ValueError('matched artifact verification failed')
                rows[name]=dict(file=fingerprint(path),post_guard_gain_db=float(gain),front_mid_error_db=error,whole_clip=pk)
                del x,quantized,y
            report['tracks'][song]=dict(target_front_mid_power_dbfs=target,variants=rows)
            print('match',song,'verified',flush=True)
    else:
        scenarios={f'bank{b}':load_upstream_scenario(ROOT/f'assets/soundamp/remaining-blocks-20260919/common-input-bank{b}-hypothesis.json') for b in (7,10)}
        report['scenario_files']=[fingerprint(ROOT/f'assets/soundamp/remaining-blocks-20260919/common-input-bank{b}-hypothesis.json') for b in (7,10)]
        report['common_input_audit']=fingerprint(scenarios['bank7']['audit_path'])
        report['scenario_models']={k:AmpChain(upstream_scenario=v).metadata() for k,v in scenarios.items()}
        report.update(scenarios=scenarios,frames_rendered=12*SR,window_source_seconds=[2,10],
                      B_definition='B1=0.25*A ten-bus donor except slots2/3/8/9 zero; B2=0.25*A split16 donor. Separate +/- and delayed96sample tests. Arbitrary stress amplitudes, NOT calibrated bounds or actual B processor/state. Other B path zero; C4D17 identity.',
                      level_reference='dry BANK1 passed through the SAME EQ/B stress; not stock stereo')
        for song,t in grid['tracks'].items():
            require_hash(t['source']);stereo=read_audio(t['source']['path'],2)[:12*SR]
            dry=np.zeros((len(stereo),6));dry[:,:2]=stereo
            ref=band_powers(stereo[2*SR:10*SR,0]);rows={}
            for bank,spec in scenarios.items():
                c=AmpChain(upstream_scenario=spec); by_name={}
                for name in ['dry_bank1']+a.names:
                    x=dry if name=='dry_bank1' else read_audio(t['variants'][name]['file']['path'],6)[:12*SR]
                    stages=c.process(x);y=stages['out']; b1,b2=stress_signals(stages);del stages
                    z=np.zeros_like(x)
                    # B injection is downstream of common-input EQ. The
                    # zero-state upstream response to zero is exactly zero;
                    # use the plain chain for these two isolated contributions.
                    y1=chain.process(z,first_b=b1,capture_stages=False)['out']; y2=chain.process(z,second_b=b2,capture_stages=False)['out']
                    cases={'B0':y,'B1_plus':y+.25*y1,'B1_minus':y-.25*y1,
                           'B1_delay96':y+.25*delay_channels(y1,[96]*16),
                           'B2_plus':y+.25*y2,'B2_minus':y-.25*y2,
                           'B2_delay96':y+.25*delay_channels(y2,[96]*16)}
                    measured={}
                    for case,out in cases.items():
                        stat=measure_output(c,out,ref,2*SR,10*SR,0 if name=='dry_bank1' else 512)
                        stat['whole_clip']=full_peak(out);measured[case]=stat
                    by_name[name]=measured
                    del cases,y,y1,y2,b1,b2,z
                for name in a.names:
                    for case,stat in by_name[name].items():
                        stat['delta_vs_same_scenario_dry']=delta(stat,by_name['dry_bank1'][case])
                rows[bank]=by_name
            report['tracks'][song]=rows
            (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print('stress',song,'done',flush=True)
    report['complete']=True
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('phase',choices=('grid','bass','stress','match'))
    p.add_argument('--baseline',type=Path,default=ROOT/'output/upmix-matrix-corrected-20260920/music-v7/report.json')
    p.add_argument('--renderer',type=Path,default=ROOT/'build/upmix-grid-20260920/renderer')
    p.add_argument('--model',type=Path,default=ROOT/'build/hssr_model.bin')
    p.add_argument('--guard',type=Path,default=ROOT/'build/hssr_guard.bin')
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--retention',type=float,default=.75);p.add_argument('--surround',type=float,default=.3)
    p.add_argument('--grid',type=Path);p.add_argument('--names',nargs='+',default=['r75_s30','r100_s30'])
    p.add_argument('--extra',type=Path);p.add_argument('--extra-names',nargs='+',default=['bass1'])
    a=p.parse_args();base=json.loads(a.baseline.read_text());chain=AmpChain()
    if base['model']!=chain.metadata():raise ValueError('current default v7 baseline required')
    a.out.mkdir(parents=True,exist_ok=False)
    if a.phase in ('grid','bass'):render_sweep(a,base,chain)
    else:postprocess(a,base,chain)


if __name__=='__main__':main()
