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

Do not overwrite `/etc/scripts/carplay_cleanup.sh`: the custom cleanup calls that
stock script after stopping the renderers.

Lifecycle semantics:

- if either renderer already exists when CarPlay starts, it is not restarted;
- while the direct `dio_manager` child is alive, only a missing renderer is restarted;
- when `dio_manager` exits, the supervisor SIGTERMs both renderers, force-kills only
  a one-second residual, and exits with the dio status;
- external cleanup creates `/tmp/carplay_supervisor.stop` first, so the monitor cannot
  race by respawning a renderer, then performs the same renderer cleanup and finally
  invokes Audi's `/etc/scripts/carplay_cleanup.sh` for the stock mdnsd/PPS cleanup.

Verify on the unit in `/tmp/carplay_wrapper.log`: `already running - leave untouched`
or `starting ...`, followed by `dio_manager exited ... - stopping renderers` and the
two `SIGTERM` lines. A `SIGKILL residual` line means that renderer did not complete
its normal shutdown within one second and needs separate investigation.

## MU1316 QNX 6.5 compatibility audit

The supervisor deliberately uses only facilities present in the extracted P5087
firmware:

- `/bin/sh` is QNX PD KSH 5.2.14 (`VERSION=650-4423`) and supports the functions,
  traps, command substitution, `${name%%pattern}`, background jobs, `$!`, and
  `wait <pid>` used here;
- `kill` is also a shell builtin and accepts QNX forms `-TERM` and `-KILL`;
- QNX `tr` 6.5 is installed at `/mnt/app/armle/usr/bin/tr`; its own usage data
  explicitly supports `\octal` with one to three octal digits, including the
  `tr '\000' ' '` conversion used for `/proc/<pid>/cmdline`;
- `rm -f` and integer `sleep` are stock facilities; the original firmware uses
  `sleep` throughout `startup.sh` and `/etc/scripts`;
- the shared process helper appends a safe subset of the stock MMX command search
  locations and retains the inherited head, so its utilities do not depend on
  `smartphone_integrator` preserving `PATH` and dio keeps its original resolution order.

No GNU-only commands or options are used: there is no `pgrep`, `pkill`, `wait -n`,
GNU `stat`, `readlink -f`, `timeout`, or fractional sleep.
