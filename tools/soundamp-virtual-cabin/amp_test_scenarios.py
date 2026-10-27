"""Reusable EXPLICIT end-to-end diagnostic scenarios.

Not factory/current vehicle presets, acoustic calibration or listening targets.
Builds an explicit neutral-runtime hypothesis without importing test modules.
Unlike the old wiring stress fixture it does not impose low limiter thresholds
or arbitrary pre/post attenuation. Caller must supply independent auxiliary signals
to AmpChain.process; this module never derives microphone/aux PCM from music.
"""
import copy
import hashlib
import json
from pathlib import Path

import numpy as np

from amp_b_adaptive import BandLevelMaskReference, FrameVectorSmoother
from amp_b_bound_scenario import retained_config
from amp_b_pipeline import BSpectralFrameReference, StereoTransientReference
from amp_b_spatial import SpatialFrameReference
from amp_b_tail_binding import build_effective_mode5
from amp_second_b import SecondBAuxReference, load_static_inventory, IMAGE
from amp_second_b_control import load_control_inventory, target_overlay, MASTER
from amp_complex_path import load_calibrated_fir

ROOT=Path(__file__).resolve().parents[2]
AUDIT_DIRECTORY=ROOT/'assets/soundamp/remaining-blocks-20260920-v3'
FACTORY_VERSION='explicit-e2e-diagnostic-v3'
POINTER_VARIANTS=('default','20:3')
SPECTRAL_CHOICES=('neutral_diagnostic','retained_calibration')
NUMERIC_CHOICES=('host_math','firmware_library_nearest','firmware_library_floor')


def make_neutral_diagnostic_spectral():
    """Synthetic unity spectral branches; explicit zero analysis/adaptive state.

This exercises transform/transient/level plumbing but deliberately DISABLES
spatial processing, smoothing and subtraction. It does not reconstruct their
live controls. No arbitrary parameter override mechanism is exposed.
"""
    bins=513;edges=np.linspace(0,bins,75,dtype=int)
    transient_parameters=np.zeros(88)
    transient_parameters[76:78]=1
    transient_parameters[[78,79,82,84,86]]=1
    def transient():
        return StereoTransientReference(bins=bins,edges=edges,params=transient_parameters,
                                        history=np.zeros(296))
    level_parameters=np.zeros(16);level_parameters[14:16]=1
    level_parameters[[2,4,6,8,10]]=1
    def level():
        return BandLevelMaskReference(bins=bins,edges=edges,params=level_parameters,
            power_history=np.zeros(74),db_history=np.zeros(74),gain_history=np.zeros(74))
    spatial_parameters=np.zeros(28)
    spatial_parameters[[4,5,11,13,15,17,19,22,23,25]]=1
    spatial_parameters[9:11]=[.3,2e-6]
    spatial=SpatialFrameReference(bins=bins,edges=edges,params=spatial_parameters,
        eigen_selector=0,balance_enabled=False,gate_selector=0,low_band=0,high_band=73,
        balance_slope=1,balance_offset=0,left_history=np.zeros(74),right_history=np.zeros(74),
        cross_history=np.zeros(74,complex),diffuse_history=np.zeros(74),
        minimum_history=0,mean_history=0,gate_history=0)
    return BSpectralFrameReference(main_transient=transient(),post_transient=transient(),
        level_sum=level(),level_difference=level(),spatial=spatial,
        smooth_left=FrameVectorSmoother(size=256,history_gain=.75,input_gain=.25),
        smooth_right=FrameVectorSmoother(size=256,history_gain=.75,input_gain=.25),
        pre_subtraction=dict(enabled=False,start=.2,end=.2,low_bin=0,high_bin=512,negative_gain=0),
        crossfeed=0,main_selector=0,post_selector=0,main_enabled=True,post_enabled=True,
        level_enabled=True,spatial_enabled=False,smoothing_enabled=False)


def _second_b_parameters(inventory,gain_mode):
    stages={}
    for node in inventory['nodes']:
        handler=node['process_visa_address_candidate']
        channels=node['parameters'].get('channel_count')
        if handler=='0x12879c':
            value=dict(current=[1.]*channels,target=[1.]*channels,mode=0,rates=[[1.,1.]]*channels)
        elif handler=='0x126559':
            value=dict(records=[[0.,0.,1.,0.,0.]],history=np.zeros((3,1,2)).tolist())
        elif handler=='0x126a95':value=dict(counts=[0]*channels,records=[],history=[])
        elif handler=='0x126092':value=dict(record=[0.,0.,1.,0.,0.],history=[0.,0.])
        elif handler=='0x1269bc':value=dict(coefficients=[1.,1.,1.])
        elif handler=='0x127a4d':value=dict(gain=1.)
        elif handler=='0x124517':
            value=dict(samples=[0]*16,history=[[] for _ in range(16)],lifecycle='settled_dma')
        else:raise ValueError(f'unsupported second-B diagnostic inventory handler:{handler}')
        stages[node['descriptor']]=value
    controls=load_control_inventory()
    overlay=target_overlay(controls,source_levels=[0]*3,common_addition=0,
        table1_positions=[0]*3,table2_positions=[0]*3,sub_enabled=True,sub_position=0,
        scalar_filter_selector=0,assume_loader_input_eq_counts=True)
    # This is a declared delivered/settled hypothesis, not target==current proof.
    for descriptor,parameters in overlay['stages'].items():
        stages[descriptor].update(copy.deepcopy(parameters))
    for parameters in stages.values():
        if 'target' in parameters:parameters['current']=parameters['target'].copy()
    for descriptor in ('0xc4b96','0xc4ce8'):
        stages[descriptor]['history']=np.zeros((sum(stages[descriptor]['counts']),2)).tolist()
    stages['0xc4cf9']['history']=[[0.]*n for n in stages['0xc4cf9']['samples']]
    if gain_mode==1:
        for descriptor in MASTER:
            target=stages[descriptor]['target'].copy()
            stages[descriptor]=dict(current=target.copy(),target=target.copy(),mode=1,previous_mode=1,
                previous_target=target.copy(),time_parameters=[4.],scale=.75,steps=[[0.,0.]])
    return ({'label':f'SYNTHETIC E2E DIAGNOSTIC secondB gainmode{gain_mode}; retained/control targets ASSUMED delivered/settled, rawcontrols0; not live/stock',
             'stages':stages},overlay)


def make_test_scenario(*,input_eq_bank,auxiliary_gain_mode=1,first_b_pointer_variant='default',
                       adaptive_enable=0,first_b_spectral='neutral_diagnostic',
                       first_b_numerics='host_math'):
    """Return AmpChain kwargs for a labelled diagnostic, never a car preset.

input_eq_bank is an explicit0..15 hypothesis, not the MOST bank1 selector.
auxiliary_gain_mode0 selects direct targets;1 selects three synthetic settled
mode1 controls (no fade). Targets come from retained/static writers with explicit
rawcontrols0 and an assumed completed delivery. Historical neutral first-B uses
16:default pointers with supplied mode5, a wiring diagnostic NOT a delivered
mode-control history. Retained spectral uses API20=1/3 THEN API16=5, consistently
binding mode5 pointers, with delivery/settling explicitly assumed. Retained B01
calibration bank1 is independent
of all those selectors. adaptive_enable0 gives an explicit disabled-adaptation
full-mode hypothesis;1 is an explicit auxiliary-probe sensitivity scenario.

Runtime gains1 and limiter threshold100/initial1 are synthetic neutral-runtime
choices, NOT listening recommendations or reconstructed amplifier thresholds.
"""
    if type(input_eq_bank) is not int or input_eq_bank not in range(16):
        raise ValueError('explicit integer input EQ bank0..15 required')
    if type(auxiliary_gain_mode) is not int or auxiliary_gain_mode not in (0,1):
        raise ValueError('diagnostic auxiliary gain mode must be integer0 or1')
    if type(adaptive_enable) is not int or adaptive_enable not in (0,1):
        raise ValueError('explicit diagnostic adaptive enable must be integer0 or1')
    if type(first_b_pointer_variant) is not str or first_b_pointer_variant not in POINTER_VARIANTS:
        raise ValueError("first-B pointer variant must be 'default' or '20:3'")
    if type(first_b_spectral) is not str or first_b_spectral not in SPECTRAL_CHOICES:
        raise ValueError('explicit neutral_diagnostic or retained_calibration spectral choice required')
    if type(first_b_numerics) is not str or first_b_numerics not in NUMERIC_CHOICES:
        raise ValueError('known explicit first-B numerical reference required')
    if first_b_numerics!='host_math' and first_b_spectral!='retained_calibration':
        raise ValueError('firmware numerical references require retained calibration')
    files={name:AUDIT_DIRECTORY/name for name in ('b_config_control.json','b_mode_control.json','audit.json')}
    config=json.loads(files['b_config_control.json'].read_text())
    mode=json.loads(files['b_mode_control.json'].read_text())
    audit=json.loads(files['audit.json'].read_text())
    cfg=retained_config(config,source='b01',bank=1)
    mode20='20:default' if first_b_pointer_variant=='default' else first_b_pointer_variant
    retained=first_b_spectral=='retained_calibration'
    mode16='16:5' if retained else '16:default'
    pointers={int(k):v for name in (mode20,mode16) for k,v in mode['pointer_maps'][name].items()}
    spectral_binding=None
    if retained:
        from amp_b_spectral_binding import (load_spectral_calibration,bind_spectral_parameters,
                                            build_spectral_reference)
        from amp_b_spatial_binding import build_effective_spatial
        calibration=load_spectral_calibration(source='b01')
        bound_spectral=bind_spectral_parameters(calibration,effective_config=cfg,pointers=pointers)
        spatial,bound_spatial=build_effective_spatial(effective_config=cfg,pointers=pointers,
            edges=calibration['edges'],assume_successful_fresh_initialization=True)
        spectral=build_spectral_reference(bound_spectral,spatial=spatial,
            spatial_enabled=bound_spatial['enabled'],zero_histories=True,
            numeric_mode='host_math' if first_b_numerics=='host_math' else 'firmware_library',
            fix_rounding=None if first_b_numerics=='host_math' else first_b_numerics.rsplit('_',1)[1])
        spectral.binding_metadata['spatial']=copy.deepcopy(bound_spatial['metadata'])
        spectral_binding=copy.deepcopy(spectral.binding_metadata)
        input_mix_gain=bound_spectral['input_mix_gain']
    else:
        spectral=make_neutral_diagnostic_spectral()
        input_mix_gain=.5
    first=build_effective_mode5(effective_config=cfg,pointers=pointers,library_mode=5,
        assume_successful_settled_allocations=True,zero_histories=True,
        spectral=spectral,input_mix_gain=input_mix_gain)
    inventory=load_static_inventory()
    second_state,second_overlay=_second_b_parameters(inventory,auxiliary_gain_mode)
    second=SecondBAuxReference(scenario=second_state,inventory=inventory)
    complex_profile=audit['complex_control_profiles'][0]
    initial_fir=load_calibrated_fir(IMAGE,complex_profile,audit['source_sha256'])
    manifest={'factory':FACTORY_VERSION,'diagnostic_only':True,'actual_vehicle_state_proven':False,
        'subjective_or_acoustic_optimality_proven':False,'input_eq_bank_hypothesis':input_eq_bank,
        'first_b_retained_source':'b01','first_b_calibration_bank':1,'first_b_library_mode':5,
        'input_mix_gain_object97_hypothesis':input_mix_gain,
        'input_mix_gain_origin':'retained B01 common header43 -> object97' if retained else 'supplied diagnostic0.5',
        'first_b_pointer_maps':[mode20,mode16],'auxiliary_gain_mode':auxiliary_gain_mode,
        'first_b_spectral_choice':first_b_spectral,
        'first_b_numerical_reference':first_b_numerics,
        'first_b_mode_control_history':(
            {'apply_order':[[20,1 if mode20=='20:default' else 3],[16,5]],
             'delivery_and_effective_snapshot_assumed':True,'full_replay_or_transition_simulated':False}
            if retained else {'kind':'historical synthetic mode5/16:default wiring fixture; no delivered-state claim'}),
        'adaptive_enable':adaptive_enable,
        'common_input_mapping_hypothesis':[0,1,2,3,4,5,None,None],'common_input_delay_samples':0,
        'wrapper_delay_samples':1024,'wrapper_assumption':'steady on-time successful callbacks; initial banks zero',
        'spectral':('retained B01 common-header and effective-config spectral controls, nonuniform74-band map, successful fresh initialization; not live'
                    if retained else 'SYNTHETIC neutral branch diagnostic; not bound to retained spectral controls'),
        'spectral_binding':spectral_binding,
        'disabled_spectral_features':[name for name,enabled in (
            ('spatial',spectral.spatial_enabled),('smoothing',spectral.smoothing_enabled),
            ('pre_subtraction',spectral.pre_subtraction['enabled']),
            ('main_transient',spectral.main_enabled),('post_transient',spectral.post_enabled),
            ('level',spectral.level_enabled)) if not enabled],
        'initial_histories':('spectral successful constructor: spatial gate1, other histories zero, smoother first-call prime; '
                             if retained else 'fresh zero spectral histories; ')+
                            'zero tail/EQ/FIR histories; secondB current/target/prior at chosen writer targets, zero mode1 steps',
        'complex':f'profile0 static coefficients and retained INITIAL FIR (not live learned state); SYNTHETIC enable{adaptive_enable}, sideband0, zero power offsets/floor-200',
        'complex_initial_fir':{'image_path':str(IMAGE),'image_sha256':audit['source_sha256'],
                               'bank_sha256':complex_profile['fir_bank_AAA60']['sha256'],
                               'hypothesis':'retained profile0 INITIAL coefficient bank, not current learned FIR'},
        'second_b':'retained/static writer coefficients plus explicit rawcontrols0; targets ASSUMED delivered and settled, independent graph8/9/10 PCM required',
        'second_b_control_inputs':copy.deepcopy(second_overlay['control_inputs']),
        'second_b_target_tenths_db':copy.deepcopy(second_overlay['target_tenths_db']),
        'second_b_state_assumptions':{'chosen_targets_delivered_and_settled':True,
            'current_equals_target':True,'zero_filter_and_delay_histories':True,
            'master_mode':auxiliary_gain_mode,'other_gain_modes':0,
            'input_eq_counts':'loader8/4/4 explicitly assumed, not live verified'},
        'auxiliary_input_requirement':'caller supplies post-C4D11 graph11 and graph8/9/10; never inferred from music',
        'runtime':'SYNTHETIC neutral-runtime pre/post gains1; two independent threshold100/initial1 limiters, not hardware bounds',
        'unknowns':['active input EQ bank and mapping',
                    ('actual first-B common-header updates and effective controls; selected retained coefficients now bound'
                     if retained else 'actual first-B spectral controls/object97 gain'),
                    'effective current tail/pointer state','actual B callback timing/history',
                    'actual second-B filters/gains/matrices/delays and physical sources',
                    'C4D17 mode/FIR/powers/physical auxiliary','runtime gains and limiter parameters',
                    'speaker transfer functions and cabin acoustics'],
        'source_files':{name:{'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
                        for name,path in files.items()},
        'factory_source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    first.tail_binding_metadata['diagnostic_test_scenario']=copy.deepcopy(manifest)
    limiter=dict(threshold=[100.]*16,attack=[.5]*16,release=[1.001]*16,
                 minimum=1e-6,maximum=1,initial=1,linked=False)
    complex_scenario=dict(profile=complex_profile,fir_coefficients=initial_fir,
        adaptive_enable=adaptive_enable,sideband=0,power_x_db=[0]*5,power_y_db=[0]*5,
        power_floor_db=[-200]*5,rounding='nearest')
    return dict(upstream_scenario=dict(audit_path=str(files['audit.json']),
            graph_input_sources=[0,1,2,3,4,5,None,None],eq_family='tone',
            shared_gains_tenths_db=[0]*4,bank_index=input_eq_bank,delay_samples=0),
        first_b_scenario=dict(reference=first,wrapper_delay_samples=1024,
            label=(f'CONDITIONAL retained B01 spectral + bank1 tails; {mode20} THEN {mode16}; successful fresh spectral init; on-time wrapper'
                   if retained else f'SYNTHETIC E2E DIAGNOSTIC neutral spectral/controls + retained B01bank1 tails; {mode20}/{mode16}; zero histories; on-time wrapper')),
        second_b_scenario=dict(reference=second,
            label=f'SYNTHETIC E2E DIAGNOSTIC independent aux8/9/10, gainmode{auxiliary_gain_mode}; not live/stock'),
        complex_block='full_mode_reference',complex_scenario=complex_scenario,
        runtime=dict(pre_complex_gain=[1.]*10,post_complex_gain=[1.]*10,
                     limiters=[limiter,copy.deepcopy(limiter)]))


def scenario_manifest(scenario):
    """Copy provenance from a factory scenario without changing its histories."""
    try:
        result=scenario['first_b_scenario']['reference'].tail_binding_metadata['diagnostic_test_scenario']
    except (KeyError,AttributeError,TypeError) as exc:
        raise ValueError('scenario from make_test_scenario required') from exc
    if result.get('factory')!=FACTORY_VERSION:raise ValueError('unknown diagnostic factory version')
    return copy.deepcopy(result)
