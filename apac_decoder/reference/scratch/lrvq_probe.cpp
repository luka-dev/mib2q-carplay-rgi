// scratch oracle driver: drives APACSpectralData::Deserialize in the real
// Pioneer/Apple library with crafted bitstreams and compares its output with a
// model built from the hypothesised Huffman-codebook semantics.
//
//   g++ -std=c++11 -O2 -o /tmp/lrvq_probe /src/scratch/lrvq_probe.cpp -ldl -lm
//
// Owned by the LRVQ-semantics investigation; not part of the port.
#include <dlfcn.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

static uint64_t rngstate = 0x243f6a8885a308d3ULL;
static uint32_t rnd() { rngstate ^= rngstate << 13; rngstate ^= rngstate >> 7; rngstate ^= rngstate << 17; return (uint32_t)(rngstate >> 32); }

// ---------------------------------------------------------------- library ---
struct BS { const uint8_t* cur; const uint8_t* end; uint32_t cache; int32_t bitsLeft; };

typedef void     (*fn_bs_ctor)(BS*, const uint8_t*, uint32_t);
typedef uint32_t (*fn_bs_getbits)(BS*, uint32_t);
typedef uint32_t (*fn_bs_remain)(const BS*);
typedef void     (*fn_bs_putback)(BS*, uint32_t);
typedef int64_t  (*fn_spec_des)(void*, BS*, const void*, const void*, const void*);
typedef void**   (*fn_gethuff)(uint32_t);

static fn_bs_ctor    bs_ctor;
static fn_bs_getbits bs_getbits;
static fn_bs_remain  bs_remain;
static fn_spec_des   spec_des;
static fn_gethuff    gethuff;
static fn_bs_putback bs_putback;

// -------------------------------------------------------- recovered layout ---
struct HuffEntry { uint64_t a; const uint16_t* payload; };
struct HuffTable { const HuffEntry* e; uint64_t n; };

struct SFData {                 // APACScaleFactorData, 728 bytes
    float    scf[120];          // +0    stride 15 per window group
    uint8_t  nrec[8];           // +480  records per window group
    uint16_t rec[120];          // +488  stride 15; low byte codebook, high byte band run
};
struct SFBOffsets { uint16_t nbands; uint16_t pad[3]; const uint16_t* off; };
struct SpecData  { float* begin; float* end; };

static const float* kX43;       // the i^(4/3) table, index -4..1023

// ------------------------------------------------------------------ model ---
struct Model {
    BS* bs; uint32_t w; int avail;
    void needbits(int k) { if (avail < k) { if (avail < 32) w |= bs_getbits(bs, 32 - avail); avail = 32; } }
    uint16_t huff(const HuffTable* t) {
        uint32_t mask = (1u << (32 - t->n)) - 1;
        int k = __builtin_clz(mask | ~w);
        const HuffEntry& e = t->e[k];
        uint32_t idx = (w << k) >> e.a;
        uint16_t r = e.payload[idx];
        int L = r & 0x3f;
        avail -= L; w <<= L;
        return r;
    }
    bool signbit_() { bool neg = (int32_t)w < 0; w <<= 1; --avail; return neg; }
    // the cb11 escape, transcribed from APACSpectralData::Deserialize
    uint32_t esc() {
        if (avail <= 0x14) { if (avail < 32) w |= bs_getbits(bs, 32 - avail); avail = 32; }
        uint32_t k  = __builtin_clz(~w | 0x800000);
        uint32_t nb = k + 4;
        uint32_t t  = w << (k + 1);
        avail -= (int)(k + 1 + nb);
        uint32_t v = (1u << nb) + (t >> ((32 - nb) & 31));
        w = t << nb;
        return v;
    }
    static float mag(uint32_t v) { return v > 1023 ? (float)pow((double)v, 4.0 / 3.0) : kX43[v]; }
};

static void model_run(BS* bs, const SFData* sf, const SFBOffsets* sfb, float* out, uint32_t nwin)
{
    Model M; M.bs = bs; M.w = 0; M.avail = 0;
    const uint16_t* off = sfb->off;
    uint32_t coefsPerWin = off[sfb->nbands];
    (void)coefsPerWin; (void)nwin;
    float* p = out;
    uint32_t band = 0, prev = 0;
    for (uint32_t r = 0; r < sf->nrec[0]; ++r) {
        uint16_t rec = sf->rec[r];
        uint32_t cb = rec & 0xff, run = rec >> 8;
        band += run;
        if (!cb) { uint32_t endc = off[band]; while (prev < endc) { *p++ = 0.f; ++prev; } continue; }
        const HuffTable* t = (const HuffTable*)gethuff(cb);
        bool zf = (cb & 1) != 0;
        for (uint32_t b = prev ? 0 : 0; ; ) { (void)b; break; }
        // one scalefactor per band in the run
        for (uint32_t bb = band - run + 1; bb <= band; ++bb) {
            float sfv = sf->scf[bb - 1];
            uint32_t endc = off[bb], n = endc - prev;
            if (cb <= 2) {
                for (uint32_t i = 0; i < n; i += 4) {
                    M.needbits((int)t->n);
                    if (zf && (int32_t)M.w >= 0) { M.w <<= 1; --M.avail; p[0]=p[1]=p[2]=p[3]=0.f; }
                    else { uint16_t r16 = M.huff(t);
                        int q0 = (int16_t)r16 >> 14;
                        int q1 = (int)((int16_t)(r16 << 2)) >> 14;
                        int q2 = (int)((int16_t)(r16 << 4)) >> 14;
                        int q3 = (int)((int16_t)(r16 << 6)) >> 14;
                        p[0]=kX43[q0]*sfv; p[1]=kX43[q1]*sfv; p[2]=kX43[q2]*sfv; p[3]=kX43[q3]*sfv; }
                    p += 4;
                }
            } else if (cb <= 4) {
                for (uint32_t i = 0; i < n; i += 4) {
                    M.needbits((int)t->n + 4);
                    if (zf && (int32_t)M.w >= 0) { M.w <<= 1; --M.avail; p[0]=p[1]=p[2]=p[3]=0.f; }
                    else { uint16_t r16 = M.huff(t);
                        for (int q = 0; q < 4; ++q) {
                            int m = (r16 >> (14 - 2*q)) & 3;
                            if (!m) { p[q] = 0.f; continue; }
                            if (M.signbit_()) m = -m;
                            p[q] = kX43[m] * sfv; } }
                    p += 4;
                }
            } else if (cb <= 6) {
                for (uint32_t i = 0; i < n; i += 2) {
                    M.needbits((int)t->n);
                    if (zf && (int32_t)M.w >= 0) { M.w <<= 1; --M.avail; p[0]=p[1]=0.f; }
                    else { uint16_t r16 = M.huff(t);
                        int a0 = (int)((int16_t)(r16 << 1)) >> 12;
                        int a1 = (int)((int16_t)(r16 << 6)) >> 12;
                        p[0]=kX43[a0]*sfv; p[1]=kX43[a1]*sfv; }
                    p += 2;
                }
            } else if (cb <= 10) {
                for (uint32_t i = 0; i < n; i += 2) {
                    M.needbits((int)t->n + 2);
                    if (zf && (int32_t)M.w >= 0) { M.w <<= 1; --M.avail; p[0]=p[1]=0.f; }
                    else { uint16_t r16 = M.huff(t);
                        uint32_t m0 = (r16 >> 11) & 0xF, m1 = (r16 >> 6) & 0xF;
                        if (m0) { float v = kX43[m0]; if (M.signbit_()) v = -v; p[0] = v * sfv; } else p[0] = 0.f;
                        if (m1) { float v = kX43[m1]; if (M.signbit_()) v = -v; p[1] = v * sfv; } else p[1] = 0.f; }
                    p += 2;
                }
            } else {            /* cb == 11 */
                for (uint32_t i = 0; i < n; i += 2) {
                    M.needbits((int)t->n + 2);
                    uint16_t r16 = M.huff(t);
                    uint32_t m0 = r16 >> 11, m1 = (r16 >> 6) & 0x1F;
                    bool s0 = false, s1 = false;
                    if (m0) s0 = M.signbit_();
                    if (m1) s1 = M.signbit_();
                    if (!m0 && !m1) { p[0] = 0.f * sfv; p[1] = 0.f * sfv; }
                    else {
                        float v0 = Model::mag(m0 == 16 ? M.esc() : m0); if (s0) v0 = -v0;
                        p[0] = v0 * sfv;
                        float v1 = Model::mag(m1 == 16 ? M.esc() : m1); if (s1) v1 = -v1;
                        p[1] = v1 * sfv;
                    }
                    p += 2;
                }
            }
            prev = endc;
        }
    }
    // mirror the library's trailing PutBack
    bs_putback(bs, (uint32_t)M.avail);
}


// ---------------------------------------------------------------- SCF test ---
// Model of APACScaleFactorData::Deserialize (0x9a140), the one place that uses
// tbl->n as a shift (0x80000000u >> n).
typedef int64_t (*fn_sfd_des)(void*, BS*, const void*);
static fn_sfd_des sfd_des;
static const float* kPow2q;      // 2^(k/4), k = 0..3, at 0x310280

static int sfd_model(BS* bs, const uint8_t* ics, SFData* out)
{
    const HuffTable* t = (const HuffTable*)gethuff(0);
    uint32_t nmask = 0x80000000u >> t->n;
    int runBits = (ics[10] == 2) ? 3 : 5;
    int sfv = (int)bs_getbits(bs, 8);
    if (!ics[12]) return 0;
    for (uint32_t g = 0; g < ics[12]; ++g) {
        uint32_t band = 0, done = 0, nrec = 0;
        if (ics[11]) for (;;) {
            uint32_t cb = bs_getbits(bs, 4);
            if (cb > 11) return 1;
            uint32_t run = 0, f;
            do { f = bs_getbits(bs, runBits);
                 run += f;
                 if (run + band > ics[11]) return 1;
            } while (f == (1u << runBits) - 1u);
            if (!run) return 1;
            done += run;
            out->rec[15 * g + nrec] = (uint16_t)(cb | (run << 8));
            if (cb) {
                uint32_t w = 0; int avail = 0; int e = sfv - 100;
                for (;;) {
                    if ((uint32_t)t->n > (uint32_t)avail) {
                        if (avail <= 31) w |= bs_getbits(bs, 32 - avail);
                        avail = 32;
                    }
                    uint32_t bits;
                    if ((int32_t)w < 0) {
                        int k = __builtin_clz(nmask | ~w);
                        const HuffEntry& en = t->e[k];
                        uint16_t r = en.payload[(w << k) >> en.a];
                        avail -= r & 0x3f; w <<= r & 0x3f;
                        sfv += (int16_t)r >> 6;
                        if (sfv > 255)       { sfv = 255;  e = 155;  bits = 0x52D744FDu; }
                        else if (sfv < -255) { sfv = -256; e = -356; bits = 0x13000000u; }
                        else { e = sfv - 100; bits = *(const uint32_t*)&kPow2q[sfv & 3] + ((uint32_t)(e >> 2) << 23); }
                    } else {
                        w <<= 1; --avail;
                        bits = *(const uint32_t*)&kPow2q[((e + 100) & 3)] + ((uint32_t)(e >> 2) << 23);
                    }
                    memcpy(&out->scf[15 * g + band], &bits, 4);
                    ++band;
                    if (band >= done) break;
                }
                bs_putback(bs, (uint32_t)avail);
            } else {
                for (uint32_t i = 0; i < run; ++i) out->scf[15 * g + band + i] = 0.f;
                band += run;
            }
            ++nrec;
            if (done >= ics[11]) { out->nrec[g] = (uint8_t)nrec; break; }
        }
        else out->nrec[g] = 0;
    }
    return 0;
}

static int scf_test(int trials)
{
    alignas(16) uint8_t ics[4096];
    int fails = 0;
    for (int mode = 0; mode < 3; ++mode) {
        int bad = 0, ok = 0;
        for (int t = 0; t < trials; ++t) {
            memset(ics, 0, sizeof ics);
            ics[10] = mode ? 2 : 0;
            ics[11] = mode ? 14 : 40;      // bands
            ics[12] = (mode == 2) ? 8 : 1; // window groups
            const int runBits_ = mode ? 3 : 5;
            // structured stream: valid record headers, random scalefactor payload,
            // so the decode actually reaches the codebook-0 Huffman loop.
            std::vector<uint8_t> buf(8192, 0);
            { size_t bp = 0;
              auto put = [&](uint32_t v, int n) { for (int i = n - 1; i >= 0; --i) { if ((v >> i) & 1) buf[bp >> 3] |= 0x80u >> (bp & 7); ++bp; } };
              put(rnd() & 0xff, 8);
              bool single = mode != 2 && (t & 1) != 0;   // one record per group: the rest of the
                                            // stream is payload, so random bits stay in sync
              for (uint32_t g = 0; g < ics[12]; ++g) {
                  uint32_t left = ics[11];
                  while (left) {
                      if (single) {
                          uint32_t cb1 = 1 + rnd() % 11, rem1 = left, esc1 = (1u << runBits_) - 1u;
                          put(cb1, 4);
                          while (rem1 >= esc1) { put(esc1, runBits_); rem1 -= esc1; }
                          put(rem1, runBits_);
                          left = 0;
                          break;
                      }
                      uint32_t cb = rnd() % 12;
                      uint32_t run = 1 + rnd() % left;
                      put(cb, 4);
                      uint32_t rem = run, esc = (1u << runBits_) - 1u;
                      while (rem >= esc) { put(esc, runBits_); rem -= esc; }
                      put(rem, runBits_);
                      left -= run;
                      if (cb) {
                          if (mode == 2) { for (uint32_t i = 0; i < run; ++i) put(0, 1); }   // every delta = the 1-bit '0' code
                          else           { for (uint32_t i = 0; i < run * 3 + 8; ++i) put(rnd() & 1, 1); }
                      }
                  }
              }
              for (; bp < buf.size() * 8; ++bp) if (rnd() & 1) buf[bp >> 3] |= 0x80u >> (bp & 7);
            }
            SFData A, B; memset(&A, 0, sizeof A); memset(&B, 0, sizeof B);
            BS b1, b2; bs_ctor(&b1, buf.data(), (uint32_t)buf.size());
            bs_ctor(&b2, buf.data(), (uint32_t)buf.size());
            uint32_t r0 = bs_remain(&b1);
            int64_t ra = sfd_des(&A, &b1, ics);
            uint32_t ua = r0 - bs_remain(&b1);
            int rb = sfd_model(&b2, ics, &B);
            uint32_t ub = r0 - bs_remain(&b2);
            if (ra == 0) ++ok;
            bool m = (ra != rb) || (ra == 0 && (ua != ub || memcmp(&A, &B, sizeof A)));
            if (m) {
                if (!bad) {
                    printf("SCF mode%d MISMATCH trial %d ret lib=%lld model=%d bits %u/%u\n",
                           mode, t, (long long)ra, rb, ua, ub);
                    for (int i = 0; i < 120; ++i)
                        if (memcmp(&A.scf[i], &B.scf[i], 4)) { printf("   scf[%d] %g vs %g\n", i, A.scf[i], B.scf[i]); break; }
                    for (int i = 0; i < 120; ++i)
                        if (A.rec[i] != B.rec[i]) { printf("   rec[%d] %04x vs %04x\n", i, A.rec[i], B.rec[i]); break; }
                    for (int i = 0; i < 8; ++i)
                        if (A.nrec[i] != B.nrec[i]) { printf("   nrec[%d] %u vs %u\n", i, A.nrec[i], B.nrec[i]); break; }
                }
                ++bad; ++fails;
            }
        }
        printf("SCF %s: %d/%d trials matched (%d decoded without error)\n",
               mode == 0 ? "long  (1 group, 5-bit runs, random payload)" :
               mode == 1 ? "short (1 group, 3-bit runs, random payload)" :
                           "short (8 groups, 3-bit runs, all-zero deltas)", trials - bad, trials, ok);
    }
    return fails;
}

// ------------------------------------------------------------------- main ---

int main(int argc, char** argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    const char* libp = getenv("APAC_LIB") ? getenv("APAC_LIB") : "/opt/apac/lib/libSPF_CP_PlugInWrapper.so";
    void* h = dlopen(libp, RTLD_NOW | RTLD_GLOBAL);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
    bs_ctor    = (fn_bs_ctor)   dlsym(h, "_ZN16TBitstreamReaderIjEC1EPKhj");
    bs_getbits = (fn_bs_getbits)dlsym(h, "_ZN16TBitstreamReaderIjE7GetBitsEj");
    bs_remain  = (fn_bs_remain) dlsym(h, "_ZNK16TBitstreamReaderIjE13BitsRemainingEv");
    spec_des   = (fn_spec_des)  dlsym(h, "_ZN16APACSpectralData11DeserializeER16TBitstreamReaderIjERK11APACICSInfoRK19APACScaleFactorDataRKN4apac10SFBOffsetsE");
    gethuff    = (fn_gethuff)   dlsym(h, "_ZN4apac15GetHuffmanTableEj");
    bs_putback = (fn_bs_putback)dlsym(h, "_ZN16TBitstreamReaderIjE7PutBackEj");
    sfd_des    = (fn_sfd_des)   dlsym(h, "_ZN19APACScaleFactorData11DeserializeER16TBitstreamReaderIjERK11APACICSInfo");
    if (!bs_ctor || !bs_getbits || !bs_remain || !spec_des || !gethuff) { fprintf(stderr, "missing symbol\n"); return 2; }

    // the i^(4/3) table: reached from the library image via the known offset of
    // GetHuffmanTable's own page; instead take it from the caller-supplied base.
    void* base = 0;
    { Dl_info di; if (dladdr((void*)gethuff, &di)) base = di.dli_fbase; }
    kX43 = (const float*)((uint8_t*)base + 0x3308c0);
    kPow2q = (const float*)((uint8_t*)base + 0x310280);
    if (fabsf(kX43[1] - 1.0f) > 1e-6f || fabsf(kX43[8] - 16.0f) > 1e-4f || fabsf(kX43[-1] + 1.0f) > 1e-6f) {
        fprintf(stderr, "x^(4/3) table not where expected (%g %g %g)\n", kX43[-1], kX43[1], kX43[8]); return 2;
    }

    int trials = argc > 1 ? atoi(argv[1]) : 200;
    const uint32_t NB = 16, W = 8;            // 16 bands of 8 coefficients
    uint16_t off[NB + 1]; for (uint32_t i = 0; i <= NB; ++i) off[i] = (uint16_t)(i * W);
    SFBOffsets sfb; memset(&sfb, 0, sizeof sfb); sfb.nbands = NB; sfb.off = off;

    alignas(16) uint8_t ics[4096];
    int fails = 0, ran = 0;
    for (int cb = 1; cb <= 11; ++cb) {
        int cbfail = 0;
        for (int t = 0; t < trials; ++t) {
            SFData sf; memset(&sf, 0, sizeof sf);
            sf.nrec[0] = (uint8_t)NB;
            for (uint32_t i = 0; i < NB; ++i) { sf.rec[i] = (uint16_t)(cb | (1u << 8)); sf.scf[i] = 1.0f + 0.25f * i; }
            memset(ics, 0, sizeof ics);
            ics[10] = 0; ics[11] = (uint8_t)NB; ics[12] = 1; *(uint32_t*)(ics + 16) = 1;

            std::vector<uint8_t> buf(2048);
            for (size_t i = 0; i < buf.size(); ++i) buf[i] = (uint8_t)rnd();

            std::vector<float> a(NB * W + 64, -777.f), b(NB * W + 64, -777.f);
            SpecData sda = { a.data(), a.data() + NB * W };
            BS bs1, bs2;
            bs_ctor(&bs1, buf.data(), (uint32_t)buf.size());
            bs_ctor(&bs2, buf.data(), (uint32_t)buf.size());
            uint32_t r0 = bs_remain(&bs1);
            spec_des(&sda, &bs1, ics, &sf, &sfb);
            uint32_t used_lib = r0 - bs_remain(&bs1);
            model_run(&bs2, &sf, &sfb, b.data(), 1);
            uint32_t used_mod = r0 - bs_remain(&bs2);
            ++ran;
            bool bad = used_lib != used_mod;
            for (uint32_t i = 0; i < NB * W && !bad; ++i)
                if (memcmp(&a[i], &b[i], 4) != 0) bad = true;
            if (bad) {
                if (cbfail == 0) {
                    printf("cb %2d MISMATCH trial %d: bits lib=%u model=%u\n", cb, t, used_lib, used_mod);
                    for (uint32_t i = 0; i < NB * W; ++i)
                        if (memcmp(&a[i], &b[i], 4)) { printf("   [%3u] lib=%.9g model=%.9g\n", i, a[i], b[i]); if (i > 8) break; }
                }
                ++cbfail; ++fails;
            }
        }
        printf("cb %2d: %d/%d trials matched (floats bit-exact + bit count)\n", cb, trials - cbfail, trials);
    }
    printf("\n");
    fails += scf_test(trials);
    printf("\n%s  (%d spectral comparisons, %d total failures)\n", fails ? "FAIL" : "ALL MATCH", ran, fails);
    return fails ? 1 : 0;
}
