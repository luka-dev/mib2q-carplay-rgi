/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// The APAC inverse transform: DCT_IV, MDCT and the window / modulation tables.
//
// Reference functions in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
//
//   0x000b6e80  DCT_IV::PreModulation(const float*, DSPSplitComplex*, float) const
//   0x000b70b0  DCT_IV::PostModulationExtend(const DSPSplitComplex*, float*, float*) const
//   0x000b7340  DCT_IV::DCT_IV()
//   0x000b7410  DCT_IV::Initialize(unsigned)
//   0x000b74b0  MDCT::MDCT()
//   0x000b7530  MDCT::Initialize(unsigned, float)
//   0x000b75c0  MDCT::InverseInPlaceTransform(float*, float*)
//   0x00103330  MultiRadixFFT::CplxOutOfPlaceTransform(...)
//   0x000b33b0  GetDCTIVModulationCoefficients
//   0x00105590  GetSineWindowCoefficientPtr
//   0x00105600  GetKBDWindowCoefficientPtr
//
// Decompiled in reference/decomp/{DCT_IV,MDCT,MultiRadixFFT}.c and
// reference/decomp/_free_functions.c.
//
// THE ONE PLACE THIS IS NOT A TRANSCRIPTION.  Everything except the complex
// FFT is transcribed index-for-index out of the disassembly, so given the same
// FFT output it does the same float multiplies in the same order.  The FFT
// itself is *not* in the library: `MultiRadixFFT::CplxOutOfPlaceTransform` is a
// three-line wrapper around `vDSP_DFT_Execute`, i.e. Apple's Accelerate
// framework, which is closed and is not shipped on the head unit.  So the FFT
// here is a fresh radix-2 implementation and the agreement with the library is
// float-noise, not bit-exact.  tests/test_mdct.cpp measures it.
//
// Only n == 128 and n == 1024 exist: GetSineWindowCoefficientPtr and
// GetKBDWindowCoefficientPtr return NULL for everything else, and
// GetDCTIVModulationCoefficients has exactly those two tables.

#ifndef APAC_TRANSFORM_H
#define APAC_TRANSFORM_H

#include <stdint.h>

namespace apac {

// The two block sizes, and the complex FFT length each one needs (n / 2).
const uint32_t kTransformLong  = 1024;
const uint32_t kTransformShort = 128;
const uint32_t kMaxTransform   = kTransformLong;

// The largest real FFT apac::LPMDCTFilter is ever initialised with, and the
// complex FFT length that needs (half of it).  The MDCT itself only ever asks
// for 512.
const uint32_t kMaxRealFFT     = 2048;
const uint32_t kMaxFFT         = kMaxRealFFT / 2;

// Not every transform length in this decoder is a power of two.  BWELPC (stage
// e) builds filters of 768 and 96 points, which need complex transforms of 384
// and 48 - both 3 * 2^k.  vDSP, which the library calls, takes f * 2^k for
// f in {1, 3, 5, 15} and so never noticed; our radix-2 FFT did.
// ComplexFFT therefore also accepts m = 3 * p for a power-of-two p up to this
// bound, which it transforms as three interleaved sub-FFTs of length p and one
// three-point butterfly.  384 / 3 = 128 is the largest p any of it asks for.
const uint32_t kMaxFFTRadix3   = 128;

// The library's own return values, spelled as the FourCCs they are.
enum TransformStatus {
    kTransformOk      = 0,
    kTransformBadSize = 1936292453,  // 'size'
    kTransformNotInit = 561214580    // '!stt'
};

// A float stored as its IEEE-754 bit pattern.  The constant tables are copied
// out of the binary word for word, and a decimal literal would not round-trip.
union F32 {
    uint32_t u;
    float    f;
};

namespace tables {
extern const F32 kSineWindow128[128];
extern const F32 kSineWindow1024[1024];
extern const F32 kKBDWindow128[128];
extern const F32 kKBDWindow1024[1024];
extern const F32 kDCTIVMod128[128];
extern const F32 kDCTIVMod1024[1024];
extern const F32 kTNSCoefficients[32];
extern const F32 kLPCLagWindow[16];
extern const uint32_t kTNSMaxBandRecords[24];
}  // namespace tables

// The three table lookups, exactly as the library implements them: any n other
// than 128 or 1024 returns NULL.  Each window table holds the RISING half of a
// symmetric window of length 2n; the falling half is read backwards.
const float* GetSineWindowCoefficientPtr(uint32_t n);
const float* GetKBDWindowCoefficientPtr(uint32_t n);
const float* GetDCTIVModulationCoefficients(uint32_t n);

// Out-of-place complex DFT, split (separate real / imaginary arrays), forward:
//
//     out[k] = sum_j in[j] * exp(-2*pi*i*j*k/m)
//
// No scaling, which is what vDSP_DFT_Execute on a vDSP_DFT_zop setup does.
// Sizes actually used are 48, 64, 384 and 512.  m must be a power of two >= 2,
// or 3 * p for such a p no larger than kMaxFFTRadix3.
class ComplexFFT {
public:
    ComplexFFT();
    bool Initialize(uint32_t m);
    uint32_t Size() const { return mM; }
    void Forward(const float* inRe, const float* inIm,
                 float* outRe, float* outIm) const;

private:
    // The radix-2 core, over `p` points already in bit-reversed order in
    // re / im.  Shared by the plain path and by each third of the radix-3 one.
    void Radix2(float* re, float* im, uint32_t p) const;

    uint32_t mM;
    uint32_t mP3;                     // 0 = plain radix 2; else m / 3
    uint16_t mRev[kMaxFFT];           // built for mP3 ? mP3 : mM
    float    mTwRe[kMaxFFT / 2];
    float    mTwIm[kMaxFFT / 2];
    // exp(-2*pi*i*k/m) and its square, k < m/3.  Only the radix-3 path uses
    // them; 4 * kMaxFFTRadix3 floats, i.e. 2 KiB.
    float    mW1Re[kMaxFFTRadix3];
    float    mW1Im[kMaxFFTRadix3];
    float    mW2Re[kMaxFFTRadix3];
    float    mW2Im[kMaxFFTRadix3];
};

// MDCT is DCT_IV plus a scale factor; the library expresses that as
// `class MDCT : public DCT_IV`, which our decoder flattens because nothing else
// derives from either and neither has any virtual worth keeping.
class MDCT {
public:
    MDCT();

    // n is the transform size (128 or 1024).  scale == 0 means 1.0f / n, which
    // is what every caller in the decoder passes.
    int32_t Initialize(uint32_t n, float scale);

    // The inverse MDCT of `buf` (n coefficients).  Writes the first n time
    // samples over `buf` and the second n into `buf2`.
    //
    // *** buf2 MUST BE n FLOATS OF ITS OWN.  NOT n/2, AND NOT buf + n/2. ***
    //
    // The name says "in place" and the output is described as "the second
    // half", so the natural reading is that `buf2` is half a block inside the
    // same array.  It is not: `buf2` is the FFT staging area as well as the
    // output - PreModulation writes n/2 reals at buf2[0] and n/2 imaginaries at
    // buf2[n/2] before PostModulationExtend overwrites the whole of it - so a
    // caller passing `buf + n/2` on an n-float array writes n/2 floats past the
    // end.  A real caller made exactly that mistake; on the host it landed on
    // nothing and on armle-v7 it destroyed a global, which is the worst way for
    // it to fail.  Its contents are not read on entry.
    //
    // The two call sites in src/filterbank.cpp are the worked example:
    // `mScratch` is `kTransformLong + kTransformShort` floats precisely so that
    // the long transform gets 1024 at `mScratch` and the short one gets its own
    // 128 at `mScratch + 1024`.
    //
    // The output is antisymmetric in the first half and symmetric in the
    // second - `buf[j] == -buf[n-1-j]`, `buf2[j] == buf2[n-1-j]` - which is
    // what "Extend" in the library's name refers to.
    int32_t InverseInPlaceTransform(float* buf, float* buf2);

    uint32_t Size() const { return mN; }
    float    Scale() const { return mScale; }

    // Exposed so the differential test can compare the three stages
    // separately.  `re` and `im` are n/2 floats each.
    void PreModulation(const float* in, float* re, float* im, float scale) const;
    void PostModulationExtend(const float* re, const float* im,
                              float* buf, float* buf2) const;

private:
    uint32_t     mN;
    float        mScale;
    const float* mCoef;
    ComplexFFT   mFFT;
    float        mScratch[kMaxTransform];
};

// Real-input DFT in Apple's vDSP packing, which is what MultiRadixRealFFT
// wraps and therefore what apac::LPMDCTFilter is built on.  Measured against
// the library rather than assumed:
//
//   Forward: re[0] = X[0], im[0] = X[n/2] (both real), and re[k] / im[k] are
//            Re/Im X[k] for k = 1 .. n/2-1.  UNSCALED - the wrapper's
//            vDSP_vsmul by the 0.5 at 0x345db4 cancels vDSP's factor of two,
//            so what comes out is the plain DFT.
//   Inverse: the plain unnormalised inverse, x[j] = sum_k X[k] e^{+2pi i jk/n},
//            so Inverse(Forward(x)) == n * x.
//
// `Inverse` may be called with `out` aliasing `re` - AutocorrFromPSD does
// exactly that - so it stages through its own scratch.
class RealFFT {
public:
    RealFFT();
    // 4 <= n <= kMaxRealFFT and n / 2 a length ComplexFFT accepts, so a power
    // of two or 3 * a power of two - the latter is what BWELPC's 768- and
    // 96-point filters need.
    bool Initialize(uint32_t n);
    uint32_t Size() const { return mN; }

    // Both stage through internal scratch, so neither is const.
    void Forward(const float* x, float* re, float* im);
    void Inverse(const float* re, const float* im, float* out);

private:
    uint32_t   mN;
    ComplexFFT mFFT;
    float      mWRe[kMaxRealFFT / 2];
    float      mWIm[kMaxRealFFT / 2];
    float      mAr[kMaxRealFFT / 2];
    float      mAi[kMaxRealFFT / 2];
    float      mBr[kMaxRealFFT / 2];
    float      mBi[kMaxRealFFT / 2];
};

}  // namespace apac

#endif  // APAC_TRANSFORM_H
