"""Read-only verification of the two Sep21 batches; writes new review artifacts."""
import csv
import hashlib
import itertools
import json
from pathlib import Path
import sys

import numpy as np
import soundfile as sf

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/soundamp-virtual-cabin'))
from audit_upmix_end_to_end import verify,fingerprint,array_fingerprint
from amp_end_to_end_metrics import measure,paired_delta

SCENARIO='full_static_nearest_zero_aux'
NAMES=('dry_bank1','zero_trim','startup','retain75_bass1')
VARIANTS=[dict(name='zero_trim',render_mode='s16',surround=.4,front_bass=0.,lfe_gain=0.,retention=0.,lfe='assist'),
          dict(name='startup',render_mode='s16',surround=.4,front_bass=-3.,lfe_gain=0.,retention=0.,lfe='assist'),
          dict(name='retain75_bass1',render_mode='float',surround=.3,front_bass=-1.,lfe_gain=0.,retention=.75,lfe='assist')]


def longest_run(mask):
    padded=np.pad(np.asarray(mask,dtype=np.int8),(1,1))
    delta=np.diff(padded)
    starts=np.flatnonzero(delta==1);ends=np.flatnonzero(delta==-1)
    return int(np.max(ends-starts)) if len(starts) else 0


def main():
    out=Path(__file__).parent
    if (out/'review.json').exists() or (out/'rows.csv').exists():
        raise ValueError('refusing overwrite of review artifacts')
    sources={};rows=[];fingerprints={};pcm_identities={};track_sets=[]
    def collect(value):
        if isinstance(value,dict):
            if 'path' in value and 'sha256' in value:
                key=(value['path'],value['sha256']);fingerprints[key]=value
            for item in value.values():collect(item)
        elif isinstance(value,list):
            for item in value:collect(item)
    for bank in (7,10):
        path=ROOT/f'output/upmix-full-model-check-eq{bank}-20260921/report.json'
        r=json.loads(path.read_text());sources[str(bank)]=fingerprint(path)
        assert r['complete'] and r['completed_rows']==r['expected_rows']==28
        assert r['parameters']['variants']==VARIANTS
        assert r['parameters']['input_eq_bank_hypothesis']==bank
        assert r['parameters']['programme_frames']==288000 and r['parameters']['tail_frames']==12000
        assert r['axes']['scenarios']==[SCENARIO]
        assert len(r['tracks'])==7 and not r['batch']['errors']
        track_sets.append(set(r['tracks']));collect(r)
        for name,track in r['tracks'].items():
            shard=json.loads(Path(r['batch']['shards'][name]['report']['path']).read_text())
            assert shard['tracks'][name]==track and shard['complete']
            group=track['scenarios'][SCENARIO]
            assert set(group['rows'])==set(NAMES)
            assert group['auxiliary']['kind']=='explicit_zero'
            for aux_record in group['auxiliary']['files'].values():
                data,rate=sf.read(aux_record['path'],always_2d=True)
                assert rate==48000 and not np.any(data)
            for variant in NAMES:
                row=group['rows'][variant];pcmr=track['pcm6'][variant]
                identity=pcmr['file']['sha256'];key=(name,variant)
                if key in pcm_identities:assert pcm_identities[key]==identity
                pcm_identities[key]=identity
                scenario=row['scenario']
                assert scenario['first_b_numerical_reference']=='firmware_library_nearest'
                assert scenario['adaptive_enable']==0 and not scenario['actual_vehicle_state_proven']
                assert row['model']['bank']==1
                pcm,rate=sf.read(pcmr['file']['path'],dtype='float64',always_2d=True)
                assert rate==48000 and pcm.shape==(300000,6)
                output,rate=sf.read(row['file']['path'],dtype='float64',always_2d=True)
                assert rate==48000 and output.shape==(300000,16)
                assert np.isfinite(output).all() and np.isfinite(pcm).all()
                assert array_fingerprint(output,'<f4')==row['output_arrays']['measurement_float32']
                measured=measure(pcm,output,288000,12000)
                assert measured==row['metrics']
                assert paired_delta(measured,group['rows']['dry_bank1']['metrics'])==row['delta_vs_dry_bank1']
                assert measured['pcm6']['digital_c_nonzero_samples']==0
                peaks=measured['outputs']['whole_clip']['per_channel_peak_dbfs']
                worst=int(np.argmax(peaks));delta=row['delta_vs_dry_bank1']
                rail=np.abs(pcm)>=32767/32768
                record={'eq_bank':bank,'track':name,'variant':variant,
                    'peak_dbfs':measured['outputs']['whole_clip']['peak_dbfs'],
                    'peak_config_channel':worst,
                    'output_samples_above_unity':measured['outputs']['whole_clip']['samples_above_0dbfs'],
                    'output_frames_above_unity':measured['outputs']['whole_clip']['frames_above_0dbfs'],
                    'pcm6_peak_dbfs':measured['pcm6']['peak_dbfs'],
                    'pcm6_near_rail_samples':int(rail.sum()),
                    'pcm6_longest_near_rail_run':max(longest_run(rail[:,i]) for i in range(6)),
                    'woofer60_120_vs_dry_db':delta['front woofers'][1],
                    'mid500_2000_vs_dry_db':delta['front mid/treble'][4],
                    'bass_mid_tilt_vs_dry_db':delta['front woofers'][1]-delta['front mid/treble'][4],
                    'sub20_60_vs_dry_db':delta['subwoofer'][0],
                    'sub60_120_vs_dry_db':delta['subwoofer'][1],
                    'rear500_2000_vs_dry_db':delta['rear doors'][4],
                    'guard_telemetry_complete':pcmr.get('guard_telemetry_complete',False),
                    'guard_minimum_gain':pcmr.get('guard',{}).get('minimum-guard-gain'),
                    'guard_attenuated_frames':pcmr.get('guard',{}).get('attenuated-frames'),
                    'guard_total_frames':pcmr.get('guard',{}).get('total-frames')}
                rows.append(record)
                print('verified',bank,name,variant,flush=True)
    assert track_sets[0]==track_sets[1]
    assert len(rows)==56
    assert {(v['eq_bank'],v['track'],v['variant']) for v in rows}==set(itertools.product((7,10),track_sets[0],NAMES))
    for record in fingerprints.values():verify(record)
    for record in sources.values():verify(record)
    summary={}
    for bank in (7,10):
        summary[str(bank)]={}
        for variant in NAMES:
            selection=[v for v in rows if v['eq_bank']==bank and v['variant']==variant]
            worst=max(selection,key=lambda v:v['peak_dbfs'])
            summary[str(bank)][variant]={
                'max_peak_dbfs':worst['peak_dbfs'],'max_peak_track':worst['track'],
                'max_peak_config_channel':worst['peak_config_channel'],
                'rows_exceeding_unity':sum(v['peak_dbfs']>0 for v in selection),
                'pcm6_near_rail_samples':sum(v['pcm6_near_rail_samples'] for v in selection),
                'pcm6_longest_near_rail_run':max(v['pcm6_longest_near_rail_run'] for v in selection),
                'mean_bass_mid_tilt_vs_dry_db':float(np.mean([v['bass_mid_tilt_vs_dry_db'] for v in selection])),
                'mean_sub20_60_vs_dry_db':float(np.mean([v['sub20_60_vs_dry_db'] for v in selection])),
                'mean_sub60_120_vs_dry_db':float(np.mean([v['sub60_120_vs_dry_db'] for v in selection]))}
    result={'complete':True,'verified_rows':56,'unique_referenced_file_hashes_checked':len(fingerprints),
        'verification':'all56 saved FLOAT32 output hashes and every metric recomputed; exact grid/shard equality; identical PCM6 across EQ scenarios; all aux exactlyzero',
        'sources':sources,'review_script':fingerprint(__file__),
        'local_startup_script':fingerprint(ROOT/'deploy/smartphone_integrator/carplay_startup.sh'),
        'scope':'seven6-second music prefixes +.25s zero tail; full conditional electronics, not complete songs or vehicle acceptance',
        'limitations':['input EQ7/10 and mode/pointers/history hypotheses, not live readback',
            'startup variant matches upmix parameters only at unityprogramme gain; no actual SoundCheck metadata or inherited environment',
            'S16 has no float guard telemetry; clean final PCM does not imply absence of internal limiting',
            'runtime gains1, limiter threshold100; >0dBFS is model headroom warning, not hardware clipping proof',
            'auxzero/adaptation0 do not exercise real independent sources or adaptation',
            'dry is L/R through BANK1, NOT stock stereo; output group names unverified physical candidates',
            'float candidate changesfour controls; no single-cause attribution',
            'no acoustic transfer, transitions, full alias proof or bitexactSHARC claim'],
        'summary':summary,'rows':rows,'hardware_or_subjective_acceptance':False}
    with (out/'rows.csv').open('x',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    with (out/'review.json').open('x') as stream:json.dump(result,stream,indent=2,allow_nan=False);stream.write('\n')
    print(json.dumps(summary,indent=2))


if __name__=='__main__':main()
