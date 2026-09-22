# LANDMINES — traps that already cost us time

*The rules that break, slow, or mislead — each with the symptom, how it was found,
and where it's documented. Read this before touching a subsystem you don't own.*

> Format per entry: **Rule** (imperative) · what goes wrong · how it was found ·
> `provenance`. Many of these are also distilled in `MEMORY.md` / `memory/*.md`;
> this is the one-page cross-project index. Memory paths are under
> `~/.claude/projects/…/memory/`; doc paths under `docs/`. When a memory note and
> `MEMORY.md` disagree, **`MEMORY.md` is canonical** — conflicts are flagged inline.

---

## Build / toolchain / compile

**Never use `__thread` / `static __thread` in an LD_PRELOAD'd .so on QNX 6.5.**
GCC 4.4.2 emits emulated TLS (`__emutls_*`); any access from a thread that existed
before the .so was preloaded crashes in libc — dio_manager has ~48 such threads →
deterministic SIGSEGV on the first RouteGuidance message. Use heap or one static +
mutex; `#define STBI_NO_THREAD_LOCALS`. · on-car crash 2026-05-12 + audit ·
`memory/qnx_tls_emutls_trap.md`; build assert `nm|grep -ci emutls==0` in
`scripts/build_hook_common.sh`.

**Build the on-target float/DSP path with `-ffp-contract=off` (no fast-math).**
The ARMv7 target has no FMA; a host compiler contracts `a·b+c` into one rounding and
diverges bit-for-bit from the S16 reference — this is what lets the offline render
tools reproduce the car's exact bits. Ship flags: `-O2 -mfpu=neon -mfloat-abi=softfp
-ffp-contract=off`, default libm. · release runtime audit ·
`docs/UPMIX_RELEASE_RUNTIME_AUDIT.md`. (No dedicated memory file — worth promoting.)

**Declare stock CFLite exports with their real 3-arg ABI.**
`CFArrayAppendCString(arr,str,-1)` and `CFDictionaryGetInt64(dict,key,NULL)` are
3-arg; a 2-arg proto leaves ARM r2 garbage → arbitrary overread / arbitrary write. ·
IDA decompile · `memory/cflite_abi_3arg_trap.md`.

**Keep `.init_array` compiler-only (size 000004); no eager module constructors.**
A constructor that reaches `LOG_INFO → pthread_create` under the QNX loader lock makes
cold-start depend on scheduler timing (cold-boot deadlock). · production audit @9295b04
· build assert 6 in `scripts/build_hook_common.sh`; `memory/production_full_audit_9295b04.md`.

**The hook's dynamic export set is an exact allowlist — don't widen it.**
Hidden visibility + version script + a `nm -D` diff assert reject any accidental export;
also asserts no exceptions/RTTI/STL and no libstdc++ in DT_NEEDED (APAC linked with GCC,
not G++; `-fno-threadsafe-statics` avoids the `__cxa_guard` this libstdc++ lacks). ·
build discipline · `scripts/build_hook_common.sh`, `scripts/apac_flags.sh`.

**Rust std for QNX 6.5 armv7 needs `+strict-align` (procnto runs SCTLR.A=1).**
Unaligned loads SIGBUS — std's integer-format LUT read faults, so `println!("{}",n)`
crashes while string printing works. · QEMU bring-up · `memory/rust_qnx65_armv7_feasible.md`.

**Go cross-built for QNX commits its heap reservation — patch `MAP_LAZY`.**
QNX commits anonymous maps, so Go's 512 MB reservation is real RAM (`GOMEMLIMIT`/`GOGC`
don't touch the startup reservation); reclaimed ~516 MB. Add `_qnxMAP_LAZY=0x80` in
`runtime/mem_bsd.go`. · on-car FreeMem 183→704 MB · `memory/qnx_go_map_lazy.md`.

**C builds go through the Docker `qnx65-armv7-toolchain` image; pick the right tag.**
`:4.9` = GCC 4.9.4 (C++11), `:8.5`/`:latest` = GCC 8.5 (C++17, Go, Rust); stock 4.4.2/qcc
is reference-only. BSP libs export only on-unit → scripts auto-gen import stubs. · toolchain
README · `Tools/qnx-65-sdp-docker/README.md`, MEMORY.md "Toolchains".

---

## QNX runtime

**Never return a negative from the QSA write interposer to stock dio.**
Stock `CQNXSoundIF::write` treats any negative as an unsigned byte count → `memset(-47)`
SIGSEGV in libc (our `-ECANCELED` crashed dio_manager). `spa_stock_write_result()`:
failure→0, cancel→size, never negative. · QNX core dump 2026-09-19 ·
`memory/stock_qsa_write_negative_return_crash.md`; `docs/AUDIO_CALL_ROUTING.md`.

**An LD_PRELOAD hook must never realloc/free a buffer owned by the hooked C++ lib.**
`rgd_identify_patcher` realloc'd Cinemo's `NmeArray` (C++ `new[]`, not malloc) →
mismatched-allocator heap corruption during Identify → CarPlay wouldn't start. Skip the
patch if the TLV doesn't fit spare capacity. · on-unit 2026-07-11 ·
`memory/hook_identify_realloc_corruption.md`.

**Env gates must check the VALUE (`env_on()`), never bare `getenv()!=NULL`.**
`export FOO=0` left the gate truthy → the feature stayed ON; cost a full debug session.
Also: on-unit env changes only take effect after a dio-supervisor relaunch (reboot); a
redeployed `.so` is per-fork and needs none. · on-unit debug ·
`memory/hook_getenv_presence_check_trap.md`.

**Instrument with `LOG_WARN`, not `LOG_INFO` — the live threshold is WARN.**
Every `LOG_INFO` is dropped at runtime, so a new counter looks dead; cost a car session.
`touch /mnt/app/carplay_verbose` lifts to INFO without a rebuild; `LOG=0` compiles all
logging/captures out. · on-car · `memory/hook_log_is_warn_only.md`.

**`maneuver_render` needs `GRAPHICS_ROOT=/proc/boot` or EGL SIGSEGVs.**
Adreno `libEGL` getenv's it; unset → null-deref in `eglGetDisplay`. "Worked before" only
because j9 had the var. altscreen_render (no EGL) doesn't need it. Renderer /tmp logs need
`setvbuf(_IONBF)`. · live test 2026-07-31 · `memory/graphics_root_egl_trap.md`.

---

## Deploy / SSH

**NEVER reboot / kill j9 / restart the unit yourself — ask every single time.**
The user is usually in the car; killing j9 blacks the screen ~50 s. Approval to deploy
is not approval to reboot, and approval once is not approval next time. · standing
directive · `memory/feedback_never_reboot_unit.md`.

**Deploying `carplay_hook.jar` = a full unit reboot (j9 `-Xbootclasspath`).**
The jar does nothing until the JVM restarts, and killing j9 is a critical-process death →
whole-system restart (~50 s, SSH drops). Contrast the C hook: only dio_manager needs
killing (per-fork, invisible). Path `/mnt/app/eso/hmi/lsd/jars/carplay_hook.jar` after
`mount -uw /mnt/app`. · measured 2026-08-22 · `memory/java_jar_deploy_reboots_unit.md`.

**SSH to the head unit has no scp/tar/cksum on the far side — read back to verify.**
(1) No remote `scp`/`tar`/`sha256sum`; push via `ssh "cat > f.new" < local` + chmod + mv +
sync; verify by read-back + local `cmp`. (2) sshd rate-limits password auth → use one
ControlMaster with a SHORT `ControlPath` (`/tmp/audi-cm/sock`, not a scratchpad path >104 B).
(3) `ssh` in a `while read` eats the loop's stdin → `ssh -n`. (4) zsh won't word-split
unquoted `$OPTS` → put deploy snippets in a `#!/bin/bash` script. `/mnt/app` is read-only
until `mount -uw`; the login banner prints on stdout even for `ssh host cmd`. · each cost a
failed attempt · `memory/audi_ssh_transport_traps.md`.

**The password in `audi_ssh.sh` is STALE — the working root password is supplied by the user.**
Deploys failed on auth until the correct password was used. · this session ·
`memory/audi_ssh_transport_traps.md` (credential kept out of tree).

**Activate a release by same-partition rename, published last; renderers are always-on services.**
The rename is atomic per file, not a transaction; adding renderer processes as
`servicemgr external:true` breaks the generation model; never overwrite the stock cleanup
script (shared renderer state). · deployment design · `docs/DEPLOYMENT.md`,
`deploy/RENDERERS_AS_SERVICES.md`.

---

## Networking / USB / connect

**The live PF ruleset is per-interface (`pf.ecm0.conf` on the LTE car) — verify via `pfctl -sr`.**
Four profiles ship; patching `pf.conf` alone silently does nothing (6031/6032 stay blocked,
no error). Patch all four; check the ACTIVE ruleset, not the file. · verified 2026-08-28 ·
`memory/pf_profile_per_interface_trap.md`.

**A fast USB replug wedges the device stack — CarPlay never returns until reboot.**
The replug kills `io-usb-dcd`; `/dev/io-usb/otg_dev` goes missing → `carplay0` UP but no
RUNNING (no carrier) → iAP2 never starts → dio misses its budget → SI relaunches it forever
(~500 `TIMEOUT_STARTUP`). Guard in **startup** (self-heals on retry):
`start_otg_stack_device_mode.sh`. `ifconfig carplay0` without RUNNING = stack problem, not
our code. · on-car 2026-09-03 · `memory/usb_device_stack_replug_wedge.md`,
`docs/USB_REPLUG_RECOVERY_RUNBOOK.md`.

**A clean dio exit spares the USB stack; ERROR/TIMEOUT makes SI destroy `carplay0`.**
Only `ERROR_RUNTIME`/`TIMEOUT_*`/`ERROR_SETUP` exits escalate SI → `RestartDeviceStack` →
`stopncm` → `carplay0` destroyed. A `DEINITIALIZED` exit runs `softPath_noStackRestart()`.
ERROR/TIMEOUT ⇒ fix belongs in dio/hook (exit cleanly), not the USB/firewall layer; diagnose
via slog `[CSIStateRunning]` `oldAppState`. · IDA RE of SI · `memory/si_stack_restart_kills_carplay0.md`.

**Debounce the CarPlayApp inactive edge — a fast replug bounces state with no debounce.**
Each ACTIVATING↔INVALID↔ACTIVE edge does a full BAPBridge teardown + cluster ctx flap →
session collapses. Fix = ~400 ms bounded wait on `wantActive=false`, coalesced. (Proposed,
not yet implemented.) · on-car build 34c4c9d · `memory/reconnect_flak_lifecycle_debounce.md`.

**Don't re-investigate `-w 10000` or startncm en-dashes — proven harmless by decompile.**
IPv6 DAD costs ~1 s/connect; the only lever is the global `net.inet6.ip6.dad_count` (set 0
before `start_network`). · decompile 2026-08-24 · `memory/ncm_connect_speed_stock.md`.

---

## Audio — upmix tuning & measurement

**Never tune on 6-second prefixes; use ≥60 s fragments.**
Short prefixes understate bass/mid tilt by ~1.5 dB mean (up to 4.8 dB); quiet intros
attribute nothing. All 6 s conclusions were withdrawn. · 60 s one-variable grid ·
`memory/upmix_long_onevar_2026-09-21.md`, `docs/UPMIX_LONG_ONEVAR_2026-09-21.md`.

**Treat any LFE-default change as a bass edit and re-verify.**
The 18.09 build silently raised the LFE assist default 4096→12288 = **+9.5 dB sub** — the
"bass everywhere" complaint. Fixed via `AUDIO_STEREO_LFE_GAIN` (dB re 4096). · on-car
root-trace 19.09 · `memory/upmix_second_scene_formulation.md`.

**LFE is not a monotonic sub-volume knob.**
In the corrected linear chain LFE sums destructively with other bass — zeroing it can
*raise* modelled sub. "Sub only from LFE / no LFE = silent sub" is false. · corrected
re-render · `docs/UPMIX_MATRIX_RERENDER_2026-09-20.md`.

**Front bass is the only bass lever, and it moves the modelled sub too.**
Front bass −3→0 raised woofer +2.9 dB *and* sub +3.3 dB; the "front woofers only" comment
is wrong. Retention does not touch bass (it's a mid/front restore). · one-variable grid ·
`memory/upmix_long_onevar_2026-09-21.md`.

**Retention lifts the front broadband — apply it with an HF taper, not flat.**
Retention 0.5 raised the front +1.3 dB across 0.5–8 kHz (bright objects drag their treble
forward = "too much treble in front"). Deployed `AUDIO_HSSR_RETENTION_HF=1000:2500`. ·
deploy 2026-09-22 · `docs/research/HSSR_UPMIX_MODEL.md §3.6`,
`docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md §7`.

**The float "candidate" is a bass increase, not a tilt reduction — don't ship it.**
Its −0.46 dB "tilt" was actually a +1.9 dB bass rise + mid restore + rear cut. The whole
float/cabin/quiet path was deleted, S16 byte-identical. · re-attribution ·
`memory/upmix_long_onevar_2026-09-21.md`.

**The input-EQ bank choice dwarfs every other bass lever.**
bank10 vs bank7 moved bass ~+6 dB — more than surround/front-bass/LFE combined; but the
live bank isn't proven in-car, so don't draw bass conclusions without pinning it. · offline
re-render · `docs/UPMIX_MATRIX_RERENDER_2026-09-20.md`.

**Don't vectorize the branchy mask/slot loops.**
They're ~60% of `run_hop` but per-bin with slot decisions; vectorizing means rewriting the
decisions, which already flipped tracker decisions once. · profiling ·
`docs/UPMIX_STATUS_2026-09-22.md §5`.

**The realtime deadline budget (8/10 ms) was invented, not measured.**
There is no established free 75–80% budget; under contention P99 hit ~21.15 ms of the
21.333 ms period. Current in-car figures are estimates; the real period-time distribution
is still uncaptured. · R1 budget audit · `docs/UPMIX_R1_RUNTIME_BUDGET.md`,
`docs/UPMIX_STATUS_2026-09-22.md`.

**Don't trigger the realtime guard on real `deviceUnderruns`.**
Correct in principle, but it reaches into the QSA record intercept that once crashed
dio_manager, and can't be tested now. Stay on a time threshold; gather the histogram first.
· design review · `docs/UPMIX_STATUS_2026-09-22.md §5`.

---

## Audio — amp / 5.1 routing

**Mirror `mpl5_dio_ent` caps to `mpl1` exactly — you cannot fix 5.1 from the PCM side.**
`deva_update_session_info_from_list` copies the record **minima** (min_voices/min_fragsize)
into the active CSD session, built at route-attach **14 s before the PCM channel exists**;
`2/0x1800` truncated each 6ch fragment and tore at 62.5 Hz. · IDA + on-car 2026-09-02 ·
`memory/carplay_51_fault_is_virtual_channel.md`.

**"Widening the minima breaks stereo" is FALSE — QSA inserts a channel conversion.**
This belief cost a month; `mpl1` served stereo at `min==max==6` all along. ·
`memory/carplay_51_fault_is_virtual_channel.md`.

**Verify audio through CarPlay, never with `qsa_play`.**
`qsa_play` sets no SAD, so it inherits the last session's `asad=0xc0` and plays L/R only;
reading that as "amp innocent" sent a 2-day search into ADSP/MOST/SHARC — all correct. The
fix was in the hook's `createMetadata` (announce `asad=0xfc`). · 2 days lost ·
`memory/carplay_sad_header.md`, `memory/deva_stock_pcm_dump.md`.

**The amp input matrix is OUTPUT-rows / INPUT-columns — don't transpose it.**
Older readings had it backwards; re-inserting the transpose shifts sub ~+15 dB. The
per-speaker bass claims from the old orientation ("sub only from LFE", "front woofers sum
L/Ls/C/LFE") are **withdrawn** — cite the corrected re-render for any bass/LFE decision. ·
static decompile 2026-09-20 · `docs/SHARC_MATRIX_CONTROL_2026-09-20.md`,
`docs/UPMIX_MATRIX_RERENDER_2026-09-20.md`.

**Internal (non-B&O) amp has NO 6-channel route — gate `AUDIO_SPATIAL` on E2P 0xFC.**
E2P 0xFC==01 routes CarPlay media → `eSNK_SPEAKERS` stereo (BlockWidth 4); a PCM6 stream
loses C/LFE/Ls/Rs and HSSR thins the fronts. `AUDIO_STEREO_UPMIX=off` alone still rides the
6-voice path. Read: `on -f rcc /usr/apps/modifyE2P r fc 1` (02=external). ·
RCC MIBRoot RE · `memory/internal_amp_no_51_path.md`, `docs/INTERNAL_AMP_MIBROOT_ROUTING.md`.

**`eSNK_SPEAKERS_MC` (=31) is a DEAD alias of stereo — never target it for 6ch.**
0 of 143 resource records, 0 connections; `CMainAmp` aliases it to stereo sink 1. The
correct internal 6ch pair is `{0x46f, eSNK_DTCP51(2)}`. · audioprocess RE ·
`memory/internal_amp_no_51_path.md`, `docs/INTERNAL_AMP_6CH_PATCH_PLAN.md`.

---

## Audio — spatial / codec wireformat

**The inline magic cookie is the codec authority — never switch decoders on a stream index field.**
Reading TLV 8 (`spf`) or TLV 2 as the live format index overwrote the negotiated index on
the first packet and **muted every non-Atmos track**. TLV 2 / SETUP index are advisory
hints; drop unsupported ones, don't tear down a working decoder. · two vehicle captures ·
`memory/apap_no_format_index.md`.

**CarPlay spatial is AAC-LC 48k 5.1.2 (bit 42), NOT PCM 5.1 (bit 44).**
The phone never offers bit 44 on the realtime path; both bits were offered live and the
phone chose stereo (0x8000) both times — advertising is not sufficient. · iOS 27 RE + live
A/B · `memory/carplay_spatial_atmos_wireformat.md`, `docs/reference/AUDIO.md §9-19`.

**Phone-side spatial is gated by `ScreenSpatialAudioReceiver` — no HU advert can flip it.**
`APAudioFormatIsScreenSpatialAudioReceiver` returns false unless a phone-side feature
flag/device-model is set; it never reads the accessory declaration. The gap is entirely
phone-side. · iOS 27 beta 6 RE · `docs/HANDOFF_SPATIAL_AUDIO.md`,
`memory/carplay_spatial_atmos_wireformat.md`.

**The 8→6 fold is mandatory — there is no 8-channel route.**
`deva_mib2_channel_map` switches on {1,2,3,6} only; `deva_csd_start_csd_session_playback`
hardcodes 6; the amp declares positions 7/8 as BCL/BCR with no height codes, so 7.1.4
TopSide folds into the rears. 6 is a hard ceiling. · RE + on-car ·
`memory/carplay_sad_header.md`, `memory/carplay_spatial_atmos_wireformat.md`.

**210 libairplay is PCM-passthrough — don't add in-lib AAC/OMX/FDK decode.**
`AudioConverterNew_compat` is a dead stub (−6735); 210 advertises PCM only; audio out is
ALSA (OMX is video-only). Multichannel arrives via the buffered type-103 path instead. ·
RE 2026-07-13 · `memory/audio_210_pcm_passthrough.md`, `memory/cinemo_audio_codec_abi.md`.

---

## Cluster contexts

**DisplayManager id 103 is NOT creatable — reuse stock backings 101/102.**
`createImageDisplayable(103)` silently no-ops (unregistered DSI id); reuse 101 (328×180) /
102 (210×153). The 101/102 constant *names* are inverted vs physical size. · on-car ·
`memory/carplay_cluster_context_architecture.md`.

**altScreen advert must stay OFF or the cluster flaps ctx 80↔74.**
With advert ON the projection flaps ACTIVE→INVALID and the backing flickers. (Older
`reconnect_flak` note says advert ON streamed fine — the flap trap is the later canonical
position; re-verify on the current build.) · on-car ·
`memory/carplay_cluster_context_architecture.md` (canonical) vs
`memory/reconnect_flak_lifecycle_debounce.md` (stale).

**Enter cluster ctx 80 via the DM switch guarded by `i<80`; the stock view-mode chain is dead on FPK.**
Cluster map = FPK here, so `refreshViewMode`'s `if(!FPK&&!MMI)` guard skips
`setFavoredViewMode(MAP)` — it's DEAD. `AltScreenModule` must be the single ctx writer; two
writers gave a black cluster / 30 s hang. (Overturns the earlier `setFavoredViewMode`
design note.) · ClusterProbe 2026-08-01 · `memory/cluster_carplay_viewmode_design.md`,
`memory/carplay_cluster_context_architecture.md`.

**`setActiveDisplayable` fires only on a context CHANGE.**
No-op without a ctx change; force `switchContext(0)→72→setActiveDisplayable(4,33)`. ·
decompile + on-car · `memory/lvds_video_pipeline.md`.

**KSS rejects MOST InfoStates=6 — patch KSSApplication @0x108F42C, or use the LVDS-map path.**
`sub_108F42C` validation rejects 6 → VC never enters the compass/AIO-arrow FPK state. The
prod LVDS-map path doesn't need it. · decompile · `memory/kss_infostates6_rejection.md`.

---

## RGI / BAP

**Never send exitViewNum=1 — it overwrites the maneuver icon (keep 0).** · on-car A/B +
decompile · `MEMORY.md` §"FSG Sync & ExitView"; `ClusterService.java`.

**Toggle the exitView variant EU↔NAR on every send.**
Unchanged variant+num → FctID49 skipped → the FSG sync never closes (sync(1) stall). ·
on-car · `MEMORY.md` §"FSG Sync & ExitView".

**Call `forceClusterRouteInfoState(true)` on start even for BAP-only routes** (sets
rgActive/rgiValid/favoredViewMode=KDK). · decompile · `memory/carplay_cluster_context_architecture.md`.

**Don't tear down RGI on `visible_in_app` (TLV 0x0F) == 0.**
0x0F = "guidance UI on screen", NOT route-active; iOS sends 0 for 3rd-party maps and when
the nav app isn't foreground, mid-route. Real end = `routeState==NO_ROUTE_SET`. · decompile
· `MEMORY.md` §"visible_in_app"; `RouteGuidance.java` @1d6cafd.

**Don't clear per-slot data on transient `maneuver_count=0`** (iOS sends it at route start;
clear only on `route_state=0` / `source_supports_rg=0`). · on-car · `MEMORY.md`.

**FctID18 distance and bargraph are INDEPENDENT — don't gate one on the other.**
Drop `!showBargraph` from the stock guard so both co-render (model 64=distance, 65=bargraph;
bargraph denom = `mDistance[firstIdx]`, guard −1/0). · decompile + on-car · `ClusterService.java:353`.

**LRU slot lookup must refresh seq on an existing-slot hit and guard active slots.**
Otherwise icon/turn-text stick. `MANEUVER_CACHE_SIZE=32` (not 8), MAX_MANEUVER_LIST=16 —
the "8 slots" comments are stale. · audit 9295b04 · `rgd_hook.c`, `rgd_tlv.h:220`.

**Waze will never drive cluster RGI — don't chase it as an HU bug.**
Waze never implements `mapTemplateShouldProvideNavigationMetadata:` → SourceSupportsRG=0;
AMap opts in but sends typeless maneuvers; no images ever cross iAP2. Waze altScreen zoom is
dropped *inside* Waze (not HU-fixable). · decompile · `memory/rgi_third_party_maneuvertype.md`.

**CMD_ALT_ZOOM is LIVE at 0x0110 — do not re-drop it** (correcting stale "DROPPED" notes;
`CMD_ALT_HID`/`CMD_KNOB` don't exist; cluster has no HID path). · audit 9295b04 · `MEMORY.md`.

**Keep the full 1440×542 view-area frame — a reduced visible frame is rejected at activation**
(root cause unknown; blocks zoom-via-viewarea). · on-car ·
`memory/carplay_updateviewarea_cluster_zoom.md`.

---

## AltScreen / video

**forceKeyFrame / cluster IDR must use `uuid`, NOT `displayUUID` (P0).**
iOS 26.6 reads `params["uuid"]` (displayUUID is a HID key) → cluster IDR falls back to the
primary screen → with showUI gated behind first IDR the cluster is permanently BLACK. Fix at
`altscreen_hook.c:1578`. · audit 9295b04 + decompile · `memory/production_full_audit_9295b04.md`.

**Set TCP_NODELAY on the hook→renderer socket; don't send [len][frame] as two writes.**
Two records on a Nagle socket held the sub-MSS AU → cluster video rubber-banding
(48→34 ms after fix). Don't probe Nagle on macOS — QNX runs an old NetBSD stack. · on-car ·
`memory/altscreen_delivery_nagle.md`.

**altScreen gate = the `"altScreen"` string in `/info` `extendedFeatures` → stream 111;
declare it in the dio patch, not libairplay.** extendedFeatures is a CFArray of key strings
(not a bitmask). · decompile · `memory/altscreen_ios26_gate_verified.md`.

**Cluster panel is 1440×542, NOT 1920×720** (wrong geometry mis-sizes the advert + OMX
window; the phone encodes 111 at the advertised size). · firmware qcdisplaycfg ·
`memory/cluster_panel_resolution.md`.

**`main_disp_modernize` must use the REAL ScreenCopyMain uuid — never fabricate.**
A fabricated main uid/UUID made iOS route main video on modern keys while the decoder waited
on the real UUID → main 110 BLACK. iOS fetches /info ONCE at session start. · on-car ·
`memory/reconnect_flak_lifecycle_debounce.md`.

**Don't remove `glFinish` / per-frame external-image rebind in altscreen_render "to look faster"**
— both are load-bearing (stale-UV / green-frame fix). · investigation ·
`docs/ALTSCREEN_FRAME_RATE_INVESTIGATION.md`.

**Error codes:** −16721 = empty cluster URL list (no cluster-capable app); −6740 =
advertised-but-unhandled; −6735 = viewArea before video up (self-heals). ·
`memory/altscreen_ios26_gate_verified.md`.

---

## Java / HMI

**Target Foundation Profile 1.1 (~J2SE 1.4): no generics/enum/enhanced-for/autoboxing/StringBuilder.**
Runtime is IBM J9 CDC1.1+FP1.1, `-noverify`, bytecode major 48; build `-source 1.4 -target
1.4`. · live core dump · `MEMORY.md` §"Java runtime".

**A 1.5 API method compiles then NoSuchMethodError on-unit — gate with the firmware JCL.**
`-target 1.4` limits bytecode, not API surface (e.g. `Class.getSimpleName`). Gate with
`Patches/jxe2jar/tools/recompile_check.py --jcl …/jcl.jar`; extract the .class from the
firmware jar to see the real class. · on-car · `MEMORY.md` §"method-level 1.5 API trap".

**`"isNativeLittleEndian"` is a decompiler artifact for `""` — and javac INLINES it.**
36 poisoned `static final String = "isNativeLittleEndian"` in the compile jar get inlined by
value, so a clean *source* grep misses them. After every Java build grep the JAR *bytecode*
(`unzip -p … '*.class' | strings | grep -c isNativeLittleEndian` == 0). · bytecode audit ·
`memory/isnativelittleendian_inlined_constant.md`.

**Canonical Java trees are `java_patch_new_replace` + `java_patch_new_tab` ONLY**
(`java_patch_new`/`java_patch` deleted 2026-07-29; the reconnect note saying otherwise is
stale). Build via `build_variant.sh`. · repo state · `MEMORY.md` §"Canonical Java trees".

**Renderers are always-on framework services — Java never spawns/kills them**
(BAPBridge only connects/reconnects; spawning races the supervisor). ·
`deploy/RENDERERS_AS_SERVICES.md`.

---

## RE methodology

**12-class supervised separation on 240 songs cannot work — the rear needs one mask, not twelve.**
The model collapsed to ≈ mute+drums (raw-scalar control == init; disabled == mute); the
band-power interface structurally can't separate lead/backing or keys/guitar. Mis-posed, not
under-trained. · independent Q2 audit · `memory/upmix_q2_independent_audit.md`,
`docs/research/HSSR_UPMIX_MODEL.md`.

**Loading exported int8 weights into PyTorch is NOT the shipped C inference — validate against the C path.**
Runtime = standardize→clamp±6→int8 GEMV with rational sigmoid/tanh + reset-after GRU. ·
training audit · `docs/HSSR_DATASET_TRAINING_AUDIT.md`, `docs/research/HSSR_UPMIX_MODEL.md §3`.

**Don't refuse MPS over 3e-8 nondeterminism — it made the 4-core NAS the real bottleneck.**
Research needs sealed weights after the fact, not bit-exact restart; the M3 Max (≈20–50× the
NAS) was refused purely on that self-imposed contract, so iteration scale — not rigor — was
the limit. · compute audit · `memory/upmix_compute_mac_vs_nas.md`.

**Don't fit amateur mic-sweep passbands as an EQ / filter bank.**
Reported speaker ranges are author observations; fitting rectangular passbands and applying
after the existing electronic EQ is an invented filter bank. They cross-check physical
grouping only. · workbook review · `docs/B9_SPEAKER_WORKBOOK_EVIDENCE_2026-09-20.md`.

**Dolby's kernel call period is unknown — delays are not convertible to ms; don't hard-code the ms table.**
If the real period is 1024 samples (not the 64-sample hypothesis) the delays are 64–660 ms
(not decorrelation) and the borrowed delay table must be revised. · Dolby teardown ·
`docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md §5`, `docs/research/DOLBY_ATMOS_TEARDOWN.md`.

**"Dolby is better than HSSR" has never been tested — don't cite it as a reason to switch.**
The Dolby principles were ported behind default-off flags, never A/B'd in-car against the
deployed HSSR. · distribution plan status · `docs/research/DOLBY_ATMOS_TEARDOWN.md`.

---

## Cross-cutting P0s (production audit @9295b04)

**Cold-boot deadlock: start modules OUTSIDE `CarPlayApp.lock`.**
ABBA inversion `CarPlayApp.lock` ↔ `AltScreenNavStatusGate.class` (startPass holds the lock
across `MODULES[i].start()` while NavigationJobs takes the Gate then the lock) → whole-HU
hang. · audit + direct read · `memory/production_full_audit_9295b04.md`,
`docs/PRODUCTION_FULL_AUDIT.md`.

**Cover-art was dead** (`NmeTransport::Recv` had no caller) and **forceKeyFrame used the wrong
key** (the cluster-black P0 above) — both from the same audit; re-run the audit's checks
before claiming those paths work. · `docs/PRODUCTION_FULL_AUDIT.md`.
