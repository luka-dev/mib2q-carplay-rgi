/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// Differential test: our APACScaleFactorData::Deserialize against the real one.
//
// FIXTURE: tests/fixtures/scalefactors.fixture.  Each R line is what
// APACScaleFactorData::Deserialize at 0x0009a140 returned inside the running
// library, plus every byte it wrote into a zeroed 728-byte object and the exact
// number of bits it consumed.
//
//   regenerate:  apac_decoder/tools/gen_fixtures.sh     (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `scf`
//   cases:       apac_decoder/tools/gen_scf_cases.py
//
// The scalefactors are compared as BIT PATTERNS, not as floats: the original
// builds them by adding an exponent into the float's bit pattern rather than
// with a pow, so anything short of bit-exact is a port that got the exponent
// arithmetic wrong.
//
// rc is the original's convention: 1 means ERROR.

#include "apac/spectrum.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t unhex(const char* s, unsigned char* out, size_t cap)
{
    if (!strcmp(s, "-")) return 0;
    size_t n = strlen(s) / 2;
    assert(n <= cap);
    for (size_t i = 0; i < n; ++i)
        out[i] = (unsigned char)((hexval(s[2 * i]) << 4) | hexval(s[2 * i + 1]));
    return n;
}

long g_idx;
void eq(const char* what, long got, long want)
{
    if (got == want) return;
    fprintf(stderr, "\nMISMATCH case %ld: %s got %ld, library says %ld\n",
            g_idx, what, got, want);
    exit(1);
}

char g_line[1 << 16];
char g_hex[1 << 16];

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/scalefactors.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    static unsigned char buf[1 << 15];
    size_t buflen = 0;
    long caseIdx = -1;
    unsigned ws = 0, maxSfb = 0, groups = 0;

    int cases = 0, errors = 0, shorts = 0, clampHi = 0, clampLo = 0;
    long scfChecked = 0, recChecked = 0;

    while (fgets(g_line, sizeof g_line, f)) {
        if (g_line[0] == '#' || g_line[0] == '\n') continue;

        if (!strncmp(g_line, "SCF ", 4)) {
            int got = sscanf(g_line, "SCF %ld %u %u %u %65535s",
                             &caseIdx, &ws, &maxSfb, &groups, g_hex);
            assert(got == 5);
            buflen = unhex(g_hex, buf, sizeof buf);
            continue;
        }
        if (strncmp(g_line, "R ", 2)) continue;
        assert(caseIdx >= 0);

        long idx; int rc, bitsUsed;
        char nrecHex[64], recHex[1024], scfHex[2048];
        int got = sscanf(g_line, "R %ld %d %d %63s %1023s %2047s",
                         &idx, &rc, &bitsUsed, nrecHex, recHex, scfHex);
        assert(got == 6 && idx == caseIdx);
        g_idx = idx;

        unsigned char wantNrec[8], wantRec[256], wantScf[512];
        size_t nRec = unhex(recHex, wantRec, sizeof wantRec);
        size_t nScf = unhex(scfHex, wantScf, sizeof wantScf);
        assert(unhex(nrecHex, wantNrec, sizeof wantNrec) == 8);
        assert(nRec == 2 * 15 * (groups > 8 ? 8 : groups));
        assert(nScf == 4 * 15 * (groups > 8 ? 8 : groups));

        apac::APACICSInfo ics;
        ics.Clear();
        ics.windowSequence  = (uint8_t)ws;
        ics.maxSfb          = (uint8_t)maxSfb;
        ics.numWindowGroups = (uint8_t)groups;

        apac::BitstreamReader bs(buf, (uint32_t)buflen);
        apac::ScaleFactorData sf;
        sf.Clear();
        int32_t before = bs.BitsRemaining();
        bool err = sf.Deserialize(bs, ics);
        int32_t used = before - bs.BitsRemaining();

        eq("return code",   err ? 1 : 0, rc);
        eq("bits consumed", used,        bitsUsed);
        for (int i = 0; i < 8; ++i) {
            char what[32];
            snprintf(what, sizeof what, "numRecords[%d]", i);
            eq(what, sf.numRecords[i], wantNrec[i]);
        }
        if (memcmp(sf.records, wantRec, nRec) != 0) {
            for (size_t i = 0; i < nRec / 2; ++i)
                if (memcmp(&sf.records[i], wantRec + 2 * i, 2) != 0) {
                    fprintf(stderr, "\nMISMATCH case %ld: records[%u] got %04x, "
                            "library says %02x%02x\n", g_idx, (unsigned)i,
                            sf.records[i], wantRec[2 * i + 1], wantRec[2 * i]);
                    return 1;
                }
        }
        if (memcmp(sf.scf, wantScf, nScf) != 0) {
            for (size_t i = 0; i < nScf / 4; ++i)
                if (memcmp(&sf.scf[i], wantScf + 4 * i, 4) != 0) {
                    uint32_t a, b;
                    memcpy(&a, &sf.scf[i], 4);
                    memcpy(&b, wantScf + 4 * i, 4);
                    fprintf(stderr, "\nMISMATCH case %ld: scf[%u] bits %08x (%g), "
                            "library says %08x\n", g_idx, (unsigned)i, a,
                            (double)sf.scf[i], b);
                    return 1;
                }
        }
        recChecked += (long)(nRec / 2);
        scfChecked += (long)(nScf / 4);

        // Coverage: the two precomputed out-of-range constants, 2^-89 and
        // 2^38.75.  A case list that never drove the running value out of
        // [-256, 255] would leave both clamp branches untested.
        for (size_t i = 0; i < nScf / 4; ++i) {
            uint32_t b;
            memcpy(&b, wantScf + 4 * i, 4);
            if (b == 0x13000000u) ++clampLo;
            if (b == 0x52D744FDu) ++clampHi;
        }

        ++cases;
        if (rc) ++errors;
        if (ws == 2) ++shorts;
        caseIdx = -1;
    }
    fclose(f);

    // A fixture that had silently emptied, or that only ever hit one branch,
    // would make this test meaningless.
    assert(cases > 400);
    assert(errors > 100);
    assert(shorts > 100);
    assert(clampLo > 0 && clampHi > 0);

    printf("OK  %d cases (%d rejected, %d eight-short), %ld records and %ld "
           "scalefactors bit-exact, both clamp constants reached\n",
           cases, errors, shorts, recChecked, scfChecked);
    return 0;
}
