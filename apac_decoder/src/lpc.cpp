#include "apac/lpc.h"

#include "apac/transform.h"

#include <math.h>
#include <string.h>

namespace apac {

LPCBase::LPCBase() : mOrder(0), mLPC(0), mAutocorr(0)
{
    memset(mSignal, 0, sizeof mSignal);
    memset(mCorr, 0, sizeof mCorr);
}

void LPCBase::Configure(uint32_t order, float* lpcOut, const Autocorr* autocorr)
{
    mOrder = order;
    mLPC = lpcOut;
    mAutocorr = autocorr;
}

// apac::LPCBase::LPC_Weighting, 0xb4190.
void LPCBase::LPC_Weighting(const float* in, float* out, float gamma, uint16_t order)
{
    out[0] = in[0];
    float g = gamma;
    for (uint32_t i = 1; i <= (uint32_t)order; ++i) {
        out[i] = in[i] * g;
        g = g * gamma;
    }
}

// apac::LPCBase::LevinsonDurbin, 0xb4620.  Standard, with two habits worth
// keeping verbatim: the pair update reads both a[i] and a[m-i] BEFORE writing
// either, and a prediction error that goes non-positive is clamped to 0.01
// rather than ending the recursion.
void LPCBase::LevinsonDurbin(const float* r)
{
    float* a = mLPC;
    const uint32_t order = mOrder;

    a[0] = 1.0f;
    float k = -r[1] / r[0];
    a[1] = k;
    float err = r[0] + k * r[1];

    for (uint32_t m = 2; m <= order; ++m) {
        float acc = 0.0f;
        for (uint32_t j = 0; j < m; ++j)
            acc = acc + r[m - j] * a[j];
        k = -acc / err;
        // The original spells this as `while (1)` with a computed break, and it
        // runs exactly m>>1 times: the pair (i, m-i) walking inwards.  Written
        // as a counted loop so it cannot run away on a malformed order - the
        // same call the spectrum agent made on the band-zeroing loop.
        for (uint32_t t = 1; t <= (m >> 1); ++t) {
            const uint32_t i = t;
            const uint32_t hi = m - t;
            const float lo = a[i];
            const float up = a[hi];
            a[hi] = up + k * lo;
            a[i] = lo + k * up;
        }
        err = err + k * acc;
        a[m] = k;
        if (err <= 0.0f)
            err = 0.01f;
    }
}

// apac::LPCBase::ComputeLPCs, 0xb4930.
//
// Hex-Rays merges the two stack VLAs into one array here, which makes the
// pseudocode look as if it zeroed the signal it had just windowed.  It does
// not: there are two buffers, one for the windowed signal (windowLength +
// order floats, the tail zero-padded so the autocorrelation lag never runs
// past the end) and one for the autocorrelation itself (order + 1 floats).
void LPCBase::ComputeLPCs(const float* signal, bool lagWindow)
{
    uint32_t order = mAutocorr->order;
    uint32_t len = mAutocorr->windowLength;
    const float* win = mAutocorr->window;
    // The original allocas to fit whatever the Autocorr says; the port has
    // fixed buffers, so an over-long window or order is clamped rather than
    // allowed to run off the end.  Both caps are far above anything the profile
    // produces (orders are 8 to 16, windows a frame long).
    if (order > kMaxLPCOrder) order = kMaxLPCOrder;
    if (len > kMaxLPCWindow) len = kMaxLPCWindow;
    float* x = mSignal;
    float* r = mCorr;
    uint32_t i, n, k;

    for (i = 0; i < len; ++i)
        x[i] = signal[i] * win[i];
    for (i = 0; i <= order; ++i)
        x[len + i] = 0.0f;
    for (i = 0; i <= order; ++i)
        r[i] = 0.0f;

    for (n = 0; n < len; ++n) {
        const float s = x[n];
        for (k = 0; k <= order; ++k)
            r[k] = r[k] + s * x[n + k];
    }

    // A ridge on r[0], multiplicative above 1 and absolute below it.
    r[0] = (r[0] < 1.0f) ? 1.0001f : (r[0] * 1.0001f);

    if (lagWindow && order) {
        // The original hardcodes the first factor as 0.99941 - it is
        // kLPCLagWindow[0] folded into an immediate - and reads the rest of
        // the table from index 1 on, so r[i] takes kLPCLagWindow[i-1].
        for (i = 1; i <= order; ++i)
            r[i] = r[i] * tables::kLPCLagWindow[i - 1].f;
    }

    LevinsonDurbin(r);
}

// ------------------------------------------------------------- LSFTools ----

// apac::LSFTools::LSP_To_LSF, 0xb5010.
void LSFTools::LSP_To_LSF(const float* lsp, float* lsf, uint16_t order, float fs)
{
    const float half = fs * 0.5f;
    for (uint32_t i = 0; i < (uint32_t)order; ++i)
        lsf[i] = (float)((double)half / kLSFPi * (double)acosf(lsp[i]));
}

// apac::LSFTools::LSF_To_LSP, 0xb50f0 and 0xb51c0.  The two differ only in
// whether the destination is the source.
void LSFTools::LSF_To_LSP(const float* lsf, float* lsp, float fs) const
{
    const double half = (double)(fs * 0.5f);
    for (uint32_t i = 0; i < order; ++i)
        lsp[i] = (float)cos((double)lsf[i] * kLSFPi / half);
}

void LSFTools::LSF_To_LSP(float* inout, float fs) const
{
    LSF_To_LSP(inout, inout, fs);
}

// apac::LSFTools::Condition_LSFs, 0xb4390.
void LSFTools::Condition_LSFs(float* lsf, float minGap, float fs) const
{
    const uint32_t n = order;
    if (!n)
        return;
    float limit = -(minGap - fs * 0.5f);
    float floorVal = minGap;
    for (uint32_t i = 0; i < n; ++i) {
        float v = lsf[i];
        if (v < floorVal) { v = floorVal; lsf[i] = floorVal; }
        floorVal = minGap + v;
    }
    if (lsf[n - 1] > limit) {
        for (int32_t i = (int32_t)n - 1; i >= 0; --i) {
            float v = lsf[i];
            if (v > limit) { v = limit; lsf[i] = limit; }
            limit = v - minGap;
        }
    }
}

// apac::LSFTools::ConditionLSFs, 0x9c6e0 - the constants are the original's.
void LSFTools::ConditionLSFs(float* lsf) const
{
    Condition_LSFs(lsf, 50.0f, 24000.0f);
}

// apac::LSFTools::get_LSFWeightingFactors, 0xb4220.  Transcribed with the
// original's loop shape, which handles two entries per iteration and leaves
// the last one (or the last two, for order < 4) to the tail.
void LSFTools::get_LSFWeightingFactors(const float* a, float* w,
                                       uint16_t order, float fs)
{
    float prev = a[0];
    float wPrev = 1.0f / a[0];
    const int end = (int)order - 2;
    uint32_t m = 1;
    float* last = w;

    if (end > 1) {
        uint32_t i = 1, j = 1;
        // Bounded: `i` steps by two and the original's break is `end <= i`, so
        // this cannot exceed order iterations even on a nonsense order.
        for (uint32_t guard = 0; guard <= (uint32_t)order; ++guard) {
            i += 2;
            m = i;
            const float g = 1.0f / (a[j] - prev);
            w[j - 1] = g + wPrev;
            wPrev = 1.0f / (a[j + 1] - a[j]);
            w[j] = g + wPrev;
            j = i;
            if (end <= (int)i) break;
            prev = a[m - 1];
        }
        last = w + (m - 1);
        prev = a[m - 1];
    }
    const float g = 1.0f / (a[m] - prev);
    *last = g + wPrev;
    w[m] = 1.0f / -(a[m] - fs * 0.5f) + g;
}

namespace {

// sub_B40B0, 0xb40b0: builds one of the two half-order polynomials from every
// other LSP.  out gets n + 1 coefficients.
void LSPPolynomial(const float* lsp, float* out, uint32_t n)
{
    out[0] = 1.0f;
    out[1] = lsp[0] * -2.0f;
    if (n <= 1)
        return;
    float prev = 1.0f;
    for (uint32_t m = 1; m < n; ++m) {
        const float c = lsp[2 * m] * -2.0f;
        out[m + 1] = (prev + prev) + out[m] * c;
        for (uint32_t k = m; k >= 2; --k)
            out[k] = out[k] + (out[k - 2] + out[k - 1] * c);
        out[1] = out[1] + c;
        prev = out[m];
    }
}

}  // namespace

// apac::LSFTools::LSP_To_LPC, 0xb4460.
//
// Two half-order polynomials, one from the even LSPs and one from the odd,
// then the difference / sum recursion and the half-sum / half-difference
// split.  WHICH polynomial takes the minus was not readable from the
// decompilation - Hex-Rays gave both stack arrays the same base pointer and
// dropped one side of every merged expression - so it was settled against the
// library instead; tests/test_lpc.cpp is the evidence.
void LSFTools::LSP_To_LPC(const float* lsp, float* lpc, uint16_t order)
{
    const uint32_t half = (uint32_t)order >> 1;
    float f1[kMaxLPCOrder / 2 + 2];
    float f2[kMaxLPCOrder / 2 + 2];

    LSPPolynomial(lsp, f1, half);
    LSPPolynomial(lsp + 1, f2, half);

    // The library's assignment, settled by tests/test_lpc.cpp: the polynomial
    // built from the EVEN-indexed LSPs takes the sum recursion and the odd one
    // the difference.  The other way round leaves lpc[0] and lpc[1] right and
    // every later coefficient off by a constant - exactly the kind of near-miss
    // the decompilation could not have ruled out on its own.
    for (uint32_t j = half; j >= 1; --j) {
        f1[j] = f1[j] + f1[j - 1];
        f2[j] = f2[j] - f2[j - 1];
    }

    lpc[0] = 1.0f;
    for (uint32_t i = 1; i <= half; ++i) {
        lpc[i] = (f1[i] + f2[i]) * 0.5f;
        lpc[order + 1 - i] = (f1[i] - f2[i]) * 0.5f;
    }
}

// apac::LSFTools::LSF_To_LPC, 0xb5370.  Three virtual calls in the original,
// all through the object's own vtable:
//
//   if (vtable[136] == apac::LSFTools::ConditionLSFs)   // slot 17
//        vtable[128](this, lsf, 50.0f, 24000.0f);       // Condition_LSFs
//   else vtable[136](this, lsf);                        // a derived override
//   vtable[96] (this, lsf, lsp, *(float*)(this + 112)); // LSF_To_LSP
//   vtable[112](this, lsp, lpc, *(u16*)(this + 104));   // LSP_To_LPC
//
// The equality test is a devirtualisation: for apac::LSFTools itself and for
// APACLRVQDecoder::LRVQLPC - and, it turned out, for BWELPC as well - slot 17
// IS ConditionLSFs, so the branch taken is the one with the literal 50 and
// 24000, whatever this object's own sampleRate says.
//
// CORRECTION, stage e.  This note used to say BWELPC overrides slot 17 and
// takes the other branch.  It does not: BWELPC's vtable at 0x0042b5d8
// overrides EXACTLY ONE slot, `Initialize(void)`, and every other entry is
// apac::LSFTools' or apac::LPCBase's own.  BWELPC also never calls LSF_To_LPC
// at all - BWELPC::DecodeLSFsToLPCs calls Condition_LSFs, LSF_To_LSP and
// LSP_To_LPC itself, with the same two literals.  Read out of the binary.
//
// Note the two rates are not the same value: Condition_LSFs is handed the
// hardcoded 24000 and LSF_To_LSP the member.  For every caller here they agree.
void LSFTools::LSF_To_LPC(float* lsf, float* lpc) const
{
    float lsp[kMaxLPCOrder];
    ConditionLSFs(lsf);
    LSF_To_LSP(lsf, lsp, sampleRate);
    LSP_To_LPC(lsp, lpc, (uint16_t)order);
}

// --------------------------------------------------------- LPMDCTFilter ----

LPMDCTFilter::LPMDCTFilter() : mFFTSize(0), mOrder(0), mScale(0.0f)
{
    memset(mTmp, 0, sizeof mTmp);
    memset(mImpulse, 0, sizeof mImpulse);
    memset(mAcc, 0, sizeof mAcc);
}

// apac::LPMDCTFilter::Initialize(unsigned short, unsigned short), 0x9c7b0.
float LPMDCTFilter::Initialize(uint16_t fftSize, uint16_t order)
{
    mFFTSize = fftSize;
    mOrder = order;
    mFFT.Initialize(fftSize);
    mScale = 2.0f / (float)fftSize;
    return mScale;
}

// apac::LPMDCTFilter::LPImpulseToSpecEnvelope, 0xb4ce0.  Forward real FFT, then
// vDSP_vdist - the magnitude of each bin.  The Nyquist slot is zeroed first;
// after the packing that means bin 0's magnitude is |DC| rather than
// sqrt(DC^2 + Nyquist^2), which would be meaningless.
void LPMDCTFilter::LPImpulseToSpecEnvelope(const float* impulse, float* env,
                                           uint32_t n)
{
    const uint32_t half = mFFTSize >> 1;
    float* re = mTmp;
    float* im = mTmp + half;
    mFFT.Forward(impulse, re, im);
    im[0] = 0.0f;
    const uint32_t stride = (n && mFFTSize / n == 4) ? 2 : 1;
    for (uint32_t i = 0; i < n; ++i) {
        const float a = re[i * stride];
        const float b = im[i * stride];
        env[i] = sqrtf(a * a + b * b);
    }
}

// apac::LPMDCTFilter::LPCToSpecEnvelope, 0xb4be0: zero-pad the coefficients to
// the transform length and hand them to LPImpulseToSpecEnvelope, which the
// original reaches through its own vtable.
void LPMDCTFilter::LPCToSpecEnvelope(const float* lpc, float* env, uint32_t n)
{
    memset(mImpulse, 0, sizeof(float) * mFFTSize);
    memcpy(mImpulse, lpc, sizeof(float) * (mOrder + 1));
    LPImpulseToSpecEnvelope(mImpulse, env, n);
}

// apac::LPMDCTFilter::AutocorrFromPSD, 0xb47b0.  Square the magnitudes into the
// real half of a packed spectrum, zero the Nyquist slot, inverse transform, and
// read off the first order+1 lags - every second one when fftSize/n is 4.
void LPMDCTFilter::AutocorrFromPSD(const float* mag, float* r, uint32_t n)
{
    const uint32_t N = mFFTSize;
    memset(mTmp, 0, sizeof(float) * N);
    for (uint32_t i = 0; i < n; ++i)
        mTmp[i] = mag[i] * mag[i];
    mTmp[N >> 1] = 0.0f;
    mFFT.Inverse(mTmp, mTmp + (N >> 1), mTmp);
    const uint32_t stride = (n && N / n == 4) ? 2 : 1;
    for (uint32_t i = 0; i <= mOrder; ++i)
        r[i] = mTmp[i * stride];
}

// apac::LPMDCTFilter::MDCTToAutoCorr, 0xb4de0.
void LPMDCTFilter::MDCTToAutoCorr(const float* spec, float* r, uint32_t hop,
                                  uint32_t count, uint32_t n)
{
    memset(r, 0, sizeof(float) * (mOrder + 1));
    for (uint32_t i = 0; i < count; ++i) {
        AutocorrFromPSD(spec + i * hop, mAcc, n);
        for (uint32_t j = 0; j <= mOrder; ++j)
            r[j] = mAcc[j] + r[j];
    }
}

}  // namespace apac
