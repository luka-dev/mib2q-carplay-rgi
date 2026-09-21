import copy
import json
import sys
import unittest

import numpy as np

from amp_complex_calibration import ComplexCalibrationReference
from amp_complex_modes import CalibrationSequence
from amp_complex_timing import CalibrationExcitationReference, CalibrationShapedNoise, ROOT, LOADER
from amp_complex_path import ComplexFullModeReference


def profile():
    return json.loads((ROOT/'output/r2/soundamp/remaining-blocks-20260919/audit.json').read_text())['complex_control_profiles'][0]


def state_args(p=None,seq=None):
    p=profile() if p is None else p
    generator=CalibrationShapedNoise(noise_type=0,
        core_state=dict(seed=31,shuffle=list(range(1,33)),previous=0,alusat=False),
        builtin_records=None,custom_records=None,initial_filter_state=np.zeros((2,2)))
    excitation=CalibrationExcitationReference(
        sequence=CalibrationSequence(1,64,128) if seq is None else seq,
        generator=generator,excitation_gain=.1)
    return dict(profile=p,excitation=excitation,fir_coefficients=np.zeros((10,512)),
        initial_fir_history=np.zeros((10,512)),initial_power_state=0.,
        initial_reference_gains=np.ones(10),initial_auxiliary_gain=1.,
        auxiliary_filter_state=np.zeros((2,2,1)),prefilter_state=np.zeros((8,2,12)),
        scratch_graph96=np.zeros(64),rounding='nearest',detector_stride=16)


def running_sequence(gate=1):
    return CalibrationSequence(gate,0,640,phase=2,selected_bit=gate,active_gates=gate)


def identity_detector_profile():
    p=profile();p['detector_layout']['prefilter_coefficients']=[[0.,0.,0.,0.]]*8
    p['detector_layout']['prefilter_scale']=1.
    for record in p['detector_biquad_banks_AAB44'][:2]:record['coefficients']=[0.,0.,1.,0.,0.]
    for v in p['controls_AAC88']:
        if v['descriptor_offset']==28:v['calibrated']=.02
    return p


def literal_pair_compact(audio,stride):
    """Independent scalar memory copy of four SIMD pair loads/stores."""
    scratch=audio.copy().reshape(-1)
    read=write=0
    for _ in range(4):
        r,s=scratch[read],scratch[read+1]
        scratch[write],scratch[write+1]=r,s
        read+=2*stride;write+=2
    return scratch[:8].reshape(4,2)


class CalibrationPathTests(unittest.TestCase):
    def test_loader_stride_and_in_place_alias_order(self):
        sys.path.insert(0,str(ROOT/'tools'))
        from audit_amp_output_bindings import loader_dm
        from extract_amp_sharc_app import parse_blocks
        from inspect_amp_visa import load_shortwords,listing
        blob=LOADER.read_bytes();mem=load_shortwords(blob);dm=loader_dm(blob,parse_blocks(blob))
        self.assertEqual(dm[0xC4D17+12],16)
        self.assertEqual(dm[0xC4D17+13],4)
        self.assertEqual(dm[0xC4F92+9],64)
        lines='\n'.join(listing(mem,0x1273A8,0x1273CB))
        for statement in ('R0=DM(+12,I0)','R0+=R0','M1=R0','R0=DM(I2,M1)','DM(I3,M0)=R0'):
            self.assertIn(statement,lines)
        stereo=np.arange(128.).reshape(64,2)
        np.testing.assert_array_equal(literal_pair_compact(stereo,dm[0xC4D17+12]),stereo[[0,16,32,48]])
        # The destination overlaps the source, but none of the future32-word
        # spaced reads were overwritten by prior contiguous2-word writes.
        self.assertFalse(np.array_equal(literal_pair_compact(stereo,16),stereo[:4]))

    def test_mode2_output_bypasses_prefilter_delay_and_controller(self):
        args=state_args(seq=running_sequence())
        expected=copy.deepcopy(args['excitation']).process_block()['audio']
        path=ComplexCalibrationReference(**args)
        r=path.process_block(np.arange(64.)*.001)
        np.testing.assert_array_equal(r['audio'],expected)
        self.assertGreater(np.max(abs(r['audio'])),0)
        self.assertEqual(r['stages_executed'],[1,3,4,5,7,10,12,13,14,15])
        self.assertIsNone(r['target_update'])
        self.assertFalse(path.metadata()['audio_delay_stage8_active'])

    def test_configure_clears_only_history_preserving_coefficients_and_other_states(self):
        args=state_args();args['fir_coefficients'][:]=.01;args['initial_fir_history'][:]=.3
        args['initial_power_state']=3.;args['prefilter_state'][:]=.4
        args['auxiliary_filter_state'][:]=.5;args['scratch_graph96'][:]=.6
        path=ComplexCalibrationReference(**args);seed=path.excitation.generator.core.seed
        r=path.process_block(np.ones(64))
        self.assertEqual(r['sequence']['reason'],'configure')
        np.testing.assert_array_equal(path.residual.history,0)
        np.testing.assert_array_equal(path.residual.coefficients,.01)
        np.testing.assert_array_equal(path.prefilter.state,.4)
        np.testing.assert_array_equal(path.aux_eq.state,.5)
        np.testing.assert_array_equal(path.scratch,.6)
        self.assertEqual(path.residual.power_state,3.)
        self.assertEqual(path.excitation.generator.core.seed,seed)
        self.assertEqual(path.residual.reference_gains[0],path.reference_gains[0])
        np.testing.assert_array_equal(path.residual.reference_gains[1:],0)
        self.assertEqual(path.residual.auxiliary_gain,path.fields[29])
        r=path.process_block(np.ones(64))
        self.assertEqual(r['sequence']['reason'],'wait')
        np.testing.assert_array_equal(path.residual.history,0)

    def test_independent_lms_and_graph96_planar_binding(self):
        rng=np.random.default_rng(75)
        p=identity_detector_profile();args=state_args(p,running_sequence(4))
        args['initial_fir_history']=rng.normal(0,.01,(10,512))
        args['fir_coefficients']=rng.normal(0,.002,(10,512))
        args['initial_reference_gains']=np.arange(1,11)/10
        args['initial_auxiliary_gain']=.7
        args['scratch_graph96']=np.arange(64.)/100
        expected_h=args['initial_fir_history'].copy();weights=args['fir_coefficients'].copy()
        expected_audio=copy.deepcopy(args['excitation']).process_block()['audio']
        aux=np.arange(64.)/1000
        refs=expected_audio[[0,16,32,48]];x=refs*args['initial_reference_gains']
        expected=[]
        for i in range(4):
            # Independent global ring addressing rather than production'sshift.
            expected_h=np.concatenate([x[i,:,None],expected_h[:,:511]],axis=1)
            err=.7*aux[16*i]-float(np.dot(weights.reshape(-1),expected_h.reshape(-1)))
            weights+=.02*err*expected_h;expected.append(err)
        path=ComplexCalibrationReference(**args);r=path.process_block(aux)
        np.testing.assert_array_equal(r['detector_references'],refs)
        np.testing.assert_array_equal(r['detector_auxiliary'],aux[::16])
        np.testing.assert_allclose(r['residual'],expected,atol=1e-15)
        np.testing.assert_allclose(path.residual.coefficients,weights,atol=1e-15)
        np.testing.assert_array_equal(path.scratch[:4],aux[::16])
        np.testing.assert_array_equal(path.scratch[4:44],refs.T.reshape(40))
        np.testing.assert_allclose(path.scratch[44:48],expected,atol=1e-15)
        np.testing.assert_array_equal(path.scratch[48:52],args['scratch_graph96'][::16])
        np.testing.assert_array_equal(path.scratch[52:],args['scratch_graph96'][52:])

    def test_stale_sum_lane_does_not_become_music_or_affect_lms(self):
        a=state_args(seq=running_sequence());b=copy.deepcopy(a)
        a['scratch_graph96'][:]=100.;b['scratch_graph96'][:]=-100.
        pa,pb=ComplexCalibrationReference(**a),ComplexCalibrationReference(**b)
        for _ in range(3):
            ra,rb=pa.process_block(np.ones(64)*.01),pb.process_block(np.ones(64)*.01)
            np.testing.assert_array_equal(ra['audio'],rb['audio'])
            np.testing.assert_array_equal(ra['residual'],rb['residual'])
            np.testing.assert_array_equal(pa.residual.coefficients,pb.residual.coefficients)
        self.assertFalse(np.array_equal(ra['unused_detector_scratch_lane'],rb['unused_detector_scratch_lane']))

    def test_prefilter_independent_scalar_sections_and_decimation(self):
        args=state_args(seq=running_sequence());rng=np.random.default_rng(76)
        args['prefilter_state']=rng.normal(0,1e-12,(8,2,12))
        path=ComplexCalibrationReference(**args)
        audio=copy.deepcopy(args['excitation']).process_block()['audio'];aux=np.sin(np.arange(64.)/8)*.01
        # Stored auxiliary records are identity; scalar SOS cascade independently.
        for record in args['profile']['detector_biquad_banks_AAB44'][:2]:
            self.assertEqual(record['coefficients'],[0.,0.,1.,0.,0.])
        low=np.column_stack([audio,aux,np.zeros(64)])*path.prefilter_scale
        state=args['prefilter_state'].copy()
        for section,(a2,a1,b2,b1) in enumerate(args['profile']['detector_layout']['prefilter_coefficients']):
            z1,z2=state[section]
            for i,x in enumerate(low.copy()):
                y=x+z1
                z1,z2=b1*x+a1*y+z2,b2*x+a2*y
                low[i]=y
            state[section]=z1,z2
        r=path.process_block(aux)
        np.testing.assert_allclose(r['detector_references'],low[::16,:10],atol=1e-24)
        np.testing.assert_allclose(r['detector_auxiliary'],low[::16,10],atol=1e-24)
        np.testing.assert_allclose(path.prefilter.state,state,atol=1e-24)

    def test_completion_last_block_learns_then_no_state_advancement(self):
        args=state_args(identity_detector_profile(),CalibrationSequence(512,0,64,phase=2,
            selected_bit=512,active_gates=512))
        path=ComplexCalibrationReference(**args)
        path.process_block(np.ones(64)*.03)
        last=path.process_block(np.ones(64)*.03)
        self.assertTrue(path.excitation.sequence.complete)
        self.assertIsNotNone(last['residual']);self.assertEqual(path.learning_calls,2)
        weights=path.residual.coefficients.copy();scratch=path.scratch.copy()
        done=path.process_block(np.ones(64))
        self.assertEqual(done['sequence']['reason'],'complete')
        self.assertIsNone(done['residual']);self.assertEqual(path.learning_calls,2)
        np.testing.assert_array_equal(path.residual.coefficients,weights)
        np.testing.assert_array_equal(path.scratch,scratch)

    def test_auxiliary_bypass_preserves_filter_history(self):
        args=state_args(identity_detector_profile(),running_sequence())
        for item in args['profile']['controls_AAC88']:
            if item['descriptor_offset']==36:item['calibrated']=1
        args['auxiliary_filter_state'][:]=.7
        path=ComplexCalibrationReference(**args);aux=np.arange(64.)/100
        r=path.process_block(aux)
        np.testing.assert_array_equal(r['filtered_auxiliary'],aux)
        np.testing.assert_array_equal(path.aux_eq.state,.7)

    def test_reject_unknown_shapes_stride_and_nonfinite_input_before_advancement(self):
        for key,value in [('detector_stride',1),('scratch_graph96',np.zeros(63)),
                           ('initial_fir_history',np.zeros((10,511))),('initial_power_state',-1)]:
            args=state_args();args[key]=value
            with self.assertRaises(ValueError):ComplexCalibrationReference(**args)
        path=ComplexCalibrationReference(**state_args())
        with self.assertRaises(ValueError):path.process_block(np.full(64,np.nan))
        self.assertEqual(path.calls,0)
        self.assertEqual(path.excitation.sequence.phase,4)


if __name__=='__main__':unittest.main()
