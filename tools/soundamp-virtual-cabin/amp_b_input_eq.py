"""Fixed effective upstream bank-EQ kernel 12618C, not the whole B input path.

12682B chooses and smooths per-channel parameters before this kernel. Supply
those effective values explicitly: loader zeros are NOT playback settings.
Float64 recurrence, no hardware rounding, selector lifecycle or coefficient
interpolation emulation. Audio layout is frames x channels (unpacked SIMD).
"""
import numpy as np


def _finite(value):
    a = np.asarray(value)
    if np.iscomplexobj(a):
        raise ValueError('finite real values required')
    a = np.asarray(a, dtype=float)
    if not np.isfinite(a).all():
        raise ValueError('finite real values required')
    return a.copy()


class BInputBankEQReference:
    """12618C: channel gain then cascade with implicit b0=1.

    Records shape (channels, sections, 4): [-a2,-a1,b2,b1], NOT the
    [-a1,-a2,b0,b1,b2] order used by 12610B. State shape (channels,
    sections, 2) stores transposed-direct-form-II z1,z2 per channel.
    Omitted history is an explicit zero-state test scenario.
    """
    def __init__(self, *, records, gains, history=None):
        c, g = _finite(records), _finite(gains)
        if (c.ndim != 3 or c.shape[2] != 4 or min(c.shape[:2]) < 1
                or g.shape != (c.shape[0],)):
            raise ValueError('channel x section x 4 records and channel gains required')
        shape = (*c.shape[:2], 2)
        h = np.zeros(shape) if history is None else _finite(history)
        if h.shape != shape:
            raise ValueError('channel x section x 2 history required')
        self.records, self.gains, self.state = c, g, h

    def process(self, audio):
        x = _finite(audio)
        if x.ndim != 2 or x.shape[1] != len(self.gains):
            raise ValueError('frames x configured channels required')
        out = x*self.gains
        for channel in range(len(self.gains)):
            for section, (a2, a1, b2, b1) in enumerate(self.records[channel]):
                z1, z2 = self.state[channel, section]
                for frame in range(len(out)):
                    sample = out[frame, channel]
                    y = sample+z1
                    z1 = z2+b1*sample+a1*y
                    z2 = b2*sample+a2*y
                    out[frame, channel] = y
                self.state[channel, section] = z1, z2
        return out
