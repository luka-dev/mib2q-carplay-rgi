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
- `hssr_model.bin` (only when selecting the development `hssr-neon` profile)
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
`AUDIO_STEREO_LFE=assist` LR4/80 Hz bass feed. The renderer is compiled into
the hook with `UPMIX_PROFILE`; its checked-in default is `hssr-object-focus`.
The exact compatibility rollback is `AUDIO_STEREO_UPMIX=off`, followed by a
`dio_manager`/CarPlay child restart because the toggle is parsed once at
process start. See
[`../../docs/STEREO_TO_5_1_UPMIX_DESIGN.md`](../../docs/STEREO_TO_5_1_UPMIX_DESIGN.md)
for the signal equations, SoundAmp row bounds and required stationary A/B.

`UPMIX_PROFILE=ambient-hq ./scripts/build_hook.sh` selects the implemented clean-room 1024/256
STFT primary/ambient renderer without changing the SAD, codec choice or fixed
PCM6 device contract. It adds exactly 1024 frames (21.333 ms at 48 kHz) to the
synthetic Ls/Rs only; dry L/R, optional LFE and native APAC 5.1 remain
undelayed. This remains an available comparison profile, not the checked-in
startup policy.
Its host timing and QEMU retired-instruction count are regression measurements;
QEMU does not measure Krait cycles, cache/IPC behavior or realtime deadline
margin.

`UPMIX_PROFILE=hsr-td ./scripts/build_hook.sh` selects the clean-room six-band time-domain
covariance/projector candidate. It has no FFT, artificial decorrelator, or
block-fill latency; dry L/R, optional LFE and native APAC 5.1 retain the same
contracts. A GCC 8.5 ARMv7/QNX build and calibrated QEMU instruction run pass.
It remains selectable for comparison at build time.

`UPMIX_PROFILE=hssr-neon ./scripts/build_hook.sh` selects the development semantic router. It
loads `AUDIO_HSSR_MODEL`, defaulting to
`/mnt/app/root/hooks/hssr_model.bin`; a missing, corrupt, or dimension-mismatched
model is rejected and the stream safely falls back to `music-stage`. HSSR uses
a 512/128 STFT plus a quantized 2x160 GRU controller and delays fronts, rears,
and LFE together by 512 frames. Native APAC multichannel audio still bypasses
the stereo upmixer. MUSDB-trained model blobs are evaluation-only and must be
retrained on commercially cleared stems before release.

The build profile `hssr-active` uses the same model, latency and bounded
`0.25` rear rail, but gives confident background/FX and spatial-percussion
regions a stronger semantic mask. It also subtracts `1.75 x` the generated
rear sample from the matching front instead of the fold-compatible `-3 dB`
coefficient, so selected material can localize behind the listener through the
known SoundAmp matrix. This deliberately sacrifices exact stereo fold-back for
the selected component; switching back to `hssr-neon` restores the conservative
fold-compatible policy.

The build profile `hssr-object` remains the V1 assertive listening profile.
It keeps the same model, 512-frame latency, `0.25` rear bound, zero digital C,
and LFE contract. A soft per-bin mask retains stable spectral peaks and bins
whose measured pan agrees with the semantic band, suppressing the broad
inter-peak bed without binary musical-noise gating. Selected bins receive up
to `+4.1 dB` fine-bin makeup; a class-gated post-mask stage gives rearable
background/FX/instrument bands up to another `+3.2 dB`. They retain more of
their original L/R distinction and use `2.0 x` front subtraction.
`hssr-active` remains the immediate less-assertive rollback without changing
the model or PCM route.

The build profile `hssr-object-v2` is the persistent-object V2 profile.
It replaces V1's independent peak selection with two held spatial slots, a
strict ridge-to-slot match and a rank-one coherent-source projection.  The
rear gain remains capped at `0.25`, latency remains 512 frames, and digital C
remains identically zero.  Set `hssr-object` to compare/rollback V1 or
`hssr-active` to remove the object tracker while retaining semantic routing.

The checked-in `hssr-object-focus` profile keeps V2's persistent extraction
vector but hard-steers accepted objects after extraction.  Its opposite rear
crossfeed is at least about -18 dB, its object-only presence emphasis covers
roughly 0.8-11 kHz, and its diffuse safety bed is quieter.  It keeps the same
512-frame latency, `0.25` rear rail and zero digital center; a `2.5 x` front
transfer makes an accepted instrument localize behind the listener.  Runtime
still exposes only `AUDIO_STEREO_UPMIX=on|off`.

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
