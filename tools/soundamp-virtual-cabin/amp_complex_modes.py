"""C4D17 lifecycle/calibration control reference, separate from audio kernels.

No live state is inferred. Addresses: 126DDE..127052 and 1270BB..1271B1.
Actions describe memory effects; 'no_write' must NOT be treated as silence.
"""
from dataclasses import dataclass


def select_stage_mask(mode_word, sideband_word):
    """Sequential bit tests give bit2 precedence over bit1 over bit0."""
    mask,inhibit=2,0
    if mode_word&1:mask=1
    if mode_word&2:mask=0xf4ba
    if mode_word&4:
        inhibit=sideband_word&1
        mask=0xdff75e5 if inhibit else 0xfff7fe5
    return mask,inhibit


@dataclass
class CalibrationSequence:
    channel_mask: int
    wait_samples: int
    measure_samples: int
    phase: int=4             # descriptor +11: configure4 / wait1 / measure2
    selected_bit: int=1      # +41
    wait_count: int=0        # +43
    measure_count: int=0     # +44
    complete: bool=False    # +10 bit0
    active_gates: int=0     # PM table +45, previous selection persists

    def __post_init__(self):
        if not 0<=self.channel_mask<1024 or min(self.wait_samples,self.measure_samples)<0:
            raise ValueError('ten-bit mask and nonnegative counters required')
        if self.selected_bit not in [1<<i for i in range(10)]:
            raise ValueError('selected_bit must be one of ten channel bits')

    def begin_block(self, frames):
        if frames<1:raise ValueError('positive frame count required')
        if self.complete:return {'action':'clear_outputs','reason':'complete'}
        # Stage1 clears music outputs before stage3, including early exits.
        phase=self.phase
        if phase&2:
            if self.measure_count<self.measure_samples:
                return self._generate(frames)
            self.measure_count=0
            advance=True
        elif phase&4:
            advance=False
        elif phase&1:
            self.wait_count+=frames
            if self.wait_count>=self.wait_samples:
                self.phase=2
                self.wait_count=0
            return {'action':'clear_outputs','reason':'wait'}
        else:
            return {'action':'clear_outputs','reason':'inactive_phase'}
        while True:
            if advance:
                self.phase=4
                self.selected_bit*=2
                if self.selected_bit>=1024:
                    self.selected_bit=1
                    self.complete=True
                    # 1270FD goes to stage4, not the function return: one
                    # final generated block uses PREVIOUS channel gates.
                    return self._generate(frames)
            if self.channel_mask&self.selected_bit:
                self.active_gates=self.selected_bit
                self.phase=1
                return {'action':'clear_outputs','reason':'configure',
                        'reset_fir_history':True,'reset_fir_coefficients':False,
                        'reference_gate_bits':self.active_gates}
            advance=True

    def _generate(self, frames):
        # Stage15 follows the generator/prefilter/LMS stages.
        self.measure_count+=frames
        return {'action':'generate_and_learn','gate_bits':self.active_gates}


@dataclass
class ComplexLifecycle:
    initialized: bool=False

    def begin_block(self, *, master_enable, mode_word, sideband_word):
        if not master_enable&1:
            return {'action':'no_write','reason':'master_disabled'}
        if not self.initialized:
            self.initialized=True
            return {'action':'no_write','reason':'bind_buffers_only'}
        mask,inhibit=select_stage_mask(mode_word,sideband_word)
        return {'action':{1:'copy_inputs',2:'clear_outputs',0xf4ba:'calibration'}.get(mask,'full_mode'),
                'stage_mask':mask,'inhibit':inhibit}


class CalibrationNoiseCore:
    """126CEC integer PRNG/shuffle core, before optional two-biquad shaping.

    Mathematical modular reference: no claim of instruction float rounding
    equivalence. Requires explicit seed, shuffle state and ALUSAT choice.
    16807/127773/2836 implement a mod(2^31-1) recurrence, not white noise
    drawn from an unrelated host RNG. No automatic state randomization.
    """
    def __init__(self, *, seed, shuffle, previous, alusat):
        if not 0<seed<2147483647 or len(shuffle)!=32 or any(not 0<=x<2147483647 for x in shuffle):
            raise ValueError('positive 31-bit seed and 32 nonnegative shuffle words required')
        if not 0<=previous<2147483647 or type(alusat) is not bool:
            raise ValueError('previous shuffle value and explicit bool alusat required')
        self.seed=seed;self.shuffle=list(shuffle);self.previous=previous;self.alusat=alusat

    def process(self, frames):
        import numpy as np
        if type(frames) is not int or frames<0:raise ValueError('nonnegative integer frame count required')
        out=np.empty(frames)
        for i in range(frames):
            self.seed=self.seed*16807%2147483647
            slot=min(31,self.previous//67108864)
            self.previous,self.shuffle[slot]=self.shuffle[slot],self.seed
            value=self.previous*2
            value=min(value,2147483647) if self.alusat else (value+2147483648)%4294967296-2147483648
            out[i]=value/2147483648
        return out
