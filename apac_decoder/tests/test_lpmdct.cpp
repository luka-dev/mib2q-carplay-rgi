// Differential test: apac::RealFFT and apac::LPMDCTFilter against
// MultiRadixRealFFT and apac::LPMDCTFilter in the real library.
//
// FIXTURE: tests/fixtures/lpmdct.fixture.
//
//   regenerate:  apac_decoder/tools/gen_fixtures_transform.sh   (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `lpmdct`
//   inputs:      apac_decoder/reference/dsp_cases.h, shared with the oracle
//
// The real FFT is checked first and on its own, because it is Apple's vDSP
// packing and nothing above it can agree until that convention does.  What the
// library actually does, established here and not assumed:
//
//   * Forward is the PLAIN unscaled DFT.  vDSP's real transform has a factor of
//     two in it and MultiRadixRealFFT::RealInPlaceTransform multiplies it back
//     out by the 0.5 stored at 0x345db4, so the two cancel.
//   * The packing puts X[0] in realp[0] and X[n/2] in imagp[0] - both are real,
//     so one slot holds both - and Re/Im X[k] in realp[k] / imagp[k].
//   * Inverse is the plain UNNORMALISED inverse, so Inverse(Forward(x)) is
//     n * x, not x.
//
// Then the five LPMDCTFilter entry points, over four transform lengths (64 to
// 512), LPC orders 1 to 16, and both values of the fftSize / n ratio - 2 and 4
// - because 4 is the one value the original tests for and it switches the
// output between stride 1 and stride 2.
//
// Tolerance and why there is one: tests/dsp_fixture.h.  The FFT here is the
// port's own for the same reason the MDCT's is.

#include "apac/lpc.h"

#include "../reference/dsp_cases.h"
#include "dsp_fixture.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

float g_want[2048];
float g_have[2048];
float g_x[4096];
// The library's own forward output, kept so that the inverse can be tested on
// it rather than on the port's forward.  Chaining the port's two transforms
// would double the error budget and measure the wrong thing.
float g_libFwd[2048];

int g_checks = 0;
int g_div4 = 0;
double g_worst = 0.0;
char g_worstWhere[128] = { 0 };

void expect(const char* what, unsigned c, uint32_t n, float scale)
{
    char label[128];
    const apac_cases::LPMDCTCase& lc = apac_cases::lpmdct_plan()[c];
    snprintf(label, sizeof label, "%s case %u (fft %u, order %u, div %u)",
             what, c, lc.fftSize, lc.order, lc.divisor);
    double w = dspfx::compare(g_have, g_want, n, label, scale);
    if (w > g_worst) {
        g_worst = w;
        snprintf(g_worstWhere, sizeof g_worstWhere, "%s", label);
    }
    ++g_checks;
}

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/lpmdct.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    char line[65536];
    unsigned caseIdx = 0;
    uint32_t N = 0, order = 0, n = 0;
    char section[32] = { 0 };
    uint32_t wantN = 0, got = 0;

    apac::LPMDCTFilter filt;
    apac::RealFFT rfft;

    struct Run {
        static void go(const char* section, unsigned c, uint32_t N, uint32_t order,
                       uint32_t n, apac::LPMDCTFilter* filt, apac::RealFFT* rfft)
        {
            if (!strcmp(section, "RFFTFWD")) {
                apac_cases::lpmdct_fill(c, 0x11111111u, g_x, N);
                memset(g_have, 0, sizeof(float) * N);
                rfft->Forward(g_x, g_have, g_have + N / 2);
                expect("RealFFT forward", c, N, dspfx::peak(g_want, N));
                memcpy(g_libFwd, g_want, sizeof(float) * N);
                return;
            }
            if (!strcmp(section, "RFFTINV")) {
                // Fed the LIBRARY's forward output, which the RFFTFWD section
                // just left in g_libFwd, so this measures the inverse alone.
                memset(g_have, 0, sizeof(float) * N);
                rfft->Inverse(g_libFwd, g_libFwd + N / 2, g_have);
                expect("RealFFT inverse", c, N, dspfx::peak(g_want, N));
                return;
            }
            if (!strcmp(section, "IMP2ENV")) {
                apac_cases::lpmdct_fill(c, 0x22222222u, g_x, N);
                memset(g_have, 0, sizeof(float) * n);
                filt->LPImpulseToSpecEnvelope(g_x, g_have, n);
                expect("LPImpulseToSpecEnvelope", c, n, dspfx::peak(g_want, n));
                return;
            }
            if (!strcmp(section, "LPC2ENV")) {
                apac_cases::lpmdct_fill(c, 0x33333333u, g_x, order + 1);
                memset(g_have, 0, sizeof(float) * n);
                filt->LPCToSpecEnvelope(g_x, g_have, n);
                expect("LPCToSpecEnvelope", c, n, dspfx::peak(g_want, n));
                return;
            }
            if (!strcmp(section, "PSD2AC")) {
                apac_cases::lpmdct_fill(c, 0x44444444u, g_x, n);
                memset(g_have, 0, sizeof(float) * (order + 1));
                filt->AutocorrFromPSD(g_x, g_have, n);
                expect("AutocorrFromPSD", c, order + 1, dspfx::peak(g_want, order + 1));
                return;
            }
            if (!strcmp(section, "MDCT2AC")) {
                apac_cases::lpmdct_fill(c, 0x55555555u, g_x, 4 * n);
                memset(g_have, 0, sizeof(float) * (order + 1));
                filt->MDCTToAutoCorr(g_x, g_have, n, 4, n);
                expect("MDCTToAutoCorr", c, order + 1, dspfx::peak(g_want, order + 1));
                return;
            }
            fprintf(stderr, "unknown section %s\n", section);
            exit(1);
        }
    };

    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "LPM ", 4)) {
            unsigned c, fft, ord, div, nn, sc;
            if (sscanf(line, "LPM %u fft=%u order=%u div=%u n=%u scale=%x",
                       &c, &fft, &ord, &div, &nn, &sc) != 6) {
                fprintf(stderr, "bad LPM line: %s", line);
                return 1;
            }
            const apac_cases::LPMDCTCase& lc = apac_cases::lpmdct_plan()[c];
            assert(lc.fftSize == fft && lc.order == ord && lc.divisor == div);
            caseIdx = c; N = fft; order = ord; n = nn;
            if (div == 4) ++g_div4;

            const float scale = filt.Initialize((uint16_t)fft, (uint16_t)ord);
            if (dspfx::float_to_bits(scale) != sc) {
                fprintf(stderr, "\nMISMATCH Initialize(%u) scale: port %08x, "
                                "library %08x\n", fft,
                        dspfx::float_to_bits(scale), sc);
                return 1;
            }
            if (!rfft.Initialize(fft)) {
                fprintf(stderr, "RealFFT::Initialize(%u) refused\n", fft);
                return 1;
            }
            section[0] = 0;
            continue;
        }

        if (line[0] != ' ') {
            size_t len = strlen(line);
            while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                line[--len] = 0;
            snprintf(section, sizeof section, "%s", line);
            if (!strcmp(section, "RFFTFWD") || !strcmp(section, "RFFTINV"))
                wantN = N;
            else if (!strcmp(section, "IMP2ENV") || !strcmp(section, "LPC2ENV"))
                wantN = n;
            else
                wantN = order + 1;
            got = 0;
            continue;
        }

        got += dspfx::parse_hex_floats(line, g_want, 2048, got);
        if (got == wantN && section[0]) {
            Run::go(section, caseIdx, N, order, n, &filt, &rfft);
            section[0] = 0;
        }
    }
    fclose(f);

    assert(g_checks == 6 * apac_cases::kLPMDCTCases);
    assert(g_div4 > 0 && g_div4 < (int)apac_cases::kLPMDCTCases);

    // Size gate: the transform length must be a power of two in range.
    apac::RealFFT r;
    assert(!r.Initialize(0));
    assert(!r.Initialize(2));
    assert(!r.Initialize(100));
    assert(!r.Initialize(apac::kMaxRealFFT * 2));
    assert(r.Initialize(apac::kMaxRealFFT));

    printf("OK  %d checks over %d configs, worst %.2g of full scale (%s)\n",
           g_checks, (int)apac_cases::kLPMDCTCases, g_worst, g_worstWhere);
    return 0;
}
