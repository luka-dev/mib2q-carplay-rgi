/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// APACLRVQDecoder - the LRVQ frame mode: lattice/residual vector quantisation
// under an LPC spectral envelope, the alternative to the transform/SQ path in
// APACSpectralData.
//
// Originals in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
//
//   0x0009a950  APACLRVQDecoder::dequantizeLrvqGain(u16, u16, float*)
//   0x0009aa10  APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, const u16[5], float[1024])
//   0x0009aee0  APACLRVQDecoder::ltpToMdctSpectralEnvelope(u16, u16, float[1024])
//   0x0009b100  APACLRVQDecoder::LrvqDeserialize(...)
//   0x0009bdc0  APACLRVQDecoder::SpectrumDeserializeSCE(...)
//   0x0009bf60  APACLRVQDecoder::SpectrumDeserializeCPE(...)
//   0x0009c1f0  APACLRVQDecoder::SpectrumDeserializeLFE(...)
//   0x000ac600  APACChannelPairElement::DeserializeLrvqFrame(BitstreamReader&)
//
// Semantics established against the running library in
// reference/decomp/LRVQ_SEMANTICS.md section 3 and 4, and re-derived from the
// disassembly here.  Two things Hex-Rays got wrong and this port does not
// repeat:
//
//   * every one of those APACLRVQDecoder members is STATIC.  The decompiler
//     invented a `this` and shifted every argument by one, which is why its
//     pseudocode for LrvqDeserialize is unusable and the port comes from the
//     appended disassembly instead.
//   * the `+0x2F0` / `+0x2F8` pointer pair is APACIndividualChannelStream's
//     APACSpectralData, not anything on APACICSInfo.
//
// The class here is not static because two of the buffers the original puts on
// the stack are far too big for one: SpectrumDeserialize{SCE,CPE,LFE} each
// alloca 9.5 KiB of envelope / residual scratch, and lsfsToMdctSpectralEnvelope
// stack-constructs a 512-point apac::LPMDCTFilter.  Those become members.

#ifndef APAC_LRVQ_H
#define APAC_LRVQ_H

#include <stdint.h>

#include "apac/bitstream.h"
#include "apac/ics_info.h"
#include "apac/lpc.h"
#include "apac/spectrum.h"
#include "apac/tns.h"

namespace apac {

// One row of the VQ mode table at 0x42b1f8, 32 bytes in the original.  The two
// codebook pointers are relocations there; here they are float offsets into the
// single contiguous kLrvqVQCodebook block, and -1 stands for NULL.
//
// `lo` and `hi` are a bit-budget range the DECODER never reads - they are how
// the encoder picks a mode from a budget - and are carried only so that the
// record is the record.
struct LrvqMode {
    uint16_t index;
    uint16_t lo;
    uint16_t hi;
    uint16_t totalBits;
    int32_t  cb0;        // float offset into kLrvqVQCodebook, -1 = NULL
    int32_t  cb1;
    int16_t  bits0;
    int16_t  n0;         // == 1 << bits0
    int16_t  bits1;
    int16_t  n1;         // == 1 << bits1, 0 when cb1 is NULL
};

extern const LrvqMode kLrvqModes[16];

// 992 vectors of 8 floats, 0x310290 .. 0x317e90.  One array on purpose - see
// src/lrvq_tables.cpp.
extern const float* const kLrvqVQCodebook;
// 32 floats at 0x32ff00, the divisor the shortLsfBooks layout applies.
extern const float* const kLrvqRamp32;
// The seven LSF codebooks lsfsToMdctSpectralEnvelope adds together.
extern const float* const kLrvqLsfCB11;
extern const float* const kLrvqLsfCB11Short;
extern const float* const kLrvqLsfCB12;
extern const float* const kLrvqLsfCB12Short;
extern const float* const kLrvqLsfCB21;
extern const float* const kLrvqLsfCB22;
extern const float* const kLrvqLsfCB23;

namespace tables {
extern const uint32_t kLrvqVQBits[7936];
extern const uint32_t kLrvqRamp32Bits[32];
extern const uint32_t kLrvqHalfBits;
extern const uint32_t kLrvqZeroBits;
extern const uint32_t kLrvqLsfCB11Bits[4096];
extern const uint32_t kLrvqLsfCB11ShortBits[4096];
extern const uint32_t kLrvqLsfCB12Bits[4096];
extern const uint32_t kLrvqLsfCB12ShortBits[4096];
extern const uint32_t kLrvqLsfCB21Bits[2048];
extern const uint32_t kLrvqLsfCB22Bits[2048];
extern const uint32_t kLrvqLsfCB23Bits[4096];
}  // namespace tables

// LRVQ writes 256 long coefficients or 8 x 32 short ones; the rest of the
// 1024-coefficient spectrum is the caller's, and the caller zeroes it first.
const uint32_t kLrvqCoefficients  = 256;
const uint32_t kLrvqEnvelopeBins  = 256;
const uint32_t kLrvqVQVectors     = 32;    // 32 index pairs per frame
const uint32_t kLrvqVQDimension   = 8;
const uint32_t kLrvqLFECoefficients = 12;  // 4 indices x 3 components
const uint32_t kLrvqLSFOrder      = 16;
const float    kLrvqLSFSampleRate = 24000.0f;
const uint32_t kLrvqLSFFFTSize    = 512;
const uint32_t kLrvqLTPFFTSize    = 2048;

class LRVQDecoder {
public:
    LRVQDecoder();

    // What SpectrumDeserializeSCE does on entry: the inter-channel prediction
    // state goes back to zero.  See the note on mQIdx below for why the port
    // does this in CPE and LFE too.
    void ResetPrediction();

    // 0x9a950.  g = (mode == 1 ? 10^(1 + 2t) : 10^(1 - 4t)) * 65536, t = idx/511.
    // Always returns 0.
    static int32_t DequantizeGain(uint16_t gainIndex, uint16_t gainMode,
                                  float* out);

    // 0x9aee0.  |1 - g*e^{-j*w*lag}| / (1 + g) on the 2048-point bin grid,
    // 256 bins, then out[256..1023] = 0.  Always returns 0.
    static int32_t LtpToMdctSpectralEnvelope(uint16_t gainIndex, uint16_t lag,
                                             float* out);

    // 0x9aa10.  Two-stage LSF VQ -> 16 LSFs in Hz -> 17 LPC coefficients ->
    // 512-point |A(w)| over 256 bins, then out[256..1023] = 0.  Always
    // returns 0.
    int32_t LsfsToMdctSpectralEnvelope(bool shortBooks, const uint16_t idx[5],
                                       float* out);

    // 0x9b100.  Returns 0 on success, 1 on a malformed VQ mode, otherwise the
    // sub-function's error.  env, env2, work, qidx and prevMode are shared
    // across a channel pair; that sharing is the whole inter-channel
    // prediction mechanism.
    int32_t LrvqDeserialize(BitstreamReader& bs, bool eightShortWindows,
                            bool firstChannel, SpectralData& spec,
                            float* env, float* env2, float* work,
                            int16_t* qidx, uint16_t* prevMode);

    // 0x9bdc0 / 0x9bf60 / 0x9c1f0.  Each reads one skip bit per channel; a set
    // bit resets that channel's APACICSInfo and leaves its spectrum zero.
    int32_t SpectrumDeserializeSCE(BitstreamReader& bs, APACICSInfo& ics,
                                   SpectralData& spec);
    int32_t SpectrumDeserializeCPE(BitstreamReader& bs, APACICSInfo& ics0,
                                   APACICSInfo& ics1, SpectralData& spec0,
                                   SpectralData& spec1);
    // The LFE has no envelope, no LTP and no reuse: one gain, one 4-bit mode
    // and four VQ index pairs give 12 coefficients.
    int32_t SpectrumDeserializeLFE(BitstreamReader& bs, APACICSInfo& ics,
                                   SpectralData& spec);

    // The three output layouts, exposed the way MDCT exposes PreModulation:
    // the differential test drives them from the LIBRARY's own post-call
    // `work`.  That matters for the reversed short layout, which is reachable
    // only from a channel that read its own LSFs and therefore ran an FFT - so
    // end to end it can only ever be tolerance-checked, while replaying it from
    // the library's work makes the reversal, the window stride and the ramp32
    // division bit-exact.
    //
    // Interleaved and long take `work` before the gain; reversed takes it
    // after, which is the order the original applies them in.
    static void ShortLayoutInterleaved(const float* work, float gain,
                                       float* spec);
    static void ShortLayoutReversed(const float* work, float* spec);
    static void LongOutput(const float* work, float gain, float* spec);

    // The per-element buffers, public so the differential test can drive
    // LrvqDeserialize with an envelope it controls - which is how the VQ loop
    // and the two output layouts get tested bit-exactly, with no FFT under
    // them.
    float    mEnv[1024];
    float    mEnv2[1024];
    float    mWork[256];
    int16_t  mQIdx[64];
    uint16_t mPrevMode;

private:
    LPMDCTFilter mLPMDCT;    // 512-point, for LsfsToMdctSpectralEnvelope
    LSFTools     mLSF;       // order 16, 24000 Hz
    float        mLPC[kLrvqLSFOrder + 2];
};

// APACChannelPairElement::DeserializeLrvqFrame, 0xac600.
//
// The element class itself is stage f's; this is only the LRVQ frame body, so
// it takes the pieces it touches as a context rather than a `this`.  When the
// class lands, its method is a forwarding call.
struct LrvqFrameContext {
    APACICSInfo*    ics[2];        // this + 16,  this + 512
    SpectralData*   spec[2];       // (this + 24 / 520) + 0x2F0
    TNSData*        tns[2];        // this + 128, this + 318
    SFBBandOffsets  sfbLong;       // (*(this + 32) + 16) + 8
    SFBBandOffsets  sfbShort;      // (*(this + 32) + 16) + 24
    LRVQDecoder*    lrvq;
};

// Returns 0 on success and the failing step's code otherwise.
int32_t DeserializeLrvqFrame(BitstreamReader& bs, LrvqFrameContext& ctx);

}  // namespace apac

#endif  // APAC_LRVQ_H
