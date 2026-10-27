"""Isolated first-B real-FFT wrappers and 50%-overlap reference.

12B588/1330E6 packing equations are transcribed from the VISA listing, with
parallel operations reading OLD operands. The shared12EF65 complex FFT defaults
to numpy's unscaled forward FFT; explicit complex_twiddles instead selects the
stored-table DIF reference. Neither is instruction emulation. Ideal sine and
packing twiddles remain defaults; retained factories supply source-derived
tables. No bit-exact SHARC rounding or current-state claim.
"""
import numpy as np


def _size(n):
    if not isinstance(n, (int, np.integer)) or n < 8 or n & (n-1):
        raise ValueError('power-of-two frame size >=8 required')
    return int(n)


def _vector(x, n, dtype):
    a = np.asarray(x)
    if a.shape != (n,) or not np.isfinite(a).all():
        raise ValueError('finite vector of the expected size required')
    if dtype is float and np.iscomplexobj(a):
        raise ValueError('real samples required')
    return a.astype(dtype, copy=True)


def _twiddles(n, supplied):
    # PM E4754 in this image: interleaved cos(theta), -sin(theta).
    if supplied is None:
        return np.exp(-2j*np.pi*np.arange(n//4)/n)
    return _vector(supplied, n//4, complex)


def _complex_fft(samples, supplied):
    if supplied is None:
        return np.fft.fft(samples)
    from amp_b_fft import table_fft
    return table_fft(samples, supplied)


def forward_real(samples, *, twiddles=None, complex_twiddles=None):
    """12B588: N real samples -> N/2+1 complex bins; no forward gain."""
    n = _size(len(samples)); m = n//2
    x = _vector(samples, n, float); w = _twiddles(n, twiddles)
    z = _complex_fft(x[::2]+1j*x[1::2], complex_twiddles)
    out = np.empty(m+1, dtype=complex)
    out[0], out[m] = z[0].real+z[0].imag, z[0].real-z[0].imag
    for k in range(1, n//4):
        a, b, c, d = z[k].real, z[k].imag, z[m-k].real, z[m-k].imag
        u, v = w[k].real, w[k].imag
        ten, four, five, fifteen = a+c, a-c, b+d, b-d
        ten, eleven = ten+v*four, ten-v*four
        six, seven = ten+u*five, eleven-u*five
        eleven = v*five-u*four
        out[k] = .5*(six+1j*(eleven+fifteen))
        out[m-k] = .5*(seven+1j*(eleven-fifteen))
    out[n//4] = np.conj(z[n//4])
    return out


def inverse_real(spectrum, *, twiddles=None, complex_twiddles=None):
    """1330E6: prepack scaled by 1/N, then the SAME forward complex FFT.

    Imaginary DC/Nyquist values are ignored, as in the wrapper. Quarter-bin
    scale is 2/N. Replacing the final fft with ifft is incorrect.
    """
    n = _size(2*(len(spectrum)-1)); m = n//2
    x = _vector(spectrum, m+1, complex); w = _twiddles(n, twiddles)
    q = np.empty(m, dtype=complex)
    q[0] = ((x[0].real+x[m].real)+1j*(x[0].real-x[m].real))/n
    for k in range(1, n//4):
        a, b, c, d = x[k].real, x[k].imag, x[m-k].real, x[m-k].imag
        u, v = w[k].real, w[k].imag
        ten, four, five, fifteen = a+c, a-c, b+d, b-d
        ten, eleven = ten+v*four, ten-v*four
        six, seven = ten-u*five, eleven+u*five
        eleven = v*five+u*four
        q[m-k] = (six+1j*(eleven+fifteen))/n
        q[k] = (seven+1j*(eleven-fifteen))/n
    q[n//4] = 2*np.conj(x[n//4])/n
    packed = _complex_fft(q, complex_twiddles)
    out = np.empty(n)
    out[::2], out[1::2] = packed.real, packed.imag
    return out


def sine_window(n):
    """130A1C ideal sin((i+.5)*pi/N); 130AEB mirrors its stored first half.

    This is not a bit-exact model of the firmware's 125C2B trig helper.
    """
    n = _size(n)
    half = np.sin((np.arange(n//2)+.5)*np.pi/n)
    return np.concatenate((half, half[::-1]))


class NeutralOverlapReference:
    """Single lane, zero-reset 12D49F/window/FFT/IFFT/window/132B2B.

    Explicitly IDENTITY spectral processing; NOT the B upmixer. Each call
    consumes N/2 samples, returns N/2, with N/2 samples latency. Call once
    with zeros to flush the last hop. Per-lane state is independent.
    """
    def __init__(self, n=1024, *, window=None, twiddles=None):
        self.n = _size(n); self.hop = n//2
        self.window = sine_window(n) if window is None else _vector(window, n, float)
        self.twiddles = _twiddles(n, twiddles)
        self.reset()

    def reset(self):
        self.input_history = np.zeros(self.hop)
        self.output_history = np.zeros(self.hop)

    def process(self, samples):
        x = _vector(samples, self.hop, float)
        frame = np.concatenate((self.input_history, x))*self.window
        y = inverse_real(forward_real(frame, twiddles=self.twiddles),
                         twiddles=self.twiddles)*self.window
        out = self.output_history+y[:self.hop]
        self.input_history = x
        self.output_history = y[self.hop:].copy()
        return out
