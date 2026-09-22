# CarPlay child supervisor deployment

Replace `children.carplay` in
`/mnt/system/etc/eso/production/smartphone_integrator.json` with
[`carplay_child.json`](carplay_child.json).

The important ownership rule is that `children.carplay.envs` contains **no**
`LD_PRELOAD`. `carplay_startup.sh` applies the hook only to its direct
`dio_manager` child. This prevents the hook from loading into `/bin/sh`,
`maneuver_render` or `altscreen_render`.

Keep `IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production` in the child environment.
The supervisor inherits it and passes it unchanged to `dio_manager`; only the
hook and altScreen variables are added on the final `dio_manager` command.

Install these executable files under `/mnt/app/root/hooks`:

- `carplay_startup.sh`
- `carplay_monitor.sh`
- `carplay_cleanup.sh`
- `carplay_processes.sh`
- `libcarplay_hook.so`
- `libcarplay_apac.so`
- `hssr_model.bin` (required when stereo upmix is enabled)
- `maneuver_render`
- `altscreen_render`
- `flag_atlas.rgba`

Do not overwrite `/etc/scripts/carplay_cleanup.sh`: the custom cleanup calls that
stock script only for Audi's mdnsd/PPS cleanup. Renderers are persistent services
and are not tied to an identity-less SI cleanup callback.

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
[`../../docs/Audio/Stereo to 5.1 Upmix.md`](<../../docs/Audio/Stereo to 5.1 Upmix.md>)
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
  existing RTSP connection, so 6033 is not needed. The single
  [`M.I.B./SSH installer`](../../install_MoreIncredibleBash/mod/custom.sh) widens
  `6030` to `6030:6032` in all four
  `/mnt/system/etc/pf*.conf` profiles (idempotent, `.stock` backup, `mount -uw` first). That is a real
  qnx6 flash partition (`/dev/fs0p1`); boot's `move_etc_to_ramdisk` copies it into the ramdisk
  `/etc`, so `start_network` loads the selected profile with all three ports and no runtime race. The
  wrapper does **no** firewall work — it assumes the port is open, exactly like it assumes the
  child JSON and the hook are installed. Reverted only by an OTA rewriting `fs0p1`; re-run the
  installer. See `NCM_AND_FIREWALL_IDEAL_START.md`.
- `carplay_startup.sh` performs only local file checks, an atomic owner-file
  publish, a monitor fork and immediate `exec dio_manager`; no `pidin`, network
  probe, renderer initialization or USB recovery runs in SI's startup path;
- `carplay_monitor.sh` waits one second so `exec` wins scheduling, then adopts or
  starts the persistent renderers. A replacement generation atomically replaces
  the owner PID; obsolete monitors stop acting as soon as they observe it;
- renderer PIDs are kept under `/tmp`. The healthy recurring path uses only a
  numeric check and `/proc/<pid>`. A finite `pidin ar` scan runs only for initial
  adoption or after a missing/stale PID, so a stale registry cannot create a
  duplicate renderer;
- initial adoption validates exact executable identity. `altscreen_render` also
  owns `LISTEN :19831`, which detects a process wedged before serving the hook.
  `maneuver_render` is a client of Java's optional `:19800`, so its live exact
  process identity is the only valid non-invasive readiness check;
- the wrapper `exec`s `dio_manager`, so smartphone_integrator tracks the exact
  process it started. The monitor never signals dio, touches OTG/USB state, or
  participates in SI retry policy;
- every ordinary `dio_manager` exit leaves both renderers alive. Only a genuinely
  missing renderer is restarted, after a five-second backoff. Cold boot starts
  `altscreen_render`, then gives its EGL initialization a one-second head start
  before `maneuver_render` to avoid a dual Qualcomm `eglInitialize` burst;
- external cleanup is identity-less (smartphone_integrator does not tell it which dio
  generation it runs for), so it does NOT create the global stop marker and does NOT
  stop renderers by the shared PID files — either could damage an already-started
  replacement generation. Cleanup only invokes
  Audi's `/etc/scripts/carplay_cleanup.sh` for the stock mdnsd/PPS cleanup;
- physical USB recovery is deliberately absent from these scripts. Rapid-replug
  recovery belongs to the matching NCM generation fence and the exact-version
  `smartphone_integrator` A/B candidate under `deploy/experimental/usb_replug_recovery`.

Verify on the unit in `/tmp/carplay_wrapper.log`: startup must log `exec dio_manager`
before the monitor logs `generation=... active`, followed by `adopted ...` or
`starting ...`. Ordinary dio exit logs that persistent renderers remain alive.

CarPlay uses `restartDelay: 3000`, `stopTimeout: 8000`; `portResetTime` remains 250 ms.
The full settling delay is intentional: rapid OTG stop/start cycles can leave the
iAP2 NCM accessory with only one of its two interfaces enabled. Renderer ownership
is independent of USB/device-stack recovery and must not be used as a reason to
shorten this delay.

## MU1316 QNX 6.5 compatibility audit

The supervisor deliberately uses only facilities present in the extracted P5087
firmware:

- `/bin/sh` is QNX PD KSH 5.2.14 (`VERSION=650-4423`) and supports the functions,
  traps, command substitution, background jobs, `read`, and `exec` used here;
- `kill` is a shell builtin; the scripts use **numeric** signals (`kill -15`, `kill -9`)
  because that form is accepted by every kill implementation. Stock scripts kill by name
  via `slay -f -s SIGTERM -m name`; the monitor signals only an exact PID whose renderer
  identity was positively verified, and only when initial altScreen adoption proves it wedged;
- `pidin ar` supplies a finite process snapshot; no blocking `/proc/*/cmdline`
  walk and no GNU process utilities are used. The full snapshot is only for
  adoption after a missing/stale registry entry. Targeted `pidin -p <pid> ar`
  checks identity before replacing an adopted renderer; recurring monitor checks
  use recorded PIDs only;
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
