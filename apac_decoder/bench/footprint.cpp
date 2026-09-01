/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// footprint - how much of the 1.5 MB decoder object a single packet actually
// touches.  Host only; it needs mprotect and a signal handler.
//
// WHY IT MATTERS.  The instruction count apac_bench measures on the target is
// exact, but turning it into milliseconds needs an IPC, and the honest range
// for an IPC depends on whether the working set lives in cache.  The head unit
// is an APQ8064: 16 KB of L1 data per core and 2 MB of L2 shared with
// everything else the unit is doing.  `sizeof(apac::Decoder)` is 1.6 MB on the
// target build, which would be a bad fit for that L2 if a packet touched all of
// it - and a complete non-issue if a packet touches a fraction.  This settles
// which, by measuring rather than by reasoning about the class layout.
//
// HOW.  The decoder is placement-constructed into an mmap'd region.  Before
// each packet every page of it is made PROT_NONE; the first access to a page
// then faults, the handler counts it and maps it back readable and writable,
// and the instruction is retried.  What comes out is the number of distinct
// pages the decode read or wrote - its resident working set for that packet,
// exactly.
//
//   footprint --blob FILE [--count N]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <new>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

#include "apac/config.h"
#include "apac/decoder.h"

extern "C" {
#include "apap_crypto.h"
}

namespace {

uint8_t* gBase;
size_t gBytes;
size_t gPage;
uint8_t* gSeen;          // one byte per page
unsigned long gFaults;

void OnFault(int, siginfo_t* info, void*)
{
    uint8_t* at = (uint8_t*)info->si_addr;
    if (at < gBase || at >= gBase + gBytes) _exit(97);
    const size_t page = (size_t)(at - gBase) / gPage;
    if (!gSeen[page]) { gSeen[page] = 1; ++gFaults; }
    mprotect(gBase + page * gPage, gPage, PROT_READ | PROT_WRITE);
}

uint32_t BE32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

uint32_t CookieOffset(const uint8_t* c, uint32_t n, uint32_t* payloadBytes)
{
    uint32_t at = 0;
    while (at + 8 <= n) {
        const uint32_t size = BE32(c + at), type = BE32(c + at + 4);
        if (size < 8 || at + size > n) return 0;
        if (type == 0x64617061u) { if (size < 12) return 0; *payloadBytes = size - 12; return at + 12; }
        if (type == 0x66726d61u || type == 0x61706163u) { at += 8; continue; }
        at += size;
    }
    return 0;
}

uint32_t LE32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

}  // namespace

int main(int argc, char** argv)
{
    const char* blobPath = 0;
    long count = 200;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--blob") && i + 1 < argc) blobPath = argv[++i];
        else if (!strcmp(argv[i], "--count") && i + 1 < argc) count = strtol(argv[++i], 0, 0);
        else if (!blobPath) blobPath = argv[i];
    }
    if (!blobPath) { fprintf(stderr, "usage: footprint --blob FILE [--count N]\n"); return 2; }

    FILE* f = fopen(blobPath, "rb");
    if (!f) { fprintf(stderr, "footprint: %s: cannot open\n", blobPath); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* raw = (uint8_t*)malloc((size_t)n);
    if (fread(raw, 1, (size_t)n, f) != (size_t)n) return 2;
    fclose(f);
    if (memcmp(raw, "APACBENCH1\0\0", 12)) { fprintf(stderr, "footprint: not a bench blob\n"); return 2; }

    size_t at = 12;
    const uint32_t cookieLength = LE32(raw + at); at += 4;
    const uint8_t* cookie = raw + at; at += cookieLength;
    const uint32_t records = LE32(raw + at); at += 4;

    gPage = (size_t)sysconf(_SC_PAGESIZE);
    gBytes = (sizeof(apac::ASPDecoder) + gPage - 1) / gPage * gPage;
    gBase = (uint8_t*)mmap(0, gBytes, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANON, -1, 0);
    if (gBase == MAP_FAILED) { perror("mmap"); return 2; }
    gSeen = (uint8_t*)calloc(gBytes / gPage, 1);

    static apac::GlobalConfig config;
    uint32_t payloadBytes = 0;
    const uint32_t off = CookieOffset(cookie, cookieLength, &payloadBytes);
    if (!off) { fprintf(stderr, "footprint: no dapa atom\n"); return 2; }
    config.Reset();
    {
        apac::BitstreamReader bs(cookie + off + 2, payloadBytes - 2);
        if (config.Deserialize(bs) != apac::kOk) { fprintf(stderr, "footprint: bad cookie\n"); return 2; }
    }
    apac::ASPDecoder* decoder = new (gBase) apac::ASPDecoder();
    if (decoder->Initialize(config) != apac::kOk) { fprintf(stderr, "footprint: init\n"); return 2; }

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = &OnFault;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGBUS, &sa, 0);

    float* pcm = (float*)malloc(sizeof(float) * decoder->NumChannels() * decoder->FrameLength());
    unsigned long decoded = 0, totalPages = 0, maxPages = 0;

    for (uint32_t i = 0; i < records && (long)decoded < count; ++i) {
        const uint32_t len = LE32(raw + at); at += 4;
        const uint8_t* plain = raw + at; at += len;

        const uint8_t* audio = 0;
        size_t audioLength = 0;
        spa_apap_extension_t state;
        memset(&state, 0, sizeof state);
        if (!spa_apap_parse_extension(plain, len, &state, &audio, &audioLength)) continue;
        if (!audio || !audioLength) continue;

        // Every page cold again, so what the packet touches is what is counted.
        memset(gSeen, 0, gBytes / gPage);
        gFaults = 0;
        mprotect(gBase, gBytes, PROT_NONE);

        uint32_t got = 0;
        const uint32_t rc = decoder->DecodeFrame(audio, (uint32_t)audioLength, pcm, &got);

        mprotect(gBase, gBytes, PROT_READ | PROT_WRITE);
        if (rc != apac::kOk) { fprintf(stderr, "footprint: packet %u -> %u\n", i, rc); return 1; }

        totalPages += gFaults;
        if (gFaults > maxPages) maxPages = gFaults;
        ++decoded;
    }

    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);

    printf("footprint: %lu packets, decoder object %lu bytes in %lu pages of %lu\n",
           decoded, (unsigned long)sizeof(apac::ASPDecoder),
           (unsigned long)(gBytes / gPage), (unsigned long)gPage);
    printf("footprint: touched per packet - mean %.1f pages (%.0f KiB), worst %lu pages (%lu KiB)\n",
           (double)totalPages / (double)decoded,
           (double)totalPages / (double)decoded * (double)gPage / 1024.0,
           maxPages, maxPages * gPage / 1024);
    printf("footprint: that is %.1f %% of the object\n",
           100.0 * ((double)totalPages / (double)decoded) / (double)(gBytes / gPage));
    return 0;
}
