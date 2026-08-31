#include "apac/transform.h"

#include <math.h>
#include <string.h>

namespace apac {

// --------------------------------------------------------------- tables ----
// GetSineWindowCoefficientPtr / GetKBDWindowCoefficientPtr /
// GetDCTIVModulationCoefficients at 0x105590, 0x105600 and 0xb33b0.  All three
// are three-way branches with a NULL default; that NULL is what pins the codec
// to exactly two block sizes, so the port keeps it rather than clamping.

const float* GetSineWindowCoefficientPtr(uint32_t n)
{
    if (n == 128)  return &tables::kSineWindow128[0].f;
    if (n == 1024) return &tables::kSineWindow1024[0].f;
    return 0;
}

const float* GetKBDWindowCoefficientPtr(uint32_t n)
{
    if (n == 128)  return &tables::kKBDWindow128[0].f;
    if (n == 1024) return &tables::kKBDWindow1024[0].f;
    return 0;
}

const float* GetDCTIVModulationCoefficients(uint32_t n)
{
    if (n == 1024) return &tables::kDCTIVMod1024[0].f;
    if (n == 128)  return &tables::kDCTIVMod128[0].f;
    return 0;
}

// ------------------------------------------------------------------ FFT ----

ComplexFFT::ComplexFFT() : mM(0)
{
}

bool ComplexFFT::Initialize(uint32_t m)
{
    if (m < 2 || m > kMaxFFT || (m & (m - 1)) != 0)
        return false;
    if (m == mM)
        return true;

    uint32_t bits = 0;
    while ((1u << bits) < m)
        ++bits;

    for (uint32_t i = 0; i < m; ++i) {
        uint32_t r = 0;
        for (uint32_t b = 0; b < bits; ++b)
            if (i & (1u << b))
                r |= 1u << (bits - 1 - b);
        mRev[i] = (uint16_t)r;
    }

    // exp(-2*pi*i*j/m), j < m/2.  Computed in double and rounded once: the
    // twiddles are the only place a recurrence would visibly cost accuracy.
    const double kTwoPi = 6.283185307179586476925286766559;
    for (uint32_t j = 0; j < m / 2; ++j) {
        double a = -kTwoPi * (double)j / (double)m;
        mTwRe[j] = (float)cos(a);
        mTwIm[j] = (float)sin(a);
    }
    mM = m;
    return true;
}

void ComplexFFT::Forward(const float* inRe, const float* inIm,
                         float* outRe, float* outIm) const
{
    const uint32_t m = mM;
    for (uint32_t i = 0; i < m; ++i) {
        const uint32_t r = mRev[i];
        outRe[i] = inRe[r];
        outIm[i] = inIm[r];
    }
    for (uint32_t half = 1; half < m; half <<= 1) {
        const uint32_t step = m / (half << 1);
        for (uint32_t base = 0; base < m; base += (half << 1)) {
            for (uint32_t j = 0; j < half; ++j) {
                const float wr = mTwRe[j * step];
                const float wi = mTwIm[j * step];
                const uint32_t a = base + j;
                const uint32_t b = a + half;
                const float xr = outRe[b] * wr - outIm[b] * wi;
                const float xi = outRe[b] * wi + outIm[b] * wr;
                outRe[b] = outRe[a] - xr;
                outIm[b] = outIm[a] - xi;
                outRe[a] = outRe[a] + xr;
                outIm[a] = outIm[a] + xi;
            }
        }
    }
}

// ----------------------------------------------------------------- MDCT ----

MDCT::MDCT() : mN(0), mScale(0.0f), mCoef(0)
{
}

// MDCT::Initialize at 0xb7530 then DCT_IV::Initialize at 0xb7410.  The `n > 31`
// gate in the original is subsumed by the table lookup, which only ever
// succeeds for 128 and 1024; the port keeps both so the status codes match.
int32_t MDCT::Initialize(uint32_t n, float scale)
{
    if (scale == 0.0f)
        scale = 1.0f / (float)n;
    mScale = scale;
    if (n <= 31)
        return kTransformBadSize;

    if (n == mN)
        return kTransformOk;

    const float* coef = GetDCTIVModulationCoefficients(n);
    if (!coef)
        return kTransformBadSize;
    if (!mFFT.Initialize(n >> 1))
        return kTransformBadSize;
    mCoef = coef;
    mN = n;
    return kTransformOk;
}

// DCT_IV::PreModulation, 0xb6e80.  The vectorised body and the scalar tail do
// the same thing; this is the scalar tail, which is the readable one:
//
//     x = f*in[2m], y = f*in[n-1-2m], c = coef[2m] + i*coef[2m+1]
//     out[m] = (x + i*y) * conj(c)
void MDCT::PreModulation(const float* in, float* re, float* im, float f) const
{
    const uint32_t n = mN;
    const uint32_t half = n >> 1;
    const float* c = mCoef;
    for (uint32_t m = 0; m < half; ++m) {
        const float x  = f * in[2 * m];
        const float y  = f * in[n - 1 - 2 * m];
        const float cr = c[2 * m];
        const float ci = c[2 * m + 1];
        re[m] = x * cr + y * ci;
        im[m] = y * cr - x * ci;
    }
}

// DCT_IV::PostModulationExtend, 0xb70b0.  Same twiddle, applied to the FFT
// output, and then eight stores per pair that spread the n/2 complex results
// over the 2n real outputs.  With
//
//     w[k] = (re[k] + i*im[k]) * conj(coef[k]),   m = half-1-k
//
// the original writes, for every k in [0, half/2):
//
//     buf [half + 2k]     =  Im(w[k])      buf2[half + 2k]     = -Re(w[k])
//     buf [half + 1 + 2k] = -Re(w[m])      buf2[half + 1 + 2k] =  Im(w[m])
//     buf [2m - half]     =  Re(w[m])      buf2[2m - half]     =  Im(w[m])
//     buf [2m - half + 1] = -Im(w[k])      buf2[2m - half + 1] = -Re(w[k])
//
// which is the same as saying buf is antisymmetric and buf2 symmetric about
// their midpoints.  The port writes all eight rather than exploiting that, so
// that a future divergence shows up as a mismatch instead of being papered over.
void MDCT::PostModulationExtend(const float* re, const float* im,
                                float* buf, float* buf2) const
{
    const uint32_t n = mN;
    const uint32_t half = n >> 1;
    const float* c = mCoef;
    const uint32_t quarter = half >> 1;

    for (uint32_t k = 0; k < quarter; ++k) {
        const uint32_t m = half - 1 - k;

        const float ckr = c[2 * k];
        const float cki = c[2 * k + 1];
        const float cmr = c[2 * m];
        const float cmi = c[2 * m + 1];

        const float Rk =  re[k] * ckr + im[k] * cki;
        const float Ik = -(im[k] * ckr - re[k] * cki);
        const float Rm =  re[m] * cmr + im[m] * cmi;
        const float Im = -(re[m] * cmi - im[m] * cmr);

        const uint32_t lo = 2 * m - half;   // 0 .. half-2, even
        const uint32_t hi = half + 2 * k;   // half .. n-2, even

        buf2[hi]     = -Rk;
        buf2[hi + 1] =  Im;
        buf2[lo]     =  Im;
        buf2[lo + 1] = -Rk;

        buf[hi]     = -Ik;
        buf[hi + 1] = -Rm;
        buf[lo]     =  Rm;
        buf[lo + 1] =  Ik;
    }
}

// MDCT::InverseInPlaceTransform, 0xb75c0.  The original stages the FFT input in
// `buf2` and its output in a stack VLA; the port keeps the same aliasing but
// puts the VLA in the object, because the target is an LD_PRELOADed .so on QNX
// and a 4 KiB alloca on a shared audio thread is not worth the risk.
int32_t MDCT::InverseInPlaceTransform(float* buf, float* buf2)
{
    if (!mCoef)
        return kTransformNotInit;
    const uint32_t half = mN >> 1;
    float* fftRe = mScratch;
    float* fftIm = mScratch + half;

    PreModulation(buf, buf2, buf2 + half, mScale);
    mFFT.Forward(buf2, buf2 + half, fftRe, fftIm);
    PostModulationExtend(fftRe, fftIm, buf, buf2);
    return kTransformOk;
}

}  // namespace apac
