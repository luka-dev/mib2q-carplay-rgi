# Protector — VIN-locked builds

Bind a CarPlay build to one car. On the target VIN the mod runs; on any other
VIN the hook stays inert (stock CarPlay) and the APAC codec refuses (fail-closed).
The binding is read live on the unit and the two native `.so` are stamped at
build time — no plaintext VIN in the artifacts.

- Source of the mechanism: `protector/` (portable C, no OpenSSL).
- Deep API / internals: `protector/README.md`.
- This doc: how to **use** it and **compile** a protected CarPlay build.

---

## 1. What it does (short)

- Two `.so` (`libcarplay_hook.so`, `libcarplay_apac.so`) each carry a small VIN
  gate plus a stamped slot.
- At load the gate reads the **live VIN** from the MMX persistence store
  (`/var/fw/persistence.sqlite`: `/core/vin` + `/core/registeredVin`, quorum) and
  checksums the `.so`'s own on-disk image.
- The stamp is `KDF(secret, VIN, CRC)` over a sentinel — so it verifies **only**
  when the unit's live VIN equals the VIN baked at build, and only if the `.so`
  was not patched.
- Fail-open: any error (no/disagreeing VIN, unreadable image, unstamped) →
  inert/stock, never a brick.

Hybrid deploy model:
- **Whole-file** in the rootfs tree: hooks/`.so`/jar + the already-patched
  firmware (`smartphone_integrator`, `deva-ctrl-qc.so`).
- **In-place** on the unit by `custom.sh`: `smartphone_integrator.json` (by path)
  and pf profiles (mandatory).

---

## 2. Prerequisites

- Docker with the QNX 6.5 armv7 image `qnx65-armv7-toolchain` (from
  `Tools/qnx-65-sdp-docker`). Build it once:
  ```sh
  docker build --platform=linux/amd64 -t qnx65-armv7-toolchain Tools/qnx-65-sdp-docker
  ```
- `python3` on the host (runs `protector/protect.py`, the stamper).
- The other artifacts already built into `build/` (jar, renderers, models,
  firmware). Run `./scripts/build_java.sh` and `./scripts/build_renderers.sh`
  first for a complete package.

---

## 3. Build a protected CarPlay build (the normal path)

The VIN is provided pre-build; stamping happens inside the build.

```sh
./scripts/build_hook_protected.sh WAUZZZF45HA149423          # stamped binaries -> build/protected_<VIN>/
HOOK_AUDIO=0 LOG=0 ./scripts/build_hook_protected.sh <VIN>   # beta: no audio, no logs
```

`build_hook_protected.sh` compiles and VIN-stamps the two `.so` into
`build/protected_<VIN>/` — binaries only. Assemble the install tree by hand from
there plus the runtime assets (model, firmware, renderers); the source→on-unit
map is in `docs/RELEASE_ARTIFACTS.md`. `build_hook_protected.sh` and
`build_hook.sh` share one compile+verify core
(`scripts/build_hook_common.sh`), so the protected `.so` can never drift from
the shipping one. `build_hook.sh` itself is unprotected only.

- `PROT_VIN` — the 17-char target VIN. Setting it makes the build protected.
- `PROT_SECRET` — optional. When unset, a fresh random secret is generated per
  build and thrown away (the runtime value and the stamp are threaded from the
  same value, so a one-off build is self-consistent). Set it explicitly, kept
  OUT of the tree, only to pin one stable fleet secret across rebuilds — that is
  also what a wrong-secret rebuild is rejected against.
- Default build (no `PROT_VIN`) is byte-for-byte unchanged — protection is opt-in.

Output → `install_MoreIncredibleBash/mod/carplay/`:

```
carplay/root/                         the "/" rootfs tree, files at real paths
  mnt/app/root/hooks/libcarplay_hook.so   (stamped for the VIN)
  mnt/app/root/hooks/libcarplay_apac.so   (stamped for the VIN)
  mnt/app/root/hooks/{models, renderers, carplay_*.sh}
  mnt/app/eso/hmi/lsd/jars/carplay_hook.jar
  mnt/app/eso/bin/apps/smartphone_integrator      (whole-file, if built)
  mnt/app/armle/lib/dll/deva-ctrl-qc.so           (whole-file, if built)
carplay/carplay_child.json            installer resource for the json patch
PROTECTED.txt                         neutral marker (VIN, date) - no mechanism
```

QNX cross-build variant: `QNX_TOOLCHAIN_VARIANT=8.5 ./scripts/build_hook_protected.sh <VIN>`.

### Build flags (all `env=value` before the command)

| flag | default | effect |
|---|---|---|
| `PROT_VIN` | — | 17-char target VIN. Set it (+`PROT_SECRET`) to make a protected build. |
| `PROT_SECRET` | — | optional build secret; keep out of the tree. Unset ⇒ random per-build. |
| `HOOK_AUDIO` | `1` | `0` = **compile-time** removal of the type-103 audio path: no QSA/AirPlay audio interposers, no `probe103`/`mainBuffered`, and `libcarplay_apac.so` is not built or shipped (stock CarPlay audio). `AUDIO_SPATIAL=0` only disables 103 at runtime; `HOOK_AUDIO=0` takes it out of the binary. |
| `LOG` | `1` | `1` = logging compiled in; `0` = pre-prod, logging + captures disabled. |
| `AUDIO_STATE_TRACE` | `0` | `1` = compact lifecycle trace (needs the two trace HID seams). |
| `ALTSCREEN_PACE_TRACE` | `0` | `1` = stream-111 pacing trace. |
| `LOG_RGD_PACKET_RAW` | `0` | `1` = raw RGD packet logging (requires `LOG=1`). |
| `QNX_TOOLCHAIN_VARIANT` | `latest` | `8.5` selects the GCC 8.5 image. |

`AUDIO_SPATIAL` is **not** a build flag — it is a runtime env in
`carplay_startup.sh` (`AUDIO_SPATIAL=0/1`). With `HOOK_AUDIO=1` it gates 103 at
runtime; with `HOOK_AUDIO=0` the audio path is gone regardless.

Worked example — the current fleet build (103 out, logs on, spatial off, locked):
```sh
# ensure carplay_startup.sh has AUDIO_SPATIAL=0 (runtime belt-and-braces)
rm -f build/libcarplay_apac.so            # never ship a stale codec in a HOOK_AUDIO=0 build
HOOK_AUDIO=0 LOG=1 \
  ./scripts/build_hook_protected.sh WAUZZZF45HA149423
```
Result: `mod/carplay/root/` has 12 files (no `libcarplay_apac.so`); the hook has
0 `spa_apac_codec_get_api` / `probe103` / QSA symbols.

> The `carplay/root/` tree, `carplay_child.json` and `PROTECTED.txt` are build
> artifacts (gitignored). Regenerate them with the build; don't hand-edit.

---

## 4. Install / uninstall on the unit

Deploy the whole `install_MoreIncredibleBash/mod/` to the M.I.B. SD, then:

- **Install:** M.I.B. → Advanced Settings → Run Custom Script (`mod/custom.sh`).
  It remounts `/mnt/app` + `/mnt/system` rw, lays `carplay/root/` onto `/`
  (atomic rename per file), patches `smartphone_integrator.json` (replaces the
  `"carplay"` child) and all pf profiles (`6030` → `6030:6032`), then `sync`.
  Reboot the HU.
- **Uninstall:** deploy `uninstall_MoreIncredibleBash/mod/` and Run Custom
  Script. It restores every `*.carplay-stock` (firmware, json, pf) and removes
  the files we own (hooks/`.so`/jar). Reboot.

Verify after install (on the unit):
```sh
grep carplay_startup.sh /mnt/system/etc/eso/production/smartphone_integrator.json
grep -o '6030:6032' /mnt/system/etc/pf.conf
```
The mod only goes ACTIVE when the unit's live VIN matches the baked VIN.

> `carplay_child.json` MUST sit in `mod/carplay/` next to `root/`, or the json
> patch is skipped (custom.sh warns) and the hook never loads. The build places
> it; if you assemble the SD by hand, include it.

---

## 5. Protecting your own artifact (integration)

To VIN-lock another `.so`/binary, use the same three pieces. See
`protector/README.md` for the full API; the CarPlay wiring lives in
`protector/carplay_prot.{c,h}` and is the simplest example.

**a. Define the slot + gate once** (`protector/carplay_prot.c`):
```c
#include "prot_rt.h"
PROT_DEFINE_SLOT();                 /* one slot per .so; kept against --gc-sections */

int carplay_prot_active(const char *self_path) {
    static int done = 0, active = 0;
    if (!done) {
        static const unsigned char none = 0;
        prot_arm_path(self_path, 0x5A, &none, 0);   /* live VIN + own image */
        active = prot_active();
        done = 1;
    }
    return active;
}
```

**b. Gate the feature** at its single choke point (fail-open to stock):
```c
if (!carplay_prot_active("/mnt/app/root/hooks/libcarplay_hook.so"))
    return;                         /* wrong VIN -> inert, stock path runs */
```

**c. Compile the protector objects into the artifact** (hidden visibility so no
new dynamic exports; pure C, no constructor, so a strict audit like APAC's
stays green):
```sh
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
for p in prot_rt prot_vin carplay_prot; do
  $CC -O2 -fPIC -fvisibility=hidden -std=gnu99 -Iprotector \
      -DPROT_SECRET="\"$PROT_SECRET\"" -c protector/$p.c -o $p.o
done
# link $p.o into your .so with -DCARPLAY_PROT on the gate TU
```

**d. Stamp the built artifact for the VIN** (blind sentinel gate):
```sh
printf '\x5a' > consts.bin
PROT_SECRET="$SECRET" python3 protector/protect.py your.so \
    --vin WAUZZZF45HA149423 --mode blind --payload-file consts.bin
```

`build_hook_protected.sh` already does (c) and (d) for the two CarPlay `.so`
— this section is only for new artifacts.

Gotcha: the runtime finds the slot by scanning the file, so nothing references
`PROT_SLOT` by symbol; a `-fdata-sections/--gc-sections` build will drop it
unless retained. `carplay_prot.c` keeps it alive with a volatile self-read — do
the same in any new slot owner.

---

## 6. Verify a stamped `.so`

Locally, gate against a fake store (right vs wrong VIN):
```sh
printf '{"p":"\\/core\\/vin","d":"WAUZZZF45HA149423"}{"p":"\\/core\\/registeredVin","d":"WAUZZZF45HA149423"}' > /tmp/right.sqlite
cat > /tmp/v.c <<'C'
#include "carplay_prot.h"
#include <stdio.h>
int main(int c,char**v){printf("%d\n",carplay_prot_active(v[1]));return 0;}
C
cc -O2 -Iprotector -DPROT_SECRET="\"$SECRET\"" -DPROT_MMX_STORE='"/tmp/right.sqlite"' \
   -o /tmp/v /tmp/v.c protector/carplay_prot.c protector/prot_rt.c protector/prot_vin.c
/tmp/v install_MoreIncredibleBash/mod/carplay/root/mnt/app/root/hooks/libcarplay_hook.so   # -> 1
```
Point `PROT_MMX_STORE` at a store with a different VIN → `0`. Confirm no plaintext
VIN leaked: `grep -c <VIN> <the .so>` → `0`.

---

## 7. Secret management & honest ceiling

- `PROT_SECRET` is compiled into the runtime and passed to `protect.py`. Keep it
  out of the repo (CI/provision secret). Use the same secret across a fleet.
- **Ceiling:** the secret lives in the image, so a rooted attacker who runs a
  provisioned image once can extract the derived key; `/mnt/persist` is rw so the
  VIN records are editable (editing them yields inert/stock, never premium). This
  is **anti-clone / anti-casual-patch**, not cryptographic secrecy. A real wall
  needs an off-box secret (TrustZone/secure storage or a server challenge) — the
  code doesn't change, only where `secret` comes from.
- **OPSEC:** never describe the protection mechanism in files that ship on the SD
  (e.g. `PROTECTED.txt` stays neutral: VIN, date, install hint only).
