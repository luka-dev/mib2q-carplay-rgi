# CarPlay startup presentation

The HU cursor and startup-overlay experiment was removed on 2026-09-14.
Stock `libairplay.so` owns main video stream 110 and displayable 59. HU context
78 is again `{16, 59}`. The hook does not interpose main-video config/render or
publish an extra HU readiness protocol. Stock `TMDeviceControl` owns the
Smartphone Integration identity and selected menu item.

## Cluster and preview

altScreen uses the embedded PRISM waiting loader and switches directly to the
first decoded frame. Its existing window, decoder lifecycle and recovery remain
independent of HU presentation. The desktop preview uses the same artwork and
bounded geometry; it no longer contains a simulated stream or zoom transitions.
See [PRISM](../tools/loader_preview/PRISM.md) and the
[implementation note](reference/ALTSCREEN_PRISM_WAITING_2026_09_14.md).

## Input and processes

`TouchpadController` retains single-finger MMI touchpad movement as DPAD ticks.
Touchscreen events and central-console selection/rotation use the stock DSI
paths. There is no pointer plane, pointer transport or synthetic screen click.

The supervisor starts only `maneuver_render` and `altscreen_render`.
The card installer removes an old HU renderer executable during upgrade; the
normal installation reboot ends any old process. No HU application resources
are required by the new package.

## Verification

- Java build and stock bytecode linkage audit.
- Java/native transport regression checks.
- Supervisor lifecycle, renderer PID identity, install/uninstall and legacy-file cleanup.
- QNX ARM hook/renderers, exact hook export allowlist and no emutls.
- PRISM preview GPU/artwork checks and reproducible baked headers.

The removal is verified locally. It has not been deployed or exercised on a
physical HU in this change.
