"""First-B 12E80E spectral-wrapper composition, explicit configuration only.

Fixed 1024-sample FFT /512-hop /513-bin path, reconstructed from scratch-buffer
bindings. NOT whole amplifier or full131208 dispatcher. Floating references,
not instruction execution; lifecycle/errors/aliases outside successful path
remain unmodelled. No production or AmpChain defaults are changed.
"""
import numpy as np
from amp_b_spectral import bins, magnitude, normalize, sum_difference
from amp_b_adaptive import coefficients, spectral_subtract, BandTransientMaskReference
from amp_b_transform import forward_real, inverse_real, sine_window, _vector, _twiddles


def _selector(value):
    if type(value) is not int or not 0 <= value <= 0xFFFFFFFF:
        raise ValueError('explicit converted unsigned selector required')
    return value


def _pair(left, right, count):
    a, b = bins(left), bins(right)
    if a.shape != (count,) or b.shape != a.shape:
        raise ValueError('matching configured complex spectra required')
    return a, b


class StereoTransientReference:
    """12F250: shared==1 uses average scalar inputs and LEFT histories only.

    Other selectors use independent histories. Flat history layout:
    levelL[0:74], levelR[74:148], gainL[148:222], gainR[222:296]. Both the
    scalar spectra AND complex lanes are multiplied by the mask, once each.
    The scalar inputs are not squared or recomputed from complex inputs here.
    """
    def __init__(self, *, bins, edges, params, history):
        h = coefficients(history, 296)
        self.bins = bins
        self.left = BandTransientMaskReference(bins=bins, edges=edges, params=params,
                                               level_history=h[:74], gain_history=h[148:222])
        self.right = BandTransientMaskReference(bins=bins, edges=edges, params=params,
                                                level_history=h[74:148], gain_history=h[222:])

    def history(self):
        return np.concatenate((self.left.level, self.right.level, self.left.gain, self.right.gain))

    def process(self, left, right, scalar_left, scalar_right, *, enabled, shared_selector):
        a, b = _pair(left, right, self.bins)
        x, y = coefficients(scalar_left, self.bins), coefficients(scalar_right, self.bins)
        if type(enabled) is not bool or (x < 0).any() or (y < 0).any():
            raise ValueError('boolean enable and nonnegative scalar spectra required')
        _selector(shared_selector)
        if not enabled:
            return a.copy(), b.copy(), x.copy(), y.copy()
        if shared_selector == 1:
            left_gain = right_gain = self.left.process(.5*(x+y))
        else:
            left_gain, right_gain = self.left.process(x), self.right.process(y)
        return a*left_gain, b*right_gain, x*left_gain, y*right_gain


def crossmix(left, right, *, amount):
    """12E487: symmetric complex crossfeed, no amount clamp."""
    a, b = bins(left), bins(right)
    if a.shape != b.shape: raise ValueError('matching spectra required')
    amount = coefficients([amount], 1)[0]
    return (1-amount)*a+amount*b, (1-amount)*b+amount*a


def remodulate(scalar, phase):
    """130B6D: scalar times BOTH interleaved components; not magnitude repair."""
    p = bins(phase); a = coefficients(scalar, len(p))
    return a*p


class BSpectralFrameReference:
    """12E80E/12E1BB/12E30A/12E521, fixed explicit configuration.

    One call accepts stereo512 PCM, returns named pre/main/post stereo512
    branches, NOT speaker lanes. Main always runs within this wrapper; pre
    and post run only for converted library modes2/5, otherwise return None
    (firmware does not write these outputs here). Skipped histories freeze.
    131208 may bypass this wrapper entirely: do not equate this API to mode6
    dispatcher behavior or MMI3D. Default overlap/analysis histories are zero-
    reset test state; supplied component histories are retained, never reset.
    """
    def __init__(self, *, main_transient, post_transient, level_sum, level_difference,
                 spatial, smooth_left, smooth_right, pre_subtraction, crossfeed,
                 main_selector, post_selector, main_enabled, post_enabled,
                 level_enabled, spatial_enabled, smoothing_enabled,
                 window=None, twiddles=None, complex_twiddles=None,
                 analysis_history=None, synthesis_history=None):
        self.main_transient = main_transient; self.post_transient = post_transient
        self.level_sum = level_sum; self.level_difference = level_difference
        self.spatial = spatial; self.smooth_left = smooth_left; self.smooth_right = smooth_right
        for block in (main_transient, post_transient, level_sum, level_difference, spatial):
            if block.bins != 513: raise ValueError('513-bin components required')
        if smooth_left.history.shape != (256,) or smooth_right.history.shape != (256,):
            raise ValueError('256-bin magnitude smoothers required')
        if level_sum is level_difference or smooth_left is smooth_right or main_transient is post_transient:
            raise ValueError('independent firmware state objects required')
        if not np.array_equal(level_sum.params, level_difference.params):
            raise ValueError('sum/difference level banks share coefficients, not state')
        if (smooth_left.history_gain, smooth_left.input_gain) != (smooth_right.history_gain, smooth_right.input_gain):
            raise ValueError('left/right smoothers share coefficients, not state')
        self.pre_subtraction = dict(pre_subtraction)
        # Validate subtraction controls without advancing any component.
        spectral_subtract(np.zeros(513), np.zeros(513), np.zeros(513), **self.pre_subtraction)
        self.crossfeed = coefficients([crossfeed], 1)[0]
        self.main_selector = _selector(main_selector); self.post_selector = _selector(post_selector)
        for value in (main_enabled, post_enabled, level_enabled, spatial_enabled, smoothing_enabled):
            if type(value) is not bool: raise ValueError('explicit boolean switches required')
        self.main_enabled = main_enabled; self.post_enabled = post_enabled
        self.level_enabled = level_enabled; self.spatial_enabled = spatial_enabled
        self.smoothing_enabled = smoothing_enabled
        self.window = sine_window(1024) if window is None else _vector(window, 1024, float)
        self.twiddles = _twiddles(1024, twiddles)
        self.complex_twiddles = (None if complex_twiddles is None
                                 else _vector(complex_twiddles, 256, complex))
        self.analysis_history = self._stereo_history(analysis_history)
        if synthesis_history is None:
            synthesis_history = {key:None for key in ('pre', 'main', 'post')}
        if set(synthesis_history) != {'pre', 'main', 'post'}:
            raise ValueError('three separate synthesis histories required')
        self.synthesis_history = {k:self._stereo_history(v) for k,v in synthesis_history.items()}

    @staticmethod
    def _stereo_history(values):
        if values is None: return np.zeros((2, 512))
        a = np.asarray(values)
        if a.shape != (2, 512): raise ValueError('stereo history shape(2,512) required')
        return np.stack([_vector(row, 512, float) for row in a])

    def spectral_process(self, left, right, *, mode):
        """Boundary spectra, no FFT/overlap state update. Advances spectral states."""
        mode = _selector(mode); a, b = _pair(left, right, 513)
        mag_left, phase_left = normalize(a); mag_right, phase_right = normalize(b)
        pre = None; post = None
        if mode in (2, 5):
            # Magnitude of coherent sum, not average of independent magnitudes.
            reference = .5*magnitude(a+b)
            pl, pr = spectral_subtract(reference, mag_left, mag_right, **self.pre_subtraction)
            pre = (remodulate(pl, phase_left), remodulate(pr, phase_right))
        main_l, main_r, _, _ = self.main_transient.process(a, b, mag_left, mag_right,
            enabled=self.main_enabled, shared_selector=self.main_selector)
        if mode in (2, 5):
            # A4N/B0 contain ORIGINAL spectra saved before main transient.
            total, difference = sum_difference(a, b, half=False)
            total *= self.level_sum.process(magnitude(total), enabled=self.level_enabled)
            difference *= self.level_difference.process(magnitude(difference), enabled=self.level_enabled)
            pl, pr = sum_difference(total, difference, half=True)
            pl, pr = crossmix(pl, pr, amount=self.crossfeed)
            pl, pr = self.spatial.process(pl, pr, enabled=self.spatial_enabled)
            ml, ul = normalize(pl); mr, ur = normalize(pr)
            ml[:256] = self.smooth_left.process(ml[:256], enabled=self.smoothing_enabled)
            mr[:256] = self.smooth_right.process(mr[:256], enabled=self.smoothing_enabled)
            pl, pr = remodulate(ml, ul), remodulate(mr, ur)
            total, difference = sum_difference(pl, pr, half=False)
            total, difference, _, _ = self.post_transient.process(total, difference,
                magnitude(total), magnitude(difference), enabled=self.post_enabled,
                shared_selector=self.post_selector)
            post = sum_difference(total, difference, half=True)
        return {'pre':pre, 'main':(main_l, main_r), 'post':post}

    def process(self, left, right, *, mode):
        _selector(mode)
        x = np.stack([_vector(left, 512, float), _vector(right, 512, float)])
        spectra = [forward_real(np.concatenate((old, new))*self.window, twiddles=self.twiddles,
                                complex_twiddles=self.complex_twiddles)
                   for old, new in zip(self.analysis_history, x)]
        branches = self.spectral_process(*spectra, mode=mode)
        self.analysis_history = x
        out = {}
        for name, branch in branches.items():
            if branch is None:
                out[name] = None
                continue
            frames = np.stack([inverse_real(z, twiddles=self.twiddles,
                                complex_twiddles=self.complex_twiddles)*self.window for z in branch])
            out[name] = frames[:, :512]+self.synthesis_history[name]
            self.synthesis_history[name] = frames[:, 512:].copy()
        return out


class BMode2FixedReference:
    """131208 mode2: spectral pre->four, post->six, main->two, then finish.

    Inputs are the two library PCM lanes AFTER upstream input preparation.
    Fixed supplied tail coefficients/state only. Does not implement ready/
    transition handling, mode5's extra input preparation, mode6, parameter
    updates or vehicle profile selection. Not integrated into AmpChain.
    """
    def __init__(self, *, spectral, six_tail, four_tail, two_tail, finish):
        self.spectral = spectral; self.six_tail = six_tail
        self.four_tail = four_tail; self.two_tail = two_tail; self.finish = finish

    def process(self, left, right):
        return self._process_prepared(left, right, mode=2)

    def _process_prepared(self, left, right, *, mode):
        branch = self.spectral.process(left, right, mode=mode)
        # Actual dispatch calls two-tail before six-tail before four-tail.
        two = self.two_tail.process(np.asarray(branch['main']).T)
        six = self.six_tail.process(np.asarray(branch['post']).T)
        four = self.four_tail.process(np.asarray(branch['pre']).T)
        return self.finish.process(six, four, two)


def prepare_mode5(left, right, extra, *, input_mix_gain):
    """1315D3..131629: (S0 + g*S2, S1 + g*S2), 512 samples per lane.

    S2 is wrapper graph port22, NOT necessarily raw MOST centre PCM. Firmware
    copies S2 to P+512, applies object+97 once, then adds S0 into S+2048 and
    S1 into P+512. The gain is the effective linear scalar AFTER 12FAF2's
    per-call coefficient update; no dB conversion, averaging or clipping here.
    Reference is non-mutating; firmware workspace aliasing is not emulated.
    """
    if (not np.isscalar(input_mix_gain) or np.iscomplexobj(input_mix_gain)
            or not np.isfinite(input_mix_gain)):
        raise ValueError('finite real effective input-mix gain required')
    a, b, c = (_vector(v, 512, float) for v in (left, right, extra))
    shared = float(input_mix_gain)*c
    return np.stack((a+shared, b+shared))


class BMode5FixedReference(BMode2FixedReference):
    """Fixed mode5 from THREE wrapper PCM lanes to eight B outputs.

    Input gain object+97 is explicitly supplied, not inferred from Bank/MMI.
    Mode2/5 have the same recovered spectral/tail routing, but mode5 first
    adds the third input to both lanes. Normal init registers interpolation
    from object+299, so the update call skips object+97. Source selection,
    later writes, lifecycle and upstream transfer/gain/EQ remain outside this
    reference; do not confuse those stages with the downstream A07 matrix.
    """
    def __init__(self, *, input_mix_gain, **components):
        prepare_mode5(np.zeros(512), np.zeros(512), np.zeros(512),
                      input_mix_gain=input_mix_gain)
        super().__init__(**components)
        self.input_mix_gain = float(input_mix_gain)

    def process(self, left, right, extra):
        prepared = prepare_mode5(left, right, extra, input_mix_gain=self.input_mix_gain)
        return self._process_prepared(*prepared, mode=5)
