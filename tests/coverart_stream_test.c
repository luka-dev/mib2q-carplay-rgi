#include "coverart/coverart_stream.h"
#include "framework/iap2_protocol.h"
#include <assert.h>

static coverart_stream_t stream;
static uint8_t *image;
static size_t image_len;
static uint8_t jpg[600], newer[600], wire[200000];

static size_t packet(uint8_t *out, uint8_t seq, uint8_t session, uint8_t control,
                     const uint8_t *payload, size_t size)
{
    size_t n = 9 + (size ? size + 1 : 0);
    assert(n <= 65535);
    memset(out, 0, n);
    out[0] = 255; out[1] = 90; write_be16(out + 2, (uint16_t)n);
    out[4] = control; out[5] = seq; out[7] = session;
    out[8] = iap2_cksum_neg(out, 8);
    if (size) {
        memcpy(out + 9, payload, size);
        out[n - 1] = iap2_cksum_neg(payload, size);
    }
    return n;
}

static size_t data(uint8_t *out, uint8_t seq, uint8_t id, uint8_t op,
                   const uint8_t *bytes, size_t len)
{
    uint8_t payload[2000];
    assert(len <= sizeof(payload) - 2);
    payload[0] = id; payload[1] = op;
    if (len) memcpy(payload + 2, bytes, len);
    return packet(out, seq, 2, 0x40, payload, len + 2);
}

static size_t setup(uint8_t *out, uint8_t seq, uint8_t id, uint64_t size, uint16_t kind)
{
    uint8_t payload[12] = {0};
    payload[0] = id; payload[1] = 4;
    write_be64(payload + 2, size); write_be16(payload + 10, kind);
    return packet(out, seq, 2, 0x40, payload, sizeof(payload));
}

static void feed(const uint8_t *bytes, size_t len)
{
    assert(!image);
    coverart_stream_feed(&stream, bytes, len, &image, &image_len);
}

static void expect(const uint8_t *bytes, size_t len)
{
    assert(image && image_len == len && !memcmp(image, bytes, len));
    free(image); image = NULL; image_len = 0;
}

static void reset(void)
{
    assert(!image);
    coverart_stream_dispose(&stream);
}

static void fragmented_and_batched(void)
{
    size_t n, split;
    n = data(wire, 1, 1, 0x80, jpg, 300);
    n += data(wire + n, 2, 1, 0x40, jpg + 300, 300);
    for (split = 1; split < n; ++split) {
        feed(wire, split); assert(!image);
        feed(wire + split, n - split); expect(jpg, sizeof(jpg));
        assert(stream.raw_len == 0); reset();
    }
    feed(wire, n); expect(jpg, sizeof(jpg)); reset(); /* no lookahead */
    n = data(wire, 1, 1, 0xc0, jpg, 600);
    n += data(wire + n, 2, 2, 0xc0, newer, 600);
    n += data(wire + n, 3, 3, 0x80, jpg, 300);
    feed(wire, n); expect(newer, sizeof(newer));
    n = data(wire, 4, 3, 0x40, jpg + 300, 300);
    feed(wire, n); expect(jpg, sizeof(jpg)); reset();
    n = data(wire, 1, 1, 0x80, jpg, 1); /* split JPEG signature */
    feed(wire, n); assert(!image);
    n = data(wire, 2, 1, 0, jpg + 1, 599); feed(wire, n); assert(!image);
    n = data(wire, 3, 1, 0x40, NULL, 0); feed(wire, n);
    expect(jpg, sizeof(jpg)); reset();
}

static void file_identity_and_lifecycle(void)
{
    size_t n = setup(wire, 1, 1, 600, 2);
    n += setup(wire + n, 2, 2, 600, 7); /* non-artwork file, even if JPEG */
    n += data(wire + n, 3, 1, 0x80, jpg, 300);
    n += data(wire + n, 4, 2, 0xc0, newer, 600);
    n += data(wire + n, 5, 1, 0x40, jpg + 300, 300);
    feed(wire, n); expect(jpg, 600); reset();
    /* An older transfer finishing late must not replace a newer complete image,
     * including when completion happens in a later Recv callback. */
    n = data(wire, 1, 1, 0x80, jpg, 300);
    n += data(wire + n, 2, 2, 0xc0, newer, 600);
    feed(wire, n); expect(newer, 600);
    n = data(wire, 3, 1, 0x40, jpg + 300, 300);
    feed(wire, n); assert(!image); reset();
    n = data(wire, 1, 1, 0x80, jpg, 300);
    n += data(wire + n, 2, 1, 2, NULL, 0); /* cancel */
    n += data(wire + n, 3, 1, 0x40, jpg + 300, 300);
    feed(wire, n); assert(!image); reset();
    n = setup(wire, 1, 1, 599, 2);
    n += data(wire + n, 2, 1, 0xc0, jpg, 600);
    feed(wire, n); assert(!image); reset();
    n = setup(wire, 1, 1, 601, 2);
    n += data(wire + n, 2, 1, 0xc0, jpg, 600);
    feed(wire, n); assert(!image); reset();
    n = setup(wire, 1, 1, UINT64_MAX, 2);
    n += data(wire + n, 2, 1, 0xc0, jpg, 600);
    feed(wire, n); assert(!image && !stream.files[1].data); reset();
    n = data(wire, 1, 1, 0x80, jpg, 300); feed(wire, n);
    coverart_stream_reset(&stream);
    n = data(wire, 2, 1, 0x40, jpg + 300, 300);
    feed(wire, n); assert(!image); reset();
}

static void checksums_order_and_wrap(void)
{
    size_t n = data(wire, 254, 1, 0x80, jpg, 200);
    feed(wire, n); assert(!image);
    feed(wire, n); assert(!image); /* retransmitted first chunk */
    n = data(wire, 0, 1, 0x40, jpg + 400, 200);
    feed(wire, n); assert(!image && stream.pending_bytes);
    n = data(wire, 255, 1, 0, jpg + 200, 200);
    wire[n - 1] ^= 1; feed(wire, n); assert(!image);
    wire[n - 1] ^= 1; feed(wire, n); expect(jpg, 600);
    assert(!stream.pending_bytes); reset();
    n = data(wire, 1, 1, 0xc0, jpg, 600);
    wire[8] ^= 1; feed(wire, n); assert(!image);
    wire[8] ^= 1; feed(wire, n); expect(jpg, 600); reset();
}

static void size_only_setup(void)
{
    uint8_t payload[12] = {128, 4};
    uint8_t not_image[600];
    size_t n, len;
    write_be64(payload + 2, 600);
    n = packet(wire, 1, 2, 0x40, payload, 10);
    n += data(wire + n, 2, 128, 0x80, jpg, 300);
    n += data(wire + n, 3, 128, 0x40, jpg + 300, 300);
    feed(wire, n); expect(jpg, 600); reset();
    /* Unknown size is legal; completion still requires a last-data opcode. */
    write_be64(payload + 2, 0);
    n = packet(wire, 1, 2, 0x40, payload, 10);
    n += data(wire + n, 2, 128, 0x80, jpg, 600);
    feed(wire, n); assert(!image);
    n = data(wire, 3, 128, 0x40, NULL, 0);
    feed(wire, n); expect(jpg, 600); reset();
    /* A short size or half a type is malformed, not a legacy fallback. */
    for (len = 0; len <= 9; ++len) {
        if (len == 8) continue;
        n = packet(wire, 1, 2, 0x40, payload, len + 2);
        n += data(wire + n, 2, 128, 0xc0, jpg, 600);
        feed(wire, n); assert(!image); reset();
    }
    write_be64(payload + 2, 599);
    n = packet(wire, 1, 2, 0x40, payload, 10);
    n += data(wire + n, 2, 128, 0xc0, jpg, 600);
    feed(wire, n); assert(!image); reset();
    write_be64(payload + 2, COVERART_STREAM_MAX_FILE + 1);
    n = packet(wire, 1, 2, 0x40, payload, 10);
    n += data(wire + n, 2, 128, 0xc0, jpg, 600);
    feed(wire, n); assert(!image && !stream.files[128].data); reset();
    /* Size-only does not accept arbitrary data that merely contains a JPEG. */
    write_be64(payload + 2, 600);
    memcpy(not_image, jpg, sizeof(not_image)); not_image[0] = 0;
    n = packet(wire, 1, 2, 0x40, payload, 10);
    n += data(wire + n, 2, 128, 0xc0, not_image, sizeof(not_image));
    feed(wire, n); assert(!image); reset();
}

static void negotiated_sessions(void)
{
    uint8_t lsp[16] = {1, 8, 0xff, 0xff, 0, 100, 0, 20, 5, 3, 2, 0, 1, 7, 1, 1};
    uint8_t payload[602];
    size_t n = packet(wire, 10, 0, 0xc0, lsp, sizeof(lsp));
    payload[0] = 1; payload[1] = 0xc0; memcpy(payload + 2, jpg, 600);
    n += packet(wire + n, 11, 2, 0x40, payload, sizeof(payload));
    feed(wire, n); assert(!image); /* session 2 is control in this negotiation */
    n = packet(wire, 12, 7, 0x40, payload, sizeof(payload));
    feed(wire, n); expect(jpg, 600);
    coverart_stream_reset(&stream); assert(stream.session_kind[7] == 2);
    n = packet(wire, 13, 0, 0x10, NULL, 0); feed(wire, n);
    assert(!stream.have_seq && !stream.session_kind[7]); reset();
}

static void bounds_and_noise(void)
{
    unsigned i, active = 0;
    uint32_t rng = 1;
    size_t n = 0;
    for (i = 0; i < 200; ++i) n += data(wire + n, (uint8_t)i, (uint8_t)i, 0x80, jpg, 600);
    assert(n > 65535);
    feed(wire, n); assert(!image);
    for (i = 0; i < 256; ++i) active += stream.files[i].started;
    assert(active == COVERART_STREAM_MAX_ACTIVE); reset();
    for (i = 0; i < 5000; ++i) {
        size_t j, size = i % 511 + 1;
        for (j = 0; j < size; ++j) {
            rng = rng * 1664525u + 1013904223u;
            wire[j] = (uint8_t)(rng >> 24);
        }
        feed(wire, size); assert(!image);
    }
    reset();
}

int main(void)
{
    memset(jpg, 0x11, sizeof(jpg)); jpg[0] = 255; jpg[1] = 216; jpg[2] = 255;
    jpg[100] = 255; jpg[101] = 217; /* embedded thumbnail EOI must not end transfer */
    jpg[598] = 255; jpg[599] = 217;
    memcpy(newer, jpg, 600); newer[200] = 0x22;
    fragmented_and_batched(); file_identity_and_lifecycle();
    checksums_order_and_wrap(); size_only_setup(); negotiated_sessions(); bounds_and_noise();
    puts("coverart_stream_test: fragmentation, batch, file IDs, completion, checksum, reorder/wrap, reset and bounds PASS");
    return 0;
}
