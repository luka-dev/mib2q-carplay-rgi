"""Fixed-parameter B output branches, given explicit time-domain boundaries.

No spectral generator, live mode, parameter changes or error/reset behavior is
invented. Output branch order is recovered from 12A7F9, 12EDE6 and 131208.
"""
import numpy as np
from amp_b_decorrelator import FixedDelay, NestedAllpass, BTwoChannelTailReference
from amp_b_filters import LibraryFilterCascade


def audio(values, channels):
    x=np.asarray(values,dtype=float)
    if x.ndim!=2 or x.shape[1]!=channels or not np.isfinite(x).all():
        raise ValueError(f'finite frames x {channels} audio required')
    return x


def finite_gain(value):
    if not np.isscalar(value) or not np.isfinite(value):raise ValueError('finite linear gain required')
    return float(value)


def pair(config):
    # 12EB9B, optional 12B90B for each channel, then 132D0B.
    return BTwoChannelTailReference(common_delay=0,nested=[None,None],**config)


class BSixChannelTailReference:
    """12A7F9: two nested all-passes/channel BEFORE three-way stereo fan-out.

    pair_configs has three explicit delay/gain/EQ/single-allpass/gain banks.
    Third pair has no single allpass in firmware: both entries must be None.
    """
    def __init__(self, *, pre_gain, common_delay, nested, pair_configs, final_gain):
        if len(nested)!=2 or any(len(v)!=2 for v in nested) or len(pair_configs)!=3:
            raise ValueError('two nested stages per channel and three pair banks required')
        if pair_configs[2]['single']!=[None,None]:
            raise ValueError('third pair has no single-allpass stage')
        self.pre_gain=finite_gain(pre_gain);self.final_gain=finite_gain(final_gain)
        self.delays=[FixedDelay(common_delay) for _ in range(2)]
        self.nested=[[None if v is None else NestedAllpass(**v) for v in row] for row in nested]
        self.pairs=[pair(v) for v in pair_configs]

    def process(self, values):
        x=audio(values,2)*self.pre_gain
        y=np.column_stack([self.delays[i].process(x[:,i]) for i in range(2)])
        for i in range(2):
            for stage in self.nested[i]:
                if stage is not None:y[:,i]=stage.process(y[:,i])
        # Copies happen before the first pair's private processing.
        return np.column_stack([stage.process(y) for stage in self.pairs])*self.final_gain


class BFourChannelTailReference:
    """12EDE6: common stereo delay -> copy -> two delay/gain/EQ pairs -> gain."""
    def __init__(self, *, common_delay, pair_configs, final_gain):
        if len(pair_configs)!=2 or any(v['single']!=[None,None] or v['final_gain']!=1 for v in pair_configs):
            raise ValueError('two banks without single-allpass or private final-gain stages required')
        self.delays=[FixedDelay(common_delay) for _ in range(2)]
        self.pairs=[pair(v) for v in pair_configs];self.final_gain=finite_gain(final_gain)

    def process(self, values):
        x=audio(values,2)
        y=np.column_stack([self.delays[i].process(x[:,i]) for i in range(2)])
        return np.column_stack([stage.process(y) for stage in self.pairs])*self.final_gain


class BOutputFinishReference:
    """131208 mode2/5 merge and successful fixed filter/gain finish, 12B87B.

    Inputs are outputs of the six/four/two-channel tail functions, NOT raw
    library inputs. Keeping them explicit avoids fabricating spectral audio.
    Other modes (including mode6 early return) are not emulated by this class.
    """
    def __init__(self, *, six_gain, four_gain, two_gain, six_eq, two_eq,
                 final_eq, final_gain):
        self.six_gain=finite_gain(six_gain);self.four_gain=finite_gain(four_gain)
        self.two_gain=finite_gain(two_gain);self.final_gain=finite_gain(final_gain)
        self.six_eq=LibraryFilterCascade(six_eq,6)
        self.two_eq=LibraryFilterCascade(two_eq,2)
        self.final_eq=LibraryFilterCascade(final_eq,8)

    def process(self, six, four, two):
        a,b,c=audio(six,6),audio(four,4),audio(two,2)
        if len(a)!=len(b) or len(a)!=len(c):raise ValueError('matching frame counts required')
        a=a*self.six_gain;a[:,:4]+=b*self.four_gain
        y=np.column_stack([self.six_eq.process(a),self.two_eq.process(c*self.two_gain)])
        return self.final_eq.process(y)*self.final_gain
