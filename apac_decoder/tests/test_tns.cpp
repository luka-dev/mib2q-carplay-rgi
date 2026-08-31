// Differential test: apac::TNSData against APACTNSData in the real library -
// the constructor's rate table, ParseTNSData, and Apply.
//
// FIXTURE: tests/fixtures/tns.fixture, produced by calling
// APACTNSData::APACTNSData (0xaed10), ParseTNSData (0xaedc0) and Apply
// (0xaf240) inside the running library.
//
//   regenerate:  apac_decoder/tools/gen_fixtures_transform.sh   (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `tns`
//   inputs:      apac_decoder/reference/dsp_cases.h, shared with the oracle
//
// The three checks, in order of how much they pin down:
//
//   1. The constructor over all 13 sample rates plus two the table does not
//      have.  This is where 12 / 7 (maximum filter order) and the per-rate
//      band caps come from - 40 long / 14 short at 48 kHz.
//   2. ParseTNSData: the ENTIRE 190-byte object, byte for byte, plus the exact
//      bit count consumed and the return code.  That is the strongest form
//      available, because the object is the parser's whole output and its
//      layout is the original's own - the filter records really do sit at
//      +2 + 23*i.  Both block layouts are covered: a long block's records are
//      indexed by filter, a short block's by WINDOW, and `nFilt` is a count in
//      one case and an 8-bit per-window mask (window 0 in bit 7) in the other.
//   3. Apply on the object the parser just produced, over a 1024-coefficient
//      spectrum.  This one comes out BIT EXACT - worst difference 0 over all
//      32 cases, 24 of which actually filter something - and it should: unlike
//      the MDCT there is no FFT in it, so the parcor-to-LPC recursion and the
//      all-pole filter do the same float multiplies in the same order as the
//      original.  Anything else here would be a real divergence, not rounding.
//
// The parse cases include ones the library REJECTS (a filter whose band length
// is zero).  Those still leave a partly filled object and Apply still runs on
// it; both sides see the same bytes, so the comparison is real.

#include "apac/tns.h"

#include "apac/sfb_offsets.h"
#include "../reference/dsp_cases.h"
#include "dsp_fixture.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

float g_spec[1024];
float g_want[1024];

int g_cfg = 0;
int g_parses = 0;
int g_rejects = 0;
int g_applies = 0;
int g_shortCases = 0;
int g_filtered = 0;      // cases where Apply actually changed the spectrum
double g_worst = 0.0;

int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

int main(int argc, char** argv)
{
    // The object must be the original's 190 bytes: Apply and ParseTNSData both
    // address the filter records as `this + 2 + 23*i`.
    assert(sizeof(apac::TNSFilter) == 23);
    assert(sizeof(apac::TNSData) == 190);

    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/tns.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    const apac::SFBOffsets* rec = apac::GetSFBOffsetTable(48000, 1024);
    assert(rec != 0);

    char line[65536];
    apac::TNSData tns;
    apac::APACICSInfo ics;
    unsigned caseIdx = 0;
    uint32_t got = 0;
    bool collecting = false;
    bool eightShort = false;
    float inScale = 0.0f;

    memset(&tns, 0, sizeof tns);
    memset(&ics, 0, sizeof ics);

    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "CFG ", 4)) {
            unsigned rate, ol, os, bl, bs2;
            if (sscanf(line, "CFG %u maxOrderLong=%u maxOrderShort=%u "
                             "bandsLong=%u bandsShort=%u",
                       &rate, &ol, &os, &bl, &bs2) != 5) {
                fprintf(stderr, "bad CFG line: %s", line);
                return 1;
            }
            apac::TNSData t;
            t.Configure(rate);
            if (t.maxOrderLong != ol || t.maxOrderShort != os ||
                t.maxBandsLong != bl || t.maxBandsShort != bs2) {
                fprintf(stderr,
                        "\nMISMATCH Configure(%u): port %u/%u/%u/%u, library %u/%u/%u/%u\n",
                        rate, t.maxOrderLong, t.maxOrderShort,
                        t.maxBandsLong, t.maxBandsShort, ol, os, bl, bs2);
                return 1;
            }
            ++g_cfg;
            continue;
        }

        if (!strncmp(line, "TNS ", 4)) {
            unsigned c, seq, maxSfb;
            int rc, bits;
            if (sscanf(line, "TNS %u seq=%u maxSfb=%u rc=%d bits=%d",
                       &c, &seq, &maxSfb, &rc, &bits) != 5) {
                fprintf(stderr, "bad TNS line: %s", line);
                return 1;
            }
            const apac_cases::TNSCase& tc = apac_cases::tns_plan()[c];
            assert(tc.windowSequence == seq && tc.maxSfb == maxSfb);
            caseIdx = c;
            eightShort = (seq == 2);
            if (eightShort) ++g_shortCases;

            memset(&tns, 0, sizeof tns);
            tns.Configure(48000);
            memset(&ics, 0, sizeof ics);
            ics.maxSfbLong  = (uint8_t)rec->numSfbLong;
            ics.maxSfbShort = (uint8_t)rec->numSfbShort;
            ics.windowSequence = (uint8_t)seq;
            ics.maxSfb = (uint8_t)maxSfb;

            static uint8_t bitbuf[apac_cases::kTNSBitBytes + 32];
            memset(bitbuf, 0, sizeof bitbuf);
            apac_cases::tns_fill_bits(c, bitbuf, apac_cases::kTNSBitBytes);
            apac::BitstreamReader br(bitbuf, apac_cases::kTNSBitBytes);
            int32_t before = br.BitsRemaining();
            bool err = tns.ParseTNSData(br, ics);
            int32_t used = before - br.BitsRemaining();

            const int ourRc = err ? -1 : 0;
            if (ourRc != rc || used != bits) {
                fprintf(stderr,
                        "\nMISMATCH ParseTNSData case %u: port rc=%d bits=%d, "
                        "library rc=%d bits=%d\n", c, ourRc, used, rc, bits);
                return 1;
            }
            ++g_parses;
            if (rc != 0) ++g_rejects;
            continue;
        }

        if (!strncmp(line, "OBJ ", 4)) {
            const unsigned char* ours = (const unsigned char*)&tns;
            const char* p = line + 4;
            for (uint32_t i = 0; i < sizeof(apac::TNSData); ++i) {
                int hi = hexval(p[2 * i]), lo = hexval(p[2 * i + 1]);
                if (hi < 0 || lo < 0) {
                    fprintf(stderr, "\nOBJ line too short at byte %u\n", i);
                    return 1;
                }
                // Byte 0 is the caller-set `present` flag, which ParseTNSData
                // does not touch and the harness sets afterwards.
                const unsigned want = (unsigned)((hi << 4) | lo);
                if (i != 0 && ours[i] != want) {
                    fprintf(stderr,
                            "\nMISMATCH ParseTNSData case %u byte %u: port %02x, "
                            "library %02x\n", caseIdx, i, ours[i], want);
                    return 1;
                }
            }
            // Now hand Apply the same object the library was handed.
            tns.present = 1;
            apac_cases::case_fill(apac_cases::tns_plan()[caseIdx].spectrumCase,
                                  g_spec, 1024);
            inScale = dspfx::peak(g_spec, 1024);
            apac::SFBBandOffsets so;
            so.numBands = eightShort ? rec->numSfbShort : rec->numSfbLong;
            so.offset   = eightShort ? rec->sfbOffsetShort : rec->sfbOffsetLong;
            apac::SpectralData sd;
            sd.begin = g_spec;
            sd.end = g_spec + 1024;
            static float before[1024];
            memcpy(before, g_spec, sizeof before);
            tns.Apply(ics, so, sd);
            if (memcmp(before, g_spec, sizeof before) != 0) ++g_filtered;
            continue;
        }

        if (!strncmp(line, "APPLY ", 6)) { got = 0; collecting = false; continue; }
        if (!strncmp(line, "SPEC", 4))   { collecting = true; got = 0; continue; }

        if (collecting) {
            got += dspfx::parse_hex_floats(line, g_want, 1024, got);
            if (got == 1024) {
                char what[96];
                snprintf(what, sizeof what, "TNS::Apply case %u", caseIdx);
                double w = dspfx::compare(g_spec, g_want, 1024, what, inScale);
                if (w > g_worst) g_worst = w;
                ++g_applies;
                collecting = false;
            }
        }
    }
    fclose(f);

    assert(g_cfg == 14);                                  // 13 rates + one unknown
    assert(g_parses == apac_cases::kTNSCases);
    assert(g_applies == apac_cases::kTNSCases);
    assert(g_rejects > 0);                                // the zero-length path
    assert(g_shortCases > 0 && g_shortCases < g_parses);  // both block layouts
    // A test where Apply were a no-op everywhere would pass vacuously.
    assert(g_filtered >= g_applies / 2);

    // Not present means not applied, whatever the filters say.
    apac::TNSData quiet;
    memset(&quiet, 0, sizeof quiet);
    quiet.Configure(48000);
    quiet.nFilt = 1;
    quiet.filter[0].length = 10;
    quiet.filter[0].order = 4;
    static float untouched[1024];
    apac_cases::case_fill(9, untouched, 1024);
    static float copy[1024];
    memcpy(copy, untouched, sizeof copy);
    apac::SFBBandOffsets so;
    so.numBands = rec->numSfbLong;
    so.offset = rec->sfbOffsetLong;
    apac::SpectralData sd;
    sd.begin = untouched;
    sd.end = untouched + 1024;
    apac::APACICSInfo li;
    memset(&li, 0, sizeof li);
    li.maxSfb = (uint8_t)rec->numSfbLong;
    quiet.Apply(li, so, sd);
    assert(memcmp(copy, untouched, sizeof copy) == 0);

    printf("OK  %d rates, %d parses (%d rejected, %d short), %d applies "
           "(%d filtered), worst %.2g\n",
           g_cfg, g_parses, g_rejects, g_shortCases, g_applies,
           g_filtered, g_worst);
    return 0;
}
