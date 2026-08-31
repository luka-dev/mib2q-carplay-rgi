// Stage e - APACBWE2Decoder's synthesis and BWELPC - against the real library.
//
// Fixtures: tests/fixtures/bwe_{lsf,env,lpc,frame}.fixture, produced by
// reference/apac_bwe_ref.cpp calling Pioneer's copy of Apple's decoder.  The
// inputs are regenerated here from reference/bwe_cases.h, which the oracle
// includes too, so the fixtures carry only the library's answers.
//
// BWE IS NEVER ACTIVE ON THE CARPLAY CAPTURE - patching GetActiveFlag or
// DecodeFrame to return 0 leaves all 3977 packets bit-identical - so every case
// here is built for the purpose and driven straight at the library, exactly the
// way stage h1 had to drive LRVQ.
//
// WHAT IS BIT-EXACT AND WHAT IS NOT.
//
// Bit-exact, and asserted as bit patterns:
//
//   * the two 512 x 16 LSF codebooks and the 64-entry gain table;
//   * the conditioned LSF vector - the codebook sum and Condition_LSFs have no
//     FFT under them and nothing else in between;
//   * the geometry.  Every coefficient the synthesis does NOT write - the whole
//     core below bwStart, everything above bwStart + 512, and for a short block
//     every bin of every window outside its own 64-wide extension - must come
//     back from the library byte-identical to what went in.  A wrong bwStart, a
//     wrong patch width, a wrong window stride or an off-by-one in the group
//     walk all fail there rather than inside a tolerance;
//   * the return code and the two early exits (an inactive channel, maxSfb 0).
//
// Not bit-exact, and it cannot be: apac::LPMDCTFilter ends in a real FFT, and
// the library's is Apple's closed vDSP_DFT_Execute.  Same reason as stage d and
// the LRVQ envelopes, and the same machinery (tests/dsp_fixture.h).  Two
// tolerances are reported separately so a regression cannot hide in the looser
// one: the FFT alone, measured by handing BOTH sides the same LPC vector, and
// the whole chain.
//
// The chain is looser than the FFT by a lot, and the reason is structural
// rather than a defect: the synthesis DIVIDES the core envelope by the
// extension envelope, so wherever |A(w)| has a null the reconstruction is
// ill-conditioned by construction - in the original exactly as much as here.
// Stage h1 hit the same thing on the LRVQ residual and wrote it up the same way.
//
// SO IS THE TOLERANCE HIDING A BUG?  Fifteen deliberate mutations of the port
// say no; ALL FIFTEEN fail this test:
//
//   the long divisor slice moved up by one patch; the long patch source off by
//   one bin; the long gain read from window group 1; `coreEnd >= 384` weakened
//   to `>`; the narrow bwStart changed from 256 to 128; the long extension
//   envelope built over 256 bins instead of 512; the short patch source pinned
//   to window 0 instead of following the window; the short group walk off by
//   one; the short LPC analysis given 1 block instead of 8; the short extension
//   envelope 32 bins instead of 64; the two LSF codebooks swapped; the 1.0001
//   ridge on lag 0 dropped; the filter selector answering 512 where it should
//   answer 768; the radix-3 butterfly's sqrt(3)/2 sign flipped; and the
//   radix-3 gather taking the three sub-sequences in the wrong order.
//
// Rerun them with the mut() loop recorded in the stage-e section of
// PORT_STATUS.md.  `make clean` first: the last of those runs failed as a
// plausible numeric mismatch in an UNMUTATED build, because the objects had
// gone out of step - which is the trap the Makefile note already warns about.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apac/bwe.h"
#include "apac/bwe_lpc.h"
#include "apac/element.h"
#include "apac/config.h"
#include "apac/sfb_offsets.h"

#include "dsp_fixture.h"

using dspfx::bits_to_float;
using dspfx::float_to_bits;
using dspfx::parse_hex_floats;
using dspfx::peak;

// reference/bwe_cases.h asks the includer for this; here it is the PORT's own
// table, so nothing about the case plan is shared with the library except the
// numbers it computes from it.
static uint32_t BweCoreEdge(uint32_t rate, uint32_t seq, uint32_t maxSfb);

#include "../reference/bwe_cases.h"

namespace {

int gChecks = 0;
int gBitChecks = 0;

// File scope, not function-local statics: a function-local static with a
// constructor emits __cxa_guard_acquire / __cxa_guard_release, which the QNX
// 6.5 armle-v7 link in tools/build_qnx.sh does not have.  These are also far
// too big for a stack - BWELPC carries two 47 KiB apac::LPMDCTFilters.
apac::BWELPC      gLPC;
apac::BWELPC      gLPCFresh;
apac::BWE2Decoder gBWE;
apac::SyntacticElement gElem;
BweLsfCase   gLsfCases[kBweLsfCases];
BweFrameCase gFrameCases[kBweFrameCases];
float gSpec[kBweFrameLength];
float gRef[kBweFrameLength];
float gEnv[2048];
float gEnvRef[2048];
// For the conditioning measurement below; both are far too big for a stack.
apac::LPMDCTFilter gAux;
apac::LPCBase      gBase;

const char* gFixtureDir = "tests/fixtures";

void fail(const char* what, int idx)
{
    fprintf(stderr, "\nFAIL %s at case %d\n", what, idx);
    exit(1);
}

struct Fixture {
    FILE* fh;
    char  line[8192];

    void Open(const char* name)
    {
        char path[512];
        snprintf(path, sizeof path, "%s/%s", gFixtureDir, name);
        fh = fopen(path, "r");
        if (!fh) {
            fprintf(stderr, "\ncannot open %s\n", path);
            exit(1);
        }
    }
    const char* Next()
    {
        while (fgets(line, sizeof line, fh)) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
                continue;
            return line;
        }
        return 0;
    }
    const char* Expect(const char* tag)
    {
        const char* l = Next();
        if (!l || strncmp(l, tag, strlen(tag)) != 0) {
            fprintf(stderr, "\nfixture: expected %s, got %s", tag,
                    l ? l : "<eof>\n");
            exit(1);
        }
        return l;
    }
    void Floats(const char* tag, float* out, uint32_t n)
    {
        Expect(tag);
        uint32_t got = 0;
        while (got < n) {
            const char* l = Next();
            if (!l) { fprintf(stderr, "\nfixture: short float block\n"); exit(1); }
            got += parse_hex_floats(l, out, n, got);
        }
    }
    void Close() { fclose(fh); }
};

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
        ++gBitChecks;
    }
}

// |ours - theirs| against a tolerance in units of the block's own peak.
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
        ++gChecks;
    }
    return worst;
}

// ------------------------------------------------------------- the tables ----
//
// The gain table is a 6-bit index and the two codebooks are 9-bit ones, so the
// whole of each is reachable from a legal bitstream and there is no
// out-of-range read to argue about the way stage h1 had to.  What IS worth
// asserting is that they are what the generator says they are, that neither has
// been truncated, and that the first codebook really is the monotonically
// rising one - which is what makes Condition_LSFs' floor pass a no-op on most
// entries and its ceiling pass reachable at all.
void checkTables()
{
    if (apac::kBWELsfCB0[0] <= 0.0f || apac::kBWELsfCB0[0] >= 24000.0f)
        fail("codebook 0 is not in Hz", 0);
    int rising = 0;
    for (uint32_t e = 0; e < apac::kBWELsfBookSize; ++e) {
        const float* v = apac::kBWELsfCB0 + e * apac::kBWELPCOrder;
        int ok = 1;
        for (uint32_t i = 1; i < apac::kBWELPCOrder; ++i)
            if (v[i] <= v[i - 1])
                ok = 0;
        rising += ok;
        ++gChecks;
    }
    if (rising != (int)apac::kBWELsfBookSize)
        fail("codebook 0 is not monotonically rising in every entry", rising);
    // The gain table rises and spans five decades; both ends are pinned.
    for (uint32_t i = 1; i < apac::kBWEGainSize; ++i) {
        if (!(apac::kBWEGain[i] > apac::kBWEGain[i - 1]))
            fail("the gain table is not rising", (int)i);
        ++gChecks;
    }
    if (float_to_bits(apac::kBWEGain[0]) != 0x38fba882u ||
        float_to_bits(apac::kBWEGain[apac::kBWEGainSize - 1]) != 0x4324bd42u)
        fail("the gain table's ends moved", 0);
    gBitChecks += 2;

    // BWELPC::kDftSizes at 0x00331a58 and the switch that indexes it.
    static const uint32_t kN[8]   = { 32, 48, 64, 256, 384, 512, 128, 96 };
    static const uint32_t kFFT[8] = { 64, 96, 128, 512, 768, 1024, 1024, 1024 };
    for (int i = 0; i < 8; ++i) {
        if (apac::BWELPC::FFTSizeFor(kN[i]) != kFFT[i])
            fail("the filter selector", i);
        ++gChecks;
    }
}

// ---------------------------------------------------------------- stage 1 ----
// BWELPC::DecodeLSFsToLPCs.
void checkLsf(double* worstLsp, double* worstLpc)
{
    Fixture f;
    f.Open("bwe_lsf.fixture");
    const char* hdr = f.Expect("lsf cases");
    int n = 0, order = 0;
    if (sscanf(hdr, "lsf cases %d order %d", &n, &order) != 2 ||
        n != kBweLsfCases || order != kBweLPCOrder)
        fail("bwe_lsf.fixture header", 0);

    BweBuildLsfCases(gLsfCases);
    float lsf[kBweLPCOrder], lsp[kBweLPCOrder], lpc[kBweLPCOrder + 1];
    *worstLsp = 0.0;
    *worstLpc = 0.0;
    for (int i = 0; i < kBweLsfCases; ++i) {
        char tag[64];
        snprintf(tag, sizeof tag, "case %d idx %u %u", i, gLsfCases[i].idx[0],
                 gLsfCases[i].idx[1]);
        f.Expect(tag);
        f.Floats("lsf", lsf, kBweLPCOrder);
        f.Floats("lsp", lsp, kBweLPCOrder);
        f.Floats("lpc", lpc, kBweLPCOrder + 1);

        if (!gLPC.DecodeLSFsToLPCs(gLsfCases[i].idx))
            fail("DecodeLSFsToLPCs refused a legal index pair", i);
        // The codebook sum and Condition_LSFs: no FFT, no trig, bit-exact.
        expectBits(gLPC.LSF(), lsf, kBweLPCOrder, "lsf", i);
        double w = relCompare(gLPC.LSP(), lsp, kBweLPCOrder, "lsp", i, 1e-6);
        if (w > *worstLsp) *worstLsp = w;
        w = relCompare(gLPC.LsfLPC(), lpc, kBweLPCOrder + 1, "lpc", i, 4e-4);
        if (w > *worstLpc) *worstLpc = w;
    }
    // Out of range in both fields and in each on its own.
    uint16_t bad[2];
    bad[0] = 512; bad[1] = 0;
    if (gLPC.DecodeLSFsToLPCs(bad)) fail("index 512 in field 0 was accepted", 0);
    bad[0] = 0; bad[1] = 512;
    if (gLPC.DecodeLSFsToLPCs(bad)) fail("index 512 in field 1 was accepted", 0);
    bad[0] = 0xffff; bad[1] = 0xffff;
    if (gLPC.DecodeLSFsToLPCs(bad)) fail("index 65535 was accepted", 0);
    gChecks += 3;
    f.Close();
}

// ---------------------------------------------------------------- stage 2 ----
// BWELPC::LPCToMDCTSpectrum.  BOTH sides get the same LPC vector, so the only
// difference here is the FFT - this is the number that says how much the
// radix-2 (and, at 96 and 768 points, radix-3) transform costs.
double gWorstPerSize[kBweEnvSizes];

void checkEnv(double* worst, int* sizes)
{
    Fixture f;
    f.Open("bwe_env.fixture");
    const char* hdr = f.Expect("env vectors");
    int nv = 0, ns = 0;
    if (sscanf(hdr, "env vectors %d sizes %d", &nv, &ns) != 2 ||
        nv != kBweEnvVectors || ns != kBweEnvSizes)
        fail("bwe_env.fixture header", 0);
    *worst = 0.0;
    *sizes = 0;
    for (int v = 0; v < kBweEnvVectors; ++v) {
        float lpc[kBweLPCOrder + 1];
        BweBuildLPC(lpc, (uint32_t)v);
        for (int s = 0; s < kBweEnvSizes; ++s) {
            const uint32_t n = kBweEnvN[s];
            char tag[64];
            snprintf(tag, sizeof tag, "case %d %d n %u", v, s, n);
            f.Expect(tag);
            f.Floats("env", gEnvRef, n);
            memset(gEnv, 0, sizeof gEnv);
            gLPC.LPCToMDCTSpectrum(lpc, gEnv, n);
            // The two lengths the selector does NOT name run at 1024 points
            // and read only the first n bins of it, i.e. the low-frequency end
            // where |A(w)| cancels hardest; they get their own bound.
            const double tol = (n == kBweEnvN[6] || n == kBweEnvN[7]) ? 2e-5 : 1e-6;
            const double w = relCompare(gEnv, gEnvRef, n, "env",
                                        v * kBweEnvSizes + s, tol);
            if (w > *worst) *worst = w;
            if (w > gWorstPerSize[s]) gWorstPerSize[s] = w;
            ++*sizes;

            // The same call on a filter that has never held any other size, to
            // pin the port's two-filter-with-lazy-Initialize substitution for
            // the original's six persistent objects.  gLPC has just been asked
            // for every other size in the sweep; gLPCFresh is re-Initialized.
            gLPCFresh.Initialize();
            static float fresh[2048];
            memset(fresh, 0, sizeof fresh);
            gLPCFresh.LPCToMDCTSpectrum(lpc, fresh, n);
            expectBits(fresh, gEnv, n, "filter reuse", v * kBweEnvSizes + s);
        }
    }
    f.Close();
}

// How far the LPC vector moves when its own autocorrelation is perturbed by
// one part in `eps`.  Levinson-Durbin at order 16 amplifies hard, so this is
// what says whether a difference between the port and the library is the FFT's
// 2e-07 arriving at the output or something structural: the two are compared
// against 8x THIS, not against a number picked to pass.  The same test stage h1
// applies to LSP_To_LPC.
double lpcSensitivity(const float* spec, uint32_t hop, uint32_t len,
                      uint32_t blocks, float eps)
{
    float r[apac::kBWELPCOrder + 2];
    float rp[apac::kBWELPCOrder + 2];
    float a1[apac::kBWELPCOrder + 1];
    float a2[apac::kBWELPCOrder + 1];
    gAux.Initialize((uint16_t)apac::BWELPC::FFTSizeFor(len),
                    (uint16_t)apac::kBWELPCOrder);
    gAux.MDCTToAutoCorr(spec, r, hop, blocks, len);
    r[0] = (r[0] == 0.0f) ? 1.0001f : r[0] * 1.0001f;
    for (uint32_t i = 0; i <= apac::kBWELPCOrder; ++i)
        rp[i] = r[i] * (1.0f + ((i & 1) ? eps : -eps));
    gBase.Configure(apac::kBWELPCOrder, a1, 0);
    gBase.LevinsonDurbin(r);
    gBase.Configure(apac::kBWELPCOrder, a2, 0);
    gBase.LevinsonDurbin(rp);
    const double sc = (double)peak(a1, apac::kBWELPCOrder + 1);
    double worst = 0.0;
    for (uint32_t i = 0; i <= apac::kBWELPCOrder; ++i) {
        const double d = fabs((double)a1[i] - (double)a2[i]);
        if (sc > 0.0 && d / sc > worst)
            worst = d / sc;
    }
    return worst;
}

// ---------------------------------------------------------------- stage 3 ----
// BWELPC::MDCTCoefficientsToLPC, on the frame cases' own spectra.
void checkLpc(double* worst, double* worstRatio)
{
    Fixture f;
    f.Open("bwe_lpc.fixture");
    const char* hdr = f.Expect("lpc cases");
    int n = 0;
    if (sscanf(hdr, "lpc cases %d", &n) != 1 || n != kBweFrameCases)
        fail("bwe_lpc.fixture header", 0);
    float lpc[kBweLPCOrder + 1];
    *worst = 0.0;
    *worstRatio = 0.0;
    for (int i = 0; i < kBweFrameCases; ++i) {
        const BweFrameCase& c = gFrameCases[i];
        const uint32_t edge = BweCoreEdge(c.sampleRate, c.windowSequence,
                                          c.maxSfb);
        const uint32_t bwStart = (edge >= 384) ? 384u : 256u;
        char tag[64];
        snprintf(tag, sizeof tag, "case %d seq %u bwStart %u", i,
                 c.windowSequence, bwStart);
        f.Expect(tag);
        f.Floats("lpc", lpc, kBweLPCOrder + 1);
        const uint32_t hop = (c.windowSequence == 2) ? 128u : 1024u;
        const uint32_t len = (c.windowSequence == 2) ? (bwStart >> 3) : bwStart;
        const uint32_t blocks = (c.windowSequence == 2) ? 8u : 1u;
        gLPC.MDCTCoefficientsToLPC(c.spec, hop, len, blocks);
        // 2e-07 is what the envelope check above measured the FFT at.
        const double sens = lpcSensitivity(c.spec, hop, len, blocks, 2e-7f);
        const double bound = (sens > 1e-9) ? 8.0 * sens : 1e-6;
        const double w = relCompare(gLPC.CoreLPC(), lpc, kBweLPCOrder + 1,
                                    "core lpc", i, bound);
        if (w > *worst) *worst = w;
        if (sens > 1e-12 && w / sens > *worstRatio) *worstRatio = w / sens;
    }
    f.Close();
}

// ---------------------------------------------------------------- stage 4 ----
// APACBWE2Decoder::DecodeFrame end to end.
//
// The header is not injected: it is PARSED, out of a bitstream this builds by
// hand in exactly the shape APACBWE2Decoder::Deserialize reads for a single
// channel element - a 1 bit, two 9-bit codebook indices, and one 6-bit gain per
// window group.  So the seam between the parser and the synthesis is under test
// too, and the scale array is filled the only way the decoder can fill it.
uint32_t groupsOf(uint32_t word)
{
    uint32_t n = 0;
    for (uint32_t g = 0; g < 8; ++g)
        if (((word >> (4 * g)) & 0xF) != 0)
            ++n;
    return n;
}

struct BitWriter {
    uint8_t  buf[64];
    uint32_t pos;
    void Init() { pos = 0; memset(buf, 0, sizeof buf); }
    void Put(uint32_t v, uint32_t n)
    {
        for (int i = (int)n - 1; i >= 0; --i) {
            if ((v >> i) & 1u)
                buf[pos >> 3] |= (uint8_t)(0x80u >> (pos & 7));
            ++pos;
        }
    }
};

long   gBandBins = 0;
long   gBandLoose = 0;

void checkFrame(double* worstTouched, int* nLong, int* nShort, int* nWide,
                int* nSkipped, int* nGroups)
{
    Fixture f;
    f.Open("bwe_frame.fixture");
    const char* hdr = f.Expect("frame cases");
    int n = 0;
    if (sscanf(hdr, "frame cases %d", &n) != 1 || n != kBweFrameCases)
        fail("bwe_frame.fixture header", 0);
    *worstTouched = 0.0;
    *nLong = *nShort = *nWide = *nSkipped = *nGroups = 0;

    for (int i = 0; i < kBweFrameCases; ++i) {
        const BweFrameCase& c = gFrameCases[i];
        char tag[64];
        snprintf(tag, sizeof tag, "case %d rc %u ch %u", i, 0u, c.channel);
        f.Expect(tag);
        f.Floats("spec", gRef, kBweFrameLength);

        const apac::SFBOffsets* table =
            apac::GetSFBOffsetTable(c.sampleRate, 1024);
        if (!table)
            fail("no SFB record", i);

        // The element the header is read against.  Only the ICS fields
        // Deserialize and DecodeFrame look at have to be right.
        if (gElem.Configure(apac::kElementSCE, table, c.sampleRate) != apac::kOk)
            fail("element Configure", i);
        apac::APACICSInfo& ics = gElem.GetICSInfo(true);
        ics.windowSequence = (uint8_t)c.windowSequence;
        ics.maxSfb = (uint8_t)c.maxSfb;
        ics.windowGroupWord = c.windowGroupWord;
        const uint32_t groups = groupsOf(c.windowGroupWord);
        ics.numWindowGroups = (uint8_t)groups;
        *nGroups += (int)groups;

        BitWriter w;
        w.Init();
        w.Put(1, 1);                       // the per-channel present bit
        w.Put(c.env[0], 9);
        w.Put(c.env[1], 9);
        for (uint32_t g = 0; g < groups; ++g)
            w.Put(c.scale[g], 6);
        apac::BitstreamReader bs(w.buf, (uint32_t)sizeof w.buf);

        gBWE.Initialize(table, &gLPC);
        const int32_t before = bs.BitsRemaining();
        if (gBWE.Deserialize(bs, c.channel, gElem) != apac::kOk)
            fail("BWE header", i);
        if (before - bs.BitsRemaining() != (int32_t)w.pos)
            fail("BWE header bit count", i);
        if (!gBWE.GetActiveFlag(c.channel))
            fail("the channel did not come up active", i);
        ++gChecks;

        memcpy(gSpec, c.spec, sizeof gSpec);
        const uint32_t rc = gBWE.DecodeFrame(ics, gSpec, c.channel);
        if (rc != apac::kOk)
            fail("DecodeFrame returned non-zero", i);
        ++gChecks;

        // Which bins the synthesis is allowed to have written.
        const uint32_t edge = BweCoreEdge(c.sampleRate, c.windowSequence,
                                          c.maxSfb);
        const uint32_t bwStart = (edge >= 384) ? 384u : 256u;
        const bool wide = (edge >= 384);
        const bool shortBlock = (c.windowSequence == 2);
        uint8_t touched[kBweFrameLength];
        memset(touched, 0, sizeof touched);
        // maxSfb == 0 is the second half of the entry gate: nothing at all.
        if (c.maxSfb != 0) {
            // ... and so is an all-zero core, which the find_if guard catches.
            bool anyCore = false;
            if (shortBlock) {
                for (uint32_t win = 0; win < 8 && !anyCore; ++win)
                    for (uint32_t k = 0; k < (bwStart >> 3); ++k)
                        if (c.spec[win * 128 + k] != 0.0f) { anyCore = true; break; }
            } else {
                for (uint32_t k = 0; k < bwStart; ++k)
                    if (c.spec[k] != 0.0f) { anyCore = true; break; }
            }
            if (anyCore) {
                if (shortBlock)
                    for (uint32_t win = 0; win < 8; ++win)
                        for (uint32_t k = 0; k < 64; ++k)
                            touched[win * 128 + (bwStart >> 3) + k] = 1;
                else
                    for (uint32_t k = 0; k < 512; ++k)
                        touched[bwStart + k] = 1;
            }
        }
        uint32_t nTouched = 0;
        for (uint32_t k = 0; k < kBweFrameLength; ++k)
            nTouched += touched[k];
        if (nTouched == 0)
            ++*nSkipped;
        else if (shortBlock)
            ++*nShort;
        else
            ++*nLong;
        if (nTouched && wide)
            ++*nWide;

        // The untouched bins must come back BIT-IDENTICAL - both to the input
        // and to what the library returned.  That is what pins the geometry.
        for (uint32_t k = 0; k < kBweFrameLength; ++k) {
            if (touched[k])
                continue;
            if (float_to_bits(gSpec[k]) != float_to_bits(c.spec[k]))
                fail("the port wrote outside the extension band", i);
            if (float_to_bits(gRef[k]) != float_to_bits(c.spec[k]))
                fail("the library wrote outside the predicted band", i);
            gBitChecks += 2;
        }
        // The extension band itself, with the FFT tolerance.
        if (nTouched) {
            static float ours[kBweFrameLength];
            static float theirs[kBweFrameLength];
            uint32_t m = 0;
            for (uint32_t k = 0; k < kBweFrameLength; ++k)
                if (touched[k]) { ours[m] = gSpec[k]; theirs[m] = gRef[k]; ++m; }
            const double wr = relCompare(ours, theirs, m, "extension band", i,
                                         2e-2);
            if (wr > *worstTouched) *worstTouched = wr;

            // The sharp gate, and it is per bin and RELATIVE to that bin's own
            // value rather than to the band's peak.  Against-the-peak is the
            // wrong shape here: it lets a whole patch be wrong by a hair and it
            // convicts a single bin sitting on a null.  Measured over the whole
            // plan, EVERY extension coefficient of EVERY case lands within 1e-01
            // of the library's own value and all but one within 1e-02, so those
            // are what is required.  A wrong patch source, a wrong stride, a
            // swapped patch order or a gain read from the wrong window group
            // moves every bin of at least one patch - a quarter of the band -
            // by a factor, not by a part in ten.
            uint32_t within2 = 0, within1 = 0;
            for (uint32_t k = 0; k < m; ++k) {
                const double t = fabs((double)theirs[k]);
                const double d = fabs((double)ours[k] - (double)theirs[k]);
                ++gBandBins;
                if (!(t == 0.0 ? (d == 0.0) : (d <= 1e-3 * t)))
                    ++gBandLoose;
                if (t == 0.0 ? (d == 0.0) : (d <= 1e-2 * t)) ++within2;
                if (t == 0.0 ? (d == 0.0) : (d <= 1e-1 * t)) ++within1;
            }
            if (within1 != m) {
                fprintf(stderr,
                        "\nFAIL extension band case %d: %u of %u coefficients "
                        "outside 1e-01 of the library\n", i, m - within1, m);
                exit(1);
            }
            if ((double)within2 < 0.99 * (double)m) {
                fprintf(stderr,
                        "\nFAIL extension band case %d: only %u of %u "
                        "coefficients within 1e-02 of the library\n",
                        i, within2, m);
                exit(1);
            }
            gChecks += (int)m;
        }

        // The port shares ONE BWELPC where the original has one per channel.
        // Nothing in it survives a call, so a fresh instance must give the same
        // bits - run every case a second time that way and require it.
        gLPCFresh.Initialize();
        gBWE.Initialize(table, &gLPCFresh);
        apac::BitstreamReader bs2(w.buf, (uint32_t)sizeof w.buf);
        if (gBWE.Deserialize(bs2, c.channel, gElem) != apac::kOk)
            fail("BWE header (fresh)", i);
        static float again[kBweFrameLength];
        memcpy(again, c.spec, sizeof again);
        if (gBWE.DecodeFrame(ics, again, c.channel) != rc)
            fail("a fresh BWELPC changed the return code", i);
        expectBits(again, gSpec, kBweFrameLength, "shared vs fresh BWELPC", i);
    }

    // The two gates, checked where the fixture cannot: a channel that never
    // came up active must be left alone even with a perfectly good header.
    const BweFrameCase& c = gFrameCases[0];
    const apac::SFBOffsets* table = apac::GetSFBOffsetTable(c.sampleRate, 1024);
    gBWE.Initialize(table, &gLPC);
    apac::APACICSInfo ics;
    ics.Clear();
    ics.windowSequence = 0;
    ics.maxSfb = 30;
    ics.windowGroupWord = 1;
    memcpy(gSpec, c.spec, sizeof gSpec);
    if (gBWE.DecodeFrame(ics, gSpec, 0) != apac::kOk)
        fail("an inactive channel was not a no-op", 0);
    expectBits(gSpec, c.spec, kBweFrameLength, "inactive channel", 0);

    // A maxSfb past the end of the table is impossible from a bitstream and is
    // refused rather than read out of bounds.  The original indexes the table
    // with it unchecked.
    BitWriter w;
    w.Init();
    w.Put(1, 1); w.Put(0, 9); w.Put(0, 9); w.Put(0, 6);
    if (gElem.Configure(apac::kElementSCE, table, c.sampleRate) != apac::kOk)
        fail("element Configure", 0);
    apac::APACICSInfo& e = gElem.GetICSInfo(true);
    e.windowSequence = 0;
    e.maxSfb = (uint8_t)(table->numSfbLong + 1);
    e.numWindowGroups = 1;
    e.windowGroupWord = 1;
    apac::BitstreamReader bs(w.buf, (uint32_t)sizeof w.buf);
    if (gBWE.Deserialize(bs, 0, gElem) != apac::kOk)
        fail("BWE header", 0);
    if (gBWE.DecodeFrame(e, gSpec, 0) != apac::kBadValue)
        fail("maxSfb past the table was not refused (long)", 0);
    e.windowSequence = 2;
    e.maxSfb = (uint8_t)(table->numSfbShort + 1);
    if (gBWE.DecodeFrame(e, gSpec, 0) != apac::kBadValue)
        fail("maxSfb past the table was not refused (short)", 0);
    // And a build with no synthesis refuses rather than silently doing nothing.
    gBWE.Initialize(table, 0);
    apac::BitstreamReader bs3(w.buf, (uint32_t)sizeof w.buf);
    e.windowSequence = 0;
    e.maxSfb = 30;
    if (gBWE.Deserialize(bs3, 0, gElem) != apac::kOk)
        fail("BWE header", 0);
    if (gBWE.DecodeFrame(e, gSpec, 0) != apac::kPortNeedsBWE)
        fail("a null BWELPC did not refuse", 0);
    gChecks += 4;
}

}  // namespace

static uint32_t BweCoreEdge(uint32_t rate, uint32_t seq, uint32_t maxSfb)
{
    const apac::SFBOffsets* t = apac::GetSFBOffsetTable(rate, 1024);
    if (!t) {
        fprintf(stderr, "\nno SFB record for %u Hz\n", rate);
        exit(1);
    }
    if (seq == 2)
        return 8u * (uint32_t)t->sfbOffsetShort[maxSfb];
    return (uint32_t)t->sfbOffsetLong[maxSfb];
}

int main(int argc, char** argv)
{
    if (argc > 1)
        gFixtureDir = argv[1];

    gLPC.Initialize();
    checkTables();

    double worstLsp = 0.0, worstLpc = 0.0;
    checkLsf(&worstLsp, &worstLpc);

    double worstEnv = 0.0;
    int envCases = 0;
    checkEnv(&worstEnv, &envCases);

    BweBuildFrameCases(gFrameCases);
    double worstCoreLpc = 0.0, worstCoreRatio = 0.0;
    checkLpc(&worstCoreLpc, &worstCoreRatio);

    double worstBand = 0.0;
    int nLong = 0, nShort = 0, nWide = 0, nSkipped = 0, nGroups = 0;
    checkFrame(&worstBand, &nLong, &nShort, &nWide, &nSkipped, &nGroups);

    printf("OK  %d bit-exact checks, %d toleranced; %d LSF index pairs, "
           "%d envelope calls, %d DecodeFrame cases (%d long, %d short, "
           "%d wide geometry, %d skipped by a gate, %d window groups)\n",
           gBitChecks, gChecks, kBweLsfCases, envCases, kBweFrameCases,
           nLong, nShort, nWide, nSkipped, nGroups);
    printf("    bit-exact: the two 512x16 LSF codebooks and the 64-entry gain "
           "table, every conditioned LSF vector, every coefficient outside the "
           "extension band on BOTH sides, and shared-vs-fresh BWELPC on every "
           "case\n");
    printf("    with an FFT under them (Apple's vDSP_DFT_Execute is not on the "
           "head unit): LPCToMDCTSpectrum %.2e of full scale on the library's "
           "own LPC vectors, incl. the 96- and 768-point radix-3 transforms; "
           "MDCTCoefficientsToLPC %.2e (%.1fx what a 2e-07 perturbation of its "
           "OWN autocorrelation moves it by, so it is order-16 Levinson "
           "conditioning and not an index); LSP %.2e, LSF->LPC %.2e\n",
           worstEnv, worstCoreLpc, worstCoreRatio, worstLsp, worstLpc);
    printf("    the extension band end to end: %.2e of the band's peak.  The "
           "synthesis DIVIDES one LP envelope by another, so a null of |A(w)| "
           "is ill-conditioned by construction - in the original as much as "
           "here; same effect stage h1 measured on the LRVQ residual\n",
           worstBand);
    printf("    and it is a SCATTER, not a shift.  Per bin, against that bin's "
           "OWN value: every one of the %ld extension coefficients is within "
           "1e-01 and at least 99 %% of each case within 1e-02 (both required), "
           "and %ld of them - %.2f %% - are outside 1e-03.  A wrong patch "
           "source, stride, patch order or gain group moves every bin of at "
           "least one patch by a factor\n",
           gBandBins, gBandLoose,
           100.0 * (double)gBandLoose / (double)gBandBins);
    return 0;
}
