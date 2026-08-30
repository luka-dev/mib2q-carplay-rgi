# CarPlay child supervisor deployment

Replace `children.carplay` in
`/mnt/system/etc/eso/production/smartphone_integrator.json` with
[`carplay_child.json`](carplay_child.json).

The important ownership rule is that `children.carplay.envs` contains **no**
`LD_PRELOAD`. `carplay_startup.sh` applies the hook only to its direct
`dio_manager` child. This prevents the hook from loading into `/bin/sh` or
`maneuver_render`.

Keep `IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production` in the child environment.
The supervisor inherits it and passes it unchanged to `dio_manager`; only the
hook variable is added on the final `dio_manager` command.

Install these executable files under `/mnt/app/root/hooks`:

- `carplay_startup.sh`
- `carplay_cleanup.sh`
- `carplay_processes.sh`
- `libcarplay_hook.so`
- `maneuver_render`
- `flag_atlas.rgba`

Do not overwrite `/etc/scripts/carplay_cleanup.sh`: the custom cleanup calls that
stock script only for Audi's mdnsd/PPS cleanup. Renderers are persistent services;
an explicit supervisor stop, not ordinary dio churn, owns their final teardown.

Lifecycle semantics:

- if `maneuver_render` already exists when CarPlay starts, it is not restarted;
- renderer PIDs are kept under `/tmp`; one finite `pidin ar` snapshot seeds the
  files when adopting an older running generation, then startup and the monitor
  use constant-time `/proc/<pid>` checks instead of repeatedly scanning every
  process while `dio_manager` initializes;
- the wrapper `exec`s `dio_manager`, so smartphone_integrator tracks the exact dio
  PID; a monitor shell restarts only a genuinely missing renderer while it lives;
- every ordinary `dio_manager` exit leaves `maneuver_render` alive, so a replacement
  inherits the existing EGL allocations instead of re-entering fragile Qualcomm
  `eglInitialize`; only a missing/crashed renderer is restarted, after a 5 s backoff;
- `maneuver_render` is a client of Java's route-scoped `:19800` listener, so it is
  preserved by live PID while RGI/Java intentionally has the listener closed;
- external cleanup is identity-less (smartphone_integrator does not tell it which dio
  generation it runs for), so it does NOT create the global stop marker and does NOT
  stop renderers by the shared PID files — either could damage an already-started
  replacement generation. Cleanup only invokes
  Audi's `/etc/scripts/carplay_cleanup.sh` for the stock mdnsd/PPS cleanup.

Verify on the unit in `/tmp/carplay_wrapper.log`: `already running - leave untouched`
or `starting ...`, followed on dio exit by `persistent renderer left alive`.
`SIGKILL residual` is valid only after an explicit supervisor stop.

CarPlay uses `restartDelay: 3000`, `stopTimeout: 8000`; `portResetTime` remains 250 ms.
The full settling delay is intentional: rapid OTG stop/start cycles can leave the
iAP2 NCM accessory with only one of its two interfaces enabled. Renderer ownership
is handled independently by the PID-snapshot cleanup and must not be used as a
reason to shorten the USB/device-stack delay.

## MU1316 QNX 6.5 compatibility audit

The supervisor deliberately uses only facilities present in the extracted P5087
firmware:

- `/bin/sh` is QNX PD KSH 5.2.14 (`VERSION=650-4423`) and supports the functions,
  traps, command substitution, background jobs, `read`, and `exec` used here;
- `kill` is a shell builtin; the scripts use **numeric** signals (`kill -15`, `kill -9`)
  because that form is accepted by every kill implementation. Stock scripts kill by name
  via `slay -f -s SIGTERM -m name` and, in one spot, `kill -sigkill <pid>`; we kill by the
  captured PID snapshot instead (slay-by-name would also hit the replacement generation),
  so only the signal token had to be a universally-accepted numeric one;
- `pidin ar` supplies a finite process snapshot; no blocking `/proc/*/cmdline`
  walk and no GNU process utilities are used. It is only the compatibility
  fallback for adopting pre-existing renderers; normal checks use recorded PIDs;
- `rm -f` and integer `sleep` are stock facilities; the original firmware uses
  `sleep` throughout `startup.sh` and `/etc/scripts`;
- `/usr/bin/grep` is a stock MU1316 binary, used only by the USB pre-SETUP
  diagnostic;
- the shared process helper appends a safe subset of the stock MMX command search
  locations and retains the inherited head, so its utilities do not depend on
  `smartphone_integrator` preserving `PATH` and dio keeps its original resolution order.

No GNU-only commands or options are used: there is no `pgrep`, `pkill`, `wait -n`,
GNU `stat`, `readlink -f`, `timeout`, fractional sleep, or dependency on `tr`.
