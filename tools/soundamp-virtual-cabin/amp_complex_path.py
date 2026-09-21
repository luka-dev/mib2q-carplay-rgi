"""C4D17 fixed full-mode, zero-state reference composition.

This is an explicit SCENARIO, not playback readback or a complete amplifier.
Only control mode4 is reconstructed here; calibration/noise-test mode2 and
startup's first binding-only call are excluded. Float64/scipy math is not a
bit-exact SHARC emulator. Port11 must be supplied; it is never invented from
music. Stage12 compacts four stride16 detector frames before stage13 unpacks.
"""
import copy
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.signal import sosfilt

from amp_complex_control import AdaptiveResidual, ComplexControl, shape_powers
from amp_dynamic import ComplexFiveBand, ComplexStereoDelay


def load_calibrated_fir(image, profile, expected_image_sha256):
    """Read the audited initial FIR bank; reject mismatched firmware/bank."""
    data=Path(image).read_bytes()
    if hashlib.sha256(data).hexdigest()!=expected_image_sha256:
        raise ValueError('CodeFlash SHA-256 differs from the parameter audit')
    bank=profile['fir_bank_AAA60']
    start=int(bank['file_offset'],16)
    raw=data[start:start+10*512*4]
    if bank['shape']!=[10,512] or len(raw)!=10*512*4 or hashlib.sha256(raw).hexdigest()!=bank['sha256']:
        raise ValueError('FIR bank differs from the parameter audit')
    return np.frombuffer(raw,dtype='<f4').astype(float).reshape(10,512)


def biquad_sos(record):
    """126092/12610B: [-a1,-a2,b0,b1,b2]."""
    a,b,c,d,e=record
    return [c,d,e,1,-a,-b]


class StatefulSOS:
    def __init__(self, sos, channels):
        self.sos=np.asarray(sos,dtype=float)
        if self.sos.ndim!=2 or self.sos.shape[1]!=6 or not np.isfinite(self.sos).all():
            raise ValueError('finite SOS array required')
        self.state=np.zeros((len(self.sos),2,channels))

    def process(self, audio):
        out,self.state=sosfilt(self.sos,audio,axis=0,zi=self.state)
        return out


class PowerHistory:
    """Stages17..19, pair lane0=residual and lane1=music sum.

    Return (x=music, y=residual) only at the end of each complete window.
    The reductions are over per-block energies, NOT the maximum sample.
    """
    def __init__(self, blocks, *, maximum, sum_scale, max_scale):
        if not isinstance(blocks,int) or blocks<1: raise ValueError('positive block count required')
        if not np.isfinite([sum_scale,max_scale]).all(): raise ValueError('finite scales required')
        self.data=np.zeros((blocks,5,2))
        self.position=0
        self.maximum=bool(maximum)
        self.sum_scale=sum_scale
        self.max_scale=max_scale

    def push(self, bands):
        bands=np.asarray(bands,dtype=float)
        if bands.ndim!=3 or bands.shape[1:]!=(5,2) or not len(bands) or not np.isfinite(bands).all():
            raise ValueError('finite frames x 5 bands x 2 lanes required')
        self.data[self.position]=np.sum(bands*bands,axis=0)
        self.position+=1
        if self.position<len(self.data): return None
        self.position=0
        p=self.data.max(axis=0)*self.max_scale if self.maximum else self.data.sum(axis=0)*self.sum_scale
        return p[:,1].copy(),p[:,0].copy()


class ComplexFullModeReference:
    """Supply one audited profile and explicit runtime choices; 64-frame calls.

    music is ten inputs at C4D17's boundary; auxiliary is graph11 at this
    boundary, AFTER upstream scalar gain C4D11. Model uses all stage kernels
    in the full mask, but no claim about live profile/mask/powers is made.
    The physical meaning and scale of auxiliary graph11 are unverified.
    """
    def __init__(self, profile, fir_coefficients, *, adaptive_enable, sideband,
                 power_x_db, power_y_db, power_floor_db, rounding):
        self.profile=copy.deepcopy(profile)
        self.profile_sha256=hashlib.sha256(json.dumps(self.profile,sort_keys=True).encode()).hexdigest()
        p=self.profile
        d=p['detector_layout']
        if (d['audio_frames'],d['detector_frames'],d['history_blocks'])!=(64,4,16):
            raise ValueError('only audited 64/4/16 layout supported')
        if sideband not in (0,1) or adaptive_enable not in (0,1):
            raise ValueError('explicit binary sideband/enable required')
        self.sideband=sideband
        self.fields={c['descriptor_offset']:c['calibrated'] for c in p['controls_AAC88']}
        f=self.fields
        self.power_x_db=np.broadcast_to(np.asarray(power_x_db,dtype=float),(5,)).copy()
        self.power_y_db=np.broadcast_to(np.asarray(power_y_db,dtype=float),(5,)).copy()
        self.power_floor_db=np.broadcast_to(np.asarray(power_floor_db,dtype=float),(5,)).copy()
        if not np.isfinite([self.power_x_db,self.power_y_db,self.power_floor_db]).all():
            raise ValueError('finite explicit power offsets required')
        self.reference_gains=np.asarray(d['reference_gains'],dtype=float)
        self.delay=ComplexStereoDelay([v['requested'] for v in p['stage8_loader_delays']],
                                      [v['maximum'] for v in p['stage8_loader_delays']])
        self.bands=ComplexFiveBand(np.asarray(p['five_band_coefficients_AAB44']).reshape(4,2,3),
                                   gains=[1]*5,steps=[0]*5)
        self.aux_eq=StatefulSOS([biquad_sos(v['coefficients']) for v in p['detector_biquad_banks_AAB44'][:2]],1)
        # Six stereo pairs: five music pairs, then auxiliary/music-sum pair.
        self.prefilter=StatefulSOS([[1,b1,b2,1,-a1,-a2]
              for a2,a1,b2,b1 in d['prefilter_coefficients']],12)
        self.prefilter_scale=d['prefilter_scale']
        self.residual=AdaptiveResidual(fir_coefficients,reference_gains=self.reference_gains,
                                       auxiliary_gain=f[29],step=f[28],rounding=rounding)
        self.initial_fir_sha256=hashlib.sha256(self.residual.coefficients.astype('<f4').tobytes()).hexdigest()
        self.detector_bands=[StatefulSOS([biquad_sos(v['coefficients'])],2)
                            for v in p['detector_biquad_banks_AAB44'][2:]]
        self.history=PowerHistory(16,maximum=f[18]&1,
                                  sum_scale=d['history_sum_scale'],max_scale=d['history_max_scale'])
        self.control=ComplexControl()
        self.control_options=dict(helper_parameters=p['helper_parameters_loader'],
            y_floor_db=self.power_floor_db,cap_code=p['cap_code_AAFC0'],common_code=f[17],
            adaptive_enable=adaptive_enable,inhibit=sideband,cap_enable=f[26],freeze=f[33],
            link_upper=f[38],other_base=p['other_base_AAD70'],other_scale=f[27],
            adaptive_rates=[f[22],f[23]],other_rates=[f[24],f[25]],force_smoothing=f[35],
            smoothing_stage=not sideband)
        self.common_gain=1.0
        self.common_step=0.0
        self.blocks=0

    def process_block(self, music, auxiliary):
        music=np.asarray(music,dtype=float)
        auxiliary=np.asarray(auxiliary,dtype=float)
        if music.shape!=(64,10) or auxiliary.shape!=(64,) or not np.isfinite(music).all() or not np.isfinite(auxiliary).all():
            raise ValueError('one finite 64 x 10 music block and 64 auxiliary samples required')
        f=self.fields
        # Stage5 auxiliary biquads/bypass, stage6 weighted sum of INPUT music.
        aux=auxiliary if f[36]&1 else self.aux_eq.process(auxiliary[:,None])[:,0]
        music_sum=music@self.reference_gains
        work=self.delay.process(music) # stages7/8 interleaving abstracted
        if not self.sideband: work=self.bands.process(work) # stage9
        output=work.copy()             # stage10 splits into separate outputs
        if not self.sideband and not f[34]&1:
            output*= (self.common_gain+np.arange(64)*self.common_step)[:,None]
            self.common_gain+=64*self.common_step
        # Stage12 mutates packed scratch, NOT the stage10 output buffers.
        packed=np.column_stack([work,aux,music_sum])
        low=self.prefilter.process(packed*self.prefilter_scale)
        # 1273A8..1273C5 FIRST compacts each filtered pair in place: descriptor
        # +12=16, M1=32 interleaved words, four SIMD reads at0/32/64/96.
        # Stage13 127A80 then sequentially unpacks that compacted prefix.
        # Effective sample positions are0/16/32/48, NOT original low[:4].
        decimated=low[::16][:4]
        refs,aux_low,sum_low=decimated[:,:10],decimated[:,10],decimated[:,11]
        residual=self.residual.process(refs,aux_low)
        pair=np.column_stack([residual,sum_low])
        powers=self.history.push(np.stack([b.process(pair) for b in self.detector_bands],axis=1))
        update=None
        if powers is not None:
            x,y=shape_powers(*powers,x_db=self.power_x_db,y_db=self.power_y_db,
                              cumulative_min=f[37]&1)
            update=self.control.update(x,y,**self.control_options)
            # Targets calculated AFTER audio; the next block starts the ramp.
            self.bands.gains=update['gains'].copy()
            self.bands.steps=update['steps'].copy()
            self.common_gain=update['common_gain']
            self.common_step=update['common_step']
            update['power_x']=x
            update['power_y']=y
        self.blocks+=1
        return {'audio':output,'detector_references':refs,'detector_auxiliary':aux_low,
                'detector_music_sum':sum_low,'residual':residual,'update':update}

    def metadata(self):
        return {'model':'c4d17-fixed-full-mode-reference-v3','profile_index':self.profile['profile_index'],
                'sideband':self.sideband,'adaptive_enable':self.control_options['adaptive_enable'],
                'power_x_db':self.power_x_db.tolist(),'power_y_db':self.power_y_db.tolist(),
                'power_floor_db':self.power_floor_db.tolist(),'rounding':self.residual.rounding,
                'initial_fir_float32_sha256':self.initial_fir_sha256,
                'profile_parameters_sha256':self.profile_sha256,
                'detector_sample_indices':[0,16,32,48],
                'supersedes':'v2 omitted stage12 stride16 compaction; v2 adaptive scenario numbers are invalid',
                'warning':'Explicit zero-state full-mode scenario, not live amp state. Mode2, startup binding call, hardware rounding and upstream auxiliary source excluded.'}
