# CarPlay release artifacts — MU1316

Artifacts stay as loose files in `build/`. No ZIP is produced or release uploaded.
`python3 scripts/package_mib.py` adds runtime/configuration copies and the verified
SI/DEVA images without recompiling or replacing existing native outputs or JAR.
`python3 scripts/package_mib.py --check` verifies the prepared file set.

## Files to attach to a release

Use the allowlist in `build/SHA256SUMS`, plus `CKSUMS` and this guide. The
`../mod/custom.sh` entry refers to `install_MoreIncredibleBash/mod/custom.sh`.
Do not publish the entire development `build/` tree: it also contains test
outputs, intermediate builds and vehicle captures.

| Artifact | Purpose / HU target |
| --- | --- |
| `libcarplay_hook.so`, `libcarplay_apac.so` | `/mnt/app/root/hooks/` |
| `maneuver_render`, `altscreen_render`, `flag_atlas.rgba` | `/mnt/app/root/hooks/` |
| `hssr_model.bin` | Copy of the selected shipping V11 model; `/mnt/app/root/hooks/` |
| `carplay_startup.sh`, `carplay_monitor.sh`, `carplay_processes.sh`, `carplay_cleanup.sh` | Runtime copies; `/mnt/app/root/hooks/` |
| `carplay_hook.jar` | `/mnt/app/eso/hmi/lsd/jars/` |
| `carplay_child.json` | Fragment used to replace only `children.carplay` in SI JSON |
| `smartphone_integrator` | Verified final re-arm image; `/mnt/app/eso/bin/apps/smartphone_integrator` |
| `patch_si_rearm.py` | Optional host-side reproduction/reversal tool; Python is not needed on the HU |
| `deva-ctrl-qc.so` | Final **mirror** driver; `/mnt/app/armle/lib/dll/deva-ctrl-qc.so` |
| `carplay_pf.patch` | Reference diff for four stock PF profiles, opening TCP `6030:6032` |
| `release.json`, `CKSUMS`, `SHA256SUMS` | Build identity and integrity checks |

The installer uses its own scoped shell PF edit so it needs no `patch`, Python,
sed or awk on the HU. SI and DEVA installation accepts only the exact known
stock/final MU1316 images; unknown versions stop before writes. Their original
images are backed up as `<target>.carplay-stock` before same-partition rename.
Existing final patches are preserved without claiming ownership of them.

SI's final SHA-256 is
`606236a81c7c4dafda6fdd08fe6eb27c43252796784da25d9760a7d552ee2af3`,
matching the 2026-09-16 HU readback. DEVA's final mirror SHA-256 is
`398e7213be4e966719eed5ff9736fb9142f11e161033f01263f29db640e06355`;
the three-step build is documented in `deploy/deva-ctrl-qc/BUILD_CHAIN.md`.

## SD layout

For installation, copy the contents of `install_MoreIncredibleBash/` over the
M.I.B. SD root, then put the release artifacts in `/build/` on that card:

```text
SD1/
  mod/custom.sh
  build/                       release files listed above
```

For removal, copy the contents of `uninstall_MoreIncredibleBash/` over the card
instead, replacing `/mod/custom.sh`. Removal does not need the build files.
Both use **GEM → M.I.B. → Advanced Settings → Run Custom Script** with CarPlay
disconnected. Both leave the reboot to the owner.
