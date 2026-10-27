"""Explicit settled pre-B scenario: graph0..7 to processed eight input lanes.

Calibrated writer targets, not live RAM; no default vehicle preset. Stateful
float64 audio reference after delay/DMA initialization and coefficient ramps
have settled. Does not emulate interrupts, startup clears or changing controls.
The first three output columns feed B graph24/23/22; physical MOST mapping is
outside this API. No default AmpChain integration.
"""
import copy
import numpy as np
from scipy.signal import sosfreqz
from amp_b_input_eq import BInputBankEQReference, _finite
from amp_complex_path import StatefulSOS
from amp_dsp_chain import design_to_sos


def select_level_bank(level_word, thresholds):
    """B0066: signed fixed-point word /2^24, truncating toward zero."""
    if type(level_word) is not int or not -(1 << 31) <= level_word < (1 << 31):
        raise ValueError('signed32 level word required, not MMI volume percentage')
    t = np.asarray(thresholds)
    if (t.shape != (16,) or not np.issubdtype(t.dtype, np.integer)
            or np.any(t < -128) or np.any(t > 127) or np.any(np.diff(t) > 0)):
        raise ValueError('16 descending signed-byte thresholds required')
    level = abs(level_word)//(1 << 24) * (-1 if level_word < 0 else 1)
    return next((i for i, threshold in enumerate(t) if level >= threshold), 15)


def input_delay_target(parameters, *, enabled, milliseconds):
    """A9D80: caller supplies the actual request, never infers live enable."""
    if type(enabled) is not bool or type(milliseconds) is not int or not 0 <= milliseconds <= 65535:
        raise ValueError('explicit boolean enable and uint16 milliseconds required')
    d = parameters['delay']
    compensation = d['minimum_candidate_samples']+d['fixed_compensation_samples']
    return min(d['maximum_samples'], max(0, (48*milliseconds if enabled else 0)-compensation))


def _design(record, gain):
    r = copy.deepcopy(record)
    r['gain_tenths_db'] = gain
    # Mathematical equivalent of firmware's float32 gain lookup; not bit-exact.
    r['gain_linear'] = 10.0**(gain/200)
    return design_to_sos(r)


class BInputPathFixedReference:
    """Settled, zero-initial-history scenario; all control choices are required.

    eq_family='tone': 4 gains (three tone bands plus profile low shelf); fifth
    section bypasses. 'graphic': 5 gains. Caller must resolve the firmware's
    position/addition or graphic-disable controls before supplying these gains.
    bank_index is the EFFECTIVE selector after enable/commit/delivery, not the
    raw level-derived candidate. Delay is a settled uniform sample count.
    """
    def __init__(self, parameters, *, eq_family, shared_gains_tenths_db,
                 bank_index, delay_samples):
        if parameters['calibration']['signature_matches_dump'] is not True:
            raise ValueError('this reference requires the audited calibrated-copy scenario')
        if type(bank_index) is not int or not 0 <= bank_index <= 15:
            raise ValueError('explicit effective bank index0..15 required')
        if type(delay_samples) is not int or not 0 <= delay_samples <= 9600:
            raise ValueError('settled integer delay0..9600 required')
        gains = _finite(shared_gains_tenths_db)
        if eq_family not in ('tone', 'graphic') or gains.shape != ((4,) if eq_family == 'tone' else (5,)):
            raise ValueError('tone requires4 gains, graphic requires5 gains')
        if np.any(np.abs(gains) > 1000):
            raise ValueError('gain outside bounded reference range')
        s = parameters['shared_eq']
        if eq_family == 'tone':
            templates = s['tone']['templates']+[s['tone']['fourth_template']]
        else:
            templates = s['graphic']['records']
        shared_sos = [_design(r, g) for r, g in zip(templates, gains)]
        if eq_family == 'tone': shared_sos.append([1,0,0,1,0,0])
        pre = parameters['prefilter']['records'][parameters['prefilter']['first_b_record_index']]
        self.prefilter = StatefulSOS([design_to_sos(pre)], 8)
        self.shared = StatefulSOS(shared_sos, 8)
        self.input_gain = _finite(parameters['input_gain']['target_linear'])
        if self.input_gain.shape != (8,): raise ValueError('eight input gains required')
        bank = parameters['bank_eq']
        sos = np.array([_design(r, g) for r, g in zip(bank['templates'],bank['gains_tenths_db'][bank_index])])
        if sos.shape != (3,6) or np.any(sos[:,0] == 0):
            raise ValueError('three bank sections with nonzero b0 required')
        packed = np.column_stack((-sos[:,5],-sos[:,4],sos[:,2]/sos[:,0],sos[:,1]/sos[:,0]))
        self.bank = BInputBankEQReference(records=np.tile(packed,(8,1,1)),
                                         gains=np.full(8,np.prod(sos[:,0])))
        self.delay_history = np.zeros((delay_samples,8))
        self.scenario = {'profile':parameters['profile_index'],'eq_family':eq_family,
                         'shared_gains_tenths_db':gains.tolist(),'bank_index':bank_index,
                         'delay_samples':delay_samples,'live_state_proven':False}

    def response(self, frequencies):
        f=_finite(frequencies)
        if f.ndim!=1 or np.any(f<0) or np.any(f>24000):
            raise ValueError('frequency vector within0..24000Hz required')
        _,pre=sosfreqz(self.prefilter.sos,worN=f,fs=48000)
        _,shared=sosfreqz(self.shared.sos,worN=f,fs=48000)
        out=np.empty((len(f),8),complex)
        phase=np.exp(-2j*np.pi*f*self.scenario['delay_samples']/48000)
        for ch,c in enumerate(self.bank.records):
            sos=np.column_stack((np.ones(len(c)),c[:,3],c[:,2],np.ones(len(c)),-c[:,1],-c[:,0]))
            _,bank=sosfreqz(sos,worN=f,fs=48000)
            out[:,ch]=pre*shared*bank*self.input_gain[ch]*self.bank.gains[ch]*phase
        return out

    def process(self, audio):
        x = _finite(audio)
        if x.ndim != 2 or x.shape[1] != 8:
            raise ValueError('graph0..7 audio, frames x8 required; raw MOST6 is not accepted')
        if not len(x): return x
        if len(self.delay_history):
            buffered = np.concatenate((self.delay_history,x))
            self.delay_history = buffered[len(x):].copy()
            x = buffered[:len(x)].copy()
        return self.bank.process(self.shared.process(self.prefilter.process(x)*self.input_gain))
