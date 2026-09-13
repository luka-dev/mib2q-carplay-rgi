# Distant-arrow contrast proposal — 2026-09-13

The user found the Audi-inspired aluminium base insufficiently distinct from
asphalt at zero fill. This is an isolated host preview, not a selected/deployed theme.

Compared with D, the new shader endpoints are #CBC5B0 → #FFF6E0. The existing
lighting transforms these values; they are not intended as literal display colours.
Arrow side faces additionally receive a 0.32–1 normal-based multiplier. Top faces,
road paint, soft fill front and contact shadows are unchanged. This is darker
side-face shading, not a new screen-space outline or thicker geometry.

comparison-far.png and comparison-uturn.png show zero fill. comparison-half.png
shows 50% fill. Both base and active remain neutral; raising the base reduces
the brightness range available to the fill/blink, which needs visual review.
Nine actual GL captures passed the existing premultiplied-alpha check without
GL errors. Physical-cluster contrast and motion have not been verified.
Production source, binaries and deployment are unchanged.
Temporary build and raw captures: build/arrow-contrast-study/.
