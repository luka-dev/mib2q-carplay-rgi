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
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

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

#define CSD_AS_CMD_ALLOC_PMEM         70688u   /* 0x11420 */
#define CSD_AS_CMD_MAP_SHARED_MEMORY  70679u   /* 0x11417 */

/* Every PMEM pool the driver asks for, kept mapped so its contents can be dumped:
 * this is the memory the driver fills with audio destined for the DSP. */
static struct { int fd; uint32_t size; void *base; } g_pool[32];
static int g_pool_count;

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
/* Dump the first frames of every pool that has stopped being silent.  This is
 * the whole reason the stub owns the memory: the driver fills these for the DSP,
 * so whatever is here is what the hardware would have played.  One shot -- the
 * question is the layout of the samples, not their evolution. */
static void csd_dump_pools_once(void)
{
    static int done;
    int i, j;
    if (done) return;
    for (i = 0; i < g_pool_count && i < 32; i++) {
        const unsigned char *b = (const unsigned char *)g_pool[i].base;
        uint32_t n = g_pool[i].size < 64 ? g_pool[i].size : 64;
        int nonzero = 0;
        if (!b) continue;
        for (j = 0; j < (int)n; j++) if (b[j]) { nonzero = 1; break; }
        if (!nonzero) continue;
        done = 1;
        fprintf(g_trace, "POOL %d (size %u) first %u bytes:\n  ", i,
                (unsigned)g_pool[i].size, (unsigned)n);
        for (j = 0; j < (int)n; j++) {
            fprintf(g_trace, "%02x ", b[j]);
            if ((j & 15) == 15) fprintf(g_trace, "\n  ");
        }
        fprintf(g_trace, "\n");
    }
}

int32_t csd_ioctl(int32_t handle, uint32_t cmd, void *arg, uint32_t len)
{
    csd_dump_pools_once();
    fprintf(g_trace, "csd_ioctl(h=%d cmd=%#x arg=%p len=%u)\n",
            handle, (unsigned)cmd, arg, (unsigned)len);

    /* CSD_AS_CMD_ALLOC_PMEM, recovered from allocateCsdBufferPool @0x2e374:
     * the driver passes {size, pid, 0, 0} and expects the DSP side to write a
     * shared-memory FILE DESCRIPTOR into word 3.  It then calls mem_offset64 on
     * that fd to get a physical offset, so an ordinary shm_open is not enough --
     * the object has to be anonymous physical memory.  Answering "success"
     * without filling word 3 is what made the driver memset a null pointer.
     *
     * These buffers are also the prize: this is the memory the driver fills with
     * audio for the DSP, so owning it is what lets the bytes be inspected. */
    if (cmd == CSD_AS_CMD_ALLOC_PMEM && arg && len >= 16) {
        uint32_t *w = (uint32_t *)arg;
        uint32_t size = w[0];
        char name[40];
        int fd;
        void *base;

        snprintf(name, sizeof(name), "/csd_pool_%d", g_pool_count);
        shm_unlink(name);
        fd = shm_open(name, O_RDWR | O_CREAT, 0666);
        if (fd < 0 || shm_ctl(fd, SHMCTL_ANON | SHMCTL_PHYS, 0, size) == -1) {
            fprintf(g_trace, "  ALLOC_PMEM size=%u FAILED (%s)\n",
                    (unsigned)size, strerror(errno));
            if (fd >= 0) close(fd);
            return -1;
        }
        base = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (base == MAP_FAILED) {
            fprintf(g_trace, "  ALLOC_PMEM size=%u mmap FAILED (%s)\n",
                    (unsigned)size, strerror(errno));
            close(fd);
            return -1;
        }
        /* Word 3 is an ADDRESS, not a descriptor.  The caller passes it straight
         * to mem_offset64(addr, -1, ...) -- the -1 is "no fd", which only makes
         * sense if the first argument is a mapped pointer -- and then stores it
         * for later use as a buffer.  Handing back the fd instead made the driver
         * memset at address 62, which is exactly the fd we had returned. */
        w[3] = (uint32_t)(uintptr_t)base;
        if (g_pool_count < (int)(sizeof(g_pool) / sizeof(g_pool[0]))) {
            g_pool[g_pool_count].fd = fd;
            g_pool[g_pool_count].size = size;
            g_pool[g_pool_count].base = base;
        }
        fprintf(g_trace, "  ALLOC_PMEM size=%u -> addr=%p fd=%d (pool %d)\n",
                (unsigned)size, base, fd, g_pool_count);
        g_pool_count++;
        return 0;
    }
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
