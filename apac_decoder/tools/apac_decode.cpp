/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// apac_decode - decode APAC packets with OUR decoder and write float32 PCM.
//
// This is our side of the whole-capture comparison that
// apac_decoder/reference/run.sh runs with the library:
//
//     ./apac_decoder/reference/run.sh --f32          # library -> pioneer.f32
//     apac_decode COOKIE OUT.f32 --list LIST         # ours    -> ours.f32
//     apac_decoder/reference/compare_f32.py pioneer.f32 ours.f32 6
//
// Output is INTERLEAVED float32 in the decoder's own channel order, exactly the
// layout apac_ref writes with --f32, so the two files are directly comparable.
//
// The cookie walk is the same one APACDecoder_GetCookieOffset does: a chain of
// big-endian [u32 size][u32 type] atoms, with `dapa`'s payload starting at
// atomStart + 12.  That payload begins with the 16-bit 0x0800 sync word and
// apac::GlobalConfig starts two bytes further on - the offset trap NOTES.md
// records.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apac/config.h"
#include "apac/decoder.h"

namespace {

const uint32_t kMaxPacket = 1 << 20;

bool ReadFile(const char* path, unsigned char* buf, uint32_t cap, uint32_t* len)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "apac_decode: %s: cannot open\n", path);
        return false;
    }
    const size_t n = fread(buf, 1, cap, f);
    fclose(f);
    *len = (uint32_t)n;
    return true;
}

void DumpSpectrum(void* ctx, uint32_t channel, uint32_t windowSequence,
                  const float* spectrum, uint32_t count)
{
    FILE* f = (FILE*)ctx;
    const uint32_t hdr[2] = { channel, windowSequence };
    fwrite(hdr, sizeof hdr, 1, f);
    fwrite(spectrum, sizeof(float), count, f);
}

uint32_t BE32(const unsigned char* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

// APACDecoder_GetCookieOffset, 0x00106030.  Returns the offset of the `dapa`
// payload, or 0.
uint32_t CookieOffset(const unsigned char* c, uint32_t n, uint32_t* payloadBytes)
{
    uint32_t at = 0;
    while (at + 8 <= n) {
        const uint32_t size = BE32(c + at);
        const uint32_t type = BE32(c + at + 4);
        if (size < 8 || at + size > n)
            return 0;
        if (type == 0x64617061u) {              // 'dapa'
            if (size < 12)
                return 0;
            *payloadBytes = size - 12;
            return at + 12;
        }
        if (type == 0x66726d61u || type == 0x61706163u) {   // 'frma', 'apac'
            at += 8;                            // container: descend
            continue;
        }
        at += size;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    const char* cookiePath = 0;
    const char* outPath = 0;
    const char* listPath = 0;
    long limit = -1;
    bool quiet = false;
    const char* dumpPath = 0;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!strcmp(a, "--list") && i + 1 < argc) listPath = argv[++i];
        else if (!strcmp(a, "--count") && i + 1 < argc) limit = strtol(argv[++i], 0, 0);
        else if (!strcmp(a, "--quiet")) quiet = true;
        else if (!strcmp(a, "--dump-spectra") && i + 1 < argc) dumpPath = argv[++i];
        else if (!cookiePath) cookiePath = a;
        else if (!outPath) outPath = a;
    }
    if (!cookiePath || !outPath || !listPath) {
        fprintf(stderr, "usage: apac_decode COOKIE OUT.f32 --list LIST [--count N]\n");
        return 2;
    }

    static unsigned char cookie[4096];
    uint32_t cookieLen = 0;
    if (!ReadFile(cookiePath, cookie, sizeof cookie, &cookieLen))
        return 2;

    uint32_t payloadBytes = 0;
    const uint32_t off = CookieOffset(cookie, cookieLen, &payloadBytes);
    if (off == 0 || payloadBytes < 3) {
        fprintf(stderr, "apac_decode: no dapa atom in the cookie\n");
        return 2;
    }

    static apac::GlobalConfig gc;
    gc.Reset();
    {
        // +2: past the 0x0800 sync word.
        apac::BitstreamReader bs(cookie + off + 2, payloadBytes - 2);
        const uint32_t rc = gc.Deserialize(bs);
        if (rc != apac::kOk) {
            fprintf(stderr, "apac_decode: GlobalConfig::Deserialize -> %u\n", rc);
            return 2;
        }
    }

    // APACDecoder_Create builds an APACASPDecoder, so this is the one to use.
    static apac::ASPDecoder decoder;
    {
        const uint32_t rc = decoder.Initialize(gc);
        if (rc != apac::kOk) {
            fprintf(stderr, "apac_decode: Decoder::Initialize -> %u\n", rc);
            return 2;
        }
    }
    const uint32_t channels = decoder.NumChannels();
    const uint32_t frames = decoder.FrameLength();
    if (!quiet)
        fprintf(stderr, "apac_decode: %u Hz, %u channels, %u frames per packet\n",
                gc.sampleRate, channels, frames);

    FILE* list = fopen(listPath, "r");
    if (!list) {
        fprintf(stderr, "apac_decode: %s: cannot open\n", listPath);
        return 2;
    }
    FILE* out = fopen(outPath, "wb");
    if (!out) {
        fprintf(stderr, "apac_decode: %s: cannot create\n", outPath);
        return 2;
    }

    // Same record layout as reference/spectrum_dump_shim.cpp writes for the
    // library: [u32 channel][u32 windowSequence][1024 float32].
    FILE* dump = dumpPath ? fopen(dumpPath, "wb") : 0;
    if (dump)
        decoder.SetSpectrumSink(&DumpSpectrum, dump);

    static unsigned char packet[kMaxPacket];
    float* pcm = (float*)malloc(sizeof(float) * channels * frames);
    char line[4096];
    long done = 0, total = 0;
    int status = 0;

    while (fgets(line, sizeof line, list)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!n)
            continue;
        if (limit >= 0 && done >= limit)
            break;
        uint32_t bytes = 0;
        if (!ReadFile(line, packet, sizeof packet, &bytes)) { status = 2; break; }
        uint32_t got = 0;
        const uint32_t rc = decoder.DecodeFrame(packet, bytes, pcm, &got);
        if (rc != apac::kOk) {
            fprintf(stderr, "apac_decode: packet %ld (%s) -> status %u\n", done, line, rc);
            status = 1;
            break;
        }
        if (got)
            fwrite(pcm, sizeof(float), (size_t)channels * got, out);
        total += got;
        ++done;
    }

    fclose(list);
    fclose(out);
    if (dump)
        fclose(dump);
    free(pcm);
    if (!quiet)
        fprintf(stderr, "apac_decode: %ld packets, %ld frames, %u channels -> %s\n",
                done, total, channels, outPath);
    return status;
}
