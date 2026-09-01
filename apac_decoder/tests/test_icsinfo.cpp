/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// Differential test: our APACICSInfo::Deserialize against the real one.
//
// FIXTURE: tests/fixtures/icsinfo.fixture.  Each R line is what
// APACICSInfo::Deserialize at 0x00099e60 returned inside the running library,
// and what it left in the six fields it touches - called on a zeroed 768-byte
// object whose +8 / +9 (maxSfbLong / maxSfbShort) came from the case line,
// because those two are inputs the syntactic element fills from the SFB table.
//
//   regenerate:  apac_decoder/tools/gen_fixtures.sh     (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `ics`
//
// The cases sweep every window sequence against every value of the 6-bit maxSfb
// field, both values of the codingType shortcut, and nine (maxSfbLong,
// maxSfbShort) limit pairs including 48 kHz's 49 and 14.  The bit count is
// compared too, so a branch that reads the wrong width fails even when it
// happens to land on the same field values.
//
// rc is the original's convention: TRUE means error.

#include "apac/ics_info.h"

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

struct Case {
    long idx;
    unsigned maxLong, maxShort;
    int coding;
    unsigned char buf[512];
    size_t len;
};

int g_cases = 0, g_errors = 0, g_shorts = 0;

void eq(const Case& c, const char* what, long got, long want)
{
    if (got == want) return;
    fprintf(stderr,
            "\nMISMATCH case %ld (maxSfbLong %u, maxSfbShort %u, codingType %d): "
            "%s got %ld, library says %ld\n",
            c.idx, c.maxLong, c.maxShort, c.coding, what, got, want);
    exit(1);
}

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/icsinfo.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    Case c;
    memset(&c, 0, sizeof c);
    bool have = false;
    char line[8192];

    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "ICS ", 4)) {
            char hex[4096];
            int got = sscanf(line, "ICS %ld %u %u %d %4095s",
                             &c.idx, &c.maxLong, &c.maxShort, &c.coding, hex);
            assert(got == 5);
            c.len = strcmp(hex, "-") ? strlen(hex) / 2 : 0;
            assert(c.len <= sizeof c.buf);
            for (size_t i = 0; i < c.len; ++i)
                c.buf[i] = (unsigned char)((hexval(hex[2 * i]) << 4) | hexval(hex[2 * i + 1]));
            have = true;
            continue;
        }
        if (strncmp(line, "R ", 2)) continue;
        assert(have);

        long idx; unsigned rc, ws, maxSfb, groups, word; int bitsUsed;
        int got = sscanf(line, "R %ld %u %u %u %u %u %d",
                         &idx, &rc, &ws, &maxSfb, &groups, &word, &bitsUsed);
        assert(got == 7 && idx == c.idx);

        apac::BitstreamReader bs(c.buf, (uint32_t)c.len);
        apac::APACICSInfo ics;
        ics.Clear();
        ics.maxSfbLong  = (uint8_t)c.maxLong;
        ics.maxSfbShort = (uint8_t)c.maxShort;
        int32_t before = bs.BitsRemaining();
        bool err = ics.Deserialize(bs, c.coding);
        int32_t used = before - bs.BitsRemaining();

        eq(c, "return code",       err ? 1 : 0,          (long)rc);
        eq(c, "windowSequence",    ics.windowSequence,   (long)ws);
        eq(c, "maxSfb",            ics.maxSfb,           (long)maxSfb);
        eq(c, "numWindowGroups",   ics.numWindowGroups,  (long)groups);
        eq(c, "windowGroupWord",   ics.windowGroupWord,  (long)word);
        eq(c, "bits consumed",     used,                 bitsUsed);

        ++g_cases;
        if (rc) ++g_errors;
        if (ws == 2) ++g_shorts;
        have = false;
    }
    fclose(f);

    // A fixture that had silently emptied, or that only ever hit one branch,
    // would make this test meaningless.
    assert(g_cases > 900);
    assert(g_errors > 50);
    assert(g_shorts > 50);

    // An eight-short block with all seven grouping bits set is one group of
    // eight windows; with none set it is eight groups of one, 0x1111111.
    // windowSequence 2, maxSfb 14, then the seven grouping bits.
    {
        const unsigned char grouped[4] = { 0xBB, 0xFF, 0x00, 0x00 };   // 1111111
        apac::BitstreamReader bs(grouped, 4);
        apac::APACICSInfo ics;
        ics.Clear();
        ics.maxSfbLong = 49; ics.maxSfbShort = 14;
        assert(!ics.Deserialize(bs, 0));
        assert(ics.windowSequence == 2 && ics.maxSfb == 14);
        assert(ics.numWindowGroups == 1 && ics.windowGroupWord == 8);
    }
    {
        const unsigned char ungrouped[4] = { 0xB8, 0x00, 0x00, 0x00 };  // 0000000
        apac::BitstreamReader bs(ungrouped, 4);
        apac::APACICSInfo ics;
        ics.Clear();
        ics.maxSfbLong = 49; ics.maxSfbShort = 14;
        assert(!ics.Deserialize(bs, 0));
        assert(ics.windowSequence == 2 && ics.maxSfb == 14);
        assert(ics.numWindowGroups == 8 && ics.windowGroupWord == 0x11111111u);
    }
    // A long block at 48 kHz: six bits of maxSfb, and 49 is the largest the
    // band table allows.
    {
        const unsigned char ok49[2] = { 0x31, 0x00 };   // windowSeq 0, maxSfb 49
        apac::BitstreamReader bs(ok49, 2);
        apac::APACICSInfo ics;
        ics.Clear();
        ics.maxSfbLong = 49; ics.maxSfbShort = 14;
        assert(!ics.Deserialize(bs, 0));
        assert(ics.windowSequence == 0 && ics.maxSfb == 49 && ics.numWindowGroups == 1);
        const unsigned char bad50[2] = { 0x32, 0x00 };
        apac::BitstreamReader bs2(bad50, 2);
        apac::APACICSInfo ics2;
        ics2.Clear();
        ics2.maxSfbLong = 49; ics2.maxSfbShort = 14;
        assert(ics2.Deserialize(bs2, 0));
    }

    printf("OK  %d cases, %d rejections, %d eight-short blocks, field and bit exact\n",
           g_cases, g_errors, g_shorts);
    return 0;
}
