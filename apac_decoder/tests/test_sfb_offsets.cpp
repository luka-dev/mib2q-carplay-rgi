// Differential test: our apac::GetSFBOffsetTable against the real one.
//
// FIXTURE: tests/fixtures/sfb_offsets.fixture, produced by calling
// apac::GetSFBOffsetTable at 0x0009d9d0 inside the running library and reading
// the returned record and both offset arrays back out.
//
//   regenerate:  apac_decoder/tools/gen_fixtures.sh     (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `sfb`
//
// It covers all 13 sample rates in the decoder's rate table (7350 Hz has no SFB
// record and must come back NULL) and the frameLength gate, which admits only
// 1024.  48 kHz - the CarPlay rate - must give 49 long bands and 14 short.

#include "apac/sfb_offsets.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int g_records = 0;
int g_offsets = 0;

void expect(bool ok, const char* what, unsigned rate, unsigned long got, unsigned long want)
{
    if (ok) return;
    fprintf(stderr, "\nMISMATCH rate %u: %s got %lu, library says %lu\n",
            rate, what, got, want);
    exit(1);
}

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/sfb_offsets.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    char line[8192];
    const apac::SFBOffsets* rec = 0;
    unsigned rate = 0, frameLength = 0;
    bool sawLong = false, sawShort = false, saw48k = false;

    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "SFB ", 4)) {
            char tail[64];
            unsigned nLong, nShort;
            if (sscanf(line, "SFB %u %u %63s", &rate, &frameLength, tail) == 3 &&
                !strcmp(tail, "NULL")) {
                rec = apac::GetSFBOffsetTable(rate, frameLength);
                expect(rec == 0, "record", rate, (unsigned long)(uintptr_t)rec, 0);
                ++g_records;
                continue;
            }
            int got = sscanf(line, "SFB %u %u %u %u", &rate, &frameLength, &nLong, &nShort);
            assert(got == 4);
            rec = apac::GetSFBOffsetTable(rate, frameLength);
            expect(rec != 0, "record present", rate, 0, 1);
            expect(rec->numSfbLong == nLong, "numSfbLong", rate, rec->numSfbLong, nLong);
            expect(rec->numSfbShort == nShort, "numSfbShort", rate, rec->numSfbShort, nShort);
            expect(rec->sampleRate == rate, "sampleRate", rate, rec->sampleRate, rate);
            ++g_records;
            if (rate == 48000 && frameLength == 1024) saw48k = true;
            continue;
        }

        bool isLong = !strncmp(line, "LONG", 4);
        if (!isLong && strncmp(line, "SHORT", 5) != 0) continue;
        assert(rec != 0);
        const uint16_t* ours = isLong ? rec->sfbOffsetLong : rec->sfbOffsetShort;
        uint32_t n = (isLong ? rec->numSfbLong : rec->numSfbShort) + 1;

        char* p = line + (isLong ? 4 : 5);
        for (uint32_t i = 0; i < n; ++i) {
            char* endp = 0;
            unsigned long want = strtoul(p, &endp, 10);
            expect(endp != p, "offset field present", rate, i, n);
            p = endp;
            expect(ours[i] == want, isLong ? "long offset" : "short offset",
                   rate, ours[i], want);
            ++g_offsets;
        }
        // Nothing but whitespace may be left: an array longer in the library
        // than here would be a truncation bug in our tables.
        while (*p == ' ' || *p == '\t') ++p;
        expect(*p == '\n' || *p == 0, isLong ? "long array length" : "short array length",
               rate, 1, 0);
        if (isLong) sawLong = true; else sawShort = true;
    }
    fclose(f);

    assert(sawLong && sawShort && saw48k);
    assert(g_records >= 15);       // 13 rates + the two frameLength rejections

    // The CarPlay case, pinned explicitly so a silently emptied fixture cannot
    // let this test pass.
    const apac::SFBOffsets* r = apac::GetSFBOffsetTable(48000, 1024);
    assert(r && r->numSfbLong == 49 && r->numSfbShort == 14);
    assert(r->sfbOffsetLong[0] == 0 && r->sfbOffsetLong[49] == 1024);
    assert(r->sfbOffsetShort[0] == 0 && r->sfbOffsetShort[14] == 128);
    assert(apac::GetSFBOffsetTable(48000, 128) == 0);
    assert(apac::GetSFBOffsetTable(7350, 1024) == 0);
    assert(apac::SampleRateForIndex(3) == 48000);
    assert(apac::SampleRateForIndex(13) == 0);

    printf("OK  %d records, %d band offsets, all 12 rates long and short\n",
           g_records, g_offsets);
    return 0;
}
