// The APAC inverse transform: DCT_IV, MDCT and the window / modulation tables.
//
// Originals in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
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
// here is a fresh radix-2 implementation and the agreement with the original is
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
const uint32_t kMaxFFT         = kMaxTransform / 2;

// The original's own return values, spelled as the FourCCs they are.
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
// Sizes actually used are 64 and 512; m must be a power of two >= 2.
class ComplexFFT {
public:
    ComplexFFT();
    bool Initialize(uint32_t m);
    uint32_t Size() const { return mM; }
    void Forward(const float* inRe, const float* inIm,
                 float* outRe, float* outIm) const;

private:
    uint32_t mM;
    uint16_t mRev[kMaxFFT];
    float    mTwRe[kMaxFFT / 2];
    float    mTwIm[kMaxFFT / 2];
};

// MDCT is DCT_IV plus a scale factor; the library expresses that as
// `class MDCT : public DCT_IV`, which the port flattens because nothing else
// derives from either and neither has any virtual worth keeping.
class MDCT {
public:
    MDCT();

    // n is the transform size (128 or 1024).  scale == 0 means 1.0f / n, which
    // is what every caller in the decoder passes.
    int32_t Initialize(uint32_t n, float scale);

    // The inverse MDCT of `buf` (n coefficients).  Writes the first n time
    // samples over `buf` and the second n into `buf2`.  buf2 is also used as
    // scratch, so it must be n floats and its contents are not read.
    //
    // The output is antisymmetric in the first half and symmetric in the
    // second - `buf[j] == -buf[n-1-j]`, `buf2[j] == buf2[n-1-j]` - which is
    // what "Extend" in the original's name refers to.
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

}  // namespace apac

#endif  // APAC_TRANSFORM_H
