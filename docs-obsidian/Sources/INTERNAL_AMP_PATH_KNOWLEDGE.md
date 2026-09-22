# Internal-amplifier (non-B&O) path — what the repo + firmware already prove

Date: 2026-09-19. Read-only research; nothing modified, no binary executed, car untouched.
Firmware roots used:
- `MMX` = `Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/` (app, ifs2, persist, system)
- `RCC` = `Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/rootifs_out/` (+ `rcc_extracted/ifs{1,2,3}_out`)
- `MU1316-EEProm.{bin,txt}`, `MU1316-LOG.txt` in the same MU1316 dir (real dump of THIS B&O car).

**Correction to the task premise, first:** `MIBRoot` (the audio-policy owner, `CAudioManagementPresCtrl`,
`AudioPersistence`, `Amp.Amplifier`) runs on the **RCC**, not the MMX (`rootifs_out/usr/apps/MIBRoot`
== `MIBRoot_RCC`, both 16,468,409 B; `rootifs_out/usr/apps/` also holds `modifyE2P`, `mib2_ioc_flash`).
Everything amplifier-specific lives on the RCC; the MMX audio stack is amplifier-agnostic (see §1.4).
Also: `DEVICETYPE_STANDARD_6CH` (=3) is the **internal** 6-channel amp (`AU_6CH`, id 0x122), not an
external non-B&O amp (see §1.3).

---

## 1. PROVEN — internal speaker sink path

### 1.1 Route selection (RCC MIBRoot) — `docs-obsidian/Sources/DIO_MEDIA_SINK_STATIC_AUDIT.md`, `docs-obsidian/Sources/MC_SELECTION_AUDIT.md`
Connection 162 (`CL_ENT_AMP_DIO_MEDIA`, terminal 1) → resource `0x78` → record @VA `0x00de05a4`:

| `ExternalAmplifierCoded` | source | sink |
|---|---|---|
| 0 (internal) | `eSRC_ENT_AMP_DIO_MEDIA` (0x46e) | `eSNK_SPEAKERS` (1) |
| 1 (external, B&O/Bose) | `eSRC_ENT_AMP_DIO_MEDIA_MC` (0x46f) | `eSNK_DTCP51` (2) |

Selector = single byte @VA `0x010bc0e8`, written by `onExternalAmplifierCodedUpdate` (`0x001e6a3c`) from DSI
property `Amp.Amplifier.ExternalAmplifierCoded`. That boolean is computed by `CPAudioPersistenceClient`
(`0x001e03e8`): E2P record `0xF1`, byte `+0x0B` (`Sound_System`); `==2` → external; `0xFF` → "invalid sound
system ==> set SoundSystem = INTERN"; anything else → internal. No channel count, PR code or amp capability
participates. `[AudioDiag] [AudioPersistence] speakerCoding = EXT_AMP / INT_AMP_2CH / INT_AMP_4CH / INT_AMP_6CH`
strings exist in the same MIBRoot (RCC) — the 2/4/6 split comes from the detailed `CodedAmplifier`
enum, which the router does **not** read.

### 1.2 MOST source width on the internal branch — `docs-obsidian/Sources/MOST_SOURCE_BLOCKWIDTH_AUDIT.md`
`eSRC_ENT_AMP_DIO_MEDIA` (0x46e) → `DSI:MmxSourcePresCtrl/1` → **srcID 58, BlockWidth 4 bytes** per
synchronous frame (= 2 × 16-bit = stereo). The MC branch is source 7 (srcID 52, BW 26). Table @MIBRoot VA
`0x00207dd4`; AudioProcess copy @VA `0x002d3f80` (`01 00 04 … 07 00 1a`). So on an internal-amp car the HU
allocates a **stereo-width** synchronous resource for CarPlay media. There is no 6-ch DIO route in the
stock table at all: the G22 audio table (`RCC etc/MIB_AudioTable_AU_G22.xml`, gzip; decoded copy next to
this file) has

```
<snk amid="eSNK_SPEAKERS"    id="SPEAKERS"     nr="1"/>
<snk amid="eSNK_DTCP51"      id="MULTICHANNEL" nr="1"/>
<snk amid="eSNK_SPEAKERS_MC" id="SPEAKERS_MC"  nr="1"/>
<conn id="LC_ENT_AMP_DIO_MEDIA.SPEAKERS">     <resource snk="SPEAKERS"     src="LS_ENT_AMP_DIO_MEDIA"/>
<conn id="LC_ENT_AMP_DIO_MEDIA.MULTICHANNEL"> <resource snk="MULTICHANNEL" src="LS_ENT_AMP_DIO_MEDIA.MC"/>
```
`SPEAKERS_MC` is declared but referenced by **zero** connections (53 use SPEAKERS, 35 MULTICHANNEL).

### 1.3 Amplifier identities — audio table `<amplifier>` block + LSD Java
```
<amplifier external="no" … subwoofer="1"/>                      (default = internal)
<amplifier id="120" settings="AU_2CH" …/>   <amplifier id="121" settings="AU_4CH" …/>
<amplifier id="122" settings="AU_6CH"/>
<amplifier id="004" ref="140"/>  <amplifier external="MOST" id="140" settings="AU_PREM_1"/>
<amplifier external="MOST" id="160" settings="AU_PREM_2"/>  <amplifier external="MOST" id="161" settings="AU_STD_1"/>
<amplifier external="MOST" id="181" movingLenses="1" settings="AU_ADV_1"/>  (B&O 3D Advanced = this car)
```
`Tools/jxe2jar/out/MU1316-vf/de/audi/tone/app/AmplifierVariant.java` maps DSI `amplifier` ints →
ids: 1 BASIC_2CH→0x120, 2 BASIC_4CH→0x121, **3 STANDARD_6CH→0x122 (internal 6ch)**, 18 STANDARD_BOSE_AUDI→0x161,
4 ALPINE_BO_AUDI→0x140, 17 PREMIUM_BOSE_AUDI→0x160, 21 ADVANCED_BO_AUDI→0x181(AU_ADV_1).
Enum values: `org/dsi/ifc/audio/Constants.java` (`DEVICETYPE_BASIC_2CH=1 … STANDARD_6CH=3 … ADVANCED_BO_AUDI=21`).
RCC `AudioProcess --amptype=<id>` help: "If not given, /HBpersistence/etc/AmplifierCoding is read, if it does
not provide the identifier – /HBpersistence/etc/useExternalAmplifier, **otherwise an internal 6-channel
amplifier will be assumed**."

### 1.4 MMX/deva/ADSP side is amplifier-agnostic (so the hook's PCM6 stream always reaches TDM)
- `MMX system/etc/boot/startup.sh`: `VARIANT=G22` hardcoded (l.426); `io-audio -d qc …platform_id=mib2…`
  and `ACDB_CFG_PATH=/etc/acdb/mmx2_8064/acdb_cfg audio_service` are unconditional (l.618–655). No coding
  read anywhere in MMX boot scripts; no PPS object mirrors coding (MMX binaries reference only
  `/pps/services/audio/mixer`, usb, networking, `/pps/system/nvram/deviceinfo`).
- deva has exactly one entertainment output: `PCM_PLAY_MPL_1 -> TDM_OUT_MPL` (driver strings; `deva_mixer_router_init`),
  CSD device 78, six 24-bit slots at byte offsets 2,5,8,11,14,17 of a 32-byte grouped TDM frame, plus the
  2-byte SAD header (`docs-obsidian/Sources/AFE_TDM_MOST_SLOT_AUDIT.md`, `docs-obsidian/Sources/DEVICE_TOPOLOGY_AUDIT.md`). The only other
  outputs are ANN/TEL (`TDM_OUT_ANN_1_{16,24,48}KHZ`, `TDM_OUT_ANN_2`, `TDM_OUT_TEL`); `TDM_OUT_MPL2_ID` is
  "not supported". Device nodes: `mono_/stereo_/six_channel_speakerp`, `stereo_to_all_speakersp`.
- Therefore on an internal-amp car the MMX still packs L R C LFE Ls Rs into six TDM slots and sends the
  ASAD mask exactly as on the B&O car; the **width reduction happens on the RCC**: only a 4-byte
  (stereo) synchronous source is allocated for `MmxSourcePresCtrl/1`.
- Stock internal-amp cars already run this identical 6-slot AFE config with a 2-voice CSD session, so
  the "first two slots ↔ 4-byte source" binding is exercised in production (inference, not measured).

### 1.5 What the internal amplifier physically is (RCC)
- RCC SoC = TI Jacinto 5 (DRA6xx) with a C6000 DSP: `rcc_extracted/ifs2_out/lib/dll/dspipc-dra6xx.so`
  ("chipset layer dll for the Jacinto 5 dspipc communication", usage
  `dev-dspipc -ddra6xx,…,loader=/etc/bootloader.bin,filename=/etc/application.bin`, "not a C6000 COFF file?").
- Amp DSP control: `rootifs_out/bin/dev-ioamp-router` (`/dev/io-amp`, ops `Amp_Equalizer*`, `Amp_ChannelDelay`,
  `Amp_SinkDelay`, `Amp_Loudness`, `Amp_MOSTMute`, `Amp_LimiterChannelGroupMute`…) over `libdspipc.so`.
- Signal-path arbiter: `rootifs_out/bin/dev_signalpath_configurator` (`/dev/spc`) — refuses
  "connection not possible, number of channels does not match for device %s".
- Audio data into the DSP: `AudioProcess` `A2I_Client_*` via `rootifs_out/opt/sys/lib/liba2itodspipc.so.1`
  → `/dev/dspipc/dra6xx.0` (params `uNumChannels`, `uSizeOfSample`, `uSampleRate`);
  `router.CRouter.setResultSourceInit: isSpecialInternalSource, mapping ch_desc to buffer`;
  `CAudioManager: SetSinkdelay(INT_AMP SPEAKERS)`.
- The RCC is itself a MOST node hosting FBlock 0x22 (`RPM_MOST_FBLOCK_22` in AudioProcess), i.e. the
  internal amp is the local Amplifier FBlock (inference from strings; not traced).

### 1.6 Answer to (a): a 6-voice PCM stream on `mpl5_dio_ent` with route `eSNK_SPEAKERS`
- Not downmixed anywhere on the HU: LPASS's generic downmixer bypasses 5.1→5.1, the AFE packer writes all
  six slots (`docs-obsidian/Sources/LPASS_COPP_TOPOLOGY_AUDIT.md`, `AFE_TDM_MOST_SLOT_AUDIT.md`).
- The RCC allocates only 4 bytes/frame for this source → **C/LFE/Ls/Rs are not transported**; expected
  audible result = **front L/R only**, everything the hook renders into the other four channels is lost.
  Undefined-behaviour risk (not "silent, benign"): SAD `ASAD=0xfc`/24-bit declared against a stereo
  allocation, and the SPC channel-count check — neither observable statically.
- With `AUDIO_SPATIAL=0` (2-ch, ASAD 0xc0) + the deployed mirror deva (`min=max=6`), QSA converts 2→6 the
  same way `mpl1_int_ent` has always served stereo (`deploy/deva-ctrl-qc/BUILD_CHAIN.md`), so slots 1/2
  carry L/R exactly as stock → this is the safe policy for `Sound_System != 2`.

---

## 2. UNKNOWN — needs measurement or RE
1. Live behaviour of PCM6 + `ASAD=0xfc` on the BW-4 internal route: clean L/R only? SPC/DSP rejection?
   level change? Needs an internal-amp car (or the INIC/RCC `isSpecialInternalSource` mapping RE).
2. Whether the internal `AU_6CH` DSP application (`/etc/application.bin`, C6000) can ingest 6 channels at
   all; the stock table never routes `SPEAKERS_MC`, so a 6-ch internal route would need MIBRoot/AudioProcess
   table changes on the RCC — out of scope for a hook.
3. External non-B&O (Bose `0x160/0x161`, `AU_STD_1`/`AU_PREM_2`): `Sound_System=2` → same
   `eSNK_DTCP51` MC route, but the Bose amp's sink `0x26` profile / BlockWidth acceptance / SHARC matrix
   are unaudited (all amp audits are `Firmwares/SoundAmp` = B&O J525). PCM6 there is UNVALIDATED.
4. Which E2P bytes carry the detailed `CodedAmplifier` enum (0x120/0x121/0x122/0x140/…); only byte 11 of
   record 0xF1 (`Sound_System`) is proven for the router. `startLoudspeakerDiagnosis(SoundSystem(%d), coded(0x%4x)…)`
   shows both exist as separate values.
5. Whether `/HBpersistence/etc/AmplifierCoding` / `useExternalAmplifier` exist on a production RCC (only
   `/HBpersistence/SWDL/...` paths appear in `MU1316-RCC_fs0.bin` strings) — probably engineering overrides.
6. `modifyE2P` exact stdout format for a 1-byte read at a non-16-aligned address (dump tool printed
   16-byte rows labelled `0x<addr>\t…`; `modifyPackage.sh` greps `05` from `r 0xe0 0x01`, so a bare
   value line is expected). Confirm once on the unit.
7. Whether `[AudioDiag] … Amptype from EEPROM is %s` / `speakerCoding = …` ever reach an slog readable via
   `on -f rcc sloginfo` (HBTC trace scopes; `MIBRoot.AudioTraces.hbtc`).

---

## 3. Reading the amplifier coding from a shell at CarPlay start (MMX) — ranked

**Ground truth on this car** (`MU1316-EEProm.bin` @0xF1, and `MU1316-LOG.txt:14 "Coding: 0194530200…"`):
```
0xF1: 01 94 53 02 00 00 00 00 13 22 00 [02] 04 1a 00 10 2f 00 12 e5 01 70 09 04 be
                                   byte 11 = 0xFC = 0x02 → Sound_System = 2 = external (B&O)   ✔ matches audits
```
This 25-byte block is the 5F "long coding"; E2P address 0xF1 is a flat EEPROM offset (dump is flat, MIB tool
prints the same bytes).

| rank | method | evidence | caveats |
|---|---|---|---|
| **1** | `on -f rcc /usr/apps/modifyE2P r fc 1` (or `r f1 19` and take the 12th byte) | RCC-resident tool `rootifs_out/usr/apps/modifyE2P` — "Usage (all values in hex) / Read: %s r <address> <length> / Example: %s r 100 0a"; used by RCC boot script `rcc_extracted/ifs2_out/usr/bin/modifyPackage.sh:32` `( /usr/apps/modifyE2P r 0xe0 0x01 2>&1 \| grep -q 05 )` → proves scriptable, grep-able; `MMX startup.sh:367` already runs `on -f rcc /usr/apps/mergelog -v` → `on -f rcc` works from the MMX; `MU1316-EEProm.txt` header "Wait for DSI to connect … DSI Connected role1.DSIE2PAccess" = this tool produced the dump | blocks until RCC DSI `DSIE2PAccess` is up (fine at CarPlay start; wrap with a background+kill timeout, QNX 6.5 has no `timeout`); value `02`→external, `01`→internal, `ff`→treat as internal (MIBRoot fallback). Read-only, no VIP write. |
| 2 | `cat /net/rcc/HBpersistence/etc/AmplifierCoding` / `…/useExternalAmplifier` (also try `/net/rcc/mnt/efs-persist/etc/…`) | `AudioProcess` help text (RCC) names exactly these files as the amptype source before the internal-6ch default; qnet `/net/rcc/…` is used by `MMX startup.sh:378` and the MIB tool (`/net/rcc/mnt/efs-persist/SWDL/Log`) | existence on production units unverified (§2.5); if absent, absence ≠ internal. Cheap `ls` probe first. |
| 3 | Java LSD hook: `DSISound.ATTR_ACTIVEAMPLIFIERCAPABILITIES=35` → `DSISoundListener.updateActiveAmplifierCapabilities(AmplifierCapabilities{amplifier=DEVICETYPE_*})`; our `java_patch_new_*` runs in the same JVM as `de/audi/tone/app/dsi/DSISoundListenerImpl.java` → write `/tmp/carplay_amp_type` | gives the full amp id (distinguishes B&O Advanced 21 / Bose 17,18 / internal 1,2,3) — the only route that can gate Bose separately | needs a Java patch + arrives whenever LSD boots vs. when `carplay_startup.sh` runs (order unproven); MMX-side only. |
| 4 | `on -f rcc sloginfo \| grep -E 'Amptype from EEPROM\|speakerCoding ='` | MIBRoot strings `[AudioDiag] [AudioPersistence] Amptype from EEPROM is %s`, `speakerCoding = INT_AMP_2CH/4CH/6CH/EXT_AMP`, `onExternalAmplifierCodedUpdate(%d, state=%d)` | trace-scope dependent, unproven to appear; try once, don't design around it. |
| 5 | `VIPCmd ee bc Sound_System` read form | only the **write** form is evidenced (`MMX app/eso/hmi/engdefs/scripts/SCALE/amplifier_{intern,extern}.sh`: `on -f rcc /ffs/extbin/apps/bin/VIPCmd ee bc Sound_System 1\|2`); binary is on RCC `/ffs` which is in no local image (`MU1316-RCC_fs0.bin` has zero `VIPCmd` strings) | read syntax unknown; VIP tool = IOC command channel — don't probe blind at startup. |
| — | PPS / `/dev/shmem/metainfo2.txt` / `MU1316-datasets.csv` | checked; nothing mirrors coding or amp type | not a route. |

Sketch for `deploy/smartphone_integrator/carplay_startup.sh` (rank 1, to be verified on the unit once):
```sh
# ponytail: one E2P byte decides the whole policy; fall back to stereo on any doubt
SS=$(on -f rcc /usr/apps/modifyE2P r fc 1 2>/dev/null | awk '/^0x/{print $2}')
case "$SS" in 02) export AUDIO_SPATIAL=1 ;; *) export AUDIO_SPATIAL=0 ;; esac
```
(`AUDIO_SPATIAL` is the existing gate at `carplay_startup.sh:57`; `AUDIO_STEREO_UPMIX=on` at l.60 must
also go off with it, since the upmix output is PCM6.)

---

## 4. Files worth deeper RE (paths)
RCC (amplifier side — where all the differences are):
- `rootifs_out/usr/apps/MIBRoot` (= `MIBRoot_RCC`, SHA `bab21755…`): `CPAudioPersistenceClient` E2P 0xF1 handler
  @`0x001e03e8`; `CodedAmplifier` callback @`0x0020a2f4` ("please check Byte 11"); `MmxSourcePresCtrl` source
  table @`0x00207dd4` (source 1 = BW 4); route table @`0x00ddefb8`/`0x00ddfc1c`. Find which E2P bytes carry
  the detailed amp id and whether any path routes `eSNK_SPEAKERS_MC`.
- `rootifs_out/bin/AudioProcess` (SHA `3d591ef3…`): sink init for `eSNK_SPEAKERS` (`requestSinkInit2 … bandwidth, ch_desc`),
  `isSpecialInternalSource` ch_desc→buffer mapping, `A2I_*` open params (channels for the internal DSP),
  `--amptype` / `/HBpersistence/etc/AmplifierCoding` parser, `MIB_AudioTable.xml` loader.
- `rootifs_out/etc/MIB_AudioTable_AU_G22.xml` (gzip, 12,802 lines decoded → `MIB_AudioTable_AU_G22.decoded.xml`
  beside this file): amplifier ids/settings, all conn/sink graphs; siblings `_AU_G11`, `_VW`, `_SK`, `_PO_*`, `_BY`.
- Internal amp DSP: `rcc_extracted/ifs2_out/bin/dev-dspipc`, `rcc_extracted/ifs2_out/lib/dll/dspipc-dra6xx.so`,
  the C6000 images `/etc/bootloader.bin` + `/etc/application.bin` (not found under `rcc_extracted/ifs*_out` at
  depth 5 — locate in `MU1316-RCC_fs0.bin` / `/ffs`), `rootifs_out/bin/dev-ioamp-router` (+ `libioamp-router-config.so.1`),
  `rootifs_out/bin/dev_signalpath_configurator` (channel-match rule), `rootifs_out/opt/sys/lib/liba2itodspipc.so.1`.
- `rootifs_out/bin/car` (MOST NetServices; NAL resources BW 4/26, INIC socket/MLB channel setup) — the
  TDM-slot → synchronous-source byte binding that every audit still lists as an inference.
- `rootifs_out/usr/apps/modifyE2P` — the E2P reader (DSI `DSIE2PAccess` client), rank-1 tool above.
MMX (already audited, nothing amp-specific, listed for completeness):
- `app/armle/lib/dll/deva-ctrl-qc.so` (device table @VA `0xA7D44`, router `deva_mixer_router_init`, SAD devctl `0x8018fa01`),
  `app/armle/bin/audio_service`, `app/armle/images/lpass.signed.mbn`, `system/etc/acdb/mmx2_8064/Speaker_cal.acdb`
  (dev 78 property `0x1319f` slot map), `system/etc/boot/startup.sh` (l.415–435 env, l.612–655 audio launch).
Java (MMX LSD): `Tools/jxe2jar/out/MU1316-vf/de/audi/tone/app/AmplifierVariant.java`,
  `…/de/audi/tone/app/dsi/DSISoundListenerImpl.java`, `…/org/dsi/ifc/audio/{Constants,AmplifierCapabilities,DSISound,DSISoundListener}.java`.
Dumps: `MU1316-EEProm.bin/.txt` (flat 8 KiB EEPROM, coding @0xF1), `MU1316-LOG.txt` (MIB tool session incl. `Coding:` line
  and a `modifyE2P` write at 0x3F0).
