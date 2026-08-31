// APACLRVQDecoder - the LRVQ frame mode.
//
// Transcribed from the AArch64 disassembly appended to
// reference/decomp/APACLRVQDecoder.c, not from its pseudocode: Hex-Rays
// mistook every one of these static members for a non-static one and shifted
// all their arguments by one.  The semantics were established against the
// running library first (reference/decomp/LRVQ_SEMANTICS.md sections 3 and 4)
// and tests/test_lrvq.cpp re-checks them here.

#include "apac/lrvq.h"

#include <math.h>
#include <string.h>

namespace apac {

namespace {

// The two vDSP scalars in the VQ loop, as the library stores them.  memcpy,
// not a reinterpret_cast: GCC 4.9.4 on the QNX target rejects the cast under
// -Werror=strict-aliasing, and every compiler turns this memcpy into the same
// single load.
inline float LrvqHalf()
{
    float f;
    memcpy(&f, &tables::kLrvqHalfBits, sizeof f);
    return f;
}
inline float LrvqZero()
{
    float f;
    memcpy(&f, &tables::kLrvqZeroBits, sizeof f);
    return f;
}

}  // namespace

LRVQDecoder::LRVQDecoder() : mPrevMode(0)
{
    // The original leaves all of this uninitialised on its own stack.  A port
    // cannot: see the note in ResetPrediction, and the envelope note below.
    for (uint32_t i = 0; i < 1024; ++i) {
        mEnv[i] = 1.0f;
        mEnv2[i] = 1.0f;
    }
    memset(mWork, 0, sizeof mWork);
    memset(mQIdx, 0, sizeof mQIdx);
    memset(mLPC, 0, sizeof mLPC);
    mLPMDCT.Initialize((uint16_t)kLrvqLSFFFTSize, (uint16_t)kLrvqLSFOrder);
    mLSF.order = kLrvqLSFOrder;
    mLSF.sampleRate = kLrvqLSFSampleRate;
}

// SpectrumDeserializeSCE zeroes qidx and prevMode before the call.
// SpectrumDeserializeCPE does NOT - its copies are uninitialised stack (0x9bf60
// has no clear loop and no memset, only the stack-clash probes), so a CPE whose
// FIRST channel carries the skip bit lets the second channel's `sameMode` reuse
// predict from whatever was there.  That is a real uninitialised read in the
// library, and it is also the only route by which the `-1` stage-1 sentinel
// could ever reach a codebook index (see LrvqDeserialize).  The port calls this
// from both, which is deterministic and is what the library itself does in the
// one place it initialises at all.
//
// SpectrumDeserializeLFE needs none of it: the LFE has no inter-channel reuse,
// so it neither reads nor writes qidx or prevMode.
void LRVQDecoder::ResetPrediction()
{
    memset(mQIdx, 0, sizeof mQIdx);
    mPrevMode = 0;
}

// ------------------------------------------------------ dequantizeLrvqGain --
// 0x9a950.  45 instructions, straight line.  511.0f is 0x43FF8000 and 65536.0f
// is 0x47800000; both are exact in float, so the only libm call is powf.
// Checked bit-exact against the library on all 512 indices x both modes.
int32_t LRVQDecoder::DequantizeGain(uint16_t gainIndex, uint16_t gainMode,
                                    float* out)
{
    const float t = (float)gainIndex / 511.0f;
    // The original calls the FLOAT powf.  The port evaluates the same power in
    // double and rounds once, because powf is not correctly rounded everywhere:
    // glibc/aarch64's is (checked against a 200-bit reference on all 1024
    // inputs, 0 disagreements), macOS's is off by one ULP on 6 of them, and
    // QNX 6.5's is untested.  Rounding a double result gives the correctly
    // rounded float on any libm with a sane pow, so this matches the library
    // where powf would not.  Everything after the power stays in float, which
    // is where the remaining rounding lives.
    const float p = (gainMode == 1) ? (float)pow(10.0, (double)(t + t))
                                    : (float)pow(10.0, (double)(t * 4.0f));
    const float g = (gainMode == 1) ? p * 10.0f : 10.0f / p;
    *out = g * 65536.0f;
    return 0;
}

// ------------------------------------------------ ltpToMdctSpectralEnvelope --
// 0x9aee0.  The original builds the 2048-tap impulse response of
// A(z) = 1 - g*z^-lag, takes a 2048-point real FFT magnitude, keeps 256 bins
// and scales by 1/(1+g).  That has a closed form and the closed form is what
// the port uses - it agrees with the library to 4.05e-06 relative over 136 192
// bins and saves a 2048-point transform and its 47 KiB of state.
//
// ONE case the closed form does not cover.  The original writes impulse[0] = 1
// and THEN impulse[lag] = -g, so lag == 0 overwrites the 1.0f and the response
// is the constant -g, whose magnitude is g at every bin.  lag is a 10-bit field
// so 0 is perfectly reachable; the sweep in env_probe.cpp started at lag 1 and
// never saw it.
int32_t LRVQDecoder::LtpToMdctSpectralEnvelope(uint16_t gainIndex, uint16_t lag,
                                               float* out)
{
    const float g = ((float)gainIndex / 127.0f) * 0.74f + 0.25f;
    const double scale = 1.0 / (1.0 + (double)g);
    if (lag == 0) {
        const float v = (float)((double)g * scale);
        for (uint32_t k = 0; k < kLrvqEnvelopeBins; ++k)
            out[k] = v;
    } else {
        const double twoPi = 6.283185307179586476925286766559;
        for (uint32_t k = 0; k < kLrvqEnvelopeBins; ++k) {
            const double w = twoPi * (double)k * (double)lag / 2048.0;
            const double m = 1.0 - 2.0 * (double)g * cos(w) + (double)g * (double)g;
            out[k] = (float)(sqrt(m) * scale);
        }
    }
    memset(out + kLrvqEnvelopeBins, 0,
           sizeof(float) * (1024 - kLrvqEnvelopeBins));
    return 0;
}

// ----------------------------------------------- lsfsToMdctSpectralEnvelope --
// 0x9aa10.  Two-stage split VQ over five 9-bit indices gives 16 LSFs in Hz at a
// nominal 24000 Hz rate; apac::LSFTools::LSF_To_LPC turns them into 17 direct
// form coefficients; a 512-point real FFT of that impulse response gives 256
// magnitudes.  The `bool` picks the *_SHORT variants of the two stage-1 books
// and nothing else - stage 2 is shared.
//
// The original stack-constructs an APACLRVQDecoder::LRVQLPC and an
// APACLRVQDecoder::LPMDCT_LONG for this.  Neither overrides anything that runs
// here (LRVQLPC's only overrides are Initialize, which throws, and the
// destructors), so the concrete chain is apac::LSFTools' and apac::LPMDCTFilter's
// and the port holds one of each as a member.
//
// No bounds check is emitted on idx[0..4] and each book has exactly 512 rows,
// so a 9-bit field is in range by construction rather than by check.  The port
// keeps it that way; the caller reads exactly 9 bits.
int32_t LRVQDecoder::LsfsToMdctSpectralEnvelope(bool shortBooks,
                                                const uint16_t idx[5],
                                                float* out)
{
    float lsf[kLrvqLSFOrder];
    const float* cb11 = shortBooks ? kLrvqLsfCB11Short : kLrvqLsfCB11;
    const float* cb12 = shortBooks ? kLrvqLsfCB12Short : kLrvqLsfCB12;
    const float* s1a = cb11 + 8 * (uint32_t)idx[0];
    const float* s1b = cb12 + 8 * (uint32_t)idx[1];
    const float* s2a = kLrvqLsfCB21 + 4 * (uint32_t)idx[2];
    const float* s2b = kLrvqLsfCB22 + 4 * (uint32_t)idx[3];
    const float* s2c = kLrvqLsfCB23 + 8 * (uint32_t)idx[4];

    for (uint32_t i = 0; i < 4; ++i)
        lsf[i] = s1a[i] + s2a[i];
    for (uint32_t i = 0; i < 4; ++i)
        lsf[4 + i] = s1a[4 + i] + s2b[i];
    for (uint32_t i = 0; i < 8; ++i)
        lsf[8 + i] = s1b[i] + s2c[i];

    mLSF.LSF_To_LPC(lsf, mLPC);

    // The original memsets impulse[17..511] and memcpys lpc[0..16] into a
    // 512-float stack buffer, which is exactly what LPCToSpecEnvelope does with
    // fftSize 512 and order 16.
    mLPMDCT.LPCToSpecEnvelope(mLPC, out, kLrvqEnvelopeBins);
    memset(out + kLrvqEnvelopeBins, 0,
           sizeof(float) * (1024 - kLrvqEnvelopeBins));
    return 0;
}

// ------------------------------------------------------------ the VQ stage --
namespace {

// One (stage 0, stage 1) index pair -> one column of the 8 x 32 residual.
// `stride` and `count` are the vDSP shape: (32, 8) in LrvqDeserialize and
// (4, 3) in SpectrumDeserializeLFE, which is the only difference between the
// two copies of this loop in the library.
inline void LrvqPlaceVector(const float* cb0, const float* cb1, float* dst,
                            uint32_t stride, uint32_t count)
{
    if (cb1) {
        const float half = LrvqHalf();
        for (uint32_t n = 0; n < count; ++n)
            dst[stride * n] = (cb0[n] + cb1[n]) * half;
    } else {
        const float zero = LrvqZero();
        for (uint32_t n = 0; n < count; ++n)
            dst[stride * n] = cb0[n] + zero;
    }
}

}  // namespace

// ----------------------------------------------------------- the LRVQ frame --
// 0x9b100, 3264 bytes, 811 instructions.
int32_t LRVQDecoder::LrvqDeserialize(BitstreamReader& bs,
                                     bool eightShortWindows, bool firstChannel,
                                     SpectralData& spec, float* env, float* env2,
                                     float* work, int16_t* qidx,
                                     uint16_t* prevMode)
{
    float gain;
    const uint32_t gainMode = bs.GetBits(1);
    const uint32_t gainIndex = bs.GetBits(9);
    int32_t err = DequantizeGain((uint16_t)gainIndex, (uint16_t)gainMode, &gain);
    if (err)
        return err;

    // var_94 in the original, and it means two different things at two points:
    // before the VQ section "no VQ data follows, keep the previous channel's
    // work", and after it "an LTP envelope was decoded, fold it into env".  The
    // transition happens inside the LTP block.
    bool reuseResidual = false;
    bool noLtpSection = false;
    bool shortLsfBooks = false;
    bool reuseEnvelope = false;

    if (!firstChannel) {
        reuseEnvelope = bs.GetBits(1) != 0;
        if (!eightShortWindows)
            noLtpSection = bs.GetBits(1) != 0;
        reuseResidual = bs.GetBits(1) != 0;
    }

    if (firstChannel || !reuseEnvelope) {
        uint16_t lsf[5];
        for (uint32_t i = 0; i < 5; ++i)
            lsf[i] = (uint16_t)bs.GetBits(9);
        if (eightShortWindows)
            shortLsfBooks = bs.GetBits(1) != 0;
        err = LsfsToMdctSpectralEnvelope(shortLsfBooks, lsf, env);
        if (err)
            return err;
    }

    bool runVQ = true;
    bool foldLtp = false;
    if (!eightShortWindows && !noLtpSection) {
        if (bs.GetBits(1)) {
            const uint32_t ltpGain = bs.GetBits(7);
            const uint32_t ltpLag = bs.GetBits(10);
            err = LtpToMdctSpectralEnvelope((uint16_t)ltpGain, (uint16_t)ltpLag,
                                            env2);
            if (err)
                return err;
            if (reuseResidual) {
                // The overload's first meaning applied: no VQ data follows, and
                // the envelope is folded in on the way straight to the output.
                for (uint32_t i = 0; i < kLrvqEnvelopeBins; ++i)
                    env[i] = env[i] * env2[i];
                runVQ = false;
            } else {
                foldLtp = true;
            }
        } else if (reuseResidual) {
            runVQ = false;
        }
    } else if (reuseResidual) {
        runVQ = false;
    }

    if (runVQ) {
        const uint32_t vqMode = bs.GetBits(4);
        // Evaluated BEFORE *prevMode is overwritten.
        const bool sameMode = !firstChannel && *prevMode == vqMode;
        *prevMode = (uint16_t)vqMode;

        const LrvqMode& m = kLrvqModes[vqMode];
        if (m.cb0 < 0 && m.cb1 < 0)
            return 1;
        if (((uint32_t)(uint16_t)m.bits0 | (uint32_t)(uint16_t)m.bits1) == 0)
            return 1;

        const float* cb0 = kLrvqVQCodebook + m.cb0;
        const float* cb1 = (m.cb1 >= 0) ? kLrvqVQCodebook + m.cb1 : 0;

        // The stage-1 sentinel.  A single-stage mode never writes qidx[2k+1],
        // so it keeps this value for the whole frame - and if a later channel
        // ever reused it, cb1[8*(-1) + n] would read the eight floats in front
        // of the table.  It cannot: `sameMode` means both channels chose the
        // same mode ROW, and whether a row has a stage 1 is a property of the
        // row, so a mode that could reuse the sentinel is a mode that never
        // reads one.  The one path that breaks that argument is the library's
        // uninitialised qidx in SpectrumDeserializeCPE, which ResetPrediction
        // closes.  Even so the codebook block is laid out as one array here, so
        // a negative index reads the same bytes it reads in the library rather
        // than off the end of an object.
        int16_t idx1 = -1;
        for (uint32_t k = 0; k < kLrvqVQVectors; ++k) {
            int16_t idx0;
            if (sameMode && bs.GetBits(1))
                idx0 = qidx[2 * k];
            else
                idx0 = (int16_t)bs.GetBits((uint32_t)m.bits0);
            // LDRH of n0 against LDRSH of the index: unsigned table bound,
            // signed compare, and `<` not `<=`, so idx == n0 is let through.
            if ((int32_t)(uint16_t)m.n0 < (int32_t)idx0)
                return 1;

            const float* v0 = cb0 + 8 * (int32_t)idx0;
            const float* v1 = 0;
            if (cb1) {
                if (sameMode && bs.GetBits(1))
                    idx1 = qidx[2 * k + 1];
                else
                    idx1 = (int16_t)bs.GetBits((uint32_t)m.bits1);
                if ((int32_t)(uint16_t)m.n1 < (int32_t)idx1)
                    return 1;
                v1 = cb1 + 8 * (int32_t)idx1;
            }
            LrvqPlaceVector(v0, v1, work + k, kLrvqVQVectors, kLrvqVQDimension);
            qidx[2 * k] = idx0;
            qidx[2 * k + 1] = idx1;
        }

        if (!eightShortWindows && foldLtp) {
            for (uint32_t i = 0; i < kLrvqEnvelopeBins; ++i)
                env[i] = env[i] * env2[i];
        }
    }

    // ---- reconstruction ---------------------------------------------------
    // env holds the ANALYSIS filter magnitude, so dividing by it applies the
    // synthesis filter; when the LTP envelope was folded in above, both
    // synthesis filters are applied at once.
    for (uint32_t i = 0; i < kLrvqCoefficients; ++i)
        work[i] = work[i] / env[i];
    if (eightShortWindows) {
        if (shortLsfBooks) {
            for (uint32_t i = 0; i < kLrvqCoefficients; ++i)
                work[i] = work[i] * gain;
            ShortLayoutReversed(work, spec.begin);
        } else {
            ShortLayoutInterleaved(work, gain, spec.begin);
        }
    } else {
        LongOutput(work, gain, spec.begin);
    }
    return 0;
}

// The shortLsfBooks layout: eight windows of 32, every EVEN window written
// backwards, each divided by the 32-entry ramp at 0x32ff00.  `work` is already
// scaled by the gain.
void LRVQDecoder::ShortLayoutReversed(const float* work, float* spec)
{
    for (uint32_t w = 0; w < 8; ++w) {
        const float* src = work + 32 * w;
        float* dst = spec + 128 * w;
        if (w & 1) {
            memcpy(dst, src, 32 * sizeof(float));
        } else {
            for (uint32_t i = 0; i < 32; ++i)
                dst[i] = src[31 - i];
        }
        for (uint32_t i = 0; i < 32; ++i)
            dst[i] = dst[i] / kLrvqRamp32[i];
    }
}

// The other short layout: `work` read down its COLUMNS - which is the VQ
// component index, not the vector index - and no ramp.
void LRVQDecoder::ShortLayoutInterleaved(const float* work, float gain,
                                         float* spec)
{
    for (uint32_t w = 0; w < 8; ++w)
        for (uint32_t n = 0; n < 32; ++n)
            spec[128 * w + n] = work[w + 8 * n] * gain;
}

void LRVQDecoder::LongOutput(const float* work, float gain, float* spec)
{
    for (uint32_t i = 0; i < kLrvqCoefficients; ++i)
        spec[i] = work[i] * gain;
}

// ---------------------------------------------------- the three entry points --
namespace {

void ZeroSpectrum(SpectralData& spec)
{
    for (float* p = spec.begin; p != spec.end; ++p)
        *p = 0.0f;
}

// APACICSInfo::Reset, devirtualised inline by all three callers.
void ResetICSInfo(APACICSInfo& ics)
{
    ics.windowSequence = 0;
    ics.maxSfb = 0;
    ics.numWindowGroups = 0;
    ics.windowGroupWord = 1;
}

}  // namespace

// 0x9bdc0.
int32_t LRVQDecoder::SpectrumDeserializeSCE(BitstreamReader& bs,
                                            APACICSInfo& ics, SpectralData& spec)
{
    ZeroSpectrum(spec);
    if (bs.GetBits(1)) {
        ResetICSInfo(ics);
        return 0;
    }
    ResetPrediction();
    return LrvqDeserialize(bs, ics.windowSequence == 2, true, spec,
                           mEnv, mEnv2, mWork, mQIdx, &mPrevMode);
}

// 0x9bf60.  Both channels share env, env2, work, qidx and prevMode - that
// sharing IS the inter-channel prediction.
int32_t LRVQDecoder::SpectrumDeserializeCPE(BitstreamReader& bs,
                                            APACICSInfo& ics0, APACICSInfo& ics1,
                                            SpectralData& spec0,
                                            SpectralData& spec1)
{
    ResetPrediction();

    ZeroSpectrum(spec0);
    if (bs.GetBits(1)) {
        ResetICSInfo(ics0);
    } else {
        const int32_t err = LrvqDeserialize(bs, ics0.windowSequence == 2, true,
                                            spec0, mEnv, mEnv2, mWork, mQIdx,
                                            &mPrevMode);
        if (err)
            return err;
    }

    ZeroSpectrum(spec1);
    // ics1's window sequence is read BEFORE the skip bit in the original, which
    // matters only if the two aliased - they cannot.
    const bool short1 = ics1.windowSequence == 2;
    if (bs.GetBits(1)) {
        ResetICSInfo(ics1);
        return 0;
    }
    return LrvqDeserialize(bs, short1, false, spec1, mEnv, mEnv2, mWork, mQIdx,
                           &mPrevMode);
}

// 0x9c1f0.  A second, specialised copy of the VQ loop: no envelope, no LTP, no
// inter-channel reuse and no bound check on the indices.  Four index pairs give
// twelve coefficients - only the first three components of each eight-float
// codebook vector are used.
int32_t LRVQDecoder::SpectrumDeserializeLFE(BitstreamReader& bs,
                                            APACICSInfo& ics, SpectralData& spec)
{
    ZeroSpectrum(spec);
    if (bs.GetBits(1)) {
        ResetICSInfo(ics);
        return 0;
    }

    float gain;
    const uint32_t gainMode = bs.GetBits(1);
    const uint32_t gainIndex = bs.GetBits(9);
    const int32_t err = DequantizeGain((uint16_t)gainIndex, (uint16_t)gainMode,
                                       &gain);
    if (err)
        return err;

    const uint32_t vqMode = bs.GetBits(4);
    const LrvqMode& m = kLrvqModes[vqMode];
    if (m.cb0 < 0 && m.cb1 < 0)
        return 1;
    if (((uint32_t)(uint16_t)m.bits0 | (uint32_t)(uint16_t)m.bits1) == 0)
        return 1;

    const float* cb0 = kLrvqVQCodebook + m.cb0;
    const float* cb1 = (m.cb1 >= 0) ? kLrvqVQCodebook + m.cb1 : 0;

    float work[kLrvqLFECoefficients];
    for (uint32_t k = 0; k < 4; ++k) {
        const int16_t idx0 = (int16_t)bs.GetBits((uint32_t)m.bits0);
        const float* v0 = cb0 + 8 * (int32_t)idx0;
        const float* v1 = 0;
        if (cb1) {
            const int16_t idx1 = (int16_t)bs.GetBits((uint32_t)m.bits1);
            v1 = cb1 + 8 * (int32_t)idx1;
        }
        LrvqPlaceVector(v0, v1, work + k, 4, 3);
    }
    for (uint32_t i = 0; i < kLrvqLFECoefficients; ++i)
        spec.begin[i] = work[i] * gain;
    return 0;
}

}  // namespace apac
