/* cinemo_probe -- can this box create a Cinemo AAC-LC decoder at all?
 *
 * dio_manager took SIGABRT the first time the buffered AAC path asked for a decoder, and its
 * core shows libNmeAudioAAC was never mapped: the abort landed before the plugin loaded.  That
 * leaves two very different faults wearing the same crash, and they want opposite fixes:
 *
 *   the contract is wrong          -> creating a codec aborts anywhere, including here
 *   dio_manager's context is wrong -> creating a codec works here, and the fault is init order
 *
 * This runs the exact sequence hook/audio/aac_bridge.c runs, in a process of its own, and
 * prints how far it gets.  It touches no audio device, so it is safe with the car playing.
 *
 * Every step prints BEFORE it is attempted: if the process aborts, the last line printed names
 * the call that killed it.  stdout is unbuffered for the same reason.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>

#define CMT_SIZE            0x80
#define CMT_MEDIA_TYPE      0x00
#define CMT_SUBTYPE         0x04
#define CMT_SAMPLE_FORMAT   0x08
#define CMT_SAMPLE_RATE     0x0c
#define CMT_CHANNELS        0x10
#define CMT_HEADER_TYPE     0x74
#define CMT_HEADER_DATA     0x78
#define CMT_HEADER_SIZE     0x7c

#define CMT_MEDIA_AUDIO_ES  5
#define CMT_MEDIA_PCM       2
#define CMT_SUB_AAC_LC      0x101a
#define CMT_SUB_PCM         0x1023
#define CMT_FMT_S16         2
#define CMT_HDR_ASC         5

typedef int32_t (*cinemo_create_t)(void **);
typedef int32_t (*cinemo_init_t)(void *, void *, void *, const void *, const void *);
typedef int32_t (*cinemo_simple_t)(void *);
typedef const char *(*cinemo_err_t)(int32_t);
typedef int32_t (*cinemo_cfg_t)(void **);

static void set32(uint8_t *m, size_t off, uint32_t v) { memcpy(m + off, &v, 4); }
static void setp(uint8_t *m, size_t off, const void *p) { memcpy(m + off, &p, sizeof(p)); }

/* The callback must exist and must not be reached during creation.  If it fires here, the
 * decoder produced audio from nothing, which is worth knowing. */
static int32_t decoded_cb(void *ctx, const void *desc, const void *data, uint32_t bytes)
{
    (void)ctx; (void)desc; (void)data;
    printf("  !! decode callback fired during init, %u bytes\n", bytes);
    return 0;
}

int main(int argc, char **argv)
{
    /* The ASC for index 23: AAC-LC, 48 kHz, stereo.  Taken from the captured magic cookie. */
    /* Release aborted with "free malloc object that is not allocated" while the ASC lived in
     * a stack array: the codec takes ownership of the header pointer and frees it.  Hand it
     * heap memory it may keep.  PROBE_STACK_ASC reproduces the abort on demand. */
    static uint8_t ascStack[2] = { 0x11, 0x90 };
    uint8_t *asc;
    uint8_t input[CMT_SIZE], output[CMT_SIZE];
    cinemo_create_t create;
    cinemo_init_t   initialize;
    cinemo_simple_t release;
    cinemo_err_t    errstr;
    void *sdk, *codec = 0;
    int32_t rc;

    setvbuf(stdout, 0, _IONBF, 0);
    if (getenv("PROBE_STACK_ASC")) { asc = ascStack; }
    else { asc = (uint8_t *)malloc(2); asc[0] = 0x11; asc[1] = 0x90; }
    if (argc > 2) { asc[0] = (uint8_t)strtoul(argv[1], 0, 16); asc[1] = (uint8_t)strtoul(argv[2], 0, 16); }
    printf("cinemo_probe: ASC %02x %02x\n", asc[0], asc[1]);

    /* dio_manager gets libNmeSDK through DT_NEEDED; standing alone we must ask for it. */
    printf("step 1: dlopen libNmeSDK.so\n");
    sdk = dlopen("libNmeSDK.so", RTLD_NOW | RTLD_GLOBAL);
    if (!sdk) { printf("  FAIL: %s\n", dlerror()); return 1; }
    printf("  ok: %p\n", sdk);

    printf("step 2: resolve the codec entry points\n");
    create     = (cinemo_create_t)dlsym(sdk, "CinemoCreateAudioCodec");
    initialize = (cinemo_init_t)  dlsym(sdk, "ICinemoAudioCodec_Initialize");
    release    = (cinemo_simple_t)dlsym(sdk, "ICinemoAudioCodec_Release");
    errstr     = (cinemo_err_t)   dlsym(sdk, "CinemoErrorToString");
    printf("  create=%p initialize=%p release=%p errstr=%p\n",
           (void *)create, (void *)initialize, (void *)release, (void *)errstr);
    if (!create || !initialize || !release) { printf("  FAIL: missing entry point\n"); return 1; }

    /* The factory looks the decoder up by subtype and then instantiates it.  Error 23 arrives
     * after a successful lookup, so the class is known but the object could not be produced --
     * which is what an unloaded plugin .so looks like.  Load it by hand and see. */
    if (getenv("PROBE_PLUGIN")) {
        const char *pp = getenv("PROBE_PLUGIN");
        void *plug;
        printf("step 1b: dlopen %s\n", pp);
        plug = dlopen(pp, RTLD_NOW | RTLD_GLOBAL);
        printf("  %s\n", plug ? "ok" : dlerror());
    }

    /* CinemoErrorResources on the first run pointed here: the SDK finds its codec plugins
     * through a Config object, and standing alone we had never made one.  If creating a Config
     * is what unlocks the plugin folders, Initialize stops failing on the next run. */
    if (getenv("PROBE_CONFIG")) {
        cinemo_cfg_t mkcfg = (cinemo_cfg_t)dlsym(sdk, "CinemoCreateConfig");
        void *cfg = 0;
        printf("step 2b: CinemoCreateConfig (PROBE_CONFIG set)\n");
        if (!mkcfg) { printf("  no such export\n"); }
        else {
            rc = mkcfg(&cfg);
            printf("  returned rc=%d cfg=%p%s%s\n", (int)rc, cfg,
                   (rc && errstr) ? " " : "", (rc && errstr) ? errstr(rc) : "");
            /* The factory answers 23 (Interface) while it knows no class implementing
             * INmeAudioCodec.  Its class list comes from the plugin directory, and the SDK
             * exports a setter for exactly that -- there is even a SetPluginDirectory(%s)
             * log line in .rodata.  Point it at the folder holding cinemo_classes.xml. */
            if (cfg) {
                typedef int32_t (*cinemo_setdir_t)(void *, const char *);
                cinemo_setdir_t setdir =
                    (cinemo_setdir_t)dlsym(sdk, "ICinemoConfig_SetPluginDirectory");
                const char *dir = getenv("PROBE_PLUGIN_DIR");
                if (!dir) dir = "/mnt/app/armle/usr/lib/cinemo";
                printf("step 2c: ICinemoConfig_SetPluginDirectory(\"%s\")\n", dir);
                if (!setdir) printf("  no such export\n");
                else {
                    rc = setdir(cfg, dir);
                    printf("  returned rc=%d%s%s\n", (int)rc,
                           (rc && errstr) ? " " : "", (rc && errstr) ? errstr(rc) : "");
                }
            }
        }
    } else {
        printf("step 2b: skipped (set PROBE_CONFIG=1 to create a Config first)\n");
    }

    /* This is the call the core says we never returned from.  If the abort is in the contract
     * rather than in dio_manager, the process dies on the next line. */
    printf("step 3: CinemoCreateAudioCodec -- if this aborts, the fault is not dio_manager\n");
    rc = create(&codec);
    printf("  returned rc=%d codec=%p%s%s\n", (int)rc, codec,
           (rc && errstr) ? " " : "", (rc && errstr) ? errstr(rc) : "");
    if (rc != 0 || !codec) { printf("  FAIL: no codec handle\n"); return 1; }

    printf("step 4: build the media descriptors\n");
    memset(input, 0, sizeof(input));
    memset(output, 0, sizeof(output));
    set32(input, CMT_MEDIA_TYPE,  CMT_MEDIA_AUDIO_ES);
    set32(input, CMT_SUBTYPE,     CMT_SUB_AAC_LC);
    set32(input, CMT_HEADER_TYPE, CMT_HDR_ASC);
    setp (input, CMT_HEADER_DATA, asc);
    set32(input, CMT_HEADER_SIZE, 2);
    set32(output, CMT_MEDIA_TYPE,    CMT_MEDIA_PCM);
    set32(output, CMT_SUBTYPE,       CMT_SUB_PCM);
    set32(output, CMT_SAMPLE_FORMAT, CMT_FMT_S16);
    set32(output, CMT_SAMPLE_RATE,   48000);
    set32(output, CMT_CHANNELS,      2);
    printf("  in: media=%d sub=0x%x hdr=%u bytes / out: 48k S16 2ch\n",
           CMT_MEDIA_AUDIO_ES, CMT_SUB_AAC_LC, 2u);

    printf("step 5: ICinemoAudioCodec_Initialize -- the five-argument shape from the reverse\n");
    rc = initialize(codec, 0, (void *)decoded_cb, input, output);
    printf("  returned rc=%d%s%s\n", (int)rc,
           (rc && errstr) ? " " : "", (rc && errstr) ? errstr(rc) : "");

    printf("step 6: release\n");
    release(codec);
    printf("DONE rc=%d\n", (int)rc);
    return rc == 0 ? 0 : 2;
}
