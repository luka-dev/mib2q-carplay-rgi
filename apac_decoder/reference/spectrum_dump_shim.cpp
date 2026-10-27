/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// spectrum_dump_shim - dump the spectrum the ORIGINAL frame layer hands to the
// filter bank, one record per channel per frame.
//
// This is the differential probe for stages f and g.  Everything above the
// filter bank - the element layer, the CAC tool, TNS, the bandwidth-extension
// header, the core decoder's element loop - has no other observable output, so
// comparing these records against ours isolates the frame layer from the
// transform, which is the only part that cannot be bit exact.
//
//     APAC_DUMP_SPECTRA=/out/lib.spec LD_PRELOAD=/tmp/dump.so apac_ref --f32 ...
//     apac_decode ... --dump-spectra ours.spec
//     reference/compare_spectra.py lib.spec ours.spec
//
// HOW.  APACCoreDecoder::DecodeAPACFrame calls
// APACSynthesisFilterBank::FrequencyToTimeInPlace directly, not through a
// vtable, so there is no slot to replace: the entry is patched with a 16-byte
// absolute jump, exactly as transform_swap_shim.cpp does.  That destroys the
// library, so the wrapper finishes the job with OUR filter bank - which
// `run.sh swap` has already measured against the library's at 3e-8 end to end.
// The dump itself is taken BEFORE the transform runs, so it is the library's
// spectrum whatever the transform then does with it.
//
// Record: [u32 channel][u32 windowSequence][1024 float32].

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "apac/filterbank.h"

namespace {

const size_t kOffChannels = 24;     // APACSynthesisFilterBank + 24, channel count
const size_t kOffFrameLen = 40;     // + 40, the long MDCT's transform size

struct Slot {
    const void* owner;
    apac::SynthesisFilterBank fb;
};

const int kMaxSlots = 4;
Slot g_slots[kMaxSlots];
int  g_used = 0;
FILE* g_dump = 0;
long g_records = 0;

apac::SynthesisFilterBank* bankFor(const void* self)
{
    for (int i = 0; i < g_used; ++i)
        if (g_slots[i].owner == self)
            return &g_slots[i].fb;
    if (g_used == kMaxSlots) {
        fprintf(stderr, "[dump] too many filter banks\n");
        abort();
    }
    const char* o = (const char*)self;
    const uint32_t channels = (uint32_t) * (const uint64_t*)(o + kOffChannels);
    const uint32_t frameLen = *(const uint32_t*)(o + kOffFrameLen);
    Slot& s = g_slots[g_used++];
    s.owner = self;
    if (s.fb.Initialize(channels, frameLen) != 0) {
        fprintf(stderr, "[dump] ours Initialize(%u, %u) failed\n", channels, frameLen);
        abort();
    }
    s.fb.Reset();
    return &s.fb;
}

bool plantJump(void* site, void* target, const char* what)
{
    const long ps = sysconf(_SC_PAGESIZE);
    void* pg = (void*)((uintptr_t)site & ~(uintptr_t)(ps - 1));
    if (mprotect(pg, 2 * (size_t)ps, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        perror("[dump] mprotect");
        return false;
    }
    uint32_t* i = (uint32_t*)site;
    i[0] = 0x58000050;                       // LDR X16, [PC, #8]
    i[1] = 0xd61f0200;                       // BR  X16
    memcpy(i + 2, &target, sizeof target);
    __builtin___clear_cache((char*)site, (char*)site + 16);
    fprintf(stderr, "[dump] %s hooked\n", what);
    return true;
}

}  // namespace

extern "C" int32_t apac_dump_f2t(void* self, uint32_t channel, float* buf,
                                 uint32_t windowShape, uint32_t windowSequence)
{
    if (g_dump) {
        const uint32_t hdr[2] = { channel, windowSequence };
        fwrite(hdr, sizeof hdr, 1, g_dump);
        fwrite(buf, sizeof(float), 1024, g_dump);
        ++g_records;
    }
    return bankFor(self)->FrequencyToTimeInPlace(channel, buf, windowShape,
                                                 windowSequence);
}

extern "C" int32_t apac_dump_reset(void* self)
{
    bankFor(self)->Reset();
    return 0;
}

namespace {

__attribute__((constructor)) void install(void)
{
    const char* path = getenv("APAC_DUMP_SPECTRA");
    if (!path)
        return;
    g_dump = fopen(path, "wb");
    if (!g_dump) { perror("[dump] fopen"); return; }
    const char* lib = getenv("APAC_LIB");
    void* h = dlopen(lib ? lib : "/opt/apac/lib/libSPF_CP_PlugInWrapper.so",
                     RTLD_NOW | RTLD_GLOBAL);
    if (!h) { fprintf(stderr, "[dump] dlopen: %s\n", dlerror()); return; }
    void* f2t = dlsym(h, "_ZN23APACSynthesisFilterBank22FrequencyToTimeInPlaceEjPfjj");
    void* rst = dlsym(h, "_ZN23APACSynthesisFilterBank5ResetEv");
    if (!f2t || !rst) { fprintf(stderr, "[dump] filter-bank symbols missing\n"); return; }
    plantJump(f2t, (void*)&apac_dump_f2t, "FrequencyToTimeInPlace");
    plantJump(rst, (void*)&apac_dump_reset, "Reset");
}

__attribute__((destructor)) void finish(void)
{
    if (g_dump) {
        fclose(g_dump);
        fprintf(stderr, "[dump] %ld spectra written\n", g_records);
    }
}

}  // namespace
