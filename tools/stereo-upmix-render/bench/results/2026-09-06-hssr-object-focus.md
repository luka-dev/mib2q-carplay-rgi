# HSSR Object Focus checkpoint — 2026-09-06

`hssr-object-focus` is the assertive successor to Object V2.  It retains V2's
two persistent half-plane slots and measured source-vector projection, then
hard-steers only the extracted object to one rear side.  The output steering
vector starts at absolute pan `0.78` (about -18 dB opposite-side object
crossfeed), while an object-only presence curve favours roughly 0.8-11 kHz.
The diffuse safety bed is reduced and exact common-mode input still cannot
acquire an object slot.

The rear rail remains capped at `0.25`, FC remains zero, and latency remains
512 frames.  Object Focus uses `2.5 x` front transfer instead of V2's `2.0 x`,
so exact stereo fold-back is deliberately not its objective.

## Controlled corpus

Command used the existing 24-track held-out manifest, the same HSSR model,
rear `0.25` and LFE assist for both backends.

| backend | rear envelope corr | rear waveform corr | rear SI-SDR dB | rear L/R corr error | rear energy ratio | front SNR dB | fold-back SNR dB |
|---|---:|---:|---:|---:|---:|---:|---:|
| hssr-object-v2 | 0.367 | 0.467 | -5.73 | 0.390 | 0.144 | 14.33 | 18.12 |
| hssr-object-focus | 0.406 | 0.449 | -6.25 | 0.321 | 0.177 | 10.45 | 13.34 |

Focus therefore raises the held-out rear energy ratio by about 23%, improves
rear envelope agreement and reduces the L/R correlation error.  The lower
front/fold SNR is expected from stronger intentional relocation and must be
accepted or rejected by on-unit listening, not treated as a transparent-upmix
quality win.

## Host timing

512 measured calls after 128 warmups, 1024 frames/call:

| backend | mean | P95 | P99 | max |
|---|---:|---:|---:|---:|
| hssr-object-v2 | 0.225 ms | 0.241 ms | 0.260 ms | 0.270 ms |
| hssr-object-focus | 0.231 ms | 0.245 ms | 0.258 ms | 0.302 ms |

The mean host delta is about +2.5%; state size is unchanged.  Host wall time is
only a regression check, not a Krait timing claim.

## Verification

- all `scripts/test_hook_audio.sh` tests pass;
- a dedicated synthetic test proves Focus produces at least a 25:1 rear energy
  ratio for an identifiable 2:1 coherent source while V2 preserves its
  original image;
- exact mono remains below the existing no-invented-object rear-energy bound;
- SoundAmp matrix headroom tests pass at maximum configured rear/LFE gains;
- the full QNX ARMv7 hook builds with GCC 8.5 and passes the existing emutls,
  constructor, C++ runtime and dependency guards.

## Vehicle configuration

The runtime policy is now only:

```sh
export AUDIO_STEREO_UPMIX=on   # or off for exact L,R,0,0,0,0 rollback
```

The enabled renderer is selected while building:

```sh
UPMIX_PROFILE=hssr-object-focus QNX_TOOLCHAIN_VARIANT=8.5 \
  ./scripts/build_hook.sh
```

The checked-in build default is `hssr-object-focus`.
