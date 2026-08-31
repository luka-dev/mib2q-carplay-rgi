// transform_swap_shim - decode the real capture with the PORT's transform stage
// substituted into the middle of Pioneer's decoder, and see what it costs.
//
// This is the composition test for stage d.  The per-function differential
// tests feed the MDCT and the filter bank synthetic vectors; this feeds them
// the actual spectral coefficients the real stream produces, in the real order,
// with the real window-sequence and window-shape stream, for every frame of the
// capture - and then measures the end-to-end PCM difference, which is the thing
// a listener would hear.
//
//   ./apac_decoder/reference/run.sh swap
//
// HOW.  The library is linked DT_SYMBOLIC, so LD_PRELOAD cannot interpose its
// internals; NOTES.md records that trap.  So this does what scratch/stub_shim.c
// does - patches the function entry in memory - except that instead of stubbing
// the function out it plants a 16-byte absolute jump to the port's version:
//
//     LDR X16, [PC, #8]     ; 0x58000050
//     BR  X16               ; 0xd61f0200
//     .quad target
//
// Two entry points are replaced, both reached through the filter bank's vtable:
// FrequencyToTimeInPlace and Reset.  Everything else - the bitstream, the
// scalefactors, the spectrum, TNS - is still the library's, so any difference
// in the output is the port's transform stage and nothing else.
//
// The port keeps its own overlap state, so the library's copy goes stale.  That
// is safe: FrequencyToTimeInPlace and Reset are the only two functions that
// read it, and both are the ones being replaced.

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "apac/filterbank.h"

namespace {

// APACSynthesisFilterBank fields the shim reads out of the library's object, so
// the port's filter bank can be configured to match without intercepting
// Initialize (which allocates buffers the library's destructor frees).
//
//   +24  channel count
//   +40  the long MDCT's transform size, i.e. the frame length
const size_t kOffChannels = 24;
const size_t kOffFrameLen = 40;

struct Slot {
    const void* owner;
    apac::SynthesisFilterBank fb;
};

// One filter bank per decoder instance; the harness makes one.  Four is slack.
const int kMaxSlots = 4;
Slot g_slots[kMaxSlots];
int  g_used = 0;
long g_calls = 0;

apac::SynthesisFilterBank* bankFor(const void* self)
{
    for (int i = 0; i < g_used; ++i)
        if (g_slots[i].owner == self)
            return &g_slots[i].fb;
    if (g_used == kMaxSlots) {
        fprintf(stderr, "[swap] more than %d filter banks; giving up\n", kMaxSlots);
        abort();
    }
    const char* o = (const char*)self;
    const uint32_t channels = (uint32_t) * (const uint64_t*)(o + kOffChannels);
    const uint32_t frameLen = *(const uint32_t*)(o + kOffFrameLen);
    Slot& s = g_slots[g_used++];
    s.owner = self;
    const int32_t rc = s.fb.Initialize(channels, frameLen);
    if (rc != 0) {
        fprintf(stderr, "[swap] port Initialize(%u, %u) returned %d\n",
                channels, frameLen, rc);
        abort();
    }
    s.fb.Reset();
    fprintf(stderr, "[swap] port filter bank engaged: %u channels, frame %u\n",
            channels, frameLen);
    return &s.fb;
}

}  // namespace

extern "C" int32_t apac_swap_f2t(void* self, uint32_t channel, float* buf,
                                 uint32_t windowShape, uint32_t windowSequence)
{
    ++g_calls;
    return bankFor(self)->FrequencyToTimeInPlace(channel, buf, windowShape,
                                                 windowSequence);
}

extern "C" int32_t apac_swap_reset(void* self)
{
    bankFor(self)->Reset();
    return 0;
}

namespace {

bool plantJump(void* site, void* target, const char* what)
{
    const long ps = sysconf(_SC_PAGESIZE);
    void* pg = (void*)((uintptr_t)site & ~(uintptr_t)(ps - 1));
    if (mprotect(pg, 2 * (size_t)ps, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        perror("[swap] mprotect");
        return false;
    }
    uint32_t* i = (uint32_t*)site;
    i[0] = 0x58000050;                       // LDR X16, [PC, #8]
    i[1] = 0xd61f0200;                       // BR  X16
    memcpy(i + 2, &target, sizeof target);   // the absolute destination
    __builtin___clear_cache((char*)site, (char*)site + 16);
    fprintf(stderr, "[swap] %s -> port\n", what);
    return true;
}

__attribute__((constructor)) void install(void)
{
    if (!getenv("APAC_SWAP_TRANSFORM"))
        return;
    const char* path = getenv("APAC_LIB");
    void* h = dlopen(path ? path : "/opt/apac/lib/libSPF_CP_PlugInWrapper.so",
                     RTLD_NOW | RTLD_GLOBAL);
    if (!h) { fprintf(stderr, "[swap] dlopen: %s\n", dlerror()); return; }

    // Mangled names read out of the binary with gobjdump -T, never spelled by
    // hand - NOTES.md records what happens when they are.
    void* f2t = dlsym(h, "_ZN23APACSynthesisFilterBank22FrequencyToTimeInPlaceEjPfjj");
    void* rst = dlsym(h, "_ZN23APACSynthesisFilterBank5ResetEv");
    if (!f2t || !rst) {
        fprintf(stderr, "[swap] filter-bank symbols not found\n");
        return;
    }
    plantJump(f2t, (void*)&apac_swap_f2t, "FrequencyToTimeInPlace");
    plantJump(rst, (void*)&apac_swap_reset, "Reset");
}

__attribute__((destructor)) void report(void)
{
    if (g_calls)
        fprintf(stderr, "[swap] %ld transform calls served by the port\n", g_calls);
}

}  // namespace
