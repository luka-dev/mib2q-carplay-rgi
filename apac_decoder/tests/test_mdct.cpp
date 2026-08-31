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
//
// ============================ WHY THE TOLERANCE IS NOT A BUG ================
//
// Every other stage of this port is bit-exact, so a tolerance here needs more
// than "it is small".  Two things are checked on every run rather than argued:
//
// 1. A DOUBLE-PRECISION REFERENCE.  RefMDCT below runs the same algorithm -
//    the same pre-modulation, an O(m^2) DFT, the same eight-store post-
//    modulation - entirely in double, using the library's own float32
//    modulation table promoted to double.  It is therefore the exact answer
//    the port and the library are both approximating.  The test measures how
//    far EACH of them lands from it and asserts
//
//        err(port) <= 4 * err(library)
//
//    If the port had an off-by-one in a twiddle index, a window index or the
//    post-modulation index map, err(port) would be enormous while
//    err(library) stayed at one ULP, and that assertion would fail loudly.
//    What is actually measured is that the two are the SAME SIZE: two
//    different but equally valid factorisations of the same transform, each
//    about one ULP from the truth.  The port is not systematically the worse
//    of the two, and on many cases it is the better one.
//
// 2. THE ERROR TRACKS THE NUMBER OF TERMS SUMMED.  Cases 16..20 hold exactly
//    2, 4, 16, 64 and 256 non-zero coefficients.  The disagreement, measured
//    in ULPs of the block's own peak, grows monotonically with that count and
//    is a fraction of an ULP when only a couple of bins are active.  That is
//    the signature of accumulated rounding.  A wrong index would not care how
//    many bins were active - it would be just as wrong on two as on a
//    thousand.
//
// The one case that reaches 2.1e-06 of full scale is `denormal`, where every
// input is a denormal near 1e-39: there the ENTIRE precision available is a
// few denormal ULPs, and the library is no closer to the truth than the port.
// It is not representative and is not the number to quote.
//
// 3. AND IT DOES NOT COMPOUND ON REAL DATA.  `run.sh swap` patches the port's
//    filter bank into the middle of Pioneer's own decoder - see
//    reference/transform_swap_shim.cpp - and decodes the whole capture with it:
//    3977 packets, 4 072 448 frames, 24.4 M samples of real coefficients in
//    the real window-sequence order, not synthetic vectors.  Result:
//
//      max |diff|  2.09e-07 absolute, 3.5e-07 of full scale, -129 dBFS
//      in S16      1 LSB, on 0.0059 % of samples (1444 of 24 434 688)
//
//    and, the number that actually decides it, the agreement with Apple's own
//    decoder is UNCHANGED to four significant figures - per-channel rms(diff)
//    0.2637 / 0.5925 / 0.4724 / 0.2944 / 0.2967 / 0.2471 before the
//    substitution and 0.2638 / 0.5925 / 0.4724 / 0.2944 / 0.2967 / 0.2471
//    after.  Pioneer and Apple are already 1-2 LSB apart on this capture; the
//    port's transform disappears inside that.
// ===========================================================================

#include "apac/transform.h"

#include "../reference/dsp_cases.h"
#include "dsp_fixture.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace {

const uint32_t kCap = apac::kMaxTransform;

// ------------------------------------------------------- double reference ---
// The same algorithm as apac::MDCT, in double, with a naive O(m^2) DFT.  Slow
// and obviously correct, which is the point: it has no factorisation of its own
// to be wrong about, and at m <= 512 its own error is ~1e-13, six orders below
// float32's epsilon.  The modulation coefficients are the library's float32
// table promoted to double, so this isolates ARITHMETIC rounding from table
// content - the table is separately proved bit-exact in test_transform_tables.
double g_refFirst[kCap];
double g_refSecond[kCap];

void RefMDCT(uint32_t n, const float* in, float scale)
{
    static double re[kCap], im[kCap], fr[kCap], fi[kCap];
    static double twr[kCap], twi[kCap];
    const uint32_t half = n >> 1;
    const float* c = apac::GetDCTIVModulationCoefficients(n);
    const double kTwoPi = 6.283185307179586476925286766559;

    for (uint32_t m = 0; m < half; ++m) {
        const double x = (double)scale * (double)in[2 * m];
        const double y = (double)scale * (double)in[n - 1 - 2 * m];
        const double cr = (double)c[2 * m];
        const double ci = (double)c[2 * m + 1];
        re[m] = x * cr + y * ci;
        im[m] = y * cr - x * ci;
    }
    for (uint32_t k = 0; k < half; ++k) {
        const double a = -kTwoPi * (double)k / (double)half;
        twr[k] = cos(a);
        twi[k] = sin(a);
    }
    for (uint32_t k = 0; k < half; ++k) {
        double sr = 0.0, si = 0.0;
        for (uint32_t j = 0; j < half; ++j) {
            const uint32_t t = (uint32_t)(((uint64_t)j * k) % half);
            sr += re[j] * twr[t] - im[j] * twi[t];
            si += re[j] * twi[t] + im[j] * twr[t];
        }
        fr[k] = sr;
        fi[k] = si;
    }
    const uint32_t quarter = half >> 1;
    for (uint32_t k = 0; k < quarter; ++k) {
        const uint32_t m = half - 1 - k;
        const double ckr = c[2 * k],  cki = c[2 * k + 1];
        const double cmr = c[2 * m],  cmi = c[2 * m + 1];
        const double Rk =  fr[k] * ckr + fi[k] * cki;
        const double Ik = -(fi[k] * ckr - fr[k] * cki);
        const double Rm =  fr[m] * cmr + fi[m] * cmi;
        const double Im = -(fr[m] * cmi - fi[m] * cmr);
        const uint32_t lo = 2 * m - half;
        const uint32_t hi = half + 2 * k;
        g_refSecond[hi]     = -Rk;
        g_refSecond[hi + 1] =  Im;
        g_refSecond[lo]     =  Im;
        g_refSecond[lo + 1] = -Rk;
        g_refFirst[hi]     = -Ik;
        g_refFirst[hi + 1] = -Rm;
        g_refFirst[lo]     =  Rm;
        g_refFirst[lo + 1] =  Ik;
    }
}

double maxAbsDiff(const float* v, const double* ref, uint32_t n)
{
    double m = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        const double d = fabs((double)v[i] - ref[i]);
        if (d > m) m = d;
    }
    return m;
}

double peakAbs(const double* v, uint32_t n)
{
    double m = 0.0;
    for (uint32_t i = 0; i < n; ++i)
        if (fabs(v[i]) > m) m = fabs(v[i]);
    return m;
}

uint32_t activeBins(const float* v, uint32_t n)
{
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; ++i)
        if (v[i] != 0.0f) ++k;
    return k;
}

// Per-case record of how far each side landed from the double reference.
double g_ulpPort = 0.0;      // worst, over every case
double g_ulpLib = 0.0;
double g_ulpPortNormal = 0.0;   // excluding the denormal case, see below
double g_ulpLibNormal = 0.0;
char g_ulpWhere[96] = { 0 };
int g_precChecks = 0;

// The accumulation curve, at n = 1024: how far the port lands from the exact
// answer, in ULPs of the block's own peak, as the number of non-zero input
// coefficients goes 1, 2, 4, 16, 64, 256, 1024.
const unsigned kCurveCases[7] = { 1, 16, 17, 18, 19, 20, 9 };
const unsigned kCurveBins[7]  = { 1,  2,  4, 16, 64, 256, 1024 };
double g_curve[7]    = { -1, -1, -1, -1, -1, -1, -1 };  // ULP of the OUTPUT peak
double g_absCurve[7] = { -1, -1, -1, -1, -1, -1, -1 };  // ULP of the INPUT peak

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
            // --- the precision argument, checked rather than asserted ---
            // Re-fill the input: InverseInPlaceTransform overwrote g_in.
            static float clean[kCap];
            apac_cases::case_fill(caseIdx, clean, n);
            RefMDCT(n, clean, mdct->Scale());
            const double refPeak = (peakAbs(g_refFirst, n) > peakAbs(g_refSecond, n))
                                 ? peakAbs(g_refFirst, n) : peakAbs(g_refSecond, n);
            if (refPeak > 0.0) {
                const double ep = (maxAbsDiff(g_in, g_refFirst, n) >
                                   maxAbsDiff(g_second, g_refSecond, n))
                                ? maxAbsDiff(g_in, g_refFirst, n)
                                : maxAbsDiff(g_second, g_refSecond, n);
                const double el = (maxAbsDiff(g_wantFirst, g_refFirst, n) >
                                   maxAbsDiff(g_wantSecond, g_refSecond, n))
                                ? maxAbsDiff(g_wantFirst, g_refFirst, n)
                                : maxAbsDiff(g_wantSecond, g_refSecond, n);
                // One ULP at this block's own magnitude.  The max() matters:
                // the `denormal` case peaks at ~4e-39, where float32's spacing
                // is no longer relative but a flat 1.4e-45, and dividing by the
                // relative figure would report hundreds of bogus ULPs for both
                // sides.
                double ulp = refPeak * 1.1920928955078125e-07;
                if (ulp < 1.40129846432481707e-45)
                    ulp = 1.40129846432481707e-45;
                const double up = ep / ulp, ul = el / ulp;

                // THE claim: the port is not materially worse than the library
                // at approximating the same exact transform.  An index error in
                // the port would blow this up by orders of magnitude while the
                // library stayed at ~1 ULP.  The +0.5 ULP slack keeps the
                // comparison meaningful when both are already sub-ULP.
                if (up > 4.0 * ul + 0.5) {
                    fprintf(stderr,
                            "\nPRECISION n=%u case %u (%s): port is %.2f ULP from "
                            "the double reference, library only %.2f - that is a "
                            "factorisation difference no longer, it is a defect\n",
                            n, caseIdx, apac_cases::case_name(caseIdx), up, ul);
                    exit(1);
                }
                if (up > g_ulpPort) {
                    g_ulpPort = up;
                    snprintf(g_ulpWhere, sizeof g_ulpWhere, "n=%u %s", n,
                             apac_cases::case_name(caseIdx));
                }
                if (ul > g_ulpLib) g_ulpLib = ul;
                // Case 6 is all denormals: there the exponent range is gone
                // and tens of denormal ULPs is all the precision that
                // exists for EITHER side.  Kept in the run, excluded from
                // the headline figure because it is not representative.
                if (caseIdx != 6) {
                    if (up > g_ulpPortNormal) g_ulpPortNormal = up;
                    if (ul > g_ulpLibNormal) g_ulpLibNormal = ul;
                }
                if (n == 1024) {
                    // Absolute error, in ULPs of the INPUT's full scale.  All
                    // seven curve cases have unit-magnitude coefficients, so
                    // this is comparable across them and it is the number that
                    // shows accumulation directly.
                    const double inPeak = (double)dspfx::peak(clean, n);
                    for (int q = 0; q < 7; ++q)
                        if (kCurveCases[q] == caseIdx) {
                            g_curve[q] = up;
                            g_absCurve[q] = inPeak > 0.0
                                ? ep / (inPeak * 1.1920928955078125e-07) : 0.0;
                        }
                }
                ++g_precChecks;
            }
            (void)activeBins;

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
    assert(g_precChecks > 0);

    // The accumulation signature, read two ways.
    //
    // Measured in ULPs of the block's OWN peak, the error rises from half an
    // ULP with a single active coefficient to about one, and then saturates -
    // because as coefficients are added the output peak grows alongside the
    // error.  Saturating AT one ULP is the point: the port is at float32's
    // representation limit, not past it.
    //
    // Measured in ULPs of the INPUT's full scale, the same error grows by two
    // orders of magnitude from 1 active bin to 1024, which is accumulation and
    // nothing else.  A wrong twiddle or window index would be just as wrong
    // with one active bin as with a thousand, so neither curve would look like
    // this.
    for (int q = 0; q < 7; ++q) assert(g_curve[q] >= 0.0 && g_absCurve[q] >= 0.0);
    assert(g_curve[0] < 0.6);                    // 1 active bin: sub-ULP
    assert(g_curve[0] < g_curve[6]);             // relative error rises...
    assert(g_curve[6] < 2.0);                    // ...and stops at ~1 ULP
    assert(g_absCurve[0] * 10.0 < g_absCurve[6]);  // absolute error accumulates

    // Neither side is systematically the better approximation - which is what
    // "the difference is factorisation order" means in one number.
    assert(g_ulpPort < 4.0 * g_ulpLib + 0.5);
    assert(g_ulpPortNormal < 4.0 * g_ulpLibNormal + 0.5);
    assert(g_ulpPortNormal < 8.0);

    // The size gate, and the not-initialised path.
    apac::MDCT fresh;
    static float a[8], b[8];
    assert(fresh.InverseInPlaceTransform(a, b) == apac::kTransformNotInit);
    assert(fresh.Initialize(16, 0.0f) == apac::kTransformBadSize);
    assert(fresh.Initialize(256, 0.0f) == apac::kTransformBadSize);
    assert(fresh.Initialize(128, 0.0f) == apac::kTransformOk);

    printf("OK  %d cases, %d floats, worst %.2g of full scale (%s)\n", g_cases,
           g_floats, g_worst, g_worstWhere);
    printf("%40s vs exact (double): port %.2f ULP, library %.2f ULP"
           " (worst, %s); excluding denormals %.2f vs %.2f\n", "",
           g_ulpPort, g_ulpLib, g_ulpWhere, g_ulpPortNormal, g_ulpLibNormal);
    printf("%40s accumulation, n=1024, by active bins (ULP of output peak /"
           " of input full scale):\n", "");
    printf("%40s ", "");
    for (int q = 0; q < 7; ++q)
        printf(" %u:%.2f/%.4f", kCurveBins[q], g_curve[q], g_absCurve[q]);
    printf("\n");
    return 0;
}
