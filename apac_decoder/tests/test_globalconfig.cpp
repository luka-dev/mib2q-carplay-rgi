// Differential test: our apac::GlobalConfig::Deserialize against the real one,
// on the two magic cookies that exist.
//
// FIXTURE: tests/fixtures/globalconfig.fixture, produced by calling
// apac::GlobalConfig::Deserialize at 0x00097d50 inside the running library and
// reading the resulting object's fields back out at their recovered offsets.
//
//   regenerate:  apac_decoder/tools/gen_fixtures.sh     (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, commands `gc`,
//                `cookie` and `scene`
//
// The cookies themselves are in the fixture as COOKIEHEX, because neither lives
// in the repository: captures/ is gitignored (it also holds a live session key)
// and vectors/ is generated.  A magic cookie is codec configuration, not key
// material.
//
//   live       33 bytes, the real CarPlay type-103 5.1 stream
//   generated 125 bytes, afconvert -d apac -l MPEG_5_1_A
//
// THE OFFSET TRAP: APACDecoder_GetCookieOffset returns the start of the `dapa`
// payload, and that payload BEGINS with the 16-bit 0x0800 sync word.
// GlobalConfig starts two bytes further on.  Read at the payload offset itself,
// the metadata helpers report 96000 Hz, 32 channels and frame length 0.  The
// test asserts the +2 relationship from the library's own numbers.
//
// WHAT IS NOT PROVEN HERE.  The port stops on two branches it will not guess:
// apac::AudioSceneConfig (out of scope for CarPlay 5.1) and
// apac::LoudnessDRCConfig (stage h).  The generated cookie takes the first, so
// for it only the geometry is compared, plus the bit position at which the port
// stops - and that position is checkable, because the fixture shows the library
// accepts an AudioSceneConfig at exactly one offset in that neighbourhood.
// The live cookie has neither, so it is compared end to end: every field, and
// the exact number of bits the whole config consumes.

#include "apac/config.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct Expect {
    char     name[32];
    unsigned char cookie[512];
    size_t   cookieLen;
    long     payloadOffset, configOffset;
    long     capiSampleRate, capiFrameLength, capiNumChannels;
    long     rc, offset, configBytes;
    long     profile, profileLevel, sampleRate, frameLenIdx, numChannels;
    long     flag248, flag249;
    long     bitsBefore, bitsUsed;
    bool     valid;
};

int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool kv(const char* line, const char* key, long* out)
{
    size_t n = strlen(key);
    if (strncmp(line, key, n) || line[n] != '=') return false;
    *out = strtol(line + n + 1, 0, 10);
    return true;
}

void eq(const char* who, const char* what, long got, long want)
{
    if (got == want) return;
    fprintf(stderr, "\nMISMATCH %s: %s got %ld, library says %ld\n", who, what, got, want);
    exit(1);
}

int g_checked = 0;

void check(const Expect& e)
{
    assert(e.valid);

    // The trap, stated as an assertion rather than a comment.
    eq(e.name, "config offset is payload offset + 2", e.configOffset, e.payloadOffset + 2);
    eq(e.name, "reader start offset", e.offset, e.configOffset);
    eq(e.name, "config byte count", e.configBytes, (long)e.cookieLen - e.configOffset);

    apac::BitstreamReader bs(e.cookie + e.configOffset,
                             (uint32_t)(e.cookieLen - (size_t)e.configOffset));
    eq(e.name, "bits available", bs.BitsRemaining(), e.bitsBefore);

    apac::GlobalConfig gc;
    uint32_t rc = gc.Deserialize(bs);

    // Fields the library exposes, whatever branch the port stopped on.
    eq(e.name, "profile",      gc.profileLevel.profile, e.profile);
    eq(e.name, "profileLevel", gc.profileLevel.level,   e.profileLevel);
    eq(e.name, "sampleRate",   gc.sampleRate,           e.sampleRate);
    eq(e.name, "frameLenIdx",  gc.frameLengthIndex,     e.frameLenIdx);
    eq(e.name, "numChannels",  gc.numChannels,          e.numChannels);
    eq(e.name, "hasASCConfigData (+249)", gc.hasASCConfigData, e.flag249);

    // The C cookie parsers read the same three numbers straight out of the
    // 32-bit config word; they must agree with the parse.
    eq(e.name, "sampleRate vs APACDecoder_SampleRate",  gc.sampleRate,  e.capiSampleRate);
    eq(e.name, "numChannels vs APACDecoder_NumberOfChannels", gc.numChannels, e.capiNumChannels);
    eq(e.name, "frameLength vs APACDecoder_FrameLength", gc.frameLength, e.capiFrameLength);

    // The header's channel count must be what the element list actually adds up
    // to; that is the only cross-check on the codec-config body, and it is the
    // bit of the parse the library does not expose directly.
    assert(gc.numCodecConfigs == 1);
    eq(e.name, "channels from the element list", gc.codecConfigs[0].NumChannels(),
       e.numChannels);
    eq(e.name, "global channel layout", gc.layout.NumChannels(), e.numChannels);

    if (rc == apac::kOk) {
        // Nothing was skipped: demand the library's exact bit count and flag.
        eq(e.name, "return code", (long)rc, e.rc);
        eq(e.name, "deserialized (+248)", gc.deserialized, e.flag248);
        eq(e.name, "bits consumed", gc.bitsConsumed, e.bitsUsed);
    } else {
        // The port stopped on a branch it will not guess.  Only these two are
        // acceptable; anything else means the parse went wrong.
        if (rc != apac::kPortNeedsAudioScene && rc != apac::kPortNeedsLoudnessDRC) {
            fprintf(stderr, "\n%s: unexpected status %u (0x%x)\n", e.name, rc, rc);
            exit(1);
        }
        assert(gc.deserialized == 0);
        assert(gc.bitsConsumed > 0 && gc.bitsConsumed < e.bitsUsed);
    }
    ++g_checked;
}

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/globalconfig.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    Expect live, gen, cur;
    memset(&live, 0, sizeof live);
    memset(&gen, 0, sizeof gen);
    memset(&cur, 0, sizeof cur);
    bool have = false;
    bool isLive = false;

    // SCENE probe: offset -> (rc, endBit) for the generated cookie.
    long sceneOk = -1, sceneProbes = 0;

    char line[4096];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char name[64];
        if (sscanf(line, "COOKIE %63s", name) == 1 && strncmp(line, "COOKIEHEX", 9)) {
            if (have) { cur.valid = true; if (isLive) live = cur; else gen = cur; }
            memset(&cur, 0, sizeof cur);
            snprintf(cur.name, sizeof cur.name, "%s", name);
            isLive = !strcmp(name, "live");
            have = true;
            continue;
        }
        char hex[2048];
        if (sscanf(line, "COOKIEHEX %2047s", hex) == 1) {
            size_t n = strlen(hex) / 2;
            assert(n <= sizeof cur.cookie);
            for (size_t i = 0; i < n; ++i)
                cur.cookie[i] = (unsigned char)((hexval(hex[2 * i]) << 4) | hexval(hex[2 * i + 1]));
            cur.cookieLen = n;
            continue;
        }
        long at, rc, endBit;
        if (sscanf(line, "SCENE %ld %ld %ld", &at, &rc, &endBit) == 3) {
            ++sceneProbes;
            if (rc == 0) {
                assert(sceneOk == -1);   // exactly one accepted offset
                sceneOk = at;
            }
            continue;
        }

        const char* p = line;
        if (!strncmp(line, "CAPI ", 5)) {
            p += 5;
            if (kv(p, "payloadOffset", &cur.payloadOffset)) continue;
            if (kv(p, "configOffset", &cur.configOffset)) continue;
            if (kv(p, "sampleRate", &cur.capiSampleRate)) continue;
            if (kv(p, "frameLength", &cur.capiFrameLength)) continue;
            if (kv(p, "numChannels", &cur.capiNumChannels)) continue;
            continue;
        }
        if (kv(p, "rc", &cur.rc)) continue;
        if (kv(p, "offset", &cur.offset)) continue;
        if (kv(p, "configBytes", &cur.configBytes)) continue;
        if (kv(p, "profileLevel", &cur.profileLevel)) continue;
        if (kv(p, "profile", &cur.profile)) continue;
        if (kv(p, "sampleRate", &cur.sampleRate)) continue;
        if (kv(p, "frameLenIdx", &cur.frameLenIdx)) continue;
        if (kv(p, "numChannels", &cur.numChannels)) continue;
        if (kv(p, "flag248", &cur.flag248)) continue;
        if (kv(p, "flag249", &cur.flag249)) continue;
        if (kv(p, "bitsBefore", &cur.bitsBefore)) continue;
        if (kv(p, "bitsUsed", &cur.bitsUsed)) continue;
    }
    if (have) { cur.valid = true; if (isLive) live = cur; else gen = cur; }
    fclose(f);

    check(live);
    check(gen);
    assert(g_checked == 2);

    // The live CarPlay cookie: 48 kHz, 1024 frames, 6 channels, and the whole
    // config parsed to the last bit.
    {
        apac::BitstreamReader bs(live.cookie + live.configOffset,
                                 (uint32_t)(live.cookieLen - (size_t)live.configOffset));
        apac::GlobalConfig gc;
        assert(gc.Deserialize(bs) == apac::kOk);
        assert(gc.sampleRate == 48000);
        assert(gc.frameLength == 1024);
        assert(gc.numChannels == 6);
        assert(gc.deserialized == 1);
        assert(gc.bitsConsumed == 150);
        // kAudioChannelLayoutTag_MPEG_5_1_D = (124 << 16) | 6: C L R Ls Rs LFE,
        // the order reference/NOTES.md measured on the decoded audio.
        assert(gc.layout.tag == ((124u << 16) | 6u));
        assert(gc.codecConfigs[0].numElements == 4);
    }

    // The generated cookie stops at the audio-scene flag.  The fixture's SCENE
    // probe shows the library's own AudioSceneConfig::Deserialize succeeds at
    // exactly one offset in that neighbourhood, so landing anywhere else would
    // be a parse error rather than a matter of taste.
    {
        apac::BitstreamReader bs(gen.cookie + gen.configOffset,
                                 (uint32_t)(gen.cookieLen - (size_t)gen.configOffset));
        apac::GlobalConfig gc;
        assert(gc.Deserialize(bs) == apac::kPortNeedsAudioScene);
        assert(sceneProbes >= 5 && sceneOk >= 0);
        eq("generated", "bit position where the audio scene begins",
           gc.bitsConsumed, sceneOk);
        assert(gc.layout.tag == ((121u << 16) | 6u));   // MPEG_5_1_A
    }

    printf("OK  2 cookies field-exact, live cookie bit-exact (150 bits), "
           "scene offset pinned by %ld probes\n", sceneProbes);
    return 0;
}
