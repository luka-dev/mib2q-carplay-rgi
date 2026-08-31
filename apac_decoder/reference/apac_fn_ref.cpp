// apac_fn_ref - PER-FUNCTION reference harness for Apple's APAC decoder as
// linked into Pioneer's libSPF_CP_PlugInWrapper.so (SPHERA 1.030010).
//
// apac_ref.cpp drives the whole decoder through Apple's C API.  This is its
// sibling: it reaches past that API and calls INDIVIDUAL C++ member functions
// out of the library by their Itanium-mangled names, so a hand port can be
// diffed against the original one function at a time.
//
// Everything about loading the library - the arm64 container, the libMali /
// OpenSSL-3 stub, the apluser group - is the same as apac_ref and is set up by
// run.sh and Dockerfile.  See NOTES.md.
//
// Symbols used (addresses are in docs/reference/apac_symbols_pioneer.txt):
//
//   0xb1430  _ZN16TBitstreamReaderIjEC1EPKhj          ctor(const uint8*, uint32)
//   0xb14e0  _ZN16TBitstreamReaderIjE7GetBitsEj        GetBits(uint32)
//   0xb15b0  _ZNK16TBitstreamReaderIjE13BitsRemainingEv
//   0xb1600  _ZN16TBitstreamReaderIjE13FillCacheFromEPKh
//   0xb16b0  _ZN16TBitstreamReaderIjE6GetBitEv
//   0xb1760  _ZN16TBitstreamReaderIjE8SkipBitsEj
//   0xb1850  _ZN16TBitstreamReaderIjE15AlignToNextByteEv
//   0xb1910  _ZN16TBitstreamReaderIjE7PutBackEj
//   0x97d50  _ZN4apac12GlobalConfig11DeserializeER16TBitstreamReaderIjE
//   0x978c0  _ZN4apac12GlobalConfig5ResetEv
//   0x9d9d0  _ZN4apac17GetSFBOffsetTableEjj
//
// Usage:
//   apac_fn_ref probe                       resolve every symbol, print nothing else
//   apac_fn_ref bs      < cases > results    bit-reader differential, see below
//   apac_fn_ref ics     < cases > results    APACICSInfo::Deserialize, per case
//   apac_fn_ref sfb                          dump apac::GetSFBOffsetTable for all rates
//   apac_fn_ref gc COOKIE [--offset N]       apac::GlobalConfig::Deserialize on a cookie
//   apac_fn_ref gc-raw FILE [--offset N]     ... on a raw config blob (no atom walk)
//   apac_fn_ref cookie  COOKIE               the pure C cookie parsers
//   apac_fn_ref scene   COOKIE --at BITS     apac::AudioSceneConfig::Deserialize
//                                            at a given bit position, so a port
//                                            that stops there can prove it is
//                                            standing in the right place
//
// `bs` reads a case file on stdin and writes the same file back with a result
// line appended after each case:
//
//   CASE <idx> <align> <bufhex> <op,op,...>
//   R <opidx> <op> <ret> <curoff> <cache> <bitsleft>
//
// ops: gN=GetBits(N)  b=GetBit  sN=SkipBits(N)  a=AlignToNextByte
//      pN=PutBack(N)  r=BitsRemaining  fc=FillCacheFrom(cur)
// <align> is the byte misalignment the buffer is presented at, 0..3, so the
// constructor's pre-alignment path is exercised.  <curoff> is signed and
// relative to the buffer base: PutBack can legitimately move it negative.

#include <dlfcn.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

const char* kDefaultLib = "/opt/apac/lib/libSPF_CP_PlugInWrapper.so";

// 24 bytes; see CLASSES.md section 1.  Constructed by the library, never by us.
struct BsState {
    const uint8_t* cur;
    const uint8_t* end;
    uint32_t       cache;
    int32_t        bitsLeft;
};

// 40-byte record returned by apac::GetSFBOffsetTable; RODATA.md section 2.
struct SFBOffsetRecord {
    uint64_t        sampleRate;
    uint64_t        numSfbLong;
    const uint16_t* sfbOffsetLong;
    uint64_t        numSfbShort;
    const uint16_t* sfbOffsetShort;
};

typedef void     (*fn_bs_ctor_t)(void*, const uint8_t*, uint32_t);
typedef uint32_t (*fn_bs_getbits_t)(void*, uint32_t);
typedef uint32_t (*fn_bs_getbit_t)(void*);
typedef void     (*fn_bs_skip_t)(void*, uint32_t);
typedef void     (*fn_bs_align_t)(void*);
typedef void     (*fn_bs_putback_t)(void*, uint32_t);
typedef int32_t  (*fn_bs_remaining_t)(const void*);
typedef void     (*fn_bs_fill_t)(void*, const uint8_t*);
typedef int32_t  (*fn_gc_deser_t)(void*, void*);
typedef int32_t  (*fn_scene_deser_t)(void*, void*);
typedef uint8_t  (*fn_ics_deser_t)(void*, void*, int32_t);
typedef void     (*fn_gc_reset_t)(void*);
typedef const SFBOffsetRecord* (*fn_sfb_t)(uint32_t, uint32_t);
typedef uint32_t (*fn_cookie_u32_t)(const void*, uint32_t);

void*             g_lib = 0;
fn_bs_ctor_t      bs_ctor;
fn_bs_getbits_t   bs_getbits;
fn_bs_getbit_t    bs_getbit;
fn_bs_skip_t      bs_skip;
fn_bs_align_t     bs_align;
fn_bs_putback_t   bs_putback;
fn_bs_remaining_t bs_remaining;
fn_bs_fill_t      bs_fill;
fn_gc_deser_t     gc_deserialize;
fn_scene_deser_t  scene_deserialize;
fn_ics_deser_t    ics_deserialize;
fn_gc_reset_t     gc_reset;
fn_sfb_t          sfb_table;
void*             gc_vt_asc;
void*             gc_vt_codec;

template <typename T>
T need(const char* name)
{
    void* p = dlsym(g_lib, name);
    if (!p) {
        fprintf(stderr, "apac_fn_ref: missing symbol %s\n", name);
        exit(2);
    }
    return reinterpret_cast<T>(p);
}

void load_library()
{
    const char* libPath = getenv("APAC_LIB") ? getenv("APAC_LIB") : kDefaultLib;
    g_lib = dlopen(libPath, RTLD_LAZY | RTLD_GLOBAL);
    if (!g_lib) {
        fprintf(stderr, "apac_fn_ref: dlopen(%s): %s\n", libPath, dlerror());
        exit(1);
    }
    bs_ctor      = need<fn_bs_ctor_t>     ("_ZN16TBitstreamReaderIjEC1EPKhj");
    bs_getbits   = need<fn_bs_getbits_t>  ("_ZN16TBitstreamReaderIjE7GetBitsEj");
    bs_getbit    = need<fn_bs_getbit_t>   ("_ZN16TBitstreamReaderIjE6GetBitEv");
    bs_skip      = need<fn_bs_skip_t>     ("_ZN16TBitstreamReaderIjE8SkipBitsEj");
    bs_align     = need<fn_bs_align_t>    ("_ZN16TBitstreamReaderIjE15AlignToNextByteEv");
    bs_putback   = need<fn_bs_putback_t>  ("_ZN16TBitstreamReaderIjE7PutBackEj");
    bs_remaining = need<fn_bs_remaining_t>("_ZNK16TBitstreamReaderIjE13BitsRemainingEv");
    bs_fill      = need<fn_bs_fill_t>     ("_ZN16TBitstreamReaderIjE13FillCacheFromEPKh");
    gc_deserialize = need<fn_gc_deser_t>  ("_ZN4apac12GlobalConfig11DeserializeER16TBitstreamReaderIjE");
    gc_reset     = need<fn_gc_reset_t>    ("_ZN4apac12GlobalConfig5ResetEv");
    scene_deserialize = need<fn_scene_deser_t>(
        "_ZN4apac16AudioSceneConfig11DeserializeER16TBitstreamReaderIjE");
    sfb_table    = need<fn_sfb_t>         ("_ZN4apac17GetSFBOffsetTableEjj");
    ics_deserialize = need<fn_ics_deser_t>(
        "_ZN11APACICSInfo11DeserializeER16TBitstreamReaderIjEN4apac10CodingTypeE");
    gc_vt_asc    = need<void*>            ("_ZTVN4apac12GlobalConfig13ASCConfigDataE");
    gc_vt_codec  = need<void*>            ("_ZTVN4apac12GlobalConfig15CodecConfigDataE");
}

bool read_file(const char* path, std::vector<uint8_t>& out)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "apac_fn_ref: %s: %s\n", path, strerror(errno));
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

int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parse_hex(const std::string& s, std::vector<uint8_t>& out)
{
    out.clear();
    if (s == "-") return true;      // an empty buffer, spelled so sscanf sees a field
    if (s.size() & 1) return false;
    for (size_t i = 0; i < s.size(); i += 2) {
        int hi = hexval(s[i]), lo = hexval(s[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back((uint8_t)((hi << 4) | lo));
    }
    return true;
}

// ------------------------------------------------------------------ bs ----
// Cases come in on stdin so a run needs no shell quoting of long op strings.
int cmd_bs()
{
    char line[65536];
    // The reader is handed a pointer inside a generously padded, 4-aligned
    // slab: PutBack legitimately walks `cur` backwards past the start and
    // FillCacheFrom reads up to 4 bytes at `cur`, so both ends need slack.
    static uint8_t slab[65536] __attribute__((aligned(64)));
    const size_t kLead = 256;

    while (fgets(line, sizeof line, stdin)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (strncmp(line, "CASE ", 5) != 0) {
            // comments and blank lines are copied through verbatim
            printf("%s\n", line);
            continue;
        }
        printf("%s\n", line);

        char idxs[64], bufhex[40000], ops[40000];
        unsigned align = 0;
        if (sscanf(line, "CASE %63s %u %39999s %39999s", idxs, &align, bufhex, ops) != 4) {
            fprintf(stderr, "apac_fn_ref: malformed CASE: %s\n", line);
            return 2;
        }
        std::vector<uint8_t> buf;
        if (!parse_hex(bufhex, buf)) {
            fprintf(stderr, "apac_fn_ref: bad hex in %s\n", idxs);
            return 2;
        }
        memset(slab, 0, sizeof slab);
        uint8_t* base = slab + kLead + (align & 3);
        if (!buf.empty()) memcpy(base, &buf[0], buf.size());

        BsState bs;
        memset(&bs, 0, sizeof bs);
        bs_ctor(&bs, base, (uint32_t)buf.size());
        printf("R 0 ctor 0 %ld %08x %d\n",
               (long)(bs.cur - base), bs.cache, bs.bitsLeft);

        int opIdx = 1;
        char* save = 0;
        for (char* tok = strtok_r(ops, ",", &save); tok; tok = strtok_r(0, ",", &save)) {
            long ret = 0;
            switch (tok[0]) {
            case 'g': ret = (long)(uint32_t)bs_getbits(&bs, (uint32_t)strtoul(tok + 1, 0, 10)); break;
            case 'b': ret = (long)(uint32_t)bs_getbit(&bs); break;
            case 's': bs_skip(&bs, (uint32_t)strtoul(tok + 1, 0, 10)); break;
            case 'a': bs_align(&bs); break;
            case 'p': bs_putback(&bs, (uint32_t)strtoul(tok + 1, 0, 10)); break;
            case 'r': ret = (long)bs_remaining(&bs); break;
            case 'f': bs_fill(&bs, bs.cur); break;
            default:
                fprintf(stderr, "apac_fn_ref: unknown op '%s'\n", tok);
                return 2;
            }
            printf("R %d %s %ld %ld %08x %d\n", opIdx++, tok, ret,
                   (long)(bs.cur - base), bs.cache, bs.bitsLeft);
        }
        fflush(stdout);
    }
    return 0;
}

// ----------------------------------------------------------------- ics ----
// APACICSInfo is 768 bytes but Deserialize only reads +8 and +9 - the maximum
// scalefactor-band counts for long and short blocks, which the element sets from
// the SFB table - and only writes +10 .. +16.  A zeroed slab with those two
// bytes filled in is therefore a legal object.
//
//   ICS <idx> <maxSfbLong> <maxSfbShort> <codingType> <bufferHex|->
//   R <idx> <rc> <windowSequence> <maxSfb> <numWindowGroups> <groupWord> <bitsUsed>
int cmd_ics()
{
    char line[8192];
    static uint8_t slab[4096] __attribute__((aligned(64)));
    static uint8_t obj[1024] __attribute__((aligned(16)));

    while (fgets(line, sizeof line, stdin)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (strncmp(line, "ICS ", 4) != 0) { printf("%s\n", line); continue; }
        printf("%s\n", line);

        char idxs[64], bufhex[4096];
        unsigned maxLong, maxShort;
        int coding;
        if (sscanf(line, "ICS %63s %u %u %d %4095s",
                   idxs, &maxLong, &maxShort, &coding, bufhex) != 5) {
            fprintf(stderr, "apac_fn_ref: malformed ICS: %s\n", line);
            return 2;
        }
        std::vector<uint8_t> buf;
        if (!parse_hex(bufhex, buf)) { fprintf(stderr, "apac_fn_ref: bad hex\n"); return 2; }
        memset(slab, 0, sizeof slab);
        if (!buf.empty()) memcpy(slab, &buf[0], buf.size());
        memset(obj, 0, sizeof obj);
        obj[8] = (uint8_t)maxLong;
        obj[9] = (uint8_t)maxShort;

        BsState bs;
        memset(&bs, 0, sizeof bs);
        bs_ctor(&bs, slab, (uint32_t)buf.size());
        int32_t before = bs_remaining(&bs);
        uint8_t rc = ics_deserialize(obj, &bs, coding);
        int32_t after = bs_remaining(&bs);

        uint32_t groupWord;
        memcpy(&groupWord, obj + 16, 4);
        printf("R %s %u %u %u %u %u %d\n", idxs, (unsigned)(rc & 1),
               (unsigned)obj[10], (unsigned)obj[11], (unsigned)obj[12],
               groupWord, before - after);
        fflush(stdout);
    }
    return 0;
}

// ----------------------------------------------------------------- sfb ----
void dump_sfb(uint32_t rate, uint32_t frameLength)
{
    const SFBOffsetRecord* r = sfb_table(rate, frameLength);
    if (!r) {
        printf("SFB %u %u NULL\n", rate, frameLength);
        return;
    }
    printf("SFB %u %u %llu %llu\n", rate, frameLength,
           (unsigned long long)r->numSfbLong, (unsigned long long)r->numSfbShort);
    printf("LONG");
    for (uint64_t i = 0; i <= r->numSfbLong; ++i) printf(" %u", r->sfbOffsetLong[i]);
    printf("\n");
    printf("SHORT");
    for (uint64_t i = 0; i <= r->numSfbShort; ++i) printf(" %u", r->sfbOffsetShort[i]);
    printf("\n");
}

int cmd_sfb(int argc, char** argv)
{
    // The 13 rates in the decoder's own sample-rate table at 0x3483a8.  8000 is
    // the last one GetSFBOffsetTable knows; 7350 is in the rate table but not in
    // the 12 SFB records, and must come back NULL.
    static const uint32_t kRates[] = {
        96000, 88200, 64000, 48000, 44100, 32000,
        24000, 22050, 16000, 12000, 11025, 8000, 7350
    };
    if (argc > 0) {
        for (int i = 0; i < argc; ++i)
            dump_sfb((uint32_t)strtoul(argv[i], 0, 0), 1024);
        return 0;
    }
    for (size_t i = 0; i < sizeof kRates / sizeof kRates[0]; ++i)
        dump_sfb(kRates[i], 1024);
    // frameLength is a hard gate: anything but 1024 returns NULL.
    dump_sfb(48000, 128);
    dump_sfb(48000, 2048);
    return 0;
}

// ------------------------------------------------------------------ gc ----
// apac::GlobalConfig has no exported constructor.  Deserialize's first act is
// Reset(), which walks the two unique_ptr vectors at +176 and +200 and frees
// them - on zeroed memory begin == end == NULL and both loops are skipped, so a
// zeroed slab is a legal empty object.  The two things zero is NOT good enough
// for are the embedded CodecConfigData / ASCConfigData sub-objects: each is
// {vptr, GlobalConfig*} at +0 and +16, and Deserialize dereferences the back
// pointer at +24 near the end (`v137 = *((_QWORD *)this + 3)`).
const size_t kGlobalConfigSlab = 4096;   // 0x110 observed; over-allocated

void* make_global_config()
{
    void* p = 0;
    if (posix_memalign(&p, 64, kGlobalConfigSlab) != 0 || !p) {
        fprintf(stderr, "apac_fn_ref: out of memory\n");
        exit(1);
    }
    memset(p, 0, kGlobalConfigSlab);
    char* o = (char*)p;
    // A vtable pointer points at slot 2 of the vtable object (past
    // offset-to-top and typeinfo).
    *(void**)(o +  0) = (char*)gc_vt_codec + 16;
    *(void**)(o +  8) = o;
    *(void**)(o + 16) = (char*)gc_vt_asc + 16;
    *(void**)(o + 24) = o;
    return p;
}

int run_gc(const std::vector<uint8_t>& blob, uint32_t offset, bool dump)
{
    if (offset > blob.size()) {
        fprintf(stderr, "apac_fn_ref: offset %u past end of %zu-byte blob\n",
                offset, blob.size());
        return 2;
    }
    // Give the reader its own 4-aligned copy: the config's alignment inside the
    // cookie is not the alignment the decoder sees, and the ctor's behaviour
    // depends on it.
    std::vector<uint8_t> cfg(blob.begin() + offset, blob.end());
    void* gc = make_global_config();
    BsState bs;
    memset(&bs, 0, sizeof bs);
    bs_ctor(&bs, cfg.empty() ? (const uint8_t*)"" : &cfg[0], (uint32_t)cfg.size());
    const uint8_t* base = bs.end - cfg.size();
    int32_t before = bs_remaining(&bs);
    int32_t rc = gc_deserialize(gc, &bs);
    int32_t after = bs_remaining(&bs);

    const char* o = (const char*)gc;
    printf("rc=%d\n", rc);
    printf("offset=%u\n", offset);
    printf("configBytes=%zu\n", cfg.size());
    printf("profile=%u\n",      (unsigned)*(const uint16_t*)(o + 32));
    printf("profileLevel=%u\n", (unsigned)*(const uint8_t*) (o + 34));
    printf("sampleRate=%u\n",   (unsigned)*(const uint32_t*)(o + 36));
    printf("frameLenIdx=%u\n",  (unsigned)*(const uint8_t*) (o + 41));
    printf("numChannels=%u\n",  (unsigned)*(const uint8_t*) (o + 43));
    printf("flag248=%u\n",      (unsigned)*(const uint8_t*) (o + 248));
    printf("flag249=%u\n",      (unsigned)*(const uint8_t*) (o + 249));
    printf("bitsBefore=%d\n", before);
    printf("bitsAfter=%d\n", after);
    printf("bitsUsed=%d\n", before - after);
    printf("bsCurOff=%ld\n", (long)(bs.cur - base));
    printf("bsCache=%08x\n", bs.cache);
    printf("bsBitsLeft=%d\n", bs.bitsLeft);
    if (dump) {
        printf("obj=");
        for (size_t i = 0; i < 272; ++i) printf("%02x", (unsigned char)o[i]);
        printf("\n");
    }
    free(gc);
    return 0;
}

// ----------------------------------------------------------- scene ----
// apac::AudioSceneConfig is an 8-byte object - one owning pointer to an
// SC_AudioScenes, which its Deserialize allocates - so unlike GlobalConfig it
// needs nothing but zeroed memory.  GlobalConfig::Deserialize reaches it through
// AncillaryConfig; running it here at an explicit bit offset is how a port that
// stops short of the audio-scene branch proves it stopped in the right place.
int run_scene(const std::vector<uint8_t>& blob, uint32_t offset, uint32_t atBit)
{
    if (offset > blob.size()) {
        fprintf(stderr, "apac_fn_ref: offset %u past end\n", offset);
        return 2;
    }
    std::vector<uint8_t> cfg(blob.begin() + offset, blob.end());
    uint64_t obj[4];
    memset(obj, 0, sizeof obj);
    BsState bs;
    memset(&bs, 0, sizeof bs);
    bs_ctor(&bs, cfg.empty() ? (const uint8_t*)"" : &cfg[0], (uint32_t)cfg.size());
    int32_t total = bs_remaining(&bs);
    bs_skip(&bs, atBit);
    int32_t rc = scene_deserialize(obj, &bs);
    int32_t after = bs_remaining(&bs);
    printf("sceneAtBit=%u\n", atBit);
    printf("sceneRc=%d\n", rc);
    printf("sceneEndBit=%d\n", total - after);
    printf("configBits=%d\n", total);
    return 0;
}

// ------------------------------------------------------------- cookie ----
int cmd_cookie(const std::vector<uint8_t>& cookie)
{
    fn_cookie_u32_t GetCookieOffset  = need<fn_cookie_u32_t>("APACDecoder_GetCookieOffset");
    fn_cookie_u32_t GetVersion       = need<fn_cookie_u32_t>("APACDecoder_GetVersion");
    fn_cookie_u32_t GetProfile       = need<fn_cookie_u32_t>("APACDecoder_GetProfile");
    fn_cookie_u32_t SampleRate       = need<fn_cookie_u32_t>("APACDecoder_SampleRate");
    fn_cookie_u32_t FrameLength      = need<fn_cookie_u32_t>("APACDecoder_FrameLength");
    fn_cookie_u32_t NumberOfChannels = need<fn_cookie_u32_t>("APACDecoder_NumberOfChannels");
    const void* c = &cookie[0];
    uint32_t off = GetCookieOffset(c, (uint32_t)cookie.size());
    // The dapa payload starts with the 16-bit 0x0800 sync word; GlobalConfig is
    // two bytes further on.  NOTES.md, "Magic-cookie layout".
    uint32_t cfg = off + 2;
    printf("cookieBytes=%zu\n", cookie.size());
    printf("payloadOffset=%u\n", off);
    printf("configOffset=%u\n", cfg);
    printf("sync=%u\n", GetVersion(c, off));
    printf("profileAtCfg=%u\n", GetProfile(c, cfg));
    printf("sampleRate=%u\n", SampleRate(c, cfg));
    printf("frameLength=%u\n", FrameLength(c, cfg));
    printf("numChannels=%u\n", NumberOfChannels(c, cfg));
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr,
            "usage: apac_fn_ref probe\n"
            "       apac_fn_ref bs            < cases.txt\n"
            "       apac_fn_ref sfb [RATE...]\n"
            "       apac_fn_ref gc COOKIE  [--offset N] [--dump]\n"
            "       apac_fn_ref gc-raw FILE [--offset N] [--dump]\n"
            "       apac_fn_ref cookie COOKIE\n");
        return 2;
    }
    load_library();
    std::string cmd = argv[1];

    if (cmd == "probe") {
        fprintf(stderr, "apac_fn_ref: library loaded, all symbols resolved\n");
        return 0;
    }
    if (cmd == "bs")  return cmd_bs();
    if (cmd == "ics") return cmd_ics();
    if (cmd == "sfb") return cmd_sfb(argc - 2, argv + 2);

    if (cmd == "gc" || cmd == "gc-raw" || cmd == "cookie" || cmd == "scene") {
        if (argc < 3) { fprintf(stderr, "apac_fn_ref: %s needs a file\n", cmd.c_str()); return 2; }
        std::vector<uint8_t> blob;
        if (!read_file(argv[2], blob)) return 1;
        if (blob.empty()) { fprintf(stderr, "apac_fn_ref: %s is empty\n", argv[2]); return 1; }
        bool dump = false;
        long offset = -1, atBit = -1;
        for (int i = 3; i < argc; ++i) {
            if (!strcmp(argv[i], "--dump")) dump = true;
            else if (!strcmp(argv[i], "--offset") && i + 1 < argc) offset = strtol(argv[++i], 0, 0);
            else if (!strcmp(argv[i], "--at") && i + 1 < argc) atBit = strtol(argv[++i], 0, 0);
        }
        if (cmd == "cookie") return cmd_cookie(blob);
        if (offset < 0) {
            if (cmd == "gc-raw") {
                offset = 0;
            } else {
                fn_cookie_u32_t GetCookieOffset =
                    need<fn_cookie_u32_t>("APACDecoder_GetCookieOffset");
                offset = (long)GetCookieOffset(&blob[0], (uint32_t)blob.size()) + 2;
            }
        }
        if (cmd == "scene") {
            if (atBit < 0) { fprintf(stderr, "apac_fn_ref: scene needs --at BITS\n"); return 2; }
            return run_scene(blob, (uint32_t)offset, (uint32_t)atBit);
        }
        return run_gc(blob, (uint32_t)offset, dump);
    }

    fprintf(stderr, "apac_fn_ref: unknown command '%s'\n", cmd.c_str());
    return 2;
}
