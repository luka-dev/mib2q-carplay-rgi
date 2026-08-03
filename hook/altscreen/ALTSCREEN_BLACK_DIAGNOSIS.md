# altScreen black-screen fix: implementation verdict

Date: 2026-08-03  
Target: MU1316 / APQ8064, QNX 6.5 ARMv7  
Deployment: stock `libairplay.so` 210.81 + LD_PRELOAD hook + external `altscreen_render`

## Verdict

The CarPlay protocol and receive path are complete. The supplied unit logs prove negotiation,
type-111 SETUP, AirPlay screen framing, AES-CTR, avcC, IDR, Maps `showUI`, Annex-B forwarding,
OMX input, and a full 1440x542 `FillBufferDone`. The remaining fault is format presentation:
the Qualcomm decoder produces native 32m tiled NV12 (`OMX 0x7f000004`, Screen `0x1000c`),
which plane 99 cannot scan out directly.

The renderer has now been changed to preserve the allocator contract that reaches OMX Idle and
to convert only **after** decode:

```text
OMX AVC decoder
  -> Qualcomm 32m Screen WINDOW buffer
  -> post offscreen tiled window
  -> screen_flush_context(SCREEN_WAIT_IDLE)
  -> screen_read_window into Screen-owned RGBA8888 buffer
  -> post RGBA buffer on managed displayable 99
  -> only then OMX_FillThisBuffer recycles the tiled source
```

This is implemented and QNX-built, but physical visibility is still an on-unit acceptance test.
The code deliberately fails closed if this BSP lacks `screen_read_window`; it never falls back to
the pixmap allocator that previously wedged OMX and destabilized `dio_manager`.

## Why this ownership is mandatory

Stock libairplay 210 provides the authoritative Qualcomm decoder contract:

| Stock address | Behavior |
|---|---|
| `0x8a164-0x8a18c` | Requests decoder output color `0x7f000004`. |
| `0x8b350-0x8b39c` | Creates Screen **window** buffers before OMX enters Idle. |
| `0x8b4cc` | Gets each window buffer's `SCREEN_PROPERTY_POINTER`. |
| `0x88fa8` | Attaches those pointers to OMX output buffers. |
| `0x86db0` | Delivers the filled output pointer to the renderer. |
| `0x8b660-0x8b694` | Reverse-maps that pointer to its exact `screen_buffer_t` and posts it. |
| `0x86614` | Calls `FillThisBuffer` only after the consumer is finished. |

The failed linear-NV12 experiment violated the component's native output requirement. The failed
EGL pixmap experiment violated the Screen-window allocation contract by substituting independent
pixmap storage. Both are removed.

`mh2p-cluster` independently establishes the safe post-decode pattern. At upstream commit
`a37c917c1b1479e11684e158efc0e1be3c07faa7`, `gal_cluster.c:630-670` waits for decode,
posts the native staging buffer, flushes with `SCREEN_WAIT_IDLE`, and calls
`screen_read_window` into Screen-owned RGBA storage. Its comments at `gal_cluster.c:910-920`
record that user-owned destinations did not work. Its NVIDIA NvMedia decoder is not reusable on
Qualcomm, but the Screen ownership/readback sequence is.

The QNX Screen header confirms that `screen_read_window` may target a window buffer and requires
`SCREEN_USAGE_NATIVE` at
`Patches/libairplay-mhi2q/vendor/shims/screen/screen.h:10297-10328`.

## Implemented renderer changes

### Native 32m decode allocation restored

- `altscreen_render/sink_omx_qnx.c:216-247` requests and verifies
  `kQOMX_ColorFmt_32m` only. Linear `0x27` is gone.
- `sink_omx_qnx.c:259-280` reads back the component's actual size/count/stride definition.
- `sink_omx_qnx.c:300-320` binds pointers obtained from the tiled Screen window to
  `OMX_UseBuffer`.
- `sink_omx_qnx.c:355-393` creates all native output buffers before waiting for Idle, then
  primes them only after Executing.

### Post-decode tiled-to-RGBA conversion

- `altscreen_render/screen_readback_present.c:145-199` creates an offscreen staging Screen
  window in format `0x1000c`, usage `0x484`, with one buffer per OMX output header.
- `screen_readback_present.c:202-247` creates a managed RGBA8888 plane-99 window with three
  Screen-owned buffers and `READ|WRITE|NATIVE|OPENGL_ES2` usage.
- `screen_readback_present.c:260-307` performs the blocking
  tiled-post -> wait-idle -> readback -> RGBA-post sequence and logs the first converted-frame
  checksum.
- `sink_omx_qnx.c:119-160` maps `FillBufferDone` to the corresponding source buffer and does
  not recycle it until readback returns.
- `sink_omx_qnx.c:326-352` frees OMX headers before destroying their backing Screen window,
  and balances `OMX_Init` even when startup fails.

### Dead/dangerous renderer code removed

- Deleted the independent tiled-pixmap/EGLImage module (`gpu_tiled_present.*`).
- Deleted the standalone EGL calibration path (`testpattern_qnx.c`) from this renderer.
- Removed all altScreen EGL/GLES linkage and stubs from `compile_render_qnx.sh:62-70`.
- Removed the live output-port disable/free/rebuild state machine. Stock 210 ignores
  `OMX_EventPortSettingsChanged`; current code logs and ignores it at
  `sink_omx_qnx.c:89-93`, avoiding a `FillDone`/`FreeBuffer` race.
- Retained the three-start limiter: repeated startup failure cannot indefinitely consume the
  system OMX pool (`sink_omx_qnx.c:461-470`).

## Implemented hook cleanup

The hook remains self-contained; it does not create a stock ScreenStream for type 111.

- `hook/altscreen/altscreen_hook.c:398-450` advertises only when every primitive needed by the
  live 111 path resolves, preventing a partially satisfiable CarPlay contract.
- `altscreen_hook.c:522-549` resolves only the live socket/AES/CF/control functions.
- `altscreen_hook.c:611-805` is the sole receive implementation: own socket, 128-byte AirPlay
  screen header, AES-CTR, avcC/AVCC handling, IDR request, Maps activation, forwarding.
- `altscreen_hook.c:807-894` creates `{type:111,dataPort}` without a stock Screen object and
  starts the acceptor before allowing SETUP success.
- `altscreen_hook.c:920-983` merges `altScreen` into control-SETUP
  `enabledFeatures`, handles 111, and propagates setup failure instead of returning a false
  RTSP success.
- `altscreen_hook.c:989-1025` stops and joins the receiver before stock tears down its session.

Removed from the production hook:

- the unreachable `CFBinaryPlistV0CreateWithData` interpose;
- the obsolete `altScreenURLs` SetProperty/HTTP opener theory;
- the internal `ScreenStreamProcessData` hybrid tap and thread router;
- unused `AirPlayReceiverSessionScreen_{Create,Setup,SetSecurityInfo,Start,Process,Stop}` symbols,
  fake time-sync callbacks, and idle stock Screen object.

`AirPlayReceiverSessionScreen_CopyDisplaysInfo` remains intentionally: it supplies the real stock
main-display dictionary used alongside the synthesized cluster display
(`altscreen_hook.c:59-62,112`).

## Deployment correction

The CarPlay child supervisor enables both required gates only on `dio_manager`:

- `deploy/smartphone_integrator/carplay_startup.sh` starts `dio_manager` with
  `LD_PRELOAD=libcarplay_hook.so`, `ALTSCREEN_HANDLE_ALT=1`, and
  `ALTSCREEN_ADVERT_DISPLAYS=1`.
- `children.carplay.envs` contains no `LD_PRELOAD`, so the wrapper and renderers
  cannot inherit the hook (`deploy/smartphone_integrator/carplay_child.json`).

## Build verification

Completed on 2026-08-03:

- `make -C altscreen_render hostcheck` — passes; only the vendor OMX header's existing
  `OMX_SKIP64BIT` warning remains.
- `./compile_hook.sh` — ARM QNX shared object, `emutls=0`.
- `./compile_render_qnx.sh` — ARM QNX executable, `emutls=0`; no EGL/GLES dependency in
  `altscreen_render` (maneuver renderer still uses its intentional EGL path).
- `build/altscreen_render` SHA-256:
  `6447b65d6f9bb858c86b32d3f20718013961e4247b5d852c59bbb3a2f5c319a9`.
- `build/libcarplay_hook.so` SHA-256:
  `be0db2f4b4ecde4ba7b89806dade4d6143b99a6491815340fb69bef951ab26c0`.

## On-unit acceptance

The first deployment should be judged from `/tmp/altscreen_render.log` in this order:

1. `cluster_surface: window id=99 1440x542 fmt=8 ... managed`
2. `staging manage rc=0 ... id=AA_ALTSCREEN_STAGING`
3. `ready 1440x542, N tiled OMX window buffers -> 3 RGBA plane-99 buffers`
4. `decoder started 1440x542, native-32m N Screen window bufs -> RGBA readback`
5. `FillBufferDone ... filled=1277952`
6. `READBACK PATH OK: Qcom 32m window -> screen_read_window -> RGBA8888 plane 99 ...`

Step 6 plus a changing, nonzero hash proves Screen accepted the 32m source and filled RGBA;
physical visibility then proves displayable 99 composition. If it fails, the log identifies the
exact stage and `errno`. A missing symbol or `screen_read_window` failure is a presentation-only
BSP limitation; it must not affect the hook, stream 111, decoder allocation, or `dio_manager`.

If this BSP cannot read format `0x1000c`, the next safe fallback is an EGLImage **alias of these
same decoder-accepted window buffers** using `screen_attach_pixmap_buffer`; never restore an
independently allocated pixmap as OMX output.
