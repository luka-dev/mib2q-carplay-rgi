#ifndef MHI2_VIDEO_CAPTURE_BOUNDS_H
#define MHI2_VIDEO_CAPTURE_BOUNDS_H
/* OpenMAX's three lengths are uint32_t in the pinned ARM/QNX ABI. */
static int video_capture_bounds(uint32_t allocated, uint32_t filled, uint32_t offset)
{
    return allocated && allocated <= 2097152 && filled && filled <= 262144 &&
        offset <= allocated && filled <= allocated-offset;
}
#endif
