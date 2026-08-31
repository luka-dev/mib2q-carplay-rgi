/* Reachability probe.  APAC_STUB is a comma-separated list of mangled names;
   each named function's entry is overwritten with `MOV W0,#1 ; RET`.  If a
   function is never entered on this content the decode is unchanged; if it is,
   the 1 propagates as a decode error.  Same in-memory patch as stub_shim.c -
   the library is DT_SYMBOLIC so LD_PRELOAD cannot interpose its internals. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

__attribute__((constructor)) static void patch(void)
{
    const char* list = getenv("APAC_STUB");
    if (!list) return;
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", list);
    void* h = dlopen("/opt/apac/lib/libSPF_CP_PlugInWrapper.so", RTLD_NOW | RTLD_GLOBAL);
    long ps = sysconf(_SC_PAGESIZE);
    for (char* s = strtok(buf, ","); s; s = strtok(NULL, ",")) {
        void* p = h ? dlsym(h, s) : 0;
        if (!p) { fprintf(stderr, "[reach] %s NOT FOUND\n", s); continue; }
        void* pg = (void*)((uintptr_t)p & ~(uintptr_t)(ps - 1));
        if (mprotect(pg, 2 * ps, PROT_READ | PROT_WRITE | PROT_EXEC)) { perror("mprotect"); continue; }
        uint32_t* i = (uint32_t*)p;
        i[0] = getenv("APAC_STUB_RET0") ? 0x52800000u : 0x52800020u;
        i[1] = 0xd65f03c0;   /* RET        */
        __builtin___clear_cache((char*)p, (char*)p + 8);
        fprintf(stderr, "[reach] stubbed %s\n", s);
    }
}
