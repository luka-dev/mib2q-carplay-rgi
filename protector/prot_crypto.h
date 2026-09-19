/* prot_crypto.h - bundled SHA-256 + HMAC + CRC32, no OpenSSL (QNX/embedded safe).
 * Header-only so the same code is shared by prot_rt.c and any host test.
 * SHA-256 is a compact public-domain-style implementation. */
#ifndef PROT_CRYPTO_H
#define PROT_CRYPTO_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ---- CRC32 (IEEE, same polynomial as zlib.crc32) ---- */
static uint32_t prot_crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xedb88320u & -(c & 1));
    }
    return c ^ 0xffffffffu;
}

/* ---- SHA-256 ---- */
typedef struct { uint32_t s[8]; uint64_t len; uint8_t buf[64]; size_t n; } prot_sha256_ctx;

static const uint32_t PROT_K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };

#define PROT_ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))

static void prot_sha256_blk(prot_sha256_ctx *c, const uint8_t *p)
{
    uint32_t w[64], a,b,cc,d,e,f,g,h,t1,t2;
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i*4]<<24 | (uint32_t)p[i*4+1]<<16 |
               (uint32_t)p[i*4+2]<<8 | p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = PROT_ROR(w[i-15],7) ^ PROT_ROR(w[i-15],18) ^ (w[i-15]>>3);
        uint32_t s1 = PROT_ROR(w[i-2],17) ^ PROT_ROR(w[i-2],19) ^ (w[i-2]>>10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a=c->s[0];b=c->s[1];cc=c->s[2];d=c->s[3];e=c->s[4];f=c->s[5];g=c->s[6];h=c->s[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = PROT_ROR(e,6)^PROT_ROR(e,11)^PROT_ROR(e,25);
        uint32_t ch = (e&f)^(~e&g);
        t1 = h + S1 + ch + PROT_K256[i] + w[i];
        uint32_t S0 = PROT_ROR(a,2)^PROT_ROR(a,13)^PROT_ROR(a,22);
        uint32_t mj = (a&b)^(a&cc)^(b&cc);
        t2 = S0 + mj;
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c->s[0]+=a;c->s[1]+=b;c->s[2]+=cc;c->s[3]+=d;c->s[4]+=e;c->s[5]+=f;c->s[6]+=g;c->s[7]+=h;
}

static void prot_sha256_init(prot_sha256_ctx *c)
{
    c->s[0]=0x6a09e667;c->s[1]=0xbb67ae85;c->s[2]=0x3c6ef372;c->s[3]=0xa54ff53a;
    c->s[4]=0x510e527f;c->s[5]=0x9b05688c;c->s[6]=0x1f83d9ab;c->s[7]=0x5be0cd19;
    c->len=0;c->n=0;
}
static void prot_sha256_update(prot_sha256_ctx *c, const uint8_t *p, size_t n)
{
    c->len += n;
    while (n) {
        size_t k = 64 - c->n; if (k > n) k = n;
        memcpy(c->buf + c->n, p, k); c->n += k; p += k; n -= k;
        if (c->n == 64) { prot_sha256_blk(c, c->buf); c->n = 0; }
    }
}
static void prot_sha256_final(prot_sha256_ctx *c, uint8_t out[32])
{
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80;
    prot_sha256_update(c, &pad, 1);
    uint8_t z = 0;
    while (c->n != 56) prot_sha256_update(c, &z, 1);
    uint8_t L[8];
    for (int i = 0; i < 8; i++) L[i] = (uint8_t)(bits >> (56 - 8*i));
    prot_sha256_update(c, L, 8);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(c->s[i]>>24); out[i*4+1] = (uint8_t)(c->s[i]>>16);
        out[i*4+2] = (uint8_t)(c->s[i]>>8);  out[i*4+3] = (uint8_t)c->s[i];
    }
}
static void prot_sha256(const uint8_t *p, size_t n, uint8_t out[32])
{
    prot_sha256_ctx c; prot_sha256_init(&c); prot_sha256_update(&c,p,n); prot_sha256_final(&c,out);
}

/* ---- HMAC-SHA256 ---- */
static void prot_hmac(const uint8_t *key, size_t klen,
                      const uint8_t *msg, size_t mlen, uint8_t out[32])
{
    uint8_t k[64] = {0}, ip[64], op[64], ih[32];
    if (klen > 64) { prot_sha256(key, klen, k); } else memcpy(k, key, klen);
    for (int i = 0; i < 64; i++) { ip[i] = k[i]^0x36; op[i] = k[i]^0x5c; }
    prot_sha256_ctx c;
    prot_sha256_init(&c); prot_sha256_update(&c,ip,64); prot_sha256_update(&c,msg,mlen);
    prot_sha256_final(&c,ih);
    prot_sha256_init(&c); prot_sha256_update(&c,op,64); prot_sha256_update(&c,ih,32);
    prot_sha256_final(&c,out);
}

/* ---- sha256 counter-mode keystream, matches protect.py _ks() ---- */
static void prot_keystream(const uint8_t key[32], uint8_t *out, size_t n)
{
    uint32_t ctr = 0;
    while (n) {
        uint8_t in[36], blk[32];
        memcpy(in, key, 32);
        in[32]=(uint8_t)(ctr>>24);in[33]=(uint8_t)(ctr>>16);in[34]=(uint8_t)(ctr>>8);in[35]=(uint8_t)ctr;
        prot_sha256(in, 36, blk);
        size_t k = n < 32 ? n : 32;
        memcpy(out, blk, k); out += k; n -= k; ctr++;
    }
}

#endif /* PROT_CRYPTO_H */
