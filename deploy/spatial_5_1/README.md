# Discrete 5.1 CarPlay — experimental deploy bundle

Makes the iPhone push **discrete 5.1 PCM** over wired CarPlay to the B&O amp, instead of stereo.
Two HU-side pieces, no amp-side change. Full RE in `docs/reference/AUDIO.md` §9–11.

> **Do not activate this bundle for 5.1 yet.** A 2026-09-02 direct vehicle test
> proved that the current driver patch negotiates and clocks six-channel DMA
> correctly, but the runtime CSD route still initializes from MPL5's stereo
> minima (`2 / 0x1800`). Six-channel playback is therefore known-broken while
> ordinary two-channel playback is clean. Keep `AUDIO_SPATIAL=0` until the CSD
> session is dynamically re-provisioned. Full evidence:
> [`../../docs/ROUTER_INPUT_WIDTH_AUDIT.md`](../../docs/ROUTER_INPUT_WIDTH_AUDIT.md).

## Intended chain (the marked handoff is currently wrong)
```
iPhone --(advertised 5.1 bit)--> libairplay hook: AirPlayAudioFormatToASBD -> 6ch ASBD
       -> opens mpl5_dio_ent @ 6 voices  (needs the deva patch below)
       -> deva timer/DMA 6ch -> CSD route/session [CURRENTLY LEFT AT 2ch]
       -> six_channel_speakerp (6ch output, io-audio default)
       -> TDM/MOST -> B&O amp (already 5.1-native; used today by mpl1_int_ent=6ch)
```
The amp and its 6ch path exist and are exercised by internal media. Lifting the
CarPlay device cap and widening its DMA period are necessary, but the route
session must also follow the negotiated width; the current patch does not yet
do that.

## Components
| piece | file | on-unit target | activation | reboot? |
|---|---|---|---|---|
| libairplay hook | `build/libcarplay_hook.so` | `/mnt/app/root/hooks/libcarplay_hook.so` | historical `SPATIAL_5_1`; current policy is `AUDIO_SPATIAL=0` | no — next dio fork |
| deva driver patch | `deploy/deva-ctrl-qc/deva-ctrl-qc.so` | `/mnt/app/armle/lib/dll/deva-ctrl-qc.so` | swap the .so | **yes** — io-audio/deva restart |

`SPATIAL_5_1` is the historical control used when this bundle was written. The
current startup policy uses `AUDIO_SPATIAL` and defaults it to `0`. Do not turn
either form on with the current deva binary. With an unpatched driver the
six-voice open is rejected; with the current partial patch it opens but reaches
a stereo CSD consumer and sounds broken.

## Install (staging only — never reboots)
```
sh deploy/spatial_5_1/install_spatial_5_1.sh            # dry-run, shows the plan
sh deploy/spatial_5_1/install_spatial_5_1.sh --apply    # copy the patched deva .so
```
`--apply` copies the patched deva `.so`. It does **not** restart io-audio, relaunch dio, or reboot.
No on-device `.bak` (stock deva is recoverable from the firmware image). The hook flag lives in
`carplay_startup.sh` (deployed separately) — it is already on by default.

## Activation order (blocked until the CSD fix exists)
Do not use the former activation recipe with the current binary. It reproduces
the six-channel corruption. Once a dynamic CSD re-provision fix is implemented
and validated, deployment still requires the hook, startup profile, driver
copy, and a user-approved reboot of `io-audio`.

## Rollback
- Set `AUDIO_SPATIAL=0` in the current `carplay_startup.sh` → stock stereo (next connect).
- Restore stock `deva-ctrl-qc.so` from the firmware image → reboot. (Stock stereo also still opens
  with the patched deva since `min_voices` was kept at 2, so the deva revert is only for full cleanup.)

## Still to implement and confirm

- Rebuild the MPL5 CSD stream, context, and buffer pool from the negotiated
  voice count and active fragment on every 2/6 transition.
- Preserve the already-proven clean two-channel mode.
- Use the six-only minima change only as a controlled acoustic A/B, not as the
  production patch.

This README is historical bundle documentation. Consult
`deploy/deva-ctrl-qc/README.md` for the exact staged-artifact state; no reboot or
new deployment was performed during the router-width measurement pass.
