# MU1316 LPASS COPP topology audit

Status: static Hexagon audit completed. No firmware image or ACDB binary was modified.

## Result

The selected COPP ID `0x10314` is **not an established six-to-two-channel cap** in
this LPASS build.

Device 78 still opens the endpoint as six channels, and the Hexagon implementation
does not replace that output count with two. The `0x10314` RX graph contains several
legacy stereo-only post-processors, but all observed paths handle them in one of two
ways:

* device-78 ACDB leaves the processor disabled; or
* GenTopo rejects its six-channel reinitialization, marks that individual module
  inactive, and continues the graph.

The modules which remain in the signal path accept at least eight channels. The
generic channel converter also has an explicit `5.1 -> 5.1` bypass, and
`audproc_svc` copies the configured output count and all eight channel-map bytes
instead of hard-coding stereo.

This closes the simple hypothesis "the name `SPEAKER_STEREO_AUDIO_COPP` means this
graph necessarily emits two channels." It does not prove the runtime state of a
particular car. The subsequent slot audit proves the configured AFE/TDM TX packing
path as six physical slots, while finding a separate SAD/ASAD header which can still
declare only two active positions. See
[`AFE_TDM_MOST_SLOT_AUDIT.md`](AFE_TDM_MOST_SLOT_AUDIT.md).

Two proposed ACDB substitutions have different meanings:

* `0x10be3` is a no-op for RX graph selection: it and stock `0x10314` point to the
  same descriptor.
* `0x10312` is a real three-module graph, and all three modules are statically
  six-channel-safe. It is therefore a valid controlled diagnostic, but Qualcomm's
  public name is `NULL_COPP_TOPOLOGY` / `NONE_AUDIO_COPP`, and stock MU1316 never
  uses it for a six-channel device.

If a topology A/B is wanted, `0x1031b` is the better static candidate. Qualcomm
names it `SPEAKER_MCH_PEAK_VOL`; this exact LPASS image implements it as
`Resampler + Peak Meter + SoftVolume`, and all three accept up to eight channels.
That is still a diagnostic experiment, not a production fix proved by static
analysis.

### State of the prepared artifact

At the time of this audit, the repository contains one generated calibration:

```text
docs-obsidian/Sources/acdb/Speaker_cal.acdb
SHA-256 c6e23c0eeeaa46ed30e0e15d9a70617f2a2a25fae1f9bcc0114ff640c1e7883f
```

It is variant 2: the dwords at `0x78a9`, `0x78b1`, `0x78b9` and `0x78c1` are all
`0x00010312`. The earlier separate `Speaker_cal.10312.acdb` was removed, and the
former `0x10be3` main artifact was replaced after its identical RX graph was found.
The generator still defaults to `0x10312` and accepts another topology as its third
argument. No generated binary was changed during this audit.

## Artifacts and tools

The audited image is:

```text
app/armle/images/lpass.signed.mbn
SHA-256 ee1d50b2a4a12b6f808c9b13645605be054d2df6e2d6e14412d8a3f0d5bf1d4a
```

It is an ELF32 Hexagon/DSP6 image with load mappings relevant here:

| File range | Virtual address | Purpose |
|---:|---:|---|
| `0x3000...` | `0xc0000000...` | executable code |
| `0x4bc000...` | `0xc04b9000...` | read-only data and strings |
| `0x615000...` | `0xc0613000...` | writable topology tables |

IDA Pro 9.3 on the analysis host has no Hexagon processor module. The installed
radare2 identifies `elf32-hexagon` but has no Hexagon disassembler backend; treating
its output as ARM produces false instructions. Homebrew LLVM 22.1.8 does have the
correct backend and was used for all instructions below:

```sh
/opt/homebrew/Cellar/llvm/22.1.8/bin/llvm-objdump -d \
  --start-address=<hexagon-va> --stop-address=<hexagon-va> lpass.signed.mbn
```

The warning that no named section overlaps the requested range is harmless for this
sectionless image: LLVM still disassembles the matching `PT_LOAD` segment as
Hexagon. radare2 remained useful only for byte and pointer searches.

## 1. Topology registry and exact RX graphs

The topology registry uses 20-byte records. For an RX endpoint, record offset `+8`
points to a descriptor `{module_count, module_list, reserved}`.

| Registry file offset | ID | RX descriptor | Module list |
|---:|---:|---:|---:|
| `0x4ecc8c` | `0x10be3` | `0xc04e981c` | `0xc0615980` |
| `0x4ecca0` | `0x10312` | `0xc04e9804` | `0xc0615be8` |
| `0x4eccdc` | `0x10314` | `0xc04e981c` | `0xc0615980` |
| `0x4ece1c` | `0x1031b` | `0xc04e9738` | `0xc0613c1c` |

The two equal descriptor pointers prove that changing dev 78 from `0x10314` to
`0x10be3` does not change its primary RX graph.

### Stock `0x10314` / `0x10be3` RX graph

The descriptor at file `0x4ec81c` is `{11, 0xc0615980, 0}`. Its module records start
at file `0x617980`; each record is `0x2c` bytes.

| Order | Module ID | Identification | Media-width result |
|---:|---:|---|---|
| 0 | `0x10719` | resampler | accepts up to 8 |
| 1 | `0x10772` | delay | accepts up to 8 |
| 2 | `0x10bfe` | SoftVolume | accepts up to 8 |
| 3 | `0x00002` | internal ComboPP | accepts up to 8; only EQ may run on multichannel |
| 4 | `0x10c2a` | PBE | stereo-only; ACDB disabled; disabled by GenTopo on 6ch |
| 5 | `0x10c02` | channel IIR | stereo-only text; ACDB disabled |
| 6 | `0x10c06` | MBDRC | ACDB disabled |
| 7 | `0x10c59` | QEnsemble | accepts only 1/2; no dev-78 calibration; disabled on 6ch |
| 8 | `0x10c39` | SPA | requires exactly 2; no dev-78 calibration; disabled on 6ch |
| 9 | `0x10c37` | RX codec gain | accepts up to 8 |
| 10 | `0x00004` | internal Sample Slip | accepts up to 8 |

The module record layout recovered from call sites is:

```text
+0x00 module_id
+0x04 get_static_properties
+0x14 init
+0x24 flags
+0x28 instance_size
```

No module ID for the 5.1 downmixer occurs in this graph.

### `0x10312` RX graph: proposed variant 2

The descriptor at file `0x4ec804` is `{3, 0xc0615be8, 0}` and the module records
start at file `0x617be8`:

```text
0x10719  resampler
0x10bfe  SoftVolume
0x00004  Sample Slip
```

All three validators accept up to eight channels. Therefore the earlier caveat
"nothing shows that `0x10312` preserves six channels" is resolved: its complete RX
module list has no static two-channel limit. What remains unproved is platform
fitness, calibration behavior, and the effect on the real vehicle.

### `0x1031b` RX graph: explicit multichannel candidate

The descriptor at file `0x4ec738` is `{3, 0xc0613c1c, 0}` and the module records
start at file `0x615c1c`:

```text
0x10719  resampler
0x10712  Peak Meter
0x10bfe  SoftVolume
```

Peak Meter checks its channel count against 8 at `0xc0246ea0`; the other two
modules also accept eight. This exactly matches the public
`SPEAKER_MCH_PEAK_VOL` name and makes it a better controlled topology test than
the publicly named NULL/NONE `0x10312` graph.

## 2. Hexagon media-format validators

The decisive instructions use the media-format dword at offset `+0x00` as
`num_channels`. Important checks are:

```text
Resampler, c0219e90: r17 = memw(r0+#0x0)
           c0219e98: if (r17 > 8) fail

ComboPP,   c02224c8: r0 = memw(r1+#0x0)
           c02224cc: if (!(r0 > 8)) accept

PeakMeter, c0246e9c: r1 = memw(r0+#0x0)
           c0246ea0: if (r1 > 8) fail

RX Gain,   c0248fc4: r1 = memw(r0+#0x0)
           c0248fc8: if (r1 > 8) fail

SampleSlip,c0249c1c: r2 = memw(r0+#0x0)
           c0249c20: if (r2 > 8) fail

Delay,     c024db0c: r1 = memw(r0+#0x0)
           c024db10: if (r1 > 8) fail
```

SoftVolume has two count-validation paths at `0xc021cb00` and `0xc021ccfc`; both
compare the supplied count to 8. Its initialization also allocates an eight-entry
channel state.

The incompatible modules are equally explicit:

```text
PBE,       c0229b54: read num_channels
           c0229b7c: require num_channels == 2

QEnsemble, c022e58c: read num_channels
           c022e590..c022e5b4: accept only 1 or 2

SPA,       c023ee94: read num_channels
           c023eebc: require num_channels == 2
```

Their matching strings say `Only upto 2 channels supported`. These checks prove
that the modules themselves are stereo-only, but not that they downmix or truncate.
They return failure on six channels instead.

## 3. What device-78 ACDB actually enables

The relevant `Speaker_cal.acdb` chunks are:

| Chunk | LUT | CDFT | CDOT |
|---|---:|---:|---:|
| audio processing gain-independent | `ACGILUT0 @ 0x66f` | `0x6cf` | `0x727` |
| audio processing gain-dependent | `ACGDLUT0 @ 0x75b` | `0x111b` | `0x1153` |
| audio volume | `AVOLLUT0 @ 0x1177` | `0x7267` | `0x7287` |

`DATAPOOL` payloads begin at file `0x7689`. Dev 78 has four gain-independent
rows, one for each application `0x11130..0x11133`, all at 48 kHz and all resolving
to the same module/parameter set:

| Module / parameter | Payload | Interpretation established from service call sites |
|---|---|---|
| `0x10c20 / 0x10c21` | `00000000` | ComboPP QCPR enable = 0 |
| `0x10c20 / 0x10c22` | `00000000` | QCPR configuration word |
| `0x10c20 / 0x10c23` | `00000000` | QCPR parameter |
| `0x10c20 / 0x10c24` | `00000000` | QCPR parameter |
| `0x10c20 / 0x10c25` | `01000000` | QCPR parameter |
| `0x10c20 / 0x10c26` | `ffff0000` | QCPR parameter |
| `0x10c2a / 0x10c2b` | `00000000` | PBE enable = 0 |
| `0x10c2a / 0x10c49` | 176-byte block | PBE configuration, inactive while enable is 0 |
| `0x10c37 / 0x10c38` | `00200000` | RX codec gain `0x2000` (unity) |

The gain-dependent set is also disabled at its enable parameters:

| Module / parameter | Payload | Result |
|---|---|---|
| `0x10c02 / 0x10c03` | `00000000` | channel IIR disabled |
| `0x10c02 / 0x10c04` | `00200000` | IIR scalar/config value |
| `0x10c02 / 0x10c05` | 76-byte block | IIR configuration |
| `0x10c06 / 0x10c07` | `00000000` | MBDRC disabled |
| `0x10c06 / 0x10c08` | 80-byte block | MBDRC configuration |

The volume table has 124 dev-78 rows: four applications times 31 volume steps. It
contains `SoftVolume` parameter `0x10c29` and the `0x111d2 / 0x111d5` volume curve;
neither is a channel-count override. No dev-78 entry was found for QEnsemble or SPA.

In `audio_service`, `csd_q6msg_create_qcpr_enable @ 0x171cac` sends module
`0x10c20`, parameter `0x10c21`; the configuration and disable builders at
`0x171da8` and `0x171e60` confirm this identification. Thus the zero QCPR enable is
not inferred merely from a name table.

## 4. GenTopo disables a rejecting module; it does not narrow the graph

The media-change loop is in `audproc_appi_topo.cpp` at
`0xc01892c0..0xc0189408`. For each module it calls the module's reinit function at
`0xc01893d4`.

On success:

```text
c01893d8  compare return value with 0
c01893e0  store 1 in module active byte at +0xfa
```

On failure:

```text
c01893e4..c01893f4  log module ID and "failed to reinit. Disabling it"
c0189400             store 0 in module active byte at +0xfa
c0189404             continue with the next module
```

This is the exact behavior reached when PBE, QEnsemble or SPA receives the six-channel
media type. Their validator does not return a two-channel output format: it fails, and
GenTopo removes the module from active processing. The topology open remains alive.

This behavior is also consistent with the observed successful six-channel device
open. A graph constructor which treated one stereo-only module's rejection as fatal
would fail the open instead of silently producing the reported working L/R stream.

## 5. Output count, channel map and downmix

`audproc_svc` logs the requested output policy in the function beginning at
`0xc018e9f0`:

```text
Set output media Begin:
keepInputFreq, outputFreq, useNativeNumChannels, outputChannels
```

At `0xc018ea0c..0xc018ea18` it reads those four values independently. When the
configured channel count is used, `0xc018eab4..0xc018eac4` copies eight mapping
bytes from the request and passes the resulting media format to
`0xc018e3f0`. There is no constant 2 in that path.

The generic channel-mixer code compiled into the image is not a hidden unconditional
downmix. Its strings and branches enumerate only supported conversions and include:

```text
APPI Downmix: bypass, input #channels == output #channels
APPI 5.1 Downmix only supports 5.1->2, 5.1->1, 5.1->5.1(bypass)
```

The service-side ADM open already sends `dev_num_channel=6` and channel map
`FL FR FC LFE LS RS`. Therefore the statically selected conversion is `6 -> 6`
bypass, not `6 -> 2`.

## 6. What is and is not safe to conclude

Proved statically:

1. `0x10be3` cannot test a different RX graph from `0x10314`.
2. all three modules in variant `0x10312` accept six channels;
3. all three modules in explicit multichannel topology `0x1031b` accept six channels;
4. the enabled path through stock `0x10314` has no recovered two-channel validator;
5. stereo-only modules reject six channels and are disabled rather than producing a
   two-channel media type;
6. the requested output count/map remain six through `audproc_svc`'s static path.

Not proved statically:

* which modules are active in the exact live vehicle instance after all runtime
  commands;
* whether COPP output samples 3--6 reach AFE port 128;
* the actual sample values present in the six slots on a particular live run;
* whether the INIC/MLB/MOST formatter transmits those slots.

The later AFE audit statically proves that `FUN_c0174970` loops over six and packs
three bytes at offsets `2,5,8,11,14,17`. It also finds that the custom TDM header
independently declares effective sample width and active positions. The cheapest
next measurement is therefore the existing start log:

```text
convert_sad_header_to_csd: ... asad=0x?? ... sample_width=0x??
```

`ASAD=0xc0` establishes the HU-side six-PCM/two-active-channel mismatch. If the live
value is `0xfc`, an electrical one-hot capture at `TDM_OUT_MPL` and then the MOST
source remains necessary. If C/Ls/Rs are already absent at COPP output, capture the
live GenTopo module-active bitmap and output media format; that would expose runtime
state not represented by the static configuration above.
