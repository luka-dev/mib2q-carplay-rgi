"""Simultaneous-window electrical metrics for complete conditional DSP paths.

No A-only time alignment when B/aux paths have different delays. Group powers
are averages of candidate electrical outputs, NOT coherent acoustic sums/SPL.
Finite tail padding does not certify settled IIR/adaptive state or true peaks.
"""
import numpy as np

from amp_music_report import BANDS, GROUPS, band_powers


def _audio(value, channels):
    if np.iscomplexobj(value):raise ValueError('real audio required')
    x=np.asarray(value,dtype=np.float64)
    if x.ndim!=2 or x.shape[1]!=channels or not np.isfinite(x).all():
        raise ValueError(f'finite frames x{channels} audio required')
    return x


def _db_power(value):
    return (10*np.log10(np.maximum(value,1e-30))).tolist()


def _levels(x):
    if not len(x):
        return {'frames':0,'rms_dbfs':None,'peak_dbfs':None,'per_channel_peak_dbfs':None,
                'frames_above_0dbfs':0,'samples_above_0dbfs':0}
    magnitude=np.abs(x)
    channel_peak=magnitude.max(axis=0)
    return {'frames':len(x),'rms_dbfs':_db_power(np.mean(x*x,axis=0)),
            'peak_dbfs':float(20*np.log10(max(float(channel_peak.max()),1e-15))),
            'per_channel_peak_dbfs':(20*np.log10(np.maximum(channel_peak,1e-15))).tolist(),
            'frames_above_0dbfs':int(np.count_nonzero(magnitude.max(axis=1)>1)),
            'samples_above_0dbfs':int(np.count_nonzero(magnitude>1))}


def measure(pcm6, out16, programme_frames, tail_frames):
    """Common prefix programme plus explicitly supplied finite trailing samples.

    These are signal/output observations, not a pass/fail sound-quality rating.
    Returned frame lengths deliberately include leading DSP latency in the
    programme window. Use identical windows and input-delay convention for dry
    and every variant; do not attribute electrical aux energy to music alone.
    """
    pcm6=_audio(pcm6,6);out16=_audio(out16,16)
    if (type(programme_frames) is not int or programme_frames<2 or type(tail_frames) is not int
            or tail_frames<0 or len(pcm6)!=len(out16) or len(pcm6)!=programme_frames+tail_frames):
        raise ValueError('matching arrays and explicit programme>=2/tail>=0 frame counts required')
    programme=out16[:programme_frames]
    bands=band_powers(programme).T
    groups={name:{'power_dbfs':_db_power(bands[indices].mean(axis=0)),
                  'programme_peak_dbfs':_levels(programme[:,indices])['peak_dbfs']}
            for name,indices in GROUPS.items()}
    rail=np.abs(pcm6)>=32767/32768
    return {'analysis':{'programme_window_frames':[0,programme_frames],
                'tail_window_frames':[programme_frames,len(out16)],
                'alignment':'common simultaneous windows including leading latency; NO A-only channel alignment',
                'power':'Welch mean-square electrical band power; arithmetic mean within candidate groups',
                'peak':'sample peaks, not intersample/true peaks or a physical amplifier overload verdict',
                'tail':'finite caller-supplied padding, NOT proof all IIR/adaptive state has drained',
                'speaker_labels':'unverified physical-output candidates',
                'normalization':'none'},
            'bands_hz':[list(v) for v in BANDS],
            'pcm6':{**_levels(pcm6),
                    'channel_order':['L','R','C','LFE','Ls','Rs'],
                    'digital_c_nonzero_samples':int(np.count_nonzero(pcm6[:,2])),
                    'samples_at_or_above_s16_rail':rail.sum(axis=0).tolist(),
                    'rail_note':'near-rail observation; alone not proof of clipping'},
            'outputs':{'whole_clip':_levels(out16),'programme':_levels(programme),
                       'finite_tail':_levels(out16[programme_frames:]),
                       'per_config_band_power_dbfs':_db_power(bands)},
            'groups':groups,
            'flags':{'nonfinite_audio':False,
                     'electrical_output_exceeds_0dbfs':bool(np.any(np.abs(out16)>1)),
                     'digital_c_is_zero':not bool(np.any(pcm6[:,2]))},
            'sound_quality_or_vehicle_acceptance_established':False}


def paired_delta(measured, dry):
    """Same-scenario/window paired electrical power differences; not loudness."""
    if measured['analysis']!=dry['analysis'] or measured['bands_hz']!=dry['bands_hz']:
        raise ValueError('paired comparisons require identical windows and analysis')
    return {name:(np.asarray(measured['groups'][name]['power_dbfs'])-
                  np.asarray(dry['groups'][name]['power_dbfs'])).tolist() for name in GROUPS}
