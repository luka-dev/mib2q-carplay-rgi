#!/usr/bin/env python3
"""Offline retained-B state sensitivity, NOT vehicle/acoustic validation.

Independent axes: input EQ selector, B pointer map, FIX rounding hypothesis,
C4D17 adaptation, C4D17 auxiliary and second-B auxiliary. Every row starts from
the declared fresh state. This is a bounded synthetic signal grid, not an
exhaustive control/history space or a certified worst-case signal bound.
"""
import argparse
import copy
from concurrent.futures import ProcessPoolExecutor, as_completed
import itertools
import json
from pathlib import Path

import numpy as np

from amp_dsp_chain import AmpChain, reference_fingerprint
from amp_test_scenarios import make_test_scenario, scenario_manifest
from audit_upmix_end_to_end import array_fingerprint, fingerprint, source_provenance, verify

STAGES=('common_input8','first_b8','first_b10','second_b16','pre_complex_gain',
        'complex','ten','mixed16','limiter_1','limiter_2','out')


def probes(frames):
    if type(frames) is not int or frames<4096 or frames%512:
        raise ValueError('whole512-hop frames >=4096 required')
    programme=frames-1024
    t=np.arange(programme)/48000
    x=np.zeros((frames,6));c4=np.zeros(frames);second=np.zeros((frames,3))
    x[:programme]=.003*np.sin(2*np.pi*t[:,None]*np.array([31,71,149,347,997,5003]))
    c4[:programme]=.01*np.sin(2*np.pi*63*t)
    second[:programme]=.003*np.sin(2*np.pi*t[:,None]*np.array([79,113,173]))
    return x,c4,second


def levels(value):
    x=np.asarray(value)
    if x.ndim!=2 or not np.isfinite(x).all():
        raise ValueError('finite frames xchannels stage required')
    return {'array':array_fingerprint(x,'<f8'),
            'peak_linear':np.abs(x).max(axis=0).tolist(),
            'rms_linear':np.sqrt(np.mean(x*x,axis=0)).tolist(),
            'samples_above_unity':int(np.count_nonzero(abs(x)>1))}


def evaluate_context(*,bank,pointer,rounding,frames):
    """Eight independent aux/adaptation combinations, reset for EVERY row."""
    parameters=make_test_scenario(input_eq_bank=bank,first_b_spectral='retained_calibration',
        first_b_pointer_variant=pointer,first_b_numerics='firmware_library_'+rounding)
    original=reference_fingerprint(parameters)
    x,c4,second=probes(frames)
    rows=[];first_hash=None;second_hash={};checks=[];numerical_comparison=None
    for adaptive,complex_on,second_on in itertools.product((0,1),repeat=3):
        scenario=copy.deepcopy(parameters)
        scenario['complex_scenario']['adaptive_enable']=adaptive
        # Keep embedded declaration consistent with the explicitly changed axis.
        manifest=scenario['first_b_scenario']['reference'].tail_binding_metadata['diagnostic_test_scenario']
        manifest['adaptive_enable']=adaptive
        manifest['complex']=manifest['complex'].replace('enable0,','enable'+str(adaptive)+',')
        aux=dict(complex_auxiliary=c4 if complex_on else np.zeros_like(c4),
                 second_b_auxiliary=second if second_on else np.zeros_like(second))
        chain=AmpChain(**scenario)
        result=chain.process(x,**aux)
        stages={name:levels(result[name]) for name in STAGES}
        fh=stages['first_b10']['array']['sha256'];sh=stages['second_b16']['array']['sha256']
        if first_hash is None:first_hash=fh
        if second_on not in second_hash:second_hash[second_on]=sh
        if first_hash!=fh or second_hash[second_on]!=sh:
            raise AssertionError('independent auxiliary/adaptation axis leaked upstream')
        row={'adaptive_enable':adaptive,'complex_aux_enabled':bool(complex_on),
             'second_b_aux_enabled':bool(second_on),'stages':stages}
        # Both extremes independently replay boundary injections and resets.
        if (adaptive,complex_on,second_on) in ((0,0,0),(1,1,1)):
            injected={k:v for k,v in scenario.items() if k not in ('first_b_scenario','second_b_scenario')}
            separate=AmpChain(**injected).process(x,complex_auxiliary=aux['complex_auxiliary'],
                first_b=result['first_b10'],second_b=result['second_b16'])
            error=float(np.max(abs(result['out']-separate['out'])))
            repeated=chain.process(x,**aux)['out']
            repeat=bool(np.array_equal(repeated,result['out']))
            if error!=0 or not repeat:raise AssertionError('boundary/reset mismatch')
            checks.append({'axes':[adaptive,complex_on,second_on],
                           'boundary_injection_max_absolute_error':error,'reset_exact':repeat})
        if (adaptive,complex_on,second_on)==(0,0,0):
            host=make_test_scenario(input_eq_bank=bank,first_b_spectral='retained_calibration',
                first_b_pointer_variant=pointer,first_b_numerics='host_math')
            host_out=AmpChain(**host).process(x,**aux)['out']
            difference=result['out']-host_out
            numerical_comparison={'axes':[0,0,0],
                'host_math_output':array_fingerprint(host_out,'<f8'),
                'max_absolute_difference':float(np.max(abs(difference))),
                'rms_difference':float(np.sqrt(np.mean(difference*difference))),
                'scope':'only this supplied signal/state; NOT a certified error or audibility bound'}
        rows.append(row)
    if reference_fingerprint(parameters)!=original:raise AssertionError('caller state changed')
    return {'bank':bank,'pointer_variant':pointer,'fix_rounding_hypothesis':rounding,
        'scenario':scenario_manifest(parameters),
        'scenario_scope':'base template adaptive0; each row explicitly overrides adaptive_enable and the two auxiliary inputs',
        'input_arrays':{
            'music6':array_fingerprint(x,'<f8'),'complex_probe':array_fingerprint(c4,'<f8'),
            'second_b_probe':array_fingerprint(second,'<f8')},'rows':rows,
        'checks':{'upstream_independent_of_aux_and_adaptation':True,
                  'second_b_independent_of_complex_and_adaptation':True,
                  'caller_state_unchanged':True,'extreme_replays':checks},
        'numerical_comparison':numerical_comparison}


def run(*,out,banks=tuple(range(16)),frames=4096,jobs=1):
    out=Path(out)
    if out.exists():raise ValueError('refusing overwrite of existing output')
    if not banks or len(set(banks))!=len(banks) or any(type(b) is not int or b not in range(16) for b in banks):
        raise ValueError('unique explicit EQ selectors0..15 required')
    if type(jobs)is not int or not 1<=jobs<=16:raise ValueError('integer jobs1..16 required')
    probes(frames)
    tools_root=Path(__file__).resolve().parents[1]
    extraction=sorted(set(tools_root.glob('audit_amp_b*.py')) |
                      set(tools_root.glob('extract_amp*.py')) |
                      {tools_root/'inspect_amp_visa.py',tools_root/'audit_amp_output_bindings.py'})
    sources=source_provenance()+[fingerprint(Path(__file__))]+[fingerprint(p) for p in extraction]
    report={'schema':1,'complete':False,'scope':__doc__,'source_code':sources,
        'axes':{'input_eq_banks':list(banks),'pointer_variants':['default','20:3'],
                'fix_rounding':['nearest','floor'],'adaptive_enable':[0,1],
                'complex_aux_enabled':[False,True],'second_b_aux_enabled':[False,True]},
        'frames':frames,'programme_frames':frames-1024,'zero_tail_frames':1024,'jobs':jobs,
        'sample_rate':48000,'expected_rows':len(banks)*32,'completed_rows':0,'contexts':[],
        'claims':{'bounded_grid_complete':False,'all_static_unknowns_closed':False,
            'hardware_bit_exact':False,'actual_vehicle_state_proven':False,
            'acoustic_fidelity_proven':False,'production_preset_recommended':False},
        'remaining_boundaries':['externally selected controls and evolved histories',
            'conditional pointer alias/allocation assumptions, not whole-program absence proof',
            'per-consumer publication scheduling and asynchronous callback lifecycle',
            'SHARC40-bit arithmetic/RECIPS/RSQRTS/flags and exact FFT instruction/store schedule',
            'other math primitives and controls outside the bounded successful mode5 path',
            'runtime gains/limiters and independent auxiliary physical signals',
            'cfg-output to physical wiring and measured speaker/cabin transfer']}
    out.mkdir(parents=True,exist_ok=False)
    def checkpoint():
        temporary=out/'report.json.partial'
        temporary.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
        temporary.replace(out/'report.json')
    checkpoint()
    try:
        def accept(context):
            report['contexts'].append(context)
            report['completed_rows']+=8
            checkpoint()
            print(f"{report['completed_rows']}/{report['expected_rows']} bank={context['bank']} pointer={context['pointer_variant']} FIX={context['fix_rounding_hypothesis']}",flush=True)
        axes=list(itertools.product(banks,('default','20:3'),('nearest','floor')))
        if jobs==1:
            for bank,pointer,rounding in axes:
                accept(evaluate_context(bank=bank,pointer=pointer,rounding=rounding,frames=frames))
        else:
            with ProcessPoolExecutor(max_workers=jobs) as pool:
                futures=[pool.submit(evaluate_context,bank=bank,pointer=pointer,rounding=rounding,frames=frames)
                         for bank,pointer,rounding in axes]
                for future in as_completed(futures):accept(future.result())
        report['contexts'].sort(key=lambda c:axes.index((c['bank'],c['pointer_variant'],c['fix_rounding_hypothesis'])))
        for source in sources:verify(source)
        report['complete']=report['completed_rows']==report['expected_rows']
        report['claims']['bounded_grid_complete']=report['complete']
        checkpoint()
    except Exception as exc:
        report['error']={'type':type(exc).__name__,'message':str(exc)};checkpoint();raise
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--bank',type=int,choices=range(16),action='append')
    parser.add_argument('--frames',type=int,default=4096)
    parser.add_argument('--jobs',type=int,default=1)
    args=parser.parse_args()
    run(out=args.out,banks=tuple(args.bank) if args.bank else tuple(range(16)),frames=args.frames,jobs=args.jobs)
