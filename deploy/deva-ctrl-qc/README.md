# deva-ctrl-qc.so — mpl5_dio_ent 2→6 voices (CarPlay discrete 5.1)

Local patch of the stock Qualcomm QNX audio driver so the CarPlay entertainment PCM device
`mpl5_dio_ent` accepts **6 voices** instead of 2. Nothing here touches the unit.

> **Current status (2026-09-02): do not treat this binary as a completed 5.1
> fix.** A direct six-channel `qsa_play` test now runs at the correct rate but
> sounds broken. Static analysis found why: runtime route setup copies MPL5's
> stereo `min_voices/min_fragsize` into the active CSD session even though the
> DMA timer is six-channel. The binary and `patch_deva.sh` were deliberately not
> changed in that measurement pass. See
> [`../../docs/Audio/QNX Audio Topology.md`](<../../docs/Audio/QNX Audio Topology.md>).

> **Historical timing status (2026-08-31):** the old caps-only patch retained
> the stock 2ch S32 DMA fragment and played exactly 3x slow. The current
> `patch_deva.sh` fixes that DMA-side timing. It does not fix the independent
> CSD route/session width found on 2026-09-02. Evidence:
> [`RE_TIMING.md`](RE_TIMING.md).

## Why caps widening plus dynamic timing
Traced statically (`docs/Reference/iOS CarPlay Spatial Wireformat.md` §11 and §26). The intended path is:
`mpl5_dio_ent (QSA PCM) → io-audio -d qc → mixer → six_channel_speakerp (6ch) → MOST → B&O amp`.
The physical output, MOST sink, and amplifier are already 6-channel and
**exercised today** by the internal-media device `mpl1_int_ent`. The mixer picks its channel layout from the voice count
(`build_playback_mixer` mask `(1<<voices)-1`: 2→3=stereo, 6→63=5.1), and the output default device is
`six_channel_speakerp`. `max_voices = 2` was the negotiation cap, so widening it
is necessary. It is not sufficient: the MPL5-to-CSD handoff separately reuses
the record minima as active stereo geometry. No amp-side change is needed, but
the deva route/session logic still needs a dynamic fix.

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

## The two `min` values are valid caps; their playback use is the defect

The record now differs from the proven `mpl1_int_ent` in exactly two fields, and every other one
matches byte for byte (`flags 0x2b3`, `format 0xc`, `rates 0x80`, both rates `0xbb80`, `max_buffer`,
`max_frags`). Both remaining differences are correct as capability minima:

* **`min_fragsize` 0x1800.** One static value cannot serve both modes, because the timer is a fixed
  16 ms: at 2 voices `0x1800` is 768 frames (48 kHz, right) and `0x4800` would be 2304 (3x fast);
  at 6 voices `0x1800` is 256 frames (3x slow) and `0x4800` is 768 (right). That is precisely why
  the fix is the conditional helper on `channel->voices == 6` rather than a table edit. Writing
  `0x4800` into the table would break stereo.
* **`min_voices` 2.** Raising it statically would land in the ADO caps as a
  lower bound and make every stereo open fail. The old statement that its only
  consumer was `deva_mib2_channel_map` was false. At `0x54A14..0x54A18`,
  `deva_update_session_info_from_list` also copies it to CSD session `+0x84` as
  the **active** playback channel count. The same function copies
  `min_fragsize` to session `+0x6C` at `0x54A20..0x54A28`.

**RESOLVED 2026-09-02 — and the fix was the opposite of what this section advised.**
Both minima were raised to `6 / 0x4800`, mirroring `mpl1_int_ent`, and the car confirms
discrete 5.1 works *and* stereo still works. The warning that a stereo open would fall below
`min_voices` and fail was wrong: QSA inserts a channel conversion instead of refusing, which is
how `mpl1` has served stereo clients with `min == max == 6` all along. Dynamic re-provisioning
was attempted first and could not work -- the route attaches fourteen seconds before the PCM
channel exists. See `BUILD_CHAIN.md`.

So the caps **range** is finished, but playback incorrectly treats its lower
bounds as the negotiated format. The production fix is dynamic CSD
re-provisioning, not a permanent edit of the two minima. A static change of
both minima to `6/0x4800` is useful only as a six-channel diagnostic.

## What our patch does NOT disturb (checked, so it stays checked)
- **The mixer.** `deva_setup_mixer`/`build_playback_mixer` @0x3C248 switches on `mix_config[6]`, which
  `deva_pcm_and_mixer_device_init_platform_mib` hardcodes to **1** — the card mixer is a single
  volume/mute tree, unrelated to any device's voice count. (Closes the old "mixer mask from
  max_voices?" open item: there is no such coupling.)
- **`deva_mib2_channel_map`.** It reads **min_voices** (+20), so a
  `QUERY_CHANNEL_MAP` ioctl on the asymmetric record reports stereo. The ioctl
  remains unreachable in the image, but `min_voices` is not playback-inert:
  the independent session updater consumes it as described above.

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

## Stock PCM dump — ground truth from the unit, no reboot

The driver carries Qualcomm's own capture facility, and it is reachable on a running
unit. Two halves:

* `data_dump_path=<path>` is a real documented command-line key — the driver's own
  usage text spells out
  `io-audio -d qc default_device=six_channel_speakerp,log_level=high,bmetrics_level=medium,data_dump_path=/var/log/`.
  Files land as `<path><name>_<n>.bin`.
* `deva_mixer_switch_data_dump_set` calls `deva_data_dump_ctrl` — dumping is a
  **mixer switch**, so it toggles at runtime. Enabling it does not require
  restarting `io-audio`, and therefore does not require a reboot.

`deva_timer_data_dumps` writes what the playback timer actually consumes, which is
past every layer the QEMU harness can see. `csd_data_dumps` and `csd_sad_dumps`
sit at the CSD boundary. Symbols: `deva_data_dump_init @0x2fd0c`,
`deva_data_dump_ctrl @0x30c88`.

Why this matters: the harness proved the six-channel PCM and DMA cursor are
correct. Its faithful six-channel `csd_write` run used MPL1's default route;
without Audioman, it cannot create MPL5's runtime route. Static analysis now
predicts that the real MPL5 route copies only `0x1800` of each `0x4800` DMA
fragment before `csd_write`. The stock timer/CSD dumps can confirm those two
block sizes on the unit without changing the hook.
