# CarPlay release artifacts — MU1316

There is no packaging script anymore: build scripts drop their outputs into a
`build/` subfolder, and a release is assembled **by hand** from a few source
folders (`build/`, `models/`, `deploy/`). This doc is the source→destination map
`package_mib.py` used to encode. custom.sh installs a manually-staged tree.

## Where each file comes from, and where it lands on the unit

The on-unit column is the path under `/` (custom.sh copies `carplay/root/` onto
`/`, or you place files there yourself).

| File | Source in the repo | On-unit path |
| --- | --- | --- |
| `libcarplay_hook.so` | `build/unprotected/` (or `build/protected_<VIN>/`, stamped) | `/mnt/app/root/hooks/` |
| `libcarplay_apac.so` | `build/unprotected/` (or `build/protected_<VIN>/`); omitted for a `HOOK_AUDIO=0` build | `/mnt/app/root/hooks/` |
| `maneuver_render` | `build/` (`scripts/build_renderers.sh`) | `/mnt/app/root/hooks/` |
| `altscreen_render` | `build/` (`scripts/build_renderers.sh`) | `/mnt/app/root/hooks/` |
| `carplay_hook.jar` | `build/` (`scripts/build_java.sh`) | `/mnt/app/eso/hmi/lsd/jars/` |
| `hssr_model.bin` | `models/hssr-q6d-moises-a/hssr_model.bin` (q6d, main control model) | `/mnt/app/root/hooks/` |
| `hssr_guard.bin` | `models/hssr-selection-v11/hssr_guard.bin` (V11 vocal guard) | `/mnt/app/root/hooks/` |
| `flag_atlas.rgba` | `maneuver_render/resources/flag_atlas.rgba` | `/mnt/app/root/hooks/` |
| `carplay_startup.sh` | `deploy/smartphone_integrator/` | `/mnt/app/root/hooks/` |
| `carplay_monitor.sh` | `deploy/smartphone_integrator/` | `/mnt/app/root/hooks/` |
| `carplay_processes.sh` | `deploy/smartphone_integrator/` | `/mnt/app/root/hooks/` |
| `carplay_cleanup.sh` | `deploy/smartphone_integrator/` | `/mnt/app/root/hooks/` |
| `smartphone_integrator` | `deploy/smartphone_integrator/smartphone_integrator` | `/mnt/app/eso/bin/apps/smartphone_integrator` (whole-file replace) |
| `deva-ctrl-qc.so` | `deploy/deva-ctrl-qc/deva-ctrl-qc.so.patched` (rename to `deva-ctrl-qc.so`) | `/mnt/app/armle/lib/dll/deva-ctrl-qc.so` (whole-file replace) |
| `carplay_child.json` | `deploy/smartphone_integrator/carplay_child.json` | installer resource (not a `/` file); custom.sh splices only `children.carplay` into the SI JSON |
| `patch_si_rearm.py` | `deploy/experimental/usb_replug_recovery/patch_si_rearm.py` | host-side reproduction/reversal tool; not needed on the HU |

Notes:
- The two `.so`, `maneuver_render`, `altscreen_render` and `carplay_hook.jar` are
  the compiled outputs — build them first (`build_hook.sh` /
  `build_hook_protected.sh`, `build_renderers.sh`, `build_java.sh`). Everything
  else is copied from its canonical home above, unchanged.
- **PF firewall:** no patch file is installed. custom.sh makes its own scoped
  in-shell edit opening TCP `6030:6032` across the four stock profiles
  (`pf.conf`, `pf.ecm0.conf`, `pf.mlan0.conf`, `pf.ppp0.conf`; stock copies under
  `deploy/pf/stock/`), so it needs no `patch`/Python/sed on
  the HU.

## Firmware image identities (whole-file replacements)

SI and DEVA installation accepts only the exact known final MU1316 images;
unknown versions stop before writes. Originals are backed up as
`<target>.carplay-stock` before same-partition rename.

- `smartphone_integrator` SHA-256
  `606236a81c7c4dafda6fdd08fe6eb27c43252796784da25d9760a7d552ee2af3`
  (2026-09-16 HU readback).
- `deva-ctrl-qc.so` (final mirror) SHA-256
  `398e7213be4e966719eed5ff9736fb9142f11e161033f01263f29db640e06355`;
  three-step build in `deploy/deva-ctrl-qc/BUILD_CHAIN.md`.

Model identities: q6d `hssr_model.bin`
`96abe57d9ee703f2e39c0231de97580c303ce896ce044b7450c14240002b0a0e`,
V11 guard `hssr_guard.bin`
`ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae`.

## SD layout

Copy the contents of `install_MoreIncredibleBash/` over the M.I.B. SD root, then
put the release artifacts in `/build/` on that card:

```text
SD1/
  mod/custom.sh
  build/                       the files listed above
```

For removal, copy the contents of `uninstall_MoreIncredibleBash/` over the card
instead, replacing `/mod/custom.sh`. Removal does not need the build files. Both
use **GEM → M.I.B. → Advanced Settings → Run Custom Script** with CarPlay
disconnected. Both leave the reboot to the owner.

For a VIN-locked (protected) release, assemble the same map by hand — the only
difference is the two `.so` come from `build/protected_<VIN>/` (compiled and
VIN-stamped by `build_hook_protected.sh`) instead of `build/unprotected/`. See
`docs/PROTECTOR.md`.
