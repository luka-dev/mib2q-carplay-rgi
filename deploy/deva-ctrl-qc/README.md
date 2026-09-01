# deva-ctrl-qc.so — mpl5_dio_ent 2→6 voices (CarPlay discrete 5.1)

Local patch of the stock Qualcomm QNX audio driver so the CarPlay entertainment PCM device
`mpl5_dio_ent` accepts **6 voices** instead of 2. This is the HU-side half of discrete 5.1 CarPlay
(the other half is the libairplay `AirPlayAudioFormatToASBD` interposer — see `docs/reference/AUDIO.md`
§10). Nothing here touches the unit.

> **Live-test status (2026-08-31): the old caps-only patch was incomplete.** It opened and routed
> six voices, but retained the stock 2ch S32 DMA fragment and played exactly 3x slow. The current
> `patch_deva.sh` includes the dynamic active-fragment fix and has been staged on the unit; it is not
> active until reboot and remains untested in audio. Evidence and acceptance criteria:
> [`RE_TIMING.md`](RE_TIMING.md).

## Why caps widening plus dynamic timing
Traced statically (`docs/reference/AUDIO.md` §11). The path is:
`mpl5_dio_ent (QSA PCM) → io-audio -d qc → mixer → six_channel_speakerp (6ch) → MOST → B&O amp`.
Everything downstream is already 6-channel and **exercised today** by the internal-media device
`mpl1_int_ent` (which is 6 voices). The mixer picks its channel layout from the voice count
(`build_playback_mixer` mask `(1<<voices)-1`: 2→3=stereo, 6→63=5.1), and the output default device is
`six_channel_speakerp`. The **only** thing capping CarPlay to stereo is `mpl5_dio_ent`'s
`max_voices = 2` in the driver's static capability table. Raise it to 6 and the 6ch ASBD from
libairplay opens 6 discrete voices that ride the existing path to the amp — **no amp-side change**.

There is no command-line / `DEVA_ARGS` knob for per-device voices (`deva_parse_commandline_parameters`
is a name→id `strcasecmp` chain), so it must be a binary edit.

## Capability-table edit (4 bytes / 3 fields)
Capability table `deva_mib_*` (platform `mib2`): VA `0xA7D44`, `.data` VA→file = `-0x1000`
(file `0xA6D44`). Records are 48 bytes, device index 1-based; `mpl5_dio_ent` = **idx 4** (`0xA6E04`).
Record layout confirmed against `deva_pcm_and_mixer_device_init_platform_mib` @0x5707C, which copies
these fields straight into the `ado_pcm` hw caps at `ado_pcm_create` time:
`+0 flags, +4 format, +8 rates, +12/+16 min/max_rate, +20/+24 min/max_voices, +28/+32 min/max_fragsize,
+36 max_buffer, +40 max_frags`.

| field | rec off | file off | stock | patched | mpl1_int_ent (6ch reference) |
|---|--:|--:|--:|--:|--:|
| min_voices | +20 | 0xA6E18 | 2 | 2 *(kept)* | 6 |
| **max_voices** | +24 | **0xA6E1C** | **2** | **6** | 6 |
| min_fragsize | +28 | 0xA6E20 | 0x1800 | 0x1800 *(kept)* | 0x4800 |
| **max_fragsize** | +32 | **0xA6E24** | **0x1800** | **0x4800** | 0x4800 |
| **max_buffer** | +36 | **0xA6E28** | **0xF000** | **0x2D000** | 0x2D000 |

**Why fragsize/buffer had to move too.** The MIB hardware side is S32 with a fixed 16-ms timer:
`0x1800` is the 2ch period and `0x4800` is the 6ch period. `max_buffer` must track the widened maximum:
in every stock record `max_buffer == max_fragsize * max_frags(10)` (mpl5 `0x1800*10 = 0xF000`,
mpl1 `0x4800*10 = 0x2D000`). The active value cannot be changed statically because stock stereo must
remain `0x1800`; the dynamic code patch is documented below.

The two **min** fields are deliberately left at their stock values: all three table edits only
*widen* the accepted range. The full generated binary differs from stock in 29 bytes: these four
caps bytes plus the branch and 20-byte conditional helper described in [`RE_TIMING.md`](RE_TIMING.md).
Stock stereo still selects `0x1800`; only a negotiated six-voice acquire selects `0x4800`.

Regenerate: `./patch_deva.sh [path-to-stock-deva-ctrl-qc.so]` — idempotent, verifies
`idx4 == mpl5_dio_ent`, refuses anything but stock/already-patched values, and cross-checks the result
against the live `mpl1_int_ent` record. Output: `deva-ctrl-qc.so` here.

## What our patch does NOT disturb (checked, so it stays checked)
- **The mixer.** `deva_setup_mixer`/`build_playback_mixer` @0x3C248 switches on `mix_config[6]`, which
  `deva_pcm_and_mixer_device_init_platform_mib` hardcodes to **1** — the card mixer is a single
  volume/mute tree, unrelated to any device's voice count. (Closes the old "mixer mask from
  max_voices?" open item: there is no such coupling.)
- **`deva_mib2_channel_map`.** It reads **min_voices** (+20), not max_voices — so on our now-asymmetric
  record a `QUERY_CHANNEL_MAP` ioctl would report the stereo map. Unreachable in practice: the only
  wrapper is `snd_pcm_query_channel_map`, and a grep of the whole MU1316 image shows **no binary
  imports it** — the symbol exists solely in `libasound.so.2`. Playback never consults it either (it
  uses the hardcoded maps in `deva_csd_start_csd_session_playback`). Left alone on purpose.

## Pairing / activation
Deploy together with the armed libairplay hook (`SPATIAL_5_1=1`) — the hook forces the 6ch ASBD; this
patch lets the device accept it. With the hook off, CarPlay stays stereo (min_voices=2 open still valid).

## Deploy status
On-unit `deva-ctrl-qc.so` lives under the mounted app image (`app/armle/lib/dll/deva-ctrl-qc.so`).
Replacing it requires restarting `io-audio`/deva = the whole audio subsystem → reboot territory.
The complete dynamic build was staged there on 2026-08-31 and read back byte-for-byte. Nothing was
restarted; the old mapped generation remains active until the user-controlled reboot.

## 6 is the ceiling — 8 is NOT reachable by a byte (AUDIO.md §16)
The CarPlay spatial transport is 5.1.2 = 8 channels (AUDIO.md §15), so "why not just write 8 here?"
is the natural next question. Answer: no. `deva_mib2_channel_map` @0x54ACC switches on
`{1, 2, 3, 6}` and rejects everything else, no device in the table exceeds 6, there is no 8ch output
node (`mono_` / `stereo_` / `six_channel_speakerp` only), and `deva_csd_start_csd_session_playback`
@0x24F28 **hardcodes 6** in its `voices > 2` branch (`setStreamMultichannel(s, 6, map)` etc.) — the
count is not even read from this table. 8 channels would need real driver surgery. An 8→6 fold is
therefore mandatory on our side.

## Channel order — RESOLVED (was an open item)
The hardcoded QCOM `PCM_CHANNEL_*` maps in that branch (`FL=1 FR=2 FC=3 LS=4 RS=5 LFE=6 CS=7 LB=8 RB=9`):
stream/decoder = `01 02 03 06 08 09` = **L R C LFE Ls Rs** (Apple canonical — matches our ASBD, no
reorder needed); context/device = `01 02 08 09 03 06` = L R Ls Rs C LFE (the driver re-orders itself).

## Remaining on-unit confirmation

The caps-only run already proved that a 6-voice `mpl5_dio_ent` open succeeds and reaches all six
speakers. It also exposed the active-fragment bug. After reboot, confirm that the dynamic build now
returns a 9216-byte S16 client fragment (768 frames per 16 ms), with bounded APAC buffering and normal
pitch. Full checklist: [`RE_TIMING.md`](RE_TIMING.md).
