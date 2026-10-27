"""C4D17 control reference, NOT a complete audio block or live-state model.

Evidence: VISA 1274E9..127826, helper 12796D; V850 AAC88/AAFC0.
Inputs are the two already measured five-band powers (descriptor +135/+136).
Upstream FIR, decimation, power history and scheduler remain caller-owned.
Float64 math is intentional; this is not bit-exact SHARC execution.
"""
import math
import struct

import numpy as np


def _f(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


P = _f(0x3E8A3D71)
Q = _f(0x3FED097B)


def log_reference(x):
    """124393 positive-domain rational approximation, exact reciprocal here.

    Firmware sets errno=33 and returns zero for nonpositive inputs. The
    reference returns that value but does not emulate errno or IEEE flags.
    Constants from loader DM B9393. RECIPS/Newton rounding is not emulated.
    """
    if not math.isfinite(x):
        raise ValueError('finite log input required')
    if x <= 0:
        return 0.0
    m, n = math.frexp(x)
    if m > _f(0x3F3504F3):
        z = (m - 1) / ((m + 1) / 2)
    else:
        z = (m - 0.5) / (m / 2 + 0.25)
        n -= 1
    w = z*z
    correction = z*w*(0.013600953854620457*w - 0.4649062156677246)/(w - 5.578873634338379)
    return z + correction + n*(0.693359375 - 0.0002121944271493703)


def exp_reference(x):
    """12512E rational approximation; round-to-nearest FIX assumption.

    Constants from DM B9261. Overflow/underflow branches reproduce returned
    finite saturation values, not processor flags/denormal behavior.
    """
    if not math.isfinite(x):
        raise ValueError('finite exp input required')
    if x > 127: return _f(0x7F7FFFFF)
    if x < -88: return _f(0x00800000)
    if abs(x) < 1e-9: return 1.0
    n = round(x * 1.4426950216293335)
    r = (x - n*0.693359375) - n*(-0.00021219444170128554)
    w = r*r
    a = r*(0.25 + 0.005950425285845995*w)
    b = 0.5 + w*(0.053567517548799515 + 0.0002972936490550637*w)
    return math.ldexp(0.5 + a/(b-a), n+1)


def adaptive_target(x, y, parameters):
    """Helper 12796D; params are descriptor fields [2,3,4,5].

    These are power-domain inputs, not PCM samples. The returned value is
    subsequently used as an amplitude multiplier by the five-band kernel.
    """
    xmin, ymin, subtract, cap = np.asarray(parameters, dtype=float)
    if not np.isfinite([x,y,xmin,ymin,subtract,cap]).all() or min(x,y) < 0:
        raise ValueError('finite nonnegative powers and finite parameters required')
    if y <= ymin or x <= xmin:
        return 1.0
    a = exp_reference(-P * log_reference(min(x, 1)))
    b = exp_reference(P * log_reference(min(y, 1)))
    result = exp_reference(Q * log_reference(1 + (b-subtract)*a))
    return max(1.0, min(result, cap))


def shape_powers(x, y, *, x_db, y_db, cumulative_min=False):
    """Stage 19 after history reduction: +135=x, +136=y.

    x_db = table49 + field19 - field21; y_db = table50 + field20.
    Despite power-domain operands, firmware uses 0.05*ln(10), NOT 0.1:
    127569/1275AC load/reuse 0x3d4ccccd. Preserve the actual /20 law.
    Field37 bit0 applies a prefix minimum to y, not a global minimum.
    """
    x, y = _five(x, 'x'), _five(y, 'y')
    if min(x.min(), y.min()) < 0: raise ValueError('powers must be nonnegative')
    x *= 10**(_five(x_db, 'x_db')/20)
    y *= 10**(_five(y_db, 'y_db')/20)
    return x, np.minimum.accumulate(y) if cumulative_min else y


def _five(value, name):
    out = np.broadcast_to(np.asarray(value, dtype=float), (5,)).copy()
    if not np.isfinite(out).all(): raise ValueError(f'{name} must be finite')
    return out


class ComplexControl:
    """Stages 20..26 at one target-update boundary, full control mask only.

    Every runtime gate/parameter is explicit. This class does not assume the
    vehicle uses full mode or stage9 actually applies the resulting gains.
    State defaults are neutral initialization, not a playback-state assertion.
    """
    def __init__(self):
        self.adaptive = np.ones(5)       # +52
        self.other = np.ones(5)          # +60
        self.endpoint = np.ones(5)       # +68
        self.common = 1.0               # +65 at entry
        self.previous_cap_code = 0       # +9
        self.previous_enable = 0         # +8

    def update(self, x, y, *, helper_parameters, y_floor_db, cap_code,
               common_code, adaptive_enable, inhibit, cap_enable, freeze,
               link_upper, other_base, other_scale, adaptive_rates,
               other_rates, force_smoothing, smoothing_stage=True):
        x, y = _five(x, 'x'), _five(y, 'y')
        p = np.asarray(helper_parameters, dtype=float)
        if p.shape != (5,4) or not np.isfinite(p).all():
            raise ValueError('helper_parameters must be finite 5 x 4')
        if min(x.min(), y.min()) < 0: raise ValueError('powers must be nonnegative')
        for rates in (adaptive_rates, other_rates):
            if len(rates) != 2 or not np.isfinite(rates).all() or not 0 <= rates[1] <= rates[0]:
                raise ValueError('rates must be finite [up, down], up >= down >= 0')
        if not np.isfinite([cap_code,common_code,other_scale]).all():
            raise ValueError('finite control values required')
        old_common = self.common
        # 1275CF / 12761D: 0x3d4ccccd=.05, not .1. Ideal mathematical
        # equivalent; instruction-by-instruction float rounding is excluded.
        common = 10**(-common_code/20)  # +65, stage20
        y = np.maximum(y, 10**(_five(y_floor_db, 'y_floor_db')/20))
        target = np.ones(5)
        # At 1276A1 NOT SZ jumps to the linking branch: field38=1 links.
        for i in range(3 if link_upper & 1 else 5):
            target[i] = adaptive_target(x[i], y[i], p[i])
        if link_upper & 1: target[3] = (target[2]+1)/2
        if freeze & 1:                 # stage22
            target = self.adaptive.copy()
            common = old_common
        if not (adaptive_enable & ~inhibit & 1):  # stage23, BEFORE rate clamp
            target[:] = 1
            common = 1.0
        other = _five(other_base, 'other_base') * other_scale  # stage24
        smooth = smoothing_stage and (force_smoothing & 1 or
            (cap_code == self.previous_cap_code and adaptive_enable == self.previous_enable))
        if smooth:
            target = np.maximum(np.minimum(target, self.adaptive*adaptive_rates[0]),
                                self.adaptive*adaptive_rates[1])
            other = np.maximum(np.minimum(other, self.other*other_rates[0]),
                               self.other*other_rates[1])
            common = max(min(common, old_common*adaptive_rates[0]), old_common*adaptive_rates[1])
        cap = 10**(cap_code/20) if cap_enable & ~inhibit & 1 else 1.0
        endpoint = np.minimum(np.maximum(target*other, 1), cap)
        result = {'gains': self.endpoint.copy(), 'steps': (endpoint-self.endpoint)/1024,
                  'endpoint': endpoint.copy(), 'adaptive': target.copy(),
                  'other': other.copy(), 'common_gain': old_common,
                  'common_step': (common-old_common)/1024, 'common_endpoint': common,
                  'smoothing_applied': bool(smooth)}
        self.adaptive, self.other, self.endpoint, self.common = target, other, endpoint, common
        self.previous_cap_code, self.previous_enable = cap_code, adaptive_enable
        return result


class AdaptiveResidual:
    """125F36: scalar FIR-residual/LMS reference, canonical tap ordering.

    Ten reference lanes, one auxiliary lane, 512 taps per reference. h[c,0]
    multiplies the current weighted reference sample. The hardware circular
    memory packing is NOT emulated. This is an isolated reconstruction, not
    a claim that graph input 11 is a particular physical microphone.

    Each call is ONE detector block: power-dependent step selection happens
    once per call. Arbitrarily repartitioning calls changes that selection.
    """
    POWER_STEP = np.array([1,.5,.25,.1666666567325592,.125,
        .09999999403953552,.0833333283662796,.0714285671710968,
        .0625,.04999999701976776,0])

    def __init__(self, coefficients, *, reference_gains, auxiliary_gain,
                 step, rounding, power_state=0):
        self.coefficients = np.asarray(coefficients,dtype=float).copy()
        if self.coefficients.shape != (10,512) or not np.isfinite(self.coefficients).all():
            raise ValueError('finite 10 x 512 coefficient bank required')
        self.reference_gains = np.asarray(reference_gains,dtype=float)
        if self.reference_gains.shape != (10,) or not np.isfinite(self.reference_gains).all():
            raise ValueError('ten finite reference gains required')
        if not np.isfinite([auxiliary_gain,step,power_state]).all() or step<0 or power_state<0:
            raise ValueError('invalid auxiliary gain, step or initial power')
        self.auxiliary_gain=float(auxiliary_gain)
        self.step=float(step)
        self.power_state=float(power_state)
        if rounding not in ('nearest','floor'):
            raise ValueError('explicit FIX rounding mode nearest or floor required')
        self.rounding=rounding
        self.history=np.zeros((10,512))

    def process(self, references, auxiliary):
        x=np.asarray(references,dtype=float)
        y=np.asarray(auxiliary,dtype=float)
        if x.ndim!=2 or x.shape[1]!=10 or y.shape!=(len(x),) or not np.isfinite(x).all() or not np.isfinite(y).all():
            raise ValueError('finite frames x 10 references and frames auxiliary required')
        if not len(x): return y.copy()
        x=x*self.reference_gains
        power=np.sum(x*x)/len(x)
        if power>=self.power_state:
            self.power_state=power
        else:
            self.power_state=0.017799999564886093*power+0.9821999669075012*self.power_state
        # 125FB9 sets ALUSAT (bit13), NOT TRUNC (bit15). The caller's
        # incoming rounding mode therefore still controls FIX at 125FBF.
        index=min(10,max(0,round(self.power_state) if self.rounding=='nearest'
                           else math.floor(self.power_state)))
        effective_step=self.step*self.POWER_STEP[index]
        residual=np.empty(len(x))
        for t,row in enumerate(x):
            self.history[:,1:]=self.history[:,:-1]
            self.history[:,0]=row
            error=y[t]*self.auxiliary_gain-np.sum(self.coefficients*self.history)
            residual[t]=error
            self.coefficients += error*effective_step*self.history
        return residual
