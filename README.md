# MHI2Q CarPlay cluster integration

CarPlay patch set for Audi MHI2Q infotainment.
(Based on MHI2Q firmware, but may need rebuild for different versions.)

**Disclaimer:** Use at your own risk. These patches modify firmware binaries and system configurations on your infotainment unit. Always back up all original files before making any changes. The authors are not responsible for any damage, bricked devices, or warranty issues resulting from use of these patches.

## 🖼️ Gallery

<p align="center">
  <img src="assets/gallery/maneuver_demo.gif" width="90%" /><br />
  <sub>Cluster maneuver renderer driven through a demo route</sub>
</p>

**Virtual Cockpit: route guidance from the maneuver renderer**

<p align="center">
  <img src="assets/gallery/vc_day_nav.jpeg" height="200" />
  <img src="assets/gallery/vc_night_nav.jpeg" height="200" />
</p>
<p align="center">
  <img src="assets/gallery/vc_full_map.jpeg" height="200" />
  <img src="assets/gallery/vc_lane_guidance.jpeg" height="200" />
</p>

**Audi front PDC no longer hides CarPlay** · **Cover art on the cluster**

<p align="center">
  <img src="assets/gallery/pdc_over_carplay.jpeg" width="45%" />
  <img src="assets/gallery/cover_art.jpeg" width="45%" />
</p>

**Head-up display**

<p align="center">
  <img src="assets/gallery/IMG_0623.jpeg" width="30%" />
  <img src="assets/gallery/IMG_6302.jpeg" width="30%" />
  <img src="assets/gallery/IMG_0599.jpeg" width="30%" />
</p>

## 📍 Contents

- [Gallery](#-gallery)
- [Features](#-features)
- [Repository layout](#-repository-layout)
- [Build](#-build)
- [Deployment](#-deployment)
- [Logging](#-logging)
- [Documentation](#-documentation)
- [Help wanted](#-help-wanted)
- [References](#-references)

## ✨ Features

There is nothing to switch on: plug in the iPhone and CarPlay starts as usual; the cluster
features below follow it automatically.

- **Turn-by-turn on the cluster.** During CarPlay navigation the Virtual Cockpit shows a 3D maneuver
  arrow drawn over the cluster's own native map (the stock map stays; there is no CarPlay map on the
  cluster). The arrow fills as the turn approaches and blinks just before it, lane arrows appear under
  it, and the cluster also shows distance to the turn, arrival time and remaining distance. Needs an
  app that sends CarPlay route guidance: Apple Maps and Google Maps do, AMap does with its CarPlay
  guidance setting on, Waze does not
  ([details](docs/rgd/rgd-activation.md#-which-navigation-apps-send-route-guidance)).
- **Route text in the Virtual Cockpit.** A text line names the exit sign or the next road (the
  current road when there is nothing else); long names scroll. Press **OK** (the left steering-wheel
  roller) to switch it to arrival time and time left, and press again to go back; it returns by itself
  after 20 s ([details](docs/rgd/vc-route-text.md)).
- **Head-up display.** The same maneuver icons, lane arrows and distance appear on the HUD.
- **Steering-wheel roller** keeps zooming the stock cluster map, as without CarPlay.
- **Cover art on the cluster.** The now-playing album art shows on the cluster media screen.
- **Parking popups no longer hide CarPlay.** When the Audi front PDC / parking view pops up beside it,
  CarPlay stays on screen instead of being replaced ([details](docs/hmi/pdc-small-stage.md)).
- **MMI touchpad → DPAD bridging** so finger drags navigate CarPlay menus.

## 🗂️ Repository layout

| Path | Purpose |
| --- | --- |
| `hook/` | Shipping native `libcarplay_hook.so` source |
| `java_patch/` | The only supported Java patch source |
| `java_resources/` | Resources packed into the jar (VC glyph-width / Unicode table `vc-text.bin`) |
| `maneuver_render/` | GLES maneuver overlay renderer (C, plus the C++11 `scene/` engine) |
| `common/` | Shared renderer code: QNX Screen surface, GL program-binary cache, log timestamps |
| `deploy/smartphone_integrator/` | Runtime scripts and child-process configuration for the HU |
| `install_MoreIncredibleBash/`, `uninstall_MoreIncredibleBash/`, `logging_MoreIncredibleBash/` | M.I.B. custom scripts that install / remove a staged release / collect logs |
| `scripts/` | Docker build entry points (Java / hook / renderer) and host test runners |
| `tests/` | Host tests (C, Java, Python) for the hook, Java bridge and renderer |
| `toolchain/qnx65-abi/` | QNX Screen ABI headers used only for cross-compilation |
| `docs/` | Markdown knowledge base (also opens in Obsidian) - validated RE + implementation notes (open [`docs/INDEX.md`](docs/INDEX.md)) |
| `assets/` | Screenshots and visual reference material |
| `build/` | Canonical deployable artifacts |

Raw unit logs and generated class trees are intentionally kept outside Git.

## 🔧 Build

Native code needs the QNX 6.5 ARMv7 cross-toolchain image from
[luka-dev/qnx65-armv7-toolchain](https://github.com/luka-dev/qnx65-armv7-toolchain). Build it once:

```sh
git clone https://github.com/luka-dev/qnx65-armv7-toolchain
cd qnx65-armv7-toolchain
./host-scripts/qnx-run.sh build        # qnx65-armv7-toolchain:latest (GCC 8.5)
```

Then run from this repository's root:

```sh
./scripts/build_java.sh        # → build/carplay_hook.jar
./scripts/build_hook.sh        # → build/libcarplay_hook.so
./scripts/build_renderers.sh   # → build/maneuver_render
```

All three build in Docker - no host JDK is required. The Java patch compiles in
`eclipse-temurin:8-jdk-jammy` against the stock JAR and OSGi libraries. The default
layout remains `../../Tools/jxe2jar`; set `CARPLAY_TOOLS_DIR` to relocate it, or
set `STOCK_JAR` and `CARPLAY_DEPENDENCIES` for explicit external inputs. Separate
bootstrap/runtime libraries can be supplied with `STOCK_BOOT_JAR` and
`STOCK_RUNTIME_JAR`. The car's class library (`libs/jcl/.../jcl.jar` in the default
layout) is optional for the build: when present, the patch compiles against it, which
catches APIs the unit lacks; otherwise it compiles against JDK 8, as before. The stock
linkage audit requires it. `ASM_JAR` and `ASM_TREE_JAR` override linkage-audit inputs;
`JAVA_OUTPUT` selects the output directory. No script editing is necessary.
`CARPLAY_JAVA_IMAGE` and `QNX_TOOLCHAIN_IMAGE` accept immutable image IDs; the
Java launcher records the resolved ID in `java-image-id.txt`. Java compilation
and tests share `scripts/java/`, including resource/version checks and stable
JAR ordering/timestamps. Windows uses Git Bash path conversion; POSIX hosts
retain their user identity for Java output. The two
native builds use the `qnx65-armv7-toolchain` image and synthesize their import stubs, so the resulting
ELF binds the unit's real Screen/EGL/GLES libraries at runtime. The renderer's C++ scene engine is
built with that image's `g++` and must not pull in the C++ runtime; the hook build rejects any dynamic
export beyond its five interposers. There are no Java variants.

There is one hook image: logging is always compiled in, WARN/ERROR by default, INFO with the
`carplay_verbose` marker (see [Logging](#-logging)). The only build-time switch is for debugging:

```sh
./scripts/build_hook.sh                        # production image
LOG_RGD_PACKET_RAW=1 ./scripts/build_hook.sh   # + raw RGD packet hex dumps
```

### Tests

Host-only, no unit needed:

```sh
./scripts/run_tests.sh            # C + shell: RGD parser, bus, cover art, shader cache, installer, supervisor
./scripts/test_route_info.sh      # Java route-guidance / BAP bridge against the stock interfaces
./scripts/test_java_transports.sh # Java bus + renderer sockets, touchpad
./scripts/test_maneuver_native.sh # renderer engine + lanes (macOS, ASan/UBSan)
```

The Java suites use Docker and the same configurable stock/dependency inputs as
the compiler. They run two JVMs at a time by default; `JAVA_TEST_JOBS` overrides
that limit. `./scripts/check_java.sh` runs all groups plus stock linkage;
the native contract probe also needs Python 3 and a host C compiler.
`./scripts/test_java_build.sh` checks reproducible builds, resource failures and
the original macOS input conventions. The latter uses a simulated Darwin
launcher, so actual macOS/Apple Silicon execution still needs validation. Full toolchain,
threading, boot and the complete test list live in the knowledge base - see
[`docs/architecture.md`](docs/architecture.md).

## 🚀 Deployment

**Compatibility.** The patch is not limited to US, EU or CN units, nor to one MU train: it is
meant for any MHI2Q MU firmware (developed on MU1316). What matters is:

- a fully digital instrument cluster (Audi virtual cockpit); cars with an analog cluster are not
  supported;
- preferably, the latest firmware available for the unit, flashed before installing the patch.

With both in place it should almost certainly work, as long as nothing went wrong during the
install itself.

A release is eight files plus two config edits; nothing stock is replaced and no firewall profile is
touched:

| On-unit path | Files |
| --- | --- |
| `/mnt/app/root/hooks/` | `libcarplay_hook.so`, `maneuver_render` (from `build/`), `flag_atlas.rgba` (from `maneuver_render/resources/`), `carplay_startup.sh`, `carplay_monitor.sh`, `carplay_processes.sh`, `carplay_cleanup.sh` (from `deploy/smartphone_integrator/`) |
| `/mnt/app/eso/hmi/lsd/jars/` | `carplay_hook.jar` (from `build/`) |
| `/mnt/system/etc/eso/production/smartphone_integrator.json` | `children.carplay` replaced by [`carplay_child.json`](deploy/smartphone_integrator/carplay_child.json) |
| `/mnt/system/etc/eso/production/dio_manager.json` | `MessagesSentByAccessory` += `0x5200`, `0x5203`; `MessagesReceivedFromDevice` += `0x5201`, `0x5202`, `0x5204` |

Both the `dio_manager.json` IDs and the hook's runtime Identify patch are required: without the IDs
iOS sends route guidance and the SDK silently drops it.

**With M.I.B. (recommended).** Copy `install_MoreIncredibleBash/` to the M.I.B. SD card and drop
**all assets of a release** straight into `mod/carplay/` (the eight files above plus
`carplay_child.json`; no folders needed), then run **GEM -> M.I.B. -> Advanced Settings -> Run Custom Script** (**Run individual script** on
M.I.B. release zips up to V3.7.1) with CarPlay disconnected. `custom.sh` checks that the whole
release is on the card (a partial copy stops before anything is written), copies it with atomic
renames, patches both configs in place and keeps a `.carplay-stock` backup of each; it never stops
processes or reboots. It also deletes M.I.B.'s NavActiveIgnore jar, which breaks CarPlay's app
state. To remove everything, run `uninstall_MoreIncredibleBash/` the same way.

**Manually** (no M.I.B.; needs a root shell on the unit over SSH or Telnet). `mount -uw /mnt/app` and `/mnt/system`, copy the files, back up and
edit the two configs as text (`dio_manager.json` has `##` comment lines - no JSON tools).

The step-by-step guide for both - the SD layout, installer output and warnings, the exact SI child and
`dio_manager.json` lines, verification greps, uninstall and the SSH traps - is
[`docs/deploy/install.md`](docs/deploy/install.md).

**Reboot.** Disconnect CarPlay, run `sync` and wait a few seconds, then reboot normally: a forced
reboot (the MMI button combo) right after copying can leave the files truncated or missing. The jar is
on j9's boot classpath, so it only loads after a full restart. On boot `smartphone_integrator` launches
everything; check `/tmp/carplay_hook.log` and `/tmp/carplay_java.log` (see [Logging](#-logging)).

Exact ownership rules, the `LD_PRELOAD`/env constraints and the MU1316 QNX-compat audit are in
[`deploy/smartphone_integrator/README.md`](deploy/smartphone_integrator/README.md).

## 📝 Logging

Everything logs to `/tmp` on the unit:

| File | Source |
| --- | --- |
| `/tmp/carplay_hook.log` | native hook (inside `dio_manager`) |
| `/tmp/carplay_java.log` | Java patch (bounded + rotated, `.1` = previous) |
| `/tmp/maneuver_render.log` | cluster maneuver renderer |
| `/tmp/carplay_wrapper.log` | startup wrapper and renderer monitor |

By default only warnings and errors are recorded. To capture **everything** (lift hook and Java to
`INFO`), drop a marker file on the unit - no rebuild needed:

```sh
touch /mnt/app/carplay_verbose        # survives reboot; /tmp/carplay_verbose does not
```

Hook and Java read the marker at every CarPlay session start, so it takes effect on the next phone
connect - no reboot. Remove the marker to return to the quiet default. Logs reset on reboot, so pull
them before restarting.

For raw route-guidance packet dumps, rebuild the hook with `LOG_RGD_PACKET_RAW=1` (see [Build](#-build)).

**No shell? Use M.I.B.** Copy `logging_MoreIncredibleBash/` to the card and run it like the installer.
Each run saves everything to `<card>/carplay_logs/NNN/` and then creates `/tmp/carplay_verbose`: run it
once, reconnect the phone and drive with CarPlay, run it again - the second folder holds the verbose
session. Attach that folder to a bug report.

## 📚 Documentation

`docs/` is a Markdown knowledge base (also opens in Obsidian) - one note per topic, each fact validated against code /
firmware / iOS binary. Start at [`docs/INDEX.md`](docs/INDEX.md): architecture & threading, the hook
and bus, route guidance (TLV → BAP → cluster, lanes, route text), cluster compositing and the maneuver
renderer, input, deploy/connect, build & host tests, the reverse-engineering references, and a
per-note verification status.

## 🤝 Help wanted

PRs are welcome - bug fixes, new maneuver cases, docs, on-car test reports.

**Reporting a bad maneuver icon.** The iAP2→BAP mapping covers all 54 CarPlay maneuver types but has
only been exercised on a limited set of real routes. A snippet of `/tmp/carplay_hook.log` from the
moment plus a note on what was expected helps a lot. The hook logs unrecognised route-guidance messages
as `[HOOK] Unknown 0x52xx msgid=0xNNNN dir=IN len=N` followed by a hex dump - that line is the best
starting point when iOS sends a maneuver type we don't handle yet.

## 🔗 References

Thanks for the prior work and knowledge that helped figure this out.

- https://github.com/ludwig-v/wireless-carplay-dongle-reverse-engineering
- https://github.com/EthanArbuckle/iPhone18-3_26.1_23B85_Restore
- https://github.com/adi961/mib2-android-auto-vc
- [@fifthBro](https://t.me/fifthBro)

---

<sub>What are you doing all the way down here? There's nothing to see…</sub>

<details>
<summary>…or is there?</summary>

<br>

### Coming soon. Maybe. Someday. No promises.

It was just the warm-up, next:

<p align="center">
  <img src="assets/coming-soon.jpg" width="70%" />
</p>

- **AltScreen** - full CarPlay map, right in cluster
- **Multichannel audio support** - from stereo up to 6- or even 8-channel
- **Apple Spatial Audio**
- **Dolby Atmos** - High Quality 5.1.2 masters
- **Video playback** - an Apple TV on wheels

Stay tuned. 👀

</details>

---
