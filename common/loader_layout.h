/*
 * Shared startup-loader geometry for the production QNX renderer and the
 * macOS preview.  Coordinates are panel pixels in the 1440x542 CarPlay cluster
 * canvas; padding is measured inward from the selected advertised view.
 *
 * Keep all four cells explicit.  Fullscreen is common to both VC presets;
 * compact Sport is left-aligned and compact Classic is the same size centered.
 */
#ifndef CARPLAY_LOADER_LAYOUT_H
#define CARPLAY_LOADER_LAYOUT_H

#define LOADER_VIEW_FULLSCREEN   0
#define LOADER_VIEW_SMALLSCREEN  1
#define LOADER_VIEW_COUNT        2

#define LOADER_STYLE_SPORT       0
#define LOADER_STYLE_CLASSIC     1
#define LOADER_STYLE_COUNT       2
#define LOADER_STYLE_UNKNOWN     255

typedef struct loader_layout {
    float view_x, view_y, view_w, view_h;
    float pad_left, pad_right, pad_top, pad_bottom;
    float signature_x_offset;
} loader_layout_t;

/* [style][view].  The logical Audi View-button state remains full/compact;
 * preset style selects the physical compact placement. */
static const loader_layout_t k_carplay_loader_layouts
    [LOADER_STYLE_COUNT][LOADER_VIEW_COUNT] = {
    {   /* Sport / driving-profile preset. */
        /* fullscreen: view (270,90)-(1170,452), content bottom y=394 */
        { 270.0f,  90.0f, 900.0f, 362.0f, 0.0f, 0.0f,  0.0f, 58.0f, 90.0f },
        /* smallscreen: content frame translated 7 px upward */
        {  90.0f, 135.0f, 360.0f, 317.0f, 0.0f, 0.0f, -7.0f, 65.0f,  0.0f }
    },
    {   /* Classic preset: compact area is centered between the two gauges. */
        { 270.0f,  90.0f, 900.0f, 362.0f, 0.0f, 0.0f,  0.0f, 58.0f, 90.0f },
        { 540.0f, 135.0f, 360.0f, 317.0f, 0.0f, 0.0f, -7.0f, 65.0f,  0.0f }
    }
};

static inline const loader_layout_t *carplay_loader_layout(int view, int style)
{
    if (view != LOADER_VIEW_SMALLSCREEN) view = LOADER_VIEW_FULLSCREEN;
    if (style != LOADER_STYLE_CLASSIC) style = LOADER_STYLE_SPORT;
    return &k_carplay_loader_layouts[style][view];
}

#endif /* CARPLAY_LOADER_LAYOUT_H */
