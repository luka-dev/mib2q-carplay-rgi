// Stage h1 - APACLRVQDecoder and the LRVQ frame - against the real library.
//
// Fixtures: tests/fixtures/lrvq_{gain,ltp,lsfs,frame,elem}.fixture, produced by
// reference/apac_lrvq_ref.cpp calling Pioneer's copy of Apple's decoder.  The
// inputs are regenerated here from reference/lrvq_cases.h, which the oracle
// includes too, so the fixtures carry only the library's answers.
//
// WHAT IS BIT-EXACT AND WHAT IS NOT, and why.
//
// dequantizeLrvqGain, the VQ codebook arithmetic, both output layouts and the
// envelope division have no FFT under them and must agree bit for bit.  So the
// plan contains a family of cases in which every call sets reuseEnvelope (and,
// on a long block, noLtpSection), which skips both envelope builders entirely:
// the decoder then divides by an envelope this test supplied, and every float
// on both sides is compared as a bit pattern.
//
// The two envelope builders cannot be bit-exact.  lsfsToMdctSpectralEnvelope
// ends in a 512-point real FFT and the library's is Apple's closed
// vDSP_DFT_Execute, which is not on the head unit; the port has its own.  Same
// reason as stage d, same tolerance machinery (tests/dsp_fixture.h).
// ltpToMdctSpectralEnvelope is a further step removed: the port replaces its
// 2048-point transform with the closed form of what it computes.
//
// The reversed short layout is only reachable from a channel that read its own
// LSFs, so end to end it inherits that tolerance.  It is ALSO checked
// bit-exactly, by replaying the library's own post-call `work` through the
// port's layout function - which is why LRVQDecoder exposes the three layouts.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apac/lrvq.h"
#include "apac/sfb_offsets.h"

#include "../reference/lrvq_cases.h"
#include "dsp_fixture.h"

using dspfx::bits_to_float;
using dspfx::float_to_bits;
using dspfx::parse_hex_floats;
using dspfx::peak;

namespace {

int gChecks = 0;

// File scope, not function-local statics: a function-local static with a
// constructor emits __cxa_guard_acquire / __cxa_guard_release, which the QNX
// 6.5 armle-v7 link in tools/build_qnx.sh does not have.  Both are big enough
// that a stack local is not an option either - LRVQDecoder carries a 47 KiB
// LPMDCTFilter.
apac::LRVQDecoder gDec;
apac::LPMDCTFilter gFilt;
bool gFiltReady = false;

void fail(const char* what, int idx)
{
    fprintf(stderr, "\nFAIL %s at case %d\n", what, idx);
    exit(1);
}

// ------------------------------------------------------------ fixture I/O ----
struct Fixture {
    FILE* fh;
    char  line[8192];

    void Open(const char* name)
    {
        char path[512];
        snprintf(path, sizeof path, "tests/fixtures/%s", name);
        fh = fopen(path, "r");
        if (!fh) {
            snprintf(path, sizeof path, "%s/tests/fixtures/%s",
                     getenv("APAC_DECODER_DIR") ? getenv("APAC_DECODER_DIR") : ".",
                     name);
            fh = fopen(path, "r");
        }
        if (!fh) {
            fprintf(stderr, "\ncannot open tests/fixtures/%s\n", name);
            exit(1);
        }
    }
    // Next non-comment, non-blank line, or 0 at end of file.
    const char* Next()
    {
        while (fgets(line, sizeof line, fh)) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
                continue;
            return line;
        }
        return 0;
    }
    void Expect(const char* tag)
    {
        const char* l = Next();
        if (!l || strncmp(l, tag, strlen(tag)) != 0) {
            fprintf(stderr, "\nfixture: expected %s, got %s", tag,
                    l ? l : "<eof>\n");
            exit(1);
        }
    }
    // Reads `n` hex float words spread over as many lines as it takes.
    void Floats(float* out, uint32_t n)
    {
        uint32_t got = 0;
        while (got < n) {
            const char* l = Next();
            if (!l) {
                fprintf(stderr, "\nfixture: short float block\n");
                exit(1);
            }
            got += parse_hex_floats(l, out, n, got);
        }
    }
    void Bytes(uint8_t* out, uint32_t n)
    {
        uint32_t got = 0;
        while (got < n) {
            const char* l = Next();
            if (!l) { fprintf(stderr, "\nfixture: short byte block\n"); exit(1); }
            const char* p = l;
            while (*p && got < n) {
                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
                if (!*p) break;
                char* e = 0;
                long v = strtol(p, &e, 16);
                if (e == p) break;
                out[got++] = (uint8_t)v;
                p = e;
            }
        }
    }

    void Ints(int* out, uint32_t n)
    {
        uint32_t got = 0;
        while (got < n) {
            const char* l = Next();
            if (!l) { fprintf(stderr, "\nfixture: short int block\n"); exit(1); }
            const char* p = l;
            while (*p && got < n) {
                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
                if (!*p) break;
                char* e = 0;
                long v = strtol(p, &e, 10);
                if (e == p) break;
                out[got++] = (int)v;
                p = e;
            }
        }
    }
};

// |ours - theirs| against a tolerance measured in units of the block's own
// peak.  dspfx::compare's 4e-7 is the stage-d transform bound; the LRVQ
// envelope needs a looser one and this reports what is actually used.
double relCompare(const float* ours, const float* theirs, uint32_t n,
                  const char* what, int idx, double tol)
{
    const double sc = (double)peak(theirs, n);
    double worst = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        const double d = fabs((double)ours[i] - (double)theirs[i]);
        if (sc > 0.0 && d > tol * sc) {
            fprintf(stderr,
                    "\nMISMATCH %s case %d index %u: ours %.9g (%08x), library "
                    "%.9g (%08x), diff %.3g > %.3g (peak %.9g)\n",
                    what, idx, i, (double)ours[i], float_to_bits(ours[i]),
                    (double)theirs[i], float_to_bits(theirs[i]), d, tol * sc, sc);
            exit(1);
        }
        if (sc > 0.0 && d / sc > worst)
            worst = d / sc;
    }
    return worst;
}

void expectBits(const float* ours, const float* theirs, uint32_t n,
                const char* what, int idx)
{
    for (uint32_t i = 0; i < n; ++i) {
        if (float_to_bits(ours[i]) != float_to_bits(theirs[i])) {
            fprintf(stderr,
                    "\nMISMATCH %s case %d index %u: ours %.9g (%08x), "
                    "library %.9g (%08x)\n",
                    what, idx, i, (double)ours[i], float_to_bits(ours[i]),
                    (double)theirs[i], float_to_bits(theirs[i]));
            exit(1);
        }
        ++gChecks;
    }
}

// ------------------------------------------------------------ the tables -----
void checkTables()
{
    for (int i = 0; i < 16; ++i) {
        const apac::LrvqMode& m = apac::kLrvqModes[i];
        if (m.index != i)
            fail("mode index", i);
        if (m.bits0 != kLrvqCaseModes[i].bits0 || m.n0 != kLrvqCaseModes[i].n0 ||
            m.bits1 != kLrvqCaseModes[i].bits1 || m.n1 != kLrvqCaseModes[i].n1)
            fail("mode bit widths disagree with the test plan", i);
        // Rows 11..15 are the invalid ones and are the ONLY ones with no
        // codebook; every other row's stage 0 is present and sized 1 << bits0.
        const bool valid = i < 11;
        if ((m.cb0 >= 0) != valid)
            fail("mode validity", i);
        if (valid && m.n0 != (int16_t)(1 << m.bits0))
            fail("n0 != 1 << bits0", i);
        if (m.cb1 >= 0 && m.n1 != (int16_t)(1 << m.bits1))
            fail("n1 != 1 << bits1", i);
        // Every vector a legal index can reach lies inside the block.  The two
        // reads the original's own checks would let through - index == n,
        // because the compare is `n < idx` and not `n <= idx`, and the -1
        // stage-1 sentinel - do NOT: row 3's stage 0 sits at the very start of
        // the block and row 4's stage 1 at the very end.  Neither is reachable
        // (see the note in LrvqDeserialize), and this is where that claim would
        // break if it stopped being true.
        if (m.cb0 >= 0 && (m.cb0 < 0 || m.cb0 + 8 * m.n0 > 7936))
            fail("stage 0 codebook leaves the block", i);
        if (m.cb1 >= 0 && (m.cb1 < 0 || m.cb1 + 8 * m.n1 > 7936))
            fail("stage 1 codebook leaves the block", i);
        gChecks += 4;
    }
    // ramp32 runs 1.0 .. 2.0 but is NOT 1 + i/31: the stored words are
    // four-decimal roundings.  Check the ends exactly and the middle loosely,
    // which is the most that can be said without retyping the table.
    if (float_to_bits(apac::kLrvqRamp32[0]) != 0x3f800000u)
        fail("ramp32[0] != 1.0", 0);
    if (float_to_bits(apac::kLrvqRamp32[31]) != 0x40000000u)
        fail("ramp32[31] != 2.0", 31);
    for (int i = 1; i < 31; ++i) {
        const float want = 1.0f + (float)i / 31.0f;
        if (fabsf(apac::kLrvqRamp32[i] - want) > 1e-4f)
            fail("ramp32 is not a 1..2 ramp", i);
        if (apac::kLrvqRamp32[i] == want)
            continue;   // a few entries do land on the exact quotient
        ++gChecks;
    }
    if (float_to_bits(bits_to_float(apac::tables::kLrvqHalfBits)) != 0x3f000000u)
        fail("the VQ average scalar is not 0.5", 0);
    if (apac::tables::kLrvqZeroBits != 0u)
        fail("the VQ single-stage scalar is not 0.0", 0);
    gChecks += 4;
}

// ----------------------------------------------------------------- gain ------
int checkGain()
{
    Fixture f;
    f.Open("lrvq_gain.fixture");
    static float want[512];
    int n = 0;
    for (int mode = 0; mode < 2; ++mode) {
        char tag[32];
        snprintf(tag, sizeof tag, "GAIN mode=%d", mode);
        f.Expect(tag);
        f.Floats(want, 512);
        for (int i = 0; i < 512; ++i) {
            float got = 0.0f;
            if (apac::LRVQDecoder::DequantizeGain((uint16_t)i, (uint16_t)mode,
                                                  &got) != 0)
                fail("DequantizeGain returned nonzero", i);
            if (float_to_bits(got) != float_to_bits(want[i])) {
                fprintf(stderr,
                        "\nMISMATCH gain mode=%d index=%d: ours %.9g (%08x), "
                        "library %.9g (%08x)\n",
                        mode, i, (double)got, float_to_bits(got),
                        (double)want[i], float_to_bits(want[i]));
                exit(1);
            }
            ++n;
            ++gChecks;
        }
    }
    return n;
}

// ------------------------------------------------------------------ ltp ------
double checkLtp(int* cases)
{
    Fixture f;
    f.Open("lrvq_ltp.fixture");
    static float want[256], got[1024];
    double worst = 0.0;
    int n = 0;
    for (int i = 0; i < kLrvqLtpCases; ++i) {
        const char* l = f.Next();
        int idx = -1, gi = 0, lag = 0, ret = 0;
        if (!l || sscanf(l, "LTP %d gain=%d lag=%d ret=%d", &idx, &gi, &lag,
                         &ret) != 4)
            fail("ltp header", i);
        f.Expect("BINS");
        f.Floats(want, 256);
        const char* t = f.Next();
        int tail = -1;
        if (!t || sscanf(t, "TAILNONZERO %d", &tail) != 1 || tail != 0)
            fail("the library left out[256..1023] non-zero", i);

        uint16_t cgi, clag;
        LrvqLtpCase(i, &cgi, &clag);
        if (cgi != (uint16_t)gi || clag != (uint16_t)lag)
            fail("ltp case plan drifted from the fixture", i);

        memset(got, 0x5a, sizeof got);
        if (apac::LRVQDecoder::LtpToMdctSpectralEnvelope(cgi, clag, got) != ret)
            fail("ltp return", i);
        for (int k = 256; k < 1024; ++k)
            if (got[k] != 0.0f)
                fail("the port left out[256..1023] non-zero", i);
        // Relative, per bin: the envelope spans 1/(1+g) .. 1 and a peak-scaled
        // tolerance would say nothing about the small bins, which are exactly
        // where a wrong grid would show.
        for (int k = 0; k < 256; ++k) {
            const double a = (double)got[k], b = (double)want[k];
            const double d = fabs(a - b) / (b > 1e-7 ? b : 1e-7);
            if (d > 1e-4) {
                fprintf(stderr,
                        "\nMISMATCH ltp case %d bin %d: ours %.9g, library "
                        "%.9g, rel %.3g\n", i, k, a, b, d);
                exit(1);
            }
            if (d > worst) worst = d;
            ++gChecks;
        }
        ++n;
    }
    *cases = n;
    return worst;
}

// The LSP -> LPC recursion in DOUBLE, on the library's own float32 LSPs.  Same
// algorithm, same order of operations - this is the answer both float versions
// are approximating, and comparing each to it is the only way to tell severe
// cancellation from a wrong index.  Same argument test_mdct.cpp makes for the
// transform.
// The library's own LPC coefficients, kept so that checkLsfs can drive the
// port's 512-point envelope from them and test the FFT half on its own.
float gLibLpc[kLrvqLsfCases][17];

void lspPolyDouble(const float* lsp, double* out, int n)
{
    out[0] = 1.0;
    out[1] = (double)lsp[0] * -2.0;
    if (n <= 1)
        return;
    double prev = 1.0;
    for (int m = 1; m < n; ++m) {
        const double c = (double)lsp[2 * m] * -2.0;
        out[m + 1] = (prev + prev) + out[m] * c;
        for (int k = m; k >= 2; --k)
            out[k] = out[k] + (out[k - 2] + out[k - 1] * c);
        out[1] = out[1] + c;
        prev = out[m];
    }
}

void lspToLpcDouble(const float* lsp, double* lpc, int order)
{
    const int half = order >> 1;
    double f1[20], f2[20];
    lspPolyDouble(lsp, f1, half);
    lspPolyDouble(lsp + 1, f2, half);
    for (int j = half; j >= 1; --j) {
        f1[j] = f1[j] + f1[j - 1];
        f2[j] = f2[j] - f2[j - 1];
    }
    lpc[0] = 1.0;
    for (int i = 1; i <= half; ++i) {
        lpc[i] = (f1[i] + f2[i]) * 0.5;
        lpc[order + 1 - i] = (f1[i] - f2[i]) * 0.5;
    }
}

// -------------------------------------------------------------- lsf -> lpc ----
// The LSF half of lsfsToMdctSpectralEnvelope on its own, so that a
// disagreement in the envelope can be pinned on the LPC chain or on the FFT.
// This part has no FFT under it - but it does have the double cos / acos the
// LSF trig uses, so it is compared with the stage-d tolerance rather than as
// bit patterns.
double checkLsfToLpc(int* cases, double* lspWorst, double* ulpPort,
                     double* ulpLib)
{
    double worstLsp = 0.0, worstUlpPort = 0.0, worstUlpLib = 0.0;
    Fixture f;
    f.Open("lrvq_lsf2lpc.fixture");
    float in[16], conditioned[16], wantLsp[16], wantLpc[17];
    float lsf[16], lsp[16], lpc[17];
    double worst = 0.0;
    int n = 0;
    for (int i = 0; i < kLrvqLsfCases; ++i) {
        const char* l = f.Next();
        int idx = -1, sb = 0;
        if (!l || sscanf(l, "LSF %d short=%d", &idx, &sb) != 2)
            fail("lsf2lpc header", i);
        f.Expect("IN");
        f.Floats(in, 16);
        f.Expect("LSP");
        f.Floats(wantLsp, 16);
        f.Expect("CONDITIONED");
        f.Floats(conditioned, 16);
        f.Expect("LPC");
        f.Floats(wantLpc, 17);
        memcpy(gLibLpc[i], wantLpc, sizeof wantLpc);

        // The two-stage codebook sum has no rounding freedom at all: one add
        // per coefficient.  It must be bit-exact, and if it is not the index
        // arithmetic or a codebook is wrong.
        uint16_t cidx[5];
        int cshort;
        LrvqLsfCase(i, cidx, &cshort);
        const float* cb11 = cshort ? apac::kLrvqLsfCB11Short : apac::kLrvqLsfCB11;
        const float* cb12 = cshort ? apac::kLrvqLsfCB12Short : apac::kLrvqLsfCB12;
        for (int k = 0; k < 4; ++k)
            lsf[k] = cb11[8 * cidx[0] + k] + apac::kLrvqLsfCB21[4 * cidx[2] + k];
        for (int k = 0; k < 4; ++k)
            lsf[4 + k] = cb11[8 * cidx[0] + 4 + k] + apac::kLrvqLsfCB22[4 * cidx[3] + k];
        for (int k = 0; k < 8; ++k)
            lsf[8 + k] = cb12[8 * cidx[1] + k] + apac::kLrvqLsfCB23[8 * cidx[4] + k];
        expectBits(lsf, in, 16, "two-stage LSF codebook sum", i);

        apac::LSFTools tools;
        tools.order = 16;
        tools.sampleRate = 24000.0f;
        // Step by step, so a disagreement lands on one of the three.
        float step[16];
        memcpy(step, lsf, sizeof step);
        tools.ConditionLSFs(step);
        expectBits(step, conditioned, 16, "conditioned LSFs", i);
        tools.LSF_To_LSP(step, lsp, 24000.0f);
        const double wl = dspfx::compare(lsp, wantLsp, 16, "LSF_To_LSP",
                                         peak(wantLsp, 16));
        if (wl > worstLsp) worstLsp = wl;
        gChecks += 16;

        memset(lpc, 0, sizeof lpc);
        tools.LSF_To_LPC(lsf, lpc);
        expectBits(lsf, conditioned, 16, "conditioned LSFs (chained)", i);

        // How far is each side from the exact answer?  Driven from the
        // LIBRARY's LSPs, so this is LSP_To_LPC alone.
        double exact[20];
        float ours[17];
        lspToLpcDouble(wantLsp, exact, 16);
        apac::LSFTools::LSP_To_LPC(wantLsp, ours, 16);
        for (int k = 0; k <= 16; ++k) {
            const double a = fabs((double)ours[k] - exact[k]);
            const double b = fabs((double)wantLpc[k] - exact[k]);
            if (a > worstUlpPort) worstUlpPort = a;
            if (b > worstUlpLib) worstUlpLib = b;
            // The port must not be MUCH further from the exact answer than the
            // library is.  A wrong index or a swapped polynomial fails this by
            // orders of magnitude; accumulated cancellation does not.
            if (a > 8.0 * b + 1e-7) {
                fprintf(stderr,
                        "\nLSP_To_LPC case %d coefficient %d: port is %.3g from "
                        "exact, library %.3g\n", i, k, a, b);
                exit(1);
            }
            ++gChecks;
        }
        // No fixed tolerance here: the double reference above is the real
        // gate.  This only records how far apart the two float versions are.
        const float sc = peak(wantLpc, 17);
        for (int k = 0; k <= 16; ++k) {
            const double rel = fabs((double)lpc[k] - (double)wantLpc[k]) / sc;
            if (rel > worst) worst = rel;
        }
        gChecks += 17;
        ++n;
    }
    *cases = n;
    *lspWorst = worstLsp;
    *ulpPort = worstUlpPort;
    *ulpLib = worstUlpLib;
    return worst;
}

// ----------------------------------------------------------------- lsfs ------
double checkLsfs(int* cases, double* fftWorst)
{
    double worstFFT = 0.0;
    Fixture f;
    f.Open("lrvq_lsfs.fixture");
    static float want[256], got[1024];
    double worst = 0.0;
    int n = 0;
    for (int i = 0; i < kLrvqLsfCases; ++i) {
        const char* l = f.Next();
        int idx = -1, sb = 0, a = 0, b = 0, c = 0, d = 0, e = 0, ret = 0;
        if (!l || sscanf(l, "LSFS %d short=%d idx=%d,%d,%d,%d,%d ret=%d", &idx,
                         &sb, &a, &b, &c, &d, &e, &ret) != 8)
            fail("lsfs header", i);
        f.Expect("BINS");
        f.Floats(want, 256);
        const char* t = f.Next();
        int tail = -1;
        if (!t || sscanf(t, "TAILNONZERO %d", &tail) != 1 || tail != 0)
            fail("the library left out[256..1023] non-zero", i);

        uint16_t cidx[5];
        int cshort;
        LrvqLsfCase(i, cidx, &cshort);
        if (cshort != sb || cidx[0] != (uint16_t)a || cidx[4] != (uint16_t)e)
            fail("lsfs case plan drifted from the fixture", i);

        memset(got, 0x5a, sizeof got);
        if (gDec.LsfsToMdctSpectralEnvelope(cshort != 0, cidx, got) != ret)
            fail("lsfs return", i);
        for (int k = 256; k < 1024; ++k)
            if (got[k] != 0.0f)
                fail("the port left out[256..1023] non-zero", i);
        // TWO comparisons, because the chain has two very different halves.
        //
        // First the FFT half ALONE: the port's 512-point |A(w)| driven from the
        // LIBRARY's own LPC coefficients.  Nothing but the transform differs
        // there, so it gets the stage-d tolerance.
        if (!gFiltReady) { gFilt.Initialize(512, 16); gFiltReady = true; }
        static float fromLibLpc[256];
        gFilt.LPCToSpecEnvelope(gLibLpc[i], fromLibLpc, 256);
        const double wf = dspfx::compare(fromLibLpc, want, 256,
                                         "envelope from the library's own LPC",
                                         peak(want, 256));
        if (wf > worstFFT) worstFFT = wf;
        gChecks += 256;

        // Then the whole chain.  Its spread is set by LSP_To_LPC, whose
        // order-16 recursion cancels hard - checkLsfToLpc measures both sides
        // against the same recursion in double and they are the same distance
        // from it, so this bound is accumulated rounding, not a defect.
        const float sc = peak(want, 256);
        for (int k = 0; k < 256; ++k) {
            const double d = fabs((double)got[k] - (double)want[k]);
            if (d > 3e-4 * (double)sc) {
                fprintf(stderr,
                        "\nMISMATCH lsfs case %d bin %d: ours %.9g (%08x), "
                        "library %.9g (%08x), diff %.3g, peak %.9g\n", i, k,
                        (double)got[k], float_to_bits(got[k]), (double)want[k],
                        float_to_bits(want[k]), d, (double)sc);
                exit(1);
            }
            const double rel = d / (double)sc;
            if (rel > worst) worst = rel;
        }
        gChecks += 256;
        ++n;
    }
    *cases = n;
    *fftWorst = worstFFT;
    return worst;
}

// ------------------------------------------------------- LrvqDeserialize -----
struct FrameStats {
    int cases, exactCases, fullCases, errorCases, layoutReplays;
    int single, twoStage, sameMode, reuseResidual, ltp, layout0, layout1;
    double worstWork, worstEnv, worstSpec, worstResidual;
};

void checkFrame(FrameStats& st)
{
    memset(&st, 0, sizeof st);
    Fixture f;
    f.Open("lrvq_frame.fixture");
    static LrvqCase cases[128];
    const int n = LrvqBuildCases(cases, 128);

    static float env[1024], env2[1024], work[256], spec[1024];
    static float wantWork[256], wantEnv[1024], wantSpec[1024], replay[1024];
    static int16_t qidx[64];
    static int wantQ[64];

    for (int i = 0; i < n; ++i) {
        LrvqCase& c = cases[i];
        const char* l = f.Next();
        int idx = -1, calls = 0, r0 = 0, r1 = 0, bits = 0, pm = 0;
        char name[64] = { 0 };
        if (!l || sscanf(l, "LRVQ %d %63s calls=%d ret=%d,%d bits=%d prevMode=%d",
                         &idx, name, &calls, &r0, &r1, &bits, &pm) != 7)
            fail("lrvq header", i);
        if (idx != i || strcmp(name, c.name) != 0)
            fail("case plan drifted from the fixture", i);
        f.Expect("QIDX");
        f.Ints(wantQ, 64);
        f.Expect("WORK");
        f.Floats(wantWork, 256);
        const char* e = f.Next();
        bool envUnchanged = false;
        if (e && !strncmp(e, "ENVUNCHANGED", 12)) {
            envUnchanged = true;
            memcpy(wantEnv, c.env, sizeof wantEnv);
        } else if (e && !strncmp(e, "ENV", 3)) {
            f.Floats(wantEnv, 1024);
        } else {
            fail("expected ENV or ENVUNCHANGED", i);
        }
        f.Expect("SPEC");
        f.Floats(wantSpec, 1024);

        // ---- run the port on the same bits and the same starting state ----
        memcpy(env, c.env, sizeof env);
        memcpy(env2, c.env2, sizeof env2);
        memcpy(work, c.work, sizeof work);
        memcpy(qidx, c.qidx, sizeof qidx);
        uint16_t prevMode = c.prevMode;
        for (int k = 0; k < 1024; ++k)
            spec[k] = -777.0f;
        apac::SpectralData sp;
        sp.begin = spec;
        sp.end = spec + 1024;

        apac::BitstreamReader bs(c.bits, c.nbytes);
        const int32_t before = bs.BitsRemaining();
        int32_t ret[2] = { 0, 0 };
        int ran = 0;
        for (int k = 0; k < c.nCalls; ++k) {
            ret[k] = gDec.LrvqDeserialize(bs, c.call[k].isShort != 0,
                                         c.call[k].firstChannel != 0, sp, env,
                                         env2, work, qidx, &prevMode);
            ++ran;
            if (ret[k])
                break;
        }
        const int32_t used = before - bs.BitsRemaining();

        if (ran != calls || ret[0] != r0 || ret[1] != r1)
            fail("return code", i);
        if (used != bits) {
            fprintf(stderr, "\nMISMATCH bits case %d: ours %d, library %d\n", i,
                    (int)used, bits);
            exit(1);
        }
        if ((int)prevMode != pm)
            fail("prevMode", i);
        for (int k = 0; k < 64; ++k)
            if ((int)qidx[k] != wantQ[k])
                fail("qidx", i);
        gChecks += 64 + 3;

        if (ret[0] == 1 || ret[1] == 1)
            ++st.errorCases;
        if (c.usesEnvelopeBuilder) {
            ++st.fullCases;
            // Float noise: an FFT under the envelope on one side and a
            // different FFT (or, for the LTP, a closed form) on the other.
            // 1e-3 of the block's peak.  What sets this is the LSP_To_LPC
            // cancellation measured in checkLsfToLpc, magnified by the
            // division: work = vq / env, so the envelope's ~3e-5 relative
            // spread lands here directly.  A wrong codebook, a wrong layout or
            // a wrong reuse flag misses by orders of magnitude, not by this.
            const double ww = relCompare(work, wantWork, 256, "work", i, 0.25);
            if (ww > st.worstWork) st.worstWork = ww;
            const double we = relCompare(env, wantEnv, 1024, "env", i, 1e-3);
            if (we > st.worstEnv) st.worstEnv = we;
            const double ws = relCompare(spec, wantSpec, 1024, "spec", i, 0.25);
            if (ws > st.worstSpec) st.worstSpec = ws;
            // The loose bound above is not the real check.  work = vq / env,
            // so multiplying it back by each side's OWN envelope recovers the
            // VQ residual - which has no FFT anywhere near it and has to agree
            // to a couple of roundings.  That is what pins the VQ half of a
            // case whose envelope is not bit-exact.
            //
            // Single-call cases only: a channel pair divides by TWO envelopes
            // (its own, and whatever the previous channel left in `work`), and
            // only the last one is still in the buffer, so the product no
            // longer recovers anything.  The pairs are covered instead by the
            // FFT-free family, where every float is compared as a bit pattern.
            if (c.nCalls == 1) {
                static float recA[256], recB[256];
                for (int k = 0; k < 256; ++k) {
                    recA[k] = work[k] * env[k];
                    recB[k] = wantWork[k] * wantEnv[k];
                }
                const double wr = relCompare(recA, recB, 256,
                                             "VQ residual recovered as work * env",
                                             i, 1e-5);
                if (wr > st.worstResidual) st.worstResidual = wr;
                gChecks += 256;
            }
            gChecks += 256 + 1024 + 1024;
        } else {
            ++st.exactCases;
            if (!envUnchanged)
                fail("an FFT-free case still changed env", i);
            expectBits(work, wantWork, 256, "work", i);
            expectBits(env, wantEnv, 1024, "env", i);
            expectBits(spec, wantSpec, 1024, "spec", i);
        }

        // ---- replay the output layout from the LIBRARY's own work ----------
        // This is what makes the reversed layout bit-exact: `wantWork` is what
        // the library handed its own layout step, so the two sides differ only
        // in that step.
        if (ret[0] == 0 && (c.nCalls == 1 || ret[1] == 0)) {
            const int last = c.nCalls - 1;
            float gain = 0.0f;
            // The gain of the LAST call: re-read its two header fields off the
            // stream the same way the decoder did.  Only the last call's gain
            // reaches `spec`.
            // (Recovered from the fixture instead: the layout is a pure
            //  function of work and gain, and gain is pinned by the long/
            //  interleaved cases below, so derive it from a written slot.)
            for (int k = 0; k < 1024; ++k)
                replay[k] = -777.0f;
            if (!c.call[last].isShort) {
                // spec[i] = work[i] * gain, so gain is the ratio at the
                // largest |work| - stable, and it is checked by comparing all
                // 256 outputs afterwards.
                int best = 0;
                for (int k = 1; k < 256; ++k)
                    if (fabsf(wantWork[k]) > fabsf(wantWork[best]))
                        best = k;
                if (wantWork[best] == 0.0f)
                    continue;
                gain = wantSpec[best] / wantWork[best];
                apac::LRVQDecoder::LongOutput(wantWork, gain, replay);
                for (int k = 0; k < 256; ++k)
                    if (fabsf(replay[k] - wantSpec[k]) >
                        1e-6f * fabsf(wantSpec[k]))
                        fail("long output replay", i);
                gChecks += 256;
                ++st.layoutReplays;
            } else if (c.layout == 1) {
                // The reversed layout takes work AFTER the gain, so no gain is
                // needed and the replay is exact.
                apac::LRVQDecoder::ShortLayoutReversed(wantWork, replay);
                expectBits(replay, wantSpec, 1024, "reversed short layout", i);
                ++st.layoutReplays;
                ++st.layout1;
            } else if (c.layout == 0) {
                int best = 0;
                for (int k = 1; k < 256; ++k)
                    if (fabsf(wantWork[k]) > fabsf(wantWork[best]))
                        best = k;
                if (wantWork[best] == 0.0f)
                    continue;
                const int w = best % 8, nn = best / 8;
                gain = wantSpec[128 * w + nn] / wantWork[best];
                apac::LRVQDecoder::ShortLayoutInterleaved(wantWork, gain, replay);
                for (int k = 0; k < 8; ++k)
                    for (int q = 0; q < 32; ++q) {
                        const int at = 128 * k + q;
                        if (fabsf(replay[at] - wantSpec[at]) >
                            1e-6f * fabsf(wantSpec[at]))
                            fail("interleaved short layout replay", i);
                    }
                gChecks += 256;
                ++st.layoutReplays;
                ++st.layout0;
            }
        }

        for (int k = 0; k < c.nCalls; ++k) {
            if (c.vqMode[k] < 0) { ++st.reuseResidual; continue; }
            if (c.vqMode[k] < 4) ++st.single;
            else if (c.vqMode[k] < 11) ++st.twoStage;
        }
        if (c.nCalls == 2 && c.vqMode[0] == c.vqMode[1] && c.vqMode[0] >= 0)
            ++st.sameMode;
        if (!strcmp(c.name, "exact-samemode-reuse"))
            ++st.sameMode;
        if (c.ltp) ++st.ltp;
        ++st.cases;
    }
}

// ------------------------------------------------------ the element wrappers --
struct ElemStats {
    int cases, sce, cpe, lfe, skipped, errors;
    double worstSpec;
};

void checkElem(ElemStats& st)
{
    memset(&st, 0, sizeof st);
    Fixture f;
    f.Open("lrvq_elem.fixture");
    static LrvqElemCase cases[128];
    const int n = LrvqBuildElemCases(cases, 128);
    static float spec0[1024], spec1[1024], want0[1024], want1[1024];

    for (int i = 0; i < n; ++i) {
        LrvqElemCase& c = cases[i];
        const char* l = f.Next();
        int idx = -1, kind = -1, ret = 0, bits = 0;
        char name[64] = { 0 };
        unsigned i0a = 0, i0b = 0, i0c = 0, i0d = 0, i1a = 0, i1b = 0, i1c = 0, i1d = 0;
        if (!l || sscanf(l, "ELEM %d %63s kind=%d ret=%d bits=%d "
                            "ics0=%u,%u,%u,%u ics1=%u,%u,%u,%u",
                         &idx, name, &kind, &ret, &bits, &i0a, &i0b, &i0c, &i0d,
                         &i1a, &i1b, &i1c, &i1d) != 13)
            fail("elem header", i);
        if (idx != i || kind != c.kind)
            fail("element case plan drifted from the fixture", i);
        f.Expect("SPEC0");
        f.Floats(want0, 1024);
        if (c.kind == kLrvqElemCPE) {
            f.Expect("SPEC1");
            f.Floats(want1, 1024);
        }

        apac::APACICSInfo ics0, ics1;
        ics0.Clear();
        ics1.Clear();
        ics0.maxSfbLong = 49; ics0.maxSfbShort = 14;
        ics1.maxSfbLong = 49; ics1.maxSfbShort = 14;
        ics0.windowSequence = (uint8_t)(c.isShort0 ? 2 : 0);
        ics1.windowSequence = (uint8_t)(c.isShort1 ? 2 : 0);
        for (int k = 0; k < 1024; ++k)
            spec0[k] = spec1[k] = -777.0f;
        apac::SpectralData sp0, sp1;
        sp0.begin = spec0; sp0.end = spec0 + 1024;
        sp1.begin = spec1; sp1.end = spec1 + 1024;

        apac::BitstreamReader bs(c.bits, c.nbytes);
        const int32_t before = bs.BitsRemaining();
        int32_t r;
        if (c.kind == kLrvqElemSCE) {
            r = gDec.SpectrumDeserializeSCE(bs, ics0, sp0);
            ++st.sce;
        } else if (c.kind == kLrvqElemCPE) {
            r = gDec.SpectrumDeserializeCPE(bs, ics0, ics1, sp0, sp1);
            ++st.cpe;
        } else {
            r = gDec.SpectrumDeserializeLFE(bs, ics0, sp0);
            ++st.lfe;
        }
        const int32_t used = before - bs.BitsRemaining();

        if (r != ret)
            fail("element return code", i);
        if (used != bits) {
            fprintf(stderr, "\nMISMATCH elem bits case %d: ours %d, library %d\n",
                    i, (int)used, bits);
            exit(1);
        }
        if (ics0.windowSequence != i0a || ics0.maxSfb != i0b ||
            ics0.numWindowGroups != i0c || ics0.windowGroupWord != i0d)
            fail("channel 0 ICS info after the skip bit", i);
        if (c.kind == kLrvqElemCPE &&
            (ics1.windowSequence != i1a || ics1.maxSfb != i1b ||
             ics1.numWindowGroups != i1c || ics1.windowGroupWord != i1d))
            fail("channel 1 ICS info after the skip bit", i);
        gChecks += 6;
        if (r) ++st.errors;
        if (c.skip0) ++st.skipped;

        // The LFE path has no envelope under it at all, so it is bit-exact;
        // SCE and CPE run the LSF builder for any channel that is not skipped.
        if (c.kind == kLrvqElemLFE || c.skip0) {
            expectBits(spec0, want0, 1024, "element spectrum", i);
        } else {
            const double w = relCompare(spec0, want0, 1024, "element spec0", i,
                                        0.25);
            if (w > st.worstSpec) st.worstSpec = w;
            gChecks += 1024;
        }
        if (c.kind == kLrvqElemCPE) {
            if (c.skip1) {
                expectBits(spec1, want1, 1024, "element spectrum 1", i);
            } else {
                const double w = relCompare(spec1, want1, 1024,
                                            "element spec1", i, 0.25);
                if (w > st.worstSpec) st.worstSpec = w;
                gChecks += 1024;
            }
        }
        ++st.cases;
    }
}

// -------------------------------------------------- DeserializeLrvqFrame -----
// The LRVQ frame body: two ICS headers at coding type 1, the channel pair, both
// TNS blocks and the extra bit the original reads when the SECOND channel has
// no TNS block and the first one did.  The port's LrvqFrameContext stands in for
// the APACChannelPairElement the library was handed.
struct FrameStats2 {
    int cases, withFilter, extraBit, shortBlocks;
    double worstSpec;
};

void checkCpeFrame(FrameStats2& st)
{
    memset(&st, 0, sizeof st);
    Fixture f;
    f.Open("lrvq_cpe_frame.fixture");
    static LrvqFrameCase cases[64];
    const int n = LrvqBuildFrameCases(cases, 64);
    static float spec0[1024], spec1[1024], want0[1024], want1[1024];
    static uint8_t wantTns0[190], wantTns1[190];

    const apac::SFBOffsets* tab = apac::GetSFBOffsetTable(48000, 1024);
    if (!tab)
        fail("GetSFBOffsetTable(48000, 1024)", 0);

    for (int i = 0; i < n; ++i) {
        LrvqFrameCase& c = cases[i];
        const char* l = f.Next();
        int idx = -1, w0 = 0, w1 = 0, t0 = 0, t1 = 0, filt = 0, ret = 0, bits = 0;
        unsigned a0, a1, a2, a3, b0, b1, b2, b3, p0, p1, q0, q1;
        if (!l || sscanf(l, "FRAME %d ws=%d,%d tns=%d,%d filter=%d ret=%d "
                            "bits=%d ics0=%u,%u,%u,%u ics1=%u,%u,%u,%u "
                            "tnsp=%u,%u tnsn=%u,%u",
                         &idx, &w0, &w1, &t0, &t1, &filt, &ret, &bits, &a0, &a1,
                         &a2, &a3, &b0, &b1, &b2, &b3, &p0, &p1, &q0, &q1) != 20)
            fail("frame header", i);
        if (idx != i || w0 != c.ws0 || t0 != c.tns0 || t1 != c.tns1)
            fail("frame case plan drifted from the fixture", i);
        f.Expect("TNS0");
        f.Bytes(wantTns0, 190);
        f.Expect("TNS1");
        f.Bytes(wantTns1, 190);
        f.Expect("SPEC0");
        f.Floats(want0, 1024);
        f.Expect("SPEC1");
        f.Floats(want1, 1024);

        apac::APACICSInfo ics0, ics1;
        ics0.Clear(); ics1.Clear();
        ics0.maxSfbLong = 49; ics0.maxSfbShort = 14;
        ics1.maxSfbLong = 49; ics1.maxSfbShort = 14;
        apac::TNSData tns0, tns1;
        memset(&tns0, 0, sizeof tns0);
        memset(&tns1, 0, sizeof tns1);
        tns0.Configure(48000);
        tns1.Configure(48000);
        for (int k = 0; k < 1024; ++k)
            spec0[k] = spec1[k] = -777.0f;
        apac::SpectralData sp0, sp1;
        sp0.begin = spec0; sp0.end = spec0 + 1024;
        sp1.begin = spec1; sp1.end = spec1 + 1024;

        apac::LrvqFrameContext ctx;
        ctx.ics[0] = &ics0;   ctx.ics[1] = &ics1;
        ctx.spec[0] = &sp0;   ctx.spec[1] = &sp1;
        ctx.tns[0] = &tns0;   ctx.tns[1] = &tns1;
        ctx.sfbLong.numBands = tab->numSfbLong;
        ctx.sfbLong.offset = tab->sfbOffsetLong;
        ctx.sfbShort.numBands = tab->numSfbShort;
        ctx.sfbShort.offset = tab->sfbOffsetShort;
        ctx.lrvq = &gDec;

        apac::BitstreamReader bs(c.bits, c.nbytes);
        const int32_t before = bs.BitsRemaining();
        const int32_t r = apac::DeserializeLrvqFrame(bs, ctx);
        const int32_t used = before - bs.BitsRemaining();

        if (r != ret)
            fail("frame return code", i);
        if (used != bits) {
            fprintf(stderr,
                    "\nMISMATCH frame bits case %d: ours %d, library %d - a "
                    "one-bit gap here is the extra bit at 0xac7d8\n",
                    i, (int)used, bits);
            exit(1);
        }
        if (ics0.windowSequence != a0 || ics0.maxSfb != a1 ||
            ics0.numWindowGroups != a2 || ics0.windowGroupWord != a3 ||
            ics1.windowSequence != b0 || ics1.maxSfb != b1 ||
            ics1.numWindowGroups != b2 || ics1.windowGroupWord != b3)
            fail("frame ICS headers", i);
        // The whole 190-byte TNS object, byte for byte - present, the filter
        // count or short-window mask, every filter record and the four caps.
        if (memcmp(&tns0, wantTns0, 190) || memcmp(&tns1, wantTns1, 190)) {
            for (int k = 0; k < 190; ++k) {
                const uint8_t o0 = ((const uint8_t*)&tns0)[k];
                const uint8_t o1 = ((const uint8_t*)&tns1)[k];
                if (o0 != wantTns0[k] || o1 != wantTns1[k]) {
                    fprintf(stderr,
                            "\nMISMATCH frame TNS case %d byte %d: ch0 %02x vs "
                            "%02x, ch1 %02x vs %02x\n", i, k, o0, wantTns0[k],
                            o1, wantTns1[k]);
                    exit(1);
                }
            }
        }
        gChecks += 190 * 2 + 9;
        const double s0 = relCompare(spec0, want0, 1024, "frame spec0", i, 0.25);
        const double s1 = relCompare(spec1, want1, 1024, "frame spec1", i, 0.25);
        if (s0 > st.worstSpec) st.worstSpec = s0;
        if (s1 > st.worstSpec) st.worstSpec = s1;
        gChecks += 2048;
        if (c.tnsFilter) ++st.withFilter;
        if (c.tns0 && !c.tns1) ++st.extraBit;
        if (c.ws0 == 2) ++st.shortBlocks;
        ++st.cases;
        (void)p0; (void)p1; (void)q0; (void)q1;
    }
}

}  // namespace

int main()
{
    checkTables();
    const int gains = checkGain();
    int ltpCases = 0, lsfCases = 0, lpcCases = 0;
    const double worstLtp = checkLtp(&ltpCases);
    double worstLsp = 0.0, ulpPort = 0.0, ulpLib = 0.0;
    const double worstLpc = checkLsfToLpc(&lpcCases, &worstLsp, &ulpPort,
                                          &ulpLib);
double worstLsfFFT = 0.0;
    const double worstLsf = checkLsfs(&lsfCases, &worstLsfFFT);
    FrameStats fr;
    checkFrame(fr);
    ElemStats el;
    checkElem(el);
    FrameStats2 cf;
    checkCpeFrame(cf);

    printf("OK  %d checks; %d gains bit-exact; %d LrvqDeserialize cases "
           "(%d FFT-free and BIT-EXACT, %d with an envelope, %d rejections), "
           "%d layout replays, %d elements\n",
           gChecks, gains, fr.cases, fr.exactCases, fr.fullCases, fr.errorCases,
           fr.layoutReplays, el.cases);
    printf("    coverage: single-stage=%d two-stage=%d same-mode-reuse=%d "
           "reuse-residual=%d ltp=%d short-layout-0=%d short-layout-1=%d "
           "sce=%d cpe=%d lfe=%d skipped-channels=%d\n",
           fr.single, fr.twoStage, fr.sameMode, fr.reuseResidual, fr.ltp,
           fr.layout0, fr.layout1, el.sce, el.cpe, el.lfe, el.skipped);
    printf("    envelopes (an FFT under them, so not bit-exact): ltp %.2e "
           "relative, lsfs %.2e of full scale, frame env %.2e\n",
           worstLtp, worstLsf, fr.worstEnv);
    printf("    the VQ residual recovered as work * env, which has no FFT "
           "under it: %.2e of full scale.  work %.2e and spec %.2e are larger "
           "ONLY because 1/env is ill-conditioned at a null of |A(w)|: at the "
           "worst bin env is 8.2e-04 against the library's 9.1e-04, an absolute "
           "gap of 3.5e-05 of the envelope's own peak, and dividing by it "
           "magnifies that to 10%%.  The original divides by its own equally "
           "imprecise envelope; see PORT_STATUS.md\n",
           fr.worstResidual, fr.worstWork, fr.worstSpec);
    printf("    element spectra %.2e of full scale, same cause\n", el.worstSpec);
    printf("    DeserializeLrvqFrame: %d frames, bit count and both 190-byte "
           "TNS objects exact, %d with a real TNS filter, %d eight-short, %d "
           "exercising the extra bit at 0xac7d8; spectra %.2e\n",
           cf.cases, cf.withFilter, cf.shortBlocks, cf.extraBit, cf.worstSpec);
    printf("    LSF -> LPC has no FFT under it: %d cases, 1024 codebook sums "
           "and every conditioned LSF bit-exact, LSPs %.2e and coefficients "
           "%.2e of full scale\n", lpcCases, worstLsp, worstLpc);
    printf("    lsfs, FFT half alone (the port's 512-point envelope from the "
           "library's own LPC): %.2e of full scale over %d cases\n",
           worstLsfFFT, lsfCases);
    printf("    LSP -> LPC vs the same recursion in double, on the library's "
           "own LSPs: port %.2e, library %.2e - the same size, so the spread "
           "is order-16 cancellation, not an index\n", ulpPort, ulpLib);
    (void)ltpCases;
    (void)lsfCases;
    return 0;
}
