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
- `libcarplay_apac.so`
- `hssr_model.bin` (required when stereo upmix is enabled)
- `maneuver_render`
- `altscreen_render`
- `flag_atlas.rgba`

Do not overwrite `/etc/scripts/carplay_cleanup.sh`: the custom cleanup calls that
stock script only for Audi's mdnsd/PPS cleanup. Renderers are persistent services;
an explicit supervisor stop, not ordinary dio churn, owns their final teardown.

The checked-in audio policy advertises APAC/48 kHz/5.1 index 65 plus
AAC-LC/48 kHz/stereo index 23 on the buffered stream. APAP packets carry no
format index: the decoder that accepts each changed inline cookie and its PCM
geometry select the render action. Native APAC 5.1 keeps its measured
six-channel reorder; decoded stereo uses the fixed PCM6 route with
`AUDIO_STEREO_UPMIX=on`, rear gain `0.25`, and the explicitly enabled
`AUDIO_STEREO_LFE=assist` Cabin-Aware LFE V2 feed. ON always selects the sole
`hssr-universal` renderer; there is no build-time profile selector.
The exact compatibility rollback is `AUDIO_STEREO_UPMIX=off`, followed by a
`dio_manager`/CarPlay child restart because the toggle is parsed once at
process start. See
[`../../docs/STEREO_TO_5_1_UPMIX_DESIGN.md`](../../docs/STEREO_TO_5_1_UPMIX_DESIGN.md)
for the signal equations, SoundAmp row bounds and required stationary A/B.

Universal loads `AUDIO_HSSR_MODEL`, defaulting to
`/mnt/app/root/hooks/hssr_model.bin`. A missing, corrupt, or dimension-mismatched
model leaves stereo upmix safely off. Its 512/128 STFT and quantized 2x160 GRU
controller delay fronts, rears and LFE together by 512 frames. Persistent
half-plane objects are source-projected and hard-steered; a bounded,
phase-distinct mid/high coverage path prevents dead rears while semantic
vocal/bass gates protect the front image. The rear rail remains capped at
`0.25`, digital C remains zero, and native APAC multichannel bypasses upmix.

Lifecycle semantics:

- **PF ports (deploy prerequisite, no runtime logic):** altScreen needs carplay0 TCP 6031 and
  buffered audio uses TCP 6032 beside stock 6030. `SETMAGICCOOKIE` and buffered controls reuse the
  existing RTSP connection, so 6033 is not needed. Run `install_persistent_pf.sh` once — it widens
  `6030` to `6030:6032` in all four
  `/mnt/system/etc/pf*.conf` profiles (idempotent, `.stock` backup, `mount -uw` first). That is a real
  qnx6 flash partition (`/dev/fs0p1`); boot's `move_etc_to_ramdisk` copies it into the ramdisk
  `/etc`, so `start_network` loads the selected profile with all four ports and no runtime race. The
  wrapper does **no** firewall work — it assumes the port is open, exactly like it assumes the
  child JSON and the hook are installed. Reverted only by an OTA rewriting `fs0p1`; re-run the
  script. See `NCM_AND_FIREWALL_IDEAL_START.md`.
- if either renderer already exists when CarPlay starts, it is not restarted;
- renderer PIDs are kept under `/tmp`; one finite `pidin ar` snapshot seeds the
  files when adopting an older running generation, then startup and the monitor
  use constant-time `/proc/<pid>` checks instead of repeatedly scanning every
  process while `dio_manager` initializes;
- the wrapper `exec`s `dio_manager`, so smartphone_integrator tracks the exact dio
  PID; a monitor shell restarts only a genuinely missing renderer while it lives;
- every ordinary `dio_manager` exit leaves both renderers alive, so a replacement
  inherits the existing EGL/OMX allocations instead of re-entering fragile Qualcomm
  `eglInitialize`; only a missing/crashed renderer is restarted, after a 5 s backoff;
- initial adoption checks `LISTEN :19831` only for `altscreen_render`, which owns
  that socket. `maneuver_render` is a client of Java's route-scoped `:19800`
  listener, so it is preserved by live PID while RGI/Java intentionally has the
  listener closed;
- cold boot starts `altscreen_render`, gives its EGL initialization a one-second
  head start, then starts `maneuver_render` to avoid simultaneous subdriver loading;
- external cleanup is identity-less (smartphone_integrator does not tell it which dio
  generation it runs for), so it does NOT create the global stop marker and does NOT
  stop renderers by the shared PID files — either could damage an already-started
  replacement generation. Cleanup only invokes
  Audi's `/etc/scripts/carplay_cleanup.sh` for the stock mdnsd/PPS cleanup.

Verify on the unit in `/tmp/carplay_wrapper.log`: `already running - leave untouched`
or `starting ...`, followed on dio exit by `persistent renderers left alive`.
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
- `/armle/sbin/pfctl`, `/usr/bin/sed`, and `/usr/bin/grep` are stock MU1316
  binaries. The generated PF file is syntax-checked before load, and a missing
  exact stock `6030,` token leaves the live ruleset untouched;
- the shared process helper appends a safe subset of the stock MMX command search
  locations and retains the inherited head, so its utilities do not depend on
  `smartphone_integrator` preserving `PATH` and dio keeps its original resolution order.

No GNU-only commands or options are used: there is no `pgrep`, `pkill`, `wait -n`,
GNU `stat`, `readlink -f`, `timeout`, fractional sleep, or dependency on `tr`.
