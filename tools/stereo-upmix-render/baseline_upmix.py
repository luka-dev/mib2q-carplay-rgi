#!/usr/bin/env python3
"""Classical upmix baselines for the stem harness (astra review 5: reviewers want them).

Same CLI shape as the renderer so `upmix_stem_tests.py --render "python baseline_upmix.py --mode pad"`
works: every renderer flag is ignored, the last two positionals are the stereo input and the 6-ch
output (48 kHz int16, L R C LFE Ls Rs like the renderer).

  matrix  passive surround matrix: S = (L - R) / sqrt 2, band-limited to 7 kHz, delayed 12 ms, to both
          rears at -3 dB; fronts untouched.  Centre-panned sources cancel out of the rear by construction.
  pad     primary / ambient decomposition after Avendano & Jot (2004), simplified: per STFT cell the
          short-time inter-channel coherence Phi = |<L R*>| / sqrt(<|L|^2><|R|^2>) (exponentially
          smoothed, ~50 ms); ambient mask a = 1 - Phi; rears = a * X delayed 12 ms; fronts keep the
          primary plus half of the ambient (our renderer's front retention 0.5).
"""
import sys

import numpy as np

SR = 48000
N_FFT, HOP = 1024, 256
REAR_DELAY = int(0.012 * SR)


def read_wav(path):
    import wave
    with wave.open(path, "rb") as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, w.getnchannels()).T.astype(np.float32) / 32768
    return x


def write_wav6(path, y):
    import wave
    pcm = (np.clip(y, -1, 1).T * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(6); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())


def delay(x, n):
    return np.concatenate([np.zeros(n, x.dtype), x[:-n]]) if n else x


def lowpass(x, hz):
    from scipy.signal import butter, sosfilt
    return sosfilt(butter(4, hz / (SR / 2), output="sos"), x)


def matrix(x):
    s = (x[0] - x[1]) / np.sqrt(2)
    s = delay(lowpass(s, 7000.0), REAR_DELAY) * 0.707
    return np.stack([x[0], x[1], np.zeros_like(s), np.zeros_like(s), s, s])


def stft(x):
    win = np.hanning(N_FFT + 1)[:-1]
    pad = np.concatenate([np.zeros(N_FFT), x, np.zeros(N_FFT)])
    idx = np.arange(0, len(pad) - N_FFT, HOP)
    return np.fft.rfft(np.stack([pad[i:i + N_FFT] * win for i in idx]), axis=1), len(x)


def istft(X, n):
    win = np.hanning(N_FFT + 1)[:-1]
    out = np.zeros(N_FFT * 2 + n + N_FFT); norm = np.zeros_like(out)
    for k, frame in enumerate(np.fft.irfft(X, axis=1)):
        i = k * HOP
        out[i:i + N_FFT] += frame * win; norm[i:i + N_FFT] += win ** 2
    return (out / np.maximum(norm, 1e-3))[N_FFT:N_FFT + n]


def pad(x, retention=0.5, smooth_ms=50.0):
    L, n = stft(x[0]); R, _ = stft(x[1])
    alpha = np.exp(-HOP / SR / (smooth_ms / 1000))
    ll = np.zeros_like(L, dtype=float); rr = np.zeros_like(ll); lr = np.zeros_like(L)
    pl, pr, pc = np.zeros(L.shape[1]), np.zeros(L.shape[1]), np.zeros(L.shape[1], complex)
    for t in range(L.shape[0]):                       # causal exponential smoothing of the cross-spectrum
        pl = alpha * pl + (1 - alpha) * np.abs(L[t]) ** 2
        pr = alpha * pr + (1 - alpha) * np.abs(R[t]) ** 2
        pc = alpha * pc + (1 - alpha) * L[t] * np.conj(R[t])
        ll[t], rr[t], lr[t] = pl, pr, pc
    phi = np.abs(lr) / np.sqrt(ll * rr + 1e-12)
    a = np.clip(1 - phi, 0, 1)
    a[:, : int(300 * N_FFT / SR)] = 0                  # nothing rear below 300 Hz, like the renderer
    aL, aR = istft(a * L, n), istft(a * R, n)
    fL, fR = x[0] - (1 - retention) * aL, x[1] - (1 - retention) * aR
    z = np.zeros(n)
    return np.stack([fL, fR, z, z, delay(aL, REAR_DELAY), delay(aR, REAR_DELAY)])


def main(argv):
    mode = "pad"
    if "--mode" in argv:
        mode = argv[argv.index("--mode") + 1]
    src, dst = argv[-2], argv[-1]
    x = read_wav(src)
    y = {"matrix": matrix, "pad": pad}[mode](x.astype(np.float64))
    write_wav6(dst, y[:, : x.shape[1]])


if __name__ == "__main__":
    main(sys.argv[1:])
