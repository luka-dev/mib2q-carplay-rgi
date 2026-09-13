# Fixed HUD-blue fill; base-only exploration — 2026-09-13

LATEST USER CONSTRAINT: 100% fill remains blue like the HUD. Change ONLY the base.
Do not whiten the active colour to increase contrast. Previous nearly-white-fill
proposals are rejected. Grey/silver on grey road/VC backdrop was rejected; red was
questioned because it could imply a wrong manoeuvre and differs from the blue HUD.

All three proposals preserve the existing production active endpoint exactly:
vec3 ice = vec3(0.32, 0.77, 1.0). This is not a measured physical HUD colour match.
Only vec3 quiet differs: A #0674CF; B #1D58C8; C #4D55C2.
Colour values in the final image include unchanged production lighting/saturation.
A is brighter at zero fill; B/C trade some base brightness for larger separation
from the fixed cyan/blue fill. A specific base remains unselected.

base-comparison.png: all three bases at 0%, 50%, and 100% / blink-on.
half-comparison.png: larger single-row comparison at 50%.
Grey #464646 backdrop is a contrast probe, not a calibrated cluster screenshot.
27 actual GL captures passed the premultiplied-alpha check without GL errors.
100% / blink-on captures of all three manoeuvres are PIXEL IDENTICAL across bases.
Active host luminance exceeds the base in each candidate.
No production colour, animation, geometry, feathering, road or deployment change.
Temporary sources/build/captures: build/fixed-hud-fill-study/.
