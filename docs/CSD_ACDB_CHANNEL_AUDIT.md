# MU1316 CSD/ACDB channel-width audit

Status: static audit completed 2026-09-02 and extended through the signed Hexagon
LPASS implementation. No target binary or calibration file was modified. IDA sidecar
databases were created and annotated next to the extracted `audio_service` and
`libcsdIpcClient.so` binaries. The Hexagon pass is recorded in
[`LPASS_COPP_TOPOLOGY_AUDIT.md`](LPASS_COPP_TOPOLOGY_AUDIT.md).
The subsequent AFE pass establishes the byte units, complete group frame, actual
three-byte TX packing, and the independent SAD/ASAD active-channel declaration:
[`AFE_TDM_MOST_SLOT_AUDIT.md`](AFE_TDM_MOST_SLOT_AUDIT.md).

## Question and result

The audited endpoint is CSD device `0x4e` / 78, used by Qualcomm output 128
(`TDM_OUT_MPL`). The question was whether a previously unopened component at the CSD
boundary limits this device to two channels before audio reaches the amplifier.

The direct numeric fields do **not** impose a stereo cap:

| Layer | Value for the dev-78 path | Result |
|---|---:|---|
| `libcsdIpcClient.so` multi-channel request | caller's `uint16_t` count, unchanged | 6 passes unchanged |
| `audio_service` allocation ceiling | 8 | accepts 6 |
| ACDB device `ChannelConfig` | 6 | six channels |
| ACDB slot-I2S `num_channel` | 6 | six slots described |
| ACDB slot-I2S `bit_width` | 24 | six 24-bit samples |
| ADM COPP-open `dev_num_channel` | 6 | endpoint count sent to LPASS is 6 |
| selected COPP topology | `0x10314` | Qualcomm names this speaker-stereo/default topology |

The last row is the only stereo-specific name in the direct configuration. Device 78 selects topology
`0x10314` for every application mode, while the same ADM open packet independently
declares `dev_num_channel=6`. The resulting configuration is therefore:

```text
COPP topology_id       = 0x00010314  (SPEAKER_STEREO_AUDIO_COPP / default COPP)
COPP dev_num_channel   = 6
endpoint channel map   = FL FR FC LFE LS RS
```

The signed LPASS image has now been decoded far enough to resolve that ambiguity. In
this build, the name does **not** imply a two-channel output: the modules which remain
active accept six, incompatible stereo-only modules are disabled on media reinit, and
`audproc_svc` preserves the configured six-channel output count and map.

The later slot-packing audit also proves that the six offsets are byte offsets and
that LPASS writes all six physical 24-bit slots. It finds a separate lower declaration,
however: the custom TDM/SAD header can carry `ASAD=0xc0` (two active positions) even
while the PCM and endpoint remain six-channel.

## Audited artifacts

All paths below are relative to the extracted MU1316 image at
`Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/`.

| File | SHA-256 |
|---|---|
| `app/armle/bin/audio_service` | `fb3bd4402f99612580dcf33c2c0dac4e8fb635ede92a4df16a6217fd8346af03` |
| `app/armle/lib/libcsdIpcClient.so` | `c3187e241f92107e1b3243b00af5fe78c50f27a9d0d293543c28e63ba529ddcf` |
| `app/armle/lib/libcsd_oem_lib-mmx2_8064.so` | `7f964ef3a621363f6ac447317fa0abb8a67019c104bdf58edb294ab17ebafa83` |
| `system/etc/acdb/mmx2_8064/Speaker_cal.acdb` | `a203ec5c28e689c5db5f2a10aac09596a55a92664c819acffe43a574e3cf9f9b` |
| `system/etc/acdb/mmx2_8064/Global_cal.acdb` | `6c1483b78b51cd86207ea378b2fed946f39ec92e2fd9375445c1032b04c29cdd` |
| `system/etc/acdb/mmx2_8064/w.qwsp` | `b18199e67e87e1d188390df4094853265c39b8f30572c51cc6ba5db3f7d8ef4e` |
| `app/armle/images/lpass.signed.mbn` | `ee1d50b2a4a12b6f808c9b13645605be054d2df6e2d6e14412d8a3f0d5bf1d4a` |

## 1. The IPC client preserves the requested count

`csd_ioctl @ 0x1cf8` in `libcsdIpcClient.so` recognizes command
`71689` / `0x11809` as the multi-channel configuration request. In its branch at
`0x1f04`, it constructs the external IOV as follows:

```text
0x2138  external IOV length = *(uint16_t *)argument
0x213c  external IOV base   = argument + 4   (channel_mapping)
        mmiIoMsg_2(...)
```

There is no comparison with 2, clamp, replacement, or fixed stereo channel map in this
path. A six-channel call is serialized as a six-byte channel map.

## 2. `audio_service` allows up to eight channels

`csd_ac_handle_multi_channel_cfg @ 0x143bb0` contains the diagnostic quoted in the
brief. It is a global allocation ceiling, not a dev-78-specific value:

```armasm
0x143bb0  LDRH  R3, [R1]          ; requested number of channels
0x143bb8  CMP   R3, #8
0x143bd0  BLS   0x143c64          ; accept 0..8
...
0x143c24  MOV   R12, #8           ; Max_num_channels in error log
...
0x143d08  STR   R3, [R0,#0x34]    ; active dev_num_channel
0x143d0c  LDR   R1, [R1,#4]
0x143d10  ADD   R0, R0,#0x38
0x143d14  BL    memcpy             ; copy requested channel map
```

Thus `Max_num_channels=8`; a six-channel request does not take the error branch.

## 3. The platform OEM binding has no channel table

`libcsd_oem_lib-mmx2_8064.so` is a minimal platform shim, not another source of
device geometry:

```armasm
csd_dev_oem_init @ 0x7c8:
    MOV R0,#0
    BX  LR

csd_dev_oem_get_dev_count @ 0x7d8:
    MOV R0,#0
    BX  LR
```

`csd_dev_oem_msg @ 0x7e0` is a 16-entry command switch. Its only nontrivial targets
are PMIC init/dinit and PMIC device enable/disable; the remaining accepted operations
return zero and invalid operations return `-1`. The downstream PMIC functions in this
build are themselves success stubs. There is no dev-78 record, channel count, slot map,
or COPP selection in this library.

## 4. Device 78 in `Speaker_cal.acdb`

The container was decoded using the matching local Qualcomm ACDB parser definitions in
`mm-audio-cal/audcal/acdb/src/acdb_parser.h` and `acdb_file_mgr.c`. In this file,
`DPROPLUT` begins at file offset `0xbf`, its data at `0xcb`, and contains 120
`{device_id, property_id, data_offset}` entries. `DATAPOOL` begins at `0x767d`, with
records encoded as `{uint32_t length, payload}` from `0x7689`.

The complete dev-78 property set is:

| DPROPLUT entry | Property ID | DATAPOOL payload | Length | Meaning |
|---:|---:|---:|---:|---|
| `0x27f` | `0x113b8` | `0x7871` | `0x30` | UTF-16 name `78_TDM_ENTERTAINMENT1_RX` |
| `0x28b` | `0x113ad` | `0x78a5` | `0x20` | application type to COPP topology table |
| `0x297` | `0x113af` | `0x76d9` | `0x04` | voice topology `0x10f70` |
| `0x2a3` | `0x113b0` | `0x76e1` | `0x04` | AFE topology `0x112fc` |
| `0x2af` | `0x1314e` | `0x76e9` | `0x04` | value 1 |
| `0x2bb` | `0x113b6` | `0x76f1` | `0x18` | device type I2S plus three target-property IDs |
| `0x2c7` | `0x113b7` | `0x78c9` | `0x34` | target device information; `ChannelConfig=6` |
| `0x2d3` | `0x12a4b` | `0x7901` | `0x20` | channel map `1,2,3,6,8,9,0,0` |
| `0x2df` | `0x130fd` | `0x7769` | `0x04` | value 0 |
| `0x2eb` | `0x1319f` | `0x7925` | `0x1c` | slot-I2S v1: 6 channels, 24 bits, explicit offsets |
| `0x2f7` | `0x130f6` | `0x7791` | implementation-specific grouped-I2S configuration |
| `0x303` | `0x131a0` | `0x77b5` | implementation-specific frame-sync configuration |

### Device channel configuration

Property `0x113b7` contains these 13 little-endian dwords:

```text
1, 0, 0, 6, 3, 3, 0, 7, 6, 1, 0x100, 0x100, 1
```

`csd_acdb_get_dev_info_common_type @ 0x15e968` first loads the I2S device type and
the three associated target property IDs. The I2S branch of
`csd_acdb_get_dev_info_target_type @ 0x15f70c` reaches the accepted parse at
`0x15fa48`. Its own format strings identify the copied fields as:

```text
I2S Params: Bps, SRM, DirectionMask
ChannelConfig, AfePortId, isVirtual
```

For the dev-78 payload, `ChannelConfig` is dword 8 and equals **6**. `AfePortId` is
dword 3 and equals 6; the endpoint is RX. `csd_dev_tgt_get_static_dev_info @ 0x166e7c`
copies that cached channel count and the first six channel-map entries to the runtime
device structure.

Property `0x12a4b` supplies the channel codes:

```text
01 02 03 06 08 09 = FL FR FC LFE LS RS
```

### Slot-I2S channel configuration

Device 78 references property `0x1319f`; its complete raw payload at file offset
`0x7925` is:

```text
01 00 00 00 06 00 18 00 00 00 00 00 02 00 05 00
08 00 0b 00 0e 00 11 00 ff ff ff ff
```

`csd_acdb_get_dev_prop_info @ 0x15eb4c` decodes the record as:

```text
minor_version   = 1
num_channel     = 6
bit_width       = 24
data_align_type = 0
offset[0..7]    = 2, 5, 8, 11, 14, 17, 0xffff, 0xffff
```

The diagnostic named in the brief — `slot_bit_width, slot_num_channel,
slot_channel_allocation` — belongs to the older property `0x130f7` branch. Device 78
does **not** reference that legacy property. Therefore it has no standalone legacy
`slot_channel_allocation` scalar to report: the newer `0x1319f` representation uses
`data_align_type=0` plus the eight explicit offsets above. Calling that absent field
“zero” would conflate two different record layouts.

`Global_cal.acdb` has no dev-78 or topology-`0x10314` device record. `w.qwsp` is the
plain schema, not a second store of the selected device values.

## 5. COPP selection

`csd_acdb_get_copp_topology_info @ 0x141ba8` maps the four service modes to application
types through the table at `0x1faa84`:

```text
mode 1 -> application 0x11130
mode 2 -> application 0x11132
mode 3 -> application 0x11131
mode 4 -> application 0x11133
```

It then performs ACDB query `70189` / `0x1122d` for `{dev_id, application_type}`.
Dev 78's property `0x113ad` maps **all four** applications to topology `0x10314`:

| ACDB file offset of topology value | Application | Topology |
|---:|---:|---:|
| `0x78a9` | `0x11130` | `0x10314` |
| `0x78b1` | `0x11131` | `0x10314` |
| `0x78b9` | `0x11132` | `0x10314` |
| `0x78c1` | `0x11133` | `0x10314` |

`csd_ac_ioctl @ 0x143d74` stores the selected topology at audio-context offset
`+0x2c`. If no explicit multi-channel configuration exists it seeds context `+0x34`
from the ACDB device count; for this path that default is also 6. The live deva request
has already been measured as six, so both sources agree.

## 6. The ADM open packet still requests six output channels

Both packet formats supported by `audio_service` copy the topology and endpoint width
independently from the audio-context object.

For `csd_ac_q6_send_copp_open_v2 @ 0x147010` (opcode `0x10333`):

```armasm
0x147264  LDR   R3, [R4,#0x2c]    ; topology_id
0x14726c  LDR   R12,[R4,#0x34]    ; dev_num_channel
0x147298  STR   R3, [...]          ; packet topology_id
0x1472a8  STRH  R12,[...]          ; packet dev_num_channel
```

For `csd_ac_q6_send_copp_open @ 0x147354` (opcode `0x10310`):

```armasm
0x147570  LDR   R12,[R4,#0x34]    ; dev_num_channel
0x14757c  LDR   R3, [R4,#0x2c]    ; topology_id
0x1475ac  STR   R3, [...]          ; packet topology_id
0x1475c8  STRH  R12,[...]          ; packet dev_num_channel
```

The V2 packet also takes bit width from context `+0x1b4` and sample rate from `+0x20`.
Neither packet builder substitutes 2 for 6.

Qualcomm's downstream `apr_audio-v2.h` defines `dev_num_channel` as the number of
channels the COPP sends to or receives from the endpoint and permits 1 through 8. The
same header assigns `0x10314` both the default-COPP name and the historical
`ADM_CMD_COPP_OPEN_TOPOLOGY_ID_SPEAKER_STEREO_AUDIO_COPP` name:

<https://android.googlesource.com/kernel/msm/%2B/ac6e850d96251d8cd68bd1049009a04e9d5354a6/include/sound/apr_audio-v2.h>

## 7. What is present in the signed LPASS image

`lpass.signed.mbn` is a signed Hexagon/DSP6 ELF image. Its topology registry contains:

| File offset | Topology ID | Public Qualcomm name |
|---:|---:|---|
| `0x4eccdc` | `0x10314` | default / speaker stereo COPP |
| `0x4ece1c` | `0x1031b` | speaker multichannel peak-volume COPP |
| `0x4ecd68` | `0x10715` | multichannel IIR audio COPP |

The image also contains the validation text `ADM: Invalid device num channels [%d],
valid range: 1 - 8` and multichannel-COPP-open strings. Thus LPASS has generic
multichannel support, and two explicitly multichannel topology IDs are compiled in.
Their mere presence does not establish that either has the calibration/module graph
needed by device 78.

### Internal registry comparison: `0x10be3` is not a different RX graph

The LPASS registry is a sequence of 20-byte records. Its two relevant records are:

```text
file 0x4ecc8c: 00010be3 00000000 c04e981c 00000000 c04e9810
file 0x4eccdc: 00010314 00000000 c04e981c 00000000 00000000
```

The `+8` pointer is the RX graph: speaker-only topology records populate this field,
whereas the microphone topology records populate the `+16` TX pointer. Device 78 is
`TDM_ENTERTAINMENT1_RX`. Consequently, `0x10be3` and `0x10314` resolve to the exact same
RX descriptor, `0xc04e981c`. At file offset `0x4ec81c` that descriptor is:

```text
0000000b c0615980 00000000
```

That is the same 11-module graph in both cases. Changing only the four dev-78 ACDB
topology values from `0x10314` to `0x10be3` changes the requested ID but does not select
a different primary RX processing graph. It is therefore not a meaningful graph A/B for
the missing-channel question.

`0x10312` is genuinely different:

```text
file 0x4ecca0: 00010312 00000000 c04e9804 00000000 c04e97f8
RX descriptor 0xc04e9804 / file 0x4ec804:
                00000003 c0615be8 00000000
```

It selects a three-module RX graph. The exact cited Qualcomm header does contain these
IDs and calls `0x10312` both `NULL_COPP_TOPOLOGY` and `NONE_AUDIO_COPP`; it does not call
it `LOWLATENCY_COPP_TOPOLOGY`. Those public names still do not prove this signed build's
channel behavior.

Within the stock `Speaker_cal.acdb`, `0x10312` is used by devices 9, 11, 14, 15, 35,
40, 65, 66 and 80. Their configured widths are one channel except for dev 65 (three)
and dev 80 (two). No stock six-channel device exercises this graph. Also, dev 15 is
`15_TDM_ANN1_DUP_RX`; the primary announcement RX device is dev 14.

### Hexagon module audit closes the remaining topology question

The full `0x10314` RX descriptor contains 11 modules. Its necessary transport and
gain modules — resampler, delay, SoftVolume, ComboPP, RX gain and Sample Slip — each
accept up to eight channels. PBE, QEnsemble and SPA are stereo-only, but their media
validators return failure on six channels. GenTopo at `0xc01892c0..0xc0189408` then
sets only that module's active byte to zero, logs `failed to reinit. Disabling it`, and
continues. Device-78 ACDB independently disables QCPR, PBE, channel IIR and MBDRC at
their enable parameters.

The three modules in `0x10312` are resampler, SoftVolume and Sample Slip. All three
accept up to eight channels, so variant 2 is statically six-channel-safe. Topology
`0x1031b` is an even better diagnostic candidate: the public name is
`SPEAKER_MCH_PEAK_VOL`, its exact graph is resampler, Peak Meter and SoftVolume, and
all three accept eight channels.

Finally, the `audproc_svc` output-media path at `0xc018e9f0` uses the requested output
count and copies all eight channel-map bytes. The compiled 5.1 converter explicitly
bypasses equal `5.1 -> 5.1` geometry. No static `6 -> 2` conversion remains in the
stock COPP path. Exact module records, validators and ACDB enable values are in
[`LPASS_COPP_TOPOLOGY_AUDIT.md`](LPASS_COPP_TOPOLOGY_AUDIT.md).

## Conclusion and safe next test

There is no numeric two-channel cap in either CSD library, the device's ACDB channel
record, the slot record, or the ADM endpoint count. All of those carry six (with an
eight-channel service ceiling).

There is a head-unit-side topology distinction: `Speaker_cal.acdb` binds dev 78 to
`0x10314` for all four application types while simple announcement/telephone paths use
`0x10312`. The Hexagon pass does not find a narrowing mechanism in the stock graph.
Stereo-only modules reject the format and are disabled individually; the remaining
modules and the output-media path accept six.

The former `0x10be3` artifact was not a useful graph test because `0x10be3` and
`0x10314` share RX descriptor `0xc04e981c`; it has been removed. The current prepared
`docs/acdb/Speaker_cal.acdb` selects `0x10312` at all four offsets and does choose a
different three-module graph. Its complete module list is statically six-channel-safe.
It is still an experiment rather than a production fix because the stock database never
uses it with more than three channels and its public semantic is NULL/NONE COPP.

A production change is **not** justified by the topology name alone. If a controlled
topology A/B is required, `0x1031b` is the strongest candidate recovered from this
image: it is explicitly multichannel and all of its modules accept eight.

The subsequent AFE audit proves that LPASS is configured to pack all six physical
slots, so the cheapest remaining runtime discriminator is now the existing SAD start
log rather than a topology A/B or electrical capture:

```text
convert_sad_header_to_csd: ... asad=0x?? ... sample_width=0x??
```

`ASAD=0xc0` establishes a two-active-channel HU declaration beside the six slots;
`ASAD=0xfc` clears it and leaves the downstream INIC/MOST formatter as the boundary
which static analysis cannot see. Full proof and addresses are in
[`AFE_TDM_MOST_SLOT_AUDIT.md`](AFE_TDM_MOST_SLOT_AUDIT.md).
