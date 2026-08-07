# altScreen black/green diagnosis — current on-unit verdict

Date: 2026-08-03  
Target: MU1316 / APQ8064, QNX 6.5 ARMv7  
Deployment: stock `libairplay.so` 210.81 + `libcarplay_hook.so` + external
`altscreen_render`

## Authority boundary

`Patches/libairplay-mhi2q` is Luka's reconstruction and is not behavioral
authority. Evidence below is limited to stock 210, extracted/live MU1316 graphics
binaries, the shipping hook/renderer, and captured on-unit logs.

## Current verdict

The complete type-111 path and the GPU presentation path now work on the physical
cluster. The accepted run received/decrypted real Maps video, decoded complete
1440x542 frames, imported Qualcomm 32m buffers into EGL, converted them to RGBA,
and swapped more than 2,040 colour frames. Readable road labels were physically
visible. This closes the former black/green renderer fault.

Two smaller faults remained in that accepted run:

1. the renderer also flipped the S texture coordinate, so the visible image was
   mirrored horizontally;
2. at frame 2,062, stock invoked a per-stream `AirPlayReceiverSessionTearDown`
   with a non-NULL request. The old interpose stopped altScreen on *every*
   teardown, including one for an unrelated stream. The main CarPlay session
   remained alive while type 111 went black.

Both are fixed in the current build. The hook and renderer were atomically
replaced on HU storage and verified byte-for-byte, without killing or restarting
any process. The running generation keeps its old mapped inode; the next natural
`dio_manager` generation will load the fixes.

That natural generation subsequently started with `dio_manager` PID `16179268`.
Its type-111 stream passed 3,328 forwarded frames and 3,360 GPU RGBA swaps over
more than 106 seconds, with no `VIDEO_OFF`, transport failure, OMX error, or
process restart. This already disproves the former repeatable 30-second cutoff.

## Renderer contract and accepted GPU path

Stock 210 establishes the native output allocation contract:

| Stock address | Behaviour |
|---|---|
| `0x8a164-0x8a18c` | Requests Qualcomm output colour `0x7f000004`. |
| `0x8b350-0x8b39c` | Creates Screen WINDOW buffers before OMX enters Idle. |
| `0x8b4cc` | Reads each buffer's `SCREEN_PROPERTY_POINTER`. |
| `0x88fa8` | Passes those pointers to OMX output headers. |
| `0x86db0` | Delivers the filled output buffer to the consumer. |
| `0x8b660-0x8b694` | Maps the filled pointer back to its Screen buffer. |
| `0x86614` | Recycles it with `FillThisBuffer`. |

The MU1316 decoder confirms this: linear NV12 `0x27` cannot transition from
Loaded to Idle, while native `0x7f000004` reaches Executing and returns complete
frames. The renderer therefore keeps native 32m at
`altscreen_render/sink_omx_qnx.c:43-46,217-247`.

The live `eglsub-screen.so` has an explicit format-`0x1000c` import branch at
`0x691c`, reads planar offsets at `0x6344`, and validates the UV offset at
`0x6998`. `libEGL.so.1` exports `eglWaitNative@0x51a8`,
`eglCreateImageKHR@0x5b3c`, and `eglSwapBuffers@0x5e48`.

The accepted renderer recorded during the diagnostic run:

- `source ABI fmt=0x1000c usage=0x487 size=1440x542 stride=1536
  planar=0,835584,0` (Screen adds bits to requested usage `0x484`);
- `SOURCE PROBE frame=120 ... UVavg=115 range=0..170`;
- `RGBA PROBE frame=120 avg=44,70,106 range=0..255`;
- more than 2,040 `GPU RGBA frames swapped`, with no OMX or presentation error.

Those one-shot CPU readbacks/raw dumps proved that both native chroma and the
RGBA result were real; they are no longer part of the production frame path.
The allocation and pixmap alias are at
`altscreen_render/screen_egl_present.c:177-303`; per-frame native wait,
external-image refresh, shader draw, and RGBA swap are at
`screen_egl_present.c:468-550`.

## Transport stability — accepted

The hook uses a 512 KiB send buffer, completes short writes, retries temporary
`EAGAIN` in 500 ms intervals for up to eight seconds, and replays SPS/PPS on a
genuine reconnect (`hook/altscreen/altscreen_av.c:53-144`). A fresh renderer also
causes a new IDR request (`hook/altscreen/altscreen_hook.c:762-786`).

The accepted run had no `send failed errno=11`, no renderer reconnect storm, and
no repeated OMX startup. This fix is therefore closed.

## Black-after-30-seconds root cause and fix

The accepted type-111 stream started at `00:12:21.980` and ran continuously for
2,062 frames. At `00:13:08.608`, the hook recorded:

```
AirPlayReceiverSessionTearDown session=810bc8 request=752e18 reason=0 ...
alt recv ... stop=1 after 2062 frames
```

The renderer then ended its session and emitted `VIDEO_OFF`. The full-session
teardown did not occur until `00:14:30`; thus the first call was a per-stream
teardown, not termination of CarPlay. A later natural launch reproduced the same
early per-stream teardown.

`alt_find_111` reads `request["streams"][].type` at
`hook/altscreen/altscreen_hook.c:912-930`. The corrected teardown interpose at
`altscreen_hook.c:1002-1056` stops and joins the hook receiver only when:

- `request == NULL` (full session); or
- the non-NULL per-stream request contains `type:111`.

For another stream it still calls stock's real teardown, but preserves the 111
slot/socket/thread. New proof logging is:

```
... alt111=0 action=preserve-alt
per-stream TEARDOWN has no 111 - preserving altScreen ...
```

## Horizontal orientation fix

The first colour run made readable road labels visible and thereby proved the
extra S flip was wrong. Coordinates at
`altscreen_render/screen_egl_present.c:602-608` now preserve normal horizontal S
order and flip only V for Screen's top-left origin.

## Supervisor ownership

`deploy/smartphone_integrator/carplay_startup.sh` records the current `dio` PID.
An older monitor stops renderers only if it still owns that generation; if a new
PID superseded it, the old monitor leaves the new renderers alone. `dio_manager`
remains the exact `exec` child tracked by smartphone_integrator.

A fast unplug/replug exposed a second race in the old cleanup: it took a fresh
process snapshot after its one-second SIGTERM wait, so its `SIGKILL residual`
targeted the newly launched renderer. The wrapper log records old renderer PIDs
`16244865/16269442`, followed by new PIDs `20762758/20803713`, and then the old
cleanup killing the latter. The hook consequently got `EPIPE`; after reconnect,
the new decoder had SPS/PPS but no new IDR and produced green output. Its native
probe already showed the corruption before EGL: `UVavg=0 range=0..38`.

`deploy/smartphone_integrator/carplay_startup.sh` now snapshots renderer PIDs as
soon as its dio exits, waits for a replacement owner, and preserves the snapshot
when cleanup is required. `carplay_processes.sh` sends TERM and residual KILL only
to that same immutable PID set. A newly launched renderer can no longer become a
target of delayed old-generation cleanup.

## IDR recovery after any renderer-link failure

Stock 210 `AirPlayReceiverSessionForceKeyFrame@0x28a5c` constructs only
`{"type":"forceKeyFrame"}` and passes it to
`AirPlayReceiverSessionSendCommand@0x28ac4`; it has no params/display selector.
iOS 26.6 `carEndpoint_handleEventCreatingReply@0x2276d8414-0x2276d8730`
handles that command. Its block at `0x227803620-0x227803650` selects a screen
through `carEndpoint_getScreenStreamForDisplayUUID` when a display UUID was
provided; otherwise `0x227803674-0x2278036ac` falls back to the primary screen
subtype. Thus the old repeated NULL-parameter requests targeted stream 110, not
cluster stream 111.

The hook now sends the generic command
`forceKeyFrame{params:{uuid:<cluster UUID>}}` through the already proven session
command channel. Re-verification against the extracted iOS 26.6 AirPlaySender
showed that `carEndpoint_handleEventCreatingReply@0x2276d86a4-0x2276d86bc`
reads CFString `stru_28381B728`, whose backing string is
`0x22790cc0b = "uuid"`; the separate `displayUUID` string at `0x22793039f` is
not the dictionary key used by this handler. The request
uses no completion callback: recovery is acknowledged by observing a real H.264
type-5 NAL, so stock 210 never retains a callback for a retry. Requests are limited
to one immediately plus five retries at least two seconds apart
(`altscreen_hook.c:698-708,710-851`).
IDR detection scans every length-prefixed NAL in the AVCC access unit, rather than
assuming the first NAL is the IDR (`altscreen_hook.c:679-696`).

After a localhost send failure, a `WAIT_IDR` gate drops all P-frames instead of
feeding them to a fresh OMX decoder. Only an observed IDR opens the gate and is
forwarded; the forward layer then reconnects and replays SPS/PPS before that IDR
(`altscreen_hook.c:782-851`). Consequently a reconnect can temporarily remain
black while waiting for iOS, but it cannot display green inter-prediction garbage.

## Full-HU freeze investigation and reconnect hardening

The reported whole-system freeze ended in a complete HU reboot: the process table
contained a fresh boot generation, all RAM-backed `/tmp/carplay_*` and renderer
logs were gone, and `pidin info` showed about 881 MiB free of 2 GiB after boot.
Therefore the final pre-reset line is unrecoverable. Current-boot `sloginfo` starts
at `Jan 01 00:00:00` and contains normal watchdog kicks, not the prior reset cause.
There is no persistent reset-reason file under the firmware's persist/core paths.
This does not support a RAM-log exhaustion diagnosis.

Persistent core rotation does prove a separate, pre-existing EGL reliability
problem. All four `/mnt/ota/system/core/altscreen_render{,.1,.2,.3}.core.gz`
dumps are `SIGSEGV / MAPERR` in thread 1 at the same relocation inside Qualcomm
`egl14.so`: e.g. PC `0x780c6428`, module base `0x780b0000`, hence `+0x16428`;
the other ASLR generations end in the same `+0x16428`. These cores cannot be dated
to the final reboot because the QNX clock resets, so they are evidence of EGL
churn risk, not proof of the final trigger. Likewise the retained
`postmortem_smmu_gfx3d.txt` predates the reboot and is not attributed to it.

Three safeguards now remove the plausible amplification paths:

- the unbounded frame-count IDR loop is replaced by the six-request/two-second
  limit above, with no async callback allocation;
- a normal TCP disconnect tears down only OMX; the Screen buffers, pixmap aliases,
  EGLImages, EGL context, and managed RGBA window persist and are reused on the next
  connection (`altscreen_render/sink_omx_qnx.c:197-225,338-365,492-499`);
- accepted-run-only `glReadPixels`, 3.1 MiB RGBA raw dump, and native-buffer dump
  code were removed. The steady frame path now contains no file I/O
  (`altscreen_render/screen_egl_present.c:468-550`).

Logging is also bounded and cheaper on the RAM-backed `/tmp` filesystem. The
hook already rotated at 1 MiB with three retained files
(`hook/framework/logging.h:47-56`, `logging.c:93-109`), so its maximum footprint
was only about 4 MiB and cannot explain exhaustion of a 2 GiB system. The final
guard disables the redundant `fsync()` after every direct `write()`
(`logging.c:235-236`). `altscreen_render` now keeps only the current log and one
previous process generation (`altscreen_render/main.c:108-116`), and steady
OMX/GPU counters are emitted every 600 frames instead of every 120
(`sink_omx_qnx.c:113,149,438`; `screen_egl_present.c:530`).

## Build verification

- `make -C altscreen_render hostcheck` — pass (only vendor OMX warning);
- `./compile_render_qnx.sh` — ARM EABI5, `emutls=0`;
- `./compile_hook.sh` — ARM EABI5 shared object, `emutls=0`;
- supervisor `/bin/sh -n` and `git diff --check` — pass.

| Artifact | POSIX cksum | SHA-256 |
|---|---:|---|
| `build/altscreen_render` | `3015096156 40707` | `6e9a22b7b6550dd0377f3db790e2fbe9a72d150c0d43869779378d5e3019a92b` |
| `build/libcarplay_hook.so` | `3178983965 252307` | `7bef5907ae2d1b57f5b90d7e8302c8b354e6d98fa275877816bcfaacbf905405` |

## Remaining on-unit acceptance

The corrected files are already on disk. They were copied back and compared
byte-for-byte; no process was launched or killed. On the next natural launch verify:

1. capture an actual non-111 teardown logging `action=preserve-alt` and continued
   frames after that exact line (the current run already exceeded 106 seconds);
2. confirm physically that road labels are now left-to-right;
3. on a fast unplug/replug, confirm `reusing persistent EGL/Screen presenter across
   reconnect` and no second `eglInitialize`/managed-window creation;
4. if iOS withholds IDR, confirm exactly six targeted requests over at least ten
   seconds followed by `WAIT_IDR request limit reached`, with dio/HU remaining live;
5. confirm a true 111/full-session teardown logs `action=stop-alt` and joins cleanly.

## Main-screen startup priority

Two measured natural generations disprove a 20-second delay inside the hook:

- hook initialization `00:26:31.276` to stock type-110 `READY`
  `00:26:34.688`: about 3.41 seconds;
- hook initialization `00:27:09.809` to stock type-110 `READY`
  `00:27:12.726`: about 2.92 seconds.

Within the second run, control SETUP was handled at `00:27:12.059`, type 111 was
set up by `00:27:12.168` (six milliseconds inside `alt_setup`), and stock type 110
entered INITIALIZING at `00:27:12.470`. Thus altScreen negotiation adds only a
small phone-side step; it does not account for a 20-second stall.

The supervisor did, however, run multiple complete `pidin ar` snapshots before
`exec dio_manager` and from its monitor during dio initialization. Renderer PID
files now reduce this to at most one adoption snapshot per boot/script upgrade;
all recurring checks use `/proc/<pid>`
(`deploy/smartphone_integrator/carplay_processes.sh:14-111`).

The type-111 TCP socket remains available immediately, but `altscreen_render`
waits 750 ms before creating the second Qualcomm OMX/EGL pipeline. This gives
stock type 110 priority over the shared APQ8064 video/GPU services; the measured
110 INITIALIZING-to-READY interval was only about 255-272 ms. The grace is
configurable as `ALTSCREEN_MAIN_GRACE_MS=0..3000`
(`altscreen_render/main.c:32-51,159`). New hook logging records real-stock and
post-hook time separately as `STARTUP SETUP timing`, so a future slow generation
can be attributed without guessing (`hook/altscreen/altscreen_hook.c:998-1081`).
