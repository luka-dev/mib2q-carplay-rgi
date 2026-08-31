// apac::LPCBase and the pure conversions of apac::LSFTools.
//
// Originals in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
//
//   0x000b4190  apac::LPCBase::LPC_Weighting(const float*, float*, float, unsigned short)
//   0x000b4620  apac::LPCBase::LevinsonDurbin(const float*)
//   0x000b4930  apac::LPCBase::ComputeLPCs(const float*, bool)
//   0x000b4220  apac::LSFTools::get_LSFWeightingFactors(const float*, float*,
//                                                       unsigned short, float)
//   0x000b4390  apac::LSFTools::Condition_LSFs(float*, float, float)
//   0x0009c6e0  apac::LSFTools::ConditionLSFs(float*)
//   0x000b4460  apac::LSFTools::LSP_To_LPC(const float*, float*, unsigned short)
//   0x000b40b0  the polynomial recursion LSP_To_LPC runs twice
//   0x000b5010  apac::LSFTools::LSP_To_LSF(const float*, float*, unsigned short, float)
//   0x000b50f0  apac::LSFTools::LSF_To_LSP(const float*, float*, float)
//   0x000b51c0  apac::LSFTools::LSF_To_LSP(float*, float)
//
// Decompiled in reference/decomp/apac_LPCBase.c, apac_LSFTools.c and
// _free_functions.c.
//
// WHAT IS NOT HERE, and why.  apac::LSFTools also has Initialize, Reset,
// DecodeLSF, QuantizeLSF and the LSF_To_LPC that chains the conversions
// together.  Those need the object's six std::vector members and the 64 KiB
// codebook block at 0x333000 that Initialize unpacks, and their only caller is
// APACLRVQDecoder, which PORT_STATUS.md has verified is never entered on the
// CarPlay 5.1 stream.  They are recorded as not started rather than guessed at.
//
// The trig here is DOUBLE precision on purpose: the original calls the double
// `cos` and divides by the literal 3.14159265 - which is not M_PI, it is a
// nine-digit truncation of it - and the difference is visible in float32.

#ifndef APAC_LPC_H
#define APAC_LPC_H

#include <stdint.h>

#include "apac/transform.h"

namespace apac {

// The port's caps.  The original has no fixed limit; these are far above
// anything the profile can ask for (the LPC orders in play are 8 to 16).
const uint32_t kMaxLPCOrder  = 32;
const uint32_t kMaxLPCWindow = 2048;

// The literal the original divides by.  Not M_PI - see the header note.
const double kLSFPi = 3.14159265;

// apac::Autocorr as ComputeLPCs reads it: an analysis window and a lag count.
// The original is a 32-byte object - vptr, the lag count at +8, and an owning
// {begin, end} pair at +16 - and ComputeLPCs touches exactly those three.
struct Autocorr {
    uint32_t     order;          // +8 in the original, a uint16
    const float* window;         // +16
    uint32_t     windowLength;   // (+24 - +16) / 4
};

class LPCBase {
public:
    LPCBase();

    // buf must hold order + 1 floats and is where LevinsonDurbin and
    // ComputeLPCs leave the direct-form coefficients, a[0] == 1.
    void Configure(uint32_t order, float* lpcOut, const Autocorr* autocorr);

    uint32_t Order() const { return mOrder; }
    const float* Coefficients() const { return mLPC; }

    // out[0] = in[0]; out[i] = in[i] * gamma^i, i = 1 .. order.
    static void LPC_Weighting(const float* in, float* out, float gamma,
                              uint16_t order);

    // Autocorrelation r[0 .. order] to direct-form LPC.  The original clamps a
    // non-positive prediction error to 0.01 and keeps going rather than
    // failing; the port reproduces that.
    void LevinsonDurbin(const float* r);

    // Window the signal with the Autocorr's window, autocorrelate it to
    // `order` lags, apply the 1.0001 ridge and (if lagWindow) the 16-entry lag
    // window at 0x332f80, then LevinsonDurbin.
    void ComputeLPCs(const float* signal, bool lagWindow);

private:
    uint32_t        mOrder;
    float*          mLPC;
    const Autocorr* mAutocorr;
    // The original allocas both of these; the port cannot, so they are members.
    float mSignal[kMaxLPCWindow + kMaxLPCOrder + 1];
    float mCorr[kMaxLPCOrder + 2];
};

// The stateless half of apac::LSFTools.  `order` is the original's uint16 at
// +104 and `sampleRate` its float at +112.
struct LSFTools {
    uint32_t order;
    float    sampleRate;

    // lsf[i] = fs/2 / pi * acos(lsp[i])
    static void LSP_To_LSF(const float* lsp, float* lsf, uint16_t order, float fs);

    // lsp[i] = cos(lsf[i] * pi / (fs/2)), over `order` entries.
    void LSF_To_LSP(const float* lsf, float* lsp, float fs) const;
    void LSF_To_LSP(float* inout, float fs) const;

    // LSPs to direct-form LPC through the two half-order polynomials.
    // lpc must hold order + 1 floats; order must be even.
    static void LSP_To_LPC(const float* lsp, float* lpc, uint16_t order);

    // Force a minimum gap of `minGap` between neighbouring LSFs, from the
    // bottom up and then from the top down against fs/2 - minGap.
    void Condition_LSFs(float* lsf, float minGap, float fs) const;
    // Condition_LSFs(lsf, 50.0f, 24000.0f) - the constants are in the original.
    void ConditionLSFs(float* lsf) const;

    // w[i] = 1/(lsf[i] - lsf[i-1]) + 1/(lsf[i+1] - lsf[i]), with lsf[-1] taken
    // as 0 and lsf[order] as fs/2.
    static void get_LSFWeightingFactors(const float* lsf, float* w,
                                        uint16_t order, float fs);
};

// apac::LPMDCTFilter - the bridge between an MDCT magnitude spectrum and an
// LPC autocorrelation, and back.  0x9c7b0 (Initialize), 0xb47b0
// (AutocorrFromPSD), 0xb4be0 (LPCToSpecEnvelope), 0xb4ce0
// (LPImpulseToSpecEnvelope) and 0xb4de0 (MDCTToAutoCorr).
//
// The object is 52 bytes: the FFT size and the LPC order as uint16s at +8 and
// +10, a MultiRadixRealFFT at +16, and 2/fftSize at +48.  Every entry point
// takes an `n` and compares fftSize / n against 4; that single comparison is
// what switches the output between stride 1 and stride 2, and it is the only
// use either function makes of `n` beyond the element count.
//
// Its callers are APACBWE2Decoder (stage e) and APACLRVQDecoder.
class LPMDCTFilter {
public:
    LPMDCTFilter();

    // Returns 2.0f / fftSize, which is what the original returns and stores.
    float Initialize(uint16_t fftSize, uint16_t order);

    uint32_t FFTSize() const { return mFFTSize; }
    uint32_t Order() const { return mOrder; }
    float    Scale() const { return mScale; }

    // n spectral magnitudes -> order + 1 autocorrelation lags, through the
    // inverse real FFT of the power spectrum.
    void AutocorrFromPSD(const float* mag, float* r, uint32_t n);

    // order + 1 LPC coefficients -> n magnitudes of the LP spectrum.
    void LPCToSpecEnvelope(const float* lpc, float* env, uint32_t n);

    // An fftSize-point impulse response -> n magnitudes.
    void LPImpulseToSpecEnvelope(const float* impulse, float* env, uint32_t n);

    // `count` consecutive `hop`-spaced spectra, each turned into an
    // autocorrelation by AutocorrFromPSD and summed.
    void MDCTToAutoCorr(const float* spec, float* r, uint32_t hop,
                        uint32_t count, uint32_t n);

private:
    uint32_t mFFTSize;
    uint32_t mOrder;
    float    mScale;
    RealFFT  mFFT;
    float    mTmp[kMaxRealFFT];
    float    mImpulse[kMaxRealFFT];
    float    mAcc[kMaxLPCOrder + 2];
};

}  // namespace apac

#endif  // APAC_LPC_H
