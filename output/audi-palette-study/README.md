# Audi-inspired arrow palette proposals — 2026-09-13

> Later feedback: the user questioned red as suggesting a wrong manoeuvre and wants visual consistency with the blue HUD. See ../hud-blue-study/README.md. These red proposals were not selected.

User requested more Audi-like alternatives to the turquoise, blue and amber study.
These are custom renderer proposals, not factory VC navigation themes.

Brand source: Audi Motorsport CI/CD Guideline, 03/2021, p13:
https://www.audi.com/content/dam/ci/Downloads_EN/Audi_Motorsport_CI_CD_Guideline_2021_EN.pdf
White, black, functional greys and Progressive Red #F50537 informed this study.

D: aluminium grey → white; E: deep red → bright red; F: red → white.
The red shader input compensates approximately for existing lighting/saturation;
it is not the literal brand colour. Host output samples are in palette-values.json.
White is similarly adapted to avoid the existing blue cast.
Only two arrow colour endpoints differ. Production source and all road materials,
geometry, soft fill boundary, camera and animation remain unchanged.
Blink off uses the base; blink on uses the active colour. No third state colour.

27 actual GL captures passed the existing premultiplied-alpha check without GL errors.
All three pairs have higher sampled host luminance in the active state.
Physical-cluster appearance remains unverified. No palette is selected or deployed.
Temporary code, executables and raw captures are in build/audi-palette-study/.
