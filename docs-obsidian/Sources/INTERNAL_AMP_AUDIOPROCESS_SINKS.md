# AudioProcess (RCC, MU1316) — internal-amplifier 6-channel route: static RE

Binary: `rcc_extracted/ifs3_out/bin/AudioProcess` (ARM32 QNX, base 0x100000, sha256 3d591ef3…), IDA session
`audioprocess` (existing .i64; router/CAudioManager code at 0x11xxxx–0x15xxxx IS defined, IDA's `find` just returns
substring offsets, so xrefs must be taken from the string *start*). Helpers also opened: `collectConfigData`
(ifs2 /usr/apps), `libSPC_configuration.so` (ifs3 /lib), `dev_signalpath_configurator` (strings only).
Read-only; the three `define_func` calls on already-defined functions were no-ops. Tags: **PROVEN** = decompiled /
raw bytes in this binary, **INFERRED** = follows from PROVEN facts but the exact code path was not walked.

## TL;DR

* `eSNK_SPEAKERS_MC` (31) is a dead end on the internal amp: the XML maps it to AM sink 13 and the internal-amp sink
  map (`sub_146B78`) maps AM 13 → **DSI sink 1 = the same stereo "Entertainment Stereo" input as `SPEAKERS`**
  (BW 4). Patching MIBRoot to {0x46f, 31} would allocate the 26-byte source and then squeeze it into the stereo sink.
* The internal amp *does* have a 26-byte multichannel input: DSI sink **6 "Entertainment Multichannel"** (RPM table
  width 26), and it is already reachable through the stock XML sink **`MULTICHANNEL` = `eSNK_DTCP51` nr 1 → AM
  sink 10 → internal sink 6**. Nothing in AudioProcess or the XML has to change.
* Minimal patch = MIBRoot record 0x78 internal pair → **{0x46f, 2 (eSNK_DTCP51)}**, keep `Sound_System`=1 /
  `--amptype=122`. Everything downstream (XML conn `LC_ENT_AMP_DIO_MEDIA.MULTICHANNEL`, RPM sink width 26, router
  DEVICE_INTERNAL path, SPC source input for MOST srcID 52) exists in stock. The only open point is the DSP
  application: does the C6000 `AU_6CH` program accept the 26-byte crossbar input (SPC local source 9) on its main
  zone with 6 channels. → verdict **NEEDS DSP SUPPORT** (no hard wall found on the RCC ARM side).
* Coding the car external (`Sound_System`=2) is a **hard wall**: `collectConfigData` then starts AudioProcess with
  `--amptype=EXT`, `CMainAmp` ignores the internal amplifier area 51 ("IGNORED (external amplifier configured)") and
  waits for a MOST amplifier that does not exist.

---

## 1. Sink implementation (how (src,snk) from MIBRoot becomes a physical path)

### 1.1 Entry: DMMAudioManagement → XML connection → resource
MIBRoot sends logical `(src amid, snk amid)` (e.g. `(0x46e=1134, eSNK_SPEAKERS=1)`) over DSI `DMMAudioManagement`
(server strings `CDMMAudioManagementStub.cpp` 0x2f9958, requests `requestRequestFocus/MakeAudible/ReleaseFocus`
0x2eac84/0x2eab7c/0x2eabf0, trace `requestFocus(theSource=…, theSink=SPEAKERS)` 0x2bdb58). **PROVEN (strings +
stub)**. The AudioManagement server resolves the pair against the audio table's `<conn>` list, whose `<resource
snk= src=>` names the XML `<src>`/`<snk>` records (parser `sub_18C9C8` @0x18c9c8: elements `conn`, `resource`
(`src`,`snk` attrs), `xactions/xaction`, `limit`, `fade-override`, flags `flag-ENT`, `flag-foreground`, `attn-type`,
`volumelock`, `dsp-limiter-set-id`, `prio`). **PROVEN**.

### 1.2 `<snk>` → AM sink id (physical sink enum) — `sub_18E048` @0x18e048, PROVEN
`amid` is parsed numerically or as `eSNK_*` via the 33-entry name table `off_2C35F4` (index = enum value; [31] =
`SPEAKERS_MC`, [32] = `ICC`). `nr` → +8, `amname` → explicit physical id via the 16-pair table @0x2c3574
(`ENT1`→0, `ENT120`→0, `ENT2`→1, `ENT220`→1, `ENT151E`→2, `ENT171FE`→3, `MULTI-COMPAT`→10, `ANN110`→4, `ANN210`→5,
`ANN310`→6, `HP1`→7, `TPMEMO`→8, `REF`→9, `TLAM`→12, `SPEAKERS_MC`→13, `ICC`→14). Without `amname` the default rules
at 0x18e404–0x18e578 apply:

| amid | nr | AM sink id | name (table @0x2adb6c, index = id+1) |
|---|---|---|---|
| 1 eSNK_SPEAKERS | 1 | **0** | ENT1_STEREO |
| 1 | 2 / 3 / 4 | 4 / 5 / 6 | ANN1 / ANN2 / ANN3 |
| 2 eSNK_DTCP51 | 1 | **10** | ENT1_OLD_DTCP51 ("MULTI-COMPAT") |
| 31 eSNK_SPEAKERS_MC | 1 | **13** | SPEAKERS_MC |
| 32 eSNK_ICC | 5 | 14 | ICC |
| 15 eSNK_MUHP | – | 7 | HP1 |
| 30 eSNK_TLAM | – | 12 | TLAM |
| 6 eSNK_MU_REF | – | 9 | REFSINK |

Full AM sink enum (name table 0x2adb6c): 0 ENT1_STEREO, 1 ENT2_STEREO, 2 ENT1_DTCP51, 3 ENT1_DTCP71, 4–6 ANN1–3,
7 HP1, 8 TPMEMO, 9 REFSINK, 10 ENT1_OLD_DTCP51, 11 (COUNT, unused), 12 TLAM, 13 SPEAKERS_MC, 14 ICC.

So for the G22 table: `SPEAKERS`→0, `MULTICHANNEL`→10, `SPEAKERS_MC`→13, `ANN1..3`→4..6, `HEADPHONE1`→7, `REF`→9,
`ICC`→14, `TLAM`→12.

### 1.3 AM sink id → amplifier sink number — `CMainAmp` (0x170-byte object, ctor `sub_147044`, base `sub_14A828(…,1 src,11 snk)`)
Two hard-coded maps, selected by the amplifier "area" that connects (`sub_14907C` @0x14907c: area **51 = internal**
`"CMainAmp: amplifier area %u (0x%x, internal)"`, area **42 = MOST** `"(0x%x, MOST)"`; the wrong one is rejected:
`"amplifier area 51 (internal) IGNORED (external amplifier configured)"` / `"area 42 (MOST) IGNORED (internal
amplifier configured)"`, decided by amp-descriptor byte +20 = external). Entries are 16 B `{AM sink id, x, y, sinkNr}`
added by `sub_1469CC`; `SetSinkDelay` (`sub_14262C`) reads them (`"%s not supported by the amplifier"` when absent).
**PROVEN (decompile + raw disasm 0x146b78–0x146cbc).**

| AM sink id | internal amp `sub_146B78` (area 51) | external MOST amp `sub_146A1C` (area 42) |
|---|---|---|
| 0 ENT1_STEREO (`SPEAKERS`) | **1** | 0x21 (33) |
| 4 / 5 / 6 ANN1–3 | 2 / 3 / 4 | 0x22 / 0x23 / 0x24 |
| 2 ENT1_DTCP51 | **6** | 0x26 (38) |
| 3 ENT1_DTCP71 | – | 0x32 (50) |
| 10 ENT1_OLD_DTCP51 (`MULTICHANNEL`) | **6** | **0x32 (50)** |
| 7 HP1 (`HEADPHONE1`) | 9 | 9 |
| 12 TLAM | 1 | 1 |
| **13 SPEAKERS_MC** | **1** (= stereo sink!) | – (absent → "not supported by the amplifier") |
| 14 ICC | 11 | 0x22 |

`CAudioManager::SetSinkdelay` (`sub_15A600`) confirms the numbering: sink 1 = "INT_AMP SPEAKERS", 50 = "EXT AMP MULTI
CHANNEL", 9 = "HEADPHONE". Note for the B&O audits: on the external amp the XML `MULTICHANNEL` sink is MOST sink
**0x32** (BW 26 in the FBlock-0x22 table below), not 0x26 (that one is `ENT1_DTCP51`, BW 14) — the earlier
"Connect to 0x26" claim was inferred from the amplifier side, this table is the HU side.

### 1.4 Internal sink numbers = DSI sinks of component `CDSIAudioManagement`
Descriptor list @0x2fc5ec (`sub_1C7414` installs it; `RPM::CPRPMCreator`), entries `{name, kind, id, nSrc, srcTab,
nSnk, snkTab}` with 3-byte rows `{nr, type, width}` (`CPMOSTSrcDevCtrl/CPDSISnkDevCtrl::initializeTables` copy byte 2
as "channels" = MOST BlockWidth, `sub_1C86B4` / `sub_1C9418`). **PROVEN (raw bytes + decompile).**

* `CDSIAudioManagement` (kind 2, @0x2d3f64, string 0x2eb0b8): 1 src {1:2}; **11 sinks @0x2d4140:**
  `1:4 2:2 3:2 4:2 5:4 6:26 7:26 8:4 9:4 10:4 11:2`.
  Names @0x2b8630..: 1 "Entertainment Stereo", 2 "Infotainment Mono (Navi)", 3 "… (SDS)", 4 "… (Phone)", 5 "TPMemo",
  **6 "Entertainment Multichannel"**, 7 "Entertainment Multichannel (?)", 8 "RefChannel", 9 "Headphone 1",
  10 "Headphone 2", 11 "In Car Communication".
* `RPM_MOST_FBLOCK_22` (kind 1, id 0xff22, @0x2d3fe8): 1 src {0x2d:2}; 13 sinks @0x2d41e8:
  `0x21:4 0x22:2 0x23:2 0x24:2 0x25:12 0x26:14 0x27:6 0x28:16 0x29:18 0x2a:9 0x2b:18 0x2c:21 0x32:26`.
* `MmxSourcePresCtrl` (kind 2, @0x2d4004): 15 sources @0x2d3f80 `1:4 2:26 3:2 4:2 5:2 6:2 7:26 8..15:2`; 2 sinks {1:2,2:2}.
* others: FBLOCK_34 (7 src), FBLOCK_CA (6 src @0x2d3fb8), FBLOCK_D6 (2 snk @0x2d3fb0), WPSystem/WPRing/tuners (1 src each).

### 1.5 Physical path per sink (what actually gets configured)
* **Internal sinks (1..11)** are `DEVICE_INTERNAL` (netID 1) for the router (`CRoutingServer::requestSinkInit2`
  `sub_1205C4`: netID 1 = "DEVICE_INTERNAL", 2 = "MOST_25", 3 = "MOST_50P"). `CRouter::startSinkInit2` (`sub_114A00`)
  with netID 1 attaches the sink to the *source's* connection ("find connection for source") and
  `CConnectionPending::SinkInit` (`sub_1235D0`) sends **`CrossbarConnect.SetGet` to spc** (`/dev/spc`,
  `CSPCClient.requestSpcConnect(source, sink, zone)`), i.e. the DSP crossbar of the Jacinto C6000 via
  `dev_signalpath_configurator` → `/dev/io-amp/routing` (`dev-ioamp-router` → dspipc). The MOST source (MMX INIC sync
  channel) is connected to the crossbar first (`CConnectionDisconnected.SourceInit - MOST_25 sourceNr → Send
  CrossbarConnect to spc`). **PROVEN (decompiles + strings).**
  IoAmp addressing of the internal sinks (`Amp:PC` table @0x2b8718, 9×12 B `{area, sinkNr, zone, sink, addGal, gal}`,
  accessor `sub_15D0B4`): sink 1→zone0/sink0, 2→z0/s1, 3→z0/s2, 4→z0/s3, **6→zone0/sink0**, 7→z0/s0, 9→zone2/s0,
  10→z2/s1, 11→z0/s5. So the 26-byte sink 6 addresses the same DSP main zone as the stereo sink 1. **PROVEN.**
* **MOST sinks (external amp)**: netID MOST_25 → router creates a connection object → `MOSTRouting.SinkInit`
  (`CMOSTRoutingClient`) → RPM `CGGenMOSTSnkDevCtrl::sendConnSR` → MOST `Connect(SinkNr, BlockWidth)` (documented in
  the earlier BlockWidth audit; sink numbers from §1.3).
* **A2I / dspipc (`A2I_Client_Open`, `/dev/dspipc/dra6xx.0`)** is only the **WavePlayer** hardware player
  (`CHWPlayerIOAmpRouter::initDevice` `sub_2962E8`): `uNumChannels` = 1 or 2 only
  (`A2IUtils::convertNumberOfAudioChannels(): Unknown number of audio channels!` for anything else, `sub_2974D4`).
  It is not the entertainment path. **PROVEN.**

### 1.6 Answer for the three sinks
| logical sink | AM id | internal amp | external amp |
|---|---|---|---|
| eSNK_SPEAKERS (`SPEAKERS`) | 0 | DSI sink 1, width 4 (stereo), crossbar → DSP zone 0 | MOST sink 0x21, BW 4 |
| eSNK_DTCP51 (`MULTICHANNEL`) | 10 | **DSI sink 6, width 26**, crossbar → DSP zone 0 | MOST sink 0x32, BW 26 |
| eSNK_SPEAKERS_MC (`SPEAKERS_MC`) | 13 | DSI sink 1 (stereo) — real code, but it is an alias of SPEAKERS | not in map → rejected |

`SPEAKERS_MC` is therefore "implemented" but useless for 6 ch; `DTCP51` is the multichannel input on both amp kinds.

## 2. Source widths

* Table @0x2d3f80 = `MmxSourcePresCtrl` source list (3-byte `{srcNr, type, width}`); `CPMOSTSrcDevCtrl::initializeTables`
  (`sub_1C86B4`) copies byte 2 into the per-source record ("channels" in the trace = MOST BlockWidth). Source 1 → 4,
  source 7 → 26. Sink tables are copied the same way (`sub_1C9418`). **PROVEN.**
* (a) MOST allocation: the source record's width is what the RPM sends in Connect (earlier audit; unchanged).
* (b) Internal DSP input: the router does not carry the width to spc (`sub_1235D0` passes only source nr, sink nr and
  one byte from the connection object); the SPC's own config (`libSPC_configuration.so`) maps the MOST source number
  to a DSP crossbar input: `sourceMapping` @0x2a98 = 35 × `{globalId, device 0xD001}` where the low byte is the MOST
  srcNr and the high byte the local DSP input — **srcID 0x34 (52 = MmxSourcePresCtrl/7, 26 B) → local input 9;
  srcID 0x3a (58 = /1, stereo) → local input 10**; 0x42/0x45/0x43/0x44 (Mmx/5,4,3,6) → inputs 11,12,14,15.
  Device 0xD001 = `/dev/io-amp/routing`, 26 local sources, 5 zones (`deviceConfigList` @0x2be0). The per-source
  channel info (`dsp_sourceAdditionalInfo`, `dsp_sourceDefaultZones`, `dsp_zoneInternalFlag`) lives in **.bss** and is
  filled at runtime from the device, so the SPC check `"connection not possible, number of channels does not match
  for device %s"` compares values that come from the DSP side — not decidable statically. **PROVEN (tables) /
  INFERRED (runtime fill).**
* `isSpecialInternalSource` (CRouter vtable slot +204 → `sub_113A04` → object at CRouter+12, vtbl+20) rewrites the
  net of *special* sources to NWT_INTERNAL; the DIO source is a normal MOST_25 source (its srcID is in the SPC MOST
  mapping), so this does not apply. **INFERRED** (the special-source list itself was not decoded).
* `ch_desc`: for internal sources it is a 4-byte buffer holding the source number (`setResultSourceInit`
  `sub_117AAC`); the router only checks that a MOST sink's ch_desc is non-empty ("invalid channel description").
  No channel-count arithmetic anywhere in the router. **PROVEN.**
* A 26-byte source on the internal path is accepted by AudioProcess as long as the sink is one with width 26
  (sink 6/7). `startSourceInit` only rejects a *re-init* of an existing source with a different bandwidth.

## 3. Local FBlock 0x22 / TDM

* `RPM_MOST_FBLOCK_22` is the **external MOST amplifier** (area 42 in `CMainAmp`), not the internal amp. The internal
  amplifier is DSI component `CDSIAudioManagement` + SPC crossbar + `/dev/io-amp/routing`; no MOST synchronous
  connection is involved for internal sinks (`CConnectionPending.SinkInit - DEVICE_INTERNAL … CrossbarConnect.SetGet`).
  **PROVEN.**
* FBlock 0x22 sinks known to the HU: 0x21 BW4 (stereo), 0x22–0x24 BW2, 0x25 BW12, 0x26 BW14, 0x27 BW6, 0x28 BW16,
  0x29 BW18, 0x2a BW9, 0x2b BW18, 0x2c BW21, **0x32 BW26** (used for `MULTICHANNEL`/`ENT1_DTCP71`).
* `Sound_System`=2 on an internal-amp car: `collectConfigData` (`sub_101E14`) writes `--amptype=EXT` when E2P amptype
  byte (cfg+24) == 2, else `--amptype=120/121/122` from the channel count (cfg+32: 2/4/6; unknown → 121 + "wrong number
  of channels"). With EXT, `CMainAmp` refuses area 51 → **no internal audio at all. HARD WALL.** MIBRoot patch must
  keep `Sound_System`=1.

## 4. Audio table

* Loaded at start by `CAudioMngWrapper` (`sub_13B01C`): args `--audiotable=`, `--audiobrand=`, `--amptype=`,
  `--tlam-dir=`; default `/HBpersistence/etc/MIB_AudioTable.xml` only when `--audiotable` is absent; `/dev/null` or
  empty → built-in fallback config (`sub_168F30`). The RCC start line comes from `/tmp/startArgs_AudioProcess`
  written by `collectConfigData`: `--audiotable=/etc/MIB_AudioTable_%s.xml` with `AU_G22` for brand AU / topology 3
  (else AU_G11, VW, SK, SE, PO_G11/G24, BY), `--amptype=EXT|120|121|122`, plus `--tp=/etc/config/mm_audio.hbtc …`.
  `srv-starter.cfg:573` starts `/bin/AudioProcess #/tmp/startArgs_AudioProcess`. **PROVEN.**
* Loader `sub_18B2A8`: `gzopen(path,"r")` ("cannot be opened for reading and decompression"; zlib 1.2.3 linked) →
  libxml2 `xmlReadIO` with `NONET|NOBLANKS|PEDANTIC|DTDVALID|DTDATTR|DTDLOAD|RECOVER` (2461). DTD failure only warns
  ("invalid configuration, trying to recover"; the DTD `audiomanagement-mib1.dtd` is not shipped); non-well-formed
  XML is fatal ("is an invalid XML"). So the edited file must be **gzip-compressed and well-formed**; DTD
  conformance is optional. Configuration parse order (`sub_18E630`): `<src>` (`sub_18DD8C`), `<snk>` (`sub_18E048`),
  `<conn>` (`sub_18C9C8`), settings/constraints (`sub_18B8E0/18BF40`), amplifier list (`sub_18B57C`). **PROVEN.**
* Search/override paths: none besides the argument and the `/HBpersistence/etc/MIB_AudioTable.xml` default.
  Amplifier override files: `/HBpersistence/etc/AmplifierCoding` (`"INTERN|EXTERN [x] <id>"`) and
  `/HBpersistence/etc/useExternalAmplifier` (`sub_15CDDC`) are consulted **only when `--amptype` is absent**
  (`sub_13AA10`); with the production start line they are irrelevant.
* Elements: `<src amid= id= amname= ref=>`, `<snk amid= id= nr= amname= flag-connectable= flag-fadeable=>`,
  `<conn id= …><resource snk= src=/><onstart/>…<xactions ref=><xaction ref=>S|D|A|D2|D3…</xaction></xactions>
  <settings>…</settings></conn>`, `<settings id= constraints=>`, `<constraints id=>`, `<amplifier id= settings=
  external= …/>`.
* `<amplifier id="122" settings="AU_6CH"/>` only binds amp id "122" (= `--amptype=122`) to the `AU_6CH`
  `<settings>`/`<constraints>` blocks (sound-setting ranges: `SURROUND_ACTIVITY`, `SUBACTIVITY`, `SINKDELAY`,
  `INPUTGAINOFFSET`, `VOL_ENT_*` …). It defines **no channel layout and no input format**; the sink/zone plumbing is
  hard-coded in AudioProcess (§1.3/§1.4). The static fallback table @0x2b924c holds the same ids
  (0 AU_2CH/"120", 1 AU_4CH/"121", 2 AU_6CH/"122" internal; 3 AU_PREM1/"004" external; VW/SK variants).
* A `LC_ENT_AMP_DIO_MEDIA.SPEAKERS_MC` connection is **not needed**: `LC_ENT_AMP_DIO_MEDIA.MULTICHANNEL`
  (`<resource snk="MULTICHANNEL" src="LS_ENT_AMP_DIO_MEDIA.MC"/>`, xactions present) already routes the MC source to
  AM sink 10 → internal sink 6. If one insisted on SPEAKERS_MC it would additionally require changing `sub_146B78`
  (AM 13 → 6 instead of 1), i.e. a code patch for no gain.

## 5. Patch list

(a) **MIBRoot** record 0x78 @0xDE05A4: internal pair (+4 src, +8 snk) `6e 04 00 00 | 01 00 00 00` →
    **`6f 04 00 00 | 02 00 00 00`** (eSRC_ENT_AMP_DIO_MEDIA_MC, eSNK_DTCP51) — i.e. copy the external pair into the
    internal columns; **not** {0x46f, 31}. Nothing else in MIBRoot: the mapper only reads this record, the reverse
    mapper uses the same bytes, the name lookups already know both values, and MIBRoot already registers
    `MmxSourcePresCtrl/7` for 0x46f. (Sanity, not analysed here: any MIBRoot policy that keys on snk==DTCP51 beyond
    the DSI call — the earlier MIBRoot audit says there is none.)
(b) **AudioProcess**: no change. Optional cosmetic alternative — if you want the logical name to stay SPEAKERS_MC,
    change the byte at 0x146c74's operand (`STR R6` = 1) so AM 13 maps to sink 6; needs an instruction patch (MOV
    R8→store), not a table byte. Not recommended.
(c) **XML**: no change. (`MIB_AudioTable_AU_G22.xml` must stay gzip if ever edited.)
(d) **Coding**: keep `Sound_System`=1 (internal) and 6 channels (`--amptype=122`); do not code external.
(e) **DSP / SPC (hand-over)**: prove that the internal `AU_6CH` C6000 application (`/etc/application.bin` via
    `dev-dspipc`) accepts SPC local source **9** (26-byte MOST sync input, = `MmxSourcePresCtrl/7`, srcID 0x34) on
    DSI sink 6 / IoAmp zone 0 with 6 discrete channels, and how the 26 bytes are laid out in that input
    (2 + 8×3 vs 6×4 — same open point as the B&O audit). SPC runtime check to watch: `"number of channels does not
    match for device /dev/io-amp/routing"`; router trace to watch: `CConnectionPending.SinkInit - DEVICE_INTERNAL
    sinkNr=0x6`, `CSPCClient.requestSpcConnect(source=0x34, sink=…, zone=…)`.

Hard walls found: none on the RCC ARM side for the DTCP51 route. Stereo-only points: SPEAKERS_MC→sink 1 (by table,
avoidable by not using it), A2I WavePlayer (1–2 ch, irrelevant), external coding on an internal car (fatal).

## Verdict: NEEDS DSP SUPPORT (not a hard wall). Patch = MIBRoot 8 bytes only.
