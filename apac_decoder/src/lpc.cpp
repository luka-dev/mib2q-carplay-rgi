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
        uint32_t i = 1;
        uint32_t hi = m - 1;
        for (;;) {
            const float lo = a[i];
            const float up = a[hi];
            a[hi] = up + k * lo;
            a[i] = lo + k * up;
            ++i;
            if (m - 1 - (m >> 1) == hi - 1) break;
            --hi;
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
    const uint32_t order = mAutocorr->order;
    const uint32_t len = mAutocorr->windowLength;
    const float* win = mAutocorr->window;
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
        for (;;) {
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

}  // namespace apac
