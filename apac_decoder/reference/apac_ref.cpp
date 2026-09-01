/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// apac_ref - native AArch64 reference harness for Apple's APAC decoder as
// linked into Pioneer's libSPF_CP_PlugInWrapper.so (SPHERA 1.030010).
//
// The library ships Apple's C entry points for the decoder with their names
// intact, so the C++ classes (APACDecoder, apac::GlobalConfig,
// TBitstreamReader) never have to be constructed by hand:
//
//   int32_t  APACDecoder_Create(void** handleOut);
//   int32_t  APACDecoder_Configure(void* handle, const AudioStreamDescription* out,
//                                  const void* cookie, uint16_t cookieBytes);
//   int32_t  APACDecoder_DecodeFrame(void* handle, const uint8_t* in, uint32_t inBytes,
//                                    void* out, uint32_t outCapacity, uint32_t* framesOut);
//   int32_t  APACDecoder_Reset(void* handle);
//   void     APACDecoder_Dispose(void* handle);
//
// plus five pure cookie parsers that take (cookieBytes, uint16_t offset):
//   APACDecoder_GetCookieOffset / GetVersion / GetProfile / SampleRate /
//   FrameLength / NumberOfChannels
//
// See NOTES.md for how each signature was recovered from the disassembly.
//
// Usage:
//   apac_ref --probe
//   apac_ref COOKIE OUT.pcm PACKET [PACKET ...]
//   apac_ref --dir PACKETDIR [--count N] COOKIE OUT.pcm
// Output is interleaved S16LE by default (--f32 for float32), in the decoder's
// own channel order - no remapping is applied.

#include <dlfcn.h>
#include <dirent.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace {

const char* kDefaultLib = "/opt/apac/lib/libSPF_CP_PlugInWrapper.so";

// Recovered layout of the descriptor APACDecoder_Configure() reads.  Only the
// four marked fields are touched by the library; the rest are carried for
// documentation and kept zero.  Field meanings were deduced from the checks in
// APACDecoder_Configure (see NOTES.md).
struct AudioStreamDescription {
    uint32_t sampleRate;        // +0   not read by Configure
    uint32_t formatID;          // +4   not read by Configure
    uint32_t formatFlags;       // +8   READ: 12 => S16 output, else float32
    uint32_t bytesPerFrame;     // +12  READ: must equal bytesPerSample*channels
    uint32_t framesPerPacket;   // +16  not read
    uint32_t bytesPerPacket;    // +20  not read
    uint32_t channelsPerFrame;  // +24  READ: must equal the stream's channel count
    uint32_t bitsPerChannel;    // +28  READ: stored in the decoder at +2104
    uint32_t pad[8];            // over-allocated; Configure never reads past +28
};

typedef int32_t (*fn_create_t)(void**);
typedef int32_t (*fn_configure_t)(void*, const void*, const void*, uint32_t);
typedef int32_t (*fn_decode_t)(void*, const uint8_t*, uint32_t, void*, uint32_t, uint32_t*);
typedef int32_t (*fn_reset_t)(void*);
typedef void    (*fn_dispose_t)(void*);
typedef uint32_t (*fn_cookie_u32_t)(const void*, uint32_t);

template <typename T>
T need(void* h, const char* name)
{
    void* p = dlsym(h, name);
    if (!p) {
        fprintf(stderr, "apac_ref: missing symbol %s\n", name);
        exit(2);
    }
    return reinterpret_cast<T>(p);
}

bool read_file(const char* path, std::vector<uint8_t>& out)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "apac_ref: %s: %s\n", path, strerror(errno));
        return false;
    }
    out.clear();
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        out.insert(out.end(), buf, buf + n);
    fclose(f);
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    const char* libPath = getenv("APAC_LIB") ? getenv("APAC_LIB") : kDefaultLib;
    bool wantFloat = false;
    const char* dirPath = 0;
    const char* listPath = 0;
    long limit = -1;
    uint32_t optChannels = 0, optFrameLength = 0, optRate = 0;  // 0 = take from cookie
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--f32") wantFloat = true;
        else if (a == "--dir" && i + 1 < argc) dirPath = argv[++i];
        else if (a == "--list" && i + 1 < argc) listPath = argv[++i];
        else if (a == "--count" && i + 1 < argc) limit = strtol(argv[++i], 0, 0);
        else if (a == "--lib" && i + 1 < argc) libPath = argv[++i];
        else if (a == "--channels" && i + 1 < argc) optChannels = (uint32_t)strtoul(argv[++i], 0, 0);
        else if (a == "--frame-length" && i + 1 < argc) optFrameLength = (uint32_t)strtoul(argv[++i], 0, 0);
        else if (a == "--rate" && i + 1 < argc) optRate = (uint32_t)strtoul(argv[++i], 0, 0);
        else if (a == "--probe") positional.push_back("--probe");
        else positional.push_back(a);
    }

    void* h = dlopen(libPath, RTLD_LAZY | RTLD_GLOBAL);
    if (!h) {
        fprintf(stderr, "apac_ref: dlopen(%s): %s\n", libPath, dlerror());
        return 1;
    }

    fn_create_t    APACDecoder_Create    = need<fn_create_t>(h, "APACDecoder_Create");
    fn_configure_t APACDecoder_Configure = need<fn_configure_t>(h, "APACDecoder_Configure");
    fn_decode_t    APACDecoder_DecodeFrame = need<fn_decode_t>(h, "APACDecoder_DecodeFrame");
    fn_reset_t     APACDecoder_Reset     = need<fn_reset_t>(h, "APACDecoder_Reset");
    fn_dispose_t   APACDecoder_Dispose   = need<fn_dispose_t>(h, "APACDecoder_Dispose");
    fn_cookie_u32_t GetCookieOffset = need<fn_cookie_u32_t>(h, "APACDecoder_GetCookieOffset");
    fn_cookie_u32_t GetVersion      = need<fn_cookie_u32_t>(h, "APACDecoder_GetVersion");
    fn_cookie_u32_t GetProfile      = need<fn_cookie_u32_t>(h, "APACDecoder_GetProfile");
    fn_cookie_u32_t SampleRate      = need<fn_cookie_u32_t>(h, "APACDecoder_SampleRate");
    fn_cookie_u32_t FrameLength     = need<fn_cookie_u32_t>(h, "APACDecoder_FrameLength");
    fn_cookie_u32_t NumberOfChannels = need<fn_cookie_u32_t>(h, "APACDecoder_NumberOfChannels");
    (void)APACDecoder_Reset;

    if (!positional.empty() && positional[0] == "--probe") {
        fprintf(stderr, "apac_ref: library loaded, all C entry points resolved\n");
        return 0;
    }
    if (positional.size() < 2) {
        fprintf(stderr,
            "usage: apac_ref [--f32] [--lib SO] COOKIE OUT.pcm PACKET...\n"
            "       apac_ref [--f32] --dir PACKETDIR [--count N] COOKIE OUT.pcm\n"
            "       apac_ref [--f32] --list FILE  [--count N] COOKIE OUT.pcm\n"
            "options: --channels N (6)  --frame-length N (1024)  --rate N (48000)\n");
        return 2;
    }

    std::vector<uint8_t> cookie;
    if (!read_file(positional[0].c_str(), cookie)) return 1;

    std::vector<std::string> packets;
    if (listPath) {
        FILE* lf = fopen(listPath, "r");
        if (!lf) { fprintf(stderr, "apac_ref: %s: %s\n", listPath, strerror(errno)); return 1; }
        char line[4096];
        while (fgets(line, sizeof line, lf)) {
            size_t n = strlen(line);
            while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
            if (n) packets.push_back(line);
        }
        fclose(lf);
        if (limit >= 0 && (long)packets.size() > limit) packets.resize(limit);
    } else if (dirPath) {
        DIR* d = opendir(dirPath);
        if (!d) { fprintf(stderr, "apac_ref: %s: %s\n", dirPath, strerror(errno)); return 1; }
        struct dirent* e;
        while ((e = readdir(d)) != 0) {
            if (e->d_name[0] == '.') continue;
            packets.push_back(std::string(dirPath) + "/" + e->d_name);
        }
        closedir(d);
        std::sort(packets.begin(), packets.end());
        if (limit >= 0 && (long)packets.size() > limit) packets.resize(limit);
    } else {
        for (size_t i = 2; i < positional.size(); ++i) packets.push_back(positional[i]);
    }
    if (packets.empty()) { fprintf(stderr, "apac_ref: no packets\n"); return 2; }

    // ---- cookie introspection (pure parsers, no decoder needed) -------------
    // GetCookieOffset returns the offset of the dapa payload, which BEGINS with
    // the 16-bit APAC sync word.  GlobalConfig starts two bytes later, so every
    // metadata helper must be called at off + 2.  Called on the sync word itself
    // they read 0x08007c41 instead of 0x7c41800c and report 96000 Hz / 32 ch /
    // frame length 0.  The helpers are correct; the offset is the trap.
    // docs/PIONEER_TYPE103_STATIC_SPEC.md documents the same +2 correction.
    uint32_t off = GetCookieOffset(&cookie[0], (uint32_t)cookie.size());
    uint32_t ver = GetVersion(&cookie[0], off);
    uint32_t cfg = off + 2;
    uint32_t prof = GetProfile(&cookie[0], cfg);
    uint32_t rate = SampleRate(&cookie[0], cfg);
    uint32_t flen = FrameLength(&cookie[0], cfg);
    uint32_t nch = NumberOfChannels(&cookie[0], cfg);
    fprintf(stderr,
        "apac_ref: cookie %zu bytes, payload offset %u, sync 0x%04x, config offset %u\n"
        "apac_ref: cookie says profile=%u rate=%u frameLength=%u channels=%u\n",
        cookie.size(), off, ver, cfg, prof, rate, flen, nch);
    // Command line still wins when given, so a malformed cookie can be overridden.
    if (optChannels) nch = optChannels;
    if (optFrameLength) flen = optFrameLength;
    if (optRate) rate = optRate;

    // ---- create + configure -------------------------------------------------
    void* dec = 0;
    int32_t rc = APACDecoder_Create(&dec);
    if (rc != 0 || !dec) { fprintf(stderr, "apac_ref: Create failed rc=%d\n", rc); return 1; }

    const uint32_t bytesPerSample = wantFloat ? 4u : 2u;
    AudioStreamDescription fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.sampleRate       = rate;
    fmt.formatID         = 0x6C70636D; // 'lpcm', informational only
    fmt.formatFlags      = wantFloat ? 9u : 12u; // 9 = float|packed, 12 = sint|packed
    fmt.bytesPerFrame    = bytesPerSample * nch;
    fmt.framesPerPacket  = 1;
    fmt.bytesPerPacket   = bytesPerSample * nch;
    fmt.channelsPerFrame = nch;
    fmt.bitsPerChannel   = bytesPerSample * 8;

    rc = APACDecoder_Configure(dec, &fmt, &cookie[0], (uint32_t)cookie.size());
    if (rc != 0) {
        fprintf(stderr, "apac_ref: Configure failed rc=%d (0x%08x)\n", rc, (unsigned)rc);
        APACDecoder_Dispose(dec);
        return 1;
    }
    fprintf(stderr, "apac_ref: configured, output %s %u ch\n",
        wantFloat ? "float32" : "s16le", nch);

    // ---- decode -------------------------------------------------------------
    FILE* out = fopen(positional[1].c_str(), "wb");
    if (!out) { fprintf(stderr, "apac_ref: %s: %s\n", positional[1].c_str(), strerror(errno)); return 1; }

    // Over-allocated: the error path inside DecodeFrame memsets
    // channels*frameLength*4 bytes regardless of the configured output width.
    std::vector<uint8_t> pcm(nch * flen * 4 + 4096, 0);
    std::vector<uint8_t> pkt;
    uint64_t totalFrames = 0;
    int bad = 0;

    for (size_t i = 0; i < packets.size(); ++i) {
        if (!read_file(packets[i].c_str(), pkt)) { bad = 1; break; }
        uint32_t frames = 0;
        rc = APACDecoder_DecodeFrame(dec, pkt.empty() ? 0 : &pkt[0], (uint32_t)pkt.size(),
                                     &pcm[0], (uint32_t)pcm.size(), &frames);
        if (rc != 0) {
            fprintf(stderr, "apac_ref: packet %zu (%s, %zu bytes): DecodeFrame rc=%d\n",
                i, packets[i].c_str(), pkt.size(), rc);
            bad = 1;
            break;
        }
        if (frames != flen)
            fprintf(stderr, "apac_ref: packet %zu produced %u frames (expected %u)\n",
                i, frames, flen);
        size_t n = (size_t)frames * nch * bytesPerSample;
        if (fwrite(&pcm[0], 1, n, out) != n) { fprintf(stderr, "apac_ref: write failed\n"); bad = 1; break; }
        totalFrames += frames;
    }

    fclose(out);
    APACDecoder_Dispose(dec);
    fprintf(stderr, "apac_ref: %zu packets, %llu frames, %u channels -> %s\n",
        packets.size(), (unsigned long long)totalFrames, nch, positional[1].c_str());
    return bad;
}
