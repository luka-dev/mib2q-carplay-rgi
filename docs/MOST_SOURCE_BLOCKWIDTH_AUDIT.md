# Connection 162 MOST source BlockWidth audit

Date: 2026-09-02  
Scope: static analysis of the stock MU1316 head-unit firmware. No executable,
firmware image, or analysis database was modified.

> **Subsequent receiver-side result:** the amplifier Connect handler accepts
> BlockWidth 26 for sink `0x26`. It validates only the generic range `1..372`,
> not equality with the sink's advertised width 14. See
> [`CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md`](CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md).
>
> **Subsequent AFE packing result:** device 78 does not put six four-byte slots on TDM.
> It writes six three-byte slots at offsets `2,5,8,11,14,17` in a 32-byte shared frame.
> Qualcomm's matching MMXF platform table confirms potential offsets `20,23` too, so
> `2 + 8*3 = 26` exactly covers that media region; the last two positions are not enabled
> for stock dev 78 and the exact Mmx-source binding remains an inference.
> The same audit found an independent SAD/ASAD active-channel declaration. See
> [`AFE_TDM_MOST_SLOT_AUDIT.md`](AFE_TDM_MOST_SLOT_AUDIT.md).

## Result

The stereo-width hypothesis is **falsified**. On the external-amplifier branch,
connection `162` does not allocate a 4- or 8-byte stereo source. It selects the
following source resource:

```text
logical source       eSRC_ENT_AMP_DIO_MEDIA_MC (0x46f)
resource variant     DSI:MmxSourcePresCtrl/7
MOST source number   7
source ID             52 (0x34)
allocated BlockWidth 26 bytes per synchronous frame (0x001a)
physical sink         0x26
Connect BlockWidth    26 (0x001a)
```

The stock HU therefore constructs connection 162's MOST `Connect.StartResult`
with `SinkNr 0x26` and `BlockWidth 0x001a`. In the seven-byte amplifier payload,
the sink and width are the bytes at offsets 2..4:

```text
26 00 1a
```

This corrects an inference in the earlier sink audit. The amplifier's
`SinkInfo.Get` profile for sink `0x26` advertises `BlockWidth 14`; that is the
sink's own protected 6 x 16-bit profile. The controller does **not** copy that
value into Connect. It propagates the independently allocated source width,
which is `26`.

Consequently:

- the HU allocates enough synchronous bandwidth for more than stereo;
- neither the QSA voice count nor the CarPlay API supplies this value;
- the source and sink descriptions disagree (`26` requested versus `14`
  advertised), but the amplifier does not reject that pair on width equality;
- the observation that only L/R arrive at the amplifier must be localized by
  measuring audio content, not by another width-table inference.

## Logical source to physical source

The accepted connection-table audit established the external-amplifier pair:

```text
eSRC_ENT_AMP_DIO_MEDIA_MC (0x46f) + eSNK_DTCP51
```

MIBRoot's resource-variant initializer resolves the source half. The static
initializer at VA `0x002046b4` constructs the global string at `0x010bd290` as
`MmxSourcePresCtrl`. This matters because the global occupies zero-filled file
storage; inspecting it without following the constructor makes the component
name appear absent.

In the mapping builder:

```text
0x002054f8  source number = 7
0x002054fc  component     = global CHBString "MmxSourcePresCtrl"
0x00205504  build component/source-number tuple
0x00205510  wrap it as ResourceVariant type 2
0x00205628  logical source = 0x46f
0x00205644  insert 0x46f -> that ResourceVariant
```

The formatter at AudioProcess VA `0x002637f0` renders a type-2 variant as
`DSI:%s/%u`, so the exact result is:

```text
eSRC_ENT_AMP_DIO_MEDIA_MC -> DSI:MmxSourcePresCtrl/7
```

For comparison, the non-MC logical source `0x46e` maps to
`DSI:MmxSourcePresCtrl/1`. Source 1 has `srcID 58, BlockWidth 4`; it is the narrow
resource used by the internal-amplifier branch, not an alternative selected on
the B&O/external route.

## Where the 26-byte allocation comes from

### MIBRoot source configuration

MIBRoot VA `0x00207dd4` builds the `MmxSourcePresCtrl` source table. It loads
constant `0x1a` into `r8` at `0x00207e0c`. The source-7 record is then built as:

```text
0x00207ee4  source number = 7
0x00207ee8  bandwidth     = r8 = 0x1a
0x00207ef4  source ID     = 7 + 0x2d = 52
0x00207ef8  insert record
```

The nearby records provide a useful negative control:

| Source number | Source ID | Configured bandwidth |
| ---: | ---: | ---: |
| 1 | 58 | 4 |
| 2 | 42 | 26 |
| 3 | 67 | 2 |
| 4 | 69 | 2 |
| 5 | 66 | 2 |
| 6 | 68 | 2 |
| **7** | **52** | **26** |

The generic DSI source allocator at MIBRoot VA `0x00833bcc` receives `srcNr`,
looks up the record at `0x00833c54`, loads the source ID from record `+0x00` and
the bandwidth from record `+0x04`, then supplies both to the allocation response
at `0x00833d30`. Its diagnostic is:

```text
SSBC::DSrc::reqAlloc(): [%s] srcNr %u, NetType %d, SubNetID %d
```

Thus source 7's allocation response carries `srcID 52` and bandwidth `26`.
The value is a fixed source-resource property. It is not calculated from six
PCM voices and is not read from the amplifier's sink profile.

### Independent AudioProcess source-7 record

AudioProcess contains an independent component definition at
VA `0x002d4004`:

```text
name          0x002d40a8 -> "MmxSourcePresCtrl"
kind          2
component ID  0xffff
source count  15
source table  0x002d3f80
sink count    2
sink table    0x002d4164
```

Its source table consists of three-byte `(source number, type, width)` records.
The source-7 record at VA `0x002d3f92` is:

```text
07 00 1a
```

That target record agrees exactly with MIBRoot. This is not a claim that every
unrelated source row in the two binaries is identical; source 8, for example,
differs between their static tables and does not participate in connection 162.

`CPMOSTSrcDevCtrl::initializeTables` at VA `0x001c86b4` finds the component,
iterates those records, and copies byte 2 into the per-source width field. The
stock trace calls the same byte `channels`:

```text
RPM::CPMOSTSrcDevCtrl::initializeTables():
Setting source %4u -> Type: 0x%02X; channels %u
```

Here `channels` is an imprecise diagnostic label. The value is later transported
as the 16-bit MOST BlockWidth; it is not a count of six PCM channels.

## Propagation into Connect

The relevant AudioProcess control path preserves the source allocation width:

```text
MmxSourcePresCtrl source 7 configuration       BW 26
  -> generic DSI source allocation response
  -> CGGenDSISrcDevCtrl::respAlloc              0x002848f0
  -> CGSyncSrcSRM::handleExtAllocRes             0x0027cacc
  -> source resource info +0x12                 0x0027cc94
  -> CGSyncSinkSRM::alloc                        0x00280850
  -> CGGenMOSTSnkDevCtrl::sendConnSR             0x0028d8e4
  -> CGGenMOSTSnkDevCtrl::raiseConnSR            0x0028cf80
  -> Connect.StartResult byte stream             00 1a
```

`handleExtAllocRes` logs the received value as `BW: 0x%04X` and stores it as a
half-word in the source resource object. `sendConnSR` stores the same half-word
at controller offset `+0x32` (`0x0028d97c`) and passes it to `raiseConnSR`
(`0x0028d9a4`).

`raiseConnSR` logs:

```text
RPM::CGGenMOSTSnkDevCtrl::raiseConnSR():
0x%04X, SnkNr: 0x%02X, %d, NWType: %d, BW: %d
```

When the supplied width is nonzero, it is serialized unchanged, high byte first,
at `0x0028d078..0x0028d094`. Only a zero-width call reaches the fallback lookup
at `0x0028d258`; that lookup would also return source 7's configured value 26.

The sink number remains the previously established physical resolution of
`eSNK_DTCP51`, namely `0x26`. The resulting Connect fields are therefore
`0x26/0x001a`, not `0x26/0x000e`.

## Source allocation versus sink declaration

These are separate records and must not be conflated:

| Stage | Owner | Value |
| --- | --- | ---: |
| MC logical source | MIBRoot route map | `MmxSourcePresCtrl/7` |
| Source allocation | MIBRoot/AudioProcess source table | **26** |
| Connect field | AudioProcess MOST sink controller | **26** |
| Sink `0x26` advertised profile | amplifier `SinkInfo.Get` | **14** |

`26` is not a stereo allocation. The subsequent signed-LPASS audit proves the
sample-slot packing which was unknown in this pass: the internal 32-bit containers
are repacked to six three-byte TDM slots. Thus `2 + 6*4` is not the physical TDM
layout. `2 + 8*3` matches the resource width and the available positions in the
shared group frame, but a direct binding of all 26 resource bytes remains unproved.

The lower `car` binary independently contains synchronous NAL resources with
configured bandwidth `26` (resource IDs 33 and 100). That corroborates the
presence of 26-byte transport resources, but this pass did not prove that either
numeric NAL ID is the final binding of `MmxSourcePresCtrl/7`; it is not used as
the primary source-width proof.

## What localizes the missing channels

The narrow runtime confirmation of this static result is the existing
`CGGenMOSTSnkDevCtrl::raiseConnSR` trace. For connection 162 it should print
`SnkNr 0x26` and `BW 26`. An independent control-plane capture can check
`26 00 1a` at offsets 2..4 of the amplifier's Connect payload.

That confirms allocation geometry but cannot show whether C/LFE/Ls/Rs contain
audio. The subsequent AFE audit adds a cheaper preceding check: the existing deva
start log exposes the custom SAD active-channel mask. `ASAD=0xc0` declares only
L/R active beside the six physical PCM slots; `ASAD=0xfc` declares all six.

Only if the live value is `0xfc` does the decisive one-hot measurement still need
the same samples observed at two data-plane boundaries:

1. Qualcomm ADSP/LPASS output at `TDM_OUT_MPL`, before the INIC/MLB/MOST
   formatter.
2. The allocated MOST synchronous source slots/on-wire payload.

Interpretation is direct:

| Observation | Fault boundary |
| --- | --- |
| C/Ls/Rs already absent at `TDM_OUT_MPL` | Qualcomm ADSP routing or packing after deva |
| Present at `TDM_OUT_MPL`, absent in MOST slots | INIC/MLB/MOST source formatter |
| Present on MOST, absent at amplifier matrix input | amplifier ingress/profile handling, including the 26-versus-14 discrepancy |

Since the HU source allocation is 26 bytes/frame, a capture showing the extra
channels absent at `TDM_OUT_MPL` would establish a remaining Qualcomm ADSP
runtime fault. Static analysis already proves a six-slot packing loop but cannot
show live sample values. Full SAD and slot-packer details are in
[`AFE_TDM_MOST_SLOT_AUDIT.md`](AFE_TDM_MOST_SLOT_AUDIT.md).

## Reproducibility and provenance

This audit targets the following exact MU1316 root filesystem, not the older
MIBRoot build used in the first sink-selection pass:

```text
/Users/luka/Desktop/AUDI_2/Firmwares/HU/
MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/rootifs_out/
```

| Binary | Size | SHA-256 |
| --- | ---: | --- |
| `usr/apps/MIBRoot` | 16,468,409 | `bab21755106ea5306f387102c0f81a5544f79f415de5564baec76fc7aaa05c81` |
| `bin/AudioProcess` | 2,086,332 | `3d591ef3263521ba520d301cfc9487e86badff736d53eb0d77dbe0a678df72f3` |
| `bin/car` | 1,654,633 | `f97e32e7602ea573c43769c45280bad3460187c1bde98894f0843ab0192c7f0c` |

Minimal byte-level check of the independent AudioProcess table:

```sh
r2 -q -e scr.color=false \
  -c 's 0x2d3f80; px 48; s 0x2d4004; pxw 28; s 0x2d40a8; psz' \
  rootifs_out/bin/AudioProcess
```

The MIBRoot initializer, logical mapping, and source-resource construction can
be checked without analysis-database writes:

```sh
r2 -q -e scr.color=false \
  -c 's 0x2046ac; pd 8; s 0x2054f8; pd 84; s 0x207dd4; pd 78' \
  rootifs_out/usr/apps/MIBRoot
```

The source records begin:

```text
01 00 04  02 00 1a  03 00 02  04 00 02
05 00 02  06 00 02  07 00 1a  08 00 02
```

Radare2 supplied reproducible raw-byte/disassembly checks; IDA decompiler and
xrefs were used to verify the class-level data flow and argument identities.
All use was read-only.
