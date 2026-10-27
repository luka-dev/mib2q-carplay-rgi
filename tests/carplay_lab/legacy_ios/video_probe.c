/* Bounded native diagnostic: Metal availability and CPU-fed VT encoding.
 * This does not feed test pixels into a CarPlay session. Darwin/iOS ABI only.
 */
typedef unsigned int u32;
typedef unsigned long size_t;
typedef const void *CFRef;
typedef struct __sFILE FILE;
typedef struct { long long value; int timescale; u32 flags; long long epoch; } CMTime;
extern int printf(const char *, ...);
extern int fflush(void *);
extern unsigned int alarm(unsigned int);
extern unsigned int sleep(unsigned int);
extern int fork(void);
extern int execv(const char *, char *const []);
extern int waitpid(int, int *, int);
extern int *__error(void);
extern void _exit(int);
extern void *memset(void *, int, size_t);
extern void *malloc(size_t);
extern void free(void *);
extern FILE *fopen(const char *, const char *);
extern size_t fwrite(const void *, size_t, size_t, FILE *);
extern int fclose(FILE *);
extern void CFRelease(CFRef);
extern CFRef CFStringCreateWithCString(void *, const char *, u32);
extern CFRef CFNumberCreate(void *, long, const void *);
extern CFRef kCFPreferencesAnyUser, kCFPreferencesAnyHost;
extern CFRef CFPreferencesCopyValue(CFRef, CFRef, CFRef, CFRef);
extern void CFPreferencesSetValue(CFRef, CFRef, CFRef, CFRef, CFRef);
extern unsigned char CFPreferencesSynchronize(CFRef, CFRef, CFRef);
extern long CFPreferencesGetAppIntegerValue(CFRef, CFRef, unsigned char *);
extern CFRef CFDictionaryCreateMutable(void *, long, const void *, const void *);
extern void CFDictionarySetValue(CFRef, CFRef, CFRef);
extern const unsigned char kCFTypeDictionaryKeyCallBacks[], kCFTypeDictionaryValueCallBacks[];
extern CFRef kCFBooleanTrue, kCFBooleanFalse;
extern CFRef kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder;
extern CFRef kVTCompressionPropertyKey_RealTime, kVTCompressionPropertyKey_AllowFrameReordering;
extern void *MTLCreateSystemDefaultDevice(void);
extern void objc_release(void *);
extern int VTCompressionSessionCreate(void *, int, int, u32, CFRef, CFRef, void *,
    void (*)(void *, void *, int, u32, CFRef), void *, CFRef *);
extern int VTSessionSetProperty(CFRef, CFRef, CFRef);
extern int VTCompressionSessionPrepareToEncodeFrames(CFRef);
extern int VTCompressionSessionEncodeFrame(CFRef, CFRef, CMTime, CMTime, CFRef, void *, u32 *);
extern int VTCompressionSessionCompleteFrames(CFRef, CMTime);
extern void VTCompressionSessionInvalidate(CFRef);
extern CFRef CMSampleBufferGetDataBuffer(CFRef);
extern CFRef CMSampleBufferGetFormatDescription(CFRef);
extern size_t CMBlockBufferGetDataLength(CFRef);
extern int CMBlockBufferCopyDataBytes(CFRef, size_t, size_t, void *);
extern int CMVideoFormatDescriptionGetH264ParameterSetAtIndex(CFRef, size_t,
    const unsigned char **, size_t *, size_t *, int *);
extern int CVPixelBufferCreate(void *, size_t, size_t, u32, CFRef, CFRef *);
extern int CVPixelBufferLockBaseAddress(CFRef, unsigned long long);
extern int CVPixelBufferUnlockBaseAddress(CFRef, unsigned long long);
extern void *CVPixelBufferGetBaseAddressOfPlane(CFRef, size_t);
extern size_t CVPixelBufferGetBytesPerRowOfPlane(CFRef, size_t);
extern size_t CVPixelBufferGetPlaneCount(CFRef);

struct output { FILE *file; unsigned frames, errors; size_t bytes; };
static const unsigned char start_code[] = {0, 0, 0, 1};

static int write_nal(struct output *out, const unsigned char *data, size_t size)
{
    if (fwrite(start_code, 1, 4, out->file) != 4 || fwrite(data, 1, size, out->file) != size)
        return 1;
    out->bytes += 4 + size;
    return 0;
}

static int write_access_unit(struct output *out, const unsigned char *data, size_t total, int length_bytes)
{
    if (!total || length_bytes < 1 || length_bytes > 4) return 1;
    /* Validate the complete access unit before writing any of its NAL units. */
    for (int pass = 0; pass < 2; ++pass) {
        size_t pos = 0;
        while (pos < total) {
            if (total - pos < (size_t)length_bytes) return 1;
            size_t nal_size = 0;
            for (int i = 0; i < length_bytes; ++i) nal_size = (nal_size << 8) | data[pos++];
            if (!nal_size || nal_size > total - pos) return 1;
            if (pass && write_nal(out, data + pos, nal_size)) return 1;
            pos += nal_size;
        }
    }
    return 0;
}

static void encoded(void *context, void *source, int status, u32 flags, CFRef sample)
{
    (void)source;
    struct output *out = context;
    printf("[mhi2-video-info] callback status=%d flags=0x%x sample=%d\n", status, flags, sample != 0);
    fflush(0);
    if (status || !sample || (flags & 2)) { ++out->errors; return; }
    CFRef format = CMSampleBufferGetFormatDescription(sample);
    CFRef block = CMSampleBufferGetDataBuffer(sample);
    const unsigned char *parameter = 0;
    size_t size = 0, count = 0;
    int length_bytes = 0;
    if (!format || !block || CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
            format, 0, &parameter, &size, &count, &length_bytes) ||
            count < 2 || count > 16 || length_bytes < 1 || length_bytes > 4) {
        ++out->errors; return;
    }
    if (!out->frames) {
        for (size_t i = 0; i < count; ++i) {
            if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, i,
                    &parameter, &size, 0, 0) || !size || size > 65536 || write_nal(out, parameter, size)) {
                ++out->errors; return;
            }
        }
    }
    size_t total = CMBlockBufferGetDataLength(block);
    if (!total || total > 1048576) { ++out->errors; return; }
    unsigned char *data = malloc(total);
    if (!data) { ++out->errors; return; }
    if (CMBlockBufferCopyDataBytes(block, 0, total, data)) { free(data); ++out->errors; return; }
    if (write_access_unit(out, data, total, length_bytes)) ++out->errors;
    else ++out->frames;
    free(data);
}

static int encode_probe(u32 codec, int software, const char *path)
{
    CFRef session = 0, pixels = 0;
    CFRef specification = CFDictionaryCreateMutable(0, 0,
        kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
    if (!specification) return 1;
    if (software) CFDictionarySetValue(specification,
        kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder, kCFBooleanFalse);
    struct output out = {0};
    out.file = fopen(path, "wb");
    if (!out.file) { CFRelease(specification); return 1; }
    printf("[mhi2-video-info] begin codec=0x%x software=%d output=%s\n", codec, software, path);
    fflush(0);
    int status = VTCompressionSessionCreate(0, 800, 480, codec, specification, 0, 0, encoded, &out, &session);
    CFRelease(specification);
    printf("[mhi2-video-info] create=%d session=%d\n", status, session != 0);
    fflush(0);
    if (status || !session) goto finish;
    /* Native CarPlay logs and the pinned initializer select the legacy ftvc
     * path for EncoderUsage=38. With no usage, this VM selects its own wrapper
     * again and recurses; that control is recorded separately, not rerun here.
     */
    CFRef usage_key = CFStringCreateWithCString(0, "EncoderUsage", 0x08000100);
    int usage_value = 38;
    CFRef usage_number = CFNumberCreate(0, 3 /* kCFNumberSInt32Type */, &usage_value);
    if (!usage_key || !usage_number) {
        if (usage_key) CFRelease(usage_key);
        if (usage_number) CFRelease(usage_number);
        status = -1; goto finish;
    }
    int usage = VTSessionSetProperty(session, usage_key, usage_number);
    CFRelease(usage_key);
    CFRelease(usage_number);
    int realtime = VTSessionSetProperty(session, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
    int reorder = VTSessionSetProperty(session, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
    status = VTCompressionSessionPrepareToEncodeFrames(session);
    printf("[mhi2-video-info] usage38=%d realtime=%d reorder=%d prepare=%d\n", usage, realtime, reorder, status);
    fflush(0);
    if (status || usage || realtime || reorder) { ++out.errors; goto finish; }
    status = CVPixelBufferCreate(0, 800, 480, 0x34323076 /* 420v */, 0, &pixels);
    if (status || !pixels) goto finish;
    for (int frame = 0; frame < 3; ++frame) {
        status = CVPixelBufferLockBaseAddress(pixels, 0);
        if (status) break;
        if (CVPixelBufferGetPlaneCount(pixels) != 2) {
            CVPixelBufferUnlockBaseAddress(pixels, 0); status = -1; break;
        }
        unsigned char *y = CVPixelBufferGetBaseAddressOfPlane(pixels, 0);
        unsigned char *uv = CVPixelBufferGetBaseAddressOfPlane(pixels, 1);
        size_t ys = CVPixelBufferGetBytesPerRowOfPlane(pixels, 0);
        size_t uvs = CVPixelBufferGetBytesPerRowOfPlane(pixels, 1);
        if (!y || !uv || ys < 800 || uvs < 800) {
            CVPixelBufferUnlockBaseAddress(pixels, 0); status = -1; break;
        }
        for (size_t row = 0; row < 480; ++row) memset(y + row * ys, 32 + frame * 64, ys);
        for (size_t row = 0; row < 240; ++row) memset(uv + row * uvs, 128, uvs);
        status = CVPixelBufferUnlockBaseAddress(pixels, 0);
        if (status) break;
        CMTime pts = {frame, 30, 1, 0}, duration = {1, 30, 1, 0};
        u32 flags = 0;
        status = VTCompressionSessionEncodeFrame(session, pixels, pts, duration, 0, 0, &flags);
        /* Drain before reusing the CPU pixel buffer; no asynchronous mutation. */
        if (!status) status = VTCompressionSessionCompleteFrames(session, pts);
        printf("[mhi2-video-info] encode frame=%d result=%d flags=0x%x\n", frame, status, flags);
        fflush(0);
        if (status) break;
    }
finish:
    if (session) { VTCompressionSessionInvalidate(session); CFRelease(session); }
    if (pixels) CFRelease(pixels);
    if (fclose(out.file)) ++out.errors;
    printf("[mhi2-video-info] result codec=0x%x status=%d frames=%u errors=%u bytes=%lu\n",
        codec, status, out.frames, out.errors, out.bytes);
    fflush(0);
    return status || out.errors || out.frames != 3;
}

int mhi2_video_probe(int software_mode)
{
    CFRef mode_key = 0, mode_domain = 0, old_mode = 0;
    if (software_mode) {
        mode_key = CFStringCreateWithCString(0, "UnifiedEncoderMode", 0x08000100);
        mode_domain = CFStringCreateWithCString(0, "com.apple.VideoProcessing", 0x08000100);
        int mode_value = 0;
        CFRef mode = CFNumberCreate(0, 3, &mode_value);
        if (!mode_key || !mode_domain || !mode) return 9;
        old_mode = CFPreferencesCopyValue(mode_key, mode_domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
        printf("[mhi2-video-info] software-mode previous-present=%d\n", old_mode != 0);
        CFPreferencesSetValue(mode_key, mode, mode_domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
        CFRelease(mode);
        int synced = CFPreferencesSynchronize(mode_domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
        unsigned char present = 0;
        long value = CFPreferencesGetAppIntegerValue(mode_key, mode_domain, &present);
        printf("[mhi2-video-info] software-mode synced=%d present=%d value=%ld\n", synced, present, value);
        fflush(0);
        if (!synced || !present || value != 0) {
            CFPreferencesSetValue(mode_key, old_mode, mode_domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
            CFPreferencesSynchronize(mode_domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
            return 9;
        }
    }
    int failed;
    if (software_mode) {
        /* Fresh child: its 60-second alarm/crash cannot bypass the parent's
         * preference restoration. After fork, the child only calls exec/_exit.
         */
        int child = fork();
        if (!child) {
            char *arguments[] = {"/usr/libexec/mhi2-usb-fixture",
                software_mode == 2 ? "--encoder-mode-hold" : "--video-info", 0};
            execv(arguments[0], arguments);
            _exit(127);
        }
        int child_status = 0, waited = -1;
        if (child > 0) do { waited = waitpid(child, &child_status, 0); } while (waited < 0 && *__error() == 4);
        failed = child < 0 || waited != child || child_status != 0;
        printf("[mhi2-video-info] software-mode child-status=0x%x waited=%d\n", child_status, waited > 0);
        CFPreferencesSetValue(mode_key, old_mode, mode_domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
        int restored = CFPreferencesSynchronize(mode_domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
        printf("[mhi2-video-info] software-mode restored=%d\n", restored);
        fflush(0);
        if (!restored) failed = 1;
        if (old_mode) CFRelease(old_mode);
        CFRelease(mode_key);
        CFRelease(mode_domain);
    } else {
        alarm(60);
        void *metal = MTLCreateSystemDefaultDevice();
        printf("[mhi2-video-info] metal_device=%d\n", metal != 0);
        fflush(0);
        if (metal) objc_release(metal);
        failed = encode_probe(0x61766331, 1, "/private/var/mobile/Media/mhi2-video-probe-avc1-sw.h264");
        failed |= encode_probe(0x61766331, 0, "/private/var/mobile/Media/mhi2-video-probe-avc1-default.h264");
        alarm(0);
    }
    return failed ? 8 : 0;
}
