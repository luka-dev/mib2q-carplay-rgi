/* Pass-through tracer for libcsdIpcClient.so on the live head unit.
 *
 * Why this exists: everything from the APAC decoder down to deva's DMA ring is measured
 * correct, the head unit already requests the 5.1 sink unconditionally, and the amplifier's
 * SHARC matrix routes C/LFE/Ls/Rs into the front buses -- yet a one-hot test shows only two
 * channels reaching speakers.  The one boundary nobody has observed on real hardware is CSD:
 * what deva tells the Qualcomm ADSP, and how many bytes it hands over per tick.
 *
 * It replaces the library file itself rather than needing an LD_PRELOAD, because io-audio's
 * launch is not a shell script we can edit.  The stock library must be kept beside it as
 * libcsdIpcClient_real.so; every call is forwarded to it unchanged.
 *
 * Signatures are the ones the QEMU harness proved by running the real driver against them
 * (tools/qsa-emu/csd_stub.c).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REAL_LIB "/mnt/app/armle/lib/libcsdIpcClient_real.so"
#define TRACE    "/tmp/csd_trace.log"

static void *g_lib;
static FILE *g_log;

static void trace_open(void)
{
    if (g_log) return;
    g_log = fopen(TRACE, "w");
    if (g_log) setvbuf(g_log, 0, _IONBF, 0);     /* QNX: unbuffered or the log dies with us */
}

static void tlog(const char *fmt, ...)
{
    va_list ap;
    trace_open();
    if (!g_log) return;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
}

static void *sym(const char *name)
{
    if (!g_lib) {
        g_lib = dlopen(REAL_LIB, RTLD_NOW | RTLD_GLOBAL);
        if (!g_lib) { tlog("FATAL dlopen %s: %s\n", REAL_LIB, dlerror()); return 0; }
        tlog("csd_trace: forwarding to %s\n", REAL_LIB);
    }
    return dlsym(g_lib, name);
}

#define FWD(name) static void *p; if (!p) p = sym(name); if (!p) return -1;

int32_t csd_init(void)
{
    FWD("csd_init")
    int32_t r = ((int32_t (*)(void))p)();
    tlog("csd_init -> %d\n", r);
    return r;
}

int32_t csd_deinit(void)
{
    FWD("csd_deinit")
    int32_t r = ((int32_t (*)(void))p)();
    tlog("csd_deinit -> %d\n", r);
    return r;
}

int32_t csd_query_driver_version(void)
{
    FWD("csd_query_driver_version")
    return ((int32_t (*)(void))p)();
}

int32_t csd_open(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    FWD("csd_open")
    int32_t r = ((int32_t (*)(uint32_t,uint32_t,uint32_t,uint32_t))p)(a1,a2,a3,a4);
    tlog("csd_open(%#x, %#x, %#x, %#x) -> %d\n", a1, a2, a3, a4, r);
    return r;
}

int32_t csd_close(int32_t h)
{
    FWD("csd_close")
    int32_t r = ((int32_t (*)(int32_t))p)(h);
    tlog("csd_close(%d) -> %d\n", h, r);
    return r;
}

/* The interesting one: this carries the stream/device configuration. */
int32_t csd_ioctl(int32_t h, uint32_t cmd, void *arg, uint32_t len)
{
    FWD("csd_ioctl")
    int32_t r = ((int32_t (*)(int32_t,uint32_t,void*,uint32_t))p)(h, cmd, arg, len);
    tlog("csd_ioctl(h=%d cmd=%#x len=%u) -> %d", h, cmd, len, r);
    if (arg && len) {
        const uint32_t *w = (const uint32_t *)arg;
        uint32_t n = len / 4, i;
        if (n > 16) n = 16;
        tlog("  words:");
        for (i = 0; i < n; i++) tlog(" %08x", w[i]);
    }
    tlog("\n");
    return r;
}

/* The decisive one: how many bytes actually leave deva per tick. */
int32_t csd_write(int32_t h, const void *buf, uint32_t len)
{
    static unsigned long long calls, bytes;
    static uint32_t lastLen;
    FWD("csd_write")
    int32_t r = ((int32_t (*)(int32_t,const void*,uint32_t))p)(h, buf, len);
    calls++; bytes += len;
    if (calls <= 8 || len != lastLen || (calls % 512) == 0)
        tlog("csd_write(h=%d len=%u) -> %d   [call %llu, %llu bytes total]\n",
             h, len, r, calls, bytes);
    lastLen = len;
    return r;
}

int32_t csd_read(int32_t h, void *buf, uint32_t len)
{
    FWD("csd_read")
    return ((int32_t (*)(int32_t,void*,uint32_t))p)(h, buf, len);
}

int32_t csd_register_callback(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    FWD("csd_register_callback")
    int32_t r = ((int32_t (*)(uint32_t,uint32_t,uint32_t,uint32_t))p)(a1,a2,a3,a4);
    tlog("csd_register_callback(%#x, %#x, %#x, %#x) -> %d\n", a1, a2, a3, a4, r);
    return r;
}

int32_t csd_voice_register_callback(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    FWD("csd_voice_register_callback")
    return ((int32_t (*)(uint32_t,uint32_t,uint32_t,uint32_t))p)(a1,a2,a3,a4);
}
