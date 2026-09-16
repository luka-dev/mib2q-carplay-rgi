# Deployment and process ownership

The production child definition is `deploy/smartphone_integrator/carplay_child.json`. Keep
`IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production` in that child environment and keep `LD_PRELOAD` out
of it. `carplay_startup.sh` adds `LD_PRELOAD` only to its final `dio_manager` process.

Runtime files installed together under `/mnt/app/root/hooks/`:

```text
libcarplay_hook.so
libcarplay_apac.so
maneuver_render
altscreen_render
flag_atlas.rgba
carplay_startup.sh
carplay_monitor.sh
carplay_processes.sh
carplay_cleanup.sh
```

The startup wrapper atomically records its generation, forks the monitor and immediately
`exec`s `dio_manager`. The monitor starts missing renderers and preserves healthy ones across
`dio_manager` replacement. It acts only while its exact generation owns the file; on dio exit it
leaves the persistent renderers alive and exits.

The identity-less cleanup invokes only Audi's stock `/etc/scripts/carplay_cleanup.sh` for mdnsd/PPS
cleanup. It does not touch shared renderer state and must not overwrite that stock script.

Renderer processes must not also be added as `servicemgr external:true` services; the generation
monitor is their sole runtime owner.

See `deploy/smartphone_integrator/README.md` for installation paths and the exact child JSON replacement.

Two ways in:

* **SD card, whole install.** `deploy/card/` — the card gets `/mod/custom.sh` plus
  `/mod/carplay/` (copy commands are in that README), and the unit runs it from
  *Advanced Settings -> Run Custom Script*. It installs every runtime file, the jar and the
  `children.carplay` replacement, backs up what it changes as `<file>.orig`, and is idempotent.
  See `deploy/card/README.md`.
* **ssh, hook-only iteration.** `scripts/deploy_over_ssh.sh` overwrites
  `/mnt/app/root/hooks/libcarplay_hook.so` and `libcarplay_apac.so` and restarts only `dio_manager`.
  It intentionally does not restart or redeploy Java.
