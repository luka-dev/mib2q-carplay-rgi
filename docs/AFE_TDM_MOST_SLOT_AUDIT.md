# AFE/TDM to MOST slot-width and SAD metadata audit

Date: 2026-09-02  
Scope: static analysis of the stock MU1316 `deva-ctrl-qc.so`, `audio_service`,
`Speaker_cal.acdb`, and signed Hexagon LPASS image. No firmware binary or
calibration file was modified.

## Result

The physical AFE/TDM slot geometry for CSD device 78 is internally consistent and
does not cap the stream to stereo:

```text
AFE port/device       TDM_OUT_MPL / CSD dev_id 78
PCM endpoint          6 channels, FL FR FC LFE LS RS
physical slot width   24 bits = 3 bytes
slot byte offsets     2, 5, 8, 11, 14, 17
group frame size      32 bytes at the device's 48 kHz rate
```

The offsets are byte offsets in one TDM frame. They come directly from device
property `0x1319f` in `Speaker_cal.acdb`; they are not calculated from the QSA
voice count or the MOST Connect BlockWidth. `audio_service` copies this record into
AFE parameter `0x10297`, and LPASS validates and uses all six entries.

The apparent `24-bit transmitter -> 16-bit receiver` mismatch is also coherent on
the AFE side. The LPASS TX packer takes the signed sample from a 32-bit internal
container, shifts it right by eight, and writes three bytes to the configured
24-bit slot. A QSA S16 sample which deva placed in bits 31..16 therefore becomes:

```text
internal word         [signed S16 sample][16 zero bits]
after ASR 8           [sign extension][signed S16 sample][8 zero bits]
physical TDM slot     [signed S16 sample][zero padding byte]
```

This is an intended 16-bit signal carried in a 24-bit physical slot. It does not
extract the low 24 bits and does not multiply the sample by 256.

There is, however, a separate and more important width declaration at this exact
boundary. The custom TDM/SAD sequence — a two-byte header in each of eight frames —
carries:

- `sample_width`, the useful precision inside the physical slot; and
- `ASAD`, a bit mask of active speaker/channel positions.

The header is independent of the six-channel PCM/COPP configuration. The MPL
session defaults to six active positions (`ASAD=0xfc`) and 24 useful bits, but
`DCMD_PCM_SET_SAD_METADATA` overwrites those persistent fields. AirPlay's stock
metadata path supplies its original input geometry — two channels and 16 bits —
even when the QSA hook later widens the PCM stream to six channels. The observed
result is `ASAD=0xc0`, whose popcount is two, and `sample_width=0x10`.

Thus static analysis found a concrete head-unit-side mechanism which can produce
the vehicle's one-hot result:

```text
COPP and AFE PCM       six channels
physical TDM slots    six slots written
SAD active mask       only L/R declared active (0xc0)
```

LPASS does not use ASAD to reduce its six-slot PCM copy loop; it forwards the
header beside the audio. Whether the downstream INIC/MOST formatter suppresses
slots not declared active is not visible in the available host or signed LPASS
code. Therefore stale `ASAD=0xc0` is a proved mismatch mechanism and the leading
remaining cause, but its final effect at the MOST formatter is not yet proved.

## 1. Slot offsets and units

Qualcomm's public AFE ABI defines:

```text
AFE_MODULE_TDM                         0x0001028a
AFE_PARAM_ID_PORT_SLOT_MAPPING_CONFIG 0x00010297
AFE_PARAM_ID_CUSTOM_TDM_HEADER_CONFIG 0x00010298
```

For `AFE_PARAM_ID_PORT_SLOT_MAPPING_CONFIG`, `num_channel` permits 1, 2, 4, 6,
or 8; `bitwidth` is the slot width and permits 16, 24, or 32; and each
`offset[i]` is explicitly a **byte offset from the start of the TDM frame**.
Byte zero is the first byte on the digital serial line. `data_align_type` only
controls alignment where a 24-bit sample occupies a 32-bit slot.

The corresponding stock ACDB payload for device 78 is property `0x1319f`, file
offset `0x7925`, length `0x1c`:

```text
01 00 00 00  06 00  18 00  00 00 00 00
02 00 05 00 08 00 0b 00 0e 00 11 00 ff ff ff ff
```

Decoded:

| Field | Value |
|---|---|
| minor version | 1 |
| number of channels | 6 |
| slot bit width | 24 |
| alignment type | 0 |
| offsets | 2, 5, 8, 11, 14, 17, unused, unused |

The intervals are consequently `[2,5)`, `[5,8)`, `[8,11)`, `[11,14)`,
`[14,17)`, and `[17,20)`: six contiguous, non-overlapping three-byte slots.
Bytes 0 and 1 are reserved for the custom header described in §5.

Official ABI reference:
<https://android.googlesource.com/kernel/msm/+/ac6e850d96251d8cd68bd1049009a04e9d5354a6/include/sound/apr_audio-v2.h>

## 2. ACDB to `audio_service` to LPASS

`csd_acdb_get_dev_prop_info @ 0x0015eb4c` in `audio_service` parses property
`0x1319f`. The already-audited device setup then reaches
`afe_dev_tgt_afe_tdm_slot_config_v2 @ 0x0016a0f4`, which constructs the AFE
module/parameter packet and copies:

```text
bit width
number of channels
alignment type
all eight explicit offsets
```

There is no substitution of two channels and no calculation from a MOST resource
width. The AFE packet is simply the ACDB device property in Qualcomm's runtime ABI.

In LPASS, `FUN_c01761c0 @ 0xc01761c0` handles parameter `0x10297`. Its relevant
operations are:

```text
accept version 1
accept 1..8 channel entries
accept bit widths 16, 24, or 32
accept alignment values 0 or 1
copy offset[i] to port records at +0x21c/+0x21e
store bitwidth >> 3 as slot bytes at record +0x220
store slot bytes at port +0x216
store channel count at port +0x218 and +0x44
choose a 2-byte internal container for <=16 bits, otherwise a 4-byte container
```

It also performs pairwise overlap checks. `FUN_c0176150 @ 0xc0176150` calls
`FUN_c0175e60 @ 0xc0175e60` for each active channel. That helper only divides a
possibly unaligned byte range into one or two four-byte-aligned DMA copy
fragments. It does not widen the physical slot or change the channel count.

## 3. The complete grouped TDM frame is 32 bytes

Device 78 also references grouped-I2S property `0x130f6`. Its complete payload at
file offset `0x7791` is:

```text
01 00 00 00  06 01  03 00  00 ee 02 00
06 00 0e 00 10 00 12 00 ff ff ff ff ff ff ff ff
18 00 00 00
```

For this signed LPASS build it decodes as:

```text
version         1
group id        0x0106
channel mode    3
group rate      192000 Hz
member ports    6, 14, 16, 18, unused, unused, unused, unused
group bit width 24
```

`FUN_c01a8d10 @ 0xc01a8d10` maps channel mode 3 to two group lanes.
`FUN_c01a93b0 @ 0xc01a93b0` uses a four-byte internal container for a group width
above 16 bits and applies the `192000/48000 = 4` rate ratio. The resulting frame
extent at the device's 48 kHz cadence is:

```text
4 container bytes * 2 group lanes * 4 time slots = 32 bytes
```

`FUN_c01a82e0 @ 0xc01a82e0` obtains that size and rejects any configured
`offset + slot_bytes` beyond it. Device 78 occupies bytes 2..19. Other ACDB devices
in the same group place the one-channel 16-bit telephone RX slot at byte 26, ANN1
at byte 28, and ANN2 at byte 30, so the complete observed multiplex is:

```text
bytes  0..1    custom TDM/SAD header
bytes  2..19   device 78: six 24-bit media slots
bytes 20..25   two potential 24-bit media positions, unused by stock dev 78
bytes 26..27   telephone RX, 16 bit
bytes 28..29   ANN1, 16 bit
bytes 30..31   ANN2, 16 bit
```

Qualcomm's official `apq8096-auto.c` independently contains the MMXF platform row
`{2,5,8,11,14,17,20,23}`, followed by one-channel rows `{26}`, `{28}`, and `{30}`.
That exactly matches the recovered 32-byte layout and proves that 20/23 are the two
remaining potential 24-bit positions, rather than unexplained padding:
<https://android.googlesource.com/kernel/msm.git/+/ef85d528056e472b667607601cc6a7f7cccb82b1/sound/soc/msm/apq8096-auto.c>

This resolves the packing ambiguity: the HU's fixed source `BlockWidth 26` is
naturally compatible with bytes 0..25, `2 + 8*3`, while ASAD selects active speaker
positions. A direct static binding of `MmxSourcePresCtrl/7` to this AFE group is
still absent, so that final ownership connection remains an inference. It is not
evidence for `2 + 6*4`; the AFE driver proves that device 78's physical slots are
three bytes wide.

## 4. What the LPASS transmitter writes

`FUN_c0174970 @ 0xc0174970` is the TX slot packer. For each sample frame it loops
over `port+0x44`, which was set to six by the slot-mapping parameter. For a 24-bit
slot (`port+0x216 == 3`) it:

1. reads one signed 32-bit sample from the internal interleaved buffer;
2. performs an arithmetic shift right by eight at `0xc01749e0` or
   `0xc0174b8c`, depending on the aligned/split copy path;
3. writes exactly three bytes using the per-channel offset fragments; and
4. advances to the next input sample and next slot record.

The loop has no two-channel clamp and no low-24-bit extraction. The reverse RX
unpacker at `FUN_c01746e0` likewise iterates the configured channel count.

`FUN_c0175f00 @ 0xc0175f00` is a different operation: it inserts the configured
custom header into the group buffer. It must not be confused with the PCM slot
packer.

This closes the old amplitude hypothesis at this boundary. QSA's S16-to-S32
widening left-shifts each sample by 16. LPASS's 24-bit TX operation right-shifts
that word by eight. Net result: the original signed S16 value is left-aligned in
the 24-bit wire slot with one zero padding byte. A receiver selecting the high 16
bits reconstructs the input exactly.

## 5. SAD declares effective width and active channels separately

The AFE custom-header ABI requires a two-byte header for MOST/TDM header type 2
(`ENTERTAINMENT_MOST`). The stock dev-78 configuration uses start offset 0,
header width 2, eight repeated frames, and header type 2.

`csd_acdb_set_dev_info_tdm_sad @ 0x001607bc` in `audio_service` converts the CSD
SAD description to eight header words. The second word is constructed as:

```text
0xb200 | sample_width
```

and another word contains ASAD. A measured stereo request produces:

```text
TDM SAD Header Payload:
0x3c01, 0xb210, 0x0, 0xc0, 0x7, 0x0, 0x70, 0x0
                ^16 bits   ^ASAD: L/R active
```

The `0x10298` branch of LPASS `FUN_c01761c0` copies payload byte `+2` to
`port+0x286` as `bit_width_from_header`, copies payload byte `+6` to
`port+0x287` as `asad_from_header`, and stores its popcount at `port+0x288` as
`num_chan_from_header`. For `0xc0`, that count is exactly two. The compiled
diagnostic confirms the interpretation:

```text
CUSTOM TDM HEADER parsed [bit_width_from_header:%i]
                         [asad_from_header:0x%x]
                         [num_chan_from_header:%i]
```

These values do not replace the six-channel count used by `FUN_c0174970`. They
are metadata delivered in the physical frame for the next component.

## 6. The metadata is persistent and is not derived from QSA voices

The shared MPL playback-session fields used by
`convert_sad_header_to_csd @ 0x000618e4` are:

| Session offset | Meaning |
|---:|---|
| `+0x8c` | custom header enabled |
| `+0x90` | CGMS |
| `+0x91` | media type |
| `+0x92` | ASAD active-speaker mask |
| `+0x94` | codec type |
| `+0x95` | source/effective sample width |
| `+0x98` | source sample rate |
| `+0x9c` | destination sample width |
| `+0xa0` | destination sample rate |
| `+0xa4` | bitrate |

The MPL session is initialized once in
`deva_init_platform_mib @ 0x00057af0`. The exact stores at
`0x0005a734..0x0005a7d0` establish the clean-boot defaults relevant here:

```text
header enabled          1
media type              0
ASAD                    0xfc  (six set bits)
EMI/auxiliary byte      0
codec type              0
source sample width     24
source sample rate      48000
destination width       24
destination rate        48000
bitrate                 0
```

`pcm_devctl @ 0x0003f0a0` has paired get/set branches for SAD metadata; the
observed full devctl command is `0x8018fa01`. The set branch at
`0x0003f8a8..0x0003f908` copies the caller's fields into the same
persistent MPL session, including media type, ASAD, codec type, sample width,
sample rate, destination width, and bitrate. It does not consult the current PCM
voice count.

The converter then reads media type at `0x0006197c`, codec type at
`0x00061988`, ASAD at `0x000619a8`, CGMS at `0x000619b0`, bitrate at
`0x000619b8`, and sample width at `0x000619c0`. These are recovered call-site
loads, not guessed structure names.

At every CSD playback start, `session_csd_start_playback @ 0x00069fa8` tests
the enable flag at `0x0006a030`, calls the converter at `0x0006a048`, and sends
the header at `0x0006a058` before enabling the device. It therefore faithfully
re-sends the session's current SAD state, but it does not recompute ASAD from the
six-channel stream descriptor.

The standalone [`tools/qsa_play.c`](../tools/qsa_play.c) calls only the QSA PCM
open/params/prepare/setup/write APIs. It never issues the SAD metadata devctl.
Consequently, a direct six-channel test inherits either the clean-boot default or
the metadata left by the previous writer of that shared MPL session. It is not a
controlled SAD-width test unless the metadata is explicitly set or logged.

## 7. Existing runtime evidence for both header states

A saved vehicle slog for the known-good RetroArch `mpl1_int_ent` six-channel path
shows the clean/default header in actual use:

```text
calculate_emi_flag: source_sample_width <24>
convert_sad_header_to_csd: media_type=0x0, codec_type=0x0,
    emi=0x3, asad=0xfc, cgms=0x0, bitrate=0x0,
    sample_width=0x18
audio_service: TDM SAD Header Payload:
    0x3c00, 0xb218, 0x6000, 0xfc, 0x7, 0x0, 0x0, 0x0
```

The source is
[`gpsp_slog_playback_50.txt`](../../gpSP/logs/gpsp_slog_playback_50.txt),
lines 492--508. Thus `0xfc/24` is not merely an unused initializer; the working
six-channel MPL1 path sends it.

An existing QEMU boot of the real `io-audio` and AirPlay client records the entire
opposite sequence, with no inference from field names:

```text
[CQNXSoundIF] createMetadata nChannels: 2, nBitsPerChannel: 16,
                              nSampleRate: 48000
pcm_devctl: DCMD_PCM_SET_SAD_METADATA
convert_sad_header_to_csd: media_type=0x0, codec_type=0x1,
    emi=0x0, asad=0xc0, cgms=0x0, bitrate=0x177000,
    sample_width=0x10
audio_service: TDM SAD Header Payload:
    0x3c01, 0xb210, 0x0, 0xc0, 0x7, 0x0, 0x70, 0x0
```

The PCM hook widens the QSA stream after the application has described its native
stereo input, but it does not change this separate devctl payload. Thus the normal
hooked CarPlay sequence can combine six-channel PCM with a two-channel SAD mask.
The standalone one-hot test can reproduce the same state if it is run after the
AirPlay client has written metadata, because `qsa_play` does not replace it.

Together these logs prove that the HU emits distinct six-active and two-active
declarations at the same CSD/AFE boundary. They do not by themselves prove that
the exact vehicle one-hot MPL5 run inherited `0xc0` or how the external formatter
reacts to inactive ASAD positions.

## 8. Reconciliation of the width numbers

| Width | Layer and exact meaning |
|---:|---|
| 32 bits/sample | deva/CSD internal container stride |
| 24 bits/sample | physical AFE/TDM slot width for device 78 |
| 16 useful bits | stock AirPlay SAD `sample_width`; high 16 bits of each 24-bit slot |
| 32 bytes/frame | complete shared grouped-TDM frame at 48 kHz |
| 26 bytes | fixed HU source allocation and Connect BlockWidth |
| 14 bytes | amplifier sink `0x26` advertised profile: header 2 + six 16-bit samples |

What is now proved:

```text
deva S16 high-aligned in S32
  -> LPASS ASR 8
  -> six physical 24-bit slots, low byte padded with zero
  -> SAD header declares effective width and active positions
```

What remains outside the available static code is the exact INIC/MLB/MOST
conversion from the 24-bit TDM slots to the amplifier's advertised 16-bit sink
payload. The static geometry provides a consistent conversion rule — take the high
16 bits from each active 24-bit slot — but the formatter implementation is not in
the analysed HU binaries.

The prior `2 + 6*4 = 26` explanation is superseded. Device 78's AFE wire slots are
demonstrably three bytes, not four. `2 + 8*3 = 26` matches the allocated resource
and the spare positions in the 32-byte group frame, but the exact association of
that source resource with these TDM bytes remains an inference.

## 9. Decision and smallest decisive check

No ACDB slot-offset or slot-width patch is justified. Changing the six 24-bit
slots to 16-bit would invalidate the stock grouped-frame layout and would bypass a
working, explicitly implemented 16-in-24 packing path.

The narrow next check does **not** require a MOST analyser. During one six-channel
one-hot start on the car, capture the already-existing deva line:

```text
convert_sad_header_to_csd: ... asad=0x?? ... sample_width=0x??
```

Interpretation:

| Live value | Conclusion |
|---|---|
| `ASAD=0xc0`, popcount 2 | HU sends a stereo active-channel declaration beside six-channel PCM; the mismatch is established on our side. |
| `ASAD=0xfc`, popcount 6 | SAD is also correct; then the honest remaining boundary is after LPASS packing, inside the unavailable INIC/MOST formatter or on wire. |

A controlled diagnostic can explicitly set six-channel SAD metadata before
starting `qsa_play`, but no such binary change was made in this static pass. For a
production fix, the SAD metadata must follow the widened output geometry rather
than the original stereo input geometry. The precise patch site and ownership
should be chosen only after the live line above confirms the vehicle state.

## Provenance and reproducibility

All firmware paths are relative to:

```text
/Users/luka/Desktop/AUDI_2/Firmwares/HU/
MHI2Q_US_AUG22_P5087_MU1316/extracted/
```

| Artifact | SHA-256 |
|---|---|
| `app/armle/lib/dll/deva-ctrl-qc.so` | `58dddcabaa94c2fecf989855a6f810ec219926f49669a0ed9482ed629fdc64b9` |
| `app/armle/bin/audio_service` | `fb3bd4402f99612580dcf33c2c0dac4e8fb635ede92a4df16a6217fd8346af03` |
| `app/armle/images/lpass.signed.mbn` | `ee1d50b2a4a12b6f808c9b13645605be054d2df6e2d6e14412d8a3f0d5bf1d4a` |
| `system/etc/acdb/mmx2_8064/Speaker_cal.acdb` | `a203ec5c28e689c5db5f2a10aac09596a55a92664c819acffe43a574e3cf9f9b` |

The ARM paths were checked with radare2. The signed DSP image was imported with
Ghidra 12.1.2's built-in Hexagon language. Reusable headless helpers are:

- [`ExportTdmXrefs.java`](../tools/qsa-emu/ghidra/ExportTdmXrefs.java) for address
  xrefs and whole-function decompilation;
- [`ExportScalarUses.java`](../tools/qsa-emu/ghidra/ExportScalarUses.java) for
  locating structure-offset/immediate uses and exporting their containing
  functions.

Related prior audits:

- [`CSD_ACDB_CHANNEL_AUDIT.md`](CSD_ACDB_CHANNEL_AUDIT.md)
- [`LPASS_COPP_TOPOLOGY_AUDIT.md`](LPASS_COPP_TOPOLOGY_AUDIT.md)
- [`MOST_SOURCE_BLOCKWIDTH_AUDIT.md`](MOST_SOURCE_BLOCKWIDTH_AUDIT.md)
- [`CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md`](CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md)
