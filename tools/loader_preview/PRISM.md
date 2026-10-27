# PRISM waiting loader

The glass Apple and animated spectral reflections are the production waiting
screen. The SOFT signature is `@LuKa_dev`, SF Rounded Medium, 122×18 panel pixels.
Glyphs and material are embedded; the HU performs no font or image-file lookup.

altScreen uses its original waiting-only lifecycle: the first decoded picture
stops/joins the loader, then native video is presented directly. There are no
zoom transitions, video cache, stream sampler, or separate HU overlay.

## altScreen-owned implementation

- `altscreen_render/loader/prism_baked.h`: two embedded PNGs and desktop/GLES versions of one shader.
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
**Space** pause, **Esc** close. The preview always shows the waiting animation.
For a still/capture, use `--time <ms>`, `--capture <file.rgba>`, `--frames <count>`,
`--view full|compact`, and `--style sport|classic`.

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
four layouts × six phases, checks six artwork uniforms, opaque output and black
safe-area margins. Captures additionally verify startup, moving light, signature
visibility and identical full layouts. Reports live in
`output/loader-preview/prism-final-loader/`.

Actual Adreno performance, Screen pacing and reconnect still require HU testing.
