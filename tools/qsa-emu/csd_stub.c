/* libcsdIpcClient.so stand-in for running deva-ctrl-qc under QEMU.
 *
 * deva imports exactly eight symbols from the real Qualcomm CSD IPC client, and
 * every higher-level call it makes (openAudioStreamV2, setStreamMultichannel,
 * setStreamFormatPcmMultichannel, allocateCsdBufferPool ...) is its own wrapper
 * on top of them.  Standing in for those eight is therefore enough to run the
 * whole driver on a machine with no ADSP.
 *
 * Preloading the stand-in into io-audio also lets it locate deva's DMA ring and
 * act as the absent DSP: it captures accepted resource-manager writes, renders
 * the submitted words under both plausible sample-width interpretations, and
 * exports the two S16 streams over the serial console.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <pthread.h>

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
#define CSD_AS_CMD_REGISTER_CALLBACK  70657u   /* 0x11401 */
#define CSD_AS_CMD_STOP               70660u   /* 0x11404 */
#define CSD_EVENT_BUF_DONE            71169u   /* 0x11601 */

/* Every PMEM pool the driver asks for, kept mapped so its contents can be
 * inspected.  These stay empty in the emulated playback path; client PCM enters
 * through deva's separate malloc-backed DMA ring. */
static struct { int fd; uint32_t size; void *base; } g_pool[32];

/* The audio reference qsa_probe played, loaded once for word-by-word comparison. */
static int16_t *g_music;
static size_t   g_music_samples;
static int      g_music_tried;
static int g_pool_count;

static FILE *g_trace;
static FILE *g_pcm;
static int   g_next_handle = 0x100;
static uint32_t g_csd_sequence;
static uint32_t g_csd_write_sequence;
static void install_timer_mutex_hook(void);

static int submission_measurement_enabled(void)
{
    static int checked;
    static int enabled;
    if (!checked) {
        const char *value = getenv("QSA_SUBMIT_MEASURE");
        enabled = value && value[0] && strcmp(value, "0") != 0;
        checked = 1;
    }
    return enabled;
}

static void csd_trace_open(void)
{
    if (g_trace) return;
    /* Straight to the console: the guest can fault on the way out, and a trace
     * that only exists in /tmp is lost exactly when it is most wanted. */
    g_trace = stderr;
    setvbuf(g_trace, 0, _IONBF, 0);          /* QNX /tmp logs need this */
    g_pcm = fopen("/tmp/csd_dma.pcm", "wb");
}

/* Called before main() in the guest so the very first driver call is traced. */
__attribute__((constructor)) static void csd_stub_init_once(void)
{
    csd_trace_open();
}

static void csd_hex_dump(const char *kind, uint32_t sequence,
                         const void *arg, uint32_t len)
{
    const unsigned char *p = (const unsigned char *)arg;
    uint32_t off;

    if (!arg || !len) {
        fprintf(g_trace, "CSD_%s_HEX seq=%u <empty>\n",
                kind, (unsigned)sequence);
        return;
    }
    for (off = 0; off < len; off += 16) {
        uint32_t j, count = len - off < 16 ? len - off : 16;
        fprintf(g_trace, "CSD_%s_HEX seq=%u +%04x:",
                kind, (unsigned)sequence, (unsigned)off);
        for (j = 0; j < count; j++)
            fprintf(g_trace, " %02x", p[off + j]);
        fputc('\n', g_trace);
    }
}

static int capture_write_all(int fd, const void *buf, uint32_t len)
{
    const unsigned char *p = (const unsigned char *)buf;
    uint32_t left = len;

    while (left) {
        ssize_t n = write(fd, p, left);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        left -= (uint32_t)n;
    }
    return 0;
}


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
    uint32_t sequence = ++g_csd_sequence;
    fprintf(g_trace,
            "CSD_OPEN_BEGIN seq=%u a1=%#x a2=%#x a3=%#x a4=%#x handle=%d\n",
            (unsigned)sequence, (unsigned)a1, (unsigned)a2,
            (unsigned)a3, (unsigned)a4, h);
    /* openAudioStreamV2 @0x2ecf4 passes its initialized descriptor as a2 and
     * its exact byte length as a3. Other recovered call sites use the same
     * pointer/length pair or pass both as zero. Keep a sanity ceiling so a bad
     * call cannot turn tracing into an unbounded read. */
    if (a2 && a3 && a3 <= 4096)
        csd_hex_dump("OPEN", sequence, (const void *)(uintptr_t)a2, a3);
    else
        csd_hex_dump("OPEN", sequence, 0, 0);
    fprintf(g_trace, "CSD_OPEN_END seq=%u handle=%d\n",
            (unsigned)sequence, h);
    return h;
}

int32_t csd_close(int32_t handle)
{
    fprintf(g_trace, "csd_close(%d)\n", handle);
    return 0;
}

/* The interesting one: every stream/context/format call deva makes lands here as
 * a command plus a payload. Log each full typed payload before handling it;
 * that is where a wrong voice count, rate, width or channel map is visible. */
/* Dump the first frames of every pool that has stopped being silent.  This is
 * the whole reason the stub owns the memory: the driver fills these for the DSP,
 * so whatever is here is what the hardware would have played.  One shot -- the
 * question is the layout of the samples, not their evolution. */
#define DSP_CAPTURE_FRAGMENTS 62u

/* deva_mib2_aquire allocates its DMA ring with a plain malloc(10 * frag_size) --
 * 184320 bytes at the six-voice fragment -- and the timer walks that, not the
 * CSD pools, which is why the pools stay empty.  This stub already lives in
 * io-audio's process, so interposing malloc is enough to find that ring and read
 * it: whatever the driver leaves there IS the audio, whether or not it ever
 * reaches a DSP.  Pass-through; the only addition is remembering the pointer. */
static struct {
    void *base;
    size_t size;
    size_t frag_size;
    unsigned voices;
    unsigned first_slot;
    unsigned calls;
    uint64_t bytes;
    uint64_t mismatches;
    int16_t *render_a;
    int16_t *render_b;
    size_t render_samples;
    uint64_t b_saturated;
    unsigned b_peak;
    int emit_pending;
    int emitted;
    int active;
    unsigned slot_generation[10];
    unsigned generation_submits[DSP_CAPTURE_FRAGMENTS];
    unsigned timer_advances;
    unsigned timer_delta_4800;
    unsigned timer_delta_2400;
    unsigned timer_delta_other;
    size_t timer_last_cursor;
    int timer_have_cursor;
} g_dma[8];
static int g_dma_count;
static void *(*g_real_malloc)(size_t);

typedef void (*csd_callback_fn)(uint32_t event, const void *descriptor,
                                uint32_t unused, void *context);

#define CSD_CALLBACK_QUEUE 16u

static struct {
    csd_callback_fn callback;
    void *context;
    uint32_t pending_descriptor[CSD_CALLBACK_QUEUE][2];
    unsigned pending_head;
    unsigned pending_count;
} g_handle[64];

static struct {
    int valid;
    int ring;
    const void *ring_source;
    void *pool_destination;
    size_t bytes;
    size_t ring_offset;
    unsigned slot;
    unsigned generation;
    uint64_t sequence;
} g_pending_pull;

static struct {
    int16_t *samples;
    size_t capacity_samples;
    size_t sample_count;
    int ring;
    uint64_t bytes;
    uint64_t mismatches;
    unsigned calls;
    unsigned overflow_calls;
    unsigned unknown_generations;
    unsigned delta_4800;
    unsigned delta_2400;
    unsigned delta_other;
    size_t last_offset;
    int have_offset;
    unsigned preroll_calls;
    uint64_t preroll_bytes;
    int emitted;
} g_submit;

static struct {
    int valid;
    int ring;
    size_t offset;
    size_t bytes;
    pthread_t owner;
} g_timer_pending;

static uint64_t g_pull_sequence;

static void *stub_malloc(size_t size)
{
    if (!g_real_malloc) g_real_malloc = dlsym(RTLD_NEXT, "malloc");
    return g_real_malloc(size);
}

void *malloc(size_t size)
{
    int i;
    void *p;
    p = stub_malloc(size);
    /* Ten fragments of six-voice S32, or of any plausible fragment: record the
     * shape rather than one hard-coded number. */
    if (p && size >= 60000 && size <= 400000 && (size % 10) == 0 &&
        g_dma_count < (int)(sizeof(g_dma) / sizeof(g_dma[0]))) {
        /* The later stereo open commonly reuses the six-channel allocation's
         * address. Retire the old record so reused memory cannot masquerade as
         * additional six-channel fragments. */
        for (i = 0; i < g_dma_count; i++)
            if (g_dma[i].base == p) g_dma[i].active = 0;
        g_dma[g_dma_count].base = p;
        g_dma[g_dma_count].size = size;
        g_dma[g_dma_count].active = 1;
        if (g_trace)
            fprintf(g_trace, "DMA candidate %d: %u bytes at %p\n",
                    g_dma_count, (unsigned)size, p);
        g_dma_count++;
    }
    return p;
}

/* Kept as a diagnostic for the original copy-path hypothesis.  It does not fire
 * for client writes: io-audio uses resmgr_msgread below, so the kernel copies
 * directly into the ring without going through libc memcpy. */
static int spa_in_dma(const void *d, size_t n)
{
    int i;
    /* malloc commonly reuses the same address for the later stereo run.  Search
     * newest-first so it gets its own capture instead of matching the stale,
     * larger six-channel allocation recorded earlier. */
    for (i = g_dma_count - 1; i >= 0; i--) {
        const unsigned char *b = (const unsigned char *)g_dma[i].base;
        const unsigned char *p = (const unsigned char *)d;
        if (!g_dma[i].active) continue;
        if (p >= b && p + n <= b + g_dma[i].size) return i + 1;
    }
    return 0;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    static void *(*real)(void *, const void *, size_t);
    static int shown;
    int which, from;
    void *result;
    if (!real) real = dlsym(RTLD_NEXT, "memcpy");
    which = (g_dma_count && n >= 64) ? spa_in_dma(dst, n) : 0;
    from = (g_dma_count && n >= 64) ? spa_in_dma(src, n) : 0;
    result = real(dst, src, n);
    if (which && shown < 4) {
        const unsigned char *b = (const unsigned char *)src;
        size_t j;
        shown++;
        fprintf(g_trace, "RING%d <- %u bytes:", which - 1, (unsigned)n);
        for (j = 0; j < 32 && j < n; j++) {
            if ((j & 3) == 0) fprintf(g_trace, " ");
            fprintf(g_trace, "%02x", b[j]);
        }
        fprintf(g_trace, "\n");
    }
    /* session_csd_stream_playback @0x6924c..0x6926c copies exactly one
     * fragment from dma_base+cursor into a reserved CSD pool buffer.  This is
     * the typed call site that associates the otherwise-physical CSD descriptor
     * with a concrete ring address.  csd_write follows immediately at 0x69318. */
    if (from && submission_measurement_enabled()) {
        int index = from - 1;
        size_t off = (size_t)((const unsigned char *)src -
                              (const unsigned char *)g_dma[index].base);
        size_t frag = g_dma[index].frag_size ? g_dma[index].frag_size : n;
        unsigned slot = frag ? (unsigned)(off / frag) : 0;
        unsigned generation = slot < 10 ?
            g_dma[index].slot_generation[slot] : 0;

        g_pending_pull.valid = 1;
        g_pending_pull.ring = index;
        g_pending_pull.ring_source = src;
        g_pending_pull.pool_destination = dst;
        g_pending_pull.bytes = n;
        g_pending_pull.ring_offset = off;
        g_pending_pull.slot = slot;
        g_pending_pull.generation = generation;
        g_pending_pull.sequence = ++g_pull_sequence;
        fprintf(g_trace,
                "RING_PULL seq=%llu ring=%d source=%p offset=%#x "
                "slot=%u generation=%u destination=%p bytes=%u\n",
                (unsigned long long)g_pending_pull.sequence, index, src,
                (unsigned)off, slot, generation, dst, (unsigned)n);
    }
    return result;
}

static void music_load_once(void)
{
    FILE *mf;

    if (g_music_tried) return;
    g_music_tried = 1;
    mf = fopen("/proc/boot/music6.s16", "rb");
    if (!mf) {
        fprintf(g_trace, "DSP_CAPTURE reference unavailable: %s\n",
                strerror(errno));
        return;
    }
    g_music = (int16_t *)stub_malloc(576000);
    g_music_samples = g_music ? fread(g_music, 2, 288000, mf) : 0;
    fclose(mf);
    fprintf(g_trace, "DSP_CAPTURE reference: %u music samples\n",
            (unsigned)g_music_samples);
}

static void serial_write_text(const char *s)
{
    capture_write_all(STDERR_FILENO, s, (uint32_t)strlen(s));
}

static void serial_write_base64(const unsigned char *data, size_t len)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char line[88];
    size_t pos = 0;

    /* 57 input bytes become one conventional 76-character base64 line. */
    while (pos < len) {
        size_t chunk = len - pos < 57 ? len - pos : 57;
        size_t in = 0, out = 6;
        memcpy(line, "DSP64 ", 6);
        while (in < chunk) {
            size_t remain = chunk - in;
            uint32_t v = (uint32_t)data[pos + in] << 16;
            if (remain > 1) v |= (uint32_t)data[pos + in + 1] << 8;
            if (remain > 2) v |= data[pos + in + 2];
            line[out++] = alphabet[(v >> 18) & 63];
            line[out++] = alphabet[(v >> 12) & 63];
            line[out++] = remain > 1 ? alphabet[(v >> 6) & 63] : '=';
            line[out++] = remain > 2 ? alphabet[v & 63] : '=';
            in += remain > 2 ? 3 : remain;
        }
        line[out++] = '\n';
        capture_write_all(STDERR_FILENO, line, (uint32_t)out);
        pos += chunk;
    }
}

static void dma_emit_renders(int index)
{
    char marker[256];
    struct {
        const char *name;
        const int16_t *samples;
    } stream[2];
    int i;
    size_t bytes;
    uint64_t saturation_ppm;

    if (index < 0 || index >= g_dma_count || g_dma[index].emitted ||
        !g_dma[index].render_a || !g_dma[index].render_b) return;
    g_dma[index].emitted = 1;
    g_dma[index].emit_pending = 0;
    bytes = g_dma[index].render_samples * sizeof(int16_t);
    saturation_ppm = g_dma[index].render_samples ?
        (g_dma[index].b_saturated * 1000000ULL) /
        g_dma[index].render_samples : 0;

    snprintf(marker, sizeof(marker),
             "DSP_RENDER_SUMMARY fragments=%u frames=%u samples=%u "
             "bytes_per_stream=%u mismatches=%llu\n",
             g_dma[index].calls, g_dma[index].calls * 768,
             (unsigned)g_dma[index].render_samples, (unsigned)bytes,
             (unsigned long long)g_dma[index].mismatches);
    serial_write_text(marker);
    snprintf(marker, sizeof(marker),
             "DSP_RENDER_B_STATS peak=%u saturated=%llu total=%u ppm=%llu\n",
             g_dma[index].b_peak,
             (unsigned long long)g_dma[index].b_saturated,
             (unsigned)g_dma[index].render_samples,
             (unsigned long long)saturation_ppm);
    serial_write_text(marker);

    stream[0].name = "A_HIGH16_CORRECT";
    stream[0].samples = g_dma[index].render_a;
    stream[1].name = "B_LOW24_DSP";
    stream[1].samples = g_dma[index].render_b;
    for (i = 0; i < 2; i++) {
        snprintf(marker, sizeof(marker),
                 "=== DSP_RENDER_%s_S16LE_BASE64_BEGIN "
                 "channels=6 rate=48000 frames=%u bytes=%u ===\n",
                 stream[i].name, g_dma[index].calls * 768,
                 (unsigned)bytes);
        serial_write_text(marker);
        serial_write_base64((const unsigned char *)stream[i].samples, bytes);
        snprintf(marker, sizeof(marker),
                 "=== DSP_RENDER_%s_S16LE_BASE64_END ===\n",
                 stream[i].name);
        serial_write_text(marker);
    }
    serial_write_text("=== DSP_RENDER_COMPLETE ===\n");
}

#define DSP_SUBMIT_MAX_FRAGMENTS 80u

static void submission_emit(void);

static void submission_capture(int32_t handle, uint32_t descriptor_address,
                               uint32_t payload_len)
{
    const uint32_t *words;
    size_t word_count, j, needed;
    int index;
    unsigned generation;
    size_t delta = 0;

    if (!g_pending_pull.valid) {
        g_submit.preroll_calls++;
        g_submit.preroll_bytes += payload_len;
        fprintf(g_trace,
                "CSD_SUBMIT_PREROLL call=%u handle=%d descriptor_address=%#x "
                "payload_len=%u reason=no_ring_pull\n",
                g_submit.preroll_calls, handle, (unsigned)descriptor_address,
                (unsigned)payload_len);
        return;
    }

    index = g_pending_pull.ring;
    generation = g_pending_pull.generation;
    if (index < 0 || index >= g_dma_count ||
        g_dma[index].voices != 6 ||
        payload_len != g_pending_pull.bytes || (payload_len & 3)) {
        fprintf(g_trace,
                "CSD_SUBMIT_UNMAPPED call=%u handle=%d ring=%d "
                "descriptor_address=%#x payload_len=%u pulled_bytes=%u "
                "voices=%u reason=geometry\n",
                g_submit.calls + 1, handle, index,
                (unsigned)descriptor_address, (unsigned)payload_len,
                (unsigned)g_pending_pull.bytes,
                index >= 0 && index < g_dma_count ?
                    g_dma[index].voices : 0);
        g_submit.unknown_generations++;
        g_pending_pull.valid = 0;
        return;
    }

    word_count = payload_len / sizeof(uint32_t);
    if (!g_submit.samples) {
        g_submit.capacity_samples = DSP_SUBMIT_MAX_FRAGMENTS * word_count;
        g_submit.samples = (int16_t *)stub_malloc(
            g_submit.capacity_samples * sizeof(int16_t));
        g_submit.ring = index;
        if (!g_submit.samples) {
            fprintf(g_trace, "CSD_SUBMIT render allocation failed\n");
            g_pending_pull.valid = 0;
            return;
        }
    }

    if (g_submit.have_offset) {
        delta = g_pending_pull.ring_offset >= g_submit.last_offset ?
            g_pending_pull.ring_offset - g_submit.last_offset :
            g_dma[index].size - g_submit.last_offset +
                g_pending_pull.ring_offset;
        if (delta == 0x4800)
            g_submit.delta_4800++;
        else if (delta == 0x2400)
            g_submit.delta_2400++;
        else
            g_submit.delta_other++;
    }

    fprintf(g_trace,
            "CSD_WRITE_SUBMIT call=%u handle=%d descriptor_address=%#x "
            "payload_len=%u ring=%d ring_source=%p ring_offset=%#x "
            "slot=%u generation=%u cursor_delta=%s%#x\n",
            g_submit.calls + 1, handle, (unsigned)descriptor_address,
            (unsigned)payload_len, index, g_pending_pull.ring_source,
            (unsigned)g_pending_pull.ring_offset, g_pending_pull.slot,
            generation, g_submit.have_offset ? "" : "NA/",
            (unsigned)delta);

    if (generation && generation <= DSP_CAPTURE_FRAGMENTS)
        g_dma[index].generation_submits[generation - 1]++;
    else
        g_submit.unknown_generations++;

    needed = g_submit.sample_count + word_count;
    if (needed <= g_submit.capacity_samples) {
        words = (const uint32_t *)g_pending_pull.pool_destination;
        for (j = 0; j < word_count; j++) {
            int16_t sample = (int16_t)(words[j] >> 16);
            size_t reference_index = g_submit.sample_count + j;
            g_submit.samples[reference_index] = sample;
            if (g_music && reference_index < g_music_samples &&
                sample != g_music[reference_index])
                g_submit.mismatches++;
        }
        g_submit.sample_count = needed;
    } else {
        g_submit.overflow_calls++;
        fprintf(g_trace,
                "CSD_SUBMIT overflow call=%u samples=%u capacity=%u\n",
                g_submit.calls + 1, (unsigned)needed,
                (unsigned)g_submit.capacity_samples);
    }

    g_submit.calls++;
    g_submit.bytes += payload_len;
    g_submit.last_offset = g_pending_pull.ring_offset;
    g_submit.have_offset = 1;
    g_timer_pending.valid = 1;
    g_timer_pending.ring = index;
    g_timer_pending.offset = g_pending_pull.ring_offset;
    g_timer_pending.bytes = payload_len;
    g_timer_pending.owner = pthread_self();
    g_pending_pull.valid = 0;

    {
        char count[32];
        int fd = open("/tmp/csd_submit_count",
                      O_WRONLY | O_CREAT | O_TRUNC, 0644);
        int count_len = snprintf(count, sizeof(count), "%u\n", g_submit.calls);
        if (fd >= 0) {
            capture_write_all(fd, count, (uint32_t)count_len);
            close(fd);
        }
    }
}

static void submission_emit(void)
{
    char marker[512];
    int index = g_submit.ring;
    unsigned i, distinct = 0, once = 0, repeated = 0, missing = 0;
    unsigned duplicate_submissions = 0;
    size_t bytes;

    if (g_submit.emitted || !g_submit.calls || !g_submit.samples ||
        index < 0 || index >= g_dma_count) return;
    g_submit.emitted = 1;

    for (i = 0; i < g_dma[index].calls &&
                i < DSP_CAPTURE_FRAGMENTS; i++) {
        unsigned count = g_dma[index].generation_submits[i];
        if (!count) {
            missing++;
        } else {
            distinct++;
            if (count == 1)
                once++;
            else {
                repeated++;
                duplicate_submissions += count - 1;
            }
        }
    }
    bytes = g_submit.sample_count * sizeof(int16_t);

    snprintf(marker, sizeof(marker),
             "TIMER_CURSOR_SUMMARY ring=%d advances=%u "
             "delta_0x4800=%u delta_0x2400=%u delta_other=%u\n",
             index, g_dma[index].timer_advances,
             g_dma[index].timer_delta_4800,
             g_dma[index].timer_delta_2400,
             g_dma[index].timer_delta_other);
    serial_write_text(marker);
    snprintf(marker, sizeof(marker),
             "CSD_CURSOR_SUMMARY submissions=%u delta_0x4800=%u "
             "delta_0x2400=%u delta_other=%u\n",
             g_submit.calls, g_submit.delta_4800,
             g_submit.delta_2400, g_submit.delta_other);
    serial_write_text(marker);
    snprintf(marker, sizeof(marker),
             "CSD_REGION_SUMMARY written_generations=%u distinct_submitted=%u "
             "submitted_once=%u repeated_generations=%u "
             "duplicate_submissions=%u missing_generations=%u unknown=%u\n",
             g_dma[index].calls, distinct, once, repeated,
             duplicate_submissions, missing, g_submit.unknown_generations);
    serial_write_text(marker);
    snprintf(marker, sizeof(marker),
             "CSD_BYTE_SUMMARY submitted=%llu written=%llu difference=%lld\n",
             (unsigned long long)g_submit.bytes,
             (unsigned long long)g_dma[index].bytes,
             (long long)g_submit.bytes - (long long)g_dma[index].bytes);
    serial_write_text(marker);
    snprintf(marker, sizeof(marker),
             "CSD_TOTAL_WRITE_SUMMARY csd_write_calls=%u ring_calls=%u "
             "preroll_calls=%u ring_bytes=%llu preroll_bytes=%llu "
             "total_payload_bytes=%llu\n",
             g_submit.calls + g_submit.preroll_calls, g_submit.calls,
             g_submit.preroll_calls,
             (unsigned long long)g_submit.bytes,
             (unsigned long long)g_submit.preroll_bytes,
             (unsigned long long)(g_submit.bytes + g_submit.preroll_bytes));
    serial_write_text(marker);
    snprintf(marker, sizeof(marker),
             "CSD_SUBMIT_RENDER_SUMMARY fragments=%u frames=%u samples=%u "
             "pcm_bytes=%u reference_mismatches=%llu overflow_calls=%u\n",
             g_submit.calls, (unsigned)(g_submit.sample_count / 6),
             (unsigned)g_submit.sample_count, (unsigned)bytes,
             (unsigned long long)g_submit.mismatches,
             g_submit.overflow_calls);
    serial_write_text(marker);
    snprintf(marker, sizeof(marker),
             "=== DSP_SUBMIT_HIGH16_S16LE_BASE64_BEGIN "
             "channels=6 rate=48000 frames=%u bytes=%u ===\n",
             (unsigned)(g_submit.sample_count / 6), (unsigned)bytes);
    serial_write_text(marker);
    serial_write_base64((const unsigned char *)g_submit.samples, bytes);
    serial_write_text("=== DSP_SUBMIT_HIGH16_S16LE_BASE64_END ===\n");
    serial_write_text("=== DSP_SUBMIT_COMPLETE ===\n");
    {
        int fd = open("/tmp/csd_submit_emitted",
                      O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            capture_write_all(fd, "complete\n", 9);
            close(fd);
        }
    }
}

static void callback_enqueue(int32_t handle, const uint32_t *descriptor)
{
    int index = handle - 0x100;
    unsigned tail;

    if (index < 0 || index >= (int)(sizeof(g_handle) / sizeof(g_handle[0])) ||
        !g_handle[index].callback || !descriptor)
        return;
    if (g_handle[index].pending_count >= CSD_CALLBACK_QUEUE) {
        fprintf(g_trace,
                "CSD_CALLBACK_QUEUE_FULL handle=%d pending=%u\n",
                handle, g_handle[index].pending_count);
        return;
    }
    tail = (g_handle[index].pending_head + g_handle[index].pending_count) %
           CSD_CALLBACK_QUEUE;
    g_handle[index].pending_descriptor[tail][0] = descriptor[0];
    g_handle[index].pending_descriptor[tail][1] = descriptor[1];
    g_handle[index].pending_count++;
}

static void callback_drain_buf_done(void)
{
    unsigned i;

    for (i = 0; i < sizeof(g_handle) / sizeof(g_handle[0]); i++) {
        while (g_handle[i].callback && g_handle[i].pending_count) {
            uint32_t descriptor[2];
            unsigned head = g_handle[i].pending_head;
            descriptor[0] = g_handle[i].pending_descriptor[head][0];
            descriptor[1] = g_handle[i].pending_descriptor[head][1];
            g_handle[i].pending_head = (head + 1) % CSD_CALLBACK_QUEUE;
            g_handle[i].pending_count--;
            fprintf(g_trace,
                    "CSD_CALLBACK_BUF_DONE boundary=dma_interrupt handle=%d "
                    "descriptor_address=%#x payload_len=%u pending_after=%u\n",
                    (int)i + 0x100, (unsigned)descriptor[0],
                    (unsigned)descriptor[1], g_handle[i].pending_count);
            g_handle[i].callback(CSD_EVENT_BUF_DONE, descriptor, 0,
                                 g_handle[i].context);
        }
    }
}

static void timer_boundary_measure(const char *boundary)
{
    int index;
    size_t off, after, delta = 0;

    if (!submission_measurement_enabled() || !g_timer_pending.valid)
        return;
    index = g_timer_pending.ring;
    off = g_timer_pending.offset;
    after = (off + g_timer_pending.bytes) % g_dma[index].size;
    if (g_dma[index].timer_have_cursor) {
        delta = off >= g_dma[index].timer_last_cursor ?
            off - g_dma[index].timer_last_cursor :
            g_dma[index].size - g_dma[index].timer_last_cursor + off;
        if (delta == 0x4800)
            g_dma[index].timer_delta_4800++;
        else if (delta == 0x2400)
            g_dma[index].timer_delta_2400++;
        else
            g_dma[index].timer_delta_other++;
    }
    g_dma[index].timer_advances++;
    fprintf(g_trace,
            "TIMER_ADVANCE ring=%d count=%u boundary=%s cursor_before=%#x "
            "cursor_after=%#x delta_from_previous=%s%#x frag=%#x\n",
            index, g_dma[index].timer_advances, boundary, (unsigned)off,
            (unsigned)after, g_dma[index].timer_have_cursor ? "" : "NA/",
            (unsigned)delta, (unsigned)g_timer_pending.bytes);
    g_dma[index].timer_last_cursor = off;
    g_dma[index].timer_have_cursor = 1;
    /* Clear before invoking deva's callback: that callback takes locks itself. */
    g_timer_pending.valid = 0;
    callback_drain_buf_done();
    /* Freeze the requested window after its 62nd real cursor advance. Waiting
     * for client teardown gives the live timer time to pull one stale ring slot
     * after the final write, which is outside the measurement. */
    if (g_submit.calls == DSP_CAPTURE_FRAGMENTS)
        submission_emit();
}

/* The main executable owns dma_interrupt, so ELF lookup correctly gives it
 * precedence over a preload symbol. Its first operation after entry is the
 * imported pthread_mutex_lock at io-audio 0x10f7a8. Patch only io-audio's
 * already-relocated GOT slot (0x124f78), retain its original target, and require
 * the same thread that just returned from csd_write. This is an exact, typed
 * post-cursor boundary without dereferencing the opaque subchannel. */
static int (*g_real_timer_mutex_lock)(pthread_mutex_t *);

static int timer_mutex_lock(pthread_mutex_t *mutex)
{
    if (g_timer_pending.valid &&
        pthread_equal(pthread_self(), g_timer_pending.owner))
        timer_boundary_measure("dma_interrupt+0x40");
    return g_real_timer_mutex_lock(mutex);
}

static void install_timer_mutex_hook(void)
{
    int (**slot)(pthread_mutex_t *) =
        (int (**)(pthread_mutex_t *))(uintptr_t)0x00124f78u;

    if (*slot == timer_mutex_lock) return;
    /* The QNX PLT slot remains a lazy binder trampoline here; calling that
     * trampoline outside its PLT loses the resolver's expected ip register.
     * Resolve libc directly now that io-audio is fully initialized. */
    g_real_timer_mutex_lock = dlsym(RTLD_NEXT, "pthread_mutex_lock");
    if (g_real_timer_mutex_lock) {
        *slot = timer_mutex_lock;
        fprintf(g_trace,
                "TIMER_MUTEX_HOOK slot=%p libc=%p replacement=%p\n",
                slot, g_real_timer_mutex_lock, timer_mutex_lock);
    } else {
        fprintf(g_trace,
                "TIMER_MUTEX_HOOK_FAILED slot=%p value=%p\n",
                slot, g_real_timer_mutex_lock);
    }
}

static void dma_append_raw(int index, const void *fragment, size_t size)
{
    char path[64];
    int fd;

    snprintf(path, sizeof(path), "/tmp/resmgr_dma_%d_%u.bin",
             index, (unsigned)g_dma[index].size);
    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        if (g_trace && g_dma[index].calls == 1)
            fprintf(g_trace, "DSP_CAPTURE open %s failed: %s\n",
                    path, strerror(errno));
        return;
    }
    if (capture_write_all(fd, fragment, (uint32_t)size) < 0 && g_trace)
        fprintf(g_trace, "DSP_CAPTURE write %s failed: %s\n",
                path, strerror(errno));
    close(fd);
}

static void dma_capture_fragment(int index, const void *fragment, size_t size,
                                 const char *source)
{
    const uint32_t *words = (const uint32_t *)fragment;
    size_t word_count = size / sizeof(uint32_t);
    uint64_t first_sample;
    unsigned j;

    if (index < 0 || index >= g_dma_count ||
        g_dma[index].calls >= DSP_CAPTURE_FRAGMENTS ||
        !word_count || (size & 3)) return;
    if (!g_dma[index].frag_size) {
        g_dma[index].frag_size = size;
        g_dma[index].voices = (size % (768 * 4)) == 0 ?
                              (unsigned)(size / (768 * 4)) : 0;
    }
    if (size != g_dma[index].frag_size) {
        fprintf(g_trace,
                "DSP_CAPTURE ring=%d unexpected fragment size=%u expected=%u\n",
                index, (unsigned)size, (unsigned)g_dma[index].frag_size);
        return;
    }

    if (g_dma[index].voices == 6) music_load_once();
    if (g_dma[index].voices == 6 && !g_dma[index].render_a) {
        g_dma[index].render_samples =
            DSP_CAPTURE_FRAGMENTS * word_count;
        g_dma[index].render_a = (int16_t *)stub_malloc(
            g_dma[index].render_samples * sizeof(int16_t));
        g_dma[index].render_b = (int16_t *)stub_malloc(
            g_dma[index].render_samples * sizeof(int16_t));
        if (!g_dma[index].render_a || !g_dma[index].render_b) {
            fprintf(g_trace, "DSP_CAPTURE render allocation failed\n");
            return;
        }
    }

    first_sample = (uint64_t)g_dma[index].calls * word_count;
    for (j = 0; j < word_count; j++) {
        uint64_t ref_index = first_sample + j;
        uint32_t expected;
        if (g_dma[index].voices == 6 && g_music &&
            ref_index < g_music_samples)
            expected = (uint32_t)(int32_t)g_music[ref_index] << 16;
        else
            expected = (uint32_t)((ref_index % g_dma[index].voices) + 1) << 28;
        if (words[j] != expected) {
            if (g_trace && g_dma[index].mismatches < 4)
                fprintf(g_trace,
                        "DSP_VERIFY ring=%d sample=%llu got=%#x expected=%#x\n",
                        index, (unsigned long long)ref_index,
                        (unsigned)words[j], (unsigned)expected);
            g_dma[index].mismatches++;
        }
        if (g_dma[index].voices == 6 && g_dma[index].render_a) {
            size_t out_index = (size_t)first_sample + j;
            int16_t a = (int16_t)(words[j] >> 16);
            int32_t low24 = (int32_t)(words[j] & 0x00ffffffu);
            int32_t b;
            unsigned magnitude;
            if (low24 & 0x00800000) low24 -= 0x01000000;
            /* Literal signed-low-24 interpretation, reduced to S16 for WAV. */
            b = low24 / 256;
            if (b > 32767) b = 32767;
            if (b < -32768) b = -32768;
            g_dma[index].render_a[out_index] = a;
            g_dma[index].render_b[out_index] = (int16_t)b;
            magnitude = b < 0 ? (unsigned)(-b) : (unsigned)b;
            if (magnitude > g_dma[index].b_peak)
                g_dma[index].b_peak = magnitude;
            if (b == 32767 || b == -32768)
                g_dma[index].b_saturated++;
        }
    }
    g_dma[index].calls++;
    g_dma[index].bytes += size;
    dma_append_raw(index, fragment, size);

    if (g_trace && (g_dma[index].calls == 1 ||
                    g_dma[index].calls == 10 ||
                    (g_dma[index].calls % 20) == 0 ||
                    g_dma[index].calls == DSP_CAPTURE_FRAGMENTS))
        fprintf(g_trace,
                "DSP_VERIFY ring=%d source=%s voices=%u fragments=%u "
                "bytes=%llu mismatches=%llu %s\n",
                index, source, g_dma[index].voices, g_dma[index].calls,
                (unsigned long long)g_dma[index].bytes,
                (unsigned long long)g_dma[index].mismatches,
                g_dma[index].mismatches ? "FAIL" :
                (g_dma[index].calls == DSP_CAPTURE_FRAGMENTS ?
                 "CONTINUOUS_PASS" : "PASS_SO_FAR"));
    if (g_dma[index].voices == 6 &&
        g_dma[index].calls == DSP_CAPTURE_FRAGMENTS)
        g_dma[index].emit_pending = 1;
}

/* ado_pcm_write_block @io-audio+0x11094 computes the DMA destination as
 *
 *     ring_base + fragment_index * fragment_size
 *
 * and passes that pointer directly to resmgr_msgread at 0x1111c0.  The kernel
 * copies the client message into the ring, so no libc memcpy call is involved.
 * In this no-DSP guest the next client write fails EIO before reaching the
 * server; qsa_probe prepares and retries that same fragment, returning it to
 * this proven block-copy boundary without advancing the source file. */
int resmgr_msgread(void *ctp, void *msg, int size, int offset)
{
    static int (*real)(void *, void *, int, int);
    int rc, which;

    if (!real) real = dlsym(RTLD_NEXT, "resmgr_msgread");
    which = (size > 0 && g_dma_count) ? spa_in_dma(msg, (size_t)size) : 0;

    rc = real(ctp, msg, size, offset);
    if (which && rc == size) {
        int index = which - 1;
        const unsigned char *b = (const unsigned char *)msg;
        unsigned j, write_index = g_dma[index].calls;
        unsigned preview = size < 32 ? (unsigned)size : 32;
        g_dma[index].frag_size = (size_t)size;
        g_dma[index].voices = (size % (768 * 4)) == 0 ?
                              (unsigned)size / (768 * 4) : 0;
        g_dma[index].first_slot = (unsigned)(
            ((unsigned char *)msg - (unsigned char *)g_dma[index].base) /
            (size_t)size);
        if (g_dma[index].first_slot < 10 &&
            write_index < DSP_CAPTURE_FRAGMENTS)
            g_dma[index].slot_generation[g_dma[index].first_slot] =
                write_index + 1;
        dma_capture_fragment(index, msg, (size_t)size, "resmgr");
        fprintf(g_trace,
                "DSP_CAPTURE_PATH ring=%d first_slot=%u "
                "resmgr_prepare_retry preview:",
                index, g_dma[index].first_slot);
        for (j = 0; j < preview; j++) {
            if ((j & 3) == 0) fputc(' ', g_trace);
            fprintf(g_trace, "%02x", b[j]);
        }
        fputc('\n', g_trace);
        /* Emit before returning from the 62nd write. The known teardown fault
         * can follow immediately after qsa_probe exits, so no destructor or
         * deferred worker is safe here. */
        if (g_dma[index].emit_pending &&
            !submission_measurement_enabled())
            dma_emit_renders(index);
    } else if (which && rc != size) {
        fprintf(g_trace,
                "DSP_CAPTURE resmgr partial rc=%d size=%d offset=%d\n",
                rc, size, offset);
    }
    return rc;
}

static void csd_dma_summary(void)
{
    static int calls;
    static int shown;
    int i;
    size_t j;
    if (shown > 6) return;
    if ((calls++ % 16) != 0) return;
    for (i = 0; i < g_dma_count; i++) {
        const unsigned char *b = (const unsigned char *)g_dma[i].base;
        size_t nz = 0;
        for (j = 0; j < g_dma[i].size; j++) if (b[j]) nz++;
        fprintf(g_trace, "DMA %d: %u/%u non-zero", i, (unsigned)nz,
                (unsigned)g_dma[i].size);
        if (nz) {
            shown++;
            fprintf(g_trace, "  first 32 bytes:");
            for (j = 0; j < 32; j++) {
                if ((j & 3) == 0) fprintf(g_trace, " ");
                fprintf(g_trace, "%02x", b[j]);
            }
        }
        fprintf(g_trace, "\n");
    }
}

/* Summarise every pool on each call: "all zero" is itself the answer to whether
 * the playback path uses this memory, and silence in the log cannot distinguish
 * "not dumped" from "nothing there". */
static void csd_pool_summary(void)
{
    static int calls;
    int i, j, any = 0;
    char line[512];
    size_t off = 0;
    if ((calls++ % 8) != 0) return;
    for (i = 0; i < g_pool_count && i < 12; i++) {
        const unsigned char *b = (const unsigned char *)g_pool[i].base;
        uint32_t n = g_pool[i].size, nz = 0;
        if (!b) continue;
        for (j = 0; j < (int)n; j++) if (b[j]) nz++;
        if (nz) any = 1;
        if (off < sizeof(line) - 24)
            off += snprintf(line + off, sizeof(line) - off, "%u/%u ", (unsigned)nz, (unsigned)n);
    }
    if (g_pool_count)
        fprintf(g_trace, "POOLSUM%s %s\n", any ? "(DATA)" : "(all zero)", line);
}

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
    uint32_t sequence = ++g_csd_sequence;

    csd_dump_pools_once();
    csd_pool_summary();
    csd_dma_summary();
    /* Every wrapper passes an explicit payload length to csd_ioctl. Dump before
     * handling the command: ALLOC_PMEM writes its mapped address back into word
     * three, and the measurement needs the descriptor exactly as deva sent it. */
    fprintf(g_trace,
            "CSD_IOCTL_BEGIN seq=%u handle=%d cmd=%#x len=%u arg=%p\n",
            (unsigned)sequence, handle, (unsigned)cmd, (unsigned)len, arg);
    if (len <= 4096)
        csd_hex_dump("IOCTL", sequence, arg, len);
    else
        fprintf(g_trace, "CSD_IOCTL_HEX seq=%u <refused len=%u>\n",
                (unsigned)sequence, (unsigned)len);
    fprintf(g_trace, "CSD_IOCTL_END seq=%u handle=%d cmd=%#x\n",
            (unsigned)sequence, handle, (unsigned)cmd);

    /* registerCallBack @0x2c8e8 passes exactly {function, context}.  Preserve
     * that recovered pair so the next csd_write can complete the preceding
     * submission with BUF_DONE, matching asynchronous DSP behaviour without a
     * new thread in io-audio. */
    if (cmd == CSD_AS_CMD_REGISTER_CALLBACK && arg && len >= 8 &&
        handle >= 0x100 && handle < 0x100 +
            (int)(sizeof(g_handle) / sizeof(g_handle[0]))) {
        const uint32_t *w = (const uint32_t *)arg;
        int index = handle - 0x100;
        g_handle[index].callback =
            (csd_callback_fn)(uintptr_t)w[0];
        g_handle[index].context = (void *)(uintptr_t)w[1];
        fprintf(g_trace,
                "CSD_CALLBACK_REGISTER handle=%d function=%#x context=%#x\n",
                handle, (unsigned)w[0], (unsigned)w[1]);
        /* By callback registration the loader has resolved io-audio's lazy
         * pthread_mutex_lock slot. Installing here avoids preserving its
         * pre-relocation binder trampoline from the library constructor. */
        if (submission_measurement_enabled()) install_timer_mutex_hook();
    }

    if (cmd == CSD_AS_CMD_STOP && submission_measurement_enabled()) {
        if (g_submit.calls >= DSP_CAPTURE_FRAGMENTS)
            submission_emit();
        else
            fprintf(g_trace,
                    "CSD_SUBMIT_EMIT_DEFERRED ring_calls=%u need=%u\n",
                    g_submit.calls, DSP_CAPTURE_FRAGMENTS);
    }

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
    const uint32_t *descriptor = (const uint32_t *)buf;
    uint32_t sequence = ++g_csd_write_sequence;
    uint32_t source_address = 0;
    uint32_t payload_len = len;

    fprintf(g_trace,
            "CSD_WRITE_BEGIN seq=%u handle=%d descriptor=%p len=%u\n",
            (unsigned)sequence, handle, buf, (unsigned)len);
    if (buf && len <= 4096)
        csd_hex_dump("WRITE", sequence, buf, len);
    else
        fprintf(g_trace, "CSD_WRITE_HEX seq=%u <unavailable>\n",
                (unsigned)sequence);

    /* session_csd_stream_playback builds this 36-byte descriptor at
     * 0x69190..0x69318. Words 0 and 1 are the submitted buffer address and its
     * audio-byte count; csd_write's third argument is the descriptor size. */
    if (buf && len >= 8) {
        source_address = descriptor[0];
        payload_len = descriptor[1];
    }
    if (submission_measurement_enabled())
        submission_capture(handle, source_address, payload_len);

    if (submission_measurement_enabled() && buf && len >= 8)
        callback_enqueue(handle, descriptor);

    fprintf(g_trace,
            "CSD_WRITE_END seq=%u handle=%d source_address=%#x "
            "payload_len=%u return=%u\n",
            (unsigned)sequence, handle, (unsigned)source_address,
            (unsigned)payload_len,
            (unsigned)(submission_measurement_enabled() ? payload_len : len));
    return (int32_t)(submission_measurement_enabled() ? payload_len : len);
}

static void csd_dma_summary(void);

int32_t csd_read(int32_t handle, void *buf, uint32_t len)
{
    (void)handle;
    if (buf && len) memset(buf, 0, len);
    csd_dma_summary();
    return 0;
}
