# Deployment and process ownership

The production child definition is `deploy/smartphone_integrator/carplay_child.json`. Keep
`IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production` in that child environment and keep `LD_PRELOAD` out
of it. `carplay_startup.sh` adds `LD_PRELOAD` only to its final `dio_manager` process.

Runtime files installed together under `/mnt/app/root/hooks/`:

```text
libcarplay_hook.so
libcarplay_apac.so
hssr_model.bin
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

Two SD folders share one implementation:

* **Install:** copy `install_MoreIncredibleBash/` contents over the M.I.B. card
  root and put the release artifact files in `/build/` on the card.
* **Uninstall:** copy `uninstall_MoreIncredibleBash/` contents over the card root.
  Removal needs no build files or marker.

Both start at *Advanced Settings → Run Custom Script*. Over SSH,
`scripts/deploy_over_ssh.sh [root@host] [install|check|uninstall]` selects the
matching script and uploads only the necessary files.

Both require CarPlay disconnected, check `dio_manager` before writing and again
before activation, and never stop processes. Runtime files, the exact final SI/DEVA images and PF profiles are
staged and verified; the SI JSON is published last by a same-partition rename.
A manual reboot activates the release. This is atomic per file, not a transaction
across both flash partitions.

See [the installer guide](../install_MoreIncredibleBash/README_CARPLAY.md) for
artifact preparation, checks, logs, backups and uninstall. Loose release files
stay in `build/`; [the release allowlist](RELEASE_ARTIFACTS.md) defines what is published later.
