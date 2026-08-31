// Per-function oracle for stage e (the bandwidth extension).  It dlopens
// Pioneer's copy of Apple's APAC decoder and calls BWELPC's and
// APACBWE2Decoder's members directly, then prints what the library produced in
// the fixture format tests/fixtures/ uses.
//
// A sibling of apac_fn_ref.cpp and apac_lrvq_ref.cpp rather than a command
// inside either, so that stage e can regenerate its fixtures without touching a
// file another stage shares.
//
// Driven by tools/gen_fixtures_bwe.sh.  Commands:
//
//   lsf     BWELPC::DecodeLSFsToLPCs over the index pairs in bwe_cases.h,
//           reporting the conditioned LSFs, the LSPs and the 17 LPCs
//   env     BWELPC::LPCToMDCTSpectrum, 10 minimum-phase vectors x 8 lengths
//   lpc     BWELPC::MDCTCoefficientsToLPC on the frame cases' own spectra
//   frame   APACBWE2Decoder::DecodeFrame end to end, on a built-up decoder
//
// EVERY mangled name below was read out of the binary with
//   gobjdump -T libSPF_CP_PlugInWrapper.so | grep ...
// and pasted.  Do not hand-construct one: dlsym then returns NULL, and the
// guard in need<>() is the only thing standing between that and a test that
// proves whatever you hoped.

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// ------------------------------------------------------- the library's ABI --
//
// apac::SFBOffsets, as apac::GetSFBOffsetTable returns it: a 40-byte record,
// {u64 rate, u64 numLong, const u16* long, u64 numShort, const u16* short}.
struct SFBRecord {
    uint64_t        rate;
    uint64_t        numLong;
    const uint16_t* offLong;
    uint64_t        numShort;
    const uint16_t* offShort;
};

typedef const SFBRecord* (*fn_sfbtable)(uint32_t, uint32_t);
static fn_sfbtable SFBTable;

// bwe_cases.h asks for this and the library is the authority on it here.
static uint32_t BweCoreEdge(uint32_t rate, uint32_t seq, uint32_t maxSfb)
{
    const SFBRecord* t = SFBTable(rate, 1024);
    if (!t) {
        fprintf(stderr, "no SFB record for %u Hz\n", rate);
        exit(2);
    }
    if (seq == 2)
        return 8u * t->offShort[maxSfb];
    return t->offLong[maxSfb];
}

#include "bwe_cases.h"

struct Spec { float* begin; float* end; };

typedef void     (*fn_bwelpc_init)(void*);
typedef void     (*fn_declsf)(void*, const uint16_t*);
typedef void     (*fn_mdct2lpc)(void*, const float*, uint32_t, uint32_t, uint32_t);
typedef void     (*fn_lpc2mdct)(void*, const float*, float*, uint32_t);
typedef uint32_t (*fn_decodeframe)(void*, const void*, Spec*, uint32_t);

static void* gLib;
static fn_bwelpc_init BWELPCInitialize;
static fn_declsf      DecodeLSFsToLPCs;
static fn_mdct2lpc    MDCTCoefficientsToLPC;
static fn_lpc2mdct    LPCToMDCTSpectrum;
static fn_decodeframe DecodeFrame;
static const void*    gBWELPCVTable;

template <typename T>
static T need(const char* mangled)
{
    void* p = dlsym(gLib, mangled);
    if (!p) {
        fprintf(stderr, "unresolved: %s\n", mangled);
        exit(2);
    }
    return (T)p;
}

static void resolve()
{
    SFBTable = need<fn_sfbtable>("_ZN4apac17GetSFBOffsetTableEjj");
    BWELPCInitialize = need<fn_bwelpc_init>("_ZN6BWELPC10InitializeEv");
    DecodeLSFsToLPCs = need<fn_declsf>("_ZN6BWELPC16DecodeLSFsToLPCsEPKt");
    MDCTCoefficientsToLPC =
        need<fn_mdct2lpc>("_ZN6BWELPC21MDCTCoefficientsToLPCEPKfjjj");
    LPCToMDCTSpectrum = need<fn_lpc2mdct>("_ZN6BWELPC17LPCToMDCTSpectrumEPKfPfj");
    DecodeFrame = need<fn_decodeframe>(
        "_ZN15APACBWE2Decoder11DecodeFrameERK11APACICSInfoR16APACSpectralDataj");
    gBWELPCVTable = (const char*)need<const void*>("_ZTV6BWELPC") + 16;
}

// ------------------------------------------------------------------ printing --
static void pf(const char* tag, const float* v, uint32_t n)
{
    printf("%s\n", tag);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t b;
        memcpy(&b, &v[i], 4);
        printf("%s%08x", (i % 12) ? " " : "  ", b);
        if (i % 12 == 11 || i + 1 == n)
            printf("\n");
    }
}

// ------------------------------------------------------------ fake objects --
//
// BWELPC, exactly as APACBWE2Decoder::InitInterFrameMemory builds one at
// 0x791d0: `operator new(0x120)` and then an inline field initialiser.  Only
// four words in the 288 are non-zero, and every one of them is a constant in
// that function - none comes from the bitstream or from any config:
//
//   +0    the vtable pointer (vtable + 16)
//   +8    0x0000000100400010 - u16 order 16 at +8, 4 at +10, 1 at +12
//   +104  16                 - apac::LSFTools' own order
//   +112  0x4248000046BB8000 - float 24000.0f at +112, 50.0f at +116
//
// BWELPC::Initialize() then does the rest: apac::LSFTools::Initialize sizes six
// float vectors and builds the two apac::LPCodeBook records, and six
// apac::LPMDCTFilters are created from BWELPC::kDftSizes.
static void* makeBWELPC()
{
    void* p = calloc(1, 0x120);
    if (!p) exit(2);
    char* o = (char*)p;
    memcpy(o, &gBWELPCVTable, 8);
    uint64_t geom = 0x0000000100400010ULL;
    memcpy(o + 8, &geom, 8);
    uint32_t order = 16;
    memcpy(o + 104, &order, 4);
    uint64_t rates = 0x4248000046BB8000ULL;
    memcpy(o + 112, &rates, 8);
    BWELPCInitialize(p);
    return p;
}

// The two coefficient buffers BWELPC leaves its answers in, and the two
// intermediates DecodeLSFsToLPCs writes on the way: apac::LPCBase's own LPC
// vector at +24, the LSF-derived one at +48, the LSF vector at +192 and the
// LSP vector at +216.  Each is a std::vector<float>, so the member IS the begin
// pointer.
static const float* lpcBase(void* o)  { return *(const float**)((char*)o + 24); }
static const float* lpcFromLSF(void* o) { return *(const float**)((char*)o + 48); }
static const float* lsfOf(void* o)    { return *(const float**)((char*)o + 192); }
static const float* lspOf(void* o)    { return *(const float**)((char*)o + 216); }

// APACICSInfo is 768 bytes in the library, but DecodeFrame reads exactly three
// fields out of it - +10 the window sequence, +11 maxSfb and +16 the window
// grouping word - and makes no virtual call on it at all.  A zeroed slab with
// those three set therefore takes exactly the path a real object takes.
struct FakeICS {
    uint8_t b[768];
};
static void icsInit(FakeICS& o, uint32_t seq, uint32_t maxSfb, uint32_t group)
{
    memset(&o, 0, sizeof o);
    o.b[10] = (uint8_t)seq;
    o.b[11] = (uint8_t)maxSfb;
    memcpy(o.b + 16, &group, 4);
}

// APACBWE2Decoder, as InitInterFrameMemory and Initialize leave one.  Every
// field DecodeFrame touches, and nothing else:
//
//   +8   std::vector<bool>'s word array - the per-channel active flags
//   +56  int*     8 gain indices per channel
//   +72  uint32*  2 codebook indices per channel
//   +80  the APACInstanceConfig, whose +16 is the apac::SFBOffsets record
//   +88  std::vector<std::unique_ptr<BWELPC>>'s begin - one per channel
enum { kOracleChannels = 6 };

struct FakeBWE {
    uint8_t   obj[128];
    uint64_t  active[2];
    int32_t   scale[kOracleChannels * 8];
    uint32_t  env[kOracleChannels * 2];
    uint64_t  config[4];
    void*     lpc[kOracleChannels];
};

static void bweInit(FakeBWE& b, const SFBRecord* table)
{
    memset(&b, 0, sizeof b);
    for (int i = 0; i < kOracleChannels; ++i)
        b.lpc[i] = makeBWELPC();
    b.config[2] = (uint64_t)(uintptr_t)table;      // config + 16
    void* p;
    p = b.active;  memcpy(b.obj + 8, &p, 8);
    p = b.scale;   memcpy(b.obj + 56, &p, 8);
    p = b.env;     memcpy(b.obj + 72, &p, 8);
    p = b.config;  memcpy(b.obj + 80, &p, 8);
    p = b.lpc;     memcpy(b.obj + 88, &p, 8);
}

// ------------------------------------------------------------------ commands --
static void cmdLsf()
{
    static BweLsfCase cases[kBweLsfCases];
    BweBuildLsfCases(cases);
    void* o = makeBWELPC();
    printf("lsf cases %d order %d\n", kBweLsfCases, kBweLPCOrder);
    for (int i = 0; i < kBweLsfCases; ++i) {
        DecodeLSFsToLPCs(o, cases[i].idx);
        printf("case %d idx %u %u\n", i, cases[i].idx[0], cases[i].idx[1]);
        pf("lsf", lsfOf(o), kBweLPCOrder);
        pf("lsp", lspOf(o), kBweLPCOrder);
        pf("lpc", lpcFromLSF(o), kBweLPCOrder + 1);
    }
}

static void cmdEnv()
{
    void* o = makeBWELPC();
    static float env[2048];
    printf("env vectors %d sizes %d\n", kBweEnvVectors, kBweEnvSizes);
    for (int v = 0; v < kBweEnvVectors; ++v) {
        float lpc[kBweLPCOrder + 1];
        BweBuildLPC(lpc, (uint32_t)v);
        for (int s = 0; s < kBweEnvSizes; ++s) {
            const uint32_t n = kBweEnvN[s];
            memset(env, 0, sizeof env);
            LPCToMDCTSpectrum(o, lpc, env, n);
            printf("case %d %d n %u\n", v, s, n);
            pf("env", env, n);
        }
    }
}

static void cmdLpc()
{
    void* o = makeBWELPC();
    static BweFrameCase cases[kBweFrameCases];
    BweBuildFrameCases(cases);
    printf("lpc cases %d\n", kBweFrameCases);
    for (int i = 0; i < kBweFrameCases; ++i) {
        const BweFrameCase& c = cases[i];
        const uint32_t edge = BweCoreEdge(c.sampleRate, c.windowSequence,
                                          c.maxSfb);
        const uint32_t bwStart = (edge >= 384) ? 384u : 256u;
        // Exactly the two calls DecodeFrame makes: one whole-block spectrum for
        // a long block, eight 128-spaced ones for a short block.
        if (c.windowSequence == 2)
            MDCTCoefficientsToLPC(o, c.spec, 128, bwStart >> 3, 8);
        else
            MDCTCoefficientsToLPC(o, c.spec, 1024, bwStart, 1);
        printf("case %d seq %u bwStart %u\n", i, c.windowSequence, bwStart);
        pf("lpc", lpcBase(o), kBweLPCOrder + 1);
    }
}

static void cmdFrame()
{
    static BweFrameCase cases[kBweFrameCases];
    BweBuildFrameCases(cases);
    static float spec[kBweFrameLength];
    printf("frame cases %d\n", kBweFrameCases);
    for (int i = 0; i < kBweFrameCases; ++i) {
        const BweFrameCase& c = cases[i];
        const SFBRecord* table = SFBTable(c.sampleRate, 1024);
        FakeBWE b;
        bweInit(b, table);
        // The header this case would have parsed: the channel is active, its
        // two codebook indices and its eight gain indices are in place.
        b.active[0] = 1ULL << c.channel;
        b.env[c.channel * 2 + 0] = c.env[0];
        b.env[c.channel * 2 + 1] = c.env[1];
        for (int k = 0; k < kBweScaleBands; ++k)
            b.scale[c.channel * 8 + k] = (int32_t)c.scale[k];

        FakeICS ics;
        icsInit(ics, c.windowSequence, c.maxSfb, c.windowGroupWord);
        memcpy(spec, c.spec, sizeof spec);
        Spec s;
        s.begin = spec;
        s.end = spec + kBweFrameLength;
        const uint32_t rc = DecodeFrame(b.obj, &ics, &s, c.channel);
        printf("case %d rc %u ch %u\n", i, rc, c.channel);
        pf("spec", spec, kBweFrameLength);
        for (int k = 0; k < kOracleChannels; ++k)
            free(b.lpc[k]);
    }
}

int main(int argc, char** argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    const char* lib = getenv("APAC_LIB");
    if (!lib)
        lib = "/opt/apac/lib/libSPF_CP_PlugInWrapper.so";
    gLib = dlopen(lib, RTLD_NOW | RTLD_GLOBAL);
    if (!gLib) {
        fprintf(stderr, "dlopen(%s): %s\n", lib, dlerror());
        return 2;
    }
    resolve();
    const char* cmd = (argc > 1) ? argv[1] : "probe";
    if (!strcmp(cmd, "probe")) {
        fprintf(stderr, "apac_bwe_ref: library loaded, all symbols resolved\n");
        return 0;
    }
    if (!strcmp(cmd, "lsf"))   { cmdLsf();   return 0; }
    if (!strcmp(cmd, "env"))   { cmdEnv();   return 0; }
    if (!strcmp(cmd, "lpc"))   { cmdLpc();   return 0; }
    if (!strcmp(cmd, "frame")) { cmdFrame(); return 0; }
    fprintf(stderr, "unknown command %s\n", cmd);
    return 2;
}
