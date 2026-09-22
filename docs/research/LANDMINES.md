# LANDMINES — traps that already cost us time

*The rules that break, slow, or mislead — each with the symptom, how it was found, its provenance, and where the current code enforces it. Read this before touching a subsystem you don't own.*

---

## 📋 How to read this

Every entry is a **rule** (imperative) · what goes wrong · how it was found · provenance. Many are also distilled in `MEMORY.md` / `memory/*.md`; this is the one-page cross-project index. The source code is canonical: where a memory note disagrees with the code, the code wins and the conflict is flagged inline. Rules the code already obeys stay listed, with the enforcing `path:line`, so nobody undoes them. `[open]` marks a claim that could not be checked against the tree. Memory paths are under `~/.claude/projects/…/memory/`; repo paths are relative to the repository root.

*The thirteen trap families and where each one bites.*

```mermaid
mindmap
  root((Landmines))
    Build
      QNX TLS emutls
      host fp-contract off
      CFLite 3-arg ABI
      export allowlist
    Runtime
      QSA negative write
      foreign-buffer realloc
      env value not presence
      LOG_WARN threshold
    Deploy
      never reboot j9
      read-back verify
      atomic rename
    Network
      PF per-interface
      USB replug wedge
      clean dio exit
    Audio
      60 s fragments
      LFE is a bass edit
      qsa_play misleads
    Spatial
      cookie = authority
      AAC 5.1.2 not PCM
      phone-side gate
    Cluster
      id 103 dead
      single ctx writer
    RGI
      exitViewNum 0
      visible_in_app not route
    AltScreen
      uuid not displayUUID
      TCP_NODELAY
    Java
      Foundation 1.1
      inlined constants
    Methodology
      one mask not twelve
      validate C path
    Resolved P0s
      cold-boot deadlock
      cover-art seam
      cluster IDR key
```

---

## ⚙️ Build, toolchain and compile

**Never use `__thread` / `static __thread` in an LD_PRELOAD'd .so on QNX 6.5.** GCC emits emulated TLS (`__emutls_*`); any access from a thread that existed before the .so was preloaded crashes in libc — dio_manager has ~48 such threads, so the first RouteGuidance message is a deterministic SIGSEGV. Use heap or one static + mutex; `#define STBI_NO_THREAD_LOCALS`. · on-car crash 2026-05-12 + audit · `memory/qnx_tls_emutls_trap.md`. Enforced by the build: `scripts/build_hook_common.sh:162-164` (hook) and `:241-242` (APAC) fail on any `emutls` symbol.

**Build host-side float/DSP render tools with `-ffp-contract=off` (no fast-math).** The ARMv7 target has no FMA; a host compiler contracts `a·b+c` into one rounding and diverges bit-for-bit from the S16 reference. The on-target hook build passes only `-mfpu=neon -mfloat-abi=softfp` (`scripts/build_hook_common.sh:68`) — there is nothing to fuse there — so the flag lives in the host tools: `tools/stereo-upmix-render/bench/build_host.sh:9-11`, `tools/stereo-upmix-render/README.md:43-48`. · release runtime audit · [`docs/UPMIX_RELEASE_RUNTIME_AUDIT.md`](../UPMIX_RELEASE_RUNTIME_AUDIT.md).

**Declare stock CFLite exports with their real 3-arg ABI.** `CFArrayAppendCString(arr,str,-1)` and `CFDictionaryGetInt64(dict,key,NULL)` are 3-arg; a 2-arg proto leaves ARM r2 garbage → arbitrary overread / arbitrary write. · IDA decompile · `memory/cflite_abi_3arg_trap.md`. Current prototypes: `hook/altscreen/altscreen_hook.c:62,70`.

**Keep `.init_array` compiler-only (size 000004); no eager module constructors.** A constructor that reaches `LOG_INFO → pthread_create` under the QNX loader lock makes cold start depend on scheduler timing. · production audit @9295b04 · `memory/production_full_audit_9295b04.md`. Enforced: `scripts/build_hook_common.sh:228-229` (hook), `:249-250` (APAC).

**The hook's dynamic export set is an exact allowlist — don't widen it.** Hidden visibility + version script + an `nm -D` diff reject any accidental export (`scripts/build_hook_common.sh:192-220`); the build also rejects exception/RTTI symbols (`:166-168`), C++ runtime/STL symbols (`:170-172`) and libstdc++ in DT_NEEDED (`:174-176`). APAC is linked with GCC, not G++, and `-fno-threadsafe-statics` avoids the `__cxa_guard` this libstdc++ lacks (`scripts/apac_flags.sh:12-19`). · build discipline.

**Rust std for QNX 6.5 armv7 needs `+strict-align` (procnto runs SCTLR.A=1).** Unaligned loads SIGBUS — std's integer-format LUT read faults, so `println!("{}",n)` crashes while string printing works. · QEMU bring-up · `memory/rust_qnx65_armv7_feasible.md`.

**Go cross-built for QNX commits its heap reservation — patch `MAP_LAZY`.** QNX commits anonymous maps, so Go's 512 MB reservation is real RAM (`GOMEMLIMIT`/`GOGC` don't touch the startup reservation); reclaimed ~516 MB. Add `_qnxMAP_LAZY=0x80` in `runtime/mem_bsd.go`. · on-car FreeMem 183→704 MB · `memory/qnx_go_map_lazy.md`.

**C builds go through the Docker `qnx65-armv7-toolchain` image; pick the right tag.** `:4.9` = GCC 4.9.4 (C++11), `:8.5` / `:8.5-full` (= `:latest`) = GCC 8.5 (C++17, Go, Rust); stock 4.4.2/qcc is reference-only (`../../Tools/qnx-65-sdp-docker/README.md:16-20`). The hook build selects the tag via `QNX_TOOLCHAIN_VARIANT` (`scripts/build_hook_common.sh:49`). BSP libs export only on-unit → the scripts auto-generate import stubs.

---

## 🔧 QNX runtime

**Never return a negative from the QSA write interposer to stock dio.** Stock `CQNXSoundIF::write` treats any negative as an unsigned byte count → `memset(-47)` SIGSEGV in libc (our `-ECANCELED` crashed dio_manager). Failure → 0, cancel → full size, never negative. · QNX core dump 2026-09-19 · `memory/stock_qsa_write_negative_return_crash.md`; [`docs/AUDIO_CALL_ROUTING.md`](../AUDIO_CALL_ROUTING.md). Enforced by `spa_stock_write_result()` in `hook/audio/audio_hook.c:3769-3774`, used on every error return (e.g. `:6247-6293`).

**Never realloc/free a buffer owned by the hooked library unless its allocator is proven, and always keep the skip-on-doubt guard.** The Identify patcher once corrupted the heap and CarPlay wouldn't start (on-unit 2026-07-11). The current code *does* grow Cinemo's `NmeArray` with `realloc`, and documents why that is sound: `NmeIAP2Message::Encode` (libNmeBaseClasses `0x11ffe8`) grows the same buffer with libc `realloc`, and the patch runs inside the Encode interpose. Any failed invariant (NULL base, `len>cap`, size > 8192) skips the append with no heap touch (`hook/routeguidance/rgd_hook.c:1505-1545`). The older memory claim that the buffer is C++ `new[]` contradicts this code comment. · `memory/hook_identify_realloc_corruption.md`.

**Env gates must check the VALUE (`env_on()`), never bare `getenv()!=NULL`.** `export FOO=0` left the gate truthy → the feature stayed ON; cost a full debug session. On-unit env changes only take effect after a dio-supervisor relaunch; a redeployed `.so` is per-fork and needs none. · on-unit debug · `memory/hook_getenv_presence_check_trap.md`. Current helper: `hook/altscreen/altscreen_hook.c:700-712`.

**Instrument with `LOG_WARN`, not `LOG_INFO` — the live threshold is WARN.** Every `LOG_INFO` is dropped at runtime, so a new counter looks dead; cost a car session. Default `min_level = LOG_LEVEL_WARN` (`hook/framework/logging.h:72`); `touch /mnt/app/carplay_verbose` (or `/tmp/carplay_verbose`) lifts to INFO without a rebuild (`hook/framework/logging.c:187-198`); `LOG=0` compiles logging and captures out (`scripts/build_hook_common.sh:75`). · on-car · `memory/hook_log_is_warn_only.md`.

**Launch the renderers with `GRAPHICS_ROOT=/proc/boot` or EGL SIGSEGVs.** Adreno `libEGL` getenv's it; unset → null-deref in `eglGetDisplay`. This now applies to *both* renderers: `altscreen_render` presents through Screen/EGL too (`altscreen_render/screen_egl_present.c`), so the older "altscreen_render has no EGL" note is stale. The supervisor sets it for every renderer it spawns (`deploy/smartphone_integrator/carplay_monitor.sh:93`). Renderer /tmp logs need `setvbuf(_IONBF)`. · live test 2026-07-31 · `memory/graphics_root_egl_trap.md`.

---

## 📦 Deploy and SSH

**NEVER reboot / kill j9 / restart the unit yourself — ask every single time.** The user is usually in the car; killing j9 blacks the screen ~50 s. Approval to deploy is not approval to reboot, and approval once is not approval next time. · standing directive · `memory/feedback_never_reboot_unit.md`.

**Deploying `carplay_hook.jar` = a full unit reboot (j9 `-Xbootclasspath`).** The jar does nothing until the JVM restarts, and killing j9 is a critical-process death → whole-system restart (~50 s, SSH drops). Contrast the C hook: only dio_manager needs a new generation (per-fork, invisible). The build writes `build/carplay_hook.jar` (`scripts/build_java.sh:29`); on-unit path `/mnt/app/eso/hmi/lsd/jars/carplay_hook.jar` after `mount -uw /mnt/app`. · measured 2026-08-22 · `memory/java_jar_deploy_reboots_unit.md`.

**SSH to the head unit has no scp/tar/cksum on the far side — read back to verify.**

1. No remote `scp` / `tar` / `sha256sum`; push via `ssh "cat > f.new" < local` + chmod + mv + sync; verify by read-back + local `cmp`.
2. sshd rate-limits password auth → use one ControlMaster with a SHORT `ControlPath` (`/tmp/audi-cm/sock`, not a scratchpad path >104 B).
3. `ssh` in a `while read` eats the loop's stdin → `ssh -n`.
4. zsh won't word-split unquoted `$OPTS` → put deploy snippets in a `#!/bin/bash` script.

`/mnt/app` is read-only until `mount -uw`; the login banner prints on stdout even for `ssh host cmd`. · each cost a failed attempt · `memory/audi_ssh_transport_traps.md`.

**The password in `audi_ssh.sh` (outside this repo, at the `AUDI_2` root) is STALE — the working root password is supplied by the user; never write it into the tree.** Deploys failed on auth until the correct password was used. The old in-repo deploy helpers (`package_mib.py`, `package_protected.sh`, `deploy_over_ssh.sh`, `package_cabin_candidate.py`) were deleted. · `memory/audi_ssh_transport_traps.md`.

**Activate a release by same-partition rename, published last; renderers are persistent services.** The SI JSON is published last by a same-partition rename, which is atomic per file, not a transaction (`docs/DEPLOYMENT.md:48`). Don't add renderer processes as `servicemgr external:true` services — that breaks the generation model (`docs/DEPLOYMENT.md:30`). Never overwrite the stock `/etc/scripts/carplay_cleanup.sh`; the custom cleanup calls it only for mdnsd/PPS (`docs/DEPLOYMENT.md:27-28`, `deploy/smartphone_integrator/README.md:29-31`). · deployment design.

---

## 🌐 Networking, USB and connect

**The live PF ruleset is per-interface (`pf.ecm0.conf` on the LTE car) — verify via `pfctl -sr`.** Four profiles ship; patching `pf.conf` alone silently does nothing (6031/6032 stay blocked, no error). Patch all four; check the ACTIVE ruleset, not the file. · verified 2026-08-28 · `memory/pf_profile_per_interface_trap.md`. The installer widens `6030` → `6030:6032` in all four profiles (`install_MoreIncredibleBash/mod/custom.sh:35,95-100`).

**A fast USB replug wedges the device stack — CarPlay never returns until reboot.** The replug kills `io-usb-dcd`; `/dev/io-usb/otg_dev` goes missing → `carplay0` UP but not RUNNING (no carrier) → iAP2 never starts → dio misses its budget → SI relaunches it forever (~500 `TIMEOUT_STARTUP`). `ifconfig carplay0` without RUNNING = stack problem, not our code. · on-car 2026-09-03 · `memory/usb_device_stack_replug_wedge.md`, [`docs/USB_REPLUG_RECOVERY_RUNBOOK.md`](../USB_REPLUG_RECOVERY_RUNBOOK.md). Correction: there is no `start_otg_stack_device_mode.sh` in the tree and the production `carplay_startup.sh` contains no USB guard. The recovery candidates live in `deploy/experimental/usb_replug_recovery/` (NCM generation fence, `startncm.sh`/`stopncm.sh`, `usblauncher_otg.lua.patch`, exact-version bundle), which per its README are "offline, reversible candidates … nothing in this directory deploys to the head unit by itself" (`deploy/experimental/usb_replug_recovery/README.md:3-4`). Whether a unit runs them is [open].

**A clean dio exit spares the USB stack; ERROR/TIMEOUT makes SI destroy `carplay0`.** Only `ERROR_RUNTIME` / `TIMEOUT_*` / `ERROR_SETUP` exits escalate SI → `RestartDeviceStack` → `stopncm` → `carplay0` destroyed. A `DEINITIALIZED` exit runs `softPath_noStackRestart()`. ERROR/TIMEOUT ⇒ the fix belongs in dio/hook (exit cleanly), not the USB/firewall layer; diagnose via slog `[CSIStateRunning]` `oldAppState`. · IDA RE of SI · `memory/si_stack_restart_kills_carplay0.md`.

**Debounce the CarPlayApp inactive edge — a fast replug bounces state.** Each ACTIVATING↔INVALID↔ACTIVE edge used to do a full BAPBridge teardown + cluster ctx flap, and a dio reconnect landing mid-teardown collapsed the session. Implemented: `DEACTIVATE_DEBOUNCE_MS = 400L`; `lifecycleLoop` waits on an inactive edge and drops it if a newer edge arrives (`java_patch/com/luka/carplay/core/CarPlayApp.java:228-268`). Don't remove the wait. · on-car build 34c4c9d · `memory/reconnect_flak_lifecycle_debounce.md` (its "proposed" wording is stale).

**Don't re-investigate `-w 10000` or startncm en-dashes — proven harmless by decompile.** IPv6 DAD costs ~1 s/connect; the only lever is the global `net.inet6.ip6.dad_count` (set 0 before `start_network`). · decompile 2026-08-24 · `memory/ncm_connect_speed_stock.md`.

---

## ⚡ Audio — upmix tuning and measurement

**Never tune on 6-second prefixes; use ≥60 s fragments.** Short prefixes understate bass/mid tilt by ~1.5 dB mean (up to 4.8 dB); quiet intros attribute nothing. All 6 s conclusions were withdrawn. · 60 s one-variable grid · `memory/upmix_long_onevar_2026-09-21.md`, [`docs/UPMIX_LONG_ONEVAR_2026-09-21.md`](../UPMIX_LONG_ONEVAR_2026-09-21.md).

**Treat any LFE-default change as a bass edit and re-verify.** The 18.09 build silently raised the LFE assist default 4096→12288 = +9.5 dB sub — the "bass everywhere" complaint. Fixed via `AUDIO_STEREO_LFE_GAIN` (dB re 4096); production pins it to `0` (`deploy/smartphone_integrator/carplay_startup.sh:67-68`). · on-car root-trace 19.09 · `memory/upmix_second_scene_formulation.md`.

**LFE is not a monotonic sub-volume knob.** In the corrected linear chain LFE sums destructively with other bass — zeroing it can *raise* modelled sub. "Sub only from LFE / no LFE = silent sub" is false. · corrected re-render · [`docs/UPMIX_MATRIX_RERENDER_2026-09-20.md`](../UPMIX_MATRIX_RERENDER_2026-09-20.md).

**Front bass is the only bass lever, and it moves the modelled sub too.** Front bass −3→0 raised woofer +2.9 dB *and* sub +3.3 dB; the "front woofers only" comment is wrong. Retention does not touch bass (it's a mid/front restore). · one-variable grid · `memory/upmix_long_onevar_2026-09-21.md`.

**Retention lifts the front broadband — apply it with an HF taper, not flat.** Retention 0.5 raised the front +1.3 dB across 0.5–8 kHz (bright objects drag their treble forward). The 1000:2500 Hz taper is the *library default* (`hook/audio/stereo_upmix.h:42-43`, applied at `hook/audio/stereo_upmix.c:507`); production sets retention 0.5 and does not export `AUDIO_HSSR_RETENTION_HF`, which only overrides the default (`deploy/smartphone_integrator/carplay_startup.sh:75-82`). · deploy 2026-09-22 · [`docs/research/HSSR_UPMIX_MODEL.md`](HSSR_UPMIX_MODEL.md) §3.6, [`docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md`](../UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md) §7.

**The float "candidate" is a bass increase, not a tilt reduction — don't ship it.** Its −0.46 dB "tilt" was actually a +1.9 dB bass rise + mid restore + rear cut. The whole float/cabin/quiet path was deleted, S16 byte-identical. · re-attribution · `memory/upmix_long_onevar_2026-09-21.md`.

**The input-EQ bank choice dwarfs every other bass lever.** bank10 vs bank7 moved bass ~+6 dB — more than surround/front-bass/LFE combined; but the live bank isn't proven in-car, so don't draw bass conclusions without pinning it. · offline re-render · [`docs/UPMIX_MATRIX_RERENDER_2026-09-20.md`](../UPMIX_MATRIX_RERENDER_2026-09-20.md).

**Don't vectorize the branchy mask/slot loops.** They're ~60% of `run_hop` but per-bin with slot decisions; vectorizing means rewriting the decisions, which already flipped tracker decisions once. · profiling · [`docs/UPMIX_STATUS_2026-09-22.md`](../UPMIX_STATUS_2026-09-22.md) §5.

**The realtime deadline budget (8/10 ms) was invented, not measured.** There is no established free 75–80% budget; under contention P99 hit ~21.15 ms of the 21.333 ms period. Current in-car figures are estimates; the real period-time distribution is still uncaptured. · R1 budget audit · [`docs/UPMIX_R1_RUNTIME_BUDGET.md`](../UPMIX_R1_RUNTIME_BUDGET.md), [`docs/UPMIX_STATUS_2026-09-22.md`](../UPMIX_STATUS_2026-09-22.md).

**Don't trigger the realtime guard on real `deviceUnderruns`.** Correct in principle, but it reaches into the QSA record intercept that once crashed dio_manager, and can't be tested now. Stay on a time threshold; gather the histogram first. · design review · [`docs/UPMIX_STATUS_2026-09-22.md`](../UPMIX_STATUS_2026-09-22.md) §5.

---

## 🔧 Audio — amp and 5.1 routing

**Mirror `mpl5_dio_ent` caps to `mpl1` exactly — you cannot fix 5.1 from the PCM side.** `deva_update_session_info_from_list` copies the record *minima* (min_voices/min_fragsize) into the active CSD session, built at route-attach 14 s before the PCM channel exists; `2/0x1800` truncated each 6ch fragment and tore at 62.5 Hz. · IDA + on-car 2026-09-02 · `memory/carplay_51_fault_is_virtual_channel.md`.

**"Widening the minima breaks stereo" is FALSE — QSA inserts a channel conversion.** This belief cost a month; `mpl1` served stereo at `min==max==6` all along. · `memory/carplay_51_fault_is_virtual_channel.md`.

**Verify audio through CarPlay, never with `qsa_play`.** `qsa_play` sets no SAD, so it inherits the last session's `asad=0xc0` and plays L/R only; reading that as "amp innocent" sent a 2-day search into ADSP/MOST/SHARC — all correct. The fix was in the hook (announce `asad=0xfc`; see the SAD note at `hook/audio/audio_hook.c:5126-5130`). · 2 days lost · `memory/carplay_sad_header.md`, `memory/deva_stock_pcm_dump.md`.

**The amp input matrix is OUTPUT-rows / INPUT-columns — don't transpose it.** Older readings had it backwards; re-inserting the transpose shifts sub ~+15 dB. The per-speaker bass claims from the old orientation ("sub only from LFE", "front woofers sum L/Ls/C/LFE") are withdrawn — cite the corrected re-render for any bass/LFE decision. · static decompile 2026-09-20 · [`docs/SHARC_MATRIX_CONTROL_2026-09-20.md`](../SHARC_MATRIX_CONTROL_2026-09-20.md), [`docs/UPMIX_MATRIX_RERENDER_2026-09-20.md`](../UPMIX_MATRIX_RERENDER_2026-09-20.md).

**Internal (non-B&O) amp has NO 6-channel route — gate `AUDIO_SPATIAL` on E2P 0xFC.** E2P 0xFC==01 routes CarPlay media → `eSNK_SPEAKERS` stereo (BlockWidth 4); a PCM6 stream loses C/LFE/Ls/Rs and HSSR thins the fronts. `AUDIO_STEREO_UPMIX=off` alone still rides the 6-voice path. Read: `on -f rcc /usr/apps/modifyE2P r fc 1` (02=external). · RCC MIBRoot RE · `memory/internal_amp_no_51_path.md`, [`docs/INTERNAL_AMP_MIBROOT_ROUTING.md`](../INTERNAL_AMP_MIBROOT_ROUTING.md). The checked-in startup hardcodes `AUDIO_SPATIAL=1` for this (external-amp) unit and states the E2P gate "belongs to the installer" (`deploy/smartphone_integrator/carplay_startup.sh:56-60`); no installer code reads E2P 0xFC yet — [open] for internal-amp installs.

**`eSNK_SPEAKERS_MC` (=31) is a DEAD alias of stereo — never target it for 6ch.** 0 of 143 resource records, 0 connections; `CMainAmp` aliases it to stereo sink 1. The correct internal 6ch pair is `{0x46f, eSNK_DTCP51(2)}`. · audioprocess RE · `memory/internal_amp_no_51_path.md`, [`docs/INTERNAL_AMP_6CH_PATCH_PLAN.md`](../INTERNAL_AMP_6CH_PATCH_PLAN.md).

---

## 📊 Audio — spatial and codec wireformat

**The inline magic cookie is the codec authority — never switch decoders on a stream index field.** Reading TLV 8 (`spf`) or TLV 2 as the live format index overwrote the negotiated index on the first packet and muted every non-Atmos track. TLV 2 / SETUP index are advisory hints; drop unsupported ones, don't tear down a working decoder. · two vehicle captures · `memory/apap_no_format_index.md`. Code: TLV ids at `hook/audio/apap_crypto.h:53-67` (2 = format index, 8 = samples per frame, 18 = magic cookie); policy stated at `hook/audio/audio_hook.c:6-8`.

**CarPlay realtime spatial is AAC-ELD 48k 5.1.2 (bit 42), NOT PCM 5.1 (bit 44).** The phone never offers bit 44 on the realtime path; both bits were offered live and the phone chose stereo (0x8000) both times — advertising is not sufficient. `SPA_DEFAULT_BIT 44` still exists (`hook/audio/audio_hook.h:40-43`) but only as the retained realtime migration/debug path (`hook/audio/audio_hook.c:12-15`); shipped multichannel rides the buffered type-103 stream. · iOS 27 RE + live A/B · `memory/carplay_spatial_atmos_wireformat.md`, [`docs/reference/AUDIO.md`](../reference/AUDIO.md) §9-19.

**Phone-side realtime spatial is gated by `ScreenSpatialAudioReceiver` — no HU advert can flip it.** `APAudioFormatIsScreenSpatialAudioReceiver` returns false unless a phone-side feature flag/device model is set; it never reads the accessory declaration. · iOS 27 beta 6 RE · [`docs/HANDOFF_SPATIAL_AUDIO.md`](../HANDOFF_SPATIAL_AUDIO.md), `memory/carplay_spatial_atmos_wireformat.md`.

**The 8→6 fold is mandatory — there is no 8-channel route.** `deva_mib2_channel_map` switches on {1,2,3,6} only; `deva_csd_start_csd_session_playback` hardcodes 6; the amp declares positions 7/8 as BCL/BCR with no height codes, so 7.1.4 TopSide folds into the rears. The hook keeps QSA/SAD/CSD/MOST fixed at PCM6/48 kHz/S16 per session (`hook/audio/audio_hook.c:9-10`). · RE + on-car · `memory/carplay_sad_header.md`, `memory/carplay_spatial_atmos_wireformat.md`.

**210 libairplay is PCM-passthrough — don't add in-lib AAC/OMX/FDK decode.** `AudioConverterNew_compat` is a dead stub (−6735); 210 advertises PCM only; audio out is ALSA (OMX is video-only). Multichannel arrives through the hook's own buffered type-103 receive + optional APAC codec library instead (`hook/audio/audio_hook.h:102-122`, `libcarplay_apac.so`). · RE 2026-07-13 · `memory/audio_210_pcm_passthrough.md`, `memory/cinemo_audio_codec_abi.md`.

---

## 🔍 Cluster contexts

**DisplayManager id 103 is NOT creatable — reuse stock backings 101/102.** `createImageDisplayable(103)` silently no-ops (unregistered DSI id); reuse 101 (328×180) / 102 (210×153). The 101/102 constant *names* are inverted vs physical size. · on-car · `memory/carplay_cluster_context_architecture.md`. Current use: `java_patch/com/luka/carplay/cluster/ClusterLayerController.java:32-35`.

**Don't switch the altScreen advert off to fix a cluster flap — production ships it ON.** Earlier builds saw ctx 80↔74 flapping with the advert ON. The current code ships `ALTSCREEN_HANDLE_ALT=1` and `ALTSCREEN_ADVERT_DISPLAYS=1` (`deploy/smartphone_integrator/carplay_startup.sh:52-54`), and the advert is gated by `advert_preflight`, which proves the display tree can be fulfilled before `/info` publishes it (`hook/altscreen/altscreen_hook.c:714-722`). `ALTSCREEN_ADVERT_DISPLAYS=0` is the value-checked rollback. Whether the old flap is gone on the current build is a measurement question — [open]. · `memory/carplay_cluster_context_architecture.md`.

**`AltScreenModule` is the single cluster-context writer; the stock view-mode chain is dead on FPK.** Cluster map = FPK here, so `refreshViewMode`'s `if(!FPK&&!MMI)` guard skips `setFavoredViewMode(MAP)`. Two writers gave a black cluster / 30 s hang. In code, `republish()` computes 74 / 81 / 80 (`java_patch/com/luka/carplay/core/AltScreenModule.java:301-307`) and only `applySwitch` writes the DM (`:543-600`). The older "guarded by `i<80`" wording has no match in the current tree. · ClusterProbe 2026-08-01 · `memory/cluster_carplay_viewmode_design.md`, `memory/carplay_cluster_context_architecture.md`.

**`setActiveDisplayable` / encoder re-point fires only on a context CHANGE.** No-op without a ctx change, so the first grab of the cluster must bounce through a throwaway ctx. Code: bounce via ctx 72 only when the encoder is off (coming from 74 or unknown); 80↔81 switch directly (`java_patch/com/luka/carplay/core/AltScreenModule.java:553-571`). · decompile + on-car · `memory/lvds_video_pipeline.md` ([open]: that memory file is not present).

**KSS rejects MOST InfoStates=6 — patch KSSApplication @0x108F42C, or use the LVDS-map path.** `sub_108F42C` validation rejects 6 → VC never enters the compass/AIO-arrow FPK state. The prod LVDS-map path doesn't need it. · decompile · `memory/kss_infostates6_rejection.md`.

---

## 🔧 RGI and BAP

**Never send exitViewNum=1 — it overwrites the maneuver icon (keep 0).** · on-car A/B + decompile · `MEMORY.md` §"FSG Sync & ExitView". Code: `exitViewNum = 0` (`java_patch/com/luka/carplay/rgd/BAPBridge.java:126`).

**Toggle the exitView variant EU↔NAR on every send.** Unchanged variant+num → FctID49 skipped → the FSG sync never closes (sync(1) stall). · on-car · `MEMORY.md` §"FSG Sync & ExitView". Code: `sendExitView()` (`java_patch/com/luka/carplay/rgd/BAPBridge.java:2352-2361`), called on every descriptor send (`:1046-1051`).

**Call `forceClusterRouteInfoState(true)` on start even for BAP-only routes** (sets rgActive/rgiValid/favoredViewMode=KDK). · decompile · `memory/carplay_cluster_context_architecture.md`. Code: `java_patch/com/luka/carplay/rgd/BAPBridge.java:722,2070`.

**Don't tear down RGI on `visible_in_app` (TLV 0x0F) == 0.** 0x0F = "guidance UI on screen", NOT route-active; iOS sends 0 for 3rd-party maps and when the nav app isn't foreground, mid-route. Real end = `routeState==NO_ROUTE_SET`. · decompile · `MEMORY.md` §"visible_in_app". Code: `java_patch/com/luka/carplay/rgd/RouteGuidance.java:492-518`.

**Don't clear per-slot data on transient `maneuver_count=0`.** iOS sends it at route start; drop only the cached order, and clear slots only on `route_state=0` / `source_supports_rg=0`. · on-car · `MEMORY.md`. Code: `java_patch/com/luka/carplay/rgd/RouteGuidance.java:961-990`.

**During CarPlay, keep the FctID18 distance model valid alongside the bargraph.** The stock guard `!showBargraph` is kept for stock navigation but bypassed while CarPlay is connected: `!this.showBargraph || AltScreenModule.isConnected()` (`java_patch/de/audi/tghu/navi/app/cluster/ClusterService.java:377-386`). Correction: the memory note says the guard was dropped outright at `:353` — it wasn't. The same comment warns that the VC firmware independently hides distance when `BargraphOn != 0` ([`docs/reference/DISTANCE_BARGRAPH_CHAIN.md`](../reference/DISTANCE_BARGRAPH_CHAIN.md)), so "both always co-render on the cluster" is not guaranteed by the HU. Model 64=distance, 65=bargraph; bargraph denom = `mDistance[firstIdx]`, guard −1/0. · decompile + on-car.

**LRU slot lookup must refresh seq on an existing-slot hit and guard active slots.** Otherwise icon/turn-text stick. Code: `rgd_is_active_index()` (`hook/routeguidance/rgd_hook.c:304`), `rgd_slot_for_iap_index()` with the active-slot eviction guard (`:339-367`). `MANEUVER_CACHE_SIZE=32` (`hook/routeguidance/rgd_tlv.h:230`), `MAX_MANEUVER_LIST=16` (`:219`). · audit 9295b04.

**Waze will never drive cluster RGI — don't chase it as an HU bug.** Waze never implements `mapTemplateShouldProvideNavigationMetadata:` → SourceSupportsRG=0; AMap opts in but sends typeless maneuvers; no images ever cross iAP2. Waze altScreen zoom is dropped *inside* Waze (not HU-fixable). · decompile · `memory/rgi_third_party_maneuvertype.md`.

**`CMD_ALT_ZOOM` is LIVE at `0x0110` — do not re-drop it.** `hook/framework/bus_protocol.h:92`, `java_patch/com/luka/carplay/bus/CarplayBus.java:54`, handler `zoom_on_cmd` registered at `hook/altscreen/altscreen_hook.c:2635`. `CMD_ALT_HID` / `CMD_KNOB` don't exist (only a stale comment in `hook/framework/bus.c:548` still names `CMD_KNOB`); the cluster has no HID path. · audit 9295b04.

**Keep the full 1440×542 view-area frame — a reduced visible frame is rejected at activation** (root cause unknown; blocks zoom-via-viewarea). · on-car · `memory/carplay_updateviewarea_cluster_zoom.md`.

---

## ⚡ AltScreen and video

**forceKeyFrame / cluster IDR must be keyed by `uuid`, NOT `displayUUID`.** iOS 26.6 reads `params["uuid"]` (displayUUID is a HID key) → a mis-keyed cluster IDR falls back to the primary screen → with showUI gated behind the first IDR the cluster stays BLACK. Fixed: `alt_request_cluster_idr()` builds params with `alt_params_new(1)`, which sets `uuid = CLUSTER_DISPLAY_UUID` (`hook/altscreen/altscreen_hook.c:2367,2502-2523`). · audit 9295b04 + decompile · `memory/production_full_audit_9295b04.md`.

**Keep TCP_NODELAY on the hook→renderer socket.** The hook still writes each frame as two records, `[u32 BE len]` then the access unit (`hook/altscreen/altscreen_av.c:222-250`); with Nagle on, the sub-MSS AU waited on the delayed-ACK timer → cluster rubber-banding (48→34 ms after the fix). `TCP_NODELAY` is set at `hook/altscreen/altscreen_av.c:135-142`; dropping it (or it failing) brings the stall back unless the writes are coalesced. Don't probe Nagle on macOS — QNX runs an old NetBSD stack. · on-car · `memory/altscreen_delivery_nagle.md`.

**altScreen gate = the `"altScreen"` string in `/info` `extendedFeatures` → stream 111.** extendedFeatures is a CFArray of key strings (not a bitmask). Correction: it is *not* declared by a dio patch or via SetProperty — stock 210 libairplay has no `extendedFeatures` support, so the hook interposes `AirPlayCopyServerInfo` and adds `["altScreen","viewAreas"]` to the returned dictionary (`hook/altscreen/altscreen_hook.c:11-17,184`). · decompile · `memory/altscreen_ios26_gate_verified.md`.

**Cluster panel is 1440×542, NOT 1920×720** (wrong geometry mis-sizes the advert + OMX window; the phone encodes 111 at the advertised size). Code: `CLUSTER_W/H` (`hook/altscreen/altscreen_hook.c:226-230`). · firmware qcdisplaycfg · `memory/cluster_panel_resolution.md`.

**`main_disp_modernize` must use the REAL ScreenCopyMain uuid — never fabricate.** A fabricated main uid/UUID made iOS route main video on modern keys while the decoder waited on the real UUID → main 110 BLACK. iOS fetches /info ONCE at session start. Code: `hook/altscreen/altscreen_hook.c:367,547-551`. · on-car · `memory/reconnect_flak_lifecycle_debounce.md`.

**Don't remove `glFinish` / per-frame external-image rebind in altscreen_render "to look faster"** — both are load-bearing (stale-UV / green-frame fix). `glFinish` now sits in `altscreen_render/screen_egl_present.c:993`. · investigation · [`docs/ALTSCREEN_FRAME_RATE_INVESTIGATION.md`](../ALTSCREEN_FRAME_RATE_INVESTIGATION.md).

**Error codes:** −16721 = empty cluster URL list (no cluster-capable app); −6740 = advertised-but-unhandled; −6735 = viewArea before video up (self-heals). · `memory/altscreen_ios26_gate_verified.md`.

---

## 🔧 Java and HMI

**Target Foundation Profile 1.1 (~J2SE 1.4): no generics/enum/enhanced-for/autoboxing/StringBuilder.** Runtime is IBM J9 CDC1.1+FP1.1, `-noverify`, bytecode major 48. Build: `-source 1.4 -target 1.4` (`scripts/build_java.sh:62`). · live core dump · `MEMORY.md` §"Java runtime".

**A 1.5 API method compiles then NoSuchMethodError on-unit — gate with the firmware JCL.** `-target 1.4` limits bytecode, not API surface (e.g. `Class.getSimpleName`). Gate with `Patches/jxe2jar/tools/recompile_check.py --jcl …/jcl.jar`; extract the .class from the firmware jar to see the real class. · on-car · `MEMORY.md` §"method-level 1.5 API trap".

**`"isNativeLittleEndian"` is a decompiler artifact for `""` — and javac INLINES it.** 36 poisoned `static final String = "isNativeLittleEndian"` in the compile jar get inlined by value, so a clean *source* grep misses them. After every Java build grep the JAR *bytecode* (`unzip -p … '*.class' | strings | grep -c isNativeLittleEndian` == 0); `scripts/build_java.sh` does not automate this check. · bytecode audit · `memory/isnativelittleendian_inlined_constant.md`.

**The only Java source tree is `java_patch/`.** `scripts/build_java.sh:25` sets `SRC_DIR="$PROJECT_DIR/java_patch"`; `java_patch_new_replace`, `java_patch_new_tab` and `build_variant.sh` do not exist. The `MEMORY.md` §"Canonical Java trees" entry is stale; the reconnect note that names `java_patch/` as live is correct. · repo state.

**Renderers are persistent supervised services — Java never spawns or kills them.** `carplay_startup.sh` forks `carplay_monitor.sh`, which (re)starts `maneuver_render` / `altscreen_render` (`deploy/smartphone_integrator/carplay_monitor.sh:85-95`). Java only listens: `RendererServer` on :19800 for `maneuver_render` (`java_patch/com/luka/carplay/rgd/RendererServer.java:44`) and `AltscreenControlServer` on :19832 (`java_patch/com/luka/carplay/core/AltscreenControlServer.java:32`); the renderers connect in. No `Runtime.exec`/`ProcessBuilder` exists under `java_patch/`. See the bus/renderer wire map in `hook/framework/bus_protocol.h`.

---

## 🔍 RE methodology

**12-class supervised separation on 240 songs cannot work — the rear needs one mask, not twelve.** The model collapsed to ≈ mute+drums (raw-scalar control == init; disabled == mute); the band-power interface structurally can't separate lead/backing or keys/guitar. Mis-posed, not under-trained. · independent Q2 audit · `memory/upmix_q2_independent_audit.md`, [`docs/research/HSSR_UPMIX_MODEL.md`](HSSR_UPMIX_MODEL.md).

**Loading exported int8 weights into PyTorch is NOT the shipped C inference — validate against the C path.** Runtime = standardize→clamp±6→int8 GEMV with rational sigmoid/tanh + reset-after GRU. · training audit · [`docs/HSSR_DATASET_TRAINING_AUDIT.md`](../HSSR_DATASET_TRAINING_AUDIT.md), [`docs/research/HSSR_UPMIX_MODEL.md`](HSSR_UPMIX_MODEL.md) §3.

**Don't refuse MPS over 3e-8 nondeterminism — it made the 4-core NAS the real bottleneck.** Research needs sealed weights after the fact, not bit-exact restart; the M3 Max (≈20–50× the NAS) was refused purely on that self-imposed contract, so iteration scale — not rigor — was the limit. · compute audit · `memory/upmix_compute_mac_vs_nas.md`.

**Don't fit amateur mic-sweep passbands as an EQ / filter bank.** Reported speaker ranges are author observations; fitting rectangular passbands and applying after the existing electronic EQ is an invented filter bank. They cross-check physical grouping only. · workbook review · [`docs/B9_SPEAKER_WORKBOOK_EVIDENCE_2026-09-20.md`](../B9_SPEAKER_WORKBOOK_EVIDENCE_2026-09-20.md).

**Dolby's kernel call period is unknown — delays are not convertible to ms; don't hard-code the ms table.** If the real period is 1024 samples (not the 64-sample hypothesis) the delays are 64–660 ms (not decorrelation) and the borrowed delay table must be revised. · Dolby teardown · [`docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md`](../UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md) §5, [`docs/research/DOLBY_ATMOS_TEARDOWN.md`](DOLBY_ATMOS_TEARDOWN.md).

**"Dolby is better than HSSR" has never been tested — don't cite it as a reason to switch.** The Dolby principles were ported behind flags and never A/B'd in-car against plain HSSR. Note that production now enables the bed-delay/attack pieces (`AUDIO_HSSR_BED_DELAYS=on`, `AUDIO_HSSR_BED_ATTACK=owner`, `deploy/smartphone_integrator/carplay_startup.sh:71-78`) after a headphone A/B ("chosen by Luka after the headphone A/B", `:71-72`), not after an in-car measured comparison. · distribution plan status · [`docs/research/DOLBY_ATMOS_TEARDOWN.md`](DOLBY_ATMOS_TEARDOWN.md).

---

## ✅ Resolved P0s (production audit @9295b04)

The full audit found three shipping blockers. All three are fixed in the current code; each stays here as a durable rule so it isn't reintroduced.

| P0 | Symptom | Rule | Fixed in code |
|---|---|---|---|
| Cold-boot deadlock | Whole-HU hang at start (ABBA `CarPlayApp.lock` ↔ `AltScreenNavStatusGate.class`) | Never hold `CarPlayApp.lock` across module `start()` or any external call | Lifecycle runs on a separate `lifecycleLock` worker; module start/stop happens outside `CarPlayApp.lock` (`java_patch/com/luka/carplay/core/CarPlayApp.java:38-39,184-222,328-331`) |
| Cover-art dead | No album art | The `NmeTransport::Recv` seam must have a live caller | Framework Recv interposition feeds `coverart_on_transport_recv` (`hook/coverart/coverart_hook.c:692-694,768,828`) |
| Cluster permanently black | Cluster never shows | Key cluster IDR by `uuid`, never `displayUUID` | `alt_params_new(1)` sets `uuid` (`hook/altscreen/altscreen_hook.c:2367,2502-2523`) |

Provenance: `memory/production_full_audit_9295b04.md`. `docs/PRODUCTION_FULL_AUDIT.md` is no longer in the tree — [open] where the audit write-up now lives.

---

## 🔗 See also

- [Deployment](../DEPLOYMENT.md) — release activation, supervisor and renderer ownership.
- [Supervisor README](../../deploy/smartphone_integrator/README.md) — the production child wrapper and audio policy.
- [AUDIO reference](../reference/AUDIO.md) — the spatial/codec RE behind the audio groups.
- [HSSR upmix model](HSSR_UPMIX_MODEL.md) — the model and runtime behind the upmix group.
- [USB replug runbook](../USB_REPLUG_RECOVERY_RUNBOOK.md) — diagnosis and per-component A/B for the replug wedge.
