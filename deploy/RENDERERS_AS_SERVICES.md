# Renderer lifecycle: CarPlay child supervisor

The earlier always-on `servicemgr:{external:true}` design is superseded.

`maneuver_render` and `altscreen_render` are now owned exclusively by
`deploy/smartphone_integrator/carplay_startup.sh`:

- existing renderer processes are left untouched at CarPlay startup;
- a missing renderer is started, and restarted while `dio_manager` is alive;
- when `dio_manager` exits, both renderers receive SIGTERM and release their
  EGL/OMX/Screen resources; only a residual process is SIGKILLed;
- the wrapper then exits, so nothing remains running outside a CarPlay session.

Do **not** add renderer entries to `framework.json`: servicemgr would restart them
after the CarPlay supervisor intentionally stops them. The entries have been
removed from `deploy/framework.json`.

Deployment and the exact `children.carplay` replacement are documented in
`deploy/smartphone_integrator/README.md`.
