/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apap_crypto.h"

#include <string.h>

/* ---- ChaCha20, original construction ------------------------------------------------------
 * State words 12 and 13 are a 64-bit little-endian block counter and 14/15 are the 64-bit
 * nonce.  RFC 8439 moves one nonce word into the counter; that variant is NOT what this
 * stream uses and would authenticate nothing.
 */

static uint32_t spa_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void spa_store_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t spa_rotl32(uint32_t v, int n)
{
    return (v << n) | (v >> (32 - n));
}

#define SPA_QR(a, b, c, d)                              \
    do {                                                \
        a += b; d ^= a; d = spa_rotl32(d, 16);          \
        c += d; b ^= c; b = spa_rotl32(b, 12);          \
        a += b; d ^= a; d = spa_rotl32(d, 8);           \
        c += d; b ^= c; b = spa_rotl32(b, 7);           \
    } while (0)

static void spa_chacha20_block(const uint32_t state[16], uint8_t out[64])
{
    uint32_t x[16];
    int i;

    for (i = 0; i < 16; i++) x[i] = state[i];
    for (i = 0; i < 10; i++) {
        SPA_QR(x[0], x[4], x[8],  x[12]);
        SPA_QR(x[1], x[5], x[9],  x[13]);
        SPA_QR(x[2], x[6], x[10], x[14]);
        SPA_QR(x[3], x[7], x[11], x[15]);
        SPA_QR(x[0], x[5], x[10], x[15]);
        SPA_QR(x[1], x[6], x[11], x[12]);
        SPA_QR(x[2], x[7], x[8],  x[13]);
        SPA_QR(x[3], x[4], x[9],  x[14]);
    }
    for (i = 0; i < 16; i++) spa_store_le32(out + 4 * i, x[i] + state[i]);
}

void spa_chacha20_xor(const uint8_t key[32], const uint8_t nonce[8],
                      uint64_t counter, const uint8_t *data, size_t length,
                      uint8_t *out)
{
    static const char kSigma[17] = "expand 32-byte k";
    uint32_t state[16];
    uint8_t block[64];
    size_t at = 0;
    int i;

    state[0] = spa_le32((const uint8_t *)kSigma);
    state[1] = spa_le32((const uint8_t *)kSigma + 4);
    state[2] = spa_le32((const uint8_t *)kSigma + 8);
    state[3] = spa_le32((const uint8_t *)kSigma + 12);
    for (i = 0; i < 8; i++) state[4 + i] = spa_le32(key + 4 * i);
    state[12] = (uint32_t)counter;
    state[13] = (uint32_t)(counter >> 32);
    state[14] = spa_le32(nonce);
    state[15] = spa_le32(nonce + 4);

    while (at < length) {
        size_t n = length - at;
        size_t j;
        if (n > sizeof(block)) n = sizeof(block);
        spa_chacha20_block(state, block);
        for (j = 0; j < n; j++) out[at + j] = data[at + j] ^ block[j];
        at += n;
        if (++state[12] == 0) state[13]++;
    }
}

/* ---- Poly1305 (32-bit limbs, the poly1305-donna decomposition) -----------------------------
 * 26-bit limbs so every partial product fits a uint64_t without a 128-bit type, which GCC
 * 4.9.4 on armle-v7 does not have.
 */

typedef struct {
    uint32_t r[5];
    uint32_t h[5];
    uint32_t pad[4];
    uint8_t  buffer[16];
    size_t   leftover;
} spa_poly1305_t;

static void spa_poly1305_init(spa_poly1305_t *st, const uint8_t key[32])
{
    uint32_t t0 = spa_le32(key);
    uint32_t t1 = spa_le32(key + 4);
    uint32_t t2 = spa_le32(key + 8);
    uint32_t t3 = spa_le32(key + 12);

    st->r[0] = t0 & 0x3ffffff;
    st->r[1] = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03;
    st->r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ff;
    st->r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fff;
    st->r[4] = (t3 >> 8) & 0x00fffff;

    memset(st->h, 0, sizeof(st->h));
    st->pad[0] = spa_le32(key + 16);
    st->pad[1] = spa_le32(key + 20);
    st->pad[2] = spa_le32(key + 24);
    st->pad[3] = spa_le32(key + 28);
    st->leftover = 0;
}

static void spa_poly1305_blocks(spa_poly1305_t *st, const uint8_t *m, size_t bytes,
                                uint32_t hibit)
{
    const uint32_t r0 = st->r[0], r1 = st->r[1], r2 = st->r[2];
    const uint32_t r3 = st->r[3], r4 = st->r[4];
    const uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    uint32_t h0 = st->h[0], h1 = st->h[1], h2 = st->h[2];
    uint32_t h3 = st->h[3], h4 = st->h[4];

    while (bytes >= 16) {
        uint64_t d0, d1, d2, d3, d4;
        uint32_t c;
        const uint32_t t0 = spa_le32(m);
        const uint32_t t1 = spa_le32(m + 4);
        const uint32_t t2 = spa_le32(m + 8);
        const uint32_t t3 = spa_le32(m + 12);

        h0 += t0 & 0x3ffffff;
        h1 += ((t0 >> 26) | (t1 << 6)) & 0x3ffffff;
        h2 += ((t1 >> 20) | (t2 << 12)) & 0x3ffffff;
        h3 += ((t2 >> 14) | (t3 << 18)) & 0x3ffffff;
        h4 += (t3 >> 8) | hibit;

        d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 +
             (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 +
             (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 +
             (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 +
             (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 +
             (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
        d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
        d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
        d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
        d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
        h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
        h1 += c;

        m += 16;
        bytes -= 16;
    }

    st->h[0] = h0; st->h[1] = h1; st->h[2] = h2; st->h[3] = h3; st->h[4] = h4;
}

static void spa_poly1305_update(spa_poly1305_t *st, const uint8_t *m, size_t bytes)
{
    if (st->leftover) {
        size_t want = 16 - st->leftover;
        if (want > bytes) want = bytes;
        memcpy(st->buffer + st->leftover, m, want);
        st->leftover += want;
        m += want;
        bytes -= want;
        if (st->leftover < 16) return;
        spa_poly1305_blocks(st, st->buffer, 16, 1u << 24);
        st->leftover = 0;
    }
    if (bytes >= 16) {
        size_t want = bytes & ~(size_t)15;
        spa_poly1305_blocks(st, m, want, 1u << 24);
        m += want;
        bytes -= want;
    }
    if (bytes) {
        memcpy(st->buffer, m, bytes);
        st->leftover = bytes;
    }
}

static void spa_poly1305_finish(spa_poly1305_t *st, uint8_t tag[16])
{
    uint32_t h0, h1, h2, h3, h4, c;
    uint32_t g0, g1, g2, g3, g4, mask;
    uint64_t f;

    if (st->leftover) {
        size_t i = st->leftover;
        st->buffer[i++] = 1;
        for (; i < 16; i++) st->buffer[i] = 0;
        spa_poly1305_blocks(st, st->buffer, 16, 0);
    }

    h0 = st->h[0]; h1 = st->h[1]; h2 = st->h[2]; h3 = st->h[3]; h4 = st->h[4];
    c = h1 >> 26; h1 &= 0x3ffffff;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;

    g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    g4 = h4 + c - (1u << 26);

    mask = (g4 >> 31) - 1;              /* all-ones iff h >= p */
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    h0 = (h0 | (h1 << 26)) & 0xffffffff;
    h1 = ((h1 >> 6) | (h2 << 20)) & 0xffffffff;
    h2 = ((h2 >> 12) | (h3 << 14)) & 0xffffffff;
    h3 = ((h3 >> 18) | (h4 << 8)) & 0xffffffff;

    f = (uint64_t)h0 + st->pad[0]; h0 = (uint32_t)f;
    f = (uint64_t)h1 + st->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + st->pad[2] + (f >> 32); h2 = (uint32_t)f;
    f = (uint64_t)h3 + st->pad[3] + (f >> 32); h3 = (uint32_t)f;

    spa_store_le32(tag, h0);
    spa_store_le32(tag + 4, h1);
    spa_store_le32(tag + 8, h2);
    spa_store_le32(tag + 12, h3);
    memset(st, 0, sizeof(*st));
}

void spa_poly1305_mac(const uint8_t key[32], const uint8_t *message,
                      size_t length, uint8_t tag[16])
{
    spa_poly1305_t st;
    spa_poly1305_init(&st, key);
    spa_poly1305_update(&st, message, length);
    spa_poly1305_finish(&st, tag);
}

/* ---- the AEAD ------------------------------------------------------------------------------ */

static void spa_poly1305_len(spa_poly1305_t *st, uint64_t value)
{
    uint8_t buf[8];
    spa_store_le32(buf, (uint32_t)value);
    spa_store_le32(buf + 4, (uint32_t)(value >> 32));
    spa_poly1305_update(st, buf, sizeof(buf));
}

static void spa_poly1305_pad16(spa_poly1305_t *st, size_t length)
{
    static const uint8_t zeros[16] = { 0 };
    const size_t rem = length & 15u;
    if (rem) spa_poly1305_update(st, zeros, 16 - rem);
}

/* AEAD_CHACHA20_POLY1305, original construction: the Poly1305 one-time key is the first 32
 * bytes of block 0 and the payload starts at block 1. */
static int spa_aead_verify(const uint8_t key[32], const uint8_t nonce[8],
                           const uint8_t *aad, size_t aadLength,
                           const uint8_t *ciphertext, size_t ciphertextLength,
                           const uint8_t expected[16])
{
    uint8_t zeros[64];
    uint8_t polyKey[64];
    uint8_t tag[16];
    spa_poly1305_t st;
    uint8_t diff = 0;
    size_t i;

    memset(zeros, 0, sizeof(zeros));
    spa_chacha20_xor(key, nonce, 0, zeros, sizeof(zeros), polyKey);

    spa_poly1305_init(&st, polyKey);
    spa_poly1305_update(&st, aad, aadLength);
    spa_poly1305_pad16(&st, aadLength);
    spa_poly1305_update(&st, ciphertext, ciphertextLength);
    spa_poly1305_pad16(&st, ciphertextLength);
    spa_poly1305_len(&st, (uint64_t)aadLength);
    spa_poly1305_len(&st, (uint64_t)ciphertextLength);
    spa_poly1305_finish(&st, tag);

    for (i = 0; i < sizeof(tag); i++) diff |= (uint8_t)(tag[i] ^ expected[i]);
    memset(polyKey, 0, sizeof(polyKey));
    memset(tag, 0, sizeof(tag));
    return diff == 0;
}

int spa_apap_open(const uint8_t key[SPA_APAP_KEY_SIZE],
                  const uint8_t *packet, size_t packetLength,
                  uint8_t *out, size_t outCapacity,
                  size_t *plaintextLength, int *aadVariant)
{
    const uint8_t *body;
    const uint8_t *ciphertext;
    const uint8_t *tag;
    const uint8_t *nonce;
    size_t bodyLength, ciphertextLength;
    int variant;

    if (!key || !packet || !out || !plaintextLength || !aadVariant) return 0;
    if (packetLength < SPA_APAP_HEADER_SIZE + SPA_APAP_OVERHEAD) return 0;

    body = packet + SPA_APAP_HEADER_SIZE;
    bodyLength = packetLength - SPA_APAP_HEADER_SIZE;
    ciphertextLength = bodyLength - SPA_APAP_OVERHEAD;
    if (ciphertextLength > outCapacity) return 0;
    ciphertext = body;
    tag = body + ciphertextLength;
    nonce = tag + SPA_APAP_TAG_SIZE;

    /* Audio first, then extension: the same order tools/pioneer_apap_decrypt.py uses, so a
     * stream that authenticates there authenticates identically here. */
    for (variant = SPA_APAP_AAD_AUDIO; variant <= SPA_APAP_AAD_EXTENSION; variant++) {
        const uint8_t *aad = (variant == SPA_APAP_AAD_AUDIO) ? packet + 4 : packet;
        const size_t aadLength = (variant == SPA_APAP_AAD_AUDIO) ? 8u : 12u;

        if (!spa_aead_verify(key, nonce, aad, aadLength, ciphertext, ciphertextLength, tag))
            continue;

        spa_chacha20_xor(key, nonce, 1, ciphertext, ciphertextLength, out);
        *plaintextLength = ciphertextLength;
        *aadVariant = variant;
        return 1;
    }
    return 0;
}

/* ---- the TLV prefix ------------------------------------------------------------------------ */

static int spa_read_uleb128(const uint8_t *data, size_t length, size_t *at, uint64_t *value)
{
    uint64_t result = 0;
    int shift = 0;
    int i;

    for (i = 0; i < 10; i++) {
        uint8_t byte;
        if (*at >= length) return 0;
        byte = data[(*at)++];
        result |= (uint64_t)(byte & 0x7fu) << shift;
        if (!(byte & 0x80u)) {
            *value = result;
            return 1;
        }
        shift += 7;
    }
    return 0;
}

static uint64_t spa_be_bytes(const uint8_t *p, size_t n)
{
    uint64_t v = 0;
    size_t i;
    for (i = 0; i < n && i < 8; i++) v = (v << 8) | p[i];
    return v;
}

int spa_apap_parse_extension(const uint8_t *plaintext, size_t length,
                             spa_apap_extension_t *state,
                             const uint8_t **audio, size_t *audioLength)
{
    size_t at = 0;

    if (!plaintext || !state || !audio || !audioLength) return 0;

    while (at < length) {
        uint64_t fieldId = 0, fieldLength = 0;
        size_t end;

        if (!spa_read_uleb128(plaintext, length, &at, &fieldId)) return 0;
        if (fieldId == SPA_APAP_TLV_END) {
            *audio = plaintext + at;
            *audioLength = length - at;
            return 1;
        }
        if (!spa_read_uleb128(plaintext, length, &at, &fieldLength)) return 0;
        if (fieldLength > length || at + (size_t)fieldLength > length) return 0;
        end = at + (size_t)fieldLength;

        /* Neither field 2 nor field 8 carries an APAudioFormatIndex.  Two sessions settle it:
         * one negotiated AAC-LC stereo (index 23), the other APAC 5.1 (index 65), and BOTH
         * announced 1024 here -- which is the "spf", samples per frame, from the same SETUP
         * dictionary.  A field that reads the same in two different formats is not naming the
         * format.  Reading it as one overwrote the negotiated index on the first packet and
         * muted every track in both sessions.
         *
         * The format comes from the SETUP response and is confirmed by the magic cookie; a real
         * format change shows up as a new cookie id, which is tracked separately.  These two
         * fields are parsed and skipped. */
        if (fieldId == SPA_APAP_TLV_MAGIC_COOKIE && fieldLength) {
            state->magicCookie = plaintext + at;
            state->magicCookieLength = (size_t)fieldLength;
        } else if (fieldId == SPA_APAP_TLV_MAGIC_COOKIE_ID && fieldLength <= 8) {
            state->magicCookieId = spa_be_bytes(plaintext + at, (size_t)fieldLength);
            state->hasMagicCookieId = 1;
        }
        at = end;
    }

    /* Ran off the end without the terminator: the list is malformed, and there is no way to
     * tell where the audio would have started. */
    return 0;
}

/* ---- BE32-inclusive frame assembly ---------------------------------------------------------- */

void spa_apap_framer_init(spa_apap_framer_t *framer, uint8_t *frame, size_t frameCapacity)
{
    if (!framer) return;
    memset(framer, 0, sizeof(*framer));
    framer->frame = frame;
    framer->frameCapacity = frame ? frameCapacity : 0;
}

size_t spa_apap_framer_want(const spa_apap_framer_t *framer, size_t max)
{
    size_t want;

    if (!framer || !max) return max;
    if (framer->remaining == 0 && framer->headerFill < sizeof(framer->header))
        want = sizeof(framer->header) - framer->headerFill;
    else
        want = framer->remaining;
    if (!want || want > max) want = max;
    return want;
}

int spa_apap_framer_push(spa_apap_framer_t *framer, const uint8_t *data, size_t length,
                         size_t *consumed, const uint8_t **frame, size_t *frameLength)
{
    size_t at = 0;

    if (consumed) *consumed = 0;
    if (!framer || (!data && length)) return -1;

    while (at < length) {
        if (framer->remaining == 0 && framer->headerFill < sizeof(framer->header)) {
            size_t want = sizeof(framer->header) - framer->headerFill;
            if (want > length - at) want = length - at;
            memcpy(framer->header + framer->headerFill, data + at, want);
            framer->headerFill += want;
            at += want;
            if (framer->headerFill < sizeof(framer->header)) continue;

            {
                const uint32_t declared = ((uint32_t)framer->header[0] << 24) |
                                          ((uint32_t)framer->header[1] << 16) |
                                          ((uint32_t)framer->header[2] << 8) |
                                          (uint32_t)framer->header[3];
                if (declared < sizeof(framer->header)) {
                    if (consumed) *consumed = at;
                    return -1;
                }
                framer->payload = (size_t)declared - sizeof(framer->header);
                /* An oversized frame is not a truncation to recover from: the two sides would be
                 * out of step for the rest of the stream. */
                if (framer->payload > framer->frameCapacity) {
                    if (consumed) *consumed = at;
                    return -1;
                }
                framer->remaining = framer->payload;
                framer->fill = 0;
            }
            if (framer->remaining == 0) {
                framer->headerFill = 0;
                if (frame) *frame = framer->frame;
                if (frameLength) *frameLength = 0;
                if (consumed) *consumed = at;
                return 1;
            }
            continue;
        }

        {
            size_t want = framer->remaining;
            if (want > length - at) want = length - at;
            if (framer->frame) memcpy(framer->frame + framer->fill, data + at, want);
            framer->fill += want;
            framer->remaining -= want;
            at += want;
            if (framer->remaining == 0) {
                framer->headerFill = 0;
                if (frame) *frame = framer->frame;
                if (frameLength) *frameLength = framer->fill;
                if (consumed) *consumed = at;
                return 1;
            }
        }
    }

    if (consumed) *consumed = at;
    return 0;
}
