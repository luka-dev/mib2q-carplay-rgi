# HU cursor and PRISM preview

Host window using the production `cursor_render/engine.c`, `cursor_gl.c`,
`common/prism_gl.c` and baked PRISM assets. Native canvas: 1024×480; the HU loader
uses its entire area. Optional PNG background is a desktop sample.

```sh
make -C tools/cursor-preview bundle
open "build/HU Cursor Preview.app" --args /absolute/path/background.png
```

Mouse movement sends input positions to the native scene. Its own clock holds the
cursor for 900 ms and fades it over 350 ms. The cursor is a procedural GLES2 glass ring (about 74 pixels in diameter), with
chromatic edge dispersion, specular highlights and an annular shadow. It has no
bitmap/texture and a transparent centre. Native motion filtering changes its
highlight and slightly stretches the ring along movement, then settles to neutral.
A separate native six-second optical cycle moves the spectral highlight and varies
edge dispersion even while the cursor is stationary; it does not extend idle life.

- `L`: PRISM startup, followed by its native animated aperture revealing the background.
- `B`: sample background / dark / grey / light.
- `Space`: repeating startup → moving cursor → idle fade demo.
- Click: graceful 350 ms hide from the current opacity.
- Scroll/wheel: immediate rotary takeover.
- `Esc`: close.

Loader ownership is independent of pointer/demo input: moving, clicking or scrolling
cannot replace an unfinished PRISM scene. The fixture reports first-video readiness
after four seconds; only native `engine.done` permits switching to the pointer.
There is no fixed-time reveal cutoff.

`make -C tools/cursor-preview check` exercises real SDL event routing, delayed frame
delivery and GPU reveal coverage across intermediate frames. It also runs from
`scripts/test_hu_cursor.sh`.

Rendering, animation timing and alpha are production code. Input and video readiness
are local fixtures; this does not test HU adoption, DSI, leases or scanout and opens
no HU connection. For a native-size capture set `CURSOR_PREVIEW_CAPTURE=/absolute/path/frame.bmp`
and `CURSOR_PREVIEW_STAGE=loader|reveal|cursor|cursor-motion`; the process saves and exits.
