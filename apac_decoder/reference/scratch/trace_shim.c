/* Vtable-slot tracer for the APAC frame layer.
 *
 * The library is linked DT_SYMBOLIC so LD_PRELOAD cannot interpose its
 * internals, and patching a function entry destroys the original.  But every
 * call the frame layer makes into an element, an ICSInfo or the core decoder
 * goes through an EXPORTED vtable, so replacing the slot gives a wrapper that
 * can still call the real function by its own symbol.
 *
 * Each line reports the bits still unread when the call was entered, so the
 * absolute bit position in a packet of L bytes is 8*L - remaining.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct BS { const unsigned char* cur; const unsigned char* end; unsigned cache; int bitsLeft; };

static long n_lines, limit;
static void* h;

static long left(struct BS* bs) { return (long)bs->bitsLeft + 8 * (long)(bs->end - bs->cur); }

#define WRAP1(tag, name, sym)                                                  \
    static long (*real_##name)(void*, struct BS*);                             \
    static long w_##name(void* self, struct BS* bs) {                          \
        long before = left(bs);                                                \
        long r = real_##name(self, bs);                                        \
        if (n_lines++ < limit)                                                 \
            fprintf(stderr, "[trace] %-22s in=%4ld out=%4ld rc=%ld\n",         \
                    tag, before, left(bs), r);                                 \
        return r;                                                              \
    }

WRAP1("SCE::Deserialize", sce, x)
WRAP1("CPE::Deserialize", cpe, x)
WRAP1("LFE::Deserialize", lfe, x)

static long (*real_ics)(void*, struct BS*, int);
static long w_ics(void* self, struct BS* bs, int ct)
{
    long before = left(bs);
    long r = real_ics(self, bs, ct);
    if (n_lines++ < limit)
        fprintf(stderr, "[trace] %-22s in=%4ld out=%4ld ct=%d rc=%ld  cur=%p end=%p cache=%08x bitsLeft=%d\n",
                "ICSInfo::Deserialize", before, left(bs), ct, r,
                (void*)bs->cur, (void*)bs->end, bs->cache, bs->bitsLeft);
    return r;
}

static long (*real_core)(void*, struct BS*, int);
static long w_core(void* self, struct BS* bs, int flag)
{
    long before = left(bs);
    fprintf(stderr, "[trace] Core::Deserialize ENTRY in=%ld cur=%p end=%p cache=%08x bitsLeft=%d\n",
            before, (void*)bs->cur, (void*)bs->end, bs->cache, bs->bitsLeft);
    long r = real_core(self, bs, flag);
    if (n_lines++ < limit)
        fprintf(stderr, "[trace] %-22s in=%4ld out=%4ld flag=%d rc=%ld\n",
                "Core::Deserialize", before, left(bs), flag, r);
    return r;
}

static void* reset_sce; static void* reset_cpe; static void* reset_lfe;
static long (*real_rsce)(void*); static long (*real_rcpe)(void*); static long (*real_rlfe)(void*);
static long w_rsce(void* s){ if(n_lines++<limit) fprintf(stderr,"[trace] SCE::Reset\n"); return real_rsce(s);} 
static long w_rcpe(void* s){ if(n_lines++<limit) fprintf(stderr,"[trace] CPE::Reset\n"); return real_rcpe(s);} 
static long w_rlfe(void* s){ if(n_lines++<limit) fprintf(stderr,"[trace] LFE::Reset\n"); return real_rlfe(s);} 

/* APACSynthesisFilterBank::FrequencyToTimeInPlace, vidx 4: the spectrum on the
   way in.  Dumping it here gives a per-channel spectral oracle for the whole
   frame layer without touching the bitstream. */
static long (*real_f2t)(void*, unsigned, float*, unsigned, unsigned);
static long n_f2t;
static long w_f2t(void* self, unsigned ch, float* buf, unsigned shape, unsigned seq)
{
    if (n_f2t < limit) {
        double s = 0; int nz = 0; int i;
        for (i = 0; i < 1024; ++i) { s += (double)buf[i] * buf[i]; if (buf[i] != 0.0f) ++nz; }
        fprintf(stderr, "[spec] ch=%u seq=%u nz=%d energy=%.9g first=%.9g %.9g %.9g %.9g\n",
                ch, seq, nz, s, buf[0], buf[1], buf[2], buf[3]);
    }
    ++n_f2t;
    return real_f2t(self, ch, buf, shape, seq);
}

static void unprotect(void* p)
{
    long ps = sysconf(_SC_PAGESIZE);
    void* pg = (void*)((uintptr_t)p & ~(uintptr_t)(ps - 1));
    if (mprotect(pg, 2 * ps, PROT_READ | PROT_WRITE)) perror("mprotect");
}

static void hook(const char* vt, int slot, void* wrapper, void** realOut)
{
    void** v = (void**)dlsym(h, vt);
    if (!v) { fprintf(stderr, "[trace] %s missing\n", vt); return; }
    unprotect(&v[slot]);
    *realOut = v[slot];
    v[slot] = wrapper;
}

__attribute__((constructor)) static void patch(void)
{
    limit = getenv("APAC_TRACE_N") ? atol(getenv("APAC_TRACE_N")) : 40;
    h = dlopen("/opt/apac/lib/libSPF_CP_PlugInWrapper.so", RTLD_NOW | RTLD_GLOBAL);
    if (!h) { fprintf(stderr, "[trace] dlopen failed\n"); return; }
    /* slot 2 is virtual index 0, so vidx k is slot k + 2. */
    hook("_ZTV11APACICSInfo",             4, (void*)w_ics,  (void**)&real_ics);
    hook("_ZTV24APACSingleChannelElement", 4, (void*)w_sce, (void**)&real_sce);
    hook("_ZTV22APACChannelPairElement",   4, (void*)w_cpe, (void**)&real_cpe);
    hook("_ZTV14APACLFEElement",           4, (void*)w_lfe, (void**)&real_lfe);
    hook("_ZTV24APACSingleChannelElement", 5, (void*)w_rsce, (void**)&real_rsce);
    hook("_ZTV22APACChannelPairElement",   5, (void*)w_rcpe, (void**)&real_rcpe);
    hook("_ZTV14APACLFEElement",           5, (void*)w_rlfe, (void**)&real_rlfe);
    hook("_ZTV18APACCoreLBRDecoder",       8, (void*)w_core, (void**)&real_core);
    hook("_ZTV23APACSynthesisFilterBank",  6, (void*)w_f2t,  (void**)&real_f2t);
    (void)reset_sce; (void)reset_cpe; (void)reset_lfe;
}
