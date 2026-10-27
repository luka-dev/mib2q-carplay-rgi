/* Verify pool ownership on failures; no guest or internal Apple code runs. */
#include <assert.h>
#include <stdint.h>
#define sleep probe_sleep
#define printf probe_printf
#define fflush probe_fflush
#include "screen_render_probe.c"
#undef printf
extern int puts(const char *);

static unsigned char pixels[1440*542*4];
static const ScreenSpec *active_spec = &main_screen;
static size_t plane_row(void) { return active_spec->framebuffer == 2 ? 1472 : 832; }
static CFRef selected_fb(void);
static int short_remote, aliased_remote, render_error;
static int mode_error, mode_calls;
static const char colors_tag,timings_tag,color_tag,timing_tag,color_id_tag,timing_id_tag;
unsigned long CFArrayGetTypeID(void) { return 4; }
unsigned long CFDictionaryGetTypeID(void) { return 3; }
unsigned long CFNumberGetTypeID(void) { return 2; }
unsigned long CFGetTypeID(CFRef ref) {
    if(ref==&colors_tag || ref==&timings_tag)return 4;
    if(ref==&color_tag || ref==&timing_tag)return 3;
    return 2;
}
long CFArrayGetCount(CFRef ref) { assert(ref==&colors_tag || ref==&timings_tag); return 1; }
CFRef CFArrayGetValueAtIndex(CFRef ref,long index) { assert(!index);return ref==&colors_tag ? &color_tag : &timing_tag; }
CFRef CFDictionaryGetValue(CFRef ref,CFRef key) { assert(key);return ref==&color_tag ? &color_id_tag : &timing_id_tag; }
int CFNumberGetValue(CFRef ref,long type,void *out) { assert(type==3);*(int *)out=ref==&color_id_tag ? 0 : 1;return 1; }
void CFShow(CFRef ref) { assert(ref==&colors_tag || ref==&timings_tag); }
static unsigned char nv12_pixels[1472*542*3/2+128];
static int acquired_calls, releases, submissions, outstanding;
static int inactive, busy, bad_layout, missing, black, send_error, release_error;
static int nv12, bad_plane_row;
static const char fb_tag, fb2_tag, display_tag, key_tag, surface_tag, true_tag;
static const char source_tag, loop_tag, mode_tag;
static const char bgra_source_tag;
static int source_added, hotplug_enabled;
static CFRef selected_fb(void) { return active_spec->framebuffer == 2 ? &fb2_tag : &fb_tag; }
static u64 fake_time;
static u64 render_cost, last_submit;
static int timebase_error;
CFRef kCFBooleanTrue = &true_tag;
CFRef kCFRunLoopDefaultMode = &mode_tag;
CFRef CFRunLoopGetCurrent(void) { return &loop_tag; }
void CFRunLoopAddSource(CFRef loop, CFRef source, CFRef mode)
{ assert(loop == &loop_tag && source == &source_tag && mode == &mode_tag && !source_added); source_added = 1; }
void CFRunLoopRemoveSource(CFRef loop, CFRef source, CFRef mode)
{ assert(loop == &loop_tag && source == &source_tag && mode == &mode_tag && source_added && !hotplug_enabled); source_added = 0; }
int CFRunLoopRunInMode(CFRef mode, double seconds, unsigned char once)
{ assert(mode == &mode_tag && seconds > 0 && !once && source_added && hotplug_enabled);
  fake_time += (u64)(seconds*1e9)+1; return 3; }
int mach_timebase_info(ProbeTimebase *info) { info->numer = info->denom = 1; return timebase_error; }

unsigned int probe_sleep(unsigned int seconds) { (void)seconds; return 0; }
int probe_printf(const char *fmt, ...) { (void)fmt; return 0; }
int probe_fflush(void *file) { (void)file; return 0; }
CFRef CFStringCreateWithCString(void *allocator, const char *text, u32 encoding)
{ (void)allocator; (void)text; (void)encoding; return &key_tag; }
void CFRelease(CFRef ref) { assert(ref != &fb_tag && ref != &fb2_tag && ref != &display_tag); }
int CFEqual(CFRef lhs, CFRef rhs) { return lhs == rhs; }
CFRef IOSurfaceLookup(u32 id) { assert(id == 41+active_spec->framebuffer && outstanding); return missing ? 0 : &surface_tag; }
size_t IOSurfaceGetWidth(CFRef ref) { (void)ref; return bad_layout ? active_spec->width+1 : active_spec->width; }
size_t IOSurfaceGetHeight(CFRef ref) { (void)ref; return active_spec->height; }
size_t IOSurfaceGetBytesPerRow(CFRef ref) { return nv12 && ref == &surface_tag ? 1252 : active_spec->width*4; }
size_t IOSurfaceGetAllocSize(CFRef ref) { return nv12 && ref == &surface_tag ? sizeof(nv12_pixels) : sizeof(pixels); }
u32 IOSurfaceGetPixelFormat(CFRef ref) { return nv12 && ref == &surface_tag ? 0x34323066 : 0x42475241; }
size_t IOSurfaceGetPlaneCount(CFRef ref) { return nv12 && ref == &surface_tag ? 2 : 0; }
size_t IOSurfaceGetWidthOfPlane(CFRef ref, size_t plane) { (void)ref; return plane ? active_spec->width/2 : active_spec->width; }
size_t IOSurfaceGetHeightOfPlane(CFRef ref, size_t plane) { (void)ref; return plane ? active_spec->height/2 : active_spec->height; }
size_t IOSurfaceGetBytesPerRowOfPlane(CFRef ref, size_t plane)
{ (void)ref; return bad_plane_row && !plane ? plane_row()+118 : plane_row(); }
void *IOSurfaceGetBaseAddressOfPlane(CFRef ref, size_t plane)
{ assert(ref == &surface_tag); return nv12_pixels+64+(plane ? plane_row()*active_spec->height : 0); }
int IOSurfaceLock(CFRef ref, u32 options, u32 *seed) { (void)ref; (void)options; (void)seed; return 0; }
int IOSurfaceUnlock(CFRef ref, u32 options, u32 *seed) { (void)ref; (void)options; (void)seed; return 0; }
void *IOSurfaceGetBaseAddress(CFRef ref) { return nv12 && ref == &surface_tag ? nv12_pixels : pixels; }
int CARenderServerRenderDisplay(u32 port, CFRef display, CFRef surface, int x, int y)
{
    assert(!port && display == &display_tag && !x && !y);
    assert(surface == (nv12 ? &bgra_source_tag : &surface_tag));
    if (!black) pixels[0] = 1;
    fake_time += render_cost;
    return !render_error;
}
u64 mach_absolute_time(void) { return MHI2_SCREEN_SECONDS ? fake_time : 999; }
static long get_remote(CFRef *out, long size) { assert(size == active_spec->framebuffer); out[0] = &fb_tag; if(size==2 && !short_remote)out[1] = aliased_remote ? &fb_tag : &fb2_tag; return short_remote ? 1 : size; }
static CFRef copy_property(CFRef fb, CFRef key)
{ assert(fb == selected_fb() && key == &key_tag); return inactive ? 0 : kCFBooleanTrue; }
static int acquire(CFRef fb, VFBSize size, u32 layer, u64 refcon, u32 flags, u32 *id, u64 *token)
{
    assert(fb == selected_fb() && size.width == active_spec->width && size.height == active_spec->height && !layer && !refcon);
    assert(!flags);
    assert(!outstanding); ++acquired_calls;
    if (busy) return 1;
    outstanding = 1; *id = 41+active_spec->framebuffer; *token = 0x123456789ULL;
    return 0;
}
static int relinquish(CFRef fb, u32 id, u64 token, u32 flags)
{
    assert(fb == selected_fb() && id == 41+active_spec->framebuffer && token == 0x123456789ULL && !flags);
    assert(outstanding); outstanding = 0; ++releases;
    return release_error;
}
static int submit(CFRef fb, u32 layer, u32 id, VFBRect crop, VFBRect dest, u32 swaps, u64 time, u32 flags)
{
    assert(outstanding && fb == selected_fb() && !layer && id == 41+active_spec->framebuffer && swaps == 1 && time == mach_absolute_time() && !flags);
    assert(crop.width == active_spec->width && crop.height == active_spec->height && !crop.x && !crop.y);
    assert(dest.width == active_spec->width && dest.height == active_spec->height && !dest.x && !dest.y);
    ++submissions;
    if (MHI2_SCREEN_SECONDS && last_submit && render_cost)
        assert(time-last_submit >= render_cost); /* no catch-up copies */
    last_submit = time;
    return send_error;
}
static CFRef hotplug_source(CFRef fb) { assert(fb == selected_fb()); return &source_tag; }
static int get_modes(CFRef fb,CFRef *colors,CFRef *timings) { assert(fb==&fb2_tag);*colors=&colors_tag;*timings=&timings_tag;return 0; }
static int mode_size(CFRef modes,u32 id,VFBSize *out) { assert(modes==&timings_tag && id==1);*out=(VFBSize){1440,542};return 0; }
static int set_mode(CFRef fb,u32 color,u32 timing) { assert(fb==&fb2_tag && color==0 && timing==1);++mode_calls;return mode_error; }
static int enable_hotplug(CFRef fb, void (*callback)(u32, void *), void *context)
{ assert(fb == selected_fb() && callback && !context && source_added && !hotplug_enabled); hotplug_enabled = 1; callback(1, context); return 0; }
static int disable_hotplug(CFRef fb)
{ assert(fb == selected_fb() && hotplug_enabled && !outstanding); hotplug_enabled = 0; return 0; }
static void reset(void)
{
    acquired_calls = releases = submissions = outstanding = 0;
    inactive = busy = bad_layout = missing = black = send_error = release_error = 0;
    nv12 = bad_plane_row = 0;
    short_remote = aliased_remote = render_error = 0;
    mode_error = mode_calls = 0;
    active_spec = &main_screen;
    fake_time = 999;
    render_cost = last_submit = 0;
    timebase_error = 0;
    memset(nv12_pixels, 0xa5, sizeof(nv12_pixels));
    assert(!source_added && !hotplug_enabled);
    vfb_get_remote = get_remote; vfb_copy_property = copy_property;
    vfb_acquire = acquire; vfb_relinquish = relinquish; vfb_submit = submit;
    vfb_hotplug_source = hotplug_source; vfb_enable_hotplug = enable_hotplug; vfb_disable_hotplug = disable_hotplug;
    vfb_get_modes=get_modes;vfb_set_mode=set_mode;vfb_mode_size=mode_size;
}
int main(void)
{
    reset(); assert(!framebuffer_frames(&display_tag, 0));
    assert(acquired_calls == submissions && releases == submissions && !outstanding);
    assert(MHI2_SCREEN_SECONDS ? submissions >= 15 : submissions == 15);
    reset(); nv12 = 1; assert(!framebuffer_frames(&display_tag, &bgra_source_tag));
    assert(acquired_calls == submissions && releases == submissions && !outstanding);
    assert(MHI2_SCREEN_SECONDS ? submissions >= 15 : submissions == 15);
    assert(nv12_pixels[64] == 0 && nv12_pixels[64+832*480] == 128);
    assert(nv12_pixels[63] == 0xa5 && nv12_pixels[64+800] == 0xa5);
    assert(nv12_pixels[64+832*480+800] == 0xa5);
    reset(); nv12 = bad_plane_row = 1;
    assert(framebuffer_frames(&display_tag, &bgra_source_tag));
    assert(acquired_calls == 1 && releases == 1 && !submissions && !outstanding);
    reset(); inactive = 1; assert(framebuffer_frames(&display_tag, 0));
    assert(!acquired_calls && !releases && !submissions);
    reset(); busy = 1; assert(framebuffer_frames(&display_tag, 0));
    assert(acquired_calls == (MHI2_SCREEN_SECONDS ? 31 : 30) && !releases && !submissions);
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
    if (MHI2_SCREEN_SECONDS) {
        reset(); timebase_error = 1;
        assert(framebuffer_frames(&display_tag, 0) == 18);
        assert(!acquired_calls && !source_added && !hotplug_enabled);
        reset(); render_cost = 400000000;
        assert(!framebuffer_frames(&display_tag, 0));
        assert(submissions > 1 && submissions < MHI2_SCREEN_SECONDS*3+2);
        assert(acquired_calls == releases && !outstanding);
    }
    reset(); active_spec = &cluster_screen; nv12 = 1;
    assert(!framebuffer_frames_spec(&display_tag, &bgra_source_tag, &cluster_screen));
    assert(submissions >= 15 && acquired_calls == releases && !outstanding);
    assert(mode_calls == 1);
    assert(nv12_pixels[64+1440] == 0xa5 && nv12_pixels[64+plane_row()*542+1440] == 0xa5);
    reset(); active_spec = &cluster_screen; nv12 = black = 1;
    assert(!framebuffer_frames_spec(&display_tag, &bgra_source_tag, &cluster_screen));
    assert(submissions >= 15 && acquired_calls == releases && !outstanding);
    for(unsigned failure=0;failure<5;++failure){
        reset(); active_spec = &cluster_screen; nv12 = 1;
        if(failure==0) short_remote=1;
        if(failure==1) aliased_remote=1;
        if(failure==2) bad_layout=1;
        if(failure==3) bad_plane_row=1;
        if(failure==4) render_error=1;
        assert(framebuffer_frames_spec(&display_tag, &bgra_source_tag, &cluster_screen));
        assert(!submissions && acquired_calls == releases && !outstanding);
    }
    reset(); active_spec = &cluster_screen; inactive = 1;
    assert(framebuffer_frames_spec(&display_tag, &bgra_source_tag, &cluster_screen));
    assert(!submissions && !acquired_calls && !releases);
    reset(); active_spec = &cluster_screen; nv12 = 1; mode_error=1;
    assert(framebuffer_frames_spec(&display_tag, &bgra_source_tag, &cluster_screen)==20);
    assert(mode_calls==1 && !submissions && !acquired_calls && !releases);
    puts("framebuffer acquire/release ownership and failure paths passed");
    return 0;
}
