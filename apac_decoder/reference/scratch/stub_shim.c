/* The library is linked DT_SYMBOLIC, so LD_PRELOAD cannot interpose its own
   internals.  Instead patch the entry of a chosen function to `return 1` and
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
   see whether decoding the capture changes at all.  APAC_STUB names it. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

__attribute__((constructor)) static void patch(void)
{
    const char* sym = getenv("APAC_STUB");
    if (!sym) return;
    void* h = dlopen("/opt/apac/lib/libSPF_CP_PlugInWrapper.so", RTLD_NOW | RTLD_GLOBAL);
    void* p = h ? dlsym(h, sym) : 0;
    if (!p) { fprintf(stderr, "[stub] %s not found\n", sym); return; }
    long ps = sysconf(_SC_PAGESIZE);
    void* pg = (void*)((uintptr_t)p & ~(uintptr_t)(ps - 1));
    if (mprotect(pg, 2 * ps, PROT_READ | PROT_WRITE | PROT_EXEC)) { perror("mprotect"); return; }
    uint32_t* i = (uint32_t*)p;
    i[0] = 0x52800020;   /* MOV W0, #1 */
    i[1] = 0xd65f03c0;   /* RET        */
    __builtin___clear_cache((char*)p, (char*)p + 8);
    fprintf(stderr, "[stub] %s stubbed to return 1\n", sym);
}
