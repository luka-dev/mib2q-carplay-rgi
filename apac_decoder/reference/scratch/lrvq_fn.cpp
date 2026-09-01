/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// scratch oracle driver #2: drives APACLRVQDecoder::LrvqDeserialize in the real
// library against a model reconstructed from its disassembly.
//   g++ -std=c++11 -O2 -o /tmp/lrvq_fn /src/scratch/lrvq_fn.cpp -ldl -lm
#include <dlfcn.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

struct BS { const uint8_t* cur; const uint8_t* end; uint32_t cache; int32_t bitsLeft; };
typedef void     (*fn_ctor)(BS*, const uint8_t*, uint32_t);
typedef uint32_t (*fn_getbits)(BS*, uint32_t);
typedef uint32_t (*fn_remain)(const BS*);
typedef int32_t  (*fn_lrvq)(BS*, uint8_t, uint8_t, void*, float*, float*, float*, int16_t*, uint16_t*);
typedef int32_t  (*fn_gain)(uint16_t, uint16_t, float*);
typedef int32_t  (*fn_lsfenv)(uint8_t, const uint16_t*, float*);
typedef int32_t  (*fn_ltpenv)(uint16_t, uint16_t, float*);

static fn_ctor ctor; static fn_getbits gb; static fn_remain rem;
static fn_lrvq lrvq; static fn_gain gain_fn; static fn_lsfenv lsfenv; static fn_ltpenv ltpenv;

// the mode table at 0x42b1f8, read out of the loaded image
struct Mode { uint16_t idx, lo, hi, bits; const float* t0; const float* t1;
              int16_t bits0, n0, bits1, n1; };
static const Mode* kModes;
static const float* kRamp;      // 0x32ff00, 32 entries
struct Spec { float* begin; float* end; };

static int model(BS* bs, uint8_t isShort, uint8_t isFirst, Spec* spec,
                 float* env1, float* env2, float* buf256, int16_t* buf64, uint16_t* prevMode)
{
    float g; uint16_t lsf[5];
    uint32_t gainMode = gb(bs, 1), gainIdx = gb(bs, 9);
    if (gain_fn((uint16_t)gainIdx, (uint16_t)gainMode, &g)) return 1;
    int noVq = 0, noLtpSection = 0, shortLsfCb = 0, reuseEnv = 0;
    if (!isFirst) {
        reuseEnv = (int)gb(bs, 1);
        if (isShort) { noVq = (int)gb(bs, 1); noLtpSection = 0; }
        else { noLtpSection = (int)gb(bs, 1); noVq = (int)gb(bs, 1); }
    }
    if (isFirst || !reuseEnv) {
        for (int i = 0; i < 5; ++i) lsf[i] = (uint16_t)gb(bs, 9);
        if (isShort) shortLsfCb = (int)gb(bs, 1);
        if (lsfenv((uint8_t)shortLsfCb, lsf, env1)) return 1;
    }
    int ltpPresent = 0;
    if (!isShort && !noLtpSection) {
        ltpPresent = (int)gb(bs, 1);
        if (ltpPresent) {
            uint32_t lg = gb(bs, 7), lag = gb(bs, 10);
            if (ltpenv((uint16_t)lg, (uint16_t)lag, env2)) return 1;
            if (noVq) { for (int i = 0; i < 256; ++i) env1[i] *= env2[i]; goto out; }
            noVq = 1;                       // var_94 now means "LTP envelope present"
        } else {
            if (noVq) goto out;
            noVq = 0;
        }
    } else if (noVq) goto out;
    {
        uint32_t vq = gb(bs, 4);
        int same = (!isFirst && *prevMode == vq) ? 1 : 0;
        *prevMode = (uint16_t)vq;
        const Mode& m = kModes[vq];
        if (!m.t0 && !m.t1) return 1;
        if (((uint32_t)m.bits0 | (uint32_t)m.bits1) == 0) return 1;
        for (int k = 0; k < 32; ++k) {
            int16_t i0, i1 = -1;
            if (same && gb(bs, 1)) i0 = buf64[2 * k];
            else                   i0 = (int16_t)gb(bs, (uint32_t)m.bits0);
            if ((int)m.n0 < (int)i0) return 1;
            if (!m.t1) { for (int n = 0; n < 8; ++n) buf256[32 * n + k] = m.t0[8 * i0 + n] + 0.f; }
            else {
                if (same && gb(bs, 1)) i1 = buf64[2 * k + 1];
                else                   i1 = (int16_t)gb(bs, (uint32_t)m.bits1);
                if ((int)(uint16_t)m.n1 < (int)i1) return 1;
                for (int n = 0; n < 8; ++n)
                    buf256[32 * n + k] = (m.t0[8 * i0 + n] + m.t1[8 * i1 + n]) * 0.5f;
            }
            buf64[2 * k] = i0; buf64[2 * k + 1] = i1;
        }
        if (!isShort && noVq) for (int i = 0; i < 256; ++i) env1[i] *= env2[i];
    }
out:
    if (isShort) {
        for (int i = 0; i < 256; ++i) buf256[i] = buf256[i] / env1[i];
        if (shortLsfCb) {
            for (int i = 0; i < 256; ++i) buf256[i] = buf256[i] * g;
            for (int w = 0; w < 8; ++w) {
                float* src = buf256 + 32 * w; float* dst = spec->begin + 128 * w;
                if (w & 1) memcpy(dst, src, 128);
                else for (int i = 0; i < 32; ++i) dst[i] = src[31 - i];
                for (int i = 0; i < 32; ++i) dst[i] = dst[i] / kRamp[i];
            }
        } else {
            for (int w = 0; w < 8; ++w)
                for (int n = 0; n < 32; ++n) spec->begin[128 * w + n] = buf256[w + 8 * n] * g;
        }
    } else {
        for (int i = 0; i < 256; ++i) buf256[i] = buf256[i] / env1[i];
        for (int i = 0; i < 256; ++i) spec->begin[i] = buf256[i] * g;
    }
    return 0;
}

// ------------------------------------------------------------------ writer ---
struct BW { std::vector<uint8_t> b; size_t p = 0;
    BW(size_t n) : b(n, 0) {}
    void put(uint32_t v, int n) { for (int i = n - 1; i >= 0; --i) { if ((v >> i) & 1) b[p >> 3] |= 0x80u >> (p & 7); ++p; } } };

static uint64_t rs = 0x9e3779b97f4a7c15ULL;
static uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 32); }

int main(int argc, char** argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    const char* lp = getenv("APAC_LIB") ? getenv("APAC_LIB") : "/opt/apac/lib/libSPF_CP_PlugInWrapper.so";
    void* h = dlopen(lp, RTLD_NOW | RTLD_GLOBAL);
    if (!h) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    ctor    = (fn_ctor)   dlsym(h, "_ZN16TBitstreamReaderIjEC1EPKhj");
    gb      = (fn_getbits)dlsym(h, "_ZN16TBitstreamReaderIjE7GetBitsEj");
    rem     = (fn_remain) dlsym(h, "_ZNK16TBitstreamReaderIjE13BitsRemainingEv");
    lrvq    = (fn_lrvq)   dlsym(h, "_ZN15APACLRVQDecoder15LrvqDeserializeER16TBitstreamReaderIjEbbR16APACSpectralDataRN4apac13aligned_arrayIfLm1024EEES8_RNS6_IfLm256EEERNS6_IsLm64EEEPt");
    gain_fn = (fn_gain)   dlsym(h, "_ZN15APACLRVQDecoder18dequantizeLrvqGainEttPf");
    lsfenv  = (fn_lsfenv) dlsym(h, "_ZN15APACLRVQDecoder26lsfsToMdctSpectralEnvelopeEbRKN4apac13aligned_arrayItLm5EEERNS1_IfLm1024EEE");
    ltpenv  = (fn_ltpenv) dlsym(h, "_ZN15APACLRVQDecoder25ltpToMdctSpectralEnvelopeEttRN4apac13aligned_arrayIfLm1024EEE");
    if (!ctor || !gb || !rem || !lrvq || !gain_fn || !lsfenv || !ltpenv) { fprintf(stderr, "sym\n"); return 2; }
    Dl_info di; dladdr((void*)lrvq, &di); uint8_t* base = (uint8_t*)di.dli_fbase;
    kModes = (const Mode*)(base + 0x42b1f8);
    kRamp  = (const float*)(base + 0x32ff00);
    if (kModes[0].bits0 != 6 || kModes[10].bits1 != 8 || fabsf(kRamp[31] - 2.0f) > 1e-6f) { fprintf(stderr, "tables\n"); return 2; }

    int trials = argc > 1 ? atoi(argv[1]) : 400;
    int fails = 0, deep = 0;
    int cSingle=0,cTwo=0,cErr=0,cSame=0,cLtp=0,cNoVq=0,cRev=0,cFwd=0;
    for (int t = 0; t < trials; ++t) {
      for (int isShort = 0; isShort < 2; ++isShort) {
        // build a two-channel (CPE-like) stream: channel 0 then channel 1
        BW w(4096);
        uint32_t vq0 = rnd() % 12, vq1 = (rnd() & 1) ? vq0 : (rnd() % 12);
        int ltp0 = rnd() & 1, reuseEnv = rnd() & 1, noLtpSec = rnd() & 1, noVq = rnd() % 4 == 0;
        int ltp1 = rnd() & 1;
        auto emitVq = [&](uint32_t vq, int same) {
            w.put(vq, 4);
            const Mode& m = kModes[vq];
            if (!m.t0 && !m.t1) return;
            for (int k = 0; k < 32; ++k) {
                if (same) { if (rnd() & 1) { w.put(1, 1); } else { w.put(0, 1); w.put(rnd() % (uint32_t)m.n0, m.bits0); } }
                else w.put(rnd() % (uint32_t)m.n0, m.bits0);
                if (m.t1) {
                    if (same) { if (rnd() & 1) { w.put(1, 1); } else { w.put(0, 1); w.put(rnd() % (uint32_t)m.n1, m.bits1); } }
                    else w.put(rnd() % (uint32_t)m.n1, m.bits1);
                }
            }
        };
        // channel 0
        w.put(rnd() & 1, 1); w.put(rnd() % 512, 9);
        for (int i = 0; i < 5; ++i) w.put(rnd() % 512, 9);
        int sc0 = rnd() & 1;
        if (isShort) w.put(sc0, 1);
        if (isShort) { if (sc0) ++cRev; else ++cFwd; }
        if (!isShort) { w.put(ltp0, 1); if (ltp0) { w.put(rnd() % 128, 7); w.put(rnd() % 1024, 10); } }
        emitVq(vq0, 0);
        // channel 1
        w.put(rnd() & 1, 1); w.put(rnd() % 512, 9);
        w.put(reuseEnv, 1);
        if (isShort) w.put(noVq, 1); else { w.put(noLtpSec, 1); w.put(noVq, 1); }
        if (!reuseEnv) { for (int i = 0; i < 5; ++i) w.put(rnd() % 512, 9); if (isShort) w.put(rnd() & 1, 1); }
        if (!isShort && !noLtpSec) { w.put(ltp1, 1); if (ltp1) { w.put(rnd() % 128, 7); w.put(rnd() % 1024, 10); } }
        if (!(noVq && (isShort || noLtpSec || !ltp1))) { if (!(noVq && !isShort && !noLtpSec && ltp1)) emitVq(vq1, (vq1 == vq0) ? 1 : 0); }
        for (size_t i = w.p; i < w.b.size() * 8; ++i) if (rnd() & 1) w.b[i >> 3] |= 0x80u >> (i & 7);

        // run both sides
        static float envA[1024], envB[1024], bufA[256], sA[1024];
        static float envC[1024], envD[1024], bufB[256], sB[1024];
        static int16_t q64A[64], q64B[64];
        for (int i = 0; i < 1024; ++i) { envA[i] = envC[i] = 1.f; envB[i] = envD[i] = 1.f; sA[i] = sB[i] = -777.f; }
        for (int i = 0; i < 256; ++i) bufA[i] = bufB[i] = 0.125f * (i + 1);
        memset(q64A, 0, sizeof q64A); memset(q64B, 0, sizeof q64B);
        uint16_t pmA = 0, pmB = 0;
        Spec spA = { sA, sA + 1024 }, spB = { sB, sB + 1024 };
        BS b1, b2; ctor(&b1, w.b.data(), (uint32_t)w.b.size()); ctor(&b2, w.b.data(), (uint32_t)w.b.size());
        uint32_t r0 = rem(&b1);
        int ra = lrvq(&b1, (uint8_t)isShort, 1, &spA, envA, envB, bufA, q64A, &pmA);
        int rb = model(&b2, (uint8_t)isShort, 1, &spB, envC, envD, bufB, q64B, &pmB);
        int ra2 = 0, rb2 = 0;
        if (!ra && !rb) { ra2 = lrvq(&b1, (uint8_t)isShort, 0, &spA, envA, envB, bufA, q64A, &pmA);
                          rb2 = model(&b2, (uint8_t)isShort, 0, &spB, envC, envD, bufB, q64B, &pmB); }
        uint32_t ua = r0 - rem(&b1), ub = r0 - rem(&b2);
        bool bad = ra != rb || ra2 != rb2 || ua != ub || pmA != pmB
                || memcmp(q64A, q64B, sizeof q64A) || memcmp(sA, sB, sizeof sA)
                || memcmp(bufA, bufB, sizeof bufA) || memcmp(envA, envC, 1024 * 4);
        if (!ra && !ra2) {
            ++deep;
            if (vq0 < 4) ++cSingle; else if (vq0 < 11) ++cTwo;
            if (vq1 > 10) ++cErr;
            if (vq1 == vq0) ++cSame;
            if (!isShort && ltp0) ++cLtp;
            if (noVq) ++cNoVq;
        }
        if (ra == 1 || ra2 == 1) ++cErr;
        if (bad) {
            if (fails < 3) {
                printf("MISMATCH t=%d isShort=%d vq0=%u vq1=%u ret %d/%d %d/%d bits %u/%u pm %u/%u\n",
                       t, isShort, vq0, vq1, ra, rb, ra2, rb2, ua, ub, pmA, pmB);
                for (int i = 0; i < 64; ++i) if (q64A[i] != q64B[i]) { printf("  q64[%d] %d vs %d\n", i, q64A[i], q64B[i]); break; }
                for (int i = 0; i < 1024; ++i) if (memcmp(&sA[i], &sB[i], 4)) { printf("  spec[%d] %.9g vs %.9g\n", i, sA[i], sB[i]); break; }
                for (int i = 0; i < 256; ++i) if (memcmp(&bufA[i], &bufB[i], 4)) { printf("  buf[%d] %.9g vs %.9g\n", i, bufA[i], bufB[i]); break; }
                for (int i = 0; i < 1024; ++i) if (memcmp(&envA[i], &envC[i], 4)) { printf("  env[%d] %.9g vs %.9g\n", i, envA[i], envC[i]); break; }
            }
            ++fails;
        }
      }
    }
    printf("coverage: single-stage=%d two-stage=%d error-mode=%d same-mode-reuse=%d ltp=%d reuse-residual=%d shortRev=%d shortFwd=%d\n",
           cSingle, cTwo, cErr, cSame, cLtp, cNoVq, cRev, cFwd);
    printf("%s: %d/%d comparisons matched (%d reached the second channel cleanly)\n",
           fails ? "FAIL" : "ALL MATCH", trials * 2 - fails, trials * 2, deep);
    return fails ? 1 : 0;
}
