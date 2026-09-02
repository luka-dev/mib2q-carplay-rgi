# Connection 162 multichannel-selection audit

Date: 2026-09-02  
Scope: static analysis of the MU1316 head-unit firmware and the already-audited amplifier material. No binary was modified.

> **Subsequent amplifier result:** the logical SHARC input mixer was later recovered from
> `AD301A07` in the V850 CodeFlash. Sink `0x26` selects a 5.1 bank that routes all six
> inputs; it does not mute C/LFE/Ls/Rs. See
> [`SHARC_INPUT_OUTPUT_MATRIX.md`](SHARC_INPUT_OUTPUT_MATRIX.md). Statements below that
> the matrix is absent describe the boundary at the time of this head-unit audit and are
> superseded by that later result.
>
> **Subsequent source-width result:** `eSRC_ENT_AMP_DIO_MEDIA_MC` resolves to
> `DSI:MmxSourcePresCtrl/7`, which allocates 26 bytes per synchronous frame. The HU's
> actual Connect fields are `SinkNr 0x26, BlockWidth 0x001a`. The sink's advertised
> `BlockWidth 14` is a separate record. See
> [`MOST_SOURCE_BLOCKWIDTH_AUDIT.md`](MOST_SOURCE_BLOCKWIDTH_AUDIT.md).

## Result

For an externally coded amplifier, connection `162` selects the multichannel pair
**unconditionally**:

```text
CL_ENT_AMP_DIO_MEDIA (162), terminal 1
  if ExternalAmplifierCoded == 0:
      eSRC_ENT_AMP_DIO_MEDIA    (0x46e) -> eSNK_SPEAKERS (1)
  else:
      eSRC_ENT_AMP_DIO_MEDIA_MC (0x46f) -> eSNK_DTCP51   (2)
```

There is a non-`_MC` source, but it belongs to the **internal-amplifier** branch. It is
not a stereo alternative offered to an external amplifier. Once
`ExternalAmplifierCoded` is true, neither the CarPlay source, QSA voice count, a PR code,
nor an amplifier capability response participates in this choice.

The condition originates in persistent RCC basic coding:

```text
storage:       EEPROM/basic-coding record returned as E2P address 0x00f1
field:         byte 11, symbolic name Sound_System
internal:      Sound_System = 1
external:      Sound_System = 2
runtime prop:  Amp.Amplifier.ExternalAmplifierCoded
route cache:   MIBRoot byte at VA 0x010bc0e8
```

This B&O car satisfies the external condition operationally: connection 162 is audible
through the external MOST amplifier. The internal branch terminates at
`eSNK_SPEAKERS` and cannot account for that observation. A live EEPROM read was not made
in this static pass, so the exact byte is inferred from the active external-amplifier
path rather than copied from a vehicle dump.

The head-unit control plane does **not** declare a CarPlay channel count. Its audio
request contains connection ID, terminal and group only. Therefore the new one-hot result
(L/R audible; C/LFE/Ls/Rs silent) is not explained by MIBRoot choosing a stereo route.
The HU already asks for the protected 5.1 logical sink. Its physical sink is `0x26`, whose
advertised profile has `BlockWidth 14`; the later source audit shows that the controller
independently supplies `BlockWidth 26` in Connect.

The surviving boundary at this stage was the amplifier's SHARC source-to-output matrix.
The later amplifier audit recovered its logical 8×8 input-mixer layer from A07 and ruled
out muted C/LFE/Ls/Rs rows. The still-later source audit also rules out a stereo-width MOST
allocation. The remaining localization boundary is the audio content at Qualcomm
`TDM_OUT_MPL` versus the content of the allocated MOST synchronous slots.

## Vehicle measurement that defines this audit

A standalone `qsa_play` one-hot test wrote six-channel PCM directly to
`/dev/snd/mpl5_dio_ent`, with CarPlay absent:

```text
device limits: voices 6..6 rate 48000..48000 frag 18432..18432 formats 0x1000
TOTAL 1152000 frames in 23879 ms -> 48243 frames/s

channel 1  L    audible
channel 2  R    audible
channel 3  C    silent
channel 4  LFE  silent
channel 5  Ls   silent
channel 6  Rs   silent
```

The six-voice render-width defect is already fixed and vehicle-confirmed. This audit does
not reopen deva/CSD, the physical sink table, or the CarPlay transport.

## Exact connection-table decision

`CAudioManagementPresCtrl::requestRequestConnection` at MIBRoot VA `0x001f1790`
calls the mapper at VA `0x001e2a18`. The mapper first resolves connection/terminal to
routing resource `0x78`, then resolves that resource through a second 20-byte-record
table.

The connection record at VA `0x00ddf6e8` is:

```text
a2 00 00 00  01 00 00 00  00 00 00 00  78 00 00 00  01 00 00 00
| connection | terminal    | flags       | resource    | property    |
```

The resource record at VA `0x00de05a4` is:

```text
78 00 00 00  6e 04 00 00  01 00 00 00  6f 04 00 00  02 00 00 00
| resource   | internal src| internal snk| external src| external snk|
```

At `0x001e2a9c..0x001e2abc`, the mapper loads the byte at `0x010bc0e8`,
compares it with zero, and selects source `[record+0x0c]` when nonzero or
`[record+0x04]` when zero. At `0x001e2ac4..0x001e2aec`, the same byte selects
sink `[record+0x10]` or `[record+0x08]`. There is no other branch or argument in
this decision.

That distinction matters: the alternatives are not external stereo versus external 5.1.
They are internal-amplifier output versus external-amplifier protected 5.1 output.

## Runtime origin of `ExternalAmplifierCoded`

### Persistent field and values

The engineering scripts expose the symbolic basic-coding field and both values:

```sh
# extracted/app/eso/hmi/engdefs/scripts/SCALE/amplifier_intern.sh
on -f rcc /ffs/extbin/apps/bin/VIPCmd ee bc Sound_System 1

# extracted/app/eso/hmi/engdefs/scripts/SCALE/amplifier_extern.sh
on -f rcc /ffs/extbin/apps/bin/VIPCmd ee bc Sound_System 2
```

`ee bc` is the RCC EEPROM/basic-coding interface. The native consumer supplies the
physical record location. `CPAudioPersistenceClient`'s ReadE2P response handler at
MIBRoot VA `0x001e03e8` does the following:

```text
0x001e0418  compare returned E2P address with 0xF1
0x001e0428  load byte [response_data + 0x0B]
0x001e0430  cache it at AudioPersistence + 0x124
0x001e048c  reject 0xFF and fall back to internal value 1
0x001e04b4  compare cached value with 2
0x001e04d8  compare it with 2 again
0x001e04dc  produce false when not equal
0x001e04e0  produce true when equal
0x001e04e8  publish the resulting external-amplifier boolean
```

The adjacent diagnostics are literal and remove ambiguity:

```text
invalid sound system ==> set SoundSystem = INTERN
[AudioDiag] [AudioPersistence] Amptype from EEPROM is %s
```

The separate detailed `CodedAmplifier` enum (Basic 2CH, Standard 6CH, Alpine B&O,
Advanced B&O, and so on) is used for diagnostics and dataset plausibility. Its callback
at `0x0020a2f4` warns `Amplifier isn't coded correct! please check Byte 11 ;-)` for enum
zero. The routing mapper does not read that enum; it reads only the boolean property
described above.

### Property to routing selector

The audio presentation controller registers an amplifier client against service
`Amp.Amplifier`. Its `onExternalAmplifierCodedUpdate` callback is the thunk/body at
`0x001e6a34/0x001e6a3c`. For a valid update state (`2`) it performs:

```text
0x001e6a50  load ExternalAmplifierCoded
0x001e6a58  set the local validity byte at controller + 0x57d
0x001e6a60  store the value at controller + 0x57c
0x001e6a64  store the same value at global VA 0x010bc0e8
0x001e6a68  re-evaluate routing state
```

Only that global is read by the connection-table mapper. There is no CAN/MOST amplifier
query on this selector path and no config-file channel-width input.

## No CarPlay/HU channel-count declaration

The public LSD interface is:

```java
// org/dsi/ifc/audio/DSIAudioManagement.java
void requestConnection(int connection, int terminal, int group);
```

The decompiled names are confirmed by `BaseAudioService.toDebug`, which prints the three
values as `AC`, `AT`, and `group`, and by `callRequestConnection`, which forwards exactly
those three `int32` values. Connection 162 is the source identity
`CL_ENT_AMP_DIO_MEDIA`; it is not accompanied by sample format or voice count.

The `org.dsi.ifc.carplay` API is also width-free. `ServiceConfiguration` carries initial
app/resource ownership, display geometry, input features, Bluetooth identities, handedness
and night mode. `ResourceRequest` carries ownership/transfer policy. `mainAudioType`
describes the audio role. None has a channel-count field.

There is an unrelated `org.dsi.ifc.media.AudioStream.numChannels`, but its references are
limited to media-player stream enumeration/serialization and DVD audio-stream UI. It is
not consumed by `DSIAudioManagement.requestConnection` or the connection-162 mapper.

At the lower data-plane boundary, QSA does negotiate six voices with deva. That declaration
is real and is what fixed the earlier CSD width defect, but it never feeds MIBRoot's
amplifier-pair selector.

## Decision

No head-unit patch is justified for multichannel selection. On this externally coded car,
the stock connection-162 policy already and unconditionally selects
`eSRC_ENT_AMP_DIO_MEDIA_MC + eSNK_DTCP51`. The measured loss of source positions 3..6
occurs after that logical selection.

The later A07 audit answers the static amplifier part: the selected 5.1 bank accepts and
routes all six MOST source positions. The source-allocation audit then proves a 26-byte,
not stereo-width, source. The next useful measurement is a one-hot capture at Qualcomm
`TDM_OUT_MPL` and at the MOST synchronous source slots; that distinguishes ADSP packing
from INIC/MOST formatting and amplifier ingress.

## Provenance

- MIBRoot:
  `/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/MHI2Q_US_AUG22_P5087_MU1316/RE/MIBRoot/MIBRoot`
  
  SHA-256: `e6e710d7467be935c3fea933ef9068a66786c392f3f7a0e78155ad9a1063acd1`
- Decompiled LSD tree:
  `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-combined-vf`
- Basic-coding scripts:
  `/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app/eso/hmi/engdefs/scripts/SCALE/`
- Physical sink proof: [`DIO_MEDIA_SINK_STATIC_AUDIT.md`](DIO_MEDIA_SINK_STATIC_AUDIT.md)
- Fixed render-width proof and deployment chain:
  [`ROUTER_INPUT_WIDTH_AUDIT.md`](ROUTER_INPUT_WIDTH_AUDIT.md) and
  [`../deploy/deva-ctrl-qc/BUILD_CHAIN.md`](../deploy/deva-ctrl-qc/BUILD_CHAIN.md)
- Amplifier/SHARC boundary:
  `/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/AMP_ENTERTAINMENT_7_1_MOST.md`
- Recovered logical SHARC matrix: [`SHARC_INPUT_OUTPUT_MATRIX.md`](SHARC_INPUT_OUTPUT_MATRIX.md)
- Source allocation and actual Connect BlockWidth:
  [`MOST_SOURCE_BLOCKWIDTH_AUDIT.md`](MOST_SOURCE_BLOCKWIDTH_AUDIT.md)
