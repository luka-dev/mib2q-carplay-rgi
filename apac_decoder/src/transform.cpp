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

ComplexFFT::ComplexFFT() : mM(0), mP3(0)
{
}

bool ComplexFFT::Initialize(uint32_t m)
{
    // The length the radix-2 core runs at: the whole transform, or a third of
    // it when m is 3 * a power of two.  BWELPC needs 48 and 384; every other
    // caller in the decoder is a power of two.
    uint32_t p = m;
    uint32_t p3 = 0;
    if (m >= 2 && m <= kMaxFFT && (m & (m - 1)) == 0) {
        p3 = 0;
    } else if (m % 3 == 0) {
        p = m / 3;
        if (p < 2 || p > kMaxFFTRadix3 || (p & (p - 1)) != 0)
            return false;
        p3 = p;
    } else {
        return false;
    }
    if (m == mM)
        return true;

    uint32_t bits = 0;
    while ((1u << bits) < p)
        ++bits;

    for (uint32_t i = 0; i < p; ++i) {
        uint32_t r = 0;
        for (uint32_t b = 0; b < bits; ++b)
            if (i & (1u << b))
                r |= 1u << (bits - 1 - b);
        mRev[i] = (uint16_t)r;
    }

    // exp(-2*pi*i*j/p), j < p/2.  Computed in double and rounded once: the
    // twiddles are the only place a recurrence would visibly cost accuracy.
    const double kTwoPi = 6.283185307179586476925286766559;
    for (uint32_t j = 0; j < p / 2; ++j) {
        double a = -kTwoPi * (double)j / (double)p;
        mTwRe[j] = (float)cos(a);
        mTwIm[j] = (float)sin(a);
    }
    if (p3) {
        for (uint32_t k = 0; k < p3; ++k) {
            const double a = -kTwoPi * (double)k / (double)m;
            mW1Re[k] = (float)cos(a);
            mW1Im[k] = (float)sin(a);
            mW2Re[k] = (float)cos(2.0 * a);
            mW2Im[k] = (float)sin(2.0 * a);
        }
    }
    mM = m;
    mP3 = p3;
    return true;
}

// The decimation-in-time radix-2 butterflies over p points already permuted
// into bit-reversed order.  Lifted out of Forward unchanged so the radix-3 path
// can run it on each third.
void ComplexFFT::Radix2(float* re, float* im, uint32_t p) const
{
    for (uint32_t half = 1; half < p; half <<= 1) {
        const uint32_t step = p / (half << 1);
        for (uint32_t base = 0; base < p; base += (half << 1)) {
            for (uint32_t j = 0; j < half; ++j) {
                const float wr = mTwRe[j * step];
                const float wi = mTwIm[j * step];
                const uint32_t a = base + j;
                const uint32_t b = a + half;
                const float xr = re[b] * wr - im[b] * wi;
                const float xi = re[b] * wi + im[b] * wr;
                re[b] = re[a] - xr;
                im[b] = im[a] - xi;
                re[a] = re[a] + xr;
                im[a] = im[a] + xi;
            }
        }
    }
}

void ComplexFFT::Forward(const float* inRe, const float* inIm,
                         float* outRe, float* outIm) const
{
    const uint32_t m = mM;
    if (mP3 == 0) {
        for (uint32_t i = 0; i < m; ++i) {
            const uint32_t r = mRev[i];
            outRe[i] = inRe[r];
            outIm[i] = inIm[r];
        }
        Radix2(outRe, outIm, m);
        return;
    }

    // m == 3p.  Split by residue mod 3, transform each third with the radix-2
    // core in place in its own slice of the output, then one three-point
    // butterfly per k recombines them:
    //
    //   X[k]      = A + s          with u = W^k B, v = W^2k C,
    //   X[k+p]    = A - s/2 - i c d    s = u + v, d = u - v,
    //   X[k+2p]   = A - s/2 + i c d    c = sqrt(3)/2,
    //
    // which is A + w^j u + w^2j v for w = exp(-2*pi*i/3) written out, using
    // w^4 == w.
    const uint32_t p = mP3;
    for (uint32_t t = 0; t < 3; ++t) {
        float* re = outRe + t * p;
        float* im = outIm + t * p;
        for (uint32_t i = 0; i < p; ++i) {
            const uint32_t r = 3u * mRev[i] + t;
            re[i] = inRe[r];
            im[i] = inIm[r];
        }
        Radix2(re, im, p);
    }
    const float c = 0.86602540378443864676f;     // sqrt(3)/2
    for (uint32_t k = 0; k < p; ++k) {
        const float ar = outRe[k],         ai = outIm[k];
        const float br = outRe[p + k],     bi = outIm[p + k];
        const float cr = outRe[2 * p + k], ci = outIm[2 * p + k];
        const float ur = br * mW1Re[k] - bi * mW1Im[k];
        const float ui = br * mW1Im[k] + bi * mW1Re[k];
        const float vr = cr * mW2Re[k] - ci * mW2Im[k];
        const float vi = cr * mW2Im[k] + ci * mW2Re[k];
        const float sr = ur + vr, si = ui + vi;
        const float dr = ur - vr, di = ui - vi;
        outRe[k]         = ar + sr;
        outIm[k]         = ai + si;
        outRe[p + k]     = ar - 0.5f * sr + c * di;
        outIm[p + k]     = ai - 0.5f * si - c * dr;
        outRe[2 * p + k] = ar - 0.5f * sr - c * di;
        outIm[2 * p + k] = ai - 0.5f * si + c * dr;
    }
}

// ------------------------------------------------------------- real FFT ----
// The classic "pack a real sequence of n into a complex one of n/2, transform,
// then split" method.  Every constant in it was checked against the library
// rather than assumed - see the note on RealFFT in the header.

RealFFT::RealFFT() : mN(0)
{
}

bool RealFFT::Initialize(uint32_t n)
{
    // n must be even and n/2 a length ComplexFFT accepts - a power of two, or
    // 3 * one, which is what BWELPC's 768- and 96-point filters ask for.
    if (n < 4 || n > kMaxRealFFT || (n & 1) != 0)
        return false;
    if (n == mN)
        return true;
    if (!mFFT.Initialize(n >> 1))
        return false;
    const double kTwoPi = 6.283185307179586476925286766559;
    for (uint32_t k = 0; k < n / 2; ++k) {
        double a = -kTwoPi * (double)k / (double)n;
        mWRe[k] = (float)cos(a);
        mWIm[k] = (float)sin(a);
    }
    mN = n;
    return true;
}

void RealFFT::Forward(const float* x, float* re, float* im)
{
    const uint32_t n = mN;
    const uint32_t m = n >> 1;
    // z[k] = x[2k] + i*x[2k+1]; the FFT is out of place, so the interleave
    // costs nothing extra.
    float* zr = mAr;
    float* zi = mAi;
    float* Zr = mBr;
    float* Zi = mBi;
    for (uint32_t k = 0; k < m; ++k) {
        zr[k] = x[2 * k];
        zi[k] = x[2 * k + 1];
    }
    mFFT.Forward(zr, zi, Zr, Zi);

    // k == 0 carries both DC and Nyquist, which is the whole reason for the
    // packing: X[0] and X[n/2] are real, so one slot holds both.
    re[0] = Zr[0] + Zi[0];
    im[0] = Zr[0] - Zi[0];
    for (uint32_t k = 1; k < m; ++k) {
        const uint32_t j = m - k;
        const float er =  0.5f * (Zr[k] + Zr[j]);
        const float ei =  0.5f * (Zi[k] - Zi[j]);
        const float dr =  0.5f * (Zi[k] + Zi[j]);   //  Re of -i*(Z[k]-conj Z[j])/2
        const float di = -0.5f * (Zr[k] - Zr[j]);   //  Im of the same
        const float wr = mWRe[k], wi = mWIm[k];
        re[k] = er + (dr * wr - di * wi);
        im[k] = ei + (dr * wi + di * wr);
    }
}

void RealFFT::Inverse(const float* re, const float* im, float* out)
{
    const uint32_t n = mN;
    const uint32_t m = n >> 1;
    float* Zr = mAr;
    float* Zi = mAi;
    float* zr = mBr;
    float* zi = mBi;

    // Undo the split: E = (X[k] + X[k+m])/2, O = conj(W^k) * (X[k] - X[k+m])/2,
    // Z[k] = E + i*O.  X[k+m] is conj(X[m-k]) by the Hermitian symmetry the
    // packing assumes, and X[0] / X[m] come out of the two halves of slot 0.
    {
        const float x0 = re[0];        // X[0],   real
        const float xm = im[0];        // X[n/2], real
        Zr[0] = 0.5f * (x0 + xm);
        Zi[0] = 0.5f * (x0 - xm);
    }
    for (uint32_t k = 1; k < m; ++k) {
        const uint32_t j = m - k;
        const float ar = re[k],  ai = im[k];       // X[k]
        const float br = re[j],  bi = -im[j];      // X[k+m] = conj(X[m-k])
        const float er = 0.5f * (ar + br);
        const float ei = 0.5f * (ai + bi);
        const float fr = 0.5f * (ar - br);
        const float fi = 0.5f * (ai - bi);
        const float wr = mWRe[k], wi = -mWIm[k];   // conj(W^k)
        const float or_ = fr * wr - fi * wi;
        const float oi_ = fr * wi + fi * wr;
        Zr[k] = er - oi_;
        Zi[k] = ei + or_;
    }

    // Unnormalised inverse of length m, by conjugation around the forward one.
    for (uint32_t k = 0; k < m; ++k)
        Zi[k] = -Zi[k];
    mFFT.Forward(Zr, Zi, zr, zi);

    // z came out m times too large and conjugated; the library's inverse is
    // n times the true signal, so the surviving factor is exactly 2.
    for (uint32_t k = 0; k < m; ++k) {
        out[2 * k]     =  2.0f * zr[k];
        out[2 * k + 1] = -2.0f * zi[k];
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
