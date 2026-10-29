/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// Differential test: our TBitstreamReader against the real one.
//
// FIXTURE: tests/fixtures/bitreader.fixture.  Every `R` line in it is what
// Pioneer's copy of Apple's TBitstreamReader<unsigned int> ACTUALLY returned
// and what its 24 bytes of state ACTUALLY held afterwards, captured by running
// the library natively in the arm64 container.  Nothing in it is hand written.
//
//   regenerate:  apac_decoder/tools/gen_fixtures.sh     (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `bs`
//
// The test replays each case on the host and demands BIT-EXACT agreement on the
// returned value and on all three mutable state fields.  It covers the three
// traps that make this reader unlike a naive one:
//
//   * the refill is a big-endian 32-bit load;
//   * a short tail pads with 0xFF, not zero  (the len 1..3, 5..7 cases);
//   * the constructor pre-aligns an unaligned buffer (the align 1..3 cases).
//
// The buffer is placed at (align & 3) inside a slab whose leading 256 bytes are
// zero, exactly as the oracle does, because PutBack reads behind the buffer.

#include "apac/bitstream.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

const size_t kSlabSize = 65536;
const size_t kLead     = 256;

// The oracle's slab is 64-byte aligned; only alignment mod 4 matters to the
// reader, but matching it exactly removes the question.
unsigned char g_slabStore[kSlabSize + 64];
unsigned char* slab()
{
    unsigned char* p = g_slabStore;
    while (((uintptr_t)p & 63) != 0) ++p;
    return p;
}

int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t parse_hex(const char* s, unsigned char* out, size_t cap)
{
    if (!strcmp(s, "-")) return 0;
    size_t n = strlen(s) / 2;
    assert(n <= cap);
    for (size_t i = 0; i < n; ++i)
        out[i] = (unsigned char)((hexval(s[2 * i]) << 4) | hexval(s[2 * i + 1]));
    return n;
}

struct Case {
    long   idx;
    unsigned align;
    unsigned char buf[512];
    size_t  len;
    char    ops[8192];
};

int g_cases = 0;
long g_ops = 0;

void fail(const Case& c, int opIdx, const char* op, const char* what,
          unsigned long got, unsigned long want)
{
    fprintf(stderr,
            "\nMISMATCH case %ld align %u op %d '%s': %s got %lu (0x%lx), "
            "library says %lu (0x%lx)\n",
            c.idx, c.align, opIdx, op, what, got, got, want, want);
    exit(1);
}

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/bitreader.fixture";
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }

    Case c;
    memset(&c, 0, sizeof c);
    bool haveCase = false;
    unsigned char* base = 0;
    apac::BitstreamReader* bs = 0;
    // Storage for the reader; it is reconstructed in place for every case.
    unsigned char bsStore[sizeof(apac::BitstreamReader)];
    int expectOp = 0;

    char line[65536];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "CASE ", 5)) {
            char hex[8192];
            long idx; unsigned align;
            int got = sscanf(line, "CASE %ld %u %8191s %8191s", &idx, &align, hex, c.ops);
            assert(got == 4);
            c.idx = idx;
            c.align = align;
            c.len = parse_hex(hex, c.buf, sizeof c.buf);
            memset(slab(), 0, kSlabSize);
            base = slab() + kLead + (align & 3);
            if (c.len) memcpy(base, c.buf, c.len);
            bs = new (bsStore) apac::BitstreamReader(base, (uint32_t)c.len);
            haveCase = true;
            expectOp = 0;
            ++g_cases;
            continue;
        }

        if (strncmp(line, "R ", 2) != 0) continue;
        assert(haveCase);

        int opIdx; char op[64];
        long ret, curOff; unsigned cache; int bitsLeft;
        int got = sscanf(line, "R %d %63s %ld %ld %x %d",
                         &opIdx, op, &ret, &curOff, &cache, &bitsLeft);
        assert(got == 6);
        assert(opIdx == expectOp);
        ++expectOp;
        ++g_ops;

        long ours = 0;
        if (opIdx != 0) {
            switch (op[0]) {
            case 'g': ours = (long)bs->GetBits((uint32_t)strtoul(op + 1, 0, 10)); break;
            case 'b': ours = (long)bs->GetBit(); break;
            case 's': bs->SkipBits((uint32_t)strtoul(op + 1, 0, 10)); break;
            case 'a': bs->AlignToNextByte(); break;
            case 'p': bs->PutBack((uint32_t)strtoul(op + 1, 0, 10)); break;
            case 'r': ours = (long)bs->BitsRemaining(); break;
            case 'f': bs->FillCacheFrom(bs->cur); break;
            default: fprintf(stderr, "unknown op %s\n", op); return 1;
            }
        }

        if (ours != ret)                      fail(c, opIdx, op, "return value", (unsigned long)ours, (unsigned long)ret);
        if ((long)(bs->cur - base) != curOff) fail(c, opIdx, op, "cur offset", (unsigned long)(bs->cur - base), (unsigned long)curOff);
        if (bs->cache != cache)               fail(c, opIdx, op, "cache", bs->cache, cache);
        if (bs->bitsLeft != bitsLeft)         fail(c, opIdx, op, "bitsLeft", (unsigned long)bs->bitsLeft, (unsigned long)bitsLeft);
    }
    fclose(f);

    assert(g_cases > 500);
    assert(g_ops > 5000);
    printf("OK  %d cases, %ld library-checked operations, bit exact\n", g_cases, g_ops);
    return 0;
}
