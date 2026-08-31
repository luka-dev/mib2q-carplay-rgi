// Per-function oracle for stage h1 (LRVQ).  It dlopens Pioneer's copy of
// Apple's APAC decoder and calls APACLRVQDecoder's members directly, then
// prints what the library produced in the fixture format tests/fixtures/ uses.
//
// This is a sibling of apac_fn_ref.cpp rather than a command inside it, so that
// stage h1 and the stages other agents are working on can regenerate their
// fixtures without touching each other's file.
//
// Driven by tools/gen_fixtures_lrvq.sh.  Commands:
//
//   gain    dequantizeLrvqGain, all 512 indices x both modes
//   ltp     ltpToMdctSpectralEnvelope over the sweep in lrvq_cases.h
//   lsfs    lsfsToMdctSpectralEnvelope over the sweep in lrvq_cases.h
//   lrvq    LrvqDeserialize over the case plan in lrvq_cases.h
//   frame   APACChannelPairElement::DeserializeLrvqFrame on a built-up object
//
// EVERY mangled name below was read out of the binary with
//   gobjdump -T libSPF_CP_PlugInWrapper.so | grep ...
// and pasted.  Do not hand-construct one: dlsym then returns NULL, the guard
// below is the only thing standing between that and a test that proves nothing.

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "lrvq_cases.h"

// --------------------------------------------------------------- the library --
struct BS { const uint8_t* cur; const uint8_t* end; uint32_t cache; int32_t bitsLeft; };
struct Spec { float* begin; float* end; };

typedef void     (*fn_bsctor)(BS*, const uint8_t*, uint32_t);
typedef uint32_t (*fn_getbits)(BS*, uint32_t);
typedef int32_t  (*fn_remain)(const BS*);
typedef int32_t  (*fn_lrvq)(BS*, uint8_t, uint8_t, Spec*, float*, float*,
                            float*, int16_t*, uint16_t*);
typedef int32_t  (*fn_gain)(uint16_t, uint16_t, float*);
typedef int32_t  (*fn_lsfenv)(uint8_t, const uint16_t*, float*);
typedef int32_t  (*fn_ltpenv)(uint16_t, uint16_t, float*);
typedef int32_t  (*fn_sce)(BS*, void*, void*);
typedef int32_t  (*fn_cpe)(BS*, void*, void*, void*, void*);
typedef int32_t  (*fn_lfe)(BS*, void*, void*);
typedef int32_t  (*fn_icsdes)(void*, BS*, int32_t);
typedef void     (*fn_icsreset)(void*);
typedef int32_t  (*fn_frame)(void*, BS*);
typedef int32_t  (*fn_tnsparse)(void*, BS*, const void*);
typedef void*    (*fn_sfbtable)(uint32_t, uint32_t);
typedef int32_t  (*fn_lsf2lpc)(void*, float*, float*);
typedef int32_t  (*fn_condlsf)(void*, float*);

static void* gLib;
static fn_bsctor  BsCtor;
static fn_getbits GetBits;
static fn_remain  Remaining;
static fn_lrvq    Lrvq;
static fn_gain    Gain;
static fn_lsfenv  LsfEnv;
static fn_ltpenv  LtpEnv;
static fn_sce     SCE;
static fn_cpe     CPE;
static fn_lfe     LFE;
static fn_icsdes  ICSDeserialize;
static fn_icsreset ICSReset;
static fn_frame   LrvqFrame;
static fn_sfbtable SFBTable;
static fn_lsf2lpc LSF_To_LPC;
static fn_condlsf CondLSFs;

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
    BsCtor    = need<fn_bsctor>("_ZN16TBitstreamReaderIjEC1EPKhj");
    GetBits   = need<fn_getbits>("_ZN16TBitstreamReaderIjE7GetBitsEj");
    Remaining = need<fn_remain>("_ZNK16TBitstreamReaderIjE13BitsRemainingEv");
    Lrvq      = need<fn_lrvq>("_ZN15APACLRVQDecoder15LrvqDeserializeER16TBitstreamReaderIjEbbR16APACSpectralDataRN4apac13aligned_arrayIfLm1024EEES8_RNS6_IfLm256EEERNS6_IsLm64EEEPt");
    Gain      = need<fn_gain>("_ZN15APACLRVQDecoder18dequantizeLrvqGainEttPf");
    LsfEnv    = need<fn_lsfenv>("_ZN15APACLRVQDecoder26lsfsToMdctSpectralEnvelopeEbRKN4apac13aligned_arrayItLm5EEERNS1_IfLm1024EEE");
    LtpEnv    = need<fn_ltpenv>("_ZN15APACLRVQDecoder25ltpToMdctSpectralEnvelopeEttRN4apac13aligned_arrayIfLm1024EEE");
    SCE       = need<fn_sce>("_ZN15APACLRVQDecoder22SpectrumDeserializeSCEER16TBitstreamReaderIjER11APACICSInfoR27APACIndividualChannelStream");
    CPE       = need<fn_cpe>("_ZN15APACLRVQDecoder22SpectrumDeserializeCPEER16TBitstreamReaderIjER11APACICSInfoS4_R27APACIndividualChannelStreamS6_");
    LFE       = need<fn_lfe>("_ZN15APACLRVQDecoder22SpectrumDeserializeLFEER16TBitstreamReaderIjER11APACICSInfoR27APACIndividualChannelStream");
    ICSDeserialize = need<fn_icsdes>("_ZN11APACICSInfo11DeserializeER16TBitstreamReaderIjEN4apac10CodingTypeE");
    ICSReset  = need<fn_icsreset>("_ZN11APACICSInfo5ResetEv");
    LrvqFrame = need<fn_frame>("_ZN22APACChannelPairElement20DeserializeLrvqFrameER16TBitstreamReaderIjE");
    SFBTable   = need<fn_sfbtable>("_ZN4apac17GetSFBOffsetTableEjj");
    LSF_To_LPC = need<fn_lsf2lpc>("_ZN4apac8LSFTools10LSF_To_LPCEPfS1_");
    CondLSFs   = need<fn_condlsf>("_ZN4apac8LSFTools13ConditionLSFsEPf");
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

static void pb(const char* tag, const uint8_t* v, uint32_t n)
{
    printf("%s\n", tag);
    for (uint32_t i = 0; i < n; ++i) {
        printf("%s%02x", (i % 32) ? " " : "  ", v[i]);
        if (i % 32 == 31 || i + 1 == n)
            printf("\n");
    }
}

// -------------------------------------------------- fake objects for the calls --
//
// APACICSInfo is 768 bytes in the library.  Only its vptr and the six fields at
// +8..+19 are touched by anything here, and the vptr is only ever used to
// devirtualise Reset - the callers compare vtable slot 3 against
// APACICSInfo::Reset and inline it when it matches.  So a fake vtable whose
// slot 3 is the real Reset takes exactly the path a real object takes.
struct FakeICS {
    const void* vptr;
    uint8_t     pad[760];
};
static const void* gICSVTable[8];

static void icsInit(FakeICS& o, uint8_t maxSfbLong, uint8_t maxSfbShort,
                    uint8_t windowSequence)
{
    memset(&o, 0, sizeof o);
    o.vptr = &gICSVTable[2];             // an Itanium vptr points past the
                                         // offset-to-top / RTTI header
    o.pad[0] = maxSfbLong;               // +8
    o.pad[1] = maxSfbShort;              // +9
    o.pad[2] = windowSequence;           // +10
}
static uint8_t* icsField(FakeICS& o, int off) { return &o.pad[off - 8]; }

// APACIndividualChannelStream: only the APACSpectralData at +0x2F0 is read.
struct FakeICStream {
    uint8_t pad[0x2F0];
    Spec    spec;
    uint8_t tail[64];
};

int main(int argc, char** argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    const char* path = getenv("APAC_LIB");
    if (!path)
        path = "/opt/apac/lib/libSPF_CP_PlugInWrapper.so";
    gLib = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!gLib) {
        fprintf(stderr, "%s\n", dlerror());
        return 2;
    }
    resolve();
    gICSVTable[2 + 3] = (const void*)ICSReset;

    const char* cmd = (argc > 1) ? argv[1] : "gain";

    // -------------------------------------------------------------- gain ----
    if (!strcmp(cmd, "gain")) {
        for (int mode = 0; mode < 2; ++mode) {
            printf("GAIN mode=%d\n", mode);
            for (int i = 0; i < 512; ++i) {
                float g = 0.0f;
                int32_t r = Gain((uint16_t)i, (uint16_t)mode, &g);
                uint32_t b;
                memcpy(&b, &g, 4);
                if (r) {
                    printf("  RET %d at %d\n", r, i);
                    return 1;
                }
                printf("%s%08x", (i % 12) ? " " : "  ", b);
                if (i % 12 == 11 || i == 511)
                    printf("\n");
            }
        }
        return 0;
    }

    // --------------------------------------------------------------- ltp ----
    if (!strcmp(cmd, "ltp")) {
        static float out[1024];
        for (int i = 0; i < kLrvqLtpCases; ++i) {
            uint16_t gi, lag;
            LrvqLtpCase(i, &gi, &lag);
            memset(out, 0x5a, sizeof out);
            int32_t r = LtpEnv(gi, lag, out);
            printf("LTP %d gain=%u lag=%u ret=%d\n", i, gi, lag, r);
            pf("BINS", out, 256);
            int tail = 0;
            for (int k = 256; k < 1024; ++k)
                if (out[k] != 0.0f)
                    ++tail;
            printf("TAILNONZERO %d\n", tail);
        }
        return 0;
    }

    // -------------------------------------------------------------- lsfs ----
    if (!strcmp(cmd, "lsfs")) {
        static float out[1024];
        for (int i = 0; i < kLrvqLsfCases; ++i) {
            uint16_t idx[5];
            int shortBooks;
            LrvqLsfCase(i, idx, &shortBooks);
            memset(out, 0x5a, sizeof out);
            int32_t r = LsfEnv((uint8_t)shortBooks, idx, out);
            printf("LSFS %d short=%d idx=%u,%u,%u,%u,%u ret=%d\n", i, shortBooks,
                   idx[0], idx[1], idx[2], idx[3], idx[4], r);
            pf("BINS", out, 256);
            int tail = 0;
            for (int k = 256; k < 1024; ++k)
                if (out[k] != 0.0f)
                    ++tail;
            printf("TAILNONZERO %d\n", tail);
        }
        return 0;
    }

    // -------------------------------------------------------------- lrvq ----
    if (!strcmp(cmd, "lrvq")) {
        static LrvqCase cases[128];
        const int n = LrvqBuildCases(cases, 128);
        static float env[1024], env2[1024], work[256], spec[1024];
        for (int i = 0; i < n; ++i) {
            LrvqCase& c = cases[i];
            memcpy(env, c.env, sizeof env);
            memcpy(env2, c.env2, sizeof env2);
            memcpy(work, c.work, sizeof work);
            int16_t qidx[64];
            memcpy(qidx, c.qidx, sizeof qidx);
            uint16_t prevMode = c.prevMode;
            for (int k = 0; k < 1024; ++k)
                spec[k] = -777.0f;
            Spec sp = { spec, spec + 1024 };

            BS bs;
            BsCtor(&bs, c.bits, c.nbytes);
            const int32_t before = Remaining(&bs);
            int32_t ret[kLrvqMaxCalls] = { 0, 0 };
            int calls = 0;
            for (int k = 0; k < c.nCalls; ++k) {
                ret[k] = Lrvq(&bs, (uint8_t)c.call[k].isShort,
                              (uint8_t)c.call[k].firstChannel, &sp, env, env2,
                              work, qidx, &prevMode);
                ++calls;
                if (ret[k])
                    break;
            }
            const int32_t used = before - Remaining(&bs);
            printf("LRVQ %d %s calls=%d ret=%d,%d bits=%d prevMode=%u\n", i,
                   c.name, calls, ret[0], ret[1], used, prevMode);
            printf("QIDX\n");
            for (int k = 0; k < 64; ++k)
                printf("%s%d", (k % 16) ? " " : "  ", (int)qidx[k]);
            printf("\n");
            pf("WORK", work, 256);
            // env is untouched by every case that reuses it and runs no LTP -
            // saying so is 1024 floats a case cheaper than proving it.
            if (!memcmp(env, c.env, sizeof env))
                printf("ENVUNCHANGED\n");
            else
                pf("ENV", env, 1024);
            pf("SPEC", spec, 1024);
        }
        return 0;
    }

    // ----------------------------------------------------- sce / cpe / lfe ----
    if (!strcmp(cmd, "elem")) {
        static LrvqElemCase cases[128];
        const int n = LrvqBuildElemCases(cases, 128);
        static FakeICStream st0, st1;
        static float spec0[1024], spec1[1024];
        FakeICS ics0, ics1;
        for (int i = 0; i < n; ++i) {
            LrvqElemCase& c = cases[i];
            memset(&st0, 0, sizeof st0);
            memset(&st1, 0, sizeof st1);
            for (int k = 0; k < 1024; ++k)
                spec0[k] = spec1[k] = -777.0f;
            st0.spec.begin = spec0; st0.spec.end = spec0 + 1024;
            st1.spec.begin = spec1; st1.spec.end = spec1 + 1024;
            icsInit(ics0, 49, 14, (uint8_t)(c.isShort0 ? 2 : 0));
            icsInit(ics1, 49, 14, (uint8_t)(c.isShort1 ? 2 : 0));

            BS bs;
            BsCtor(&bs, c.bits, c.nbytes);
            const int32_t before = Remaining(&bs);
            int32_t r;
            if (c.kind == kLrvqElemSCE)
                r = SCE(&bs, &ics0, &st0);
            else if (c.kind == kLrvqElemCPE)
                r = CPE(&bs, &ics0, &ics1, &st0, &st1);
            else
                r = LFE(&bs, &ics0, &st0);
            const int32_t used = before - Remaining(&bs);
            printf("ELEM %d %s kind=%d ret=%d bits=%d ics0=%u,%u,%u,%u "
                   "ics1=%u,%u,%u,%u\n",
                   i, c.name, c.kind, r, used,
                   *icsField(ics0, 10), *icsField(ics0, 11), *icsField(ics0, 12),
                   *(uint32_t*)icsField(ics0, 16),
                   *icsField(ics1, 10), *icsField(ics1, 11), *icsField(ics1, 12),
                   *(uint32_t*)icsField(ics1, 16));
            pf("SPEC0", spec0, 1024);
            if (c.kind == kLrvqElemCPE)
                pf("SPEC1", spec1, 1024);
        }
        return 0;
    }

    // ---------------------------------------------------------- lsf2lpc ----
    // apac::LSFTools::LSF_To_LPC on a stack-built LSFTools, to separate the
    // LSF -> LPC half of lsfsToMdctSpectralEnvelope from the FFT half.  The
    // object is 0xF0 bytes, all zero except the vptr, the LSF order at +104 and
    // the sample rate at +112 - which is exactly how lsfsToMdctSpectralEnvelope
    // builds its own (0x9ab34..0x9ab50).  LSF_To_LPC devirtualises by comparing
    // vtable slot 17 against apac::LSFTools::ConditionLSFs, so the fake vtable
    // carries the library's real function pointers in slots 12, 14, 16 and 17.
    if (!strcmp(cmd, "lsf2lpc")) {
        static const void* vt[24];
        vt[2 + 12] = (const void*)dlsym(gLib, "_ZN4apac8LSFTools10LSF_To_LSPEPKfPff");
        vt[2 + 14] = (const void*)dlsym(gLib, "_ZN4apac8LSFTools10LSP_To_LPCEPKfPft");
        vt[2 + 16] = (const void*)dlsym(gLib, "_ZN4apac8LSFTools14Condition_LSFsEPfff");
        vt[2 + 17] = (const void*)CondLSFs;
        for (int i = 2 + 12; i <= 2 + 17; ++i)
            if (!vt[i] && i != 2 + 13 && i != 2 + 15) {
                fprintf(stderr, "lsf2lpc: unresolved vtable slot %d\n", i);
                return 2;
            }
        static uint8_t obj[0xF0];
        for (int i = 0; i < kLrvqLsfCases; ++i) {
            uint16_t idx[5];
            int shortBooks;
            LrvqLsfCase(i, idx, &shortBooks);
            // The same two-stage sum lsfsToMdctSpectralEnvelope does, with the
            // codebooks read straight out of the loaded image.
            Dl_info di;
            dladdr((void*)LsfEnv, &di);
            const uint8_t* base = (const uint8_t*)di.dli_fbase;
            const float* cb11 = (const float*)(base + (shortBooks ? 0x323e90 : 0x32be90));
            const float* cb12 = (const float*)(base + (shortBooks ? 0x31fe90 : 0x327e90));
            const float* cb21 = (const float*)(base + 0x31de90);
            const float* cb22 = (const float*)(base + 0x31be90);
            const float* cb23 = (const float*)(base + 0x317e90);
            float lsf[16];
            for (int k = 0; k < 4; ++k) lsf[k]     = cb11[8 * idx[0] + k]     + cb21[4 * idx[2] + k];
            for (int k = 0; k < 4; ++k) lsf[4 + k] = cb11[8 * idx[0] + 4 + k] + cb22[4 * idx[3] + k];
            for (int k = 0; k < 8; ++k) lsf[8 + k] = cb12[8 * idx[1] + k]     + cb23[8 * idx[4] + k];
            printf("LSF %d short=%d\n", i, shortBooks);
            pf("IN", lsf, 16);
            memset(obj, 0, sizeof obj);
            *(const void**)obj = &vt[2];
            *(uint16_t*)(obj + 104) = 16;
            *(float*)(obj + 112) = 24000.0f;
            // The chain broken open: condition, then LSF -> LSP, then
            // LSP -> LPC, each dumped, so a disagreement lands on one step.
            typedef int32_t (*fn_lsf2lsp)(void*, const float*, float*, float);
            typedef int32_t (*fn_lsp2lpc)(void*, const float*, float*, uint16_t);
            fn_lsf2lsp F2P = (fn_lsf2lsp)vt[2 + 12];
            fn_lsp2lpc P2C = (fn_lsp2lpc)vt[2 + 14];
            float step[16], lsp[16], lpc2[17];
            memcpy(step, lsf, sizeof step);
            CondLSFs(obj, step);
            memset(lsp, 0, sizeof lsp);
            F2P(obj, step, lsp, 24000.0f);
            memset(lpc2, 0, sizeof lpc2);
            P2C(obj, lsp, lpc2, 16);
            pf("LSP", lsp, 16);

            float lpc[17];
            memset(lpc, 0, sizeof lpc);
            LSF_To_LPC(obj, lsf, lpc);
            pf("CONDITIONED", lsf, 16);
            pf("LPC", lpc, 17);
            if (memcmp(lpc, lpc2, sizeof lpc) || memcmp(step, lsf, sizeof step)) {
                fprintf(stderr, "LSF_To_LPC is not the three steps\n");
                return 2;
            }
        }
        return 0;
    }

    // ------------------------------------------------------------ frame ----
    // APACChannelPairElement::DeserializeLrvqFrame on a hand-built element.
    // The function touches exactly seven things off `this`, all at fixed
    // offsets read out of the disassembly at 0xac600:
    //
    //   +16  APACICSInfo*                 channel 0
    //   +24  APACIndividualChannelStream* channel 0 (its spectrum is at +0x2F0)
    //   +32  a config whose +16 is the 40-byte apac::GetSFBOffsetTable record
    //   +128 APACTNSData                  channel 0, 190 bytes
    //   +318 APACTNSData                  channel 1
    //   +512 APACICSInfo*                 channel 1
    //   +520 APACIndividualChannelStream* channel 1
    //
    // so a byte array with those filled in takes the same path a real element
    // does.  The ICSInfo vtable needs slot 2 (Deserialize) and slot 3 (Reset);
    // the frame calls Deserialize through the vtable and the CPE compares slot
    // 3 against APACICSInfo::Reset to devirtualise it.
    if (!strcmp(cmd, "frame")) {
        static LrvqFrameCase cases[64];
        const int n = LrvqBuildFrameCases(cases, 64);
        gICSVTable[2 + 2] = (const void*)ICSDeserialize;
        const void* sfb = (const void*)SFBTable(48000, 1024);
        if (!sfb) {
            fprintf(stderr, "GetSFBOffsetTable(48000, 1024) returned NULL\n");
            return 2;
        }
        static uint8_t element[1024];
        static FakeICS ics0, ics1;
        static FakeICStream st0, st1;
        static float spec0[1024], spec1[1024];
        static const void* config[4];
        config[2] = sfb;                       // config + 16

        for (int i = 0; i < n; ++i) {
            LrvqFrameCase& c = cases[i];
            memset(element, 0, sizeof element);
            memset(&st0, 0, sizeof st0);
            memset(&st1, 0, sizeof st1);
            for (int k = 0; k < 1024; ++k)
                spec0[k] = spec1[k] = -777.0f;
            st0.spec.begin = spec0; st0.spec.end = spec0 + 1024;
            st1.spec.begin = spec1; st1.spec.end = spec1 + 1024;
            icsInit(ics0, 49, 14, 0);
            icsInit(ics1, 49, 14, 0);
            *(void**)(element + 16) = &ics0;
            *(void**)(element + 24) = &st0;
            *(void**)(element + 32) = config;
            *(void**)(element + 512) = &ics1;
            *(void**)(element + 520) = &st1;
            // APACTNSData's constructor caps: order 12 long / 7 short, and the
            // 48 kHz band caps 40 long / 14 short out of the table at 0x331970.
            for (int t = 0; t < 2; ++t) {
                uint8_t* o = element + (t ? 318 : 128);
                o[186] = 12; o[187] = 7; o[188] = 40; o[189] = 14;
            }

            BS bs;
            BsCtor(&bs, c.bits, c.nbytes);
            const int32_t before = Remaining(&bs);
            const int32_t r = LrvqFrame(element, &bs);
            const int32_t used = before - Remaining(&bs);
            printf("FRAME %d ws=%d,%d tns=%d,%d filter=%d ret=%d bits=%d "
                   "ics0=%u,%u,%u,%u ics1=%u,%u,%u,%u tnsp=%u,%u tnsn=%u,%u\n",
                   i, c.ws0, c.ws1, c.tns0, c.tns1, c.tnsFilter, r, used,
                   *icsField(ics0, 10), *icsField(ics0, 11), *icsField(ics0, 12),
                   *(uint32_t*)icsField(ics0, 16),
                   *icsField(ics1, 10), *icsField(ics1, 11), *icsField(ics1, 12),
                   *(uint32_t*)icsField(ics1, 16),
                   element[128], element[318], element[129], element[319]);
            pb("TNS0", element + 128, 190);
            pb("TNS1", element + 318, 190);
            pf("SPEC0", spec0, 1024);
            pf("SPEC1", spec1, 1024);
        }
        return 0;
    }

    fprintf(stderr, "unknown command %s\n", cmd);
    return 2;
}
