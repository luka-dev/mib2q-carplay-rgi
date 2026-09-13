# Approved OEM+ white-base palette — 2026-09-13

The user rejected the previous palette and decorative treatments and requested
an OEM+ appearance, then approved this white-to-blue palette and requested a
commit and push. The shader input is now applied to production render.c.
This is our adaptation, not a claim that factory Audi navigation uses white.

Only the resting arrow colour changes: shader quiet input #FFFAE8 compensates for
the existing cool lighting and produces a bright near-white top face. Active
ice remains exactly vec3(0.32, 0.77, 1.0). Road geometry, asphalt, lighting,
contact shadow, progress feather, framing and animation code are untouched.
There are no extra draws, outlines, textures, bloom or new material effects.

White carries the distant manoeuvre; the established blue carries progress.
Trade-off: the white base is brighter than full blue, so blink reads as a hue
change rather than a brightness increase. Motion and physical cluster appearance
remain separate from these static host checks.

Nine native GL stills cover slight right, U-turn and full roundabout, each at
0%, 50% and full-colour/blink-on. All passed premultiplied-alpha validation;
all three full-colour frames are pixel-identical to the previous fixed-blue
baseline. Initial preview left production sources unchanged. Integration verification
compares all nine production captures against that approved preview.
Grey #464646 backdrop is a contrast probe, not a calibrated cluster screenshot.

- oem-white.png: 2×-scale comparison of two manoeuvres at all three states.
- oem-white-native-size.png: 210×153 actual-size crop comparison.
- checks.json: capture and full-fill parity checks.

Visual reference inspected: an owner's 2017 Audi VC navigation photo shows a
blue route on neutral grey roads, simple borders and a separate progress bar:
https://www.audi-sport.net/xf/threads/319345/
Direct image: https://uploads.tapatalk-cdn.com/20170406/4345da504dbb096881840cc47951acce.jpg
This reference supports the restrained composition; white-base progress colouring
is our adaptation to replacing the separate bar, not an extracted OEM palette.

Temporary build, captures, source hash baseline and generator are under
build/oem-white-study/. Integration logs: build/renderer-release-check/.
Committing this palette does not imply vehicle deployment.
