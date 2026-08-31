// BWELPC - the LPC machinery APACBWE2Decoder::DecodeFrame runs on.
//
// Originals in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
//
//   0x000b1030  BWELPC::Initialize()
//   0x000b0d70  BWELPC::LPCToMDCTSpectrum(const float*, float*, unsigned)
//   0x000b0e10  BWELPC::MDCTCoefficientsToLPC(const float*, unsigned,
//                                             unsigned, unsigned)
//   0x000b0f60  BWELPC::DecodeLSFsToLPCs(const unsigned short*)
//   0x000b5a10  apac::LSFTools::Initialize()
//   0x000b5290  apac::LSFTools::Reset()
//   0x000b4f30  apac::LSFTools::DecodeLSF(const unsigned short*)
//
// Decompiled in reference/decomp/BWELPC.c and apac_LSFTools.c.
//
// `class BWELPC : public apac::LSFTools : public apac::LPCBase`, and its vtable
// at 0x0042b5d8 overrides EXACTLY ONE slot - `Initialize(void)`.  Every other
// entry is apac::LSFTools' or apac::LPCBase's own, read out of the binary
// rather than guessed, so the four calls DecodeLSFsToLPCs makes through the
// vptr resolve to:
//
//   vptr + 72   ->  apac::LSFTools::DecodeLSF
//   vptr + 128  ->  apac::LSFTools::Condition_LSFs(lsf, 50.0f, 24000.0f)
//   vptr + 96   ->  apac::LSFTools::LSF_To_LSP(lsf, lsp, 24000.0f)
//   vptr + 112  ->  apac::LSFTools::LSP_To_LPC(lsp, lpc, order)
//
// and MDCTCoefficientsToLPC's one call, vptr + 48, is
// apac::LPCBase::LevinsonDurbin.  All five bodies are stage d's and stage h1's
// and are reused verbatim; the only new arithmetic in this file is DecodeLSF,
// which is a sum of two codebook vectors.
//
// FIXED GEOMETRY, and it is the constructor's, not a configuration.
// APACBWE2Decoder::InitInterFrameMemory builds each BWELPC with an inline field
// initialiser (`v16[1] = 0x100400010`, `*((_DWORD *)v16 + 26) = 16`,
// `v16[14] = 0x4248000046BB8000`), so:
//
//   LPC order            16, hence 17 direct-form coefficients
//   sample rate          24000.0f  (the analysis half-band)
//   LSF conditioning gap 50.0f
//   codebooks            two, 512 entries of 16 LSFs each
//
// and apac::LSFTools::Initialize sizes every vector from those.  None of it is
// read from the bitstream, so none of it can be attacker-controlled.
//
// THE FILTER BANK, and where the port differs.  BWELPC::Initialize builds SIX
// apac::LPMDCTFilter objects, one per entry of BWELPC::kDftSizes at 0x00331a58
// - {1024, 768, 512, 128, 96, 64} - each with LPC order 16, and picks one by a
// switch on the caller's `n`:
//
//     n     32   48   64   256  384  512 and anything else
//     fft   64   96   128  512  768  1024
//
// i.e. fftSize is always 2n.  In this port a single apac::LPMDCTFilter is 47
// KiB (kMaxRealFFT-sized scratch that the original heap-allocates per size), so
// six of them would put 282 KiB into a decoder that is already 1.46 MB.  The
// port keeps TWO and re-Initializes on demand, which is observationally
// identical: apac::LPMDCTFilter holds NO state across calls - its members are
// the size, the order, 2/fftSize, the FFT twiddles and pure scratch - and
// Initialize is a pure function of (fftSize, order) that returns early when the
// size is unchanged.  The split is by role, so nothing thrashes within a frame:
// a "core" filter for the 2*bwStart sizes {64, 96, 512, 768} and an "extension"
// filter for the two fixed ones {128, 1024}.  tests/test_bwe.cpp drives the six
// sizes in interleaved orders and requires the same answers as a fresh object.
//
// IT HAS A PRICE AND THE PRICE IS MEASURED.  Saving those 190 KiB costs a
// twiddle rebuild - about 768 sin/cos for the 1024-point filter - every time
// the GEOMETRY changes between frames, per channel per frame.  On the host,
// one channel, warmed: 7 252 ns steady state against 9 695 ns alternating
// narrow-long / wide-long, i.e. +34 %.  The original does not pay it, so it
// must never be quoted as the decoder's cost without saying whose it is.
//
// The decision is deliberate: the path does not execute at all on the CarPlay
// content, and the real-time budget has 27x of headroom over the worst packet,
// so 190 KiB inside dio_manager is the scarcer resource.  If that ever stops
// being true the lever is here and nothing else moves: give this class four
// filters (core-long, core-short, ext-long, ext-short) for +94 KiB, or all six
// for +188 KiB, and change FilterFor below to pick between them.  The bit-exact
// "fresh vs reused filter" assertion in tests/test_bwe.cpp is what makes any of
// those counts legal.  Written up in PORT_STATUS.md, "The two-filter trade".

// SIZE.  `sizeof(apac::BWELPC)` is 107 456 bytes - the LARGEST single object in
// this decoder, ahead of `APACSynthesisFilterBank`'s 94 832 - because it holds
// two kMaxRealFFT-scratch `apac::LPMDCTFilter`s.  It MUST NOT go on a stack.
// One instance lives in `apac::Decoder` and is handed down; anything else that
// needs one - a test, a benchmark, a tool - declares it at FILE SCOPE.  Not a
// function-local static either: that avoids the stack but emits a guard
// variable, and this toolchain has no `__cxa_guard_acquire`.

#ifndef APAC_BWE_LPC_H
#define APAC_BWE_LPC_H

#include <stdint.h>

#include "apac/lpc.h"

namespace apac {

// The constructor's geometry, from the inline field initialiser above.
const uint32_t kBWELPCOrder      = 16;
const float    kBWESampleRate    = 24000.0f;
const float    kBWELSFMinGap     = 50.0f;
const uint32_t kBWELsfBooks      = 2;
const uint32_t kBWELsfBookSize   = 512;   // a 9-bit index, exactly
const uint32_t kBWEGainSize      = 64;    // a 6-bit index, exactly

namespace tables {
extern const uint32_t kBWELsfCB0Bits[kBWELsfBookSize * kBWELPCOrder];
extern const uint32_t kBWELsfCB1Bits[kBWELsfBookSize * kBWELPCOrder];
extern const uint32_t kBWEGainBits[kBWEGainSize];
}  // namespace tables

extern const float* const kBWELsfCB0;
extern const float* const kBWELsfCB1;
extern const float* const kBWEGain;

class BWELPC {
public:
    BWELPC();

    // BWELPC::Initialize + apac::LSFTools::Initialize + ::Reset, folded.  The
    // original's Reset also fills the vector at +120 with sixteen constants
    // from 0x00332fc0; nothing on the DECODE path reads it - its only reader is
    // apac::LSFTools::QuantizeLSF, the encoder half, which has no call site in
    // this library at all - so the port does not carry it.
    void Initialize();

    // BWELPC::DecodeLSFsToLPCs, 0xb0f60.  idx[0] indexes kBWELsfCB0 and idx[1]
    // kBWELsfCB1; both are 9-bit fields in the bitstream and both codebooks
    // hold 512 entries, so a legal index is always in range.
    //
    // Returns FALSE on an index the codebook cannot hold.  This function is
    // handed a PARSED value rather than the bitstream, which is the shape of
    // every memory-safety defect this port has found in the original
    // (APACChannelRemapper::Process, APACCACDecoder::ProcessCac,
    // TNSData::Apply), so the invariant is checked here where it is relied on
    // rather than assumed from the parser two layers up.  The original checks
    // nothing and would read up to 4 GiB past the codebook.
    bool DecodeLSFsToLPCs(const uint16_t idx[kBWELsfBooks]);

    // BWELPC::MDCTCoefficientsToLPC, 0xb0e10.  `blocks` spectra spaced `hop`
    // floats apart, each `len` bins long, summed into one autocorrelation, a
    // 1.0001 ridge on lag 0, then Levinson-Durbin.  `len` also picks the
    // filter.  The result is CoreLPC().
    void MDCTCoefficientsToLPC(const float* spec, uint32_t hop, uint32_t len,
                               uint32_t blocks);

    // BWELPC::LPCToMDCTSpectrum, 0xb0d70.  order+1 coefficients -> n magnitudes
    // of the LP spectrum, through the 2n-point filter.
    void LPCToMDCTSpectrum(const float* lpc, float* env, uint32_t n);

    // The two coefficient buffers the original keeps at +24 (apac::LPCBase's
    // own, written by LevinsonDurbin) and +48 (written by LSP_To_LPC).
    const float* CoreLPC() const { return mLPC; }
    const float* LsfLPC()  const { return mLsfLPC; }
    // +192 and +216: the decoded LSFs (after conditioning) and the LSPs.
    // Exposed for the differential test, which compares them against the
    // library's own object; nothing on the decode path reads them.
    const float* LSF() const { return mLSF; }
    const float* LSP() const { return mLSP; }

    // The original's switch, exposed so the test can assert the mapping rather
    // than restate it.  Returns 1024 for any n the switch does not name.
    static uint32_t FFTSizeFor(uint32_t n);

private:
    LPMDCTFilter* FilterFor(uint32_t n);

    LSFTools     mTools;
    LPCBase      mLPCBase;
    float        mLPC[kBWELPCOrder + 1];
    float        mLsfLPC[kBWELPCOrder + 1];
    float        mLSF[kBWELPCOrder];
    float        mLSP[kBWELPCOrder];
    float        mCorr[kBWELPCOrder + 2];
    LPMDCTFilter mCoreFilter;   // 64 / 96 / 512 / 768 - the 2*bwStart sizes
    LPMDCTFilter mExtFilter;    // 128 and 1024 - the two fixed ones
};

}  // namespace apac

#endif  // APAC_BWE_LPC_H
