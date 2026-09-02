# SD-card install (M.I.B.)

The whole patch set, installed from the card, with no ssh and no laptop in the car.

## 1. Build

```sh
./scripts/build_java.sh
./scripts/build_hook.sh
./scripts/build_renderers.sh
```

## 2. Copy to the card

The card layout is `/mod/custom.sh` plus `/mod/carplay/`. From the repository root,
with `CARD` pointing at the mounted card:

```sh
CARD=/Volumes/MIB

mkdir -p "$CARD/mod/carplay/bin"
cp deploy/card/custom.sh "$CARD/mod/custom.sh"
cp deploy/card/install.sh deploy/card/uninstall.sh "$CARD/mod/carplay/"

cp build/libcarplay_hook.so build/libcarplay_apac.so \
   build/maneuver_render build/altscreen_render build/carplay_hook.jar \
   "$CARD/mod/carplay/bin/"
cp maneuver_render/resources/flag_atlas.rgba "$CARD/mod/carplay/bin/"
cp deploy/smartphone_integrator/carplay_startup.sh \
   deploy/smartphone_integrator/carplay_processes.sh \
   deploy/smartphone_integrator/carplay_cleanup.sh \
   deploy/smartphone_integrator/carplay_child.json \
   deploy/smartphone_integrator/install_persistent_pf.sh \
   "$CARD/mod/carplay/bin/"
```

`install.sh` checks every one of those files is present before it changes anything,
so a half-copied card stops at the first line of the log rather than half-installing.

## 3. Run it

On the unit: **Advanced Settings -> Run Custom Script**, then reboot.

To uninstall, create an empty `/mod/carplay/UNINSTALL` on the card and run the custom
script again.

## What lands where

| Payload | Destination |
|---|---|
| `libcarplay_hook.so`, `libcarplay_apac.so` | `/mnt/app/root/hooks/` |
| `maneuver_render`, `altscreen_render`, `flag_atlas.rgba` | `/mnt/app/root/hooks/` |
| `carplay_startup.sh`, `carplay_processes.sh`, `carplay_cleanup.sh` | `/mnt/app/root/hooks/` |
| `carplay_hook.jar` | `/mnt/app/eso/hmi/lsd/jars/` |
| `carplay_child.json` | replaces `children.carplay` in `/mnt/system/etc/eso/production/smartphone_integrator.json` |
| `install_persistent_pf.sh` | widens the carplay0 PF rule to `6030:6032` in all four profiles |

The config is backed up once as `smartphone_integrator.json.orig` and never overwritten
after that; `uninstall.sh` restores it. Everything else we install is a new file, so
removing it is deletion.

Both scripts are idempotent, POSIX sh, and use no `sed`, `awk`, `find`, `dirname` or
`tr` — the unit has none of them.

`tests/card_install_test.sh` runs install + uninstall against a fake unit tree and checks
the patched config is still valid JSON with its other children intact.

## Related

* `scripts/deploy_over_ssh.sh` — hook-only iteration over ssh, for development. It does
  not touch the jar, the renderers or the config.
* `../smartphone_integrator/README.md` — what the supervisor does once it is installed.
