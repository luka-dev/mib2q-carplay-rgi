"""Fixed-parameter reference for the two-channel 132F86 B-library tail.

Explicit boundary audio/parameters only. No live selector, parameter changes,
allocation, error paths, reset flags, or spectral input generator is invented.
"""
import numpy as np
from amp_b_filters import LibraryFilterCascade


def mono(audio):
    x=np.asarray(audio,dtype=float)
    if x.ndim!=1 or not np.isfinite(x).all():raise ValueError('finite mono audio required')
    return x


class FixedDelay:
    """Steady positive-delay branch of 12F411, plus explicit zero bypass."""
    def __init__(self, samples):
        if type(samples) is not int or samples<0:raise ValueError('nonnegative integer delay required')
        self.buffer=np.zeros(samples);self.pos=0

    def process(self, audio):
        x=mono(audio)
        if not len(self.buffer):return x.copy()
        y=np.empty_like(x)
        for i,value in enumerate(x):
            y[i]=self.buffer[self.pos];self.buffer[self.pos]=value
            self.pos=(self.pos+1)%len(self.buffer)
        return y


class Allpass:
    """12B90B: v=x-g*d; y=d+g*v; ring stores v, not y."""
    def __init__(self, delay, gain):
        if type(delay) is not int or delay<1 or not np.isfinite(gain) or abs(gain)>=1:
            raise ValueError('positive delay and stable finite |gain|<1 required')
        self.buffer=np.zeros(delay);self.pos=0;self.gain=float(gain)

    def process(self, audio):
        values=mono(audio);y=np.empty_like(values)
        for i,x in enumerate(values):
            d=self.buffer[self.pos];v=x-self.gain*d
            y[i]=d+self.gain*v;self.buffer[self.pos]=v
            self.pos=(self.pos+1)%len(self.buffer)
        return y


class NestedAllpass:
    """12D10E, preserving old operands of parallel compute/store clauses.

    u=x-g2*d2; v=u-g1*d1; w=d1+g1*v;
    ring1<-v; ring2<-w; y=d2+g2*u. NOT two allpasses in series.
    """
    def __init__(self, delay1, gain1, delay2, gain2, *, allow_zero_delay_cells=False):
        # 12D10E active zero requests still read/write cell0 and wrap to0.
        # Explicit opt-in assumes each backing ring has >=1 addressable word
        # and zero initial cursor/history. It is not an allocation emulator.
        if type(allow_zero_delay_cells) is not bool:
            raise ValueError('explicit boolean zero-delay-cell opt-in required')
        if any(type(v) is not int or v<0 for v in (delay1,delay2)):
            raise ValueError('nonnegative integer nested delays required')
        self.inner=Allpass(max(1,delay1) if allow_zero_delay_cells else delay1,gain1)
        self.outer=Allpass(max(1,delay2) if allow_zero_delay_cells else delay2,gain2)

    def process(self, audio):
        values=mono(audio);y=np.empty_like(values);a,b=self.inner,self.outer
        for i,x in enumerate(values):
            d1,d2=a.buffer[a.pos],b.buffer[b.pos]
            u=x-b.gain*d2;v=u-a.gain*d1
            a.buffer[a.pos]=v;b.buffer[b.pos]=d1+a.gain*v
            y[i]=d2+b.gain*u
            a.pos=(a.pos+1)%len(a.buffer);b.pos=(b.pos+1)%len(b.buffer)
        return y


class BTwoChannelTailReference:
    """132F86 fixed successful branch, inputs destined for B output lanes6/7.

    Pass None explicitly to bypass an allpass. Delay transitions and EQ state2
    mute/reset are excluded. Required gains are linear, NOT dB.
    """
    def __init__(self, *, common_delay, channel_delays, pre_eq_gain, eq_records,
                 nested, single, final_gain):
        if len(channel_delays)!=2 or len(nested)!=2 or len(single)!=2:
            raise ValueError('exactly two channel configurations required')
        if not np.isfinite([pre_eq_gain,final_gain]).all():raise ValueError('finite gains required')
        self.common=[FixedDelay(common_delay) for _ in range(2)]
        self.delays=[FixedDelay(v) for v in channel_delays]
        self.pre_eq_gain=pre_eq_gain;self.final_gain=final_gain
        self.eq=LibraryFilterCascade(eq_records,2)
        self.nested=[None if v is None else NestedAllpass(**v) for v in nested]
        self.single=[None if v is None else Allpass(**v) for v in single]

    def process(self, audio):
        x=np.asarray(audio,dtype=float)
        if x.ndim!=2 or x.shape[1]!=2 or not np.isfinite(x).all():raise ValueError('finite frames x 2 required')
        y=np.column_stack([self.delays[i].process(self.common[i].process(x[:,i])) for i in range(2)])
        y=self.eq.process(y*self.pre_eq_gain)
        for i in range(2):
            for stage in (self.nested[i],self.single[i]):
                if stage is not None:y[:,i]=stage.process(y[:,i])
        return y*self.final_gain
