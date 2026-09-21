"""First-B library filter subgraph reference, not the entire B algorithm.

12DBDE validates/smooths sections; 12DAA5 pairs sections and special-cases the
odd last section; 12B529 is a direct-form-II recurrence. Runtime coefficients,
channel state and library mode must still come from an explicit scenario.
"""
import numpy as np


def stable_section(record):
    """13163D after negated feedback operands: |a2|<1, |a1|<1+a2."""
    r=np.asarray(record,dtype=float)
    if r.shape!=(5,) or not np.isfinite(r).all():raise ValueError('five finite coefficients required')
    a1,a2=-r[3],-r[4]
    return abs(a2)<1 and abs(a1)<1+a2


def coefficient_step(current, target, step):
    """12FAF2, for an address inside the registered interpolation region."""
    if not np.isfinite([current,target,step]).all():raise ValueError('finite values required')
    if current==target:return current,step
    if abs(target-current)>abs(step):return current+step,step
    return target,0.0


class LibraryFilterCascade:
    """Stored records [b0,b1,b2,-a1,-a2], one shared bank, channel states.

    Pair processing uses full biquads. ONLY an odd final section with
    abs(-a2)<float32(1e-5) uses the first-order kernel, dropping b2 too.
    Coefficients are fixed per call here; wrapper's cross-call interpolation
    and reset/mute state are not guessed. Zero sections leaves audio intact.
    """
    def __init__(self, records, channels):
        self.records=np.asarray(records,dtype=float).reshape(-1,5).copy()
        if type(channels) is not int or channels<1 or not np.isfinite(self.records).all():
            raise ValueError('finite records and positive channel count required')
        if any(not stable_section(r) for r in self.records):raise ValueError('unstable section rejected by library guard')
        self.state=np.zeros((len(self.records),2,channels))
        self.channels=channels

    def process(self, audio):
        out=np.asarray(audio,dtype=float).copy()
        if out.ndim!=2 or out.shape[1]!=self.channels or not np.isfinite(out).all():
            raise ValueError('finite frames x configured channels required')
        for index,(b0,b1,b2,fb1,fb2) in enumerate(self.records):
            first_order=index==len(self.records)-1 and len(self.records)%2 and abs(fb2)<np.float32(1e-5)
            z2,z1=self.state[index]
            for n,x in enumerate(out):
                v=x+fb1*z1+(0 if first_order else fb2*z2)
                y=b0*v+b1*z1+(0 if first_order else b2*z2)
                z2,z1=z1.copy(),v
                out[n]=y
            self.state[index]=[z2,z1]
        return out
