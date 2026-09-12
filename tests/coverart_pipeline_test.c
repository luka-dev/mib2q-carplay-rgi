/* Exercise the real decoder/publication path with synthetic JPEG and a
 * deterministic Identify reset while encoding is in progress. */
#ifndef COVERART_DIR
#error Build this test with a temporary COVERART_DIR
#endif
#define ENABLE_LOGGING 0
#include <zlib.h>
#include <pthread.h>
#include <assert.h>
static int test_compress(Bytef *, uLongf *, const Bytef *, uLong, int);
#define compress2 test_compress
#include "../hook/coverart/coverart_hook.c"
#undef compress2
#include "../hook/framework/iap2_protocol.h"

static pthread_mutex_t pause_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pause_cond = PTHREAD_COND_INITIALIZER;
static int pause_encode, reached_encode, resume_encode;
static int notifications, interrupt_write, write_calls;
static uint8_t jpeg[4096];
static size_t jpeg_len;
static uint32_t decode_generation;
static int decode_result;

static size_t file_packet(uint8_t *out, uint8_t seq, uint8_t op,
                          const uint8_t *bytes, size_t len)
{
    size_t n = len + 12;
    memset(out, 0, n);
    out[0] = 0xff; out[1] = 0x5a; write_be16(out + 2, (uint16_t)n);
    out[4] = 0x40; out[5] = seq; out[7] = 2;
    out[8] = iap2_cksum_neg(out, 8);
    out[9] = 128; out[10] = op;
    memcpy(out + 11, bytes, len);
    out[n - 1] = iap2_cksum_neg(out + 9, n - 10);
    return n;
}

static int decode_size_only_transfer(void)
{
    uint8_t wire[8192], size[8], *image = NULL;
    size_t n, image_len;
    int result;
    write_be64(size, jpeg_len);
    n = file_packet(wire, 197, 4, size, sizeof(size));
    n += file_packet(wire + n, 198, 0x80, jpeg, jpeg_len / 2);
    n += file_packet(wire + n, 199, 0x40, jpeg + jpeg_len / 2, jpeg_len - jpeg_len / 2);
    /* Live iPhone format: 8-byte Setup, first/last data, no metadata type. */
    coverart_stream_feed(&g_recv_stream, wire, 13, &image, &image_len);
    assert(!image);
    coverart_stream_feed(&g_recv_stream, wire + 13, n - 13, &image, &image_len);
    assert(image && image_len == jpeg_len && !memcmp(image, jpeg, image_len));
    result = maybe_handle_image(image, image_len, recv_generation);
    free(image);
    return result;
}

static int test_compress(Bytef *out, uLongf *out_len, const Bytef *in, uLong len, int level)
{
    int result = compress2(out, out_len, in, len, level);
    pthread_mutex_lock(&pause_mutex);
    if (pause_encode) {
        reached_encode = 1; pthread_cond_signal(&pause_cond);
        while (!resume_encode) pthread_cond_wait(&pause_cond, &pause_mutex);
    }
    pthread_mutex_unlock(&pause_mutex);
    return result;
}
static ssize_t short_write(int fd, const void *data, size_t len)
{
    ++write_calls;
    if (interrupt_write) { interrupt_write = 0; errno = EINTR; return -1; }
    return write(fd, data, len > 17 ? 17 : len);
}
static void *decode_thread(void *unused)
{
    (void)unused;
    decode_result = maybe_handle_image(jpeg, jpeg_len, decode_generation);
    return NULL;
}

int hook_process_is_dio_manager(void) { return 1; }
void bus_text_begin_with(bus_text_builder_t *b, const char *name, uint8_t *buf, uint32_t cap)
{ (void)b; (void)name; (void)buf; (void)cap; }
void bus_text_str(bus_text_builder_t *b, const char *key, const char *value)
{ (void)b; (void)key; (void)value; }
void bus_text_uint(bus_text_builder_t *b, const char *key, uint64_t value)
{ (void)b; (void)key; (void)value; }
hook_result_t bus_send_text(uint16_t type, uint8_t flags, bus_text_builder_t *b)
{ (void)flags; (void)b; assert(type == EVT_COVERART); ++notifications; return HOOK_OK; }

int main(void)
{
    FILE *f = fopen("tests/fixtures/coverart/exif-thumbnail.jpg", "rb");
    pthread_t decoder;
    uint8_t png[10000], *rgb;
    size_t png_len;
    int width, height, channels;
    assert(f); jpeg_len = fread(jpeg, 1, sizeof(jpeg), f); fclose(f);
    assert(jpeg_len > 500 && jpeg_len < sizeof(jpeg));
    runtime_initialized = 1;
    real_write = short_write;
    pause_encode = 1; decode_generation = recv_generation;
    assert(!pthread_create(&decoder, NULL, decode_thread, NULL));
    pthread_mutex_lock(&pause_mutex);
    while (!reached_encode) pthread_cond_wait(&pause_cond, &pause_mutex);
    pthread_mutex_unlock(&pause_mutex);
    pending_data = malloc(8); pending_len = 8; pending_generation = decode_generation;
    coverart_reset_recv_stream();
    assert(!pending_data && !pending_len && worker_generation != decode_generation);
    pthread_mutex_lock(&pause_mutex);
    resume_encode = 1; pthread_cond_signal(&pause_cond);
    pthread_mutex_unlock(&pause_mutex);
    assert(!pthread_join(decoder, NULL));
    assert(!decode_result && !notifications && !write_calls);
    assert(access(COVERART_FILE, F_OK) != 0);
    enqueue_image_async(malloc(8), 8, decode_generation);
    assert(!pending_data && !worker_started); /* delayed enqueue cannot cross reset */
    pause_encode = 0; interrupt_write = 1;
    assert(decode_size_only_transfer());
    assert(notifications == 1 && write_calls > 2 && g_coverart.images_found == 1);
    f = fopen(COVERART_FILE, "rb"); assert(f);
    png_len = fread(png, 1, sizeof(png), f); fclose(f);
    rgb = stbi_load_from_memory(png, (int)png_len, &width, &height, &channels, 3);
    assert(rgb && width == 256 && height == 256);
    assert(abs((int)rgb[0] - 120) <= 3 && abs((int)rgb[1] - 70) <= 3 && abs((int)rgb[2] - 20) <= 3);
    stbi_image_free(rgb);
    assert(maybe_handle_image(jpeg, jpeg_len, recv_generation));
    assert(notifications == 1); /* CRC dedup after successful publication */
    coverart_runtime_shutdown();
    puts("coverart_pipeline_test: real EXIF JPEG decode, Identify race, stale enqueue, EINTR/short write and PNG publication PASS");
    return 0;
}
