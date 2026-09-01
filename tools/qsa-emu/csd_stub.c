/* libcsdIpcClient.so stand-in for running deva-ctrl-qc under QEMU.
 *
 * deva imports exactly eight symbols from the real Qualcomm CSD IPC client, and
 * every higher-level call it makes (openAudioStreamV2, setStreamMultichannel,
 * setStreamFormatPcmMultichannel, allocateCsdBufferPool ...) is its own wrapper
 * on top of them.  Standing in for those eight is therefore enough to run the
 * whole driver on a machine with no ADSP.
 *
 * The point is not to emulate the DSP.  It is to be the capture probe: whatever
 * deva hands down here is exactly what would have reached the hardware, so
 * dumping it answers the question no on-car log can — whether the audio is
 * already wrong by the time it leaves the driver.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The firmware is built with -fstack-protector and resolves these from its own
 * libc; the QEMU BSP's libc.so.3 does not export them, so deva-ctrl-qc.so fails
 * to dlopen with "unknown symbol: __stack_chk_guard".  Providing them here costs
 * nothing: this object is already in deva's DT_NEEDED, so the loader finds them.
 * The canary value only has to be consistent within the process. */
unsigned long __stack_chk_guard = 0xff0a0000UL;

void __stack_chk_fail(void)
{
    fprintf(stderr, "csd_stub: __stack_chk_fail (guest stack smashing detected)\n");
    abort();
}

/* deva also binds a handful of Harman platform symbols lazily, which the real
 * image resolves from libraries this probe has no reason to carry.  Each one is
 * a stub: the driver only needs them to return, not to do anything, because the
 * hardware they front is exactly what is being left out. */
int rtpd_init(void)   { return 0; }
int rtpd_deinit(void) { return 0; }

static FILE *g_trace;
static FILE *g_pcm;
static int   g_next_handle = 0x100;

static void csd_trace_open(void)
{
    if (g_trace) return;
    g_trace = fopen("/tmp/csd_trace.log", "w");
    if (!g_trace) g_trace = stderr;
    setvbuf(g_trace, 0, _IONBF, 0);          /* QNX /tmp logs need this */
    g_pcm = fopen("/tmp/csd_dma.pcm", "wb");
}

/* Called before main() in the guest so the very first driver call is traced. */
__attribute__((constructor)) static void csd_stub_init_once(void) { csd_trace_open(); }

int32_t csd_init(void)
{
    csd_trace_open();
    fprintf(g_trace, "csd_init\n");
    return 0;
}

int32_t csd_deinit(void)
{
    fprintf(g_trace, "csd_deinit\n");
    return 0;
}

/* Recovered from csd_log_driver_version @0x28ba8: no arguments, and the version
 * IS the return value -- zero is the failure path the driver logs as FAILED. */
int32_t csd_query_driver_version(void)
{
    fprintf(g_trace, "csd_query_driver_version -> 0x10000\n");
    return 0x00010000;
}

/* Never dereference an argument whose type has not been recovered from a call
 * site: printing csd_open's first parameter as a string crashed the driver,
 * because it is not one.  Scalars only. */
int32_t csd_open(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    int32_t h = g_next_handle++;
    fprintf(g_trace, "csd_open(%#x %#x %#x %#x) -> %d\n",
            (unsigned)a1, (unsigned)a2, (unsigned)a3, (unsigned)a4, h);
    return h;
}

int32_t csd_close(int32_t handle)
{
    fprintf(g_trace, "csd_close(%d)\n", handle);
    return 0;
}

/* The interesting one: every stream/context/format call deva makes lands here as
 * a command plus a payload.  Log the command and the first bytes of the payload;
 * that is where a wrong voice count or channel map would be visible. */
int32_t csd_ioctl(int32_t handle, uint32_t cmd, void *arg, uint32_t len)
{
    /* Log the shape, not the contents: `len` is only trustworthy once the real
     * prototype is known, and a wrong length here reads unmapped memory. */
    fprintf(g_trace, "csd_ioctl(h=%d cmd=%#x arg=%p len=%u)\n",
            handle, (unsigned)cmd, arg, (unsigned)len);
    return 0;
}

int32_t csd_write(int32_t handle, const void *buf, uint32_t len)
{
    fprintf(g_trace, "csd_write(h=%d buf=%p len=%u)\n", handle, buf, (unsigned)len);
    return (int32_t)len;
}

int32_t csd_read(int32_t handle, void *buf, uint32_t len)
{
    fprintf(g_trace, "csd_read(h=%d buf=%p len=%u)\n", handle, buf, (unsigned)len);
    return (int32_t)len;
}
