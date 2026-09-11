#include "coverart_stream.h"
#include "../framework/iap2_protocol.h"

#define LINK_HEADER 9u
#define LINK_MAX 65535u
#define FILE_ARTWORK 2u

static void file_clear(coverart_file_t *f)
{
    free(f->data);
    memset(f, 0, sizeof(*f));
}

static void transfers_clear(coverart_stream_t *s)
{
    unsigned i;
    for (i = 0; i < 256; ++i) {
        file_clear(&s->files[i]);
        free(s->pending[i].data);
        memset(&s->pending[i], 0, sizeof(s->pending[i]));
    }
    s->pending_bytes = 0;
    s->have_seq = 0;
    s->order = 0;
    s->last_image_order = 0;
    s->have_image_order = 0;
}

void coverart_stream_reset(coverart_stream_t *s)
{
    transfers_clear(s);
    s->raw_len = 0;
    s->confirmed = 0;
}

void coverart_stream_dispose(coverart_stream_t *s)
{
    transfers_clear(s);
    free(s->raw);
    memset(s, 0, sizeof(*s));
}

static int image_signature(const uint8_t *p, size_t len)
{
    static const uint8_t png[] = {137,80,78,71,13,10,26,10};
    return (len >= 3 && p[0] == 255 && p[1] == 216 && p[2] == 255) ||
           (len >= sizeof(png) && !memcmp(p, png, sizeof(png)));
}

static void make_file_room(coverart_stream_t *s)
{
    unsigned i, count = 0, oldest = 0;
    uint32_t age = 0;
    for (i = 0; i < 256; ++i) {
        coverart_file_t *f = &s->files[i];
        if (!f->started) continue;
        ++count;
        if (s->order - f->order >= age) {
            age = s->order - f->order;
            oldest = i;
        }
    }
    if (count >= COVERART_STREAM_MAX_ACTIVE) file_clear(&s->files[oldest]);
}

static void file_payload(coverart_stream_t *s, uint8_t session,
                         const uint8_t *p, size_t len,
                         uint8_t **image, size_t *image_len)
{
    coverart_file_t *f;
    uint8_t op;
    size_t need, cap;
    uint8_t *data;
    if (len < 2 || (s->session_kind[session] && s->session_kind[session] != 2)) return;
    f = &s->files[p[0]];
    op = p[1]; p += 2; len -= 2;
    if (f->session != session) file_clear(f);
    f->session = session;
    if (op == 4) { /* Setup: uint64 size, uint16 metadata type, extra metadata. */
        file_clear(f);
        f->session = session;
        f->kind = 2; /* reject data after malformed/non-artwork Setup */
        if (len < 10 || read_be16(p + 8) != FILE_ARTWORK) return;
        f->expected = read_be64(p);
        if (f->expected > COVERART_STREAM_MAX_FILE) return;
        f->kind = 1;
        return;
    }
    if (op == 2 || op == 5 || op == 6) { /* Cancel, success, failure */
        file_clear(f);
        return;
    }
    if (op != 0 && op != 0x40 && op != 0x80 && op != 0xc0) return;
    if (f->kind == 2) return;
    if (op & 0x80) {
        free(f->data); f->data = NULL; f->len = f->cap = 0; f->started = 0;
        make_file_room(s);
        f->started = 1;
        f->order = ++s->order;
    } else if (!f->started) {
        return; /* no continuation without its first chunk */
    }
    if (len > COVERART_STREAM_MAX_FILE - f->len ||
        (f->expected && len > f->expected - f->len)) {
        file_clear(f);
        return;
    }
    need = f->len + len;
    if (need > f->cap) {
        cap = f->cap ? f->cap : 4096u;
        while (cap < need) cap = cap > COVERART_STREAM_MAX_FILE / 2u ?
                                  COVERART_STREAM_MAX_FILE : cap * 2u;
        data = realloc(f->data, cap);
        if (!data) { file_clear(f); return; }
        f->data = data; f->cap = cap;
    }
    if (len) memcpy(f->data + f->len, p, len);
    f->len = need;
    /* Legacy senders may start a file without Setup. Only accept an image at
     * offset zero, allowing its signature to be split across data packets. */
    if (f->len >= 8 && !image_signature(f->data, f->len)) {
        file_clear(f);
        return;
    }
    if (op & 0x40) {
        if ((!f->expected || f->expected == f->len) && image_signature(f->data, f->len) &&
            (!s->have_image_order || (int32_t)(f->order - s->last_image_order) > 0)) {
            free(*image);
            *image = f->data; *image_len = f->len;
            f->data = NULL;
            s->last_image_order = f->order; s->have_image_order = 1;
        }
        file_clear(f);
    }
}

static void ordered_packet(coverart_stream_t *s, const uint8_t *p, size_t len,
                           uint8_t **image, size_t *image_len)
{
    file_payload(s, p[7], p + LINK_HEADER, len - LINK_HEADER - 1, image, image_len);
    s->next_seq = (uint8_t)(p[5] + 1u);
}

static void link_packet(coverart_stream_t *s, const uint8_t *p, size_t len,
                        uint8_t **image, size_t *image_len)
{
    uint8_t delta, seq = p[5], control = p[4];
    coverart_packet_t *held;
    if (control & 0x90) { /* SYN or RST establishes a new link generation. */
        if (s->have_seq && control == 0xc0 && s->next_seq == (uint8_t)(seq + 1)) return;
        transfers_clear(s);
        memset(s->session_kind, 0, sizeof(s->session_kind));
        free(*image); *image = NULL; *image_len = 0;
        if (control & 0x80) {
            size_t off;
            const uint8_t *lsp = p + LINK_HEADER;
            size_t n = len > LINK_HEADER ? len - LINK_HEADER - 1 : 0;
            if (n >= 10 && lsp[0] == 1) {
                for (off = 10; off + 3 <= n; off += 3)
                    if (lsp[off + 1] <= 2) s->session_kind[lsp[off]] = lsp[off + 1] + 1;
            }
            s->have_seq = 1; s->next_seq = (uint8_t)(seq + 1);
        }
        return;
    }
    /* Pure ACK, EAK and sleep packets do not consume a session sequence. */
    if (control != 0x40 || len <= LINK_HEADER + 1) return;
    if (!s->have_seq) { s->have_seq = 1; s->next_seq = seq; }
    delta = (uint8_t)(seq - s->next_seq);
    if (delta >= 128) return; /* duplicate/old packet, including sequence wrap */
    if (delta) {
        held = &s->pending[seq];
        if (held->data) return;
        if (len > COVERART_STREAM_MAX_PENDING - s->pending_bytes) {
            /* Never splice a gap into an image. Abandon incomplete transfers
             * at the memory bound; a fresh first-data packet can restart. */
            transfers_clear(s);
            free(*image); *image = NULL; *image_len = 0;
            s->have_seq = 1; s->next_seq = seq;
        } else {
            held->data = malloc(len);
            if (!held->data) return;
            memcpy(held->data, p, len); held->len = len; s->pending_bytes += len;
            return;
        }
    }
    ordered_packet(s, p, len, image, image_len);
    while (s->pending[s->next_seq].data) {
        coverart_packet_t next = s->pending[s->next_seq];
        memset(&s->pending[s->next_seq], 0, sizeof(next));
        s->pending_bytes -= next.len;
        ordered_packet(s, next.data, next.len, image, image_len);
        free(next.data);
    }
}

void coverart_stream_feed(coverart_stream_t *s, const uint8_t *bytes, size_t len,
                         uint8_t **image, size_t *image_len)
{
    *image = NULL; *image_len = 0;
    if (!bytes || !len) return;
    if (!s->raw) {
        s->raw = malloc(LINK_MAX);
        if (!s->raw) return;
    }
    while (len) {
        size_t take = LINK_MAX - s->raw_len, off = 0;
        if (take > len) take = len;
        memcpy(s->raw + s->raw_len, bytes, take);
        s->raw_len += take; bytes += take; len -= take;
        while (s->raw_len - off >= LINK_HEADER) {
            const uint8_t *p = s->raw + off;
            size_t n;
            if (p[0] != 0xff || p[1] != 0x5a || iap2_cksum_neg(p, LINK_HEADER)) {
                ++off; continue;
            }
            n = read_be16(p + 2);
            if (n < LINK_HEADER) { ++off; continue; }
            if (n > s->raw_len - off) break;
            if (n == LINK_HEADER || !iap2_cksum_neg(p + LINK_HEADER, n - LINK_HEADER)) {
                s->confirmed = 1;
                link_packet(s, p, n, image, image_len);
            }
            off += n;
        }
        if (off) {
            s->raw_len -= off;
            memmove(s->raw, s->raw + off, s->raw_len);
        }
    }
}
