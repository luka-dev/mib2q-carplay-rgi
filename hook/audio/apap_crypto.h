/*
 * apap_crypto.h — opening a CarPlay type-103 APAP packet.
 *
 * The wire contract is Pioneer SPHERA 1.030010's, established statically in
 * docs/PIONEER_TYPE103_STATIC_SPEC.md and confirmed against 3980 live frames by
 * tools/pioneer_apap_decrypt.py:
 *
 *   frame   = BE32 inclusive length || packet
 *   packet  = fixed header[15] || ciphertext || poly1305 tag[16] || nonce[8]
 *
 * The AEAD is ChaCha20-Poly1305 in the ORIGINAL construction — an eight-byte
 * nonce and a 64-bit block counter, not RFC 8439's 12-byte nonce and 32-bit
 * counter.  The key is the type-103 SETUP `shk` copied verbatim; there is no
 * HKDF anywhere on this path.
 *
 * Two AAD variants are in use and the packet does not say which:
 *   audio       header[4:12]   (media time scale + 24-bit sequence)
 *   extension   header[0:12]   (the whole media time and scale)
 * Both are tried, audio first, exactly as the Python tool does.  Every frame of
 * the reference capture authenticates under the extension variant, but the
 * audio variant is part of the contract and a stream may use it.
 *
 * The plaintext of an extension packet is a ULEB128 TLV list terminated by a
 * zero field id; the APAC packet is whatever follows the terminator.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef APAP_CRYPTO_H
#define APAP_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#define SPA_APAP_KEY_SIZE      32u
#define SPA_APAP_HEADER_SIZE   15u
#define SPA_APAP_TAG_SIZE      16u
#define SPA_APAP_NONCE_SIZE    8u
#define SPA_APAP_OVERHEAD      (SPA_APAP_TAG_SIZE + SPA_APAP_NONCE_SIZE)

/* AAD variant that authenticated, reported by spa_apap_open(). */
enum {
    SPA_APAP_AAD_AUDIO = 0,     /* header[4:12] */
    SPA_APAP_AAD_EXTENSION = 1  /* header[0:12] */
};

/* Observed TLV field ids. Fields 2 and 8 both carried samples-per-frame
 * (1024) in independent AAC/23 and APAC/65 sessions; neither is a format
 * index. They are deliberately skipped. */
enum {
    SPA_APAP_TLV_END = 0,
    SPA_APAP_TLV_FRAME_LENGTH = 2,
    SPA_APAP_TLV_FRAME_LENGTH_ALT = 8,
    SPA_APAP_TLV_MAGIC_COOKIE_ID = 17,
    SPA_APAP_TLV_MAGIC_COOKIE = 18
};

/* What a decoded packet's TLV prefix said. The formatIndex pair is retained as
 * an internal compatibility field but the parser deliberately never populates
 * it: codec changes are identified by cookie, not fields 2/8. */
typedef struct {
    int hasFormatIndex;
    int hasMagicCookieId;
    uint64_t formatIndex;
    uint64_t magicCookieId;
    /* The cookie carried by this packet, when it carries one.  It points into the caller's
     * plaintext buffer and is only valid until the next packet is opened.  Having it per packet
     * is what makes a mid-session codec change possible without depending on SETMAGICCOOKIE
     * arriving first, or at all. */
    const uint8_t *magicCookie;
    size_t magicCookieLength;
} spa_apap_extension_t;

/* ChaCha20 keystream XOR, original construction: 8-byte nonce, 64-bit counter.
 * `counter` is the first block counter.  In-place is allowed (data == out). */
void spa_chacha20_xor(const uint8_t key[32], const uint8_t nonce[8],
                      uint64_t counter, const uint8_t *data, size_t length,
                      uint8_t *out);

/* Poly1305 over `parts`, producing a 16-byte tag. */
void spa_poly1305_mac(const uint8_t key[32], const uint8_t *message,
                      size_t length, uint8_t tag[16]);

/* Authenticate and decrypt one APAP packet (header included, length prefix
 * already stripped).  Writes at most `packetLength - SPA_APAP_HEADER_SIZE -
 * SPA_APAP_OVERHEAD` bytes to `out`.
 *
 * Returns 1 on success and sets *plaintextLength and *aadVariant; returns 0 if
 * the packet is malformed or neither AAD variant authenticates.  A failure
 * writes nothing the caller may use.
 */
int spa_apap_open(const uint8_t key[SPA_APAP_KEY_SIZE],
                  const uint8_t *packet, size_t packetLength,
                  uint8_t *out, size_t outCapacity,
                  size_t *plaintextLength, int *aadVariant);

/* ---- BE32-inclusive frame assembly ----------------------------------------------------------
 *
 * The buffered stream is a byte stream: one read can carry part of a length prefix, or part of a
 * body, and a frame can span any number of reads.  This is the assembly, separated from the
 * socket so it can be driven at every split point offline.
 *
 * `frame` is caller-owned storage for one packet body (the bytes after the length prefix), and
 * `frameCapacity` is the hard bound on a frame this receiver will assemble.  A frame larger than
 * that is a protocol error, not a truncation: the sender and the receiver would be out of step.
 *
 * spa_apap_framer_want() exists so the caller can read exactly one framing phase at a time.  That
 * is what keeps the drain from defeating TCP flow control - without it a fast reader swallows the
 * phone's whole multi-minute burst in seconds - so a socket caller must honour it even though
 * spa_apap_framer_push() copes with any chunk size.
 */
typedef struct {
    uint8_t header[4];
    size_t headerFill;
    uint8_t *frame;
    size_t frameCapacity;
    size_t fill;
    size_t payload;      /* declared body length of the frame in progress */
    size_t remaining;
} spa_apap_framer_t;

void spa_apap_framer_init(spa_apap_framer_t *framer, uint8_t *frame, size_t frameCapacity);

/* Bytes the caller should read next, capped at `max`.  Never zero. */
size_t spa_apap_framer_want(const spa_apap_framer_t *framer, size_t max);

/* Consume up to `length` bytes, stopping as soon as one frame completes.  Returns
 *   1  a frame completed; *frame / *frameLength describe it, valid until the next push
 *   0  more bytes needed
 *  -1  protocol error: a declared length below the prefix or above frameCapacity
 * and always sets *consumed to how many bytes were taken.  Call again with the remainder.
 */
int spa_apap_framer_push(spa_apap_framer_t *framer, const uint8_t *data, size_t length,
                         size_t *consumed, const uint8_t **frame, size_t *frameLength);

/* Split an extension packet's plaintext into its TLV state and the APAC packet
 * that follows the terminator.  Returns 1 on a well-formed TLV list, 0 if it is
 * truncated or unterminated.  `*audio` points into `plaintext`.
 */
int spa_apap_parse_extension(const uint8_t *plaintext, size_t length,
                             spa_apap_extension_t *state,
                             const uint8_t **audio, size_t *audioLength);

#endif /* APAP_CRYPTO_H */
