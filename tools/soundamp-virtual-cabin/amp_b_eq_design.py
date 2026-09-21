"""Mathematical reference for the three successful 12AB1F EQ design paths.

This is not SHARC bit-exact transcendental arithmetic or its error-logger ABI.
Inputs are the five-word design record, not SOS coefficients or UI controls.
"""
import math
import struct

from amp_b_filters import stable_section


def _f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


TWO_PI = _f32(2*math.pi)
PERCENT = _f32(.01)
HALF_LOG2_PERCENT = struct.unpack('<f', struct.pack('<I', 0x3b632166))[0]


def design_b_eq(record, *, sample_rate=48000):
    """[frequencyHz, widthPercent, gainDb, type, reserved] -> [b0,b1,b2,-a1,-a2].

Rounded type0: peaking, widthPercent/100 in octaves; type1: low shelf;
type2: high shelf, widthPercent/100 is shelf slope. The fifth design word is
not read by this function's decoded body. It must nevertheless be finite.

Finite physical-frequency domain, positive width and recovered gain clamp
[-100,+20] only. Non-real shelf radicands, overflow and numerically unstable
float32-rounded records raise; never silently substitute identity. Firmware
error fallthrough/identity reset are not an authorization to hide such cases.
Zero gain returns exact identity on the supported input domain.
"""
    if len(record) != 5 or not all(math.isfinite(v) for v in record):
        raise ValueError('five finite real design words required')
    frequency, width, gain, type_value, _reserved = record
    if (not math.isfinite(sample_rate) or sample_rate <= 0 or not 0 < frequency < sample_rate/2
            or width <= 0 or not -100 <= gain <= 20 or not 0 <= type_value < 2.5):
        raise ValueError('design outside supported finite frequency/width/gain/type domain')
    kind = math.trunc(type_value+.5)
    if gain == 0:
        return [1.0, 0.0, 0.0, 0.0, 0.0]
    w = TWO_PI*frequency/sample_rate
    sine, cosine = math.sin(w), math.cos(w)
    a = 10**(gain/40)
    try:
        if kind == 0:
            #12ACFB: w/sin(w);12AD02 constant is ln(2)/200, not ln(2)/2.
            alpha = sine*math.sinh(HALF_LOG2_PERCENT*width*w/sine)
            denominator = 1+alpha/a
            b0, b1, b2 = 1+alpha*a, -2*cosine, 1-alpha*a
            a1, a2 = -2*cosine, 1-alpha/a
        else:
            #12ABF3/12AC80: (A*A+1)/(width*.01) - (A-1)^2.
            radicand = (a*a+1)/(width*PERCENT)-(a-1)**2
            if radicand < 0:
                raise ValueError('shelf square-root radicand is negative')
            beta = sine*math.sqrt(radicand)
            if kind == 1:
                denominator = (a+1)+(a-1)*cosine+beta
                b0 = a*((a+1)-(a-1)*cosine+beta)
                b1 = 2*a*((a-1)-(a+1)*cosine)
                b2 = a*((a+1)-(a-1)*cosine-beta)
                a1 = -2*((a-1)+(a+1)*cosine)
                a2 = (a+1)+(a-1)*cosine-beta
            else:
                denominator = (a+1)-(a-1)*cosine+beta
                b0 = a*((a+1)+(a-1)*cosine+beta)
                b1 = -2*a*((a-1)+(a+1)*cosine)
                b2 = a*((a+1)+(a-1)*cosine-beta)
                a1 = 2*((a-1)-(a+1)*cosine)
                a2 = (a+1)-(a-1)*cosine-beta
        result = [_f32(v/denominator) for v in (b0, b1, b2, -a1, -a2)]
    except (OverflowError, ZeroDivisionError) as exc:
        raise ValueError('EQ design overflow or singularity') from exc
    if not all(math.isfinite(v) for v in result) or not stable_section(result):
        raise ValueError('EQ design is not stable after float32 coefficient rounding')
    return result
