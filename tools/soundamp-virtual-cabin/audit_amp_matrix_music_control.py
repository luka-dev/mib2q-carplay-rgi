#!/usr/bin/env python3
"""Offline controls on cached PCM: reinsert old transpose and remove ONLY LFE.

No fresh upmix, car access or settings changes. The transpose control uses the
current implementation with one deliberate matrix mutation, not a historical
firmware/model replay. Baseline is dry L/R-only BANK1, never stock stereo.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from amp_dsp_chain import AmpChain,SR
from amp_music_report import GROUPS,band_powers,fingerprint,measure,read_audio


def paired_deltas(reference,result):
    return {g:(np.asarray(result['groups'][g]['power_dbfs'])-
               reference['groups'][g]['power_dbfs']).tolist() for g in GROUPS}


def verify_comparable(old,new):
    """Fail closed before using historical numeric deltas."""
    for key in ('bands','group_channels','variants'):
        if old[key]!=new[key]:raise ValueError('report mismatch: '+key)
    for key in ('window_source_seconds','upmix_latency_frames','alignment','power','reference'):
        if old['analysis'][key]!=new['analysis'][key]:raise ValueError('analysis mismatch: '+key)
    if set(old['files'])!=set(new['files']):raise ValueError('different track/variant set')
    for key in old['files']:
        if old['files'][key]['sha256']!=new['files'][key]['sha256']:
            raise ValueError('different PCM: '+key)


def historical_comparison(old,new):
    verify_comparable(old,new)
    return {'old_version':old['model']['version'],'new_version':new['model']['version'],
        'same_pcm_hashes':True,'same_analysis':True,
        'scope':'historical numeric comparison; see isolated control before attributing all change to transpose',
        'group_power_delta_new_minus_old':{k:paired_deltas(old['results'][k],new['results'][k])
                                           for k in new['results']}}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--report',type=Path,required=True)
    p.add_argument('--legacy-report',type=Path,required=True)
    p.add_argument('--variant',default='gain4096_fb0')
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('tracks',nargs='+')
    a=p.parse_args();new=json.loads(a.report.read_text());old=json.loads(a.legacy_report.read_text())
    if new['model']['version']!='bank1-known-electronics-v7':raise ValueError('v7 baseline required')
    if new['model']!=AmpChain().metadata():raise ValueError('default model metadata required')
    result={'scope':__doc__,'report':fingerprint(a.report),'legacy_report':fingerprint(a.legacy_report),
        'bands':new['bands'],'historical':historical_comparison(old,new),'controls':{}}
    start,end=[round(v*SR) for v in new['analysis']['window_source_seconds']]
    latency=new['analysis']['upmix_latency_frames']
    for song in a.tracks:
        key=song+':'+a.variant;drykey=song+':dry'
        source=new['files'][drykey];authored=new['files'][key]
        for file in (source,authored):
            if fingerprint(file['path'])['sha256']!=file['sha256']:raise ValueError('input changed')
        ref=band_powers(read_audio(source['path'],2)[start:end,0])
        x=read_audio(authored['path'],6)
        transpose=AmpChain();transpose.M8=transpose.M8.T.copy();transpose.M=transpose.M8[:,:6]
        prior=measure(transpose,x,ref,start,end,latency)
        x[:,3]=0 # numeric ablation only, no altered source file is saved
        removed=measure(AmpChain(),x,ref,start,end,latency)
        actual=new['results'][key]
        row={'source':source,'authored':authored,'variant':a.variant,
            'current':actual,'transpose_reinserted':prior,'lfe_channel_zeroed':removed,
            'delta_current_minus_transpose':paired_deltas(prior,actual),
            'delta_lfe_zeroed_minus_current':paired_deltas(actual,removed),
            'delta_lfe_zeroed_vs_dry_bank1':paired_deltas(new['results'][drykey],removed),
            'transpose_control_vs_historical_max_abs_band_db':float(max(abs(v) for arr in
                paired_deltas(old['results'][key],prior).values() for v in arr))}
        result['controls'][song]=row
        print(song,'controls done',flush=True)
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(json.dumps(result,indent=2)+'\n')
    print(a.out)


if __name__=='__main__':main()
