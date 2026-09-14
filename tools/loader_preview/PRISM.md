# PRISM

Final altscreen startup scene: light traces the Apple from black, the glass
surface gradually appears, and moving spectral reflections continue while
waiting. The selected SOFT signature is `@LuKa_dev`, SF Rounded Medium (500),
122 × 18 panel pixels, with 0.045 em tracking. The glyph coverage is baked;
there is no font lookup at runtime.

The stream arrives through a zoom into the Apple, expands over the entire
1440 × 542 panel, and settles to unchanged stream pixels. On disconnect the
last owned frame zooms back to the waiting Apple without replaying the boot
contour. Retargeting preserves position and velocity.

## Embedded implementation

- `common/prism_baked.h`: compressed material/signature PNG byte arrays and
  desktop GLSL 1.20 / GLES 2.0 shader strings.
- `common/prism.c`: PNG-only memory decoder compiled into the application.
- `common/prism.h`, `prism_handoff.h`, `loader_layout.h`: shared geometry and motion.
- `altscreen_render/screen_egl_present.c`: production EGL compositor and owned
  RGBA stream cache, with the decoder rendezvous in `prism_queue.h`.
- `prism_preview.c`: standalone native macOS preview of the same baked scene.
- `prism_demo_baked.h`: fictional navigation image, linked only into the preview.

The production deliverable is **one `build/altscreen_render` executable**. It
needs the vehicle's normal Screen/EGL/GLES/OMX system libraries, but no PNG,
shader, font, resource directory, or other application asset beside it.
The desktop executable statically includes GLFW and uses macOS system frameworks.

The compositor continues rendering transitions without new input frames. At a
settled full-screen stream it waits for the next decoded frame or disconnect.
The decoder callback returns success only after its source GPU reads finish and
a corresponding composed frame is swapped. Its owned cache survives normal OMX
teardown. A stalled callback has a 750 ms bound and exits the isolated renderer
without freeing live decoder/GPU storage.

## Panel and typography

The panel is 1440 × 542 px over 292 × 110 mm, approximately 125 DPI.
The signature is 24.74 × 3.65 mm at native resolution. Its Medium weight keeps
strokes readable on this panel. The desktop window is a pixel preview, not a
physical ruler for the vehicle display.

| Resting loader | Content rectangle (px) |
| --- | --- |
| Full / either style | 270, 90, 900, 304 |
| Compact / Sport | 90, 135, 360, 252 |
| Compact / Classic | 540, 135, 360, 252 |

Only the waiting loader is confined to these safe areas. The zoom and stream
use the whole panel. Production guide overlays are disabled.

## Build and run

```sh
# Vehicle: authoritative QNX 6.5 ARM build, using the project toolchain image.
./scripts/build_renderers.sh

# macOS: needs the Homebrew GLFW static archive at build time only.
make -C tools/loader_preview
./tools/loader_preview/prism_preview

# Close the running preview first, then create PRISM.app and a bare executable.
make -C tools/loader_preview app
```

The app contains only `Contents/MacOS/prism_preview` and `Contents/Info.plist`.
The bare executable in `output/loader-preview/prism_preview` also runs alone
from an empty directory. No install is needed.

Controls: **T** simulated stream, **R** replay, **V** full/compact,
**S** Sport/Classic, **G** guides, **Space** pause, **Esc** close.
A normal launch demonstrates arrival at 3.4 s and return at 7 s. `--loader-only`
disables the demonstration. Production uses actual decoded video and lifecycle
signals; its binary contains no mock screenshot.

## Authoring and verification

PNG files and `prism.frag` are authoring inputs, not runtime dependencies.
The material generator pads the logo with a transparent 64 px exterior before
computing distances/normals; this prevents a lighting spike at the lower lobe.
To intentionally change artwork, run `make bake` (Python, NumPy, SciPy, Pillow,
and the local macOS system font are needed for the authoring generators).
Commit the regenerated headers with the source change. Normal builds use those
headers directly and do not invoke the generators.

```sh
make -C tools/loader_preview check check-bake
make -C altscreen_render hostcheck
python3 tools/loader_preview/capture_loader.py
python3 tools/loader_preview/capture_handoff.py
```

The checks cover cadence-independent/reversible motion, signature fit, owned
cache orientation after deleting the source, concurrent callbacks, single-frame
transition completion, post failure, and timeout behavior. Capture checks cover
all four layouts, black startup, safe areas, full-panel unchanged stream pixels,
and a clean return. Review media and JSON reports are under
`output/loader-preview/prism-final*`.

The QNX binary is cross-compiled and linked; actual Adreno shader performance,
Screen pacing and reconnect behavior still need on-unit verification.
