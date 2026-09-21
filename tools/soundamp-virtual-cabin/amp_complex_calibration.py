"""C4D17 fixed mode2 calibration composition with explicit initial state.

Stages1/3/4/5/7/10/12/13/14/15; no full-mode delay, five-band gain,
power-history controller, or inferred microphone. Input auxiliary is AFTER
C4D11 scalar. This mathematical reference is not live-state/hardware proof.
"""
import copy
import argparse
import hashlib
import json
from pathlib import Path
import sys

import numpy as np

from amp_complex_control import AdaptiveResidual
from amp_complex_path import StatefulSOS, biquad_sos
from amp_complex_timing import CalibrationExcitationReference


def _state(value, shape, name):
    result=np.array(value,dtype=float,copy=True)
    if result.shape!=shape or not np.isfinite(result).all():
        raise ValueError(f'explicit finite {name} with shape{shape} required')
    return result


class ComplexCalibrationReference:
    """Already enabled/bound mode2, fixed profile and64-frame scheduler.

All histories are mandatory. In particular scratch_graph96 retains a64-word
buffer: mode2 skips stage6, so the sixth packed pair's second lane is NOT a
new music sum or implicit zero. Stage13 overwrites its0..43/48..51 words,
stage14 writes44..47;52..63 persist. This stale lane does not feed mode2 LMS.

Configure clears FIR history only and sets reference weights to gate*stored
weights and auxiliary weight to field29. It does NOT clear coefficients,
power history, prefilters, auxiliary filters, generator state, or graph96.
"""
    def __init__(self, *, profile, excitation, fir_coefficients,
                 initial_fir_history, initial_power_state,
                 initial_reference_gains, initial_auxiliary_gain,
                 auxiliary_filter_state, prefilter_state, scratch_graph96,
                 rounding, detector_stride):
        if not isinstance(excitation,CalibrationExcitationReference):
            raise ValueError('explicit calibration excitation reference required')
        if type(detector_stride) is not int or detector_stride!=16:
            raise ValueError('audited descriptor+12 detector stride16 required')
        self.profile=copy.deepcopy(profile)
        p=self.profile;d=p['detector_layout']
        if (d['audio_frames'],d['detector_frames'])!=(64,4):
            raise ValueError('audited64/4 scheduler/detector layout required')
        self.fields={v['descriptor_offset']:v['calibrated'] for v in p['controls_AAC88']}
        self.excitation=copy.deepcopy(excitation)
        self.reference_gains=_state(d['reference_gains'],(10,),'stored reference gains')
        self.detector_stride=detector_stride
        self.aux_eq=StatefulSOS([biquad_sos(v['coefficients'])
            for v in p['detector_biquad_banks_AAB44'][:2]],1)
        self.aux_eq.state=_state(auxiliary_filter_state,(2,2,1),'auxiliary SOS state')
        self.prefilter=StatefulSOS([[1,b1,b2,1,-a1,-a2]
            for a2,a1,b2,b1 in d['prefilter_coefficients']],12)
        self.prefilter.state=_state(prefilter_state,(8,2,12),'prefilter SOS state')
        self.prefilter_scale=float(d['prefilter_scale'])
        if not np.isfinite(self.prefilter_scale):raise ValueError('finite prefilter scale required')
        self.scratch=_state(scratch_graph96,(64,),'graph96 scratch')
        self.residual=AdaptiveResidual(fir_coefficients,
            reference_gains=initial_reference_gains, auxiliary_gain=initial_auxiliary_gain,
            step=self.fields[28],rounding=rounding,power_state=initial_power_state)
        self.residual.history=_state(initial_fir_history,(10,512),'canonical FIR history')
        self.calls=0;self.learning_calls=0

    def process_block(self, auxiliary):
        """One explicit64-sample auxiliary block; no music input is read by mode2."""
        auxiliary=_state(auxiliary,(64,),'post-C4D11 auxiliary')
        result=self.excitation.process_block()
        action=result['sequence'];self.calls+=1
        result['scope']='fixed mode2 mathematical reference, not live calibration or acoustic inference'
        if action.get('reset_fir_history'):
            self.residual.history.fill(0)
            gates=np.array([(action['reference_gate_bits']>>i)&1 for i in range(10)])
            self.residual.reference_gains=self.reference_gains*gates
            self.residual.auxiliary_gain=float(self.fields[29])
        if action['action']!='generate_and_learn':
            return {**result,'stages_executed':[1,3],'detector_references':None,
                    'detector_auxiliary':None,'residual':None,'target_update':None}
        # Stage5, then stage7 interleaving. Stage6 weighted music sum is absent.
        aux=auxiliary if self.fields[36]&1 else self.aux_eq.process(auxiliary[:,None])[:,0]
        packed=np.column_stack([result['audio'],aux,self.scratch.copy()])
        # Stage10 copies music to outputs BEFORE stage12 mutates packed scratch.
        low=self.prefilter.process(packed*self.prefilter_scale)
        # 1273A8..1273C5 in-place compaction precedes stage13 unpack:
        # four SIMD pair loads with M1=2*descriptor12=32 words.
        detector=low[::self.detector_stride][:4]
        refs,aux_low,stale_low=detector[:,:10],detector[:,10],detector[:,11]
        self.scratch[:4]=aux_low
        self.scratch[4:44]=refs.T.reshape(40)
        self.scratch[48:52]=stale_low
        residual=self.residual.process(refs,aux_low)
        self.scratch[44:48]=residual
        self.learning_calls+=1
        return {**result,'stages_executed':[1,3,4,5,7,10,12,13,14,15],
                'filtered_auxiliary':aux,'detector_references':refs.copy(),
                'detector_auxiliary':aux_low.copy(),'unused_detector_scratch_lane':stale_low.copy(),
                'residual':residual,'target_update':None}

    def metadata(self):
        return {'model':'c4d17-fixed-calibration-mode2-reference-v1',
                'profile_index':self.profile['profile_index'],'stage_mask':'0xf4ba',
                'profile_sha256':hashlib.sha256(json.dumps(self.profile,sort_keys=True).encode()).hexdigest(),
                'detector_sample_indices':[0,16,32,48],'audio_delay_stage8_active':False,
                'calls':self.calls,'learning_calls':self.learning_calls,
                'warning':'Explicit state, already enabled/bound, no live mode/physical auxiliary/SPL/SHARC-rounding proof; never enabled in default music AmpChain.'}


def main():
    from amp_complex_modes import CalibrationSequence
    from amp_complex_path import load_calibrated_fir
    from amp_complex_timing import ROOT,LOADER,load_builtin_records,CalibrationShapedNoise
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('--audit',type=Path,default=ROOT/'output/r2/soundamp/remaining-blocks-20260919/audit.json')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    audit=json.loads(args.audit.read_text());p=audit['complex_control_profiles'][0]
    fir=load_calibrated_fir(args.image,p,audit['source_sha256'])
    builtins=load_builtin_records()
    core_state=dict(seed=31,shuffle=list(range(1,33)),previous=0,alusat=False)
    generator=CalibrationShapedNoise(noise_type=1,core_state=core_state,
        builtin_records=builtins,custom_records=None,initial_filter_state=np.zeros((2,2)))
    excitation=CalibrationExcitationReference(sequence=CalibrationSequence(1023,64,128),
        generator=generator,excitation_gain=.1)
    path=ComplexCalibrationReference(profile=p,excitation=excitation,fir_coefficients=fir,
        initial_fir_history=np.zeros((10,512)),initial_power_state=0,
        initial_reference_gains=np.zeros(10),initial_auxiliary_gain=0,
        auxiliary_filter_state=np.zeros((2,2,1)),prefilter_state=np.zeros((8,2,12)),
        scratch_graph96=np.zeros(64),rounding='nearest',detector_stride=16)
    trace=[]
    for block in range(42):
        aux=.01*np.sin(2*np.pi*63*np.arange(block*64,(block+1)*64)/48000)
        result=path.process_block(aux)
        trace.append({'block':block,'sequence':result['sequence'],
                      'audio_peak':float(np.max(abs(result['audio']))),
                      'residual_peak':None if result['residual'] is None else float(np.max(abs(result['residual'])))})
    sys.path.insert(0,str(ROOT/'tools'))
    from inspect_amp_visa import load_shortwords,listing
    mem=load_shortwords(LOADER.read_bytes())
    evidence={}
    for name,start,end in [('configure',0x1270BB,0x12716C),
                           ('binding',0x126DF0,0x126FE4),
                           ('mode2_data_path',0x12716C,0x127439)]:
        lines=list(listing(mem,start,end))
        if any('UNKNOWN' in v for v in lines):raise ValueError('unresolved calibration instruction')
        evidence[name]={'start_shortword':hex(start),'end_exclusive_shortword':hex(end),
                        'count':len(lines),'sha256':hashlib.sha256(('\n'.join(lines)+'\n').encode()).hexdigest()}
    report={'scope':'synthetic all-ten-channel mode2 composition, NOT vehicle calibration or cabin identification',
            'source_sha256':audit['source_sha256'],'loader_sha256':hashlib.sha256(LOADER.read_bytes()).hexdigest(),
            'audit_sha256':hashlib.sha256(args.audit.read_bytes()).hexdigest(),
            'reference_source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'model':path.metadata(),'sequence_initial':{'channel_mask':1023,'wait_samples':64,'measure_samples':128},
            'synthetic_generator':{'type':1,'core_state':core_state,'excitation_gain':.1},
            'other_initial_states':'auxiliary/prefilter/noise filters,FIR history,power,reference gains,aux gain andgraph96 allzero; audited profile0 FIR retained',
            'auxiliary':'explicit post-C4D11 .01-amplitude63Hz tone, no cabin-derived signal',
            'fir_initial_sha256':hashlib.sha256(fir.astype('<f4').tobytes()).hexdigest(),
            'fir_delta_l2':float(np.linalg.norm(path.residual.coefficients-fir)),
            'complete':bool(path.excitation.sequence.complete),'trace':trace,'instruction_evidence':evidence}
    encoded=json.dumps(report,indent=2,allow_nan=False)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(encoded)
        print(json.dumps({'output':str(args.output),'complete':report['complete'],
                          'learning_calls':path.learning_calls,'fir_delta_l2':report['fir_delta_l2']}))
    else:print(encoded,end='')


if __name__=='__main__':main()
