# Scene Backend Deploy

*The scene upmix renderer is the only stereo renderer of the hook since 2026-10-09: the startup block is live (variant J, fine mask net c loaded), there is no selector flag, and the Universal backend with its three models is gone from the tree. This page is the package, the measured cost, the two A/B levers, the rollback, and what to read in the log after the first car session. First car session 2026-10-09; the owner's verdict of 2026-10-10 accepts the sound (one sighted listener, no blind test). The resume fix and the cluster fix of the follow-up package are still to be checked in the car.*

---

## 📦 What is in the package

`scripts/prepare_install.sh build/unprotected` output, assembled by the usual path ([Deploy and Release](DEPLOY_AND_RELEASE.md)): the same tree as every package since 2026-09-22 with one model file, the mask blob, and none of the HSSR models. <!-- >>> protected -->Unprotected (no VIN lock); a VIN-locked copy is `scripts/build_hook_protected.sh <VIN>` then `prepare_install.sh build/protected_<VIN>` (the blob line is in `prepare_install.sh:48`, so it ships there too, [VIN Protection](VIN_PROTECTION.md)).<!-- <<< protected -->

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `build/install_scene_j_unprotected.zip` (2026-10-09 18:42) | 2,253,177 | `74fbad0aa801fff78d4f155a1aeeb1c9f8a45758fbfc9d2a64b3d391faef54b1` |
| `libcarplay_hook.so` (`build/unprotected/` of 18:42; the hook heard in the car on 2026-10-09, shipped there in `install_unprotected_scene_j_2026-10-09_2107.zip`) | 1,441,797 | `33b6ee9beb3d385dfe16249544540084740092250de1df344bd0784695fff348` |
| `libcarplay_apac.so` (unchanged since 2026-09-22) | 696,491 | `1c25799f4dd47549335cbc22cc9a059efab030d4e489937fc4d8460e341e09ce` |
| `scene_masknet_c.bin` (`models/scene-masknet-c/`) | 32,544 | `58cdc72f573d4718f6325de8a481ecd904096d8f589372e6de5cb05222d98957` |

- **Hook:** `scripts/build_hook.sh` (Docker `qnx65-armv7-toolchain:latest`, GCC 8.5, `-mfpu=neon -mfloat-abi=softfp`, the upmix DSP files at `-O3 -fno-math-errno`, `scripts/apac_flags.sh`). The renderer is `hook/audio/scene_engine.c/.h`, `scene_engine_math.c`, `scene_simd.h`, `scene_fft.h`, `scene_clusters.c/.h`, `scene_mask.c/.h`, `scene_mask_worker.c/.h` behind the `SCENE` backend of `stereo_upmix.c/.h` (`enum spa_stereo_upmix_backend`: `OFF = 0`, `SCENE = 1`); `audio_hook.c/.h` parse the knobs. The file is 1.4 MB: the type-103 render state `spa103_render` (801,520 B: engine, net and the worker's own copy of the net) is a static initializer in `.data`, the realtime instance `spa_rt_upmix` (801,408 B) is in `.bss` and costs nothing on disk (`llvm-nm -S`). The 2.1 MB build of the same morning still carried the Universal backend. `strings libcarplay_hook.so | grep -c '^AUDIO_SCENE'` prints 10 (the ten knob names), `grep -c '^AUDIO_HSSR'` prints 0.
- **Mask blob:** `tools/stereo-upmix-render/port/export_masknet_weights.py` over the fixtures' `masknet_oracle.npz` (the fine net c weights, [spec section 16](../audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md)); `SMSKNET1`, u32 24/12/4, 8131 float32, the file image of `struct scene_mask_weights`; the NEON runtime re-lays the weights in tile order at `scene_mask_init()`. Provenance and the hook-vs-torch numbers in `models/scene-masknet-c/manifest.json`. On the unit: `/mnt/app/root/hooks/scene_masknet_c.bin`, read once at audio init because `AUDIO_SCENE_MASK_BLOB` names it.
- **Everything else** is what every package ships: both jars, both renderers, `flag_atlas.rgba`, the SI image, `deva-ctrl-qc.so`, the NCM fence, the five `carplay_*.sh`. `scripts/check_model_settings.py` compares `models/scene-masknet-c/settings.json` (the four knob values the net's operating point was measured at) with the startup file.

### Follow-up package (2026-10-10 00:19)

The same tree plus the resume fix `a00959cc` (no upmix reset on a buffered pause, reset on APAP sequence gaps, one period of silence pre-rolled after every QSA prepare; [Buffered Audio Path](../audio/BUFFERED_AUDIO_PATH.md)) and the cluster fix `9305604d` (a steady compressed backlog keeps the picture, renderer at priority 15, monotonic 3 s input wait, 250 ms greeting window; [Video Pipeline](../altscreen-and-video/VIDEO_PIPELINE.md)). This is the package for the next drive. The hook takes effect at the next `dio_manager` start and the renderer at the next reboot or monitor respawn; both are the owner's to trigger.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `build/install_unprotected.zip` | 2,254,035 | `e91df62536c9a315bc3b41e9caddeccfd35b10b3294cb863ddc92f0c3abde884` |
| `libcarplay_hook.so` in it | 1,454,289 | `da7a4ae283f51994e73a0a833a39167796a98bbd3588bae0458c9748152fc6da` |
| `altscreen_render` in it | 314,546 | `9a033ffbe71987db8063233cb98f94131d7d538b0c9852b9e46428b4ea1bba48` |
| `maneuver_render` in it (unchanged) | 176,559 | `6fb8316adced07a5cf90cbb96de5aa489b2a550ec351719d32403cb88e701ac7` |

## 📊 Measured cost (QEMU, not the car)

QEMU `-icount` retired ARMv7 instructions, GCC 8.5, measured on the sources of this package (`tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh`, `--bench mask` for the net; [bench README](../../tools/stereo-upmix-render/bench/README.md)). "Before" is the first port of the morning, stage 3 of [spec section 18](../audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md); the targets were engine ≤ 1.2 M per hop, net ≤ 1.5 M per control and off the audio thread.

| What | Before | `-O2` | `-O3 -fno-math-errno` (the shipped flags) |
|---|---:|---:|---:|
| engine, per 256-frame hop, with a mask row (`--mask`) | 830,111 | 344,382 | 326,746 |
| engine, per 256-frame hop, spatial only | — | 313,679 | — |
| mask net, per 512-frame control (`scene_mask_bench`) | 19,362,951 | 1,368,181 | 1,375,745 |
| audio thread's hand-off to the worker, per 768-frame push (`--mask --handoff` minus `--mask`) | — (net on the audio thread) | +1,088 | — |

Per 768-frame QSA period the audio thread runs 0.98 M instructions for the engine at the shipped flags (p99 0.99 M, worst push 1.22 M over 30 s; the flags change no result, same S16 checksum) plus 1.1 k for the hand-off; the net's 1.37 M per 10.7 ms control runs on the worker. The one QEMU-instructions-to-Krait-milliseconds calibration so far (the Universal runs) scales these to **1.1–1.8 ms per period on the audio thread** (spatial only 1.1–1.7 ms) and 1.6–2.5 ms per control on the worker, against the 12 ms realtime guard; with `AUDIO_SCENE_MASK_SYNC=1` (net on the audio thread) 3.0 M per period, 3.5–5.6 ms. The guard histogram on the unit is the real number. Fixture gates on the same sources, all green: engine −125.8 / −131.3 dB of the output RMS against the Python reference with 1000- and 768-frame blocks bit-identical (`port/test_engine.py`); net 4.2e-4 vs torch on the C features and 5.2e-3 end to end against the goldens, whose own float32 noise is 4.2e-3 (`port/test_mask.py`); the hook against `stream_render` on the net's own rows −71.7 dB, worst channel −65.0 dB (`port/test_hook_render.py`). Bench checksums for a re-run: net `0xedf9005e82dae1ce` (10 s, `-O2` and `-O3` identical), engine `--mask` `0x0421d6cca340373e` (30 s; 60 s `0xace5f91925bec549`) and spatial `0xd96e0a0d0b33b0b5` (30 s, `-O2`).

## ⚙️ The live block

`deploy/smartphone_integrator/carplay_startup.sh:77-99` (on the unit `/mnt/app/root/hooks/carplay_startup.sh`, on the card `mod/carplay/root/mnt/app/root/hooks/carplay_startup.sh`), right before the `AUDIO_REALTIME_UPMIX` line:

```sh
export AUDIO_SCENE_MASK_BLOB="$H/scene_masknet_c.bin"
export AUDIO_SCENE_REAR_DELAY_MS=0
export AUDIO_SCENE_INST_GAMMA=1
export AUDIO_SCENE_INST_ALPHA=0.9
export AUDIO_SCENE_DUCK_DEPTH=0.25
export AUDIO_SCENE_ALPHA=0.7
export AUDIO_SCENE_SIDE_LP_HZ=7000
export AUDIO_SCENE_BLEED_CANCEL=0
export AUDIO_SCENE_FRONT_KEEP_DB=1
```

`AUDIO_STEREO_UPMIX=on` above the block (`:73`) is what selects the renderer; there is no `AUDIO_SCENE` flag any more. The environment is read once when `dio_manager` starts (SI execs `carplay_startup.sh` as the CarPlay child), so an edit on the unit applies at the next child start, in practice the next HU boot. The owner restarts the unit; the tooling never does ([Landmines](../LANDMINES.md)).

The knobs (`hook/audio/audio_hook.h:75-96`), all engine units, each clamped to its range with an `audio config: invalid …` line and the default on a bad value. The first seven values in the block are the engine defaults (`scene_config_defaults()`): removing them changes nothing (`AUDIO_SCENE_FRONT_KEEP_DB` is not: the engine default is 0, off); they exist so the running configuration can be read from the file.

| Knob | Value in the block | Range | What it is |
|---|---|---|---|
| `AUDIO_STEREO_UPMIX` | `on` (above the block) | on/off | on = the scene renderer; off = the exact L/R bypass, which wins over everything below |
| `AUDIO_SCENE_MASK_BLOB` | `$H/scene_masknet_c.bin` | path | the net; unset, unreadable or rejected = spatial only, logged once |
| `AUDIO_SCENE_MASK_SYNC` | not in the block | 0/1 | default 0: the net on the scene mask worker (`hook/audio/scene_mask_worker.h`, SCHED_RR 17), its rows applied one control (512 frames, 10.7 ms) later than on the audio thread, controls skipped instead of waited for when it falls behind; `1` runs it on the audio thread, the exact timing the port tests check |
| `AUDIO_SCENE_REAR_DELAY_MS` | `0` | −50..50 | variant J: our rear and the amp's own front bleed meet the rear doors in phase; the amp's rear bus supplies the ~11.8 ms lag `[model]` |
| `AUDIO_SCENE_INST_GAMMA` | `1` | 0.1..4 | eligibility exponent, 1 = full rear (kit letters I/J); 2 = the balanced fallback H |
| `AUDIO_SCENE_INST_ALPHA` | `0.9` | 0..1 | instrument transfer fraction |
| `AUDIO_SCENE_DUCK_DEPTH` | `0.25` | 0..1 | duck of the spatial transfer and the side layer by the protect mask (needs the blob) |
| `AUDIO_SCENE_ALPHA` | `0.7` | 0..1 | spatial relocation fraction |
| `AUDIO_SCENE_SIDE_LP_HZ` | `7000` | 0 = off, 1000..24000 | side-layer low-pass |
| `AUDIO_SCENE_BLEED_CANCEL` | `0` | 0..0.5 | variant K's bleed cancel (0.355); 0 = J |
| `AUDIO_SCENE_FRONT_KEEP_DB` | `1` | 0 = off, 0.5..12 | front anchor: the most all transfers may take from the front over ~1 s ([Stereo to 5.1](../audio/STEREO_TO_5_1_UPMIX.md#front-anchor)) |
| `AUDIO_SCENE_SIGMA_MIN` | not in the block | 0.01..1 | default 0.08; `0.15` is the documented second candidate ([spec section 15](../audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md)) |

What still applies on top of the engine's own levels: `AUDIO_STEREO_LFE=assist` with `AUDIO_STEREO_LFE_GAIN` (LFE V2, steered by the net's bass/drum posteriors when the blob is loaded) and the J525 cabin guard. `AUDIO_REALTIME_UPMIX=on` means the scene renderer also runs on the realtime path, under the guard. The Universal knobs (`AUDIO_STEREO_SURROUND`, `FRONT_BASS_DB`, `FRONT_TREBLE_DB`, `REAR_BODY_DB`, `REAR_AIR_DB`, `FRONT_RETENTION`, every `AUDIO_HSSR_*`) are not parsed any more; a leftover export is ignored.

## 🔬 The two A/B levers

1. **Spatial only** — comment out the `AUDIO_SCENE_MASK_BLOB` line. The scene renderer without the net: no instrument transfer, no duck, LFE V2 steered from the front alone; no `SCENE: mask net ready` line, and the render line shows `sceneMask=none`. Audio-thread cost 0.31 M instructions per hop. This is the spatial-vs-semantic comparison of the car kit (letters I/J against the scene-without-a-net condition).
2. **Net on the audio thread** — `export AUDIO_SCENE_MASK_SYNC=1`. The exact timing of the port tests (rows not one control late), 3.0 M instructions per period; an A/B against the worker's lag, not the default.

The listening order and the kit letters are in [spec sections 15 and 18](../audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md): the car first (I / H / scene without a net), then the knobs one at a time. `tools/stereo-upmix-render/scene_car_kit.py` renders the blinded 6-channel files for `qsa_play`.

## 🚗 The first car session and the owner's verdict

Drive of 2026-10-09, 22:40–23:30, hook `33b6ee9b…` deployed over SSH (the jar of the same package was not active: it loads only after a reboot, which the owner does). `[car]`

| Path | What the log showed |
|---|---|
| realtime type 100 (Spotify-class apps), 19 min | mean 2.0 ms / worst 6.0 ms per 16 ms period, guard never tripped (`disables=0`); worker rows 106,490, `skipped=1`, about one row in three superseded by the next (the 768-vs-512 cadence) |
| buffered type 103, AAC index 23 | `stereoUpmix=scene sceneMask=worker`, centre silent, LFE assist about −29 dBFS, Ls/Rs at 60–80 % of the front on mid-heavy content, `drop=0` |
| Atmos tracks (index 65) | bypass the upmix as designed |
| pause → resume on type 103 | a small "rip" heard by the owner, one `prepare+retry` per resume in the log; root cause and fix in [Buffered Audio Path](../audio/BUFFERED_AUDIO_PATH.md), shipped in the follow-up package above; play/pause without a rip confirmed by the owner on 2026-10-14 `[car]` |

The owner's verdict on 2026-10-10, after that drive: **the SCENE upmix sounds good, "really good"**. One sighted listener, no blind test, no level-matched A/B against `AUDIO_STEREO_UPMIX=off`; the blinded six-condition car kit was not played. This replaces the 2026-09-22 Track A acceptance as the current acceptance source. `[car]`

On 2026-10-14 the owner switched the upmix on and off at the same volume and judged that it works as intended; still sighted and informal, not the blinded kit. The same drive showed no cluster blink with `9305604d` and no rip on play/pause. `[car]`

_Timeline of the scene renderer's path into the car: from the first wired package through the first drive, the two static fixes and the owner's verdict, to the 2026-10-14 confirmations. Timelines carry no accTitle/accDescr._

```mermaid
timeline
    title 🚗 Scene renderer in the car
    section 2026-10-09
        18:42 : 📦 first SCENE package : hook 33b6ee9b
        21:07 : 🚀 deployed over SSH : jar waits for a reboot
        22:40 : 🎧 first drive : realtime worst 6 ms : 103 resume rip
        23:43 : 🔧 resume fix a00959cc
    section 2026-10-10
        00:19 : 📦 package e91df625 : cluster fix 9305604d
        verdict : ✅ owner : sounds good, really good
    section 2026-10-14
        owner : ✅ play and pause without a rip : ✅ cluster without a blink : ✅ upmix on and off at one volume : ✅ UpMix level row works
```

## 🔍 What to read in the log after the first car session

The hook log is `/tmp/carplay_hook.log`, WARN level by default (`touch /mnt/app/carplay_verbose` for everything). Exact strings:

| Line | Meaning |
|---|---|
| `SCENE: mask net ready path=/mnt/app/root/hooks/scene_masknet_c.bin bytes=32544` | the blob passed size, magic and dimensions (once, at audio init) |
| `SCENE: mask blob unreadable path=… errno=…; running without a mask` | wrong path or mode: spatial only |
| `SCENE: mask blob rejected path=… bytes=… (expected 32544 bytes of SMSKNET1); running without a mask` | truncated or wrong file: spatial only |
| `buffer103: render … stereoUpmix=scene frontGainQ15=31392 sceneMask=worker stereoLfe=assist lfeGainQ15=4096 …` | the buffered path's configuration (INFO); `sceneMask` is `worker`, `sync` (`AUDIO_SCENE_MASK_SYNC=1`), `none` (no blob) or `off` (bypass) |
| `audio realtime upmix: armed (backend=scene init=0 period=768 frames, scene_mask=worker)` | the type-100 path armed (`AUDIO_REALTIME_UPMIX=on`) |
| `audio upmix 103\|rt: scene mask worker stopped rows=A/B skipped=N stale=… superseded=… resets=…` | at stream stop, per path: B controls handed to the worker, A run by the net, **N refused** because it still held the two before (the net slower than realtime); N stays 0 when it keeps up |
| `audio upmix 103\|rt: scene mask worker failed to start; net on the audio thread` | no thread: the sync fallback |
| `buffer103: response committed {type:103,…,arrivalToRenderLatencyMs:11,…}; … (upmix=scene: latency was 0 for every backend; SCENE adds its extra 512 frames over the 512 the car is tuned to)` | the type-103 SETUP response tells the phone the renderer's extra 512 frames (11 ms); OFF still reports 0 |
| `audio config: invalid AUDIO_SCENE_…='…'; expected …; using …` | a knob outside its range, default used |
| `audio sink: detached confirmed underrun; prepare+retry recovery=N retry=9216 expected=9216` | once per resume after a pause on the type-103 path: the ring drained while paused, QSA underran, the writer prepared the channel, pre-rolled one period of silence and retried; `recoveries` counts these, the `underruns=` field in the next sink line stays 0. Several per second only while someone hammers play/pause. Since 2026-10-09 the renderer is not reset by the pause ([Buffered Audio Path](../audio/BUFFERED_AUDIO_PATH.md)). |
| `buffer103: stereo upmix init failed status=…; exact L/R passthrough retained` | the type-103 upmix did not start (config error) |
| `audio realtime upmix: DISABLED, 4 consecutive periods over 12000 us (worst N us); waiting for a break in the stream` | the guard tripped on the realtime path; it tries again after a 500 ms gap |
| `audio realtime upmix: stop periods=N upmixed=N resets=N mean_us=N worst_us=N cold_us=N disables=N revivals=N ms_hist=a/b/c/d/e/f/g/h/i disabled=0` | at every realtime stream stop: the histogram, nine buckets of one millisecond (0-1 … 7-8, the last one 8 ms or more); healthy = `disables=0`, the mass in the first buckets, `worst_us` under 12000 |

Order of reading: `skipped` (the worker keeps up or not), then `disables` and `ms_hist` (the audio thread inside the guard), then the ears. The binary on the unit carries the backend: `grep -ac AUDIO_SCENE_MASK_BLOB /mnt/app/root/hooks/libcarplay_hook.so` prints 1; `ls -l /mnt/app/root/hooks/scene_masknet_c.bin` is 32544 bytes.

## ↩️ How to roll back

- **Previous install:** `build/install_<VIN>_trackA_2026-09-28.zip` is the Track A package of 2026-09-28 (built for this unit; byte-identical to `install_<VIN>.zip` of that day). Install it like any M.I.B. package; the HSSR models come back with it. The uninstaller (`uninstall_MoreIncredibleBash/mod/custom.sh:26-29`) lists both model sets, so an uninstall after either package leaves nothing behind.
- **Rebuild the old tree:** `git checkout upmix-universal-last-2026-10-09` (commit `1501fc7d`, the last tree with the Universal backend beside SCENE), then `scripts/build_hook.sh` and `scripts/prepare_install.sh build/unprotected`; its startup file boots Universal with the scene block commented out.
- **Keep the binary, change the sound:** `AUDIO_STEREO_UPMIX=off` gives the exact L/R bypass (rears silent); commenting the blob line gives spatial only. Both need the HU restart.

## 📜 History

The dormant first package of the same day (`build/install_scene_unprotected.zip`, 3,814,130 B, hook 2,123,420 B, both backends, the scene block commented out behind an `AUDIO_SCENE=1` selector) is superseded by this one; the selector flag was deleted with the Universal backend. The retirement inventory is [Upmix Code Retirement](../audio/UPMIX_CODE_RETIREMENT_2026-10-09.md); every earlier renderer with its numbers is in [Upmix History and Results](../audio/UPMIX_HISTORY_AND_RESULTS_2026-10-09.md); the end-to-end picture is [Stereo to 5.1 Upmix](../audio/STEREO_TO_5_1_UPMIX.md).
