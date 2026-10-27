"""Isolated B spectral controls with explicit parameters/history/band edges.

12F90A successful fixed-parameter path and helpers, NOT the complete B
upmixer. Default log/pow use host math; the explicit firmware_library opt-in
uses recovered table/rational formulas, still not SHARC40-bit arithmetic.
No runtime parameters, band map or initial history are fabricated.
"""
import numpy as np
from amp_b_spectral import word_float

POWER_TO_GAIN_FACTOR=word_float(0x3DCCCCCD)  # 12F8B4: .1, unlike C4D17 .05
FREQUENCY_BYPASS_EPSILON=word_float(0x0DA24260)


def vector(values):
    x=np.asarray(values,dtype=float)
    if x.ndim!=1 or not np.isfinite(x).all():raise ValueError('finite real vector required')
    return x


def coefficients(values, count):
    x=vector(values)
    if len(x)!=count:raise ValueError(f'{count} explicit coefficients required')
    return x


def band_edges(values, bins):
    edges=np.asarray(values)
    if (edges.ndim!=1 or edges.dtype.kind not in 'iu' or not 2<=len(edges)<=75
            or edges[0]!=0 or edges[-1]!=bins or np.any(edges[1:]<edges[:-1])):
        raise ValueError('1..74 contiguous bands covering the entire vector required')
    return edges.astype(int)


def band_sum(values, edges):
    """1316C1 scalar-stride case: SUM, not mean; empty bands output zero."""
    x=vector(values);e=band_edges(edges,len(x))
    return np.array([np.sum(x[a:b]) for a,b in zip(e[:-1],e[1:])])


def expand_bands(values, edges, bins):
    """13165D: constant fill of each half-open [start,end) band."""
    x=vector(values);e=band_edges(edges,bins)
    if len(x)!=len(e)-1:raise ValueError('one value per band required')
    return np.repeat(x,np.diff(e))


def frequency_smooth(values, direct, feedback):
    """131948: forward then reverse recursive passes, including endpoints.

    Both passes initialize previous output to the endpoint INPUT. Firmware
    bypass tests direct≈1 only, not direct+feedback or feedback≈0.
    """
    x=vector(values).copy();g,h=coefficients([direct,feedback],2)
    if not len(x) or abs(g-1)<=FREQUENCY_BYPASS_EPSILON:return x
    previous=x[0]
    for i in range(len(x)):
        x[i]=g*x[i]+h*previous;previous=x[i]
    previous=x[-1]
    for i in range(len(x)-1,-1,-1):
        x[i]=g*x[i]+h*previous;previous=x[i]
    return x


def level_gain(excursion, slow_db, fast_db, *, slope, threshold_db, minimum, maximum,
               math_reference=None):
    """12F87B: positive-excursion branch, 10^(delta*.1), then clamp.

    excursion is ALREADY fast_db-slow_db-threshold_db at the 12F90A call.
    Nonpositive excursion chooses target=fast_db, hence unity before clamp.
    """
    e,s,f=vector(excursion),vector(slow_db),vector(fast_db)
    slope,threshold_db,minimum,maximum=coefficients([slope,threshold_db,minimum,maximum],4)
    if e.shape!=s.shape or e.shape!=f.shape or not 0<=minimum<=maximum:
        raise ValueError('matching bands and ordered nonnegative gain bounds required')
    target=np.where(e>0,slope*e+s+threshold_db,f)
    exponent=(target-f)*POWER_TO_GAIN_FACTOR
    with np.errstate(over='ignore'):
        gain=(np.power(10.,exponent) if math_reference is None else math_reference.pow10(exponent))
    return np.clip(gain,minimum,maximum)


def spectral_ramp(*, bins, start, end, low_bin, high_bin):
    """12AD70 valid-index, nonzero-denominator branch only.

    Firmware forces low_bin0 to1 AFTER computing step with original bounds;
    preserve that edge case instead of substituting np.linspace/interp.
    Invalid bounds/error fallthrough and zero denominator are not modelled.
    """
    if (type(bins) is not int or type(low_bin) is not int or type(high_bin) is not int
            or not 0<=low_bin<high_bin<bins):
        raise ValueError('require 0 <= low_bin < high_bin < bins')
    start,end=coefficients([start,end],2)
    step=(end-start)/(high_bin-low_bin)
    low=max(1,low_bin)
    out=np.empty(bins);out[:low]=start
    for i in range(low,high_bin):out[i]=out[i-1]+step
    out[high_bin:]=end
    return out


def spectral_subtract(reference, left, right, *, enabled, start, end,
                      low_bin, high_bin, negative_gain):
    """12ADFF: subtract ramp*reference, retain positive and scale negative part.

    The negative clamp is [-float32(1e30),0], positive [0,float32(1e30)].
    This transforms supplied scalar spectra, not raw PCM or complex samples.
    """
    ref,a,b=vector(reference),vector(left),vector(right)
    if ref.shape!=a.shape or ref.shape!=b.shape:raise ValueError('matching spectra required')
    if type(enabled) is not bool:raise ValueError('explicit boolean enabled required')
    if not enabled:return a.copy(),b.copy()
    negative_gain=coefficients([negative_gain],1)[0]
    threshold=ref*spectral_ramp(bins=len(ref),start=start,end=end,low_bin=low_bin,high_bin=high_bin)
    cap=word_float(0x7149F2CA)
    def transform(x):
        residual=x-threshold
        return np.clip(residual,0,cap)+negative_gain*np.clip(residual,-cap,0)
    return transform(a),transform(b)


class BandLevelMaskReference:
    """12F90A: energy bands, two temporal estimates, asymmetric gain history.

    params is the 16-word parameter bank; enable is supplied separately as a
    boolean to avoid pretending to emulate unsigned float conversion. Fields
    2/3 power smoothing, 4/5 dB smoothing, 6..9 rising/falling gain smoothing,
    10/11 frequency smoothing, 12 slope, 13 threshold, 14 MAXIMUM /15 MINIMUM.
    12FA2E..12FA44 pushes these in reverse clamp order: 12F8E2 passes p15
    as R4 (lower), 12F8E0 passes p14 as R8 (upper) to 12BB49. This is NOT
    the transient bank's ascending p76/p77 order.
    History arrays are mandatory: library allocation/reset is out of scope.
    numeric_mode='firmware_library' requires explicit FIX rounding nearest or
    floor; math_reference.metadata() retains its proof/precision boundaries.
    """
    def __init__(self, *, bins, edges, params, power_history, db_history, gain_history,
                 numeric_mode='host_math', fix_rounding=None):
        if type(bins) is not int or bins<1:raise ValueError('positive bin count required')
        self.bins=bins;self.edges=band_edges(edges,bins);n=len(self.edges)-1
        self.params=coefficients(params,16).copy()
        self.power=coefficients(power_history,n).copy()
        self.db=coefficients(db_history,n).copy()
        self.gain=coefficients(gain_history,n).copy()
        if not 0<=self.params[15]<=self.params[14]:raise ValueError('ordered gain bounds p15 minimum <= p14 maximum required')
        if numeric_mode not in ('host_math','firmware_library'):
            raise ValueError('numeric_mode must be host_math or firmware_library')
        if numeric_mode=='host_math' and fix_rounding is not None:
            raise ValueError('FIX rounding is only meaningful for firmware_library')
        self.numeric_mode=numeric_mode;self.math_reference=None
        if numeric_mode=='firmware_library':
            from amp_b_math import BMathReference
            self.math_reference=BMathReference(fix_rounding=fix_rounding)

    def process(self, magnitude, *, enabled):
        x=vector(magnitude);p=self.params
        if len(x)!=self.bins or (x<0).any():raise ValueError('nonnegative magnitude per bin required')
        if type(enabled) is not bool:raise ValueError('explicit boolean enabled required')
        if not enabled:return np.ones_like(x)
        power=p[2]*band_sum(x*x,self.edges)+p[3]*self.power
        if not np.isfinite(power).all() or np.any(power<=-1):
            raise ValueError('power outside finite log(1+power) reference domain')
        fast=10*(np.log10(1+power) if self.math_reference is None else self.math_reference.log10(1+power))
        slow=p[4]*fast+p[5]*self.db
        raw=level_gain(fast-slow-p[13],slow,fast,slope=p[12],threshold_db=p[13],
                       minimum=p[15],maximum=p[14],math_reference=self.math_reference)
        # 131899 compares previous history with current raw gain.
        rising=self.gain<raw
        smoothed=np.where(rising,p[6]*raw+p[7]*self.gain,p[8]*raw+p[9]*self.gain)
        self.power=power;self.db=slow;self.gain=smoothed
        return frequency_smooth(expand_bands(smoothed,self.edges,self.bins),p[10],p[11])


class FrameVectorSmoother:
    """12D355/12D3E0: first enabled call primes history without changing input.

    Later calls output input_gain*x + history_gain*previous. Disabled calls
    bypass without advancing history or consuming the first-call flag.
    Each call is one firmware update, not an arbitrary audio chunk.
    """
    def __init__(self, *, size, history_gain, input_gain):
        if type(size) is not int or size<1:raise ValueError('positive vector size required')
        self.history_gain,self.input_gain=coefficients([history_gain,input_gain],2)
        self.history=np.zeros(size);self.first=True

    def reset(self):
        self.history.fill(0);self.first=True

    def process(self, values, *, enabled):
        x=vector(values)
        if x.shape!=self.history.shape:raise ValueError('matching history size required')
        if type(enabled) is not bool:raise ValueError('explicit boolean enabled required')
        if not enabled:return x.copy()
        y=x.copy() if self.first else self.input_gain*x+self.history_gain*self.history
        self.history=y.copy();self.first=False
        return y


class BandTransientMaskReference:
    """12F103: exactly 74 band sums and a squared transient ratio.

    params has 88 words: 2..75 per-band factors, 76/77 output clamps,
    78..81 level rise/fall, 82..85 gain rise/fall, 86/87 frequency smoothing.
    Input is the supplied scalar spectrum (NOT automatically squared here).
    Both histories must be supplied. Enable/shared-stereo dispatch belongs
    to caller 12F250 and is outside this class.
    """
    def __init__(self, *, bins, edges, params, level_history, gain_history):
        if type(bins) is not int or bins<1:raise ValueError('positive bin count required')
        self.bins=bins;self.edges=band_edges(edges,bins)
        if len(self.edges)!=75:raise ValueError('12F103 requires exactly 74 bands')
        self.params=coefficients(params,88).copy()
        self.level=coefficients(level_history,74).copy()
        self.gain=coefficients(gain_history,74).copy()
        if not 0<=self.params[76]<=self.params[77]:raise ValueError('ordered gain bounds required')

    def process(self, values):
        x=vector(values);p=self.params
        if len(x)!=self.bins or (x<0).any():raise ValueError('nonnegative scalar spectrum required')
        current=band_sum(x,self.edges)
        level=np.where(self.level<current,p[78]*current+p[80]*self.level,
                       p[79]*current+p[81]*self.level)
        raw=np.ones(74);active=level<current
        if np.any(current[active]<=0):raise ValueError('active ratio denominator must be positive')
        raw[active]=((p[2:76][active]*(current[active]-level[active])+level[active])/current[active])**2
        gain=np.where(self.gain<raw,p[82]*raw+p[83]*self.gain,
                      p[84]*raw+p[85]*self.gain)
        self.level=level;self.gain=gain
        expanded=expand_bands(gain,self.edges,self.bins)
        return np.clip(frequency_smooth(expanded,p[86],p[87]),p[76],p[77])
