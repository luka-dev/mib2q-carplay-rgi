# Scene Backend Deploy — archived documentation

> Historical snapshot before the current-state documentation restructure. Commands, status and measurements below belong to their recorded revisions.

SCENE is the only stereo synthesis backend in the current hook. This page describes the checked-in build and installation path, reviewed on 2026-10-29. The [October 9–10 deployment record](../../../audio/archive/SCENE_BACKEND_DEPLOY_2026-10-09.md) preserves the first vehicle session, owner listening verdict, artifact hashes and QEMU measurements.

## ⚠️ Installation prerequisite

**Update the MMI to a supported firmware version before installing. Installing on old or unsupported firmware can brick the head unit.** Follow the supported-version list and M.I.B. procedure in the [root installation guide](../../../../README.md#installation), then [Deploy and Release](../../../build-and-deploy/DEPLOY_AND_RELEASE.md) for the developer packaging workflow.

## 📦 Build and package

Use the build commands in [Build System](../../../build-and-deploy/BUILD_SYSTEM.md), then run:

```sh
scripts/prepare_install.sh build/unprotected
python3 scripts/check_model_settings.py deploy/smartphone_integrator/carplay_startup.sh models/scene-masknet-c/settings.json
```

The staging script copies the hook, optional APAC codec and scene blob, both Java jars, both renderers, their assets, startup/monitor/cleanup scripts, the audio-driver payload and NCM components. The installer patches the unit's `smartphone_integrator` in place through `binpatch.py`; it does not replace SI with the old prebuilt re-arm image. The exact package inventory comes from [prepare_install.sh](../../../../scripts/prepare_install.sh), and install gates come from [custom.sh](../../../../install_MoreIncredibleBash/mod/custom.sh).

When the APAC codec is included, the scene blob is copied from `models/scene-masknet-c/scene_masknet_c.bin`. Its format is `SMSKNET1`, 32,544 bytes. The [manifest](../../../../models/scene-masknet-c/manifest.json) records model provenance. The startup settings check verifies the blob export against [settings.json](../../../../models/scene-masknet-c/settings.json); engine calibration is compiled into the hook.

## 🎛️ Current controls

Use **CarPlay UpMix: Off / Normal / Intense** in MMI for a live comparison. Spatial Audio is a separate setting for discrete playback. The current startup exports, fixed calibration and amplifier gate are in [Stereo to 5.1 Upmix](../../../audio/STEREO_TO_5_1_UPMIX.md).

The old environment toggles `AUDIO_STEREO_UPMIX`, `AUDIO_REALTIME_UPMIX` and `AUDIO_SCENE_MASK_SYNC` no longer select a mode. Universal is not a runtime fallback. The mask net normally runs on its dedicated SCHED_RR 17 worker; an unset, unreadable or rejected blob uses the spatial engine without semantic masking and logs the condition. Removing the blob is a developer diagnostic, not the MMI Off setting.

## 🔎 Verify a candidate

1. Record the source revision, local changes, package hash and selected firmware before installation. Old hashes in the historical record identify only those old artifacts.
2. Run the relevant audio checks from [Build System](../../../build-and-deploy/BUILD_SYSTEM.md) and the model settings check. Numerical and QEMU results do not establish on-car timing or sound.
3. After installation and the next CarPlay child start, inspect the audio initialization log for the scene mask load, selected format and output channels. Check the deadline/underrun counters during playback.
4. Compare Off, Normal and Intense on the same material. Check discrete spatial playback separately, then pause/resume, reconnect, announcements and calls. Record amplifier family and the exact build.

The [listening protocol](../../../audio/archive/UPMIX_CARPLAY_LISTENING_PROTOCOL.md) contains measurement discipline and earlier experiments; use the current MMI controls instead of its retired backend knobs. SoundCheck is owner-confirmed working; its processing order and gain handling are documented in [the runtime note](../../../audio/STEREO_TO_5_1_UPMIX.md).

## 🗃️ Historical acceptance and rollback

SCENE was first heard in the car on 2026-10-09 and accepted by the owner on 2026-10-10 (one sighted listener). The follow-up package had separate resume and cluster changes awaiting their own checks. Those observations do not certify the current working tree.

For ordinary removal, use the uninstall procedure in [Deploy and Release](../../../build-and-deploy/DEPLOY_AND_RELEASE.md). Recovering Universal for development requires a separate historical checkout at `upmix-universal-last-2026-10-09`; do not mix its old models, startup script and hook with a current package. The [retirement record](../../../audio/archive/UPMIX_CODE_RETIREMENT_2026-10-09.md) lists what was removed.
