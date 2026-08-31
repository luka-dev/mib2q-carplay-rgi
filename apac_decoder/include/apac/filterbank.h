// APACSynthesisFilterBank - inverse MDCT, windowing and overlap-add.
//
// Original: libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010)
//
//   0x000ae530  APACSynthesisFilterBank::Reset()
//   0x000ae5b0  APACSynthesisFilterBank::FrequencyToTimeInPlace(unsigned, float*,
//                                                              unsigned, unsigned)
//   0x000ae980  APACSynthesisFilterBank::Initialize(unsigned, unsigned)
//
// Decompiled in reference/decomp/APACSynthesisFilterBank.c.
//
// The object is 184 bytes and holds two MDCTs (long at +40, short at +96), a
// per-channel overlap buffer, a per-channel record of the PREVIOUS frame's
// window shape, and four window pointers at +152 arranged as two { short, long }
// pairs indexed by shape.  This port keeps all of that and only replaces the
// two `operator new` buffers with fixed arrays, because the target is an
// LD_PRELOADed .so on QNX 6.5 and the decode path must not allocate.
//
// The two parameters that are easy to swap: the third argument of
// FrequencyToTimeInPlace is the window SHAPE (0 sine, 1 KBD) and the fourth is
// the window SEQUENCE (0..3).  The original proves which is which by writing
// the third into the per-channel shape array and by rejecting a fourth above 3.
//
// frameLength must be 1024.  Initialize looks the short window up at
// frameLength >> 3, and only 128 and 1024 exist, so 1024 is the only frame
// length whose short window resolves - 128 would need a 16-point window that
// the library does not have.

#ifndef APAC_FILTERBANK_H
#define APAC_FILTERBANK_H

#include <stdint.h>

#include "apac/transform.h"

namespace apac {

// Window sequences, as the fourth argument of FrequencyToTimeInPlace.
enum WindowSequence {
    kWindowOnlyLong  = 0,
    kWindowLongStart = 1,
    kWindowEightShort = 2,
    kWindowLongStop  = 3
};

// Window shapes, as the third argument.
enum WindowShape {
    kWindowShapeSine = 0,
    kWindowShapeKBD  = 1
};

// A limit of this port, not of the format: 5.1.2 (CarPlay format index 66) is
// eight channels and nothing in the profile goes above that.
const uint32_t kFilterBankMaxChannels = 16;

class SynthesisFilterBank {
public:
    SynthesisFilterBank();

    // channels <= kFilterBankMaxChannels, frameLength == 1024.
    // Returns 0 on success; 1 or 'size' otherwise, matching the original.
    int32_t Initialize(uint32_t channels, uint32_t frameLength);

    // Zeroes the overlap buffer and the per-channel window-shape memory.
    void Reset();

    // In place: `buf` is frameLength spectral coefficients on the way in and
    // frameLength time samples on the way out.  Returns 0, or 1 if the channel
    // is out of range or the sequence is above 3.
    int32_t FrequencyToTimeInPlace(uint32_t channel, float* buf,
                                   uint32_t windowShape, uint32_t windowSequence);

private:
    // Window pointers, [shape][0] = short, [shape][1] = long, exactly the
    // 16-byte-strided pairs the original keeps at +152.
    const float* mWindow[2][2];

    MDCT     mLong;
    MDCT     mShort;
    uint32_t mChannels;
    uint32_t mFrameLength;   // the long MDCT size, 1024
    uint32_t mShortLength;   // frameLength >> 3, 128
    uint8_t  mPrevShape[kFilterBankMaxChannels];
    float    mOverlap[kFilterBankMaxChannels * kTransformLong];
    // The eight-short path accumulates 9 half-windows: 8 * 128 rising plus the
    // last falling 128.  The original's stack VLA is float[1154].
    float    mScratch[kTransformLong + kTransformShort];
};

}  // namespace apac

#endif  // APAC_FILTERBANK_H
