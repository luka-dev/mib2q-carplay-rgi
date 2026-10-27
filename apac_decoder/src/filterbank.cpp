/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apac/filterbank.h"

#include <string.h>

namespace apac {

SynthesisFilterBank::SynthesisFilterBank()
    : mChannels(0), mFrameLength(0), mShortLength(0)
{
    mWindow[0][0] = mWindow[0][1] = mWindow[1][0] = mWindow[1][1] = 0;
    memset(mPrevShape, 0, sizeof mPrevShape);
    memset(mOverlap, 0, sizeof mOverlap);
    memset(mScratch, 0, sizeof mScratch);
}

// APACSynthesisFilterBank::Initialize, 0xae980.  The library allocates the two
// buffers here and then calls Reset() through its own vtable; our decoder has the
// buffers already and calls Reset() directly.
int32_t SynthesisFilterBank::Initialize(uint32_t channels, uint32_t frameLength)
{
    if (channels == 0 || channels > kFilterBankMaxChannels)
        return kTransformBadSize;

    int32_t rc = mLong.Initialize(frameLength, 0.0f);
    if (rc != 0)
        return rc;

    mChannels = channels;
    mFrameLength = frameLength;
    mShortLength = frameLength >> 3;
    Reset();

    rc = mShort.Initialize(mShortLength, 0.0f);
    if (rc != 0)
        return rc;

    mWindow[0][0] = GetSineWindowCoefficientPtr(mShortLength);
    mWindow[0][1] = GetSineWindowCoefficientPtr(frameLength);
    mWindow[1][0] = GetKBDWindowCoefficientPtr(mShortLength);
    mWindow[1][1] = GetKBDWindowCoefficientPtr(frameLength);
    return (!mWindow[0][0] || !mWindow[0][1] || !mWindow[1][0] || !mWindow[1][1]) ? 1 : 0;
}

// APACSynthesisFilterBank::Reset, 0xae530.
void SynthesisFilterBank::Reset()
{
    if (mChannels && mFrameLength) {
        memset(mOverlap, 0, sizeof(float) * (size_t)mChannels * mFrameLength);
        memset(mPrevShape, 0, mChannels);
    }
}

// APACSynthesisFilterBank::FrequencyToTimeInPlace, 0xae5b0.
//
// The library is one function with four exits; the vDSP calls it uses map to
// these three loops:
//
//     vDSP_vma (A,1,B,1,C,1,D,1,n)   D[i] = A[i]*B[i] + C[i]
//     vDSP_vmul(A,1,B,-1,C,1,n)      C[i] = A[i]*B[-i]     (B walks backwards)
//     vDSP_vadd(A,1,B,1,C,1,n)       C[i] = A[i] + B[i]
//
// The rising half always uses the PREVIOUS frame's window shape and the falling
// half the current one, which is why the shape is remembered per channel.
int32_t SynthesisFilterBank::FrequencyToTimeInPlace(uint32_t channel, float* buf,
                                                    uint32_t windowShape,
                                                    uint32_t windowSequence)
{
    if (channel >= mChannels || windowSequence > 3)
        return 1;

    const uint32_t N = mFrameLength;                 // 1024
    const uint32_t S = mShortLength;                 // 128
    const uint32_t L = (N - S) >> 1;                 // 448, the flat shoulder
    float* ov = mOverlap + (size_t)channel * N;
    float* acc = mScratch;
    const uint32_t prevShape = mPrevShape[channel];
    const float* const* prev = mWindow[prevShape & 1];
    const float* const* cur  = mWindow[windowShape & 1];
    uint32_t i, k;

    if (windowSequence == kWindowEightShort) {
        // Eight 128-point transforms, hopped by 128, overlapped into a 1152
        // sample run.  The first sub-window rises with the previous frame's
        // shape; every later one uses the current shape.
        memset(acc, 0, sizeof(float) * S);
        float* second = acc + N;                     // scratch for each IMDCT
        float* dst = acc;
        const float* rise = prev[0];
        for (i = 0; i < 8; ++i) {
            float* sub = buf + i * S;
            mShort.InverseInPlaceTransform(sub, second);
            for (k = 0; k < S; ++k)
                dst[k] = sub[k] * rise[k] + dst[k];
            dst += S;
            for (k = 0; k < S; ++k)
                dst[k] = second[k] * cur[0][S - 1 - k];
            rise = cur[0];
        }
        memcpy(buf, ov, sizeof(float) * L);
        for (k = 0; k < N - L; ++k)
            buf[L + k] = acc[k] + ov[L + k];
        memcpy(ov, acc + (N - L), sizeof(float) * (N - L));
        memset(ov + (N - L), 0, sizeof(float) * L);
        mPrevShape[channel] = (uint8_t)windowShape;
        return 0;
    }

    mLong.InverseInPlaceTransform(buf, acc);

    if (windowSequence == kWindowLongStop) {
        // Flat, then the short rise, then flat: the overlap carries the whole
        // left half unwindowed.
        memcpy(buf, ov, sizeof(float) * L);
        for (k = 0; k < S; ++k)
            buf[L + k] = buf[L + k] * prev[0][k] + ov[L + k];
        for (k = L + S; k < N; ++k)
            buf[k] = buf[k] + ov[k];
    } else {
        for (k = 0; k < N; ++k)
            buf[k] = buf[k] * prev[1][k] + ov[k];
    }

    if (windowSequence == kWindowLongStart) {
        // Falling half is flat for L, then the short fall, then zero.
        memcpy(ov, acc, sizeof(float) * L);
        for (k = 0; k < S; ++k)
            ov[L + k] = acc[L + k] * cur[0][S - 1 - k];
        memset(ov + L + S, 0, sizeof(float) * (N - L - S));
    } else {
        for (k = 0; k < N; ++k)
            ov[k] = acc[k] * cur[1][N - 1 - k];
    }

    mPrevShape[channel] = (uint8_t)windowShape;
    return 0;
}

}  // namespace apac
