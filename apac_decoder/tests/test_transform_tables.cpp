/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// Differential test: the transform path's constant tables, word for word.
//
// FIXTURE: tests/fixtures/transform_tables.fixture, produced by calling
// GetSineWindowCoefficientPtr (0x105590), GetKBDWindowCoefficientPtr
// (0x105600) and GetDCTIVModulationCoefficients (0xb33b0) inside the running
// library and dumping what each returned.
//
//   regenerate:  apac_decoder/tools/gen_fixtures_transform.sh   (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `win`
//
// This is a BIT-EXACT test, not a tolerance one.  Our tables come
// from reference/decomp/rodata/ by tools/gen_transform_tables.py and stored as
// float32 bit patterns, so every one of the 3456 words has to match exactly.
// The point of comparing against the library rather than against the .bin dumps
// is that it also checks the dumps were taken from the right addresses.
//
// It also pins the size gate.  Only n == 128 and n == 1024 resolve; everything
// else returns NULL, which is what makes "there are exactly two block sizes" a
// checked fact rather than an assumption.

#include "apac/transform.h"

#include "dsp_fixture.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int g_words = 0;
int g_tables = 0;
int g_ptrChecks = 0;

void expectPtr(bool ours, int theirs, const char* which, int n)
{
    if (ours == (theirs != 0)) { ++g_ptrChecks; return; }
    fprintf(stderr, "\nMISMATCH %s(%d): ours says %s, library says %s\n",
            which, n, ours ? "non-NULL" : "NULL", theirs ? "non-NULL" : "NULL");
    exit(1);
}

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/transform_tables.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    char line[65536];
    static float want[apac::kMaxTransform];
    const float* have = 0;
    uint32_t wantN = 0, got = 0;
    char tableName[32] = { 0 };
    int tableSize = 0;

    // Closes out whichever table was being read.
    struct Flush {
        static void run(const float* have, const float* want, uint32_t n, uint32_t got,
                        const char* name, int size, int* words, int* tables)
        {
            if (!have) return;
            if (got != n) {
                fprintf(stderr, "\nfixture gave %u words for %s %d, expected %u\n",
                        got, name, size, n);
                exit(1);
            }
            for (uint32_t i = 0; i < n; ++i) {
                if (dspfx::float_to_bits(have[i]) != dspfx::float_to_bits(want[i])) {
                    fprintf(stderr,
                            "\nMISMATCH %s %d word %u: ours %08x, library %08x\n",
                            name, size, i, dspfx::float_to_bits(have[i]),
                            dspfx::float_to_bits(want[i]));
                    exit(1);
                }
            }
            *words += (int)n;
            *tables += 1;
        }
    };

    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "PTR ", 4)) {
            Flush::run(have, want, wantN, got, tableName, tableSize, &g_words, &g_tables);
            have = 0;
            int n, s, k, d;
            if (sscanf(line, "PTR %d sine=%d kbd=%d dctiv=%d", &n, &s, &k, &d) != 4) {
                fprintf(stderr, "bad PTR line: %s", line);
                return 1;
            }
            expectPtr(apac::GetSineWindowCoefficientPtr((uint32_t)n) != 0, s,
                      "GetSineWindowCoefficientPtr", n);
            expectPtr(apac::GetKBDWindowCoefficientPtr((uint32_t)n) != 0, k,
                      "GetKBDWindowCoefficientPtr", n);
            expectPtr(apac::GetDCTIVModulationCoefficients((uint32_t)n) != 0, d,
                      "GetDCTIVModulationCoefficients", n);
            continue;
        }

        if (!strncmp(line, "TABLE ", 6)) {
            Flush::run(have, want, wantN, got, tableName, tableSize, &g_words, &g_tables);
            if (sscanf(line, "TABLE %31s %d", tableName, &tableSize) != 2) {
                fprintf(stderr, "bad TABLE line: %s", line);
                return 1;
            }
            wantN = (uint32_t)tableSize;
            got = 0;
            if      (!strcmp(tableName, "sine"))  have = apac::GetSineWindowCoefficientPtr(wantN);
            else if (!strcmp(tableName, "kbd"))   have = apac::GetKBDWindowCoefficientPtr(wantN);
            else if (!strcmp(tableName, "dctiv")) have = apac::GetDCTIVModulationCoefficients(wantN);
            else { fprintf(stderr, "unknown table %s\n", tableName); return 1; }
            assert(have != 0);
            continue;
        }

        if (have)
            got += dspfx::parse_hex_floats(line, want, apac::kMaxTransform, got);
    }
    Flush::run(have, want, wantN, got, tableName, tableSize, &g_words, &g_tables);
    fclose(f);

    assert(g_tables == 6);          // sine/kbd/dctiv x {128, 1024}
    assert(g_words == 3456);        // 2*(128+1024) + (128+1024)
    assert(g_ptrChecks == 27);      // 9 sizes x 3 lookups

    // Two properties our decoder relies on elsewhere, pinned here so a bad
    // regeneration cannot quietly remove them.
    const float* w = apac::GetSineWindowCoefficientPtr(1024);
    assert(w[0] > 0.0f && w[1023] > w[0]);              // rising half only
    // The KBD windows are monotone rising and end just short of 1: the long one
    // stops at 0.99999994, the short one reaches exactly 1.0.  RODATA.md notes
    // they are not analytic - the Kaiser alpha was never solved for - which is
    // why they are copied rather than generated.
    const float* k = apac::GetKBDWindowCoefficientPtr(1024);
    assert(k[0] > 0.0f && k[0] < 1e-3f && k[1023] > 0.9999f && k[1023] <= 1.0f);
    assert(apac::GetKBDWindowCoefficientPtr(128)[127] == 1.0f);
    assert(apac::GetSineWindowCoefficientPtr(256) == 0);

    printf("OK  %d table words bit-exact, %d tables, %d size-gate checks\n",
           g_words, g_tables, g_ptrChecks);
    return 0;
}
