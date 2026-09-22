# Discrete 5.1 CarPlay — experimental deploy bundle

Makes the iPhone push **discrete 5.1 PCM** over wired CarPlay to the B&O amp, instead of stereo.
Two HU-side pieces, no amp-side change. Full RE in `docs/reference/AUDIO.md` §9–11.

> **Status update, 2026-09-02:** the CSD width defect is fixed and confirmed on
> the car by the three-step mirror build in
> [`../../deploy/deva-ctrl-qc/BUILD_CHAIN.md`](../../deploy/deva-ctrl-qc/BUILD_CHAIN.md). The unit now
> attaches MPL5 as six-channel and the former 62.5 Hz corruption is gone. A
> subsequent one-hot test initially found L/R audible but C/LFE/Ls/Rs silent.
> The later SAD-header fix restored all six channels and was vehicle-confirmed;
> the amplifier's logical 8×8 matrix was also recovered and does not mute those
> inputs. See [`../../docs/AUDIO_5_1_STATE.md`](../../docs/AUDIO_5_1_STATE.md).
>
> **SHARC update, 2026-09-08:** the complete amplifier SPI boot/application
> stream is embedded in CodeFlash and has been extracted and validated. It is
> not an external-flash blocker. See
> [`../../docs/SHARC_APPLICATION_EXTRACTION.md`](../../docs/SHARC_APPLICATION_EXTRACTION.md).

## Current chain
```
iPhone --(advertised 5.1 bit)--> libairplay hook: AirPlayAudioFormatToASBD -> 6ch ASBD
       -> opens mpl5_dio_ent @ 6 voices  (needs the deva patch below)
       -> deva timer/DMA 6ch -> CSD route/session 6ch [FIXED AND CONFIRMED]
       -> six_channel_speakerp (6ch output, io-audio default)
       -> TDM/MOST -> B&O amp (already 5.1-native; used today by mpl1_int_ent=6ch)
```
The amp and its protected 5.1 sink exist. The HU render path is now correct through
the six-channel CSD session. Current one-hot evidence narrows the unresolved delivery
to the amplifier's source-to-output matrix.

## Components
| piece | file | on-unit target | activation | reboot? |
|---|---|---|---|---|
| libairplay hook | `build/libcarplay_hook.so` | `/mnt/app/root/hooks/libcarplay_hook.so` | `AUDIO_SPATIAL=1` | no — next dio fork |
| deva driver patch | `deploy/deva-ctrl-qc/deva-ctrl-qc.so.patched` | `/mnt/app/armle/lib/dll/deva-ctrl-qc.so` | swap the .so | **yes** — io-audio/deva restart |

`SPATIAL_5_1` is the historical control used when this bundle was written. The
current startup policy uses `AUDIO_SPATIAL=1`. Deploy the final mirror output from
the build chain, not the earlier partial `deva-ctrl-qc.so` stage.

## Install

The obsolete `install_spatial_5_1.sh` was removed: it staged the intermediate
`deva-ctrl-qc.so`, not the vehicle-confirmed `deva-ctrl-qc.so.patched`. Follow
[`../../deploy/deva-ctrl-qc/BUILD_CHAIN.md`](../../deploy/deva-ctrl-qc/BUILD_CHAIN.md) and deploy its final
mirror output. The hook flag lives in `carplay_startup.sh` and is already on by default.

## Activation order

Deploy the hook, startup profile and final mirror driver, then reboot so `io-audio`
maps the replacement. Exact order and rollback are in
[`../../deploy/deva-ctrl-qc/BUILD_CHAIN.md`](../../deploy/deva-ctrl-qc/BUILD_CHAIN.md).

## Rollback
- Set `AUDIO_SPATIAL=0` in the current `carplay_startup.sh` → stock stereo (next connect).
- Restore stock `deva-ctrl-qc.so` from the firmware image → reboot. The mirror driver also serves
  stereo correctly through QSA channel conversion, so the driver revert is only for full cleanup.

## Optional amplifier research

- Disassemble the extracted SHARC application to recover the source-to-output
  graph, or obtain a proven runtime readback. See
  [`../../docs/SHARC_APPLICATION_EXTRACTION.md`](../../docs/SHARC_APPLICATION_EXTRACTION.md).
- Preserve the vehicle-confirmed stereo and six-channel rendering behavior in
  any future amplifier-side experiment.

This README began as historical bundle documentation. Use
`deploy/deva-ctrl-qc/BUILD_CHAIN.md` as the authority for the vehicle-confirmed driver.
