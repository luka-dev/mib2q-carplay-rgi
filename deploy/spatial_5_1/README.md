# Discrete 5.1 CarPlay — deploy bundle

Makes the iPhone push **discrete 5.1 PCM** over wired CarPlay to the B&O amp, instead of stereo.
Two HU-side pieces, no amp-side change. Full RE in `docs/reference/AUDIO.md` §9–11.

## The chain (all verified static)
```
iPhone --(advertised 5.1 bit)--> libairplay hook: AirPlayAudioFormatToASBD -> 6ch ASBD
       -> opens mpl5_dio_ent @ 6 voices  (needs the deva patch below)
       -> deva-ctrl-qc mixer 6ch path (mask 63)
       -> six_channel_speakerp (6ch output, io-audio default)
       -> TDM/MOST -> B&O amp (already 5.1-native; used today by mpl1_int_ent=6ch)
```
The amp and the 6ch path already exist and are exercised by internal media. The only two changes are
on the HU: advertise+emit 6ch (hook) and lift the CarPlay device's stereo cap (deva).

## Components
| piece | file | on-unit target | activation | reboot? |
|---|---|---|---|---|
| libairplay hook | `build/libcarplay_hook.so` | `/mnt/app/root/hooks/libcarplay_hook.so` | marker `/mnt/app/carplay_spatial_5_1` → `SPATIAL_5_1=1` (wired in `carplay_startup.sh`) | no — next dio fork |
| deva driver patch | `deploy/deva-ctrl-qc/deva-ctrl-qc.so` | `/mnt/app/armle/lib/dll/deva-ctrl-qc.so` | swap the .so | **yes** — io-audio/deva restart |

The hook is already part of the CarPlay hook deploy; the only new hook change is the `SPATIAL_5_1`
marker gate added to `carplay_startup.sh` (mirrors the altScreen marker). OFF by default → base stereo
CarPlay is byte-identical to stock on every iOS.

## Install (staging only — never reboots)
```
sh deploy/spatial_5_1/install_spatial_5_1.sh            # dry-run, shows the plan
sh deploy/spatial_5_1/install_spatial_5_1.sh --apply    # copy deva .so + arm marker
```
`--apply` copies the patched deva `.so` and `touch`es the marker. It does **not** restart io-audio,
relaunch dio, or reboot. No on-device `.bak` (stock deva is recoverable from the firmware image).

## Activation order (when you decide to go — ask first for the reboot)
1. Ensure the hook build is staged (`libcarplay_hook.so` in `/mnt/app/root/hooks/`) and
   `carplay_startup.sh` (with the `SPATIAL_5_1` gate) is deployed.
2. `--apply` this bundle (deva .so + marker).
3. **Reboot** to bring up io-audio on the patched deva (the running lib is in use). ← needs your go.
4. Connect the phone → CarPlay main audio negotiates 5.1; verify 6 discrete channels reach the amp.

## Rollback
- `rm /mnt/app/carplay_spatial_5_1` → hook advertises stereo again (next connect).
- Restore stock `deva-ctrl-qc.so` from the firmware image → reboot. (Stock stereo also still opens
  with the patched deva when the marker is off — min_voices was kept at 2 — so rolling back the deva
  swap is only needed to fully revert.)

## Still to confirm on-unit (see deploy/deva-ctrl-qc/README.md)
- 6ch mixer element vs a stereo open when the hook is off (mixer-mask source).
- fragsize tuning (mpl5 0x1800 vs mpl1's 6ch 0x4800) if underruns appear.
- Apple channel order `L R C LFE Ls Rs` vs the amp/MOST channel order.

Nothing here has been deployed. Unit untouched.
