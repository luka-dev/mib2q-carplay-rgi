/* Bounded capture of the native CarPlay display tree in the owned iOS VM.
 * Invoked only after usb_fixture validates the synthetic identity as root.
 */
typedef unsigned int u32;
typedef unsigned long size_t;
typedef const void *CFRef;
#include "bgra_to_nv12.h"
typedef struct __sFILE FILE;
extern int printf(const char *, ...);
extern int snprintf(char *, size_t, const char *, ...);
extern int fflush(void *);
extern unsigned int alarm(unsigned int);
extern unsigned int sleep(unsigned int);
extern void *memset(void *, int, size_t);
extern FILE *fopen(const char *, const char *);
extern size_t fwrite(const void *, size_t, size_t, FILE *);
extern int fclose(FILE *);
extern int unlink(const char *);
extern void CFRelease(CFRef);
extern CFRef CFRetain(CFRef);
extern CFRef CFStringCreateWithCString(void *, const char *, u32);
extern CFRef CFNumberCreate(void *, long, const void *);
extern CFRef CFDictionaryCreateMutable(void *, long, const void *, const void *);
extern void CFDictionarySetValue(CFRef, CFRef, CFRef);
extern const unsigned char kCFTypeDictionaryKeyCallBacks[], kCFTypeDictionaryValueCallBacks[];
extern CFRef kCFBooleanTrue;
extern CFRef kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement;
extern CFRef kIOSurfaceBytesPerRow, kIOSurfaceAllocSize, kIOSurfacePixelFormat, kIOSurfaceIsGlobal;
extern CFRef IOSurfaceCreate(CFRef);
extern size_t IOSurfaceAlignProperty(CFRef, size_t);
extern size_t IOSurfaceGetWidth(CFRef), IOSurfaceGetHeight(CFRef), IOSurfaceGetBytesPerRow(CFRef);
extern size_t IOSurfaceGetAllocSize(CFRef);
extern void *IOSurfaceGetBaseAddress(CFRef);
extern int IOSurfaceLock(CFRef, u32, u32 *), IOSurfaceUnlock(CFRef, u32, u32 *);
/* Pinned iOS 18A5351d export returns a boolean; the five arguments match
 * the native client wrapper at 0x188206478 and its implementation.
 */
extern int CARenderServerRenderDisplay(u32, CFRef, CFRef, int, int);
extern CFRef kCVPixelBufferIOSurfacePropertiesKey;
extern int CVPixelBufferCreate(void *, size_t, size_t, u32, CFRef, CFRef *);
extern CFRef CVPixelBufferGetIOSurface(CFRef);
extern CFRef IOServiceMatching(const char *);
extern u32 IOServiceGetMatchingService(u32, CFRef);
extern int IOServiceOpen(u32, u32, u32, u32 *), IOServiceClose(u32), IOObjectRelease(u32);
extern u32 mach_task_self_;

/* ABI verified against the pinned MediaToolbox wrappers and XPC client.
 * The acquire token belongs to the server; it is not a local CF pointer.
 */
typedef unsigned long long u64;
typedef struct { double width, height; } VFBSize;
typedef struct { double x, y, width, height; } VFBRect;
extern int CFEqual(CFRef, CFRef);
extern CFRef IOSurfaceLookup(u32);
extern u32 IOSurfaceGetPixelFormat(CFRef);
extern size_t IOSurfaceGetPlaneCount(CFRef);
extern size_t IOSurfaceGetWidthOfPlane(CFRef, size_t), IOSurfaceGetHeightOfPlane(CFRef, size_t);
extern size_t IOSurfaceGetBytesPerRowOfPlane(CFRef, size_t);
extern void *IOSurfaceGetBaseAddressOfPlane(CFRef, size_t);
extern u64 mach_absolute_time(void);
extern u32 _dyld_image_count(void);
extern const char *_dyld_get_image_name(u32);
extern const void *_dyld_get_image_header(u32);
extern long _dyld_get_image_vmaddr_slide(u32);
extern int strcmp(const char *, const char *), memcmp(const void *, const void *, size_t);
extern void *dlopen(const char *, int);
extern CFRef CFRunLoopGetCurrent(void), kCFRunLoopDefaultMode;
extern void CFRunLoopAddSource(CFRef, CFRef, CFRef), CFRunLoopRemoveSource(CFRef, CFRef, CFRef);
extern int CFRunLoopRunInMode(CFRef, double, unsigned char);

static long (*vfb_get_remote)(CFRef *, long);
static CFRef (*vfb_copy_property)(CFRef, CFRef);
static int (*vfb_acquire)(CFRef, VFBSize, u32, u64, u32, u32 *, u64 *);
static int (*vfb_relinquish)(CFRef, u32, u64, u32);
static int (*vfb_submit)(CFRef, u32, u32, VFBRect, VFBRect, u32, u64, u32);
static CFRef (*vfb_hotplug_source)(CFRef);
static int (*vfb_enable_hotplug)(CFRef, void (*)(u32, void *), void *);
static int (*vfb_disable_hotplug)(CFRef);

static int framebuffer_api(void)
{
    static void *library;
    if (!library) library = dlopen("/System/Library/Frameworks/MediaToolbox.framework/MediaToolbox", 2);
    if (!library) {
        printf("[mhi2-screen-framebuffer] MediaToolbox load failed\n");
        return 1;
    }
    static const unsigned char uuid[16] = {
        0xaf, 0xad, 0x86, 0x48, 0x0a, 0x00, 0x3a, 0x0d,
        0x89, 0x93, 0xb5, 0x82, 0xe3, 0xde, 0x12, 0xdd};
    static const u64 addresses[8] = {0x18bc51934, 0x18c10173c, 0x18c101588, 0x18c101680,
        0x18c100c54, 0x18c101128, 0x18c1011c4, 0x18c10127c};
    static const u32 instructions[8][4] = {
        {0xd503237f, 0xd10143ff, 0xa9034ff4, 0xa9047bfd},
        {0xd503237f, 0xd100c3ff, 0xa9014ff4, 0xa9027bfd},
        {0xd503237f, 0x6dbb23e9, 0xa9015ff8, 0xa90257f6},
        {0xd503237f, 0xa9bd57f6, 0xa9014ff4, 0xa9027bfd},
        {0xd503237f, 0x6db83bef, 0x6d0133ed, 0x6d022beb},
        {0xd503237f, 0xa9be4ff4, 0xa9017bfd, 0x910043fd},
        {0xd503237f, 0xa9bd57f6, 0xa9014ff4, 0xa9027bfd},
        {0xd503237f, 0xa9be4ff4, 0xa9017bfd, 0x910043fd}};
    u32 count = _dyld_image_count();
    printf("[mhi2-screen-framebuffer] loaded images=%u\n", count);
    if (!count || count > 1024) return 1;
    for (u32 i = 0; i < count; ++i) {
        const char *name = _dyld_get_image_name(i);
        if (!name || strcmp(name, "/System/Library/Frameworks/MediaToolbox.framework/MediaToolbox")) continue;
        const u32 *header = _dyld_get_image_header(i);
        if (header) printf("[mhi2-screen-framebuffer] image=%u magic=0x%x cpu=0x%x commands=%u bytes=%u\n",
                           i, header[0], header[1], header[4], header[5]);
        if (!header || header[0] != 0xfeedfacf || header[1] != 0x0100000c ||
                header[4] > 256 || header[5] > 16384) return 1;
        const unsigned char *commands = (const unsigned char *)header + 32;
        size_t offset = 0;
        int matched = 0;
        for (u32 command = 0; command < header[4]; ++command) {
            if (offset > header[5] || header[5] - offset < 8) return 1;
            const u32 *load = (const u32 *)(commands + offset);
            if (load[1] < 8 || load[1] % 4 || load[1] > header[5] - offset) return 1;
            if (load[0] == 0x1b && load[1] == 24 && !memcmp(load + 2, uuid, sizeof(uuid))) matched = 1;
            offset += load[1];
        }
        long slide = _dyld_get_image_vmaddr_slide(i);
        printf("[mhi2-screen-framebuffer] uuid-matched=%d slide=%ld\n", matched, slide);
        if (!matched || slide < 0 || (u64)slide > 0x100000000ULL) return 1;
        for (unsigned function = 0; function < 8; ++function)
            if (memcmp((const void *)(size_t)(addresses[function] + (u64)slide),
                       instructions[function], sizeof(instructions[function]))) {
                printf("[mhi2-screen-framebuffer] instruction mismatch function=%u\n", function);
                return 1;
            }
        /* These functions are internal, not dyld exports. Only this UUID and
         * these checked instructions may be called in the disposable VM.
         */
        vfb_get_remote = (void *)(size_t)(addresses[0] + (u64)slide);
        vfb_copy_property = (void *)(size_t)(addresses[1] + (u64)slide);
        vfb_acquire = (void *)(size_t)(addresses[2] + (u64)slide);
        vfb_relinquish = (void *)(size_t)(addresses[3] + (u64)slide);
        vfb_submit = (void *)(size_t)(addresses[4] + (u64)slide);
        vfb_hotplug_source = (void *)(size_t)(addresses[5] + (u64)slide);
        vfb_enable_hotplug = (void *)(size_t)(addresses[6] + (u64)slide);
        vfb_disable_hotplug = (void *)(size_t)(addresses[7] + (u64)slide);
        return 0;
    }
    printf("[mhi2-screen-framebuffer] MediaToolbox image not loaded\n");
    return 1;
}

static int number(CFRef dict, CFRef key, int value)
{
    CFRef n = CFNumberCreate(0, 3, &value);
    if (!n) return 1;
    CFDictionarySetValue(dict, key, n);
    CFRelease(n);
    return 0;
}

static int write_file(const char *path, const void *data, size_t size)
{
    FILE *file = fopen(path, "wb");
    if (!file) return 1;
    int failed = fwrite(data, 1, size, file) != size;
    if (fclose(file)) failed = 1;
    return failed;
}

static int count_rgb(const unsigned char *pixels, size_t width, size_t height,
                     size_t row, size_t bytes, size_t *count)
{
    if (!pixels || !count || !width || width > 800 || !height || height > 480 ||
            row < width*4 || row > 16384 || height > bytes/row) return 1;
    *count = 0;
    for (size_t y = 0; y < height; ++y)
        for (size_t x = 0; x < width; ++x)
            if (pixels[y*row+x*4] || pixels[y*row+x*4+1] || pixels[y*row+x*4+2]) ++*count;
    return 0;
}

static void framebuffer_hotplug(u32 active, void *context)
{
    (void)context;
    printf("[mhi2-screen-framebuffer] hotplug active=%u\n", active);
    fflush(0);
}

static int plane_bytes(CFRef surface, size_t plane, size_t *available)
{
    size_t base = (size_t)IOSurfaceGetBaseAddress(surface);
    size_t address = (size_t)IOSurfaceGetBaseAddressOfPlane(surface, plane);
    size_t bytes = IOSurfaceGetAllocSize(surface);
    if (!base || !address || address < base || address-base >= bytes) return 1;
    *available = bytes - (address-base);
    if (plane + 1 < IOSurfaceGetPlaneCount(surface)) {
        size_t next = (size_t)IOSurfaceGetBaseAddressOfPlane(surface, plane + 1);
        if (next <= address || next-base >= bytes) return 1;
        *available = next-address;
    }
    return 0;
}

static int render_nv12(CFRef display, CFRef source, CFRef target, size_t *nonzero)
{
    if (!source || IOSurfaceGetPixelFormat(source) != 0x42475241 ||
            IOSurfaceGetWidth(source) != 800 || IOSurfaceGetHeight(source) != 480 ||
            IOSurfaceGetPlaneCount(target) != 2 ||
            IOSurfaceGetWidthOfPlane(target, 0) != 800 || IOSurfaceGetHeightOfPlane(target, 0) != 480 ||
            IOSurfaceGetWidthOfPlane(target, 1) != 400 || IOSurfaceGetHeightOfPlane(target, 1) != 240) return 0;
    size_t row = IOSurfaceGetBytesPerRow(source), bytes = IOSurfaceGetAllocSize(source);
    if (row < 3200 || row > 16384 || bytes < row*480 || IOSurfaceLock(source, 0, 0)) return 0;
    unsigned char *pixels = IOSurfaceGetBaseAddress(source);
    int rendered = 0;
    if (pixels) {
        memset(pixels, 0, row*480);
        rendered = CARenderServerRenderDisplay(0, display, source, 0, 0);
    }
    if (IOSurfaceUnlock(source, 0, 0) || !rendered || IOSurfaceLock(source, 1, 0)) return 0;
    pixels = IOSurfaceGetBaseAddress(source);
    if (count_rgb(pixels, 800, 480, row, bytes, nonzero) || !*nonzero) rendered = 0;
    if (rendered) {
        if (IOSurfaceLock(target, 0, 0)) rendered = 0;
        else {
            size_t y_bytes = 0, uv_bytes = 0;
            if (plane_bytes(target, 0, &y_bytes) || plane_bytes(target, 1, &uv_bytes) ||
                    bgra_to_nv12(pixels, row, bytes,
                        IOSurfaceGetBaseAddressOfPlane(target, 0), IOSurfaceGetBytesPerRowOfPlane(target, 0), y_bytes,
                        IOSurfaceGetBaseAddressOfPlane(target, 1), IOSurfaceGetBytesPerRowOfPlane(target, 1), uv_bytes,
                        800, 480)) rendered = 0;
            if (IOSurfaceUnlock(target, 0, 0)) rendered = 0;
        }
    }
    if (IOSurfaceUnlock(source, 1, 0)) rendered = 0;
    return rendered;
}

static int framebuffer_frames(CFRef display, CFRef source)
{
    /* Request only framebuffer 1, the verified CarPlayScreen. */
    CFRef framebuf = 0;
    long count = vfb_get_remote(&framebuf, 1);
    printf("[mhi2-screen-framebuffer] remote count=%ld present=%d\n", count, framebuf != 0);
    fflush(0);
    if (count != 1 || !framebuf) return 10;
    CFRef key = CFStringCreateWithCString(0, "FramebufIsActive", 0x08000100);
    if (!key) return 11;
    CFRef hotplug_source = vfb_hotplug_source(framebuf), loop = CFRunLoopGetCurrent();
    if (!hotplug_source || !loop) { CFRelease(key); return 15; }
    CFRunLoopAddSource(loop, hotplug_source, kCFRunLoopDefaultMode);
    int enabled = vfb_enable_hotplug(framebuf, framebuffer_hotplug, 0);
    printf("[mhi2-screen-framebuffer] hotplug enable=0x%x\n", enabled);
    fflush(0);
    if (enabled) {
        CFRunLoopRemoveSource(loop, hotplug_source, kCFRunLoopDefaultMode);
        CFRelease(key);
        return 16;
    }
    unsigned submitted = 0;
    int result = 12;
    for (unsigned attempt = 0; attempt < 30; ++attempt) {
        (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, .1, 0);
        CFRef active = vfb_copy_property(framebuf, key);
        int ready = active && CFEqual(active, kCFBooleanTrue);
        if (active) CFRelease(active);
        if (!ready) {
            (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 2, 0);
            continue;
        }
        u32 id = 0;
        u64 token = 0;
        int acquired = vfb_acquire(framebuf,
            (VFBSize){800, 480}, 0, 0, 0, &id, &token);
        printf("[mhi2-screen-framebuffer] attempt=%u acquire=0x%x id=%u token-present=%d\n",
               attempt, acquired, id, token != 0);
        fflush(0);
        if (acquired) {
            (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 2, 0);
            continue;
        }
        /* Every successful acquire is relinquished, including bad layouts. */
        CFRef target = id ? IOSurfaceLookup(id) : 0;
        size_t nonzero = 0;
        int rendered = 0, sent = -1;
        if (target) {
            size_t width = IOSurfaceGetWidth(target), height = IOSurfaceGetHeight(target);
            size_t row = IOSurfaceGetBytesPerRow(target), bytes = IOSurfaceGetAllocSize(target);
            u32 format = IOSurfaceGetPixelFormat(target);
            printf("[mhi2-screen-framebuffer] layout w=%lu h=%lu row=%lu bytes=%lu format=0x%x\n",
                   width, height, row, bytes, format);
            if (width == 800 && height == 480 && format == 0x42475241 &&
                    row >= 3200 && row <= 16384 && bytes >= row * height &&
                    !IOSurfaceLock(target, 0, 0)) {
                unsigned char *pixels = IOSurfaceGetBaseAddress(target);
                if (pixels) {
                    memset(pixels, 0, row * height);
                    rendered = CARenderServerRenderDisplay(0, display, target, 0, 0);
                    if (count_rgb(pixels, width, height, row, bytes, &nonzero)) rendered = 0;
                }
                if (IOSurfaceUnlock(target, 0, 0)) rendered = 0;
            }
            if (width == 800 && height == 480 && format == 0x34323066)
                rendered = render_nv12(display, source, target, &nonzero);
            if (rendered && nonzero) {
                VFBRect rect = {0, 0, 800, 480};
                sent = vfb_submit(framebuf, 0, id, rect, rect, 1, mach_absolute_time(), 0);
            }
            CFRelease(target);
        }
        int released = vfb_relinquish(framebuf, id, token, 0);
        printf("[mhi2-screen-framebuffer] rendered=%d nonzero=%lu submit=0x%x release=0x%x\n",
               rendered, nonzero, sent, released);
        fflush(0);
        if (released || sent) { result = 13; break; }
        ++submitted;
        if (submitted == 15) { result = 0; break; }
        (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, 0);
    }
    int disabled = vfb_disable_hotplug(framebuf);
    if (disabled) result = 17;
    CFRunLoopRemoveSource(loop, hotplug_source, kCFRunLoopDefaultMode);
    CFRelease(key);
    /* The returned framebuffer is borrowed from MediaToolbox's cached array. */
    printf("[mhi2-screen-framebuffer] finished result=%d submitted=%u\n", result, submitted);
    fflush(0);
    return result;
}

static int framebuffer_probe(CFRef display, CFRef source)
{
    int api = framebuffer_api();
    printf("[mhi2-screen-framebuffer] pinned API validated=%d\n", !api);
    fflush(0);
    return api ? 14 : framebuffer_frames(display, source);
}

static int screen_render_probe(int bridge)
{
    static const char *const names[] = {"Wireless", "wireless0"};
    CFRef display[2] = {0, 0};
    CFRef properties = 0, surface = 0, pixel_buffer = 0;
    int result = 1, capture_saved = 0;
    (void)unlink("/var/mobile/Media/mhi2-carplay-screen.bgra");
    (void)unlink("/var/mobile/Media/mhi2-carplay-screen.json");
    alarm(310);
    /* Let the native display services finish their early boot registration. */
    sleep(15);
    if (bridge) {
        int api = framebuffer_api();
        printf("[mhi2-screen-framebuffer] early API validated=%d\n", !api);
        fflush(0);
    }
    u32 service = IOServiceGetMatchingService(0, IOServiceMatching("IOSurfaceRoot")), connection = 0;
    int opened = service ? IOServiceOpen(service, mach_task_self_, 0, &connection) : -1;
    printf("[mhi2-screen-render] IOSurfaceRoot service=%u open=0x%x connection=%u\n", service, opened, connection);
    fflush(0);
    if (connection) IOServiceClose(connection);
    if (service) IOObjectRelease(service);
    size_t row = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, 800 * 4);
    printf("[mhi2-screen-render] aligned row=%lu\n", row);
    fflush(0);
    if (row < 3200 || row > 16384) { result = 2; goto done; }
    size_t size = row * 480;
    properties = CFDictionaryCreateMutable(0, 0, kCFTypeDictionaryKeyCallBacks,
                                           kCFTypeDictionaryValueCallBacks);
    if (!properties || number(properties, kIOSurfaceWidth, 800) ||
            number(properties, kIOSurfaceHeight, 480) || number(properties, kIOSurfaceBytesPerElement, 4) ||
            number(properties, kIOSurfaceBytesPerRow, (int)row) ||
            number(properties, kIOSurfaceAllocSize, (int)size) ||
            number(properties, kIOSurfacePixelFormat, 0x42475241)) { result = 3; goto done; }
    for (unsigned ready = 0; ready < 30 && !surface; ++ready) {
        surface = IOSurfaceCreate(properties);
        printf("[mhi2-screen-render] allocation=%u surface=%d\n", ready, surface != 0);
        fflush(0);
        if (surface) break;
        CFRef attrs = CFDictionaryCreateMutable(0, 0, kCFTypeDictionaryKeyCallBacks,
                                               kCFTypeDictionaryValueCallBacks);
        CFRef empty = CFDictionaryCreateMutable(0, 0, kCFTypeDictionaryKeyCallBacks,
                                               kCFTypeDictionaryValueCallBacks);
        if (!attrs || !empty) {
            if (attrs) CFRelease(attrs);
            if (empty) CFRelease(empty);
            result = 3; goto done;
        }
        CFDictionarySetValue(attrs, kCVPixelBufferIOSurfacePropertiesKey, empty);
        int status = CVPixelBufferCreate(0, 800, 480, 0x42475241, attrs, &pixel_buffer);
        CFRelease(empty);
        CFRelease(attrs);
        CFRef borrowed = !status && pixel_buffer ? CVPixelBufferGetIOSurface(pixel_buffer) : 0;
        if (borrowed) surface = CFRetain(borrowed);
        printf("[mhi2-screen-render] CoreVideo status=%d surface=%d\n", status, surface != 0);
        fflush(0);
        if (surface) {
            row = IOSurfaceGetBytesPerRow(surface);
            if (row < 3200 || row > 16384) { result = 2; goto done; }
            size = row * 480;
        } else {
            if (pixel_buffer) { CFRelease(pixel_buffer); pixel_buffer = 0; }
            sleep(2);
        }
    }
    if (!surface) { result = 4; goto done; }
    printf("[mhi2-screen-render] layout w=%lu h=%lu row=%lu alloc=%lu mapped=%d\n",
           IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface), IOSurfaceGetBytesPerRow(surface),
           IOSurfaceGetAllocSize(surface), IOSurfaceGetBaseAddress(surface) != 0);
    fflush(0);
    if (!surface || IOSurfaceGetWidth(surface) != 800 || IOSurfaceGetHeight(surface) != 480 ||
            IOSurfaceGetBytesPerRow(surface) != row || IOSurfaceGetAllocSize(surface) < size) {
        result = 5; goto done;
    }
    for (unsigned i = 0; i < 2; ++i) {
        display[i] = CFStringCreateWithCString(0, names[i], 0x08000100);
        if (!display[i]) goto done;
    }
    printf("[mhi2-screen-render] waiting for native Wireless display; 800x480 row=%lu\n", row);
    fflush(0);
    for (unsigned attempt = 0; attempt < 115; ++attempt) {
        for (unsigned i = 0; i < 2; ++i) {
            if (IOSurfaceLock(surface, 0, 0)) { result = 6; goto done; }
            unsigned char *pixels = IOSurfaceGetBaseAddress(surface);
            if (!pixels) { (void)IOSurfaceUnlock(surface, 0, 0); result = 7; goto done; }
            memset(pixels, 0, size);
            int rendered = CARenderServerRenderDisplay(0, display[i], surface, 0, 0);
            if (IOSurfaceUnlock(surface, 0, 0) || IOSurfaceLock(surface, 1, 0)) goto done;
            size_t nonzero = 0;
            pixels = IOSurfaceGetBaseAddress(surface);
            /* Count RGB only: opaque black must not be mistaken for UI. */
            if (count_rgb(pixels, 800, 480, row, size, &nonzero)) {
                (void)IOSurfaceUnlock(surface, 1, 0);
                goto done;
            }
            if (rendered && nonzero) {
                char json[256];
                int length = snprintf(json, sizeof(json),
                    "{\"display\":\"%s\",\"width\":800,\"height\":480,\"row_bytes\":%lu,"
                    "\"format\":\"BGRA\",\"bytes\":%lu,\"nonzero_rgb_pixels\":%lu}\n",
                    names[i], row, size, nonzero);
                result = length <= 0 || (size_t)length >= sizeof(json) ||
                    write_file("/var/mobile/Media/mhi2-carplay-screen.bgra", pixels, size) ||
                    write_file("/var/mobile/Media/mhi2-carplay-screen.json", json, (size_t)length);
                if (IOSurfaceUnlock(surface, 1, 0)) result = 1;
                printf("[mhi2-screen-render] captured display=%s nonzero=%lu result=%d\n", names[i], nonzero, result);
                fflush(0);
                capture_saved = !result;
                if (!result && bridge) {
                    /* Preserve the verified capture even if submission fails. */
                    result = framebuffer_probe(display[i], surface);
                    if (result) printf("[mhi2-screen-render] framebuffer result=%d\n", result);
                }
                goto done;
            }
            if (IOSurfaceUnlock(surface, 1, 0)) goto done;
            if (attempt % 15 == 0) {
                printf("[mhi2-screen-render] attempt=%u display=%s rendered=%d nonzero=%lu\n",
                       attempt, names[i], rendered, nonzero);
                fflush(0);
            }
        }
        sleep(2);
    }
done:
    if (result && !capture_saved) {
        (void)unlink("/var/mobile/Media/mhi2-carplay-screen.bgra");
        (void)unlink("/var/mobile/Media/mhi2-carplay-screen.json");
    }
    if (surface) CFRelease(surface);
    if (pixel_buffer) CFRelease(pixel_buffer);
    if (properties) CFRelease(properties);
    for (unsigned i = 0; i < 2; ++i) if (display[i]) CFRelease(display[i]);
    alarm(0);
    printf("[mhi2-screen-render] finished result=%d\n", result);
    fflush(0);
    return result;
}

int mhi2_screen_render_probe(void) { return screen_render_probe(0); }
int mhi2_screen_framebuffer_probe(void) { return screen_render_probe(1); }
