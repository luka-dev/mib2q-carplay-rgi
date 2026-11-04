# PRISM waiting screen and Apple flight

The glass Apple and animated spectral reflections are the production waiting
screen. The SOFT signature is `@LuKa_dev`, SF Rounded Medium, 122×18 panel pixels.
Glyphs and material are embedded; the HU performs no font or image-file lookup.

At disconnect the persistent surface freezes until Java restores the stock map.
PRISM restarts only for a new stream; `LOADER_READY` confirms its first swap
before Java may expose plane 99. Once decoded video
arrives, a 700 ms camera move opens live video through the Apple silhouette.
After the flight, the renderer uses its original direct external-image draw.
The minimum waiting hold is 1000 ms per incoming stream; repeated loader-start
or source-attachment notifications do not extend it. The waiting glass has
brighter cyan/violet/amber light and a moving rim highlight. During the flight,
the baked material normal bends the video slightly at the glass edge, using
the same single video fetch; the bend and highlight disappear at the endpoint.

The expensive glass shader runs once into a 256×256 RGBA crop (262,144 bytes)
for the flight. This crop contains only the Apple and signature at one texel
per panel pixel, with an integer origin; black panel margins are not cached.
Each animation frame uses one native-video sample plus two art
samples, with camera/easing calculated once per frame on the CPU. There is no
video copy, readback, texture allocation or shader compilation in that loop.
The presenter holds at most one decoded picture for timer-driven redraw when
the phone sends a static map; newer pictures supersede it and empty output
cannot evict it. Normal video has no added animation timer.

## altScreen-owned implementation

- `altscreen_render/loader/prism_baked.h`: two embedded PNGs and desktop/GLES versions of the waiting and flight shaders.
- `altscreen_render/loader/prism.c`: PNG-only memory decoder.
- `altscreen_render/loader/prism.h`, `loader_layout.h`: VC layout and signature positioning.
- `altscreen_render/loader/prism_waiting.h`: logo/signature geometry for both production and preview.
- `prism_preview.c`: macOS preview; optional guide outlines are desktop-only geometry.

Each frame clears the window to opaque black and draws both artwork rectangles
in one batch at native resolution. The shader covers 2.40% of the 1440×542 panel
in full view, or 1.67% in compact view. These are pixel-area figures, not measured
GPU speedups. Moving/changing layout leaves no previous artwork behind.

The panel is 292×110 mm; the signature is 24.74×3.65 mm at native resolution.

| View | Content rectangle (px) |
| --- | --- |
| Full / either style | 270, 90, 900, 304 |
| Compact / Sport | 90, 135, 360, 252 |
| Compact / Classic | 540, 135, 360, 252 |

## Build and preview

```sh
./scripts/build_renderers.sh
make -C tools/loader_preview
./tools/loader_preview/prism_preview
make -C tools/loader_preview app
```

The preview links GLFW statically and uses macOS system frameworks. The generated
`output/loader-preview/PRISM.app` and bare executable are self-contained.
Controls: **R** replay, **V** full/compact, **S** Sport/Classic, **G** guides,
**Enter** fly into the demo video, **Space** pause, **Esc** close.
For a still/capture, use `--time <ms>`, `--capture <file.rgba>`, `--frames <count>`,
`--view full|compact`, and `--style sport|classic`. Use `--transition-at <ms>`
to capture the flight and `--video <image.png>` for a chosen sample picture.

## Authoring and verification

Runtime code, shader source, logo and signature belong to `altscreen_render/loader/`.
The preview and authoring tools consume that same implementation.

Normal builds consume the generated header. To edit artwork, `make bake` runs
material/signature generators and bakes the result; it needs Python, Pillow,
NumPy, SciPy and the local macOS font. Keep the regenerated header with its source.

```sh
make -C tools/loader_preview check check-bake
make -C altscreen_render hostcheck
python3 tools/loader_preview/capture_loader.py
```

The real-GL test compares bounded geometry with a full-panel reference draw over
four layouts × six phases, checks seven artwork uniforms, opaque output and black
safe-area margins. Captures additionally verify startup, moving light, signature
visibility and identical full layouts. Reports live in
`output/loader-preview/prism-final-loader/`.

The flight GL test checks all four layouts, opaque frames, monotonic camera motion
and exact video endpoints. It also compares the last waiting image against
the first flight image at the same animation time, including the signature,
to reject a resolution drop at handoff. Native host tests cover static-picture completion,
new pictures, empty output, teardown and one cached draw per transition.
Actual Adreno performance, Screen pacing and reconnect still require HU testing.
The HU logs flight frame count and full present mean/max microseconds at completion.
