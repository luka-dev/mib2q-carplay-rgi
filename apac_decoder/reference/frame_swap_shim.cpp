/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// frame_swap_shim - run the PORT's element and core-decoder layers inside
// Pioneer's own decoder, and let the library's top level do everything else.
//
//     ./apac_decoder/reference/run.sh swapframe          # all 3977 packets
//     ./apac_decoder/reference/run.sh swapframe 400
//
// This is the stage-f/g composition test, and the counterpart of
// transform_swap_shim.cpp for stage d.  `APACCoreDecoder::DecodeAPACFrame` is
// the whole of the port's frame layer behind one entry point: the element
// presence loop, the SCE / CPE / LFE bodies, the CAC tool, TNS, the
// bandwidth-extension header, the filter bank and the channel remap.  Above it
// the LIBRARY keeps the frame-type field, the trimming record, the ancillary
// and extension-frame decoders, ContentOutBuffer and InterleaveConvert - so
// this measures the port's frame layer against the original with the port's own
// reimplementation of the top level taken out of the picture, which the plain
// end-to-end run cannot do.
//
// HOW.  Same 16-byte absolute jump transform_swap_shim.cpp uses, for the same
// reason: the library is linked DT_SYMBOLIC so LD_PRELOAD cannot interpose its
// internals.  Two entry points are replaced, `DecodeAPACFrame` and the
// `APACCoreDecoder::Reset` that APACDecoder::Reset drives, because the port
// keeps its own filter-bank overlap and the library's copy goes stale.
//
// The port needs the stream's configuration and the shim cannot reach the
// library's parsed copy, so it parses the magic cookie itself out of
// APAC_SWAP_COOKIE - the same file apac_ref is given.
//
// WHAT THE LIBRARY STILL READS OUT OF ITS OWN OBJECT, and which therefore has
// to keep agreeing with the port: GetNumChannels() at +200 and GetBlockSize()
// through the instance config at +8.  Both are the library's, unmodified; if
// the port disagreed about either, ContentOutBuffer would reject the frame.
//
// The output vector is a std::vector<float*> the library owns and would
// normally emplace_back into.  The shim writes the three pointers directly and
// allocates its storage with ::operator new, so the library's own destructor
// frees it correctly.

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "apac/config.h"
#include "apac/decoder.h"
#include "apac/lrvq.h"

namespace {

// The library's TBitstreamReader<unsigned> and the port's apac::BitstreamReader
// are the same 24 bytes in the same order - cur, end, cache, bitsLeft - which is
// what lets the shim hand the library's reader straight to the port.
struct LayoutCheck {
    char sizeMatches[sizeof(apac::BitstreamReader) == 24 ? 1 : -1];
};

// std::vector<float*>, the layout every gcc build uses.
struct VecFloatPtr {
    float** begin;
    float** end;
    float** cap;
};

struct Slot {
    const void*       owner;
    apac::CoreDecoder core;
};

// One core decoder per library core decoder; the harness makes one.
const int kMaxSlots = 2;
Slot g_slots[kMaxSlots];
int  g_used = 0;
long g_frames = 0;
apac::GlobalConfig g_config;
bool g_haveConfig = false;
// One for the whole shim, exactly as apac::Decoder holds one for the whole
// decoder: the LRVQ prediction state is reset on entry to each spectrum call.
apac::LRVQDecoder g_lrvq;

uint32_t BE32(const unsigned char* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

// APACDecoder_GetCookieOffset, 0x00106030, then the +2 that steps over the
// 0x0800 sync word at the head of the dapa payload.
bool loadConfig(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "[frame] %s: cannot open\n", path); return false; }
    static unsigned char ck[4096];
    const size_t n = fread(ck, 1, sizeof ck, f);
    fclose(f);

    uint32_t off = 0, bytes = 0;
    for (size_t at = 0; at + 8 <= n;) {
        const uint32_t size = BE32(ck + at), type = BE32(ck + at + 4);
        if (size < 8 || at + size > n) break;
        if (type == 0x64617061u) { off = (uint32_t)at + 12; bytes = size - 12; break; }
        at += (type == 0x66726d61u || type == 0x61706163u) ? 8 : size;
    }
    if (off == 0 || bytes < 3) { fprintf(stderr, "[frame] no dapa atom\n"); return false; }

    g_config.Reset();
    apac::BitstreamReader bs(ck + off + 2, bytes - 2);
    const uint32_t rc = g_config.Deserialize(bs);
    if (rc != apac::kOk) {
        fprintf(stderr, "[frame] GlobalConfig::Deserialize -> %u\n", rc);
        return false;
    }
    return true;
}

apac::CoreDecoder* coreFor(const void* self)
{
    for (int i = 0; i < g_used; ++i)
        if (g_slots[i].owner == self)
            return &g_slots[i].core;
    if (g_used == kMaxSlots) { fprintf(stderr, "[frame] too many core decoders\n"); abort(); }
    Slot& s = g_slots[g_used++];
    s.owner = self;
    // Only the single-codec-config path is exercised, which is all either
    // cookie produces.
    const uint32_t rc = s.core.Initialize(g_config.codecConfigs[0], g_config.sampleRate,
                                          g_config.frameLength, &g_lrvq);
    if (rc != apac::kOk) {
        fprintf(stderr, "[frame] port CoreDecoder::Initialize -> %u\n", rc);
        abort();
    }
    fprintf(stderr, "[frame] port frame layer engaged: %u channels, frame %u\n",
            s.core.GetNumChannels(), s.core.GetBlockSize());
    return &s.core;
}

bool plantJump(void* site, void* target, const char* what)
{
    const long ps = sysconf(_SC_PAGESIZE);
    void* pg = (void*)((uintptr_t)site & ~(uintptr_t)(ps - 1));
    if (mprotect(pg, 2 * (size_t)ps, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        perror("[frame] mprotect");
        return false;
    }
    uint32_t* i = (uint32_t*)site;
    i[0] = 0x58000050;                       // LDR X16, [PC, #8]
    i[1] = 0xd61f0200;                       // BR  X16
    memcpy(i + 2, &target, sizeof target);
    __builtin___clear_cache((char*)site, (char*)site + 16);
    fprintf(stderr, "[frame] %s -> port\n", what);
    return true;
}

}  // namespace

extern "C" int32_t apac_frame_decode(void* self, void* bs, uint32_t frameType,
                                     VecFloatPtr* out)
{
    apac::CoreDecoder* core = coreFor(self);
    const uint32_t n = core->GetNumChannels();

    // The library hands over its own std::vector<float*>; give it capacity the
    // way it would itself, so its destructor stays correct.
    if ((size_t)(out->cap - out->begin) < n) {
        float** mem = (float**)::operator new(sizeof(float*) * n);
        if (out->begin)
            ::operator delete(out->begin);
        out->begin = mem;
        out->cap = mem + n;
    }
    out->end = out->begin;

    const uint32_t rc = core->DecodeAPACFrame(*(apac::BitstreamReader*)bs, frameType,
                                              out->begin);
    if (rc != apac::kOk)
        return (int32_t)rc;
    out->end = out->begin + n;
    ++g_frames;
    return 0;
}

extern "C" int32_t apac_frame_reset(void* self)
{
    coreFor(self)->Reset();
    return 0;
}

namespace {

__attribute__((constructor)) void install(void)
{
    const char* cookie = getenv("APAC_SWAP_COOKIE");
    if (!cookie)
        return;
    if (!loadConfig(cookie))
        return;
    g_haveConfig = true;

    const char* lib = getenv("APAC_LIB");
    void* h = dlopen(lib ? lib : "/opt/apac/lib/libSPF_CP_PlugInWrapper.so",
                     RTLD_NOW | RTLD_GLOBAL);
    if (!h) { fprintf(stderr, "[frame] dlopen: %s\n", dlerror()); return; }

    // Mangled names read out of the binary with gobjdump -T, never spelled by
    // hand - NOTES.md records what happens when they are.
    void* dec = dlsym(h, "_ZN15APACCoreDecoder15DecodeAPACFrameER16TBitstreamReader"
                         "IjEN4apac9FrameTypeERSt6vectorIPfSaIS6_EE");
    void* rst = dlsym(h, "_ZN15APACCoreDecoder5ResetEv");
    if (!dec || !rst) { fprintf(stderr, "[frame] core-decoder symbols missing\n"); return; }
    plantJump(dec, (void*)&apac_frame_decode, "APACCoreDecoder::DecodeAPACFrame");
    plantJump(rst, (void*)&apac_frame_reset, "APACCoreDecoder::Reset");
}

__attribute__((destructor)) void report(void)
{
    if (g_frames)
        fprintf(stderr, "[frame] %ld frames decoded by the port's frame layer\n", g_frames);
    (void)g_haveConfig;
}

}  // namespace
