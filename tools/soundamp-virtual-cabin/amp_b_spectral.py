"""Isolated mathematical references for recovered first-B spectral helpers.

NOT the spectral upmixer: no FFT packing, windows, adaptive masks or live
parameters are inferred. Float64 sqrt/division approximate the mathematical
intent of RSQRTS/RECIPS plus Newton steps, NOT their bit-exact hardware result.
Inputs are complex bins (firmware stores interleaved real/imaginary words).
"""
import struct
import numpy as np


def word_float(word):
    return struct.unpack('<f',struct.pack('<I',word))[0]


POWER_MIN=word_float(0x2EDBE6FF)
POWER_MAX=word_float(0x7149F2CA)
NORMALIZE_EPSILON=word_float(0x1E3CE508)


def bins(values):
    z=np.asarray(values,dtype=complex)
    if z.ndim!=1 or not np.isfinite(z).all():
        raise ValueError('finite one-dimensional complex bins required')
    return z


def clamped_power(values):
    z=bins(values)
    with np.errstate(over='ignore'):
        return np.clip(z.real*z.real+z.imag*z.imag,POWER_MIN,POWER_MAX)


def magnitude(values):
    """131835: square both components, strided sum, clamp, vector sqrt."""
    return np.sqrt(clamped_power(values))


def normalize(values, *, power_workspace=None):
    """1317A2: return magnitude and original complex bins / magnitude.

    Optional stack1 is OUTPUT scratch for computed clamped power, not supplied
    input power. 1317C2 PASS R0 sets EQ from BIN COUNT before 1317C7: non-null
    workspace does not bypass computation. Prior supplied_power API was wrong
    and is removed. Workspace must be a distinct writable real numpy vector.
    Its incoming values are irrelevant. The power floor is not additive audio.
    """
    z=bins(values)
    power=clamped_power(z)
    if power_workspace is not None:
        if (not isinstance(power_workspace,np.ndarray) or power_workspace.shape!=z.shape
                or power_workspace.dtype.kind!='f' or not power_workspace.flags.writeable
                or np.shares_memory(power_workspace,z)):
            raise ValueError('distinct writable real power workspace matching bins required')
        power_workspace[:]=power
    mag=np.sqrt(power+NORMALIZE_EPSILON)
    return mag,z/mag


def sum_difference(left, right, *, half):
    """12E3E6 (half=True) / 12E44B (False), over all 2*N float words.

    Half-sum/half-difference followed by unscaled sum/difference is unity,
    not two half-scaled transforms and not two unscaled transforms.
    """
    a,b=bins(left),bins(right)
    if a.shape!=b.shape:raise ValueError('matching bins required')
    if type(half) is not bool:raise ValueError('explicit boolean half required')
    gain=.5 if half else 1.
    return gain*(a+b),gain*(a-b)
