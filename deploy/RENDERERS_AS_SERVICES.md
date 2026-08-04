# Renderer lifecycle: CarPlay child supervisor

The earlier always-on `servicemgr:{external:true}` design is superseded.

`maneuver_render` and `altscreen_render` are now owned exclusively by
`deploy/smartphone_integrator/carplay_startup.sh`:

- existing renderer processes are left untouched at CarPlay startup;
- a missing renderer is started, and restarted while `dio_manager` is alive;
- on a fast dio replacement, the new owner generation keeps the existing
  renderers, avoiding a localhost disconnect between SPS/PPS/IDR and P-frames;
- when no replacement appears, only the exact renderer PIDs captured for the old
  generation receive SIGTERM/SIGKILL, so delayed cleanup cannot kill a new process;
- the wrapper then exits, so nothing remains running outside a CarPlay session.

Do **not** add renderer entries to `framework.json`: servicemgr would restart them
after the CarPlay supervisor intentionally stops them. The entries have been
removed from `deploy/framework.json`.

Deployment and the exact `children.carplay` replacement are documented in
`deploy/smartphone_integrator/README.md`.
