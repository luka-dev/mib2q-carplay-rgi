# Scene Backend Deploy

*The scene upmix renderer shipped dormant: `build/install_scene_unprotected.zip` installs like any M.I.B. package and changes nothing audible until the owner uncomments one block in `carplay_startup.sh`. Built 2026-10-09 and rebuilt the same day after stage 4 of the port (NEON engine, NEON mask net, the net on its own worker thread); not yet in the car.*

---

## What is in the package

`scripts/prepare_install.sh build/scene_unprotected` output, assembled by the usual path ([Deploy and Release](DEPLOY_AND_RELEASE.md)): the same tree as the Track A package plus one file, the mask blob. Unprotected (no VIN lock), unlike the 28.09 `install_protected_WAUZZZF47HA098927.zip`; a VIN-locked copy is `scripts/build_hook_protected.sh <VIN>` then `prepare_install.sh build/protected_<VIN>` (the blob line is in `prepare_install.sh`, so it ships there too, [VIN Protection](VIN_PROTECTION.md)).

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `build/install_scene_unprotected.zip` (48 entries, 8,040,078 B unpacked) | 3,814,130 | `8fb39a105f900a55801d68fb0bdd634036fd131965a9408757e7c71f0f65510b` |
| `libcarplay_hook.so` (`build/scene_unprotected/`, = `build/unprotected/` of the same run) | 2,123,420 | `bd6cf664cf0c0a90d923ee44150ce5435c786a36b3e9bf976fb58ce3e0569147` |
| `libcarplay_apac.so` | 696,491 | `1c25799f4dd47549335cbc22cc9a059efab030d4e489937fc4d8460e341e09ce` |
| `scene_masknet_c.bin` (`models/scene-masknet-c/`) | 32,544 | `58cdc72f573d4718f6325de8a481ecd904096d8f589372e6de5cb05222d98957` |

The morning build of the same day (hook 1,848,914 B `df1f7e29…`, zip 3,788,607 B `784907c0…`) is superseded: it ran the net on the audio thread at 19.4 M instructions per control.

- **Hook:** `scripts/build_hook.sh` (Docker `qnx65-armv7-toolchain:latest`, GCC 8.5, `-mfpu=neon -mfloat-abi=softfp`, the upmix DSP files at `-O3 -fno-math-errno`, `scripts/apac_flags.sh`), zero warnings, all ABI gates green (emutls 0, 26 exact interpose exports, `.init_array` compiler-only, APAC without libstdc++). Built from `ac992616` with the uncommitted scene work in the tree: `hook/audio/scene_engine.c/.h`, `scene_engine_math.c`, `scene_simd.h`, `scene_fft.h`, `scene_clusters.c/.h`, `scene_mask.c/.h`, `scene_mask_worker.c/.h`, the `SCENE` backend in `stereo_upmix.c/.h` and `audio_hook.c/.h`. The file is 2.0 MB (Track A 1.5 MB, the morning build 1.8 MB): the type-103 upmix state `spa103_render` (1,413,392 B: engine, mask net and the worker's own copy of the net) is a static initializer in `.data`; the realtime instance `spa_rt_upmix` (1,412,784 B) is in `.bss` and costs nothing on disk. The HSSR Universal path is unchanged: host bench checksums plain `0x5d56ee600edd74d6` / `--scene` `0xce073ff94429d813` (q6d model, 200 blocks, warmup 8), `scripts/run_tests.sh` green on the packaged sources.
- **Mask blob:** `tools/stereo-upmix-render/port/export_masknet_weights.py` over the fixtures' `masknet_oracle.npz` (the fine net c weights, [spec section 16](../audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md)); `SMSKNET1`, u32 24/12/4, 8131 float32, the file image of `struct scene_mask_weights`. Re-exported for this build and byte-identical to the morning's: the NEON runtime lays the weights out in tile order at `scene_mask_init()`, so the file format did not change (no `SMSKNET2`, the exporter is untouched). Provenance and the hook-vs-torch numbers in `models/scene-masknet-c/manifest.json`. On the unit it sits beside the HSSR models at `/mnt/app/root/hooks/scene_masknet_c.bin`; the hook reads it only when `AUDIO_SCENE_MASK_BLOB` names it.
- **Everything else** is what Track A ships: `hssr_model.bin`, `hssr_guard.bin`, `hssr_mask.bin` (v21a), both jars, both renderers, `flag_atlas.rgba`, the SI image, `deva-ctrl-qc.so`, the NCM fence, the five `carplay_*.sh`. The startup knobs are Track A's (`AUDIO_HSSR_MASK_DRIVE=1.5`, `AUDIO_HSSR_VOCAL_GATE=0.5`, `AUDIO_STEREO_REAR_AIR_DB=-9`); `scripts/check_model_settings.py` passes (22 knobs).

What "no-op" means here: with the block commented out the **audio config** is Track A's and the hook runs HSSR Universal. The **binary** is not the 28.09 one: it carries every commit since `800de9d` (altscreen pacing, the audits) plus the scene code. If the owner wants the Track A sound with certainty, the 28.09 BEST zip is still the reference.

## Measured cost (QEMU, not the car)

QEMU `-icount` retired ARMv7 instructions, GCC 8.5, measured on the sources of this package (`tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh`, `--bench mask` for the net; [bench README](../../tools/stereo-upmix-render/bench/README.md), "Scene engine bench"). "Before" is the morning build, stage 3 of [spec section 18](../audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md); the stage-4 targets were engine ≤ 1.2 M per hop, net ≤ 1.5 M per control and off the audio thread.

| What | Before | Now, `-O2` | Now, `-O3 -fno-math-errno` (the shipped flags) |
|---|---:|---:|---:|
| engine, per 256-frame hop, with a mask row (`--mask`) | 830,111 | 344,382 | 326,746 |
| engine, per 256-frame hop, spatial only | — | 313,679 | — |
| mask net, per 512-frame control (`scene_mask_bench`) | 19,362,951 | 1,368,181 | 1,375,745 |
| audio thread's hand-off to the worker, per 768-frame push (`--mask --handoff` minus `--mask`) | — (net on the audio thread) | +1,088 | — |

Per 768-frame QSA period the audio thread runs 0.98 M instructions for the engine at the shipped flags (p99 0.99 M, worst push 1.22 M over 30 s; 1.03 M at `-O2`, same S16 checksum: the flags change no result) plus 1.1 k for the hand-off; the net's 1.37 M per 10.7 ms control runs on the worker. The one QEMU-instructions-to-Krait-milliseconds calibration so far (the Universal runs; it put the morning build's 2.49 M per period at 2.9-4.5 ms) scales these to 1.1-1.8 ms per period on the audio thread (spatial only, measured at `-O2`: 0.94 M, 1.1-1.7 ms) and 1.6-2.5 ms per control on the worker, against the 12 ms realtime guard; with `AUDIO_SCENE_MASK_SYNC=1` (net on the audio thread) 3.0 M per period, 3.5-5.5 ms. The guard histogram on the unit is the real number. Fixture gates on the same sources, all green: engine −125.8 / −131.3 dB of the output RMS against the Python reference with 1000- and 768-frame blocks bit-identical (`port/test_engine.py`); net 4.2e-4 vs torch on the C features and 5.2e-3 end to end against the goldens, whose own float32 noise is 4.2e-3 (`port/test_mask.py`); the hook against `stream_render` on the net's own rows −71.7 dB, worst channel −65.0 dB (`port/test_hook_render.py`). Bench checksums for a re-run: net `0xedf9005e82dae1ce` (10 s, `-O2` and `-O3` identical), engine `--mask` `0x0421d6cca340373e` (30 s, `-O2` and the shipped flags identical; 60 s `0xace5f91925bec549`) and spatial `0xd96e0a0d0b33b0b5` (30 s, `-O2`).

## How to enable

The block sits in `/mnt/app/root/hooks/carplay_startup.sh` (repo: `deploy/smartphone_integrator/carplay_startup.sh`; on the card: `mod/carplay/root/mnt/app/root/hooks/carplay_startup.sh`), right before the `AUDIO_REALTIME_UPMIX` line. Remove the leading `# ` from the `export` lines you want:

```sh
# export AUDIO_SCENE=1
# export AUDIO_SCENE_MASK_BLOB="$H/scene_masknet_c.bin"
# export AUDIO_SCENE_REAR_DELAY_MS=0
# export AUDIO_SCENE_INST_GAMMA=1
# export AUDIO_SCENE_INST_ALPHA=0.9
# export AUDIO_SCENE_DUCK_DEPTH=0.25
# export AUDIO_SCENE_ALPHA=0.7
# export AUDIO_SCENE_SIDE_LP_HZ=7000
# export AUDIO_SCENE_BLEED_CANCEL=0
```

The environment is read once when `dio_manager` starts (SI execs `carplay_startup.sh` as the CarPlay child), so an edit on the unit applies at the next child start, in practice the next HU boot. The owner restarts the unit; the tooling never does ([Landmines](../LANDMINES.md)).

**Two steps, in this order.**

1. **Spatial only:** `AUDIO_SCENE=1` and the seven knob lines, `AUDIO_SCENE_MASK_BLOB` left commented. This is the scene renderer without the net: no instrument transfer, no duck, LFE V2 steered from the front alone. Audio-thread cost 0.31 M instructions per hop (0.94 M per 768-frame period, 1.1-1.7 ms by the calibration above), well inside the 12 ms guard. Read `audio realtime upmix: stop ... ms_hist=...` at a stream stop before going on: `disables=0`, the mass in the first buckets.
2. **With the net:** uncomment `AUDIO_SCENE_MASK_BLOB` too. The net runs on the scene mask worker (`hook/audio/scene_mask_worker.c`, the HSSR controller's mailbox pattern, SCHED_RR 17, started per SETUP on the type-103 path and kept across re-params on the realtime path): the audio thread pays only the hand-off, and the net's rows reach the engine one control (512 frames, 10.7 ms) late, which the engine's 300 ms instrument attack absorbs. At 1.37 M instructions per control (1.6-2.5 ms of every 10.7 ms) the worker is expected to keep up: `skipped=0` in the stop line. If it does not, a control that arrives while the worker still holds the two before is refused, never waited for: the audio thread does not block, the engine keeps its last row, and the net's 2048-sample history gets a 512-frame splice that perturbs the next 2-3 rows; `skipped=N` counts them. `AUDIO_SCENE_MASK_SYNC=1` runs the net on the audio thread instead (3.0 M per period, 3.5-5.5 ms by the calibration; the morning build's 35-55 ms tripped the guard here): the exact timing the port tests check, an A/B lever against the worker's one-control lag, not the default.

The knobs (`hook/audio/audio_hook.h`), all engine units, each clamped to its range with an `audio config: invalid ...` line and the default on a bad value:

| Knob | Value in the block | Range | What it is |
|---|---|---|---|
| `AUDIO_SCENE` | `1` | 0/1 | selects the scene renderer; needs `AUDIO_STEREO_UPMIX=on`, which stays as it is above the block (off is the bypass and wins, logged once) |
| `AUDIO_SCENE_MASK_BLOB` | `$H/scene_masknet_c.bin` | path | the net; unset, unreadable or rejected = spatial only, logged once |
| `AUDIO_SCENE_MASK_SYNC` | not in the block | 0/1 | default 0: the net runs on the scene mask worker (`hook/audio/scene_mask_worker.h`, SCHED_RR 17 like the HSSR controller), its rows applied one control (512 frames, 10.7 ms) later than on the audio thread, controls skipped instead of waited for when it falls behind; `1` runs it on the audio thread, the exact timing the port tests check (`tools/stereo-upmix-render/port/test_hook_render.py`) |
| `AUDIO_SCENE_REAR_DELAY_MS` | `0` | -50..50 | variant J: our rear and the amp's own front bleed meet the rear doors in phase; the amp's rear bus supplies the ~11.8 ms lag |
| `AUDIO_SCENE_INST_GAMMA` | `1` | 0.1..4 | eligibility exponent, 1 = full rear (kit letter I/J); 2 = the balanced fallback H |
| `AUDIO_SCENE_INST_ALPHA` | `0.9` | 0..1 | instrument transfer fraction |
| `AUDIO_SCENE_DUCK_DEPTH` | `0.25` | 0..1 | duck of the spatial transfer and the side layer by the protect mask (needs the blob) |
| `AUDIO_SCENE_ALPHA` | `0.7` | 0..1 | spatial relocation fraction |
| `AUDIO_SCENE_SIDE_LP_HZ` | `7000` | 0 = off, 1000..24000 | side-layer low-pass |
| `AUDIO_SCENE_BLEED_CANCEL` | `0` | 0..0.5 | variant K's bleed cancel (0.355); 0 = J |
| `AUDIO_SCENE_SIGMA_MIN` | not in the block | 0.01..1 | default 0.08; `0.15` is the documented second candidate ([spec section 15](../audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md)) |

All values in the block are the engine defaults (`scene_config_defaults()`): uncommenting only `AUDIO_SCENE=1` gives the same configuration; the lines exist so the running configuration can be read from the file. What the scene backend ignores: `AUDIO_STEREO_SURROUND`, `FRONT_BASS_DB`, `FRONT_TREBLE_DB`, `REAR_AIR_DB`, `FRONT_RETENTION` and every `AUDIO_HSSR_*` knob (the fronts and rears leave at the engine's own levels); what still applies: `AUDIO_STEREO_LFE=assist` with `AUDIO_STEREO_LFE_GAIN` (LFE V2, steered by the net's bass/drum posteriors when the blob is loaded) and the J525 cabin guard. `AUDIO_REALTIME_UPMIX=on` means the scene renderer also runs on the realtime path, under the guard.

## How to verify on the unit

The hook log is `/tmp/carplay_hook.log`, WARN level by default (`touch /mnt/app/carplay_verbose` for everything). Lines to look for, exact strings:

| Line | Meaning |
|---|---|
| `audio realtime upmix: enabled, scene renderer, mask net loaded, on the worker` | `AUDIO_SCENE=1` took, blob loaded, net on its thread (at audio init, `AUDIO_REALTIME_UPMIX=on`); `..., on the audio thread` with `AUDIO_SCENE_MASK_SYNC=1` |
| `audio upmix rt: scene mask worker stopped rows=A/B skipped=N stale=.. superseded=.. resets=..` | at stream stop (`103` for the buffered path): B controls handed to the worker, A run by the net, N refused because it was still busy with the two before (rows skipped: the net slower than realtime); N stays 0 when it keeps up |
| `audio upmix rt: scene mask worker failed to start; net on the audio thread` | no thread (sync fallback, the old behaviour) |
| `audio realtime upmix: enabled, scene renderer, mask net none (spatial only)` | step 1: scene renderer, no blob |
| `audio realtime upmix: enabled, model loaded` | the block is still commented: HSSR Universal, as Track A |
| `SCENE: mask net ready path=/mnt/app/root/hooks/scene_masknet_c.bin bytes=32544` | the blob passed size, magic and dimensions |
| `SCENE: mask blob unreadable path=... errno=...; running without a mask` | wrong path or mode |
| `SCENE: mask blob rejected path=... bytes=... (expected 32544 bytes of SMSKNET1); running without a mask` | truncated or wrong file |
| `buffer103: response committed {type:103,...,arrivalToRenderLatencyMs:11,...}; ... (upmix=scene: latency was 0 for every backend; SCENE adds its extra 512 frames over Universal's 512)` | the type-103 SETUP response now tells the phone the scene renderer's extra 512 frames (11 ms); OFF and Universal still report 0 as before |
| `audio config: AUDIO_SCENE=1 ignored, AUDIO_STEREO_UPMIX is off (the bypass wins)` | somebody turned the upmix off above the block |
| `audio config: invalid AUDIO_SCENE_...='...'; expected ...; using ...` | a knob outside its range, default used |
| `buffer103: stereo upmix init failed status=...; exact L/R passthrough retained` | the type-103 upmix did not start (config error) |
| `audio realtime upmix: DISABLED, 4 consecutive periods over 12000 us (worst N us); waiting for a break in the stream` | the guard tripped on the realtime path; it tries again after a 500 ms gap |
| `audio realtime upmix: stop periods=N upmixed=N resets=N mean_us=N worst_us=N cold_us=N disables=N revivals=N ms_hist=a/b/c/d/e/f/g/h/i disabled=0` | at every realtime stream stop: the histogram, nine buckets of one millisecond (0-1 ... 7-8, the last one 8 ms or more); healthy = `disables=0`, the mass in the first buckets, `worst_us` under 12000 |

The binary on the unit carries the backend: `grep -ac AUDIO_SCENE_MASK_BLOB /mnt/app/root/hooks/libcarplay_hook.so` prints 1 (on the host, `strings libcarplay_hook.so | grep -c '^AUDIO_SCENE'` prints 11: the eleven knob names, `AUDIO_SCENE_MASK_SYNC` included). The blob: `ls -l /mnt/app/root/hooks/scene_masknet_c.bin` is 32544 bytes; its SHA-256 on the host is in the table above.

## How to roll back

Put the `# ` back on the export lines (or copy the package's `carplay_startup.sh` over the unit's) and let the owner restart the unit. The blob then sits unused and the chain is Track A's, with this hook. For the 28.09 binary install `install_protected_WAUZZZF47HA098927.zip` again. The uninstaller (`uninstall_MoreIncredibleBash/mod/custom.sh`) now lists `scene_masknet_c.bin` and `hssr_mask.bin` among the owned files.

## Repo changes for this package (uncommitted)

- `deploy/smartphone_integrator/carplay_startup.sh`: the commented block (lines 122-148), its comment carrying the stage-4 costs and the worker.
- `hook/audio/scene_*.c/.h`: the stage-4 sources (NEON/SoA engine with `scene_simd.h`, the rewritten `scene_mask.c/.h`, `scene_mask_worker.c/.h`), the `AUDIO_SCENE_MASK_SYNC` knob and the SETUP latency in `audio_hook.c/.h`, `stereo_upmix.c/.h`; `scripts/apac_flags.sh` (`UPMIX_DSP_SRCS`), `scripts/run_tests.sh`, `scripts/test_hook_audio.sh`, `scripts/test_audio_interposers.sh`, `tests/stereo_upmix_test.c` (worker test), the bench (`tools/stereo-upmix-render/bench/scene_*`) and the port tests (`tools/stereo-upmix-render/port/`).
- `scripts/prepare_install.sh`: copies `models/scene-masknet-c/scene_masknet_c.bin` beside the HSSR models.
- `models/scene-masknet-c/scene_masknet_c.bin` + `manifest.json`: new; the manifest's cost and accuracy fields follow this build.
- `uninstall_MoreIncredibleBash/mod/custom.sh`: two owned files added.
- `docs/build-and-deploy/DEPLOY_AND_RELEASE.md`, `README.md`: artifact map, checksum table and index rows.
