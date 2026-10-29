# Engineering constraints

Implementation rules to preserve when changing MHI2Q CarPlay. Detailed derivations and earlier incidents are kept in the [archived engineering notes](archive/snapshots-2026-10-29/LANDMINES.md). Read the relevant current subsystem guide before editing its contracts.

## 📡 SI-owned wireless channel boundaries

- Wireless ASI needs source ownership before native execution. [Reference](wireless-carplay/WIRELESS_PRODUCTION_CHANNELS.md).
- Artwork transfer needs Start and Success/Cancel messages on its negotiated file session, in addition to link ACKs.
- `procmgr_symlink` takes target then link. Preserve fixture syscall and socket semantics when testing native QNX behavior.

## ⚙️ Build, toolchain and compile

- Never use `__thread` / `static __thread` in an LD_PRELOAD'd .so on QNX 6.5.
- Build host-side float/DSP render tools with `-ffp-contract=off` (no fast-math).
- Declare `CFArrayAppendCString(arr, str, -1)` and `CFDictionaryGetInt64(dict, key, NULL)` with their three-argument ABI.
- Keep `.init_array` compiler-only (size 000004); no eager module constructors.
- The hook's dynamic export set is an exact allowlist — don't widen it.
- Rust std for QNX 6.5 armv7 needs `+strict-align` (procnto runs SCTLR.A=1).
- Go cross-built for QNX commits its heap reservation — patch `MAP_LAZY`.
- Use the pinned Docker `qnx65-armv7-toolchain` image and build scripts. Record its version with each artifact.
- Never call `std::fmin` / `std::fmax` on floats with the QNX GCC 8.5 toolchain — use `::fminf` / `::fmaxf`. [Reference](cluster-and-rgi/LANE_GUIDANCE.md).
- Keep disabled tracing from constructing argument strings or performing other runtime work. [Reference](build-and-deploy/BUILD_SYSTEM.md#logging-options).

## 🔧 QNX runtime

- Never return a negative from the QSA write interposer to stock dio.
- Never realloc/free a buffer owned by the hooked library unless its allocator is proven, and always keep the skip-on-doubt guard.
- Env gates must check the VALUE (`env_on()`), never bare `getenv()!=NULL`.
- Instrument with `LOG_WARN`, not `LOG_INFO` — the live threshold is WARN.
- Launch the renderers with `GRAPHICS_ROOT=/proc/boot` or EGL SIGSEGVs.
- An exported symbol is not automatically interposable — check the PLT/GOT call path, and `DT_SYMBOLIC`. [Reference](altscreen-and-video/archive/ALTSCREEN_RE_HISTORY.md).
- The unit's `printf` prints its format once and drops extra arguments: one conversion per argument.
- Renderers must reopen stderr in append mode (`"a"`), or supervisor truncation never bounds the log. [Reference](build-and-deploy/archive/PRODUCTION_AUDITS.md).

## 📦 Deploy and SSH

- A Mac-written M.I.B. card carries `._name` AppleDouble files next to every payload file. [Reference](build-and-deploy/DEPLOY_AND_RELEASE.md#-the-mib-sd-install).
- `find` on some FAT cards exits 1 with a complete list. [Reference](build-and-deploy/DEPLOY_AND_RELEASE.md#-the-mib-sd-install).
- M.I.B.'s NavActiveIgnore jar must not sit next to ours. [Reference](build-and-deploy/DEPLOY_AND_RELEASE.md#-the-mib-sd-install).
- A unit/J9 restart interrupts the vehicle UI. Obtain authorization for that restart; file deployment alone does not authorize it.
- Deploying `carplay_hook.jar` = a full unit reboot (j9 `-Xbootclasspath`).
- The M.I.B. script entry runs `command.sh` on release builds and `custom.sh` on `main`. [Reference](build-and-deploy/DEPLOY_AND_RELEASE.md#-the-mib-sd-install).
- SSH to the head unit has no scp/tar/cksum on the far side — read back to verify.
- Activate a release by same-partition rename, published last; renderers are persistent services.
- Deploy matched halves together: hook with jar, jar with `maneuver_render`. [Reference](cluster-and-rgi/RGI_AND_BAP_BRIDGE.md).

## 🌐 Networking, USB and connect

- The live PF ruleset is per-interface (`pf.ecm0.conf` on the LTE car) — verify via `pfctl -sr`.
- Keep USB recovery inside the SI/launcher lifecycle and preserve NCM generation checks. Missing carrier identifies a link/stack state, not its initiating cause. [Reference](connect-and-usb/USB_REPLUG_RECOVERY.md).
- A clean dio exit spares the USB stack; ERROR/TIMEOUT makes SI destroy `carplay0`.
- Debounce the CarPlayApp inactive edge — a fast replug bounces state.
- Don't patch SI `verifyPortReset` 12 → 14 — it is the wrong branch for CarPlay. [Reference](connect-and-usb/SI_STACK_AND_REPLUG_RACE.md).
- Don't raise SI `maxSetupRetries` (2) as a fix. [Reference](connect-and-usb/SI_STACK_AND_REPLUG_RACE.md).
- Don't put a directory under `/tmp`, and don't call `rmdir`, in any on-unit script. [Reference](connect-and-usb/USB_REPLUG_RECOVERY.md#-shipped-2026-09-25-fence-on-the-car).
- Never use `reset port 3 250 1` or switch media port 1 to OTG as recovery. [Reference](connect-and-usb/USB_REPLUG_RECOVERY.md).
- `/proc/<pid>/cmdline` is empty for `usblauncher` on QNX — read argv with `pidin -F "%a %A"`. [Reference](connect-and-usb/USB_REPLUG_RECOVERY.md).
- Never let two listeners share the negative preferred port `-6030`. [Reference](connect-and-usb/SESSION_LIFECYCLE.md).
- iAP2 `0x1D03` is IdentificationRejected, not "Identify end". [Reference](connect-and-usb/VEHICLE_DATA_TO_PHONE.md).
- PASCD must come from a wheel-speed sensor, never from GNSS, and go out about once per second.
- A `0x5001` now-playing update with an unchanged media item is invisible to the HMI. [Reference](connect-and-usb/NOW_PLAYING_COLD_START.md).

## ⚡ Audio — upmix tuning and measurement

- Never tune on 6-second prefixes; use ≥60 s fragments.
- Treat any LFE-default change as a bass edit and re-verify.
- LFE is not a monotonic sub-volume knob.
- The input-EQ bank choice dwarfs every other bass lever.
- The realtime upmix guard uses a 12 ms budget per 16 ms period. Measure execution time on the target when changing DSP cost. [Reference](audio/STEREO_TO_5_1_UPMIX.md).
- Don't trigger the realtime guard on real `deviceUnderruns`. [Reference](audio/STEREO_TO_5_1_UPMIX.md).
- Preserve SoundCheck programme-gain semantics when changing the audio pipeline. [Reference](audio/STEREO_TO_5_1_UPMIX.md#output-order).
- Level-match playback when comparing UpMix modes; front/rear balance and perceived loudness are separate measurements.
- Keep type-103 `arrivalToRenderLatencyMs` consistent with the selected renderer: SCENE reports 11 ms of extra latency relative to the 512-frame baseline. [Reference](audio/BUFFERED_AUDIO_PATH.md).
- A buffered pause is not a stream break — never reset the upmix on `SETRATE 0`. [Reference](audio/BUFFERED_AUDIO_PATH.md).
- The scene mask worker drops controls when it falls behind — `skipped=N` is the first number to read after a car session. [Reference](build-and-deploy/SCENE_BACKEND_DEPLOY.md).

## 🔧 Audio — amp and 5.1 routing

- Mirror `mpl5_dio_ent` caps to `mpl1` exactly — you cannot fix 5.1 from the PCM side.
- The six-channel capability geometry still permits stereo through QSA conversion. [Reference](audio/DISCRETE_5_1_ROUTING.md).
- Verify audio through CarPlay, never with `qsa_play`.
- The amp input matrix is OUTPUT-rows / INPUT-columns — don't transpose it. [Reference](audio/archive/UPMIX_CALIBRATION_AND_TUNING.md#the-corrected-matrix-changed-the-diagnosis).
- Internal amp has no stock six-channel CarPlay route — keep the installer gate. [Reference](audio/INTERNAL_AMP_PATH.md).
- `eSNK_SPEAKERS_MC` (=31) is a DEAD alias of stereo — never target it for 6ch.
- Never code an internal-amp car as `Sound_System=2` — keep 1 and patch the MIBRoot pair. [Reference](audio/INTERNAL_AMP_PATH.md).
- `MIB_AudioTable_*.xml` must stay gzip-compressed, well-formed XML. [Reference](audio/INTERNAL_AMP_PATH.md).
- The `0x10be3` COPP topology A/B is a no-op — use `0x1031b`. [Reference](audio/QNX_AUDIO_TOPOLOGY.md).
- Don't hand-edit the AudioProcess src-7 width byte at file `0x1d3f94`. [Reference](audio/QNX_AUDIO_TOPOLOGY.md).
- `sources/acdb/Speaker_cal.acdb` is the PATCHED `0x10312` variant, not stock [Reference](audio/QNX_AUDIO_TOPOLOGY.md).
- Amp mute sentinels differ per block — never apply one block's sentinel to another. [Reference](audio/SHARC_DSP_BLOCKS.md).
- Level-EQ bank 0 ("disabled") is not flat [Reference](audio/SHARC_DSP_BLOCKS.md).
- "SHARC control 0x10 / 0x13" are the B-library API16 / API19 in hex, not new controls. [Reference](audio/SHARC_B_PROCESSORS.md).

## 📊 Audio — spatial and codec wireformat

- The inline magic cookie is the codec authority — never switch decoders on a stream index field.
- CarPlay realtime spatial is AAC-ELD 48k 5.1.2 (bit 42), NOT PCM 5.1 (bit 44).
- Phone-side realtime spatial is gated by `ScreenSpatialAudioReceiver` — no HU advert can flip it.
- The 8→6 fold is mandatory — there is no 8-channel route.
- 210 libairplay is PCM-passthrough — don't add in-lib AAC/OMX/FDK decode.
- Type-103 legacy `audioOutputFormats` must be ONE codec family (`0x400000`). [Reference](audio/BUFFERED_AUDIO_PATH.md).
- Answer `mainBuffered` only when the phone proposed it, and always together with `/info` `mainBufferedInfo`. [Reference](audio/BUFFERED_AUDIO_PATH.md).
- `audioBufferSize` is the maximum over the whole format offer. [Reference](audio/BUFFERED_AUDIO_PATH.md).
- Hand the Cinemo decoder a heap ASC header and never free it after a successful Initialize. [Reference](audio/CINEMO_AAC_DECODER.md).
- Never splice type-100 PCM into a period while type 103 owns media. [Reference](audio/BUFFERED_PLAYBACK_LIFECYCLE.md).
- Don't blank `updateFeedback` — its `sr` is the sender clock. [Reference](audio/BUFFERED_AUDIO_PATH.md).
- Run the APAP receiver / QSA writer at `SCHED_RR` 18, not the default 10. [Reference](audio/BUFFERED_PLAYBACK_LIFECYCLE.md).
- Never pull a full `sloginfo` during playback. [Reference](audio/BUFFERED_PLAYBACK_LIFECYCLE.md).
- Don't scope the APAC codec by the substring "APAC". [Reference](audio/APAC_CODEC.md).

## 🔍 Cluster contexts

- DisplayManager id 103 is NOT creatable — reuse stock backings 101/102.
- Don't switch the altScreen advert off to fix a cluster flap — production ships it ON.
- `AltScreenModule` is the single cluster-context writer; the stock view-mode chain is dead on FPK.
- `setActiveDisplayable` / encoder re-point fires only on a context CHANGE.
- KSS rejects MOST InfoStates=6 — patch KSSApplication @0x108F42C, or use the LVDS-map path.
- Don't try to block the VC side drawers from the HU. [Reference](cluster-and-rgi/VC_FPK_STATE_MACHINE.md).
- Don't force KDK opacity from the phone's target View or a timer. [Reference](cluster-and-rgi/KDK_GEOMETRY_AND_ANIMATION.md).
- Don't clear MapVisibility (FctID 44) to hide the VC NavStatus bar. [Reference](altscreen-and-video/ALTSCREEN_CLUSTER_VIDEO.md).

## 🔧 RGI and BAP

- Never send exitViewNum=1 — it overwrites the maneuver icon (keep 0).
- Toggle the exitView variant EU↔NAR on every send.
- Call `forceClusterRouteInfoState(true)` on start even for BAP-only routes
- Don't tear down RGI on `visible_in_app` (TLV 0x0F) == 0.
- Don't clear per-slot data on transient `maneuver_count=0`.
- During CarPlay, keep the FctID18 distance model valid alongside the bargraph. [Reference](cluster-and-rgi/DISTANCE_BARGRAPH.md).
- LRU slot lookup must refresh seq on an existing-slot hit and guard active slots.
- App-specific RGI depends on the phone app’s semantic output. Check the supported behavior before changing HU parsing. [Reference](cluster-and-rgi/THIRD_PARTY_NAV_APPS.md).
- `CMD_ALT_ZOOM` is LIVE at `0x0110` — do not re-drop it.
- Keep the full 1440×542 view-area frame — a reduced visible frame is rejected at activation
- Keep FctID 22 at `timeInfoType = 1`; never toggle 1 ↔ 0. [Reference](cluster-and-rgi/RGI_AND_BAP_BRIDGE.md).
- Never publish `""` in FctID 19 — publish a neutral `…`. [Reference](cluster-and-rgi/RGI_AND_BAP_BRIDGE.md).
- Never replace the outgoing iAP2 LocationInfo with `0x5200` — inject `0x5200` as an extra packet via stock `SendIAP2`. [Reference](cluster-and-rgi/IAP2_ROUTE_GUIDANCE_TLVS.md).
- The injection `generation` names a link session, never a send. [Reference](reference/ARCHITECTURE.md#cinemo-nme-and-iap2-injection-abi).
- Keep the ±1000 angle sentinels and presence guards. [Reference](cluster-and-rgi/MANEUVER_MAPPING.md).
- Ramps are slight TURNs: never send `EXIT_RIGHT/A0` or `EXIT_LEFT/60`, and don't restore the old CarPlay type table. [Reference](cluster-and-rgi/MANEUVER_MAPPING.md).
- The HUD arrow selector is the raw FD62 state index, not the `layout_pages.json` list index. [Reference](cluster-and-rgi/MANEUVER_MAPPING.md).
- Use `glBlendFuncSeparate(SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ONE, ONE_MINUS_SRC_ALPHA)`, not `glBlendFunc(SRC_ALPHA, …)`. [Reference](cluster-and-rgi/LANE_GUIDANCE.md).

## ⚡ AltScreen and video

- forceKeyFrame / cluster IDR must be keyed by `uuid`, NOT `displayUUID`.
- Keep TCP_NODELAY on the hook→renderer socket. [Reference](altscreen-and-video/VIDEO_PIPELINE.md).
- Mailbox may return only decoded slots the GPU has never read. [Reference](altscreen-and-video/VIDEO_PIPELINE.md#-decoded-output-mailbox-ownership).
- Receiving another IDR is not presentation-health confirmation. [Reference](altscreen-and-video/ALTSCREEN_CLUSTER_VIDEO.md#idr-recovery).
- A ≥1 s renderer stall must never become a generation restart. [Reference](altscreen-and-video/VIDEO_PIPELINE.md#negotiation-and-presentation-readiness).
- Keep idle/recovery waits independent of wall time. [Reference](altscreen-and-video/ALTSCREEN_PACING_VALIDATION.md#-idle-observation-timebase).
- Do not upgrade capabilities from late HELLO or trust unnegotiated STATUS. [Reference](altscreen-and-video/ALTSCREEN_PACING_VALIDATION.md#-hello-and-status-boundaries).
- The altScreen gate is the control-SETUP *response* `enabledFeatures` containing `"altScreen"` (+ `"viewAreas"`) — not `/info` `extendedFeatures`. [Reference](altscreen-and-video/ALTSCREEN_CLUSTER_VIDEO.md).
- Cluster panel is 1440×542, NOT 1920×720
- `main_disp_modernize` must use the REAL ScreenCopyMain uuid — never fabricate.
- Don't remove `glFinish` / per-frame external-image rebind in altscreen_render "to look faster"
- Decode AirPlay failures using the [status-code reference](reference/CARPLAY_STATUS_CODES.md); retain the operation and session context with the numeric code.
- Never add an output-age filter to `altscreen_render`. [Reference](altscreen-and-video/ALTSCREEN_FRAME_RATE.md).
- Never create a stock `ScreenStream` for 111. [Reference](altscreen-and-video/ALTSCREEN_CLUSTER_VIDEO.md).
- A per-stream TEARDOWN must not stop 111 unless its `streams` list contains 111. [Reference](altscreen-and-video/ALTSCREEN_CLUSTER_VIDEO.md).
- Accept an empty avcC VideoConfig (`bodySize 0`) and the 8-byte size-only cover-art Setup — both are valid. [Reference](altscreen-and-video/VIDEO_PIPELINE.md).
- Keep the 3-failure / 30 s cooldown on OMX decoder start. [Reference](altscreen-and-video/VIDEO_PIPELINE.md).
- Don't patch libosal or raise `setUpdateRate` to 60 for a smoother cluster. [Reference](altscreen-and-video/ALTSCREEN_FRAME_RATE.md).
- Keep immutable selected zoom age. [Reference](altscreen-and-video/ALTSCREEN_INPUT_COMMANDS.md).
- Validate CF construction before replacing pending state. [Reference](altscreen-and-video/ALTSCREEN_INPUT_COMMANDS.md).

## 🔧 Java and HMI

- `CarplayBus.on()` must never wait for `dispatchLock`. [Reference](reference/HOOK_TO_JAVA_BUS.md#-java-side).
- Target Foundation Profile 1.1 (~J2SE 1.4): no generics/enum/enhanced-for/autoboxing/StringBuilder.
- A 1.5 API method compiles then NoSuchMethodError on-unit — gate with the firmware JCL.
- `"isNativeLittleEndian"` is a decompiler artifact for `""` — and javac INLINES it.
- The only Java source tree is `java_patch/`.
- Renderers are persistent supervised services — Java never spawns or kills them. [Reference](reference/HOOK_TO_JAVA_BUS.md).
- Never delete a replaced stock Java class. [Reference](build-and-deploy/archive/PRODUCTION_AUDITS.md).
- `IMMICombiScreenChangeManager` must not be a required service. [Reference](cluster-and-rgi/SMARTPHONE_TAB_AND_KOMBI_DSI.md).
- Never subclass a stock helper class outside `org.dsi` / the classes the jar already uses, and never build a module's listeners in `CarPlayApp`'s static init.

## 📚 Further reading

[Architecture](reference/ARCHITECTURE.md) · [Build and deploy](build-and-deploy/README.md) · [Audio](audio/README.md) · [Cluster and RGI](cluster-and-rgi/README.md) · [AltScreen](altscreen-and-video/README.md) · [USB](connect-and-usb/README.md).
