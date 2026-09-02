# DIO media sink static audit (MU1316)

Date: 2026-09-02  
Scope: static analysis only; no hook, deva, HU, or amplifier binary was modified.

> Follow-up: [`MC_SELECTION_AUDIT.md`](MC_SELECTION_AUDIT.md) recovers the selector's
> exact persistent source. It is `Sound_System` in RCC EEPROM/basic coding, E2P record
> `0xF1` byte 11: value `2` publishes `ExternalAmplifierCoded=true`. For that external
> state, the `_MC + DTCP51` pair below is unconditional and no source channel count is
> consulted.

## Result

The stereo-sink hypothesis is **falsified** for the B&O/external-amplifier route.

For DSI connection `162` (`CL_ENT_AMP_DIO_MEDIA`, CarPlay/IOS media) and amplifier
terminal `1`, MIBRoot's stock routing table selects:

```text
eSRC_ENT_AMP_DIO_MEDIA_MC (0x46f)
eSNK_DTCP51              (2)
```

`eSNK_DTCP51` is the protected 5.1 resource. In the amplifier firmware its physical
profile is:

```text
SinkNr     = 0x26
BlockWidth = 0x000e (14 bytes)
format     = 6 x 16-bit PCM plus the protected-stream overhead
```

The route is selected from connection/coding tables. It is **not** derived from the QSA
voice count and therefore is not something that the deva caps patch needed to retrigger.
There is no smaller patch to make connection 162 select `0x26`: on an external-amplifier
car it already selects the logical resource that resolves to `0x26/14`. Changing the
logical sink word would move it away from the correct DTCP 5.1 route.

This closes only the proposed wrong-MOST-sink explanation. It does not identify the
remaining DIO-channel fault or prove a different DSP/MOST framing defect.

## Owner and call chain

There are two useful meanings of “the function that establishes connection 162”:

1. The exact DSI policy entry point is MIBRoot
   `CAudioManagementPresCtrl::requestRequestConnection`, recovered as
   `FUN_001f1790` (VA `0x001f1790`, file offset `0x000f1790`). It maps the HMI/DSI
   connection to logical router source and sink IDs.
2. The last named component issuing the network-side routing request is AudioRouter's
   `CMOSTRoutingClient` request path. The adjacent router stages are
   `CRoutingServer::requests`, `CConnection::manageConnection`, and, for the protected
   route, `CSPCClient::requests/responses`. Those class/method-scope names come from the
   stock `mm_audio.hbtc` trace catalog. The monolithic MIBRoot ELF is stripped, so the
   final `CMOSTRoutingClient` method has no defensible standalone symbol/address; assigning
   a guessed `FUN_...` address would overstate the evidence.

The recovered chain is:

```text
LSD BaseAudioService::callRequestConnection(162, terminal, channel)
  -> DSIAudioManagementProxy method 12 (three int32 values)
  -> MIBRoot CAudioManagementPresCtrl::requestRequestConnection / FUN_001f1790
  -> connection mapper / FUN_001e2a18
  -> DMM proxy wrapper / FUN_001d6cb4
  -> proxy serializer / FUN_00755a58
  -> AudioRouter.DMMRouting
  -> CRoutingServer -> CConnection::manageConnection
  -> CMOSTRoutingClient (+ CSPCClient for DTCP)
  -> amplifier FBlock 0x22, FktID 0x0112, StartResult
```

This also excludes the early candidates:

- `audio_service` is the Qualcomm CSD/DSP transport side, not the DSI-to-MOST policy
  owner.
- The Java/native DSI proxy is a marshaller. Its `requestConnection` method writes three
  `int32` values and invokes remote method `12`; it has no `SinkNr` or `BlockWidth`.
- `mediarouter` handles QSA mixer routing and is not the MOST connection policy owner.

## Exact connection-162 mapping

MIBRoot is an ARM ELF loaded at VA `0x00100000`; its first load segment begins at file
offset zero, so the file offsets below were also checked directly against the bytes.

`FUN_001e2a18` (VA `0x001e2a18`, file `0x000e2a18`) performs two 143-entry searches.
Each record is `0x14` bytes.

### Table 1: DSI connection and terminal to routing resource

Table base: VA `0x00ddefb8`, file `0x00cdefb8`.

The matching record for connection `162` (`0xa2`) and terminal `1` is at VA
`0x00ddf6e8`, file `0x00cdf6e8`:

```text
a2 00 00 00  01 00 00 00  00 00 00 00  78 00 00 00  01 00 00 00
| connection | terminal    | flags       | resource    | property    |
```

The mapper follows resource key `0x78` into the second table.

### Table 2: routing resource to internal/external-amplifier pair

Table base: VA `0x00ddfc1c`, file `0x00cdfc1c`.

Resource `0x78` is at VA `0x00de05a4`, file `0x00ce05a4`:

```text
78 00 00 00  6e 04 00 00  01 00 00 00  6f 04 00 00  02 00 00 00
| resource   | internal src| internal snk| external src| external snk|
```

Decoded:

| Amplifier coding | Source | Sink |
|---|---|---|
| internal (`mIsExternalAmpCoded == 0`) | `0x46e` = `eSRC_ENT_AMP_DIO_MEDIA` | `1` = `eSNK_SPEAKERS` |
| external/B&O (`mIsExternalAmpCoded != 0`) | `0x46f` = `eSRC_ENT_AMP_DIO_MEDIA_MC` | `2` = `eSNK_DTCP51` |

The external sink selector itself is the word at VA `0x00de05b4`, file
`0x00ce05b4`; its stock value is already `02 00 00 00`.

The source and sink names are independently anchored by MIBRoot's enum-to-string
tables:

```text
VA 0x00dddee4  eSRC_ENT_AMP_DIO_MEDIA
VA 0x00dddefb  eSRC_ENT_AMP_DIO_MEDIA_MC
VA 0x00dde98e  eSNK_SPEAKERS
VA 0x00dde99c  eSNK_DTCP51
VA 0x00dde9a8  eSNK_DTCP20
VA 0x00ddeaf6  eSNK_SPEAKERS_MC
```

`FUN_001f1790` calls the mapper and passes the selected source/sink pair to
`FUN_001d6cb4` (VA `0x001d6cb4`, file `0x000d6cb4`). That wrapper reaches
`FUN_00755a58` (VA `0x00755a58`, file `0x00655a58`), which serializes the pair to
the `AudioRouter.DMMRouting` service.

## Where the external-amplifier decision comes from

The selector is the runtime global at VA `0x010bc0e8`. `FUN_001e6a3c` (VA
`0x001e6a3c`, file `0x000e6a3c`) updates it from the amplifier-coding callback and
then rebuilds/re-evaluates routing state. Its meaning is confirmed independently by the
literal diagnostics used by `FUN_001f2cf0` (VA `0x001f2cf0`, file `0x000f2cf0`):

```text
informationStartAudio: using uninitialized mIsExternalAmpCoded
informationStopAudio: using uninitialized mIsExternalAmpCoded
```

Thus the multichannel choice is deliberately protected by “external amplifier coded,”
not inferred from `snd_pcm_channel_info`, the negotiated fragment size, or current PCM
voices. With external-amplifier coding enabled, the selected side is `eSNK_DTCP51`; the
other side is `eSNK_SPEAKERS`, not an alternative stereo MOST profile. Static analysis
recovers the condition and both outcomes, but does not read the live value of this RAM
global. The B&O hardware/configuration is the reason the external outcome applies here.

## Origin of physical `SinkNr` and `BlockWidth`

The per-connection MIBRoot table contains logical router IDs, not the seven-byte MOST
Connect payload. A scan of the whole MIBRoot image found no aligned static resource record
containing physical `SinkNr 0x26` together with bandwidth/block width `14`.

The generic router sink-resource constructor `FUN_0083599c` (VA `0x0083599c`, file
`0x0073599c`) consumes resource-map records with this recovered shape:

```text
+0x00  byte   snkNr
+0x04  uint32 snkID
+0x08  uint32 bandwidth
```

That map is obtained through the sink-device configuration interface. For an external
amplifier the physical profile is therefore resource/capability data, not a connection-162
literal and not a value synthesized from QSA voices.

The amplifier firmware supplies the decisive profile definition:

- `FUN_000C7424`, registered for local FktID `0x0111` (`SinkInfo.Get`), indexes the
  sparse sink tables by `SinkNr - 0x21`.
- The profile at index `5` is `SinkNr 0x26`, `Entertainment_5.1_SAD`, protected,
  six 16-bit channels, `BlockWidth 14`.
- `FUN_000C4B30`, registered for local FktID `0x0112`, parses the incoming
  `Connect.StartResult` payload as `SenderHandle(2), SinkNr(1), BlockWidth(2),
  ConnectionLabel(2)`, all multibyte fields big-endian.

Together, the HU logical enum `eSNK_DTCP51` and the amplifier's only protected 5.1
profile make the physical result unambiguous: `0x26/0x000e`.

## Smallest-change answer

No binary or configuration change is justified for this hypothesis. The apparent patch
site at MIBRoot VA `0x00de05b4` is already `eSNK_DTCP51`. Replacing it with
`eSNK_SPEAKERS_MC` (`31`) would not mean “choose amplifier sink 0x26”; it would replace
the correct external protected route with a different logical sink.

If an on-wire confirmation is desired later, the narrow measurement is to enable the
existing `CMOSTRoutingClient` request trace or capture the amplifier's Connect packet and
verify bytes `26 00 0e` at payload offsets 2..4. That is a confirmation step, not a patch,
and was outside this static-only pass.

## Reproducibility and provenance

The mapping was cross-checked in radare2, IDA, and Ghidra, then verified directly against
the file bytes.

- MIBRoot:
  `/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/MHI2Q_US_AUG22_P5087_MU1316/RE/MIBRoot/MIBRoot`
  
  SHA-256: `e6e710d7467be935c3fea933ef9068a66786c392f3f7a0e78155ad9a1063acd1`
- LSD JXE:
  `/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/ifs2/ifs_hmi/ifs/lsd.jxe`
  
  SHA-256: `abaf73d1e72e7178770e8a2b553cfd7afc954785b59f6db77bbb4c8700d3244b`
- Decompiled LSD tree:
  `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-combined-vf`
- Router trace catalog:
  `MU1316-special/rcc/mnt/efs-system/etc/config/mm_audio.hbtc`, lines 51-57.
- DSI/audio trace catalog:
  `MU1316-special/rcc/mnt/efs-system/etc/config/MIBRoot.AudioTraces.hbtc`, especially
  `CDSIAudioManagementSA` and `CAudioChannel_RPM`.
- Amplifier profile and handler audit:
  `/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/AMP_ENTERTAINMENT_7_1_MOST.md`.

Direct byte checks:

```text
MIBRoot file 0x00cdf6e8: a2000000 01000000 00000000 78000000 01000000
MIBRoot file 0x00ce05a4: 78000000 6e040000 01000000 6f040000 02000000
```
