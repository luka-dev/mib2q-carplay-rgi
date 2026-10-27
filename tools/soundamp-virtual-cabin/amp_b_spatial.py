"""Recovered first-B spatial statistics/controls/diffuse-mask references.

130771 successful-path math only: full buffer aliases, runtime parameter
selection, allocation/lifecycle and hardware execution are not supplied here. All history,
band layout and coefficients are explicit. Float64 sqrt/division stand in for
SHARC math/rounding. No gain is silently normalized and no live preset inferred.
"""
import numpy as np
from amp_b_spectral import bins as complex_bins, clamped_power, magnitude, word_float
from amp_b_adaptive import (vector, coefficients, band_edges, band_sum,
                            expand_bands, frequency_smooth)

COHERENCE_EPSILON = word_float(0x25800000)
BAND_POWER_MIN = word_float(0x3089705F)
BAND_POWER_MAX = word_float(0x7149F2CA)
COHERENCE_GATE = word_float(0x3F7D70A4)
RAMP_EPSILON = word_float(0x3A83126F)
MEAN_RECIPROCAL = word_float(0x3CA0A0A1)


def _pair(left, right):
    a, b = complex_bins(left), complex_bins(right)
    if a.shape != b.shape:
        raise ValueError('matching complex vectors required')
    return a, b


def coherence(power_left, power_right, cross):
    """13027B: magnitude(cross/(sqrt(PL*PR)+epsilon)), clamped to [0,1].

    131835's power floor survives: zero cross gives sqrt(float32(1e-10)),
    NOT zero coherence. This scalar floor is not additive audio. The complex
    normalization scratch is overwritten by magnitude; not retained state.
    """
    a, b, c = vector(power_left), vector(power_right), complex_bins(cross)
    if a.shape != b.shape or a.shape != c.shape or (a < 0).any() or (b < 0).any():
        raise ValueError('matching nonnegative powers and complex cross required')
    return np.clip(magnitude(c/(np.sqrt(a*b)+COHERENCE_EPSILON)), 0, 1)


class SpatialStatisticsReference:
    """130658: per-bin power floor -> band SUM -> temporal history.

    13039C/12ECC7 produce left*conj(right), including the imaginary sign.
    Output power clamps to [float32(1e-9),float32(1e30)] AFTER history is
    copied; do not replace the raw history with clamped output. Cross history
    is complex and is not clamped. Current/history gains are params11/12.
    """
    def __init__(self, *, bins, edges, direct, feedback,
                 left_history, right_history, cross_history):
        if type(bins) is not int or bins < 1:
            raise ValueError('positive bin count required')
        self.bins = bins; self.edges = band_edges(edges, bins)
        count = len(self.edges)-1
        self.direct, self.feedback = coefficients([direct, feedback], 2)
        self.left = coefficients(left_history, count).copy()
        self.right = coefficients(right_history, count).copy()
        self.cross = complex_bins(cross_history).copy()
        if self.cross.shape != self.left.shape:
            raise ValueError('one complex history per band required')

    def process(self, left, right):
        a, b = _pair(left, right)
        if len(a) != self.bins:
            raise ValueError('configured bin count required')
        cross = a*np.conj(b)
        raw_left = self.direct*band_sum(clamped_power(a), self.edges)+self.feedback*self.left
        raw_right = self.direct*band_sum(clamped_power(b), self.edges)+self.feedback*self.right
        raw_cross = self.direct*(band_sum(cross.real, self.edges)+
                                1j*band_sum(cross.imag, self.edges))+self.feedback*self.cross
        if not all(np.isfinite(x).all() for x in (raw_left, raw_right, raw_cross)):
            raise ValueError('finite reference-domain statistics required')
        self.left, self.right, self.cross = raw_left, raw_right, raw_cross
        return (np.clip(raw_left, BAND_POWER_MIN, BAND_POWER_MAX),
                np.clip(raw_right, BAND_POWER_MIN, BAND_POWER_MAX), raw_cross.copy())


def fixed_mean(values, *, previous, direct, feedback):
    """130321: entries9..59 inclusive, NOT a frequency-Hz interval.

    The caller supplies band coherence. Preserve the reciprocal correction
    sequence in the finite domain, not its NaN/overflow/AV behavior. 130341
    MANT only checks AV; its integer result is not used as a signal value.
    """
    x = vector(values)
    if len(x) < 60:
        raise ValueError('fixed mean reads at least 60 entries')
    previous, direct, feedback = coefficients([previous, direct, feedback], 3)
    total = np.sum(x[9:60]); mean = total*MEAN_RECIPROCAL
    mean -= (mean*51-total)*MEAN_RECIPROCAL
    return direct*mean+feedback*previous


class SpatialControlsReference:
    """130359 minimum,130321 fixed mean,130237 minimum-driven global gate.

    Explicit 28-word bank; min/mean use p13/14. Gate target is zero only if
    smoothed minimum > float32(.99), else one. Rising p23/24, falling p25/26.
    State corresponds to object pointers+9(min),+8(mean),+7(gate). This is
    the successful update math, not caller error-path commit semantics.
    """
    def __init__(self, *, params, minimum_history, mean_history, gate_history):
        self.params = coefficients(params, 28).copy()
        self.minimum, self.mean, self.gate = coefficients(
            [minimum_history, mean_history, gate_history], 3)

    def process(self, band_coherence):
        x = vector(band_coherence); p = self.params
        if len(x) < 60 or (x < 0).any() or (x > 1).any():
            raise ValueError('at least 60 coherence values in [0,1] required')
        minimum = p[13]*np.min(x)+p[14]*self.minimum
        mean = fixed_mean(x, previous=self.mean, direct=p[13], feedback=p[14])
        target = 0. if minimum > COHERENCE_GATE else 1.
        g, h = p[23:25] if self.gate < target else p[25:27]
        self.minimum, self.mean = minimum, mean
        self.gate = g*target+h*self.gate
        return {'minimum':minimum, 'fixed_mean':mean, 'target':target, 'gate':self.gate}


def spatial_ramp(*, bins, start, end, low_bin, high_bin, step):
    """1303B5 valid bounds: supplied step, NOT (end-start)/(high-low).

    Add float32(.001) to endpoints below that value (not max(endpoint,.001)).
    Both zero indices become1 AFTER bounds validation. Invalid firmware path
    clears output and reports error; this API rejects it instead.
    """
    if (type(bins) is not int or type(low_bin) is not int or type(high_bin) is not int
            or not 0 <= low_bin <= high_bin < bins):
        raise ValueError('require 0 <= low_bin <= high_bin < bins')
    start, end, step = coefficients([start, end, step], 3)
    if start < RAMP_EPSILON: start += RAMP_EPSILON
    if end < RAMP_EPSILON: end += RAMP_EPSILON
    lo, hi = max(1, low_bin), max(1, high_bin)
    out = np.empty(bins); out[:lo] = start
    for i in range(lo, hi): out[i] = out[i-1]+step
    out[hi:] = end
    return out


def frequency_blend(values, *, low_direct, low_feedback, high_direct, high_feedback):
    """13045E: two independent smooths, blended with w[i]=(N-i)/(N+1e-30).

    Last bin still has 1/N of the low version; this is not linspace(1,0,N).
    """
    x = vector(values)
    low = frequency_smooth(x, low_direct, low_feedback)
    high = frequency_smooth(x, high_direct, high_feedback)
    weight = (len(x)-np.arange(len(x)))/(len(x)+word_float(0x0DA24260))
    return weight*low+(1-weight)*high


class SpatialDiffuseReference:
    """1304E4: sqrt(1-coherence) -> history -> expand -> smooth/blend -> clamp.

    p19/20 temporal; LOW p17/18, HIGH p15/16 (not argument-order guess);
    clamp p21/22, then multiply both complex components of both input lanes.
    History stores pre-expansion/pre-clamp values. Explicit boundary spectra
    only; separator and final blend are separate helpers below.
    """
    def __init__(self, *, bins, edges, params, history):
        if type(bins) is not int or bins < 1:
            raise ValueError('positive bin count required')
        self.bins = bins; self.edges = band_edges(edges, bins)
        self.params = coefficients(params, 28).copy()
        self.history = coefficients(history, len(self.edges)-1).copy()
        if self.params[21] > self.params[22]:
            raise ValueError('ordered mask bounds required')

    def process(self, left, right, band_coherence):
        a, b = _pair(left, right); x = vector(band_coherence); p = self.params
        if len(a) != self.bins or x.shape != self.history.shape or (x < 0).any() or (x > 1).any():
            raise ValueError('configured spectra and band coherence in [0,1] required')
        history = p[19]*np.sqrt(1-x)+p[20]*self.history
        if not np.isfinite(history).all():
            raise ValueError('finite reference-domain history required')
        expanded = expand_bands(history, self.edges, self.bins)
        mask = np.clip(frequency_blend(expanded, low_direct=p[17], low_feedback=p[18],
                                      high_direct=p[15], high_feedback=p[16]), p[21], p[22])
        self.history = history
        return a*mask, b*mask, mask


def separator_coefficients(power_left, power_right, cross, ramp, *, eigen_selector,
                           regularization, fallback_scale, balance_enabled,
                           mean_coherence, balance_slope, balance_offset):
    """12FFBA finite-domain band matrices; explicit converted selectors.

    eigen_selector is rounded p1; regularization=p9, fallback_scale=p10;
    balance_enabled is rounded p2!=0. Balance slope/offset are object+10/+11,
    NOT parameter-bank fields. No runtime values guessed. Eigen estimate uses
    Re(cross)^2 ONLY, even though the subsequent matrix uses complex cross.

    Returns matrices [band,output,input]. Powers are floored locally to
    float32(1e-6); firmware writes those floors into its working power arrays.
    This API does not mutate caller buffers or emulate exceptional arithmetic.
    """
    a, b, c, r = vector(power_left), vector(power_right), complex_bins(cross), vector(ramp)
    if a.shape != b.shape or a.shape != c.shape or a.shape != r.shape or (a < 0).any() or (b < 0).any():
        raise ValueError('matching nonnegative powers, complex cross and band ramp required')
    if type(eigen_selector) is not int or not 0 <= eigen_selector <= 0xFFFFFFFF:
        raise ValueError('explicit converted unsigned selector required')
    if type(balance_enabled) is not bool:
        raise ValueError('explicit boolean balance enable required')
    regularization, fallback_scale, mean_coherence, balance_slope, balance_offset = coefficients(
        [regularization, fallback_scale, mean_coherence, balance_slope, balance_offset], 5)
    floor = word_float(0x358637BD)
    epsilon = word_float(0x0DA24260)
    scale = np.full(a.shape, fallback_scale*1e6)
    if eigen_selector == 1:
        smallest = .5*(a+b-np.sqrt((a-b)**2+4*c.real**2))
        use = smallest > floor
        scale[use] = 1/(smallest[use]+epsilon)
    a, b = np.maximum(a, floor), np.maximum(b, floor)
    denominator = (a+b)*scale-2+10*regularization
    if (denominator == 0).any() or not np.isfinite(denominator).all():
        raise ValueError('finite nonzero separator denominator required')
    factor = r/denominator
    left_diagonal = (a*scale-1)*factor
    right_diagonal = (b*scale-1)*factor
    coupling = c*scale*factor
    left_weight = np.ones(len(a)); right_weight = left_weight.copy()
    if balance_enabled:
        ratio = (a-b)/(a+b+epsilon)
        reciprocal = 1/(1+np.abs(ratio))
        left = np.where(ratio < 0, reciprocal, 1.)
        right = np.where(ratio < 0, 1., reciprocal)
        blend = np.clip(balance_slope*mean_coherence+balance_offset, 0, 1)
        left_weight = blend*left**2+(1-blend)*left
        right_weight = blend*right**2+(1-blend)*right
        left_diagonal = 1+left_weight*(left_diagonal-1)
        right_diagonal = 1+right_weight*(right_diagonal-1)
    result = np.empty((len(a), 2, 2), complex)
    result[:, 0, 0], result[:, 1, 1] = left_diagonal, right_diagonal
    result[:, 0, 1] = left_weight*coupling
    result[:, 1, 0] = right_weight*np.conj(coupling)
    if not np.isfinite(result).all():
        raise ValueError('finite reference-domain matrix required')
    return result


def spatial_separator(left, right, *, edges, matrices):
    """12FFBA band-wise 2x2 complex multiply, preserving half-open band bounds."""
    a, b = _pair(left, right); edges = band_edges(edges, len(a))
    matrices = np.asarray(matrices, dtype=complex)
    if matrices.shape != (len(edges)-1, 2, 2) or not np.isfinite(matrices).all():
        raise ValueError('finite 2x2 matrix for each band required')
    expanded = np.repeat(matrices, np.diff(edges), axis=0)
    return (expanded[:, 0, 0]*a+expanded[:, 0, 1]*b,
            expanded[:, 1, 0]*a+expanded[:, 1, 1]*b)


def spatial_finish(left, right, separated_left, separated_right,
                   diffuse_left, diffuse_right, *, gate_selector, gate, mix):
    """130901..130A0B: residual -> optional gate -> blend with diffuse branch.

    Inputs to this helper are explicit boundary spectra: separated is the
    12FFBA output; diffuse is 1304E4 output. Residual=input-
    separated, gate applies ONLY when rounded p3==1. Mix is p27 and is NOT
    clamped or normalized here. BTGL by zero followed by SZ tests ==1,
    not arbitrary nonzero. This is not the p0==0 bypass path.
    """
    a, b = _pair(left, right); c, d = _pair(separated_left, separated_right)
    e, f = _pair(diffuse_left, diffuse_right)
    if a.shape != c.shape or a.shape != e.shape:
        raise ValueError('matching boundary spectra required')
    if type(gate_selector) is not int or not 0 <= gate_selector <= 0xFFFFFFFF:
        raise ValueError('explicit converted unsigned selector required')
    gate, mix = coefficients([gate, mix], 2)
    gain = gate if gate_selector == 1 else 1.
    return (1-mix)*gain*(a-c)+mix*e, (1-mix)*gain*(b-d)+mix*f


class SpatialFrameReference:
    """130771 composed successful path with explicit selector conversions/state.

    p0/p1/p2/p3 and p6/p7 have been converted by the firmware caller; provide
    their meanings explicitly as enabled/eigen_selector/balance_enabled/
    gate_selector/low_band/high_band. Other coefficients come from params.
    At least60 bands are required by the fixed mean read. This is a boundary-
    spectrum model, not a complete FFT upmixer or active-vehicle configuration.
    Exceptions are outside the reference domain; do not continue such a stream.
    """
    def __init__(self, *, bins, edges, params, eigen_selector, balance_enabled,
                 gate_selector, low_band, high_band, balance_slope, balance_offset,
                 left_history, right_history, cross_history, diffuse_history,
                 minimum_history, mean_history, gate_history):
        self.params = coefficients(params, 28).copy(); p = self.params
        self.edges = band_edges(edges, bins); self.bins = bins
        count = len(self.edges)-1
        if count < 60:
            raise ValueError('130321 requires at least60 bands')
        for selector in (eigen_selector, gate_selector):
            if type(selector) is not int or not 0 <= selector <= 0xFFFFFFFF:
                raise ValueError('explicit converted unsigned selectors required')
        if type(balance_enabled) is not bool:
            raise ValueError('explicit boolean balance enable required')
        self.eigen_selector = eigen_selector; self.gate_selector = gate_selector
        self.balance_enabled = balance_enabled
        self.balance_slope, self.balance_offset = coefficients([balance_slope, balance_offset], 2)
        self.ramp = spatial_ramp(bins=count, start=p[4], end=p[5], low_bin=low_band,
                                 high_bin=high_band, step=p[8])
        self.statistics = SpatialStatisticsReference(bins=bins, edges=self.edges,
            direct=p[11], feedback=p[12], left_history=left_history,
            right_history=right_history, cross_history=cross_history)
        self.controls = SpatialControlsReference(params=p, minimum_history=minimum_history,
                                                   mean_history=mean_history, gate_history=gate_history)
        self.diffuse = SpatialDiffuseReference(bins=bins, edges=self.edges, params=p,
                                               history=diffuse_history)

    def process(self, left, right, *, enabled):
        a, b = _pair(left, right)
        if type(enabled) is not bool or len(a) != self.bins:
            raise ValueError('explicit enable and configured spectra required')
        if not enabled:
            return a.copy(), b.copy()
        p = self.params
        power_left, power_right, cross = self.statistics.process(a, b)
        coh = coherence(power_left, power_right, cross)
        control = self.controls.process(coh)
        matrices = separator_coefficients(power_left, power_right, cross, self.ramp,
            eigen_selector=self.eigen_selector, regularization=p[9], fallback_scale=p[10],
            balance_enabled=self.balance_enabled, mean_coherence=control['fixed_mean'],
            balance_slope=self.balance_slope, balance_offset=self.balance_offset)
        sep_left, sep_right = spatial_separator(a, b, edges=self.edges, matrices=matrices)
        dif_left, dif_right, _ = self.diffuse.process(a, b, coh)
        return spatial_finish(a, b, sep_left, sep_right, dif_left, dif_right,
                              gate_selector=self.gate_selector, gate=control['gate'], mix=p[27])
