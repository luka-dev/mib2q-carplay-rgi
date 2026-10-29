# Scene Backend Deploy

Build and package the SCENE stereo renderer, its mask model and the shared CarPlay audio runtime.

## ⚠️ Installation prerequisite

**Update the MMI to a supported firmware version before installing. Installing on old or unsupported firmware can brick the head unit.** Follow the supported-version list and M.I.B. procedure in the [root installation guide](../../README.md#installation), then [Deploy and Release](DEPLOY_AND_RELEASE.md) for the developer packaging workflow.

## 📦 Build and package

Use the build commands in [Build System](BUILD_SYSTEM.md), then run:

```sh
scripts/prepare_install.sh build/unprotected
python3 scripts/check_model_settings.py deploy/smartphone_integrator/carplay_startup.sh models/scene-masknet-c/settings.json
```

The staging script copies the hook, optional APAC codec and scene blob, both Java jars, both renderers, their assets, startup/monitor/cleanup scripts, the audio-driver payload and NCM components. The installer patches the unit's `smartphone_integrator` in place through `binpatch.py`. The exact package inventory comes from [prepare_install.sh](../../scripts/prepare_install.sh), and install gates come from [custom.sh](../../install_MoreIncredibleBash/mod/custom.sh).

When the APAC codec is included, the scene blob is copied from `models/scene-masknet-c/scene_masknet_c.bin`. Its format is `SMSKNET1`, 32,544 bytes. The [manifest](../../models/scene-masknet-c/manifest.json) records model provenance. The startup settings check verifies the blob export against [settings.json](../../models/scene-masknet-c/settings.json); engine calibration is compiled into the hook.

## 🎛️ Current controls

Use **CarPlay UpMix: Off / Normal / Intense** in MMI for a live comparison. Spatial Audio is a separate setting for discrete playback. The current startup exports, fixed calibration and amplifier gate are in [Stereo to 5.1 Upmix](../audio/STEREO_TO_5_1_UPMIX.md).

The mask net normally runs on its dedicated SCHED_RR 17 worker; an unset, unreadable or rejected blob uses the spatial engine without semantic masking and logs the condition. Removing the blob is a developer diagnostic, not the MMI Off setting.

## 🔎 Verify a candidate

1. Record the source revision, local changes, package hash and selected firmware before installation.
2. Run the relevant audio checks from [Build System](BUILD_SYSTEM.md) and the model settings check. Numerical and QEMU results do not establish on-car timing or sound.
3. After installation and the next CarPlay child start, inspect the audio initialization log for the scene mask load, selected format and output channels. Check the deadline/underrun counters during playback.
4. Compare Off, Normal and Intense on the same material. Check discrete spatial playback separately, then pause/resume, reconnect, announcements and calls. Record amplifier family and the exact build.

Keep the playback material and level constant when comparing MMI modes. SoundCheck gain handling is documented in [the runtime note](../audio/STEREO_TO_5_1_UPMIX.md#output-order).

## 🔄 Uninstall and records

Use the uninstall procedure in [Deploy and Release](DEPLOY_AND_RELEASE.md). Keep each candidate’s source revision, payload hashes and measured results together. Earlier packages and listening reports are in the [audio archive](../audio/archive/README.md).
