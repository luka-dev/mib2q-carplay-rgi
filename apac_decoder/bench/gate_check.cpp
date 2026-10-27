/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// gate_check - which BWE frame cases do no work, and does the LIBRARY agree?
//
// The timing pass in apac_bench classifies a case as "gated" observationally:
// the spectrum comes back from DecodeFrame bit-identical to what went in, so
// the call cost nothing worth counting.  That found 15 of 44.  Stage e's author
// classifies 11 structurally - cases built to hit the `find_if` guard or a
// maxSfb of zero.  The two criteria are not the same, so the difference is a
// set of cases that REACH the synthesis and leave the spectrum unchanged.
//
// That difference is worth resolving rather than reconciling, because the two
// explanations are not equally comfortable:
//
//   * the synthesis genuinely writes back what was already there for those
//     geometries - benign, and worth a comment; or
//   * it runs and produces nothing, which every existing test would miss.
//     tests/test_bwe.cpp compares our decoder against the library, so if the
//     library does the same nothing, our decoder agrees with it perfectly while
//     both do nothing useful.  That is the shape of defect that survives a
//     hundred thousand bit-exact checks.
//
// This needs no oracle run to tell them apart.  tests/fixtures/bwe_frame.fixture
// already holds the LIBRARY's own output spectrum for every case, produced by
// reference/apac_bwe_ref.cpp against the real library.  So for each case:
//
//   library wrote nothing  <=>  fixture spectrum == the case's input spectrum
//
// and our answer is measured here the same way.  Agreement on "nothing"
// is a classification difference; disagreement is a bug.
//
// WHAT THIS CHECK IS AND IS NOT.  It is a NECESSARY condition, not a sufficient
// one.  "The fixture spectrum equals the input" cannot separate *the library
// gated* from *the library ran and wrote zeros* - both leave the buffer
// untouched - so on the four cases that run everything and write zeros (4, 5,
// 26, 30) it establishes that the two implementations produced the same bytes,
// not that they took the same path.
//
// That gap is closed independently, from the disassembly rather than from this
// fixture (apac-bwe2): the library's long guard at 0x78490 is
// `sub_78370(spec, spec + bwStart)`, a find-first-non-zero over the WHOLE
// [0, bwStart) rather than over the replicated slice, and it proceeds when one
// is found.  Cases 4 and 5 have content at [0, 48) and [0, 4), both inside
// [0, 256), so the library proceeds too; likewise the short scan for 26 and
// 30.  So the library runs and writes zeros exactly where our decoder does.
//
// The complementary check lives in tests/test_bwe.cpp, which asserts bit-exactly
// that every coefficient OUTSIDE the predicted extension band is unchanged on
// both sides.  A decoder that gated where the library ran would show up there as
// the library having written into a region our decoder left alone.  This one
// catches the quiet-versus-quiet case that one cannot express; that one catches
// the wrote-versus-did-not case this one cannot.
//
//   gate_check [FIXTURE_DIR]        (default apac_decoder/tests/fixtures)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "apac/config.h"
#include "apac/bwe.h"
#include "apac/bwe_lpc.h"
#include "apac/element.h"
#include "apac/ics_info.h"
#include "apac/sfb_offsets.h"

static uint32_t BweCoreEdge(uint32_t rate, uint32_t seq, uint32_t maxSfb);
#include "../reference/bwe_cases.h"

static uint32_t BweCoreEdge(uint32_t rate, uint32_t seq, uint32_t maxSfb)
{
    const apac::SFBOffsets* t = apac::GetSFBOffsetTable(rate, 1024);
    if (!t) { fprintf(stderr, "no SFB record for %u Hz\n", rate); exit(1); }
    if (seq == 2) return 8u * (uint32_t)t->sfbOffsetShort[maxSfb];
    return (uint32_t)t->sfbOffsetLong[maxSfb];
}

static apac::BWELPC      gLPC;
static apac::BWE2Decoder gBWE;
static apac::SyntacticElement gElem;
static BweFrameCase gCase[kBweFrameCases];
static float gSpec[kBweFrameLength];
static float gLib[kBweFrameLength];

static FILE* gFh;
static char  gLine[8192];

static const char* nextLine(void)
{
    while (fgets(gLine, sizeof gLine, gFh)) {
        if (gLine[0] == '#' || gLine[0] == '\n' || gLine[0] == '\r') continue;
        return gLine;
    }
    return 0;
}

// Floats are printed as big-endian IEEE-754 bit patterns, one block per tag.
static void readFloats(const char* tag, float* out, uint32_t n)
{
    const char* l = nextLine();
    if (!l || strncmp(l, tag, strlen(tag)) != 0) {
        fprintf(stderr, "fixture: expected %s, got %s", tag, l ? l : "<eof>\n");
        exit(1);
    }
    uint32_t got = 0;
    while (got < n) {
        l = nextLine();
        if (!l) { fprintf(stderr, "fixture: short float block\n"); exit(1); }
        const char* p = l;
        while (*p && got < n) {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
            if (!*p) break;
            unsigned bits = 0;
            if (sscanf(p, "%8x", &bits) != 1) break;
            memcpy(&out[got++], &bits, 4);
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
        }
    }
}

int main(int argc, char** argv)
{
    const char* dir = (argc > 1) ? argv[1] : "apac_decoder/tests/fixtures";
    char path[512];
    snprintf(path, sizeof path, "%s/bwe_frame.fixture", dir);
    gFh = fopen(path, "r");
    if (!gFh) { fprintf(stderr, "cannot open %s\n", path); return 2; }

    BweBuildFrameCases(gCase);

    const char* hdr = nextLine();
    int n = 0;
    if (!hdr || sscanf(hdr, "frame cases %d", &n) != 1 || n != kBweFrameCases) {
        fprintf(stderr, "bwe_frame.fixture header: %s", hdr ? hdr : "<eof>\n");
        return 2;
    }

    int portQuiet = 0, libQuiet = 0, both = 0, disagree = 0;
    printf("case  rate  seq maxSfb coreEnd   ours     library\n");
    for (int i = 0; i < kBweFrameCases; ++i) {
        const BweFrameCase& c = gCase[i];
        char tag[64];
        snprintf(tag, sizeof tag, "case %d rc %u ch %u", i, 0u, c.channel);
        const char* l = nextLine();
        if (!l || strncmp(l, tag, strlen(tag)) != 0) {
            fprintf(stderr, "fixture: expected '%s', got %s", tag, l ? l : "<eof>\n");
            return 2;
        }
        readFloats("spec", gLib, kBweFrameLength);

        // The library's answer, against the same input the case fed it.
        const bool libWrote =
            memcmp(gLib, c.spec, sizeof(float) * kBweFrameLength) != 0;

        // Ours, run exactly as apac_bench times it.
        const apac::SFBOffsets* table = apac::GetSFBOffsetTable(c.sampleRate, 1024);
        if (!table) { fprintf(stderr, "no table, case %d\n", i); return 2; }
        if (gElem.Configure(apac::kElementSCE, table, c.sampleRate) != apac::kOk) {
            fprintf(stderr, "Configure failed, case %d\n", i); return 2;
        }
        apac::APACICSInfo& ics = gElem.GetICSInfo(true);
        ics.windowSequence = (uint8_t)c.windowSequence;
        ics.maxSfb = (uint8_t)c.maxSfb;
        ics.windowGroupWord = c.windowGroupWord;
        uint32_t groups = 0;
        for (uint32_t g = 0; g < 8; ++g)
            if (((c.windowGroupWord >> (4 * g)) & 0xFu) != 0) ++groups;
        ics.numWindowGroups = (uint8_t)groups;

        uint8_t bits[64];
        uint32_t pos = 0;
        memset(bits, 0, sizeof bits);
        struct P {
            static void put(uint8_t* b, uint32_t* at, uint32_t v, uint32_t nb) {
                for (int k = (int)nb - 1; k >= 0; --k) {
                    if ((v >> k) & 1u) b[*at >> 3] |= (uint8_t)(0x80u >> (*at & 7));
                    ++*at;
                }
            }
        };
        P::put(bits, &pos, 1, 1);
        P::put(bits, &pos, c.env[0], 9);
        P::put(bits, &pos, c.env[1], 9);
        for (uint32_t g = 0; g < groups; ++g) P::put(bits, &pos, c.scale[g], 6);

        gBWE.Initialize(table, &gLPC);
        apac::BitstreamReader bs(bits, sizeof bits);
        bool portWrote = false;
        const char* why = "";
        if (gBWE.Deserialize(bs, c.channel, gElem) != apac::kOk) {
            why = "header refused";
        } else if (!gBWE.GetActiveFlag(c.channel)) {
            why = "flag clear";
        } else {
            memcpy(gSpec, c.spec, sizeof gSpec);
            if (gBWE.DecodeFrame(ics, gSpec, c.channel) != apac::kOk) {
                why = "DecodeFrame refused";
            } else {
                portWrote = memcmp(gSpec, c.spec, sizeof gSpec) != 0;
                why = portWrote ? "" : "ran, wrote nothing";
            }
        }

        const uint32_t edge = BweCoreEdge(c.sampleRate, c.windowSequence, c.maxSfb);
        if (!portWrote) ++portQuiet;
        if (!libWrote)  ++libQuiet;
        if (!portWrote && !libWrote) ++both;
        if (portWrote != libWrote) ++disagree;

        if (!portWrote || !libWrote)
            printf("%4d %6u %4u %6u %7u   %-7s %-7s  %s\n", i, c.sampleRate,
                   c.windowSequence, c.maxSfb, edge,
                   portWrote ? "wrote" : "quiet",
                   libWrote ? "wrote" : "quiet", why);
    }
    fclose(gFh);

    printf("\nport quiet on %d of %d, library quiet on %d, both quiet on %d\n",
           portQuiet, kBweFrameCases, libQuiet, both);
    if (disagree) {
        printf("DISAGREE on %d case(s): one of them writes where the other does "
               "not.  That is a defect, not a classification difference.\n", disagree);
        return 1;
    }
    printf("no disagreement: wherever our decoder writes nothing, the library "
           "writes nothing too, and vice versa.\n");
    return 0;
}
