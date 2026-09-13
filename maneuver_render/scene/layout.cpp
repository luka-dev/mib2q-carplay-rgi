#include "scene_internal.hpp"

namespace navigation {

void Builder::key_point(float x, float h, float y) {
    if (point_count >= MaxKeys) {
        failed = true;
        return;
    }
    points[point_count][0] = x;
    points[point_count][1] = h;
    points[point_count++][2] = y;
}

void Builder::projected_bounds(const float m[16], float px, float py, float b[4]) {
    int i;
    b[0] = b[1] = 1e6f;
    b[2] = b[3] = -1e6f;
    for (i = 0; i < point_count; ++i) {
        float x = points[i][0] - px, h = points[i][1], y = points[i][2] - py;
        float w = m[3] * x + m[7] * h + m[11] * y + m[15];
        if (!std::isfinite(w) || w <= 1e-6f) {
            failed = true;
            return;
        }
        float sx = (1 + (m[0] * x + m[4] * h + m[8] * y + m[12]) / w) * (style::SourceWidth * .5f);
        float sy = (1 - (m[1] * x + m[5] * h + m[9] * y + m[13]) / w) * (style::SourceHeight * .5f);
        if (!std::isfinite(sx) || !std::isfinite(sy)) {
            failed = true;
            return;
        }
        if (sx < b[0])
            b[0] = sx;
        if (sy < b[1])
            b[1] = sy;
        if (sx > b[2])
            b[2] = sx;
        if (sy > b[3])
            b[3] = sy;
    }
}

void Builder::frame_scene(float arrow_x) {
    const Frame &frame = style::SmallContent;
    float m[16], best = 1e9f;
    int ix, iy;
    camera_pan(0, 0);
    camera_sync();
    camera_matrix(m);
    for (iy = -4; iy <= 4; ++iy)
        for (ix = -50; ix <= 50; ++ix) {
            float px = ix * .01f, py = iy * .02f, b[4], cost, overflow = 0;
            projected_bounds(m, px, py, b);
            if (b[0] < frame.left)
                overflow += frame.left - b[0];
            if (b[1] < frame.top)
                overflow += frame.top - b[1];
            if (b[2] > frame.right)
                overflow += b[2] - frame.right;
            if (b[3] > frame.bottom)
                overflow += b[3] - frame.bottom;
            /* At most a small (~6px) compromise on route centering. */
            if (fabsf(px - arrow_x) > .04f)
                overflow += (fabsf(px - arrow_x) - .04f) * 100;
            cost = overflow * 10000 + fabsf(px - arrow_x) * 100 + fabsf(py) * 12;
            if (cost < best) {
                best = cost;
                camera_x = px;
                camera_y = py;
                framing_ok = overflow < .01f;
                memcpy(frame_bounds, b, sizeof(frame_bounds));
            }
        }
    camera_pan(camera_x, camera_y);
    camera_sync();
}

} // namespace navigation
