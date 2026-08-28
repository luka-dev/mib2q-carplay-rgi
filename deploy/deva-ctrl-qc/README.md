# deva-ctrl-qc.so — mpl5_dio_ent 2→6 voices (CarPlay discrete 5.1)

Local patch of the stock Qualcomm QNX audio driver so the CarPlay entertainment PCM device
`mpl5_dio_ent` accepts **6 voices** instead of 2. This is the HU-side half of discrete 5.1 CarPlay
(the other half is the libairplay `AirPlayAudioFormatToASBD` interposer — see `docs/reference/AUDIO.md`
§10). Nothing here touches the unit.

## Why this one byte
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

## The edit (exactly 1 byte)
Capability table `deva_mib_*` (platform `mib2`): VA `0xA7D44`, `.data` VA→file = `-0x1000`
(file `0xA6D44`). Records are 48 bytes, device index 1-based; `mpl5_dio_ent` = **idx 4**.

| field | record off | VA | file off | stock | patched |
|---|--:|--:|--:|--:|--:|
| min_voices | +20 | 0xA7E18 | 0xA6E18 | 0x02 | 0x02 *(kept)* |
| **max_voices** | +24 | 0xA7E1C | **0xA6E1C** | **0x02** | **0x06** |

`min_voices` is deliberately left at 2 so stock stereo CarPlay still opens when the libairplay hook is
OFF (`SPATIAL_5_1` unset). `cmp -l` vs stock = exactly one differing byte.

Regenerate: `./patch_deva.sh [path-to-stock-deva-ctrl-qc.so]` (idempotent; verifies idx4==mpl5_dio_ent
and the byte before/after). Output: `deva-ctrl-qc.so` here.

## Pairing / activation
Deploy together with the armed libairplay hook (`SPATIAL_5_1=1`) — the hook forces the 6ch ASBD; this
patch lets the device accept it. With the hook off, CarPlay stays stereo (min_voices=2 open still valid).

## Deploy — NOT done here (needs explicit go)
On-unit `deva-ctrl-qc.so` lives under the mounted app image (`app/armle/lib/dll/deva-ctrl-qc.so`).
Replacing it requires restarting `io-audio`/deva = the whole audio subsystem → reboot territory.
**Do not deploy or restart without asking** (per the no-reboot rule). Confirm the live path first.

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

## Open items to confirm on-unit (pending)
1. Whether the per-device mixer mask is built from `max_voices` at create-time or from the runtime
   negotiated voices. If create-time, a 2ch open into a now-6ch mixer may up-map; mirror the proven
   `mpl1_int_ent` config (`min=max=6`) if stereo-when-hook-off misbehaves. `mpl1` is the reference.
2. Fragment size: mpl5 keeps `0x1800` (6144 B); the proven 6ch device `mpl1_int_ent` uses `0x4800`
   (18432 B) to hold ~1536 frames/frag at 6ch. If underruns appear, match it (records +28/+32,
   file 0xA6E20/0xA6E24, `0x1800`→`0x4800`). Left minimal for now (ponytail: tune only if measured).
3. Sample-format field differs (mpl5 `[1]=0xC` == mpl1's — same, good; channels are orthogonal).
4. Cosmetic: `deva_mib2_channel_map` reads **min_voices** (+20), not max_voices, so a
   `QUERY/GET_CHANNEL_MAP` ioctl on mpl5 returns the stereo map. Playback is unaffected (it uses the
   hardcoded maps above). Only worth fixing if some client actually queries the map.
