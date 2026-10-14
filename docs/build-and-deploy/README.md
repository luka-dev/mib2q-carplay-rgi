# Build and Deploy

*Map of content for turning source into shipped binaries, assembling a release by hand<!-- >>> protected -->, VIN-locking it<!-- <<< protected -->, and getting it onto the head unit without bricking anything.*

---

## 📋 What this section covers

The build scripts do one job: compile source into loose binaries under `build/`. `scripts/prepare_install.sh <hook build dir>` then stages a release from three source folders — `build/` (compiled outputs), `models/` (the AI upmix model), and `deploy/` (scripts, patched firmware, PF profiles) — at its on-unit paths and zips it; older packaging and SSH-deploy scripts were deleted. That staged tree goes onto an M.I.B. SD card and is installed by `custom.sh`, or is pushed file by file over SSH by hand.

The separate [wireless stock210 review builder](../wireless-carplay/README.md#build-and-reproduce) assembles an offline review archive, not a shipping installer; its source remains under `wireless/`.

This page is the entry point for the domain. Start with [Build System](BUILD_SYSTEM.md) for how the Docker QNX toolchain produces the `.so` / renderers / jar, then [Deploy and Release](DEPLOY_AND_RELEASE.md) for hand-assembly and getting it onto the car, <!-- >>> protected -->[VIN Protection](VIN_PROTECTION.md) for locking a build to one VIN, <!-- <<< protected -->and [RE Artifacts](RE_ARTIFACTS.md) for where the reverse-engineering assets live. For the hub note and the trap list see [Home](../README.md) and [Landmines](../LANDMINES.md).

## 📊 Source to unit at a glance

```mermaid
flowchart TB
  accTitle: Source through the toolchain to the head unit
  accDescr: Source compiles in the Docker QNX toolchain into loose binaries in build, which are hand-assembled with models and deploy into mod/carplay/root on an M.I.B. card, and custom.sh installs it on the unit.
  src["source: hook C, Java, renderers"]
  docker["Docker qnx65-armv7-toolchain"]
  build["build/ loose binaries only"]
  models["models/ scene mask net c"]
  deploy["deploy/ scripts, firmware, PF"]
  assemble["hand-assembled release tree"]
  card["SD mod/carplay/root tree"]
  unit["custom.sh onto unit root"]
  src --> docker --> build
  build --> assemble
  models --> assemble
  deploy --> assemble
  assemble --> card --> unit
  class src,docker primary
  class build,assemble success
  class card,unit,models,deploy neutral
  classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
  classDef neutral fill:#f3f4f6,stroke:#6b7280,stroke-width:2px,color:#1f2937
```

## 🔗 The notes

| Note | What it covers |
|---|---|
| [Build System](BUILD_SYSTEM.md) | The Docker `qnx65-armv7-toolchain` (`:latest` = GCC 8.5); `build_hook.sh` and the shared compile+verify core; the ABI-safety asserts (emutls, export allowlist, `.init_array`, no libstdc++, QSA/HID export counts); Java, renderers, host tests, and the single-source APAC flags. |
| [Deploy and Release](DEPLOY_AND_RELEASE.md) | Builds emit binaries only; hand-staging `build/` + `models/` + `deploy/` into `mod/carplay/root/`; the source→on-unit map; M.I.B. SD install via `custom.sh` (no SHA gate); manual SSH push and its traps; never reboot the unit / jar deploy = full reboot. |
| [VIN Protection](VIN_PROTECTION.md) | The protector: VIN-lock via KDF + blind sentinel, compile-time gate (`#ifdef CARPLAY_PROT`, inert unstamped), the two-step protected build into `build/protected_<VIN>/`, per-build `PROT_SECRET`, fail-open honest ceiling. | <!-- protected-only -->
| [RE Artifacts](RE_ARTIFACTS.md) | Where the reverse-engineering assets now live: `assets/soundamp` (SHARC/amp RE) and `assets/vc-fonts` (VC font extraction) — what each is and its provenance. |
| [Scene Backend Deploy](SCENE_BACKEND_DEPLOY.md) | The scene upmix renderer package (2026-10-09): what `install_scene_unprotected.zip` carries, the commented-out `AUDIO_SCENE` enable block in `carplay_startup.sh`, how to read the hook log on the unit, how to roll back. |
| [Production Audits](PRODUCTION_AUDITS.md) | Ten release-safety and readiness reviews (Aug–Sep 2026): findings, fixes, status in today's code. |

## 💡 The one thing to remember

Deploying files is fine; **restarting the unit is not** — the user is usually in the car, and a jar redeploy takes the whole system down for ~50 s. Copy, report what is staged, and stop. See [Deploy and Release](DEPLOY_AND_RELEASE.md) and [Landmines](../LANDMINES.md).

Provenance lives in `docs/build-and-deploy/DEPLOY_AND_RELEASE.md`, <!-- >>> protected -->`docs/build-and-deploy/VIN_PROTECTION.md`, <!-- <<< protected --> the `scripts/build_*.sh` set, and `install_MoreIncredibleBash/mod/custom.sh`.
