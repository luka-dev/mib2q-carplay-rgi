// Differential test: apac::MDCT against MDCT::InverseInPlaceTransform in the
// real library.
//
// FIXTURE: tests/fixtures/mdct.fixture, produced by constructing an MDCT with
// the library's own constructor (_ZN4MDCTC1Ev), calling
// MDCT::Initialize(n, 0.0f) at 0xb7530 and then
// MDCT::InverseInPlaceTransform at 0xb75c0 on each input vector.
//
//   regenerate:  apac_decoder/tools/gen_fixtures_transform.sh   (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `mdct`
//   inputs:      apac_decoder/reference/dsp_cases.h, shared with the oracle
//
// Both block sizes (128 and 1024 - the only two that exist) over all 16 cases:
// zero, three impulses, full-scale alternating, DC, DENORMALS, 1e-30, 1e8,
// three random, 1e-12, 1e4, a ramp, and a sparse vector.  Both output halves
// are checked, 36 864 floats in all.
//
// NOT bit-exact, and cannot be: the complex FFT inside the original is
// vDSP_DFT_Execute from Apple's Accelerate framework, which is closed and is
// not on the head unit, so the port has its own. tests/dsp_fixture.h explains
// the tolerance. Everything else - the pre-modulation, the eight-store
// post-modulation, the scale - is transcribed index for index and would show up
// here as a gross mismatch, not as rounding.

#include "apac/transform.h"

#include "../reference/dsp_cases.h"
#include "dsp_fixture.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const uint32_t kCap = apac::kMaxTransform;

float g_in[kCap];
float g_second[kCap];
float g_wantFirst[kCap];
float g_wantSecond[kCap];

int g_cases = 0;
int g_floats = 0;
double g_worst = 0.0;
char g_worstWhere[128] = { 0 };

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/mdct.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    char line[65536];
    apac::MDCT mdct;
    uint32_t initialised = 0;
    uint32_t n = 0;
    unsigned caseIdx = 0;
    int part = -1;                 // 0 = first half, 1 = second half
    uint32_t gotFirst = 0, gotSecond = 0;
    bool pending = false;
    bool sawShort = false, sawLong = false;

    struct Check {
        static void run(uint32_t n, unsigned caseIdx, apac::MDCT* mdct)
        {
            apac_cases::case_fill(caseIdx, g_in, n);
            const float inScale = dspfx::peak(g_in, n);
            memset(g_second, 0, sizeof(float) * n);
            int32_t rc = mdct->InverseInPlaceTransform(g_in, g_second);
            assert(rc == apac::kTransformOk);

            char what[96];
            snprintf(what, sizeof what, "n=%u case %u (%s) first half",
                     n, caseIdx, apac_cases::case_name(caseIdx));
            double a = dspfx::compare(g_in, g_wantFirst, n, what, inScale);
            snprintf(what, sizeof what, "n=%u case %u (%s) second half",
                     n, caseIdx, apac_cases::case_name(caseIdx));
            double b = dspfx::compare(g_second, g_wantSecond, n, what, inScale);
            double w = (a > b) ? a : b;
            if (w > g_worst) {
                g_worst = w;
                snprintf(g_worstWhere, sizeof g_worstWhere, "n=%u case %u (%s)",
                         n, caseIdx, apac_cases::case_name(caseIdx));
            }

            // The IMDCT's own structure: the first half is antisymmetric and
            // the second symmetric.  Checked on the port's output, so a change
            // that broke the eight-store index map would fail even if the
            // fixture were stale.
            for (uint32_t i = 0; i < n; ++i) {
                assert(g_in[i] == -g_in[n - 1 - i]);
                assert(g_second[i] == g_second[n - 1 - i]);
            }
            g_floats += (int)(2 * n);
            ++g_cases;
        }
    };

    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "MDCT ", 5)) {
            // Flush first: the size changes here, and the previous size's last
            // case has not been checked yet.
            if (pending) Check::run(n, caseIdx, &mdct);
            pending = false;
            unsigned nn, sc;
            int rc;
            if (sscanf(line, "MDCT %u init=%d scale=%x", &nn, &rc, &sc) != 3) {
                fprintf(stderr, "bad MDCT line: %s", line);
                return 1;
            }
            int32_t ours = mdct.Initialize(nn, 0.0f);
            if (ours != rc) {
                fprintf(stderr, "\nMISMATCH MDCT::Initialize(%u): port %d, library %d\n",
                        nn, ours, rc);
                return 1;
            }
            // scale == 0 means 1/n, and it has to be the same float.
            if (dspfx::float_to_bits(mdct.Scale()) != sc) {
                fprintf(stderr, "\nMISMATCH MDCT scale for n=%u: port %08x, library %08x\n",
                        nn, dspfx::float_to_bits(mdct.Scale()), sc);
                return 1;
            }
            initialised = nn;
            if (nn == 128) sawShort = true;
            if (nn == 1024) sawLong = true;
            continue;
        }

        if (!strncmp(line, "CASE ", 5)) {
            if (pending) Check::run(n, caseIdx, &mdct);
            pending = false;
            unsigned nn, ci;
            char name[32];
            int rc;
            if (sscanf(line, "CASE %u %u %31s rc=%d", &nn, &ci, name, &rc) != 4) {
                fprintf(stderr, "bad CASE line: %s", line);
                return 1;
            }
            assert(rc == 0);
            assert(nn == initialised);
            n = nn;
            caseIdx = ci;
            gotFirst = gotSecond = 0;
            part = -1;
            continue;
        }

        if (!strncmp(line, "FIRST", 5))  { part = 0; continue; }
        if (!strncmp(line, "SECOND", 6)) { part = 1; continue; }

        if (part == 0) {
            gotFirst += dspfx::parse_hex_floats(line, g_wantFirst, kCap, gotFirst);
        } else if (part == 1) {
            gotSecond += dspfx::parse_hex_floats(line, g_wantSecond, kCap, gotSecond);
            if (gotSecond == n && gotFirst == n) pending = true;
        }
    }
    if (pending) Check::run(n, caseIdx, &mdct);
    fclose(f);

    assert(sawShort && sawLong);
    assert(g_cases == 2 * apac_cases::kNumCases);

    // The size gate, and the not-initialised path.
    apac::MDCT fresh;
    static float a[8], b[8];
    assert(fresh.InverseInPlaceTransform(a, b) == apac::kTransformNotInit);
    assert(fresh.Initialize(16, 0.0f) == apac::kTransformBadSize);
    assert(fresh.Initialize(256, 0.0f) == apac::kTransformBadSize);
    assert(fresh.Initialize(128, 0.0f) == apac::kTransformOk);

    printf("OK  %d cases, %d floats, worst %.2g of full scale (%s)\n",
           g_cases, g_floats, g_worst, g_worstWhere);
    return 0;
}
