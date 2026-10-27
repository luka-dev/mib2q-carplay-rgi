/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// Differential test: our APACSpectralData::Deserialize against the real one.
//
// FIXTURE: tests/fixtures/spectraldata.fixture.  Each case carries the exact
// inputs the library was handed - the ICS fields, the whole scalefactor object,
// the SFB band offsets and the bitstream - and each R/OUT pair is what
// APACSpectralData::Deserialize at 0x000ab3b0 returned and wrote.
//
//   regenerate:  apac_decoder/tools/gen_fixtures.sh     (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `spec`
//   cases:       apac_decoder/tools/gen_spec_cases.py
//
// Coefficients are compared as BIT PATTERNS.  A float compare would hide the
// sign of zero, and the sign of zero is load-bearing here: codebooks 3 and 4
// multiply their zero lanes by the scalefactor, so a negative scalefactor
// leaves -0.0 in the output, while every other book stores a literal +0.0.
//
// The case list is built so that all of the following are exercised, and the
// counters at the bottom of this file fail the test if any of them stops being:
//   - every codebook 1..11, both block sizes
//   - the all-zero-tuple flag on the odd books
//   - codebook 11's escape, including magnitudes above 1023 (the pow path)
//   - the negative wing of the i^(4/3) table
//   - every window grouping from eight groups of one to one group of eight,
//     which is what drives the short-block staging and de-interleave
//   - maxSfb 0, numWindowGroups 0, and buffers too short to finish

#include "apac/huffman.h"
#include "apac/spectrum.h"

#include <cassert>
#include <cmath>
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
    if (n > cap) { fprintf(stderr, "hex field too long (%u)\n", (unsigned)n); exit(1); }
    for (size_t i = 0; i < n; ++i)
        out[i] = (unsigned char)((hexval(s[2 * i]) << 4) | hexval(s[2 * i + 1]));
    return n;
}

char line[1 << 19];
char hex[1 << 19];

unsigned char bufBytes[1 << 16];
unsigned char offBytes[8192];
unsigned char recBytes[240];
unsigned char scfBytes[480];
unsigned char nrecBytes[8];
unsigned char wantOut[4 * 4096];

float outBuf[4096];

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/spectraldata.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    long caseIdx = -1;
    unsigned ws = 0, maxSfb = 0, groups = 0, groupWord = 0, numBands = 0, numOut = 0;
    size_t bufLen = 0, offLen = 0, recLen = 0, scfLen = 0;

    int cases = 0, shorts = 0, grouped = 0, zeroSpectrum = 0, truncated = 0;
    long floatsChecked = 0;

    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "SPEC ", 5)) {
            int got = sscanf(line, "SPEC %ld %u %u %u %u %u %u", &caseIdx, &ws,
                             &maxSfb, &groups, &groupWord, &numBands, &numOut);
            assert(got == 7);
            continue;
        }
        if (!strncmp(line, "OFF ", 4))  { sscanf(line, "OFF %524287s",  hex); offLen  = unhex(hex, offBytes,  sizeof offBytes);  continue; }
        if (!strncmp(line, "NREC ", 5)) { sscanf(line, "NREC %524287s", hex); unhex(hex, nrecBytes, sizeof nrecBytes);          continue; }
        if (!strncmp(line, "REC ", 4))  { sscanf(line, "REC %524287s",  hex); recLen  = unhex(hex, recBytes,  sizeof recBytes);  continue; }
        if (!strncmp(line, "SCF ", 4))  { sscanf(line, "SCF %524287s",  hex); scfLen  = unhex(hex, scfBytes,  sizeof scfBytes);  continue; }
        if (!strncmp(line, "BUF ", 4))  { sscanf(line, "BUF %524287s",  hex); bufLen  = unhex(hex, bufBytes,  sizeof bufBytes);  continue; }
        if (!strncmp(line, "OUT ", 4))  { sscanf(line, "OUT %524287s",  hex); unhex(hex, wantOut, sizeof wantOut);               continue; }
        if (strncmp(line, "R ", 2)) continue;

        // R comes before OUT in the fixture, so stash it and act on OUT.
        long idx; int rc, bitsUsed;
        int got = sscanf(line, "R %ld %d %d", &idx, &rc, &bitsUsed);
        assert(got == 3 && idx == caseIdx);
        if (!fgets(line, sizeof line, f)) { fprintf(stderr, "case %ld has no OUT\n", idx); return 1; }
        assert(!strncmp(line, "OUT ", 4));
        sscanf(line, "OUT %524287s", hex);
        assert(unhex(hex, wantOut, sizeof wantOut) == 4 * numOut);
        assert(offLen == 2 * (size_t)(numBands + 1));

        apac::APACICSInfo ics;
        ics.Clear();
        ics.windowSequence  = (uint8_t)ws;
        ics.maxSfb          = (uint8_t)maxSfb;
        ics.numWindowGroups = (uint8_t)groups;
        ics.windowGroupWord = groupWord;

        apac::ScaleFactorData sf;
        sf.Clear();
        memcpy(sf.numRecords, nrecBytes, 8);
        memcpy(sf.records, recBytes, recLen);
        memcpy(sf.scf,     scfBytes, scfLen);

        apac::SFBBandOffsets sfb;
        sfb.numBands = numBands;
        sfb.offset   = reinterpret_cast<const uint16_t*>(offBytes);

        for (unsigned i = 0; i < numOut; ++i) outBuf[i] = -777.0f;
        apac::SpectralData sd;
        sd.begin = outBuf;
        sd.end   = outBuf + numOut;

        apac::BitstreamReader bs(bufBytes, (uint32_t)bufLen);
        int32_t before = bs.BitsRemaining();
        bool err = sd.Deserialize(bs, ics, sf, sfb);
        int32_t used = before - bs.BitsRemaining();

        if ((err ? 1 : 0) != rc || used != bitsUsed) {
            fprintf(stderr, "\nMISMATCH case %ld: rc %d/%d, bits %d/%d\n",
                    idx, err ? 1 : 0, rc, used, bitsUsed);
            return 1;
        }
        for (unsigned i = 0; i < numOut; ++i) {
            if (memcmp(&outBuf[i], wantOut + 4 * i, 4) == 0) continue;
            uint32_t a, b;
            memcpy(&a, &outBuf[i], 4);
            memcpy(&b, wantOut + 4 * i, 4);
            fprintf(stderr, "\nMISMATCH case %ld (ws %u, maxSfb %u, groups %u, "
                    "word %08x): out[%u] bits %08x (%.9g), library says %08x\n",
                    idx, ws, maxSfb, groups, groupWord, i, a, (double)outBuf[i], b);
            return 1;
        }
        floatsChecked += numOut;

        ++cases;
        if (ws == 2) ++shorts;
        if (ws == 2 && groupWord != 0x11111111u) ++grouped;
        if (maxSfb == 0 || groups == 0) ++zeroSpectrum;
        if (bufLen < 128) ++truncated;
        caseIdx = -1;
    }
    fclose(f);

    // The i^(4/3) table's negative wing: books 1..6 index it below zero, and a
    // decoder that clamped the index to zero would still pass most of the sweep.
    // The four values are -4^(4/3) .. -1, the mirror of X[1..4].
    for (int i = 1; i <= 4; ++i) {
        assert(apac::kInverseQuant[-i] == -apac::kInverseQuant[i]);
        assert(apac::kInverseQuant[-i] < 0.0f);
    }
    assert(apac::kInverseQuant[-1] == -1.0f);
    assert(apac::kInverseQuant[0] == 0.0f);

    // A fixture that had silently emptied, or that only ever hit one branch,
    // would make this test meaningless.
    assert(cases > 350);
    assert(shorts > 40);
    assert(grouped > 25);
    assert(zeroSpectrum >= 3);
    assert(truncated > 10);

    printf("OK  %d cases (%d eight-short, %d of them grouped), %ld coefficients "
           "bit-exact incl. the sign of zero\n",
           cases, shorts, grouped, floatsChecked);
    return 0;
}
