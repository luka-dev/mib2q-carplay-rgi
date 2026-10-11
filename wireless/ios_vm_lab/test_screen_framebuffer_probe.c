/* Verify pool ownership on failures; no guest or internal Apple code runs. */
#include <assert.h>
#include <stdint.h>
#define sleep probe_sleep
#define printf probe_printf
#define fflush probe_fflush
#include "screen_render_probe.c"
#undef printf
extern int puts(const char *);

static unsigned char pixels[800*480*4];
static unsigned char nv12_pixels[601024];
static int acquired_calls, releases, submissions, outstanding;
static int inactive, busy, bad_layout, missing, black, send_error, release_error;
static int nv12, bad_plane_row;
static const char fb_tag, display_tag, key_tag, surface_tag, true_tag;
static const char source_tag, loop_tag, mode_tag;
static const char bgra_source_tag;
static int source_added, hotplug_enabled;
CFRef kCFBooleanTrue = &true_tag;
CFRef kCFRunLoopDefaultMode = &mode_tag;
CFRef CFRunLoopGetCurrent(void) { return &loop_tag; }
void CFRunLoopAddSource(CFRef loop, CFRef source, CFRef mode)
{ assert(loop == &loop_tag && source == &source_tag && mode == &mode_tag && !source_added); source_added = 1; }
void CFRunLoopRemoveSource(CFRef loop, CFRef source, CFRef mode)
{ assert(loop == &loop_tag && source == &source_tag && mode == &mode_tag && source_added && !hotplug_enabled); source_added = 0; }
int CFRunLoopRunInMode(CFRef mode, double seconds, unsigned char once)
{ assert(mode == &mode_tag && seconds > 0 && !once && source_added && hotplug_enabled); return 3; }

unsigned int probe_sleep(unsigned int seconds) { (void)seconds; return 0; }
int probe_printf(const char *fmt, ...) { (void)fmt; return 0; }
int probe_fflush(void *file) { (void)file; return 0; }
CFRef CFStringCreateWithCString(void *allocator, const char *text, u32 encoding)
{ (void)allocator; (void)text; (void)encoding; return &key_tag; }
void CFRelease(CFRef ref) { assert(ref != &fb_tag && ref != &display_tag); }
int CFEqual(CFRef lhs, CFRef rhs) { return lhs == rhs; }
CFRef IOSurfaceLookup(u32 id) { assert(id == 42 && outstanding); return missing ? 0 : &surface_tag; }
size_t IOSurfaceGetWidth(CFRef ref) { (void)ref; return bad_layout ? 801 : 800; }
size_t IOSurfaceGetHeight(CFRef ref) { (void)ref; return 480; }
size_t IOSurfaceGetBytesPerRow(CFRef ref) { return nv12 && ref == &surface_tag ? 1252 : 3200; }
size_t IOSurfaceGetAllocSize(CFRef ref) { return nv12 && ref == &surface_tag ? sizeof(nv12_pixels) : sizeof(pixels); }
u32 IOSurfaceGetPixelFormat(CFRef ref) { return nv12 && ref == &surface_tag ? 0x34323066 : 0x42475241; }
size_t IOSurfaceGetPlaneCount(CFRef ref) { return nv12 && ref == &surface_tag ? 2 : 0; }
size_t IOSurfaceGetWidthOfPlane(CFRef ref, size_t plane) { (void)ref; return plane ? 400 : 800; }
size_t IOSurfaceGetHeightOfPlane(CFRef ref, size_t plane) { (void)ref; return plane ? 240 : 480; }
size_t IOSurfaceGetBytesPerRowOfPlane(CFRef ref, size_t plane)
{ (void)ref; return bad_plane_row && !plane ? 950 : 832; }
void *IOSurfaceGetBaseAddressOfPlane(CFRef ref, size_t plane)
{ assert(ref == &surface_tag); return nv12_pixels+64+(plane ? 832*480 : 0); }
int IOSurfaceLock(CFRef ref, u32 options, u32 *seed) { (void)ref; (void)options; (void)seed; return 0; }
int IOSurfaceUnlock(CFRef ref, u32 options, u32 *seed) { (void)ref; (void)options; (void)seed; return 0; }
void *IOSurfaceGetBaseAddress(CFRef ref) { return nv12 && ref == &surface_tag ? nv12_pixels : pixels; }
int CARenderServerRenderDisplay(u32 port, CFRef display, CFRef surface, int x, int y)
{
    assert(!port && display == &display_tag && !x && !y);
    assert(surface == (nv12 ? &bgra_source_tag : &surface_tag));
    if (!black) pixels[0] = 1;
    return 1;
}
u64 mach_absolute_time(void) { return 999; }
static long get_remote(CFRef *out, long size) { assert(size == 1); *out = &fb_tag; return 1; }
static CFRef copy_property(CFRef fb, CFRef key)
{ assert(fb == &fb_tag && key == &key_tag); return inactive ? 0 : kCFBooleanTrue; }
static int acquire(CFRef fb, VFBSize size, u32 layer, u64 refcon, u32 flags, u32 *id, u64 *token)
{
    assert(fb == &fb_tag && size.width == 800 && size.height == 480 && !layer && !refcon && !flags);
    assert(!outstanding); ++acquired_calls;
    if (busy) return 1;
    outstanding = 1; *id = 42; *token = 0x123456789ULL;
    return 0;
}
static int relinquish(CFRef fb, u32 id, u64 token, u32 flags)
{
    assert(fb == &fb_tag && id == 42 && token == 0x123456789ULL && !flags);
    assert(outstanding); outstanding = 0; ++releases;
    return release_error;
}
static int submit(CFRef fb, u32 layer, u32 id, VFBRect crop, VFBRect dest, u32 swaps, u64 time, u32 flags)
{
    assert(outstanding && fb == &fb_tag && !layer && id == 42 && swaps == 1 && time == 999 && !flags);
    assert(crop.width == 800 && crop.height == 480 && !crop.x && !crop.y);
    assert(dest.width == 800 && dest.height == 480 && !dest.x && !dest.y);
    ++submissions;
    return send_error;
}
static CFRef hotplug_source(CFRef fb) { assert(fb == &fb_tag); return &source_tag; }
static int enable_hotplug(CFRef fb, void (*callback)(u32, void *), void *context)
{ assert(fb == &fb_tag && callback && !context && source_added && !hotplug_enabled); hotplug_enabled = 1; callback(1, context); return 0; }
static int disable_hotplug(CFRef fb)
{ assert(fb == &fb_tag && hotplug_enabled && !outstanding); hotplug_enabled = 0; return 0; }
static void reset(void)
{
    acquired_calls = releases = submissions = outstanding = 0;
    inactive = busy = bad_layout = missing = black = send_error = release_error = 0;
    nv12 = bad_plane_row = 0;
    memset(nv12_pixels, 0xa5, sizeof(nv12_pixels));
    assert(!source_added && !hotplug_enabled);
    vfb_get_remote = get_remote; vfb_copy_property = copy_property;
    vfb_acquire = acquire; vfb_relinquish = relinquish; vfb_submit = submit;
    vfb_hotplug_source = hotplug_source; vfb_enable_hotplug = enable_hotplug; vfb_disable_hotplug = disable_hotplug;
}
int main(void)
{
    reset(); assert(!framebuffer_frames(&display_tag, 0));
    assert(acquired_calls == 15 && releases == 15 && submissions == 15 && !outstanding);
    reset(); nv12 = 1; assert(!framebuffer_frames(&display_tag, &bgra_source_tag));
    assert(acquired_calls == 15 && releases == 15 && submissions == 15 && !outstanding);
    assert(nv12_pixels[64] == 0 && nv12_pixels[64+832*480] == 128);
    assert(nv12_pixels[63] == 0xa5 && nv12_pixels[64+800] == 0xa5);
    assert(nv12_pixels[64+832*480+800] == 0xa5);
    reset(); nv12 = bad_plane_row = 1;
    assert(framebuffer_frames(&display_tag, &bgra_source_tag));
    assert(acquired_calls == 1 && releases == 1 && !submissions && !outstanding);
    reset(); inactive = 1; assert(framebuffer_frames(&display_tag, 0));
    assert(!acquired_calls && !releases && !submissions);
    reset(); busy = 1; assert(framebuffer_frames(&display_tag, 0));
    assert(acquired_calls == 30 && !releases && !submissions);
    for (unsigned failure = 0; failure < 5; ++failure) {
        reset();
        if (failure == 0) bad_layout = 1;
        if (failure == 1) missing = 1;
        if (failure == 2) black = 1;
        if (failure == 3) send_error = 1;
        if (failure == 4) release_error = 1;
        assert(framebuffer_frames(&display_tag, 0));
        assert(acquired_calls == 1 && releases == 1 && !outstanding);
        assert(submissions == (failure >= 3 ? 1 : 0));
    }
    puts("framebuffer acquire/release ownership and failure paths passed");
    return 0;
}
