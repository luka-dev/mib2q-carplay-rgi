#ifndef CR_SCENE_STYLE_HPP
#define CR_SCENE_STYLE_HPP
#include "../visual_style.h"
namespace navigation {
struct Color {
    float r, g, b;
};
struct Frame {
    float left, top, right, bottom;
};
namespace style {
/* The accepted instrument-cluster design, expressed once for all road scenes. */
constexpr float SourceWidth = 328, SourceHeight = 181;
constexpr Frame SmallFrame = {59, 27, 269, 180};
constexpr Frame SmallContent = {67, 35, 261, 172};
constexpr Frame WideContent = {8, 8, 320, 172};
constexpr float LaneWidth = CR_STYLE_ROAD_WIDTH;
constexpr float ArrowWidth = CR_STYLE_ARROW_WIDTH;
constexpr float RouteBase = CR_STYLE_ROUTE_BASE_Y;
constexpr float RouteTop = CR_STYLE_ROUTE_TOP_Y;
constexpr float ShoulderWidth = CR_STYLE_SHOULDER_WIDTH;
constexpr float DividerWidth = .010f;
constexpr float MinSecondaryWidth = .14f;
constexpr float WidthStep = .02f;
constexpr float EntryEnd = -1.4f;
constexpr float NominalEntry = -.55f;
constexpr float EntryFade = .40f;
constexpr float EntryRoadReach = .85f;
constexpr float ExitSolid = .20f;
constexpr float ExitFade = .30f;
constexpr float RouteScale = .90f;
constexpr float RouteOffsetY = .05f;
constexpr float GlyphInset = .022f;
constexpr Color Asphalt = {CR_STYLE_ASPHALT_R, CR_STYLE_ASPHALT_G, CR_STYLE_ASPHALT_B};
constexpr Color Shoulder = {CR_STYLE_SHOULDER_R, CR_STYLE_SHOULDER_G, CR_STYLE_SHOULDER_B};
constexpr Color Divider = {.80f, .80f, .80f};
constexpr Color RecommendationGain = {.02f, .08f, .10f};
constexpr Color RecommendedPavement = {Asphalt.r + RecommendationGain.r, Asphalt.g + RecommendationGain.g,
                                       Asphalt.b + RecommendationGain.b};
constexpr Color MutedGlyph = {.65f, .65f, .65f};
constexpr Color RecommendedGlyph = {.52f, .83f, 1.0f};
} // namespace style
} // namespace navigation
#endif
