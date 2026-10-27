#!/usr/bin/env python3
"""J525 (8W0035465 SW0420) known electronic path, bank 1, explicit hypotheses.

Chain (SHARC schedule order, all from output_binding_audit.json + V850 0xA96C8):
  PCM6 -> optional explicit graph binding + common input processing
  -> 8x8 input matrix (bank 1) -> two 8-bus delays -> row EQ -> profile EQ
  -> 20/10 pair mixer row 1 (B zero, injected, or explicit mode5 reference)
  -> complex block 0xC4D17 (default identity; explicit full-mode reference optional)
  -> 10->16 selector -> 32/16 pair mixer (second B zero, injected, or explicit
     independent auxiliary reference) -> normal output selector
  -> packed output EQ -> post EQ / optional limiter -> post EQ -> output delay
  -> optional limiter -> gain. Extra 10-bus gains/limiters require an explicit
runtime scenario. This is not a complete emulator. Delay is included in both
time and frequency responses; nonlinear scenarios must use time rendering.

Biquad generation is the exact V850 0xA96C8/0xABF7C math (symbolically traced
from the disassembly, 2026-09-19):
  K = cot(pi f/48000), G = 10^(dB/20)
  type 1  peak  P=10/q (=1/Q, Q=q/10).  boost: b=[K^2+G*K*P+1, 2(1-K^2), K^2-G*K*P+1], a=[K^2+K*P+1, ., K^2-K*P+1]
                                          cut:   b=[K^2+K*P+1, ., K^2-K*P+1],  a=[K^2+K*P/G+1, ., K^2-K*P/G+1]   -> |H(f0)| = G both ways
  type 2/4/6   LP1        b=[1,1,0]           a=[K+1, 1-K, 0]
  type 14/16/18 HP1       b=[K,-K,0]          a=[K+1, 1-K, 0]
  type 3 (P=q/10) / 5,7..13 (P=table) LP2   b=[1,2,1]        a=[K^2+2KP+1, 2(1-K^2), K^2-2KP+1]   P = zeta = 1/(2Q)
  type 15 (P=q/10) / 17,19..25 (P=table) HP2 b=[K^2,-2K^2,K^2] a=same
  type 27 high shelf (1st order, HF gain G, DC 1): G>=1: b=[GK+1, 1-GK], a=[K+1, 1-K]; G<1: b=[K+1,1-K], a=[K/G+1, 1-K/G]
  type 28 allpass2 P=10/q; type 0 bypass.   q_code 0 -> P = 10.
Fixed zeta table (CodeFlash 0x6EA30 + 4*type, HP types read the same table at type-12).

UNVERIFIED (flagged, not silently assumed away):
  * complex block 0xC4D17 live mode/state is unknown; default remains identity.
  * BOTH pair-mixer B inputs default to zero, not a verified vehicle state.
    First B may instead use an explicit mode5 parameter/history scenario.
    Second B may use a supplied aux8/9/10 parameter/history scenario (opt-in v9).
  * upstream tone/bank controls and other gain/dynamic stages require explicit
    scenarios. Physical input mapping/live controls and acoustics are unverified.
  * physical speaker labels are the audit's candidates, not a proven pinout.
"""
from __future__ import annotations
import copy, hashlib, json, math, sys
from pathlib import Path
import numpy as np
from scipy.signal import sosfilt, sosfreqz
from digital_stages import audio_block, select
from amp_dynamic import PeakLimiter
from amp_complex_path import ComplexFullModeReference

SR = 48000
ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / 'assets/soundamp/output_binding_audit.json'
MODEL_VERSION = 'bank1-known-electronics-v7'
# bank 1 (sink 0x26) input matrix, dB: rows = buses L R C LFE Ls Rs B7 B8, cols = inputs L R C LFE Ls Rs
MATRIX_DB = [[-2,-100,-6,-100,-100,-100],[-100,-2,-6,-100,-100,-100],
             [-8,-8,-3,-100,-100,-100],[-8,-8,-10,3,-14,-14],
             [-3,-100,-100,-100,6,-100],[-100,-3,-100,-100,-100,6],
             [-3,-100,-100,-100,9,-100],[-100,-3,-100,-100,-100,9]]
ZETA = [0.26865, 0.26795, 1.0, 0.70711, 1.0, 0.92388, 1.0, 0.96593, 0.5, 0.38268, 0.80903, 0.70711, 0.30902, 0.25882]
IN = ['L', 'R', 'C', 'LFE', 'Ls', 'Rs']


def lin(db_):
    return 0.0 if db_ <= -100 else 10 ** (db_ / 20)


def design_to_sos(rec, zeta=ZETA):
    """One V850 design record -> normalized [b0 b1 b2 1 a1 a2]."""
    t = rec['type_code']
    if t == 0:
        return np.array([1, 0, 0, 1, 0, 0], dtype='f8')
    K = rec['cot_value']; G = rec.get('gain_linear', 10 ** (rec['gain_tenths_db'] / 200)); q = rec['q_code']
    if t == 1:
        P = 10.0 if q == 0 else 10 / q
        if rec['gain_tenths_db'] >= 0:
            b = [K * K + G * K * P + 1, 2 * (1 - K * K), K * K - G * K * P + 1]
            a = [K * K + K * P + 1, 2 * (1 - K * K), K * K - K * P + 1]
        else:
            b = [K * K + K * P + 1, 2 * (1 - K * K), K * K - K * P + 1]
            a = [K * K + K * P / G + 1, 2 * (1 - K * K), K * K - K * P / G + 1]
    elif t in (2, 4, 6):
        b, a = [1, 1, 0], [K + 1, 1 - K, 0]
    elif t in (14, 16, 18):
        b, a = [K, -K, 0], [K + 1, 1 - K, 0]
    elif t == 3 or 5 <= t <= 13 or t == 15 or 17 <= t <= 25:
        P = (10.0 if q == 0 else q / 10) if t in (3, 15) else zeta[t if t < 14 else t - 12]
        a = [K * K + 2 * K * P + 1, 2 * (1 - K * K), K * K - 2 * K * P + 1]
        b = [1, 2, 1] if t <= 13 else [K * K, -2 * K * K, K * K]
    elif t == 26:
        # AB850: gain>=1 branch, otherwise denominator uses 1/G.
        # Present in upstream tone controls, although absent from output EQ.
        b, a = ([K + G, G - K, 0], [K + 1, 1 - K, 0]) if G >= 1 else ([K + 1, 1 - K, 0], [K + 1/G, 1/G - K, 0])
    elif t == 27:
        b, a = ([G * K + 1, 1 - G * K, 0], [K + 1, 1 - K, 0]) if G >= 1 else ([K + 1, 1 - K, 0], [K / G + 1, 1 - K / G, 0])
    elif t == 28:
        P = 10.0 if q == 0 else 10 / q
        b, a = [K * K - K * P + 1, 2 * (1 - K * K), K * K + K * P + 1], [K * K + K * P + 1, 2 * (1 - K * K), K * K - K * P + 1]
    else:
        raise ValueError(f'type {t} not modelled')
    b = np.array(b, 'f8') / a[0]; a = np.array(a, 'f8') / a[0]
    return np.concatenate([b, a])


def sos_of(records, zeta=ZETA):
    return np.array([design_to_sos(r, zeta) for r in records]) if records else np.zeros((0, 6))


def delay_channels(samples, delays):
    """Zero-state integer delay; also valid for clips shorter than a delay."""
    out = np.zeros_like(samples)
    for channel, delay in enumerate(delays):
        if delay == 0:
            out[:, channel] = samples[:, channel]
        elif delay < len(samples):
            out[delay:, channel] = samples[:-delay, channel]
    return out


def reference_fingerprint(value):
    """Stable fingerprint of explicit reference parameters AND initial histories."""
    def record(v):
        if isinstance(v,np.ndarray):
            return {'dtype':str(v.dtype),'shape':v.shape,
                    'sha256':hashlib.sha256(v.tobytes(order='C')).hexdigest()}
        if isinstance(v,np.generic):return record(v.item())
        if isinstance(v,complex):return {'real':v.real,'imag':v.imag}
        if v is None or isinstance(v,(str,int,float,bool)):return v
        if isinstance(v,dict):return {k:record(x) for k,x in sorted(v.items())}
        if isinstance(v,(list,tuple)):return [record(x) for x in v]
        if hasattr(v,'__dict__'):
            return {'class':type(v).__module__+'.'+type(v).__qualname__,'state':record(vars(v))}
        raise ValueError('unsupported reference state for reproducible fingerprint')
    return hashlib.sha256(json.dumps(record(value),sort_keys=True,allow_nan=False).encode()).hexdigest()


class AmpChain:
    def __init__(self, audit=AUDIT, row=1, complex_block='identity', runtime=None,
                 complex_scenario=None, upstream_scenario=None, first_b_scenario=None,
                 second_b_scenario=None):
        if type(row) is not int or row not in range(5):
            raise ValueError('row must be an explicit steady-state target 0..4')
        if complex_block not in ('identity', 'full_mode_reference'):
            raise ValueError('complex_block must be identity or full_mode_reference')
        if (complex_block == 'identity') != (complex_scenario is None):
            raise ValueError('full_mode_reference requires an explicit complex_scenario; identity forbids it')
        self.complex_block = complex_block
        self.complex_scenario = copy.deepcopy(complex_scenario)
        self.complex_metadata = None
        self.complex_delay = np.zeros(10, dtype=int)
        if complex_scenario is not None:
            reference = ComplexFullModeReference(**self.complex_scenario)
            self.complex_metadata = reference.metadata()
            self.complex_delay = np.repeat([
                max(0, min(v['requested'], v['maximum']))
                for v in reference.profile['stage8_loader_delays']], 2)
        self.audit_path = Path(audit)
        d = json.loads(self.audit_path.read_text())
        k = d.get('known_electronic_stages', {})
        if k.get('version') != 2 or k.get('input_matrix',{}).get('order')!='output_rows_input_columns':
            raise ValueError('audit lacks known electronic stages; regenerate with audit_amp_output_bindings.py')
        self.profile, self.row = d['profile'], row
        if self.complex_metadata is not None and self.complex_metadata['profile_index'] != self.profile:
            raise ValueError('complex scenario profile must match the electronic-path audit profile')
        self.source_sha256 = d['source_sha256']
        self.second_b_scenario = copy.deepcopy(second_b_scenario)
        self.second_b_metadata = None
        if second_b_scenario is not None:
            from amp_second_b import SecondBAuxReference
            if (set(second_b_scenario) != {'reference', 'label'}
                    or not isinstance(second_b_scenario['reference'], SecondBAuxReference)
                    or not isinstance(second_b_scenario['label'], str)
                    or not second_b_scenario['label'].strip()):
                raise ValueError('second B requires explicit SecondBAuxReference and nonempty label')
            refmeta = self.second_b_scenario['reference'].metadata()
            if refmeta['source_sha256'] != self.source_sha256:
                raise ValueError('second-B and electronic audit firmware hashes differ')
            if refmeta['output_B_slot_ports'] != d['graph_nodes'][55]['port_ids'][16:32]:
                raise ValueError('second-B output slots disagree with C4DFB; re-audit')
            self.second_b_metadata = dict(refmeta, label=second_b_scenario['label'],
                parameter_history_sha256=reference_fingerprint(self.second_b_scenario['reference']))
        self.zeta = k['zeta_table']
        self.M8 = np.asarray(k['input_matrix']['linear'], dtype='f8')
        self.M = self.M8[:, :6]
        self.upstream_scenario = copy.deepcopy(upstream_scenario)
        self.upstream_metadata = None
        self.first_b_scenario = copy.deepcopy(first_b_scenario)
        self.first_b_metadata = None
        if upstream_scenario is not None:
            # Lazy import: upstream reuses this module's verified SOS designer.
            from amp_b_upstream import BInputPathFixedReference
            required={'audit_path','graph_input_sources','eq_family',
                      'shared_gains_tenths_db','bank_index','delay_samples'}
            if set(upstream_scenario)!=required:
                raise ValueError('upstream scenario requires audit_path, explicit graph_input_sources and all fixed controls')
            source=json.loads(Path(upstream_scenario['audit_path']).read_text())
            if source['source_sha256']!=self.source_sha256:
                raise ValueError('upstream and electronic audit firmware hashes differ')
            self.upstream_parameters=copy.deepcopy(source['input_path_profiles'][self.profile])
            if self.upstream_parameters['profile_index']!=self.profile:
                raise ValueError('upstream and electronic profiles differ')
            mapping=upstream_scenario['graph_input_sources']
            if not isinstance(mapping,(list,tuple)) or len(mapping)!=8 or any(
                    v is not None and (type(v) is not int or not 0<=v<6) for v in mapping):
                raise ValueError('graph_input_sources must explicitly map8 lanes to PCM6 indices0..5 or None=silence')
            self.input_binding=np.zeros((8,6))
            for lane,src in enumerate(mapping):
                if src is not None:self.input_binding[lane,src]=1
            self.upstream_args={key:copy.deepcopy(upstream_scenario[key]) for key in
                ('eq_family','shared_gains_tenths_db','bank_index','delay_samples')}
            ref=BInputPathFixedReference(self.upstream_parameters,**self.upstream_args)
            self.upstream_metadata=dict(ref.scenario,graph_input_sources=list(mapping),
                physical_mapping_proven=False,source_sha256=source['source_sha256'],
                parameter_sha256=reference_fingerprint(self.upstream_parameters),
                boundary='settled common input for A and B; explicit physical mapping hypothesis, not live state')
        if first_b_scenario is not None:
            from amp_b_pipeline import BMode5FixedReference
            if upstream_scenario is None:
                raise ValueError('internal first B requires explicit common upstream scenario')
            if set(first_b_scenario)!={'reference','wrapper_delay_samples','label'}:
                raise ValueError('first B requires explicit reference, wrapper_delay_samples and label')
            if not isinstance(first_b_scenario['reference'],BMode5FixedReference):
                raise ValueError('only explicit BMode5FixedReference is supported')
            delay=first_b_scenario['wrapper_delay_samples']
            if type(delay) is not int or not 0<=delay<=480000:
                raise ValueError('explicit wrapper delay0..480000 samples required')
            if not isinstance(first_b_scenario['label'],str) or not first_b_scenario['label'].strip():
                raise ValueError('nonempty B parameter scenario label required')
            routes=source['first_b_output_routes']
            self.first_b_slots=[r['mixer_b_slot'] for r in routes]
            if [r['b_output_lane'] for r in routes]!=list(range(8)) or self.first_b_slots!=[0,1,4,5,6,7,8,9]:
                raise ValueError('first B lane-to-mixer binding changed; re-audit')
            self.first_b_metadata={'mode':5,'label':first_b_scenario['label'],
                'input_graph_ports':[24,23,22],'mixer_b_slots':self.first_b_slots,
                'input_mix_gain':first_b_scenario['reference'].input_mix_gain,
                'wrapper_delay_samples':delay,'live_state_proven':False,
                'parameter_history_sha256':reference_fingerprint(self.first_b_scenario['reference']),
                'boundary':'caller-supplied parameter/history snapshot; wrapper latency is explicit hypothesis; ready/gate/lifecycle/hardware rounding not emulated'}
            if hasattr(self.first_b_scenario['reference'], 'tail_binding_metadata'):
                self.first_b_metadata['tail_binding'] = copy.deepcopy(
                    self.first_b_scenario['reference'].tail_binding_metadata)
        self.pre_delays = [np.asarray(v['samples'], dtype=int) for v in k['pre_eq_delays']]
        if len(self.pre_delays) != 2 or any(v.shape != (8,) or (v < 0).any() for v in self.pre_delays):
            raise ValueError('expected two nonnegative 8-bus delay vectors')
        self.bus_sos = [sos_of(b, self.zeta) for b in d['input_bus_eq_packed_inferred']]
        row_records = k['row_eq']['rows'][row]
        # Stored rows are unity; do not invent non-unity grouping semantics.
        self.row_sos = self._verified_unity(row_records, 'C4B27 row EQ')
        self.post_eq_sos = []
        for v in k['post_output_eq']:
            counts = v.get('counts_config_order')
            order = v.get('packed_source_record_order')
            if counts != [0]*12+[2]*4 or order != [0,1,2,3,6,7,4,5]:
                raise ValueError('post EQ binding missing or changed; regenerate/re-audit')
            packed = [v['records'][i] for i in order]
            cursor, per_config = 0, [None]*16
            for cfg in d['sharc_slot_to_v850_config']:
                n = counts[cfg]
                per_config[cfg] = sos_of(packed[cursor:cursor+n], self.zeta)
                cursor += n
            if cursor != len(packed): raise ValueError('post EQ count mismatch')
            self.post_eq_sos.append(per_config)
        self.runtime = None if runtime is None else json.loads(json.dumps(runtime))
        if self.runtime is not None:
            if set(self.runtime) != {'pre_complex_gain', 'post_complex_gain', 'limiters'}:
                raise ValueError('runtime requires both 10-bus gain vectors and both limiter parameter sets')
            for name in ('pre_complex_gain', 'post_complex_gain'):
                v = np.asarray(self.runtime[name], dtype=float)
                if v.shape != (10,) or not np.isfinite(v).all():
                    raise ValueError(f'{name} must be 10 finite steady gains')
            if len(self.runtime['limiters']) != 2:
                raise ValueError('runtime requires two limiter parameter sets')
            for params in self.runtime['limiters']:
                if len(PeakLimiter(**params).threshold) != 16:
                    raise ValueError('limiter parameters must be in 16-channel config order')
        g = d['ten_bus_pair_gain_rows_db'][row]
        self.pair_a = np.array([lin(g[2 * j]) for j in range(10)])
        self.pair_b_db = [g[2 * j + 1] for j in range(10)]
        self.pair_b = np.array([lin(v) for v in self.pair_b_db])
        self.split = d['split_modes'][0]
        self.route = d['normal_route']
        mixer = k['output_pair_mixer']
        self.output_pair_a = np.asarray(mixer['a_linear'], dtype='f8')
        self.output_pair_b = np.asarray(mixer['b_linear'], dtype='f8')
        self.ch = sorted(d['channels'], key=lambda c: c['v850_config_index'])
        self.out_sos = [sos_of(c['packed_eq_records'], self.zeta) for c in self.ch]
        self.out_src = [c['ten_bus_source_modes'][0] for c in self.ch]
        self.out_delay = [int(c['delay_raw_samples_candidate']) for c in self.ch]
        self.out_gain = np.array([lin(c['gain_db']) for c in self.ch])
        self.total_a_delay = np.array([sum(v[src] for v in self.pre_delays) + self.out_delay[o]
                                       if src < 8 else self.out_delay[o] for o, src in enumerate(self.out_src)])
        self.total_a_delay += self.complex_delay[self.out_src]
        if self.upstream_metadata is not None:
            self.total_a_delay += np.where(np.asarray(self.out_src)<8,self.upstream_metadata['delay_samples'],0)
        self.label = [f"cfg{c['v850_config_index']:02d} {'+'.join(c['physical_candidate']['speaker_ids'])} {c['physical_candidate']['description']}" for c in self.ch]
        self.warnings = [('complex block 0xC4D17 modelled as identity (omits conditional 1023-sample delay and adaptive five-band processing)' if self.complex_metadata is None else
                          self.complex_metadata['warning']),
                         ('upstream processing before input matrix C4AED not numerically emulated' if self.upstream_metadata is None else
                          'common upstream A/B processing uses explicit settled scenario; graph-to-MOST mapping and live EQ bank unproven'),
                         ('first mixer B audio assumed zero unless injected; stored B dB=' + str(self.pair_b_db) if self.first_b_metadata is None else
                          'first mixer B uses explicit mode5 snapshot and wrapper-delay hypothesis, not live state'),
                         ('second mixer B audio assumed zero unless injected; nonzero B gains exist' if self.second_b_metadata is None else
                          'second mixer B uses explicit auxiliary graph8/9/10 and effective stage/history scenario, not live state'),
                         ('C4B59/C4DDA gains and C4E9A/C4ED3 limiters omitted: runtime not supplied' if self.runtime is None else
                          'C4B59/C4DDA steady gains and C4E9A/C4ED3 kernels use explicit caller scenario, not live readback'),
                         'static profile/row, ASAD FC, normal selectors; no live MMI state, tone/volume or acoustics',
                         'input-bus EQ packed grouping inferred; physical output names are candidates',
                         'row EQ supported only for verified unity; post EQ binding decoded; float64, not SHARC bit equivalence']

    def _verified_unity(self, records, name):
        sos = sos_of(records, self.zeta)
        if not np.allclose(sos[:, :3], sos[:, 3:], rtol=0, atol=1e-14):
            raise ValueError(f'{name}: non-unity channel binding is not yet verified')
        return sos

    def metadata(self):
        """Persist proof boundaries alongside every numeric report."""
        result = {'version': MODEL_VERSION if self.upstream_metadata is None else 'bank1-electronics-v8-explicit-common-input', 'source_sha256': self.source_sha256,
                'profile': self.profile, 'bank': 1, 'row': self.row, 'asad': '0xfc',
                'complex_block': 'identity_hypothesis' if self.complex_metadata is None else self.complex_block,
                'complex_scenario': copy.deepcopy(self.complex_metadata),
                'runtime_scenario': self.runtime,
                'upstream_scenario':copy.deepcopy(self.upstream_metadata),
                'first_b_scenario':copy.deepcopy(self.first_b_metadata),
                'default_auxiliary_audio': {'first_mixer_B': 'zero_hypothesis' if self.first_b_metadata is None else 'explicit_mode5_reference',
                    'second_mixer_B': 'zero_hypothesis' if self.second_b_metadata is None else 'explicit_aux_graph_reference'},
                'pre_eq_delays': [v.tolist() for v in self.pre_delays],
                'total_a_path_delay_samples': self.total_a_delay.tolist(),
                'output_pair_a_linear': self.output_pair_a.tolist(),
                'output_pair_b_linear': self.output_pair_b.tolist(),
                'verified_unity_eq_records': int(len(self.row_sos) + sum(
                    np.allclose(s[:3], s[3:], rtol=0, atol=1e-14)
                    for bank in self.post_eq_sos for channel in bank for s in channel)),
                'warnings': self.warnings}
        if self.second_b_metadata is not None:
            result.update(version='bank1-electronics-v9-explicit-second-B',
                          second_b_scenario=copy.deepcopy(self.second_b_metadata))
        return result

    def bus_response(self, w, include_delay=True):
        """Complex 8 buses x 6 inputs x frequencies; zero auxiliary inputs."""
        w = np.asarray(w, dtype='f8')
        input_response=np.broadcast_to(self.M[:,:,None],(8,6,len(w)))
        if self.upstream_metadata is not None:
            from amp_b_upstream import BInputPathFixedReference
            hu=BInputPathFixedReference(self.upstream_parameters,**self.upstream_args).response(w).T
            if not include_delay:
                hu=hu*np.exp(2j*np.pi*w*self.upstream_metadata['delay_samples']/SR)
            input_response=np.einsum('bg,gif->bif',self.M8,self.input_binding[:,:,None]*hu[:,None,:])
        H = np.zeros((8, 6, len(w)), 'c16')
        for b in range(8):
            Hb = sosfreqz(self.bus_sos[b], w, fs=SR)[1] if len(self.bus_sos[b]) else np.ones(len(w))
            if include_delay:
                Hb = Hb * np.exp(-2j * np.pi * w * sum(v[b] for v in self.pre_delays) / SR)
            for i in range(6):
                H[b, i] = input_response[b,i] * Hb * self.pair_a[b]
        return H

    def response(self, freqs, include_delay=True):
        """16 outputs x 6 inputs x frequencies, with delay by default; B=0."""
        w = np.asarray(freqs, 'f8'); Hb = self.bus_response(w, include_delay)
        if self.runtime is not None or self.complex_scenario is not None or self.first_b_scenario is not None or self.second_b_scenario is not None:
            raise ValueError('nonlinear runtime scenario has no level-independent frequency response; render audio')
        H = np.zeros((16, 6, len(w)), 'c16')
        for o in range(16):
            src = self.out_src[o]
            if src >= 8:
                continue                                  # buses 8/9: A mute, B assumed silent
            Ho = sosfreqz(self.out_sos[o], w, fs=SR)[1] if len(self.out_sos[o]) else np.ones(len(w))
            for bank in self.post_eq_sos:
                if len(bank[o]): Ho = Ho * sosfreqz(bank[o], w, fs=SR)[1]
            H[o] = Hb[src] * Ho * self.output_pair_a[self.route[o]] * self.out_gain[o]
            if include_delay:
                H[o] *= np.exp(-2j * np.pi * w * self.out_delay[o] / SR)
        return H

    def process(self, x6, *, first_b=None, second_b=None, complex_auxiliary=None,
                second_b_auxiliary=None, capture_stages=True):
        """Zero-state offline render, L R C LFE Ls Rs -> config-order outputs.

        Each call resets filters/delays and restores supplied B histories.
        Process a full clip before cropping; delayed tails are not flushed.
        Optional B arrays are actual stage-boundary audio (frames x 10 / x 16),
        not filtered again by the common input path. Missing B means zero,
        unless first_b_scenario renders mode5 from the common first3 lanes.
        Internal first B and injected first_b cannot be combined.
        Full-mode C4D17 requires explicit mono complex_auxiliary at graph11
        AFTER upstream C4D11. A partial final block is zero-padded and trimmed.
        No such auxiliary signal is accepted/ignored by the identity model.
        Explicit internal second B requires separate frames x3 graph8/9/10
        audio. It does not reuse MOST or common-input music and cannot be
        combined with an injected second_b array.
        """
        x6 = audio_block(x6)
        if x6.shape[1] != 6:
            raise ValueError('expected frames x 6 MOST input')
        if self.second_b_scenario is not None:
            aux2 = np.asarray(second_b_auxiliary)
            if (np.iscomplexobj(aux2) or aux2.shape != (len(x6),3)
                    or not np.issubdtype(aux2.dtype, np.number) or not np.isfinite(aux2).all()):
                raise ValueError('second-B scenario requires finite real frames x3 auxiliary graph8/9/10')
            if second_b is not None:
                raise ValueError('cannot combine injected second_b and internal auxiliary reference')
        elif second_b_auxiliary is not None:
            raise ValueError('second_b_auxiliary requires an explicit second-B scenario')
        if self.complex_scenario is not None:
            auxiliary = np.asarray(complex_auxiliary, dtype=float)
            if auxiliary.shape != (len(x6),) or not np.isfinite(auxiliary).all():
                raise ValueError('full-mode requires finite complex_auxiliary with one sample per input frame')
        elif complex_auxiliary is not None:
            raise ValueError('complex_auxiliary requires full_mode_reference')
        for name, value, count in [('first_b', first_b, 10), ('second_b', second_b, 16)]:
            if value is not None and audio_block(value).shape != (len(x6), count):
                raise ValueError(f'{name} must have {len(x6)} frames and {count} columns')
        if first_b is not None and self.first_b_scenario is not None:
            raise ValueError('cannot combine injected first_b with internal mode5 reference')
        stages = {}

        def keep(name, block):
            if capture_stages:
                stages[name] = block
            return block

        if self.upstream_metadata is None:
            block = keep('bus', x6 @ self.M.T)
        else:
            from amp_b_upstream import BInputPathFixedReference
            input8=keep('graph_input8',x6 @ self.input_binding.T)
            common=keep('common_input8',BInputPathFixedReference(
                self.upstream_parameters,**self.upstream_args).process(input8))
            if self.first_b_scenario is not None:
                ref=copy.deepcopy(self.first_b_scenario['reference'])
                b8=np.zeros((len(x6),8))
                for start in range(0,len(x6),512):
                    count=min(512,len(x6)-start)
                    hop=np.pad(common[start:start+count,:3],((0,512-count),(0,0)))
                    rendered=audio_block(ref.process(*hop.T))
                    if rendered.shape!=(512,8):raise ValueError('mode5 reference must return512x8')
                    b8[start:start+count]=rendered[:count]
                b8=delay_channels(b8,[self.first_b_scenario['wrapper_delay_samples']]*8)
                keep('first_b8',b8)
                first_b=np.zeros((len(x6),10))
                first_b[:,self.first_b_slots]=b8
                keep('first_b10',first_b)
            block=keep('bus',common @ self.M8.T)
        for i, delays in enumerate(self.pre_delays):
            block = keep(f'pre_delay_{i + 1}', delay_channels(block, delays))
        # Row EQ has been validated as exact unity: no guessed per-channel binding.
        keep('row_eq', block)
        block = keep('bus_eq', np.stack([sosfilt(s, block[:, b]) if len(s) and len(block) else block[:, b]
                                        for b, s in enumerate(self.bus_sos)], 1))
        ten = np.zeros((len(x6), 10))
        ten[:, :8] = block * self.pair_a[:8]
        if first_b is not None:
            ten += np.asarray(first_b) * self.pair_b
        if self.runtime is not None:
            ten = ten * self.runtime['pre_complex_gain']
            keep('pre_complex_gain', ten)
        if self.complex_scenario is not None:
            reference = ComplexFullModeReference(**self.complex_scenario)
            rendered = np.empty_like(ten)
            for start in range(0, len(ten), 64):
                count = min(64, len(ten)-start)
                music = np.pad(ten[start:start+count], ((0,64-count),(0,0)))
                aux = np.pad(auxiliary[start:start+count], (0,64-count))
                rendered[start:start+count] = reference.process_block(music, aux)['audio'][:count]
            ten = keep('complex', rendered)
        if self.runtime is not None:
            ten = ten * self.runtime['post_complex_gain']
        keep('ten', ten)
        block = keep('split16', select(ten, self.split))
        block = block * self.output_pair_a
        if self.second_b_scenario is not None:
            second_b = copy.deepcopy(self.second_b_scenario['reference']).process(aux2)
            keep('second_b16', second_b)
        if second_b is not None:
            block += np.asarray(second_b) * self.output_pair_b
        keep('mixed16', block)
        block = keep('routed16', select(block, self.route))
        block = keep('output_eq', np.stack([sosfilt(s, block[:, o]) if len(s) and len(block) else block[:, o]
                                           for o, s in enumerate(self.out_sos)], 1))
        for i, bank in enumerate(self.post_eq_sos):
            block = np.stack([sosfilt(s, block[:, o]) if len(s) and len(block)
                              and not np.array_equal(s[:, :3], s[:, 3:]) else block[:, o]
                              for o, s in enumerate(bank)], 1)
            keep(f'post_eq_{i + 1}', block)
            if i == 1:
                block = keep('output_delay', delay_channels(block, self.out_delay))
            if self.runtime is not None:
                block = keep(f'limiter_{i + 1}', PeakLimiter(**self.runtime['limiters'][i]).process(block))
        stages['out'] = block * self.out_gain
        return stages


def _selfcheck():
    def mag(sos, f):
        return abs(sosfreqz(sos, [f], fs=SR)[1][0])
    rec = lambda f, g, q, t: {'cot_value': 1 / math.tan(math.pi * f / SR), 'gain_tenths_db': g, 'q_code': q, 'type_code': t}
    for g in (-60, 60):                               # peak +-6 dB Q2 at 1 kHz: |H(f0)| = G, unity far away
        s = design_to_sos(rec(1000, g, 20, 1))
        assert abs(20 * math.log10(mag(s, 1000)) - g / 10) < 0.01 and abs(mag(s, 20) - 1) < 0.01 and abs(mag(s, 20000) - 1) < 0.02
    narrow, wide = design_to_sos(rec(1000, -60, 60, 1)), design_to_sos(rec(1000, -60, 10, 1))   # Q6 narrower than Q1
    assert mag(narrow, 1200) > mag(wide, 1200) + 0.2
    s = design_to_sos(rec(1000, 0, 7, 3))             # LP2 q=7 -> zeta 0.7 -> |H(fc)| = 1/(2*0.7)
    assert abs(mag(s, 1000) - 1 / 1.4) < 0.01 and mag(s, 20) > 0.999
    s = design_to_sos(rec(1000, 0, 7, 15))            # HP2 mirror
    assert abs(mag(s, 1000) - 1 / 1.4) < 0.01 and mag(s, 20000) > 0.99
    s = design_to_sos(rec(1000, 0, 7, 21))            # table type 21 zeta 0.38268 -> peak 1/(2*0.38268)=1.307
    assert abs(mag(s, 1000) - 1.3066) < 0.01
    s = design_to_sos(rec(1000, -30, 0, 27))          # high shelf -3 dB: HF = G, DC = 1
    assert abs(20 * math.log10(mag(s, 20000)) + 3) < 0.05 and abs(mag(s, 20) - 1) < 0.01
    s = design_to_sos(rec(1000, 30, 0, 27))           # shelf +3 dB: HF/DC = G = +3 dB, HF = +3 abs
    assert abs(20 * math.log10(mag(s, 20000)) - 3) < 0.05
    c = AmpChain(); y = c.process(np.random.default_rng(1).normal(size=(4096, 6)) * 0.1)['out']
    assert np.isfinite(y).all()
    return c


def wav_report(chain, paths, bands=((20, 60), (60, 120), (120, 250), (250, 500), (500, 2000), (2000, 8000), (8000, 20000))):
    """6-ch WAVs (L R C LFE Ls Rs) -> per-output band power dB re the file's own L channel, plus peaks."""
    import soundfile as sf
    def bp(x):
        X = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2; f = np.fft.rfftfreq(len(x), 1 / SR)
        return np.array([10 * np.log10(X[(f >= lo) & (f < hi)].sum() / len(x) + 1e-30) for lo, hi in bands])
    for path in paths:
        x, sr = sf.read(path, dtype='float64', always_2d=True); assert sr == SR and x.shape[1] == 6, path
        out = chain.process(x)['out']; ref = bp(x[:, 0])
        print(f'\n{path}  bands {[b[0] for b in bands]}  (dB re own L)   peak dBFS')
        for o in range(16):
            if np.abs(out[:, o]).max() > 1e-6:
                print(f'  {chain.label[o][:40]:40s} ' + ' '.join(f'{v:6.1f}' for v in bp(out[:, o]) - ref) + f'   {20 * np.log10(np.abs(out[:, o]).max()):6.1f}')


if __name__ == '__main__':
    c = _selfcheck()
    print('selfcheck OK;', *c.warnings, sep='\n  ')
    if len(sys.argv) > 1:
        wav_report(c, sys.argv[1:]); sys.exit()
    f = np.array([31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000], 'f8')
    H = 20 * np.log10(np.abs(c.response(f)) + 1e-12)
    print('\nper-output magnitude (dB re MOST input) at', f.astype(int).tolist())
    for o in range(16):
        for i in range(6):
            if H[o, i].max() > -60:
                print(f'{c.label[o][:46]:46s} <- {IN[i]:3s} known dly {c.total_a_delay[o]:4d}  ' + ' '.join(f'{v:6.1f}' for v in H[o, i]))
