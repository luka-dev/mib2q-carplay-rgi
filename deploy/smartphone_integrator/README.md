# CarPlay child supervisor deployment

Replace `children.carplay` in
`/mnt/system/etc/eso/production/smartphone_integrator.json` with
[`carplay_child.json`](carplay_child.json).

The important ownership rule is that `children.carplay.envs` contains **no**
`LD_PRELOAD`. `carplay_startup.sh` applies the hook only to its direct
`dio_manager` child. This prevents the hook from loading into `/bin/sh`,
`maneuver_render`, or `altscreen_render`.

Keep `IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production` in the child environment.
The supervisor inherits it and passes it unchanged to `dio_manager`; only the
hook and altScreen variables are added on the final `dio_manager` command.

Install these executable files under `/mnt/app/root/hooks`:

- `carplay_startup.sh`
- `carplay_cleanup.sh`
- `carplay_processes.sh`
- `libcarplay_hook.so`
- `maneuver_render`
- `altscreen_render`
- `flag_atlas.rgba`

Do not overwrite `/etc/scripts/carplay_cleanup.sh`: the custom cleanup calls that
stock script only for Audi's mdnsd/PPS cleanup. Renderer teardown belongs to the
generation-aware owner monitor in `carplay_startup.sh`.

Lifecycle semantics:

- if either renderer already exists when CarPlay starts, it is not restarted;
- renderer PIDs are kept under `/tmp`; one finite `pidin ar` snapshot seeds the
  files when adopting an older running generation, then startup and the monitor
  use constant-time `/proc/<pid>` checks instead of repeatedly scanning every
  process while `dio_manager` initializes;
- the wrapper `exec`s `dio_manager`, so smartphone_integrator tracks the exact dio
  PID; a monitor shell restarts only a genuinely missing renderer while it lives;
- an owner-generation file lets a fast replacement `dio_manager` inherit the
  existing renderers without an old monitor tearing them down;
- when no replacement generation appears, the old monitor SIGTERMs only the
  renderer PID snapshot captured when its dio exited, then force-kills only those
  same residual PIDs. It never re-scans and kills a new renderer;
- external cleanup is identity-less (smartphone_integrator does not tell it which dio
  generation it runs for), so it does NOT create the global stop marker and does NOT
  stop renderers by the shared PID files — either could damage an already-started
  replacement generation. Renderer teardown is left to the owner-monitor above, which
  holds this generation's dio PID and renderer PID snapshot. Cleanup only invokes
  Audi's `/etc/scripts/carplay_cleanup.sh` for the stock mdnsd/PPS cleanup.

Verify on the unit in `/tmp/carplay_wrapper.log`: `already running - leave untouched`
or `starting ...`, followed by either `superseded ... leave new renderers untouched`
or `gone - stopping renderers`. A `SIGKILL residual` line must name one of the old
snapshot PIDs; it must never name a renderer started by the replacement generation.

CarPlay keeps the stock `restartDelay: 2000`; `portResetTime` remains 250 ms.
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
- the shared process helper appends a safe subset of the stock MMX command search
  locations and retains the inherited head, so its utilities do not depend on
  `smartphone_integrator` preserving `PATH` and dio keeps its original resolution order.

No GNU-only commands or options are used: there is no `pgrep`, `pkill`, `wait -n`,
GNU `stat`, `readlink -f`, `timeout`, fractional sleep, or dependency on `tr`.
