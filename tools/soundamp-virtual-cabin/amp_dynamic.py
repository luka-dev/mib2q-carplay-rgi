"""Reconstructed J525 gain/limiter kernels, not a complete runtime emulator.

Evidence: VISA 12879C..128910 and 12823C..1283F6, ADI rev 2.4.
Finite float64 reference math; no assertion of SHARC rounding equivalence.
Callers MUST supply runtime parameters; loader state is not playback state.
"""
import numpy as np
from scipy.signal import lfilter


class ComplexStereoDelay:
    """1279ED: write, advance, read next slot of a (delay+1) stereo ring.

    Five independent stereo pairs in C4D17 stage8. Parameters are explicit:
    its loader uses requested=maximum=1023, not a zero-delay identity.
    Changing delays/ring pointers during playback is outside this kernel.
    """
    def __init__(self, requested, maximum):
        a, b = np.asarray(requested), np.asarray(maximum)
        if a.shape != (5,) or b.shape != (5,) or a.dtype.kind not in 'iu' or b.dtype.kind not in 'iu':
            raise ValueError('five integer requested/maximum delays required')
        self.delay = np.maximum(np.minimum(a,b),0)
        self.buffers = [np.zeros((int(d)+1,2)) for d in self.delay]
        self.positions = [0]*5

    def process(self, audio):
        x=np.asarray(audio,dtype=float)
        if x.ndim != 2 or x.shape[1] != 10 or not np.isfinite(x).all():
            raise ValueError('expected finite frames x 10 audio')
        out=np.empty_like(x)
        for c, buf in enumerate(self.buffers):
            p=self.positions[c]
            for i in range(len(x)):
                buf[p]=x[i,2*c:2*c+2]
                p=(p+1)%len(buf)
                out[i,2*c:2*c+2]=buf[p]
            self.positions[c]=p
        return out


class ComplexFiveBand:
    """C4D17 stage-bit 9 / helper 127852, ONLY the five-band audio kernel.

    10 channels form five SIMD stereo pairs, sharing five band gains. Four
    complementary LP/HP splits successively operate on the HP remainder.
    Coefficients per split are [[feedback, b1, b0] LP, same HP]. Initial filter
    states are zero. Gain for sample t is current + (t+1)*step. The C4D17
    detector, gain-target generation and other stages are NOT included.
    """
    def __init__(self, coefficients, *, gains, steps):
        self.coefficients = np.asarray(coefficients, dtype=float)
        if self.coefficients.shape != (4, 2, 3) or not np.isfinite(self.coefficients).all():
            raise ValueError('expected four LP/HP coefficient triples')
        self.gains = _vector(gains, 5, 'gains')
        self.steps = _vector(steps, 5, 'steps')
        self.state = np.zeros((4, 2, 1, 10))

    def process(self, audio):
        x = np.asarray(audio, dtype=float)
        if x.ndim != 2 or x.shape[1] != 10 or not np.isfinite(x).all():
            raise ValueError('expected finite frames x 10 audio')
        if not len(x): return x.copy()
        bands, remainder = [], x
        for i, pair in enumerate(self.coefficients):
            outputs = []
            for j, (feedback, b1, b0) in enumerate(pair):
                y, self.state[i,j] = lfilter([b0,b1], [1,-feedback], remainder,
                                             axis=0, zi=self.state[i,j])
                outputs.append(y)
            bands.append(outputs[0]); remainder = outputs[1]
        bands.append(remainder)
        ramp = self.gains + np.arange(1,len(x)+1)[:,None] * self.steps
        out = sum(band * ramp[:,i,None] for i,band in enumerate(bands))
        self.gains += len(x) * self.steps
        return out


def _vector(value, channels, name):
    v = np.broadcast_to(np.asarray(value, dtype=float), (channels,)).copy()
    if not np.isfinite(v).all():
        raise ValueError(f'{name} must be finite')
    return v


def gain_step(current, target, *, mode, rates=None):
    """One audio-block update of descriptor +9 toward target +12.

    Mode 0 immediate, 2 additive magnitude steps, 3 multiplicative steps.
    Rates are [up, down] pairs, not per-sample increments. Mode 1 derives
    these steps using additional state: see amp_second_b.mode1_gain_update.
    This low-level function still accepts only direct modes 0, 2, 3.
    COPYSIGN applies the target sign after magnitude smoothing.
    """
    current = np.atleast_1d(np.asarray(current, dtype=float))
    if current.ndim != 1 or not np.isfinite(current).all():
        raise ValueError('current must be a finite vector')
    target = _vector(target, len(current), 'target')
    if mode == 0:
        return target
    if mode not in (2, 3):
        raise ValueError('only decoded direct modes 0, 2, 3 are supported')
    rates = np.asarray(rates, dtype=float)
    if rates.shape != (len(current), 2) or not np.isfinite(rates).all():
        raise ValueError('rates must be channels x 2 finite [up, down] pairs')
    a, b = np.abs(current), np.abs(target)
    if mode == 3:
        # 0x128872 is conditional on ABS(target), not ABS(current).
        a = np.where(b != 0, np.maximum(a, float.fromhex('0x1.4f8b58p-17')), a)
    decreasing = a > b
    step = np.where(decreasing, rates[:, 1], rates[:, 0])
    candidate = a * step if mode == 3 else a + step
    magnitude = np.where(decreasing, np.maximum(candidate, b), np.minimum(candidate, b))
    return np.copysign(magnitude, target)


class PeakLimiter:
    """VISA 12823C: output-feedback peak gain control, no lookahead/clipping.

    descriptor[15]&1 == 0: independent channels, NOT bypass.
    descriptor[15]&1 == 1: shared gain, any channel can trigger attack;
    the shared branch uses channel-zero attack/release/bounds and initial gain.
    Output uses old gain; threshold decision sets gain for the next sample.
    State survives calls; this also tests block partition invariance.
    """
    def __init__(self, threshold, attack, release, *, minimum, maximum,
                 initial, linked=False):
        self.threshold = np.atleast_1d(np.asarray(threshold, dtype=float))
        if self.threshold.ndim != 1 or not len(self.threshold):
            raise ValueError('threshold must be a nonempty vector')
        n = len(self.threshold)
        self.attack = _vector(attack, n, 'attack')
        self.release = _vector(release, n, 'release')
        self.minimum = _vector(minimum, n, 'minimum')
        self.maximum = _vector(maximum, n, 'maximum')
        self.gain = _vector(initial, n, 'initial')
        self.linked = bool(linked)
        if (not np.isfinite(self.threshold).all() or (self.threshold < 0).any()
                or (self.minimum < 0).any() or (self.maximum < self.minimum).any()
                or (self.attack < 0).any() or (self.release < 0).any()):
            raise ValueError('invalid limiter parameters')

    def process(self, audio):
        x = np.asarray(audio, dtype=float)
        if x.ndim != 2 or x.shape[1] != len(self.threshold) or not np.isfinite(x).all():
            raise ValueError('audio must be finite frames x channels')
        y = np.empty_like(x)
        for i, sample in enumerate(x):
            if self.linked:
                y[i] = sample * self.gain[0]
                rate = self.attack[0] if np.any(np.abs(y[i]) > self.threshold) else self.release[0]
                self.gain[:] = np.clip(self.gain[0] * rate, self.minimum[0], self.maximum[0])
            else:
                y[i] = sample * self.gain
                rate = np.where(np.abs(y[i]) > self.threshold, self.attack, self.release)
                self.gain = np.clip(self.gain * rate, self.minimum, self.maximum)
        return y
