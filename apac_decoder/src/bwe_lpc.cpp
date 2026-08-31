#include "apac/bwe_lpc.h"

#include <string.h>

namespace apac {

BWELPC::BWELPC()
{
    Initialize();
}

// BWELPC::Initialize, 0xb1030.  The original's body is: apac::LSFTools::
// Initialize(), which sizes the six float vectors and builds the two
// apac::LPCodeBook records out of the descriptor pair at 0x00425ad8 and then
// calls Reset(); then six `new apac::LPMDCTFilter` with fftSize taken from
// BWELPC::kDftSizes and order 16, each Initialize()d.
//
// Here the vectors are fixed-size members, the codebooks are the tables in
// src/bwe_tables.cpp, and the filters are initialised lazily by FilterFor - so
// what is left of Initialize is the geometry and the zeroing Reset does.
void BWELPC::Initialize()
{
    mTools.order = kBWELPCOrder;
    mTools.sampleRate = kBWESampleRate;
    memset(mLPC, 0, sizeof mLPC);
    memset(mLsfLPC, 0, sizeof mLsfLPC);
    memset(mLSF, 0, sizeof mLSF);
    memset(mLSP, 0, sizeof mLSP);
    memset(mCorr, 0, sizeof mCorr);
}

// The switch at the head of both BWELPC::LPCToMDCTSpectrum and
// BWELPC::MDCTCoefficientsToLPC, which indexes the six-element filter vector by
// a byte offset: 24 -> [3], 40 -> [5], 32 -> [4], 16 -> [2], 8 -> [1], 0 -> [0],
// against BWELPC::kDftSizes = {1024, 768, 512, 128, 96, 64} at 0x00331a58.
uint32_t BWELPC::FFTSizeFor(uint32_t n)
{
    if (n == 64)
        return 128;
    if (n <= 64) {
        if (n == 32)
            return 64;
        if (n == 48)
            return 96;
        return 1024;
    }
    if (n == 256)
        return 512;
    if (n == 384)
        return 768;
    return 1024;
}

// Two objects standing in for the original's six.  See the header for why that
// is observationally identical, and for what it costs: the Initialize below
// rebuilds ~768 sin/cos whenever the geometry changes between frames, which is
// +34 % per channel on an alternating stream.  This function is the whole of
// the lever - four or six filters here removes the transient for +94 or
// +188 KiB - so if you are here to change it, read the header note first.
LPMDCTFilter* BWELPC::FilterFor(uint32_t n)
{
    const uint32_t fft = FFTSizeFor(n);
    LPMDCTFilter* f = (fft == 128 || fft == 1024) ? &mExtFilter : &mCoreFilter;
    if (f->FFTSize() != fft || f->Order() != kBWELPCOrder)
        f->Initialize((uint16_t)fft, (uint16_t)kBWELPCOrder);
    return f;
}

// apac::LSFTools::DecodeLSF, 0xb4f30: zero the LSF vector, then for each
// codebook add its idx'th `order`-float vector in with vDSP_vadd.  Two
// codebooks here, in the order LSFTools::Initialize pushed them.
//
// Then BWELPC::DecodeLSFsToLPCs' own three calls, with the literals the
// disassembly carries: 50.0f / 24000.0f into Condition_LSFs, 24000.0f into
// LSF_To_LSP, and the LSP vector's own length (16) into LSP_To_LPC.
bool BWELPC::DecodeLSFsToLPCs(const uint16_t idx[kBWELsfBooks])
{
    if (idx[0] >= kBWELsfBookSize || idx[1] >= kBWELsfBookSize)
        return false;

    const float* cb[kBWELsfBooks] = { kBWELsfCB0, kBWELsfCB1 };
    for (uint32_t i = 0; i < kBWELPCOrder; ++i)
        mLSF[i] = 0.0f;
    for (uint32_t s = 0; s < kBWELsfBooks; ++s) {
        const float* v = cb[s] + (uint32_t)idx[s] * kBWELPCOrder;
        for (uint32_t i = 0; i < kBWELPCOrder; ++i)
            mLSF[i] = v[i] + mLSF[i];
    }

    mTools.Condition_LSFs(mLSF, kBWELSFMinGap, kBWESampleRate);
    mTools.LSF_To_LSP(mLSF, mLSP, kBWESampleRate);
    LSFTools::LSP_To_LPC(mLSP, mLsfLPC, (uint16_t)kBWELPCOrder);
    return true;
}

// BWELPC::MDCTCoefficientsToLPC, 0xb0e10.  The original allocas
// (4 * order + 19) & 0x7fff0 bytes for the autocorrelation - 80 for order 16 -
// and hands that buffer to apac::LPMDCTFilter::MDCTToAutoCorr with the argument
// order (spec, r, hop, blocks, len); `len` is also what selects the filter.
//
// The 1.0001 ridge is the float 0x3f800347, materialised as an immediate, and
// the zero test is on the raw lag-0 value BEFORE the multiply - so an
// all-silent input gets 1.0001 rather than 0.
void BWELPC::MDCTCoefficientsToLPC(const float* spec, uint32_t hop,
                                   uint32_t len, uint32_t blocks)
{
    LPMDCTFilter* f = FilterFor(len);
    f->MDCTToAutoCorr(spec, mCorr, hop, blocks, len);

    const float kRidge = 1.0001f;
    mCorr[0] = (mCorr[0] == 0.0f) ? kRidge : mCorr[0] * kRidge;

    mLPCBase.Configure(kBWELPCOrder, mLPC, 0);
    mLPCBase.LevinsonDurbin(mCorr);
}

// BWELPC::LPCToMDCTSpectrum, 0xb0d70 - a tail call into
// apac::LPMDCTFilter::LPCToSpecEnvelope through the filter's vtable slot 5.
void BWELPC::LPCToMDCTSpectrum(const float* lpc, float* env, uint32_t n)
{
    FilterFor(n)->LPCToSpecEnvelope(lpc, env, n);
}

}  // namespace apac
