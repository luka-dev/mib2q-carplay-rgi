# Deployment and process ownership

The production child definition is `deploy/smartphone_integrator/carplay_child.json`. Keep
`IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production` in that child environment and keep `LD_PRELOAD` out
of it. `carplay_startup.sh` adds `LD_PRELOAD` only to its final `dio_manager` process.

Runtime files installed together under `/mnt/app/root/hooks/`:

```text
libcarplay_hook.so
maneuver_render
altscreen_render
flag_atlas.rgba
carplay_startup.sh
carplay_processes.sh
carplay_cleanup.sh
```

The wrapper starts missing renderers, preserves healthy ones during a fast `dio_manager` replacement,
and records an owner generation. Its monitor restarts a renderer only while that exact dio generation
is alive. On final exit it stops the captured renderer PIDs and exits.

The custom cleanup first blocks renderer respawn, releases renderer resources and then invokes Audi's
stock `/etc/scripts/carplay_cleanup.sh` for mdnsd/PPS cleanup. It must not overwrite that stock script.

Renderer processes must not also be added as `servicemgr external:true` services; dual ownership would
immediately restart processes that the CarPlay cleanup intentionally stopped.

See `deploy/smartphone_integrator/README.md` for installation paths and the exact child JSON replacement.

For hook-only iteration, `scripts/deploy_unit.sh` uploads `build/libcarplay_hook.so`, keeps one `.bak`
copy and restarts only `dio_manager`. It intentionally does not restart or redeploy Java.
