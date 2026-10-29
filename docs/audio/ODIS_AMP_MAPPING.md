# Audi B&O J525 (`8W0035465` / `EV_AMPMst16C2Gen2ALPI`) — ODIS-E ODX & V850 Firmware Mapping

> `<AUDI_WORKSPACE>` denotes the external firmware/RE working set, not a directory shipped with this repository. Source filenames, builds and evidence offsets below identify the required artifacts.

<a id="external-amplifier-support"></a>

## External amplifier support

**Current compatibility, 2026-10-29:** CarPlay 5.1 and UpMix support all external
amplifier families used by the supported Audi MHI2Q systems. **B&O and Bose are
confirmed.** The cross-amplifier firmware analysis below covers the MOST inputs,
channel routing, DSP matrices and sound controls; older notes calling Bose
"unaudited" are superseded.

| Amplifier family | Device type | CarPlay audio path |
| --- | --- | --- |
| Alpine B&O | `0x140` | External MOST, six-channel media |
| Bose Standard / Bose 3D | `0x161` / `0x160` | External MOST, six-channel media; firmware accepts sinks `0x26` and `0x32` with BlockWidth 26 |
| B&O Advanced | `0x181` | External MOST, six-channel media on sink `0x32`, BlockWidth 26 |
| Internal TI C674x | `0x120`–`0x122` | Stock CarPlay stereo; the installer disables the six-channel path |

Support for six-channel input does not mean the amplifiers use identical sound
controls. In particular, Bose `setSurroundOnOff(false)` changes the Centerpoint
level; it does not select the B&O linear bypass. Section 8 records the differences.
An internal amplifier with six speaker outputs also does not imply a six-channel
CarPlay input route; see [Internal Amp Path](INTERNAL_AMP_PATH.md).

Evidence: cross-amplifier firmware analysis in section 8; compatibility status
confirmed by the project owner on 2026-10-29. The dated vehicle tests in other
notes retain their original scope and are not test records for every amplifier.

## 1. How ODIS-E Stores the J525 ODX (`BV_SoundSysteUDS` / `EV_AMPMst16C2Gen2ALPI_001`)

The following evidence comes from the external `ODIS_E/AU49X/` working set. These licensed ODIS files are not distributed in this repository; filenames and source line ranges are retained for reproducibility:
- `index.xml:1860–1869` maps `BV_SoundSysteUDS` (`BV_SoundSysteUDS_011033_d.odx`) and `EV_AMPMst16C2Gen2ALPI_001` (`EV_AMPMst16C2Gen2ALPI_001016_d.odx`).
- `rt_index.xml:1014–1018` compiles both the Base Variant (`BV_SoundSysteUDS`) and ECU Variant (`EV_AMPMst16C2Gen2ALPI_001`) into `0.0.0@BV_SoundSysteUDS.bv.db` (343 concatenated `zlib` chunks), with shared MIB structures in `0.0.0@BL_DataLibraMIB.sd.db` (3,972 `zlib` chunks).
- All 32-bit string IDs inside `.db` chunks resolve via:
  - `AStringData.idx.gz` + `AStringData.data.gz` (1,814,299 ASCII keys; header `u32 count`, records `<II (offset, key_id)`, string `u32 len + bytes`).
  - `UStringData.idx.gz` + `UStringData.data.gz` (1,218,324 UTF-16LE keys; `u32 len + len*2 bytes`).
- Localized `MASxxxxx` / `IDExxxxx` text keys resolve in `C:\ProgramData\OE\DIDB\db\didb_Base-en_US.data` on the ODIS-E host.

---

## 2. Official 16-Channel Speaker Map (`DOP_TEXTTABLELPChaneSelec` & DID `0x18EE`)

From `0.0.0@BV_SoundSysteUDS.bv.db` (Chunks 110 & 201) and `didb_Base-en_US.data`, ODIS defines the exact 16 amplifier channels (`Channel_1..16`, raw `0x00..0x0F`), which match the V850 channel index `0..15` 100%:

| ODIS Channel | Raw / V850 Index | DIDB Text ID | Official ODIS English Name | Audi Current Flow No. 45 Symbol & T38 Pins | Coding `0x0600` (`AF AA AF AA`) | DSP Slot (`0x68B40`) |
|---|---:|---|---|---|---|---:|
| `Channel_1` | `0` (`0x00`) | `MAS01189` | Left front midrange speaker | **R103** door mid + **R20** lower A-pillar tweeter (T38/11,12) | `TT=1, HT=1` (`11b`) | Slot 11 |
| `Channel_2` | `1` (`0x01`) | `MAS01192` | Right front midrange speaker | **R104** door mid + **R22** lower A-pillar tweeter (T38/13,14) | `TT=1, HT=1` (`11b`) | Slot 10 |
| `Channel_3` | `2` (`0x02`) | `MAS06562` | Right front treble speaker 2 | **R221** upper A-pillar 3D tweeter R (T38/24,32) | `TT=1, HT=0` (`10b`) | Slot 9 |
| `Channel_4` | `3` (`0x03`) | `MAS06561` | Left front treble speaker 2 | **R220** upper A-pillar 3D tweeter L (T38/23,31) | `TT=1, HT=0` (`10b`) | Slot 8 |
| `Channel_5` | `4` (`0x04`) | `MAS11755` | Left front midrange speaker 2 | **R276** dash side 3D mid L (T38/25,33) | `TT=1, HT=0` (`10b`) | Slot 4 |
| `Channel_6` | `5` (`0x05`) | `MAS11756` | Right front midrange speaker 2 | **R277** dash side 3D mid R (T38/26,34) | `TT=1, HT=0` (`10b`) | Slot 5 |
| `Channel_7` | `6` (`0x06`) | `MAS01200` | Right effect speaker | **R210** C-pillar surround R (T38/28,36) | `TT=1, HT=0` (`10b`) | Slot 6 |
| `Channel_8` | `7` (`0x07`) | `MAS01199` | Left effect speaker | **R209** C-pillar surround L (T38/27,35) | `TT=1, HT=0` (`10b`) | Slot 7 |
| `Channel_9` | `8` (`0x08`) | `MAS01194` | Left rear bass speaker | **R15** rear-left door bass + **R14** tweeter (T38/4,5) | `TT=1, HT=1` (`11b`) | Slot 15 |
| `Channel_10` | `9` (`0x09`) | `MAS01196` | Right rear bass speaker | **R17** rear-right door bass + **R16** tweeter (T38/16,17) | `TT=1, HT=1` (`11b`) | Slot 14 |
| `Channel_11` | `10` (`0x0A`) | `MAS01198` | Center speaker | **R208** centre mid (T38/6,18) | `TT=1, HT=0` (`10b`) | Slot 13 |
| `Channel_12` | `11` (`0x0B`) | `MAS11754` | Center speaker 2 | **R219** centre tweeter (T38/3,15) | `TT=1, HT=0` (`10b`) | Slot 12 |
| `Channel_13` | `12` (`0x0C`) | `MAS01188` | Left front bass speaker | **R21** front-left door woofer (T38/7,8) | `TT=1, HT=0` (`10b`) | Slot 0 |
| `Channel_14` | `13` (`0x0D`) | `MAS01191` | Right front bass speaker | **R23** front-right door woofer (T38/9,10) | `TT=1, HT=0` (`10b`) | Slot 1 |
| `Channel_15` | `14` (`0x0E`) | `MAS04506` | Double subwoofer speaker 1 | **R211** subwoofer winding A (T38/19,20) | `TT=1, HT=0` (`10b`) | Slot 3 |
| `Channel_16` | `15` (`0x0F`) | `MAS04507` | Double subwoofer speaker 2 | **R211** subwoofer winding B (T38/21,22) | `TT=1, HT=0` (`10b`) | Slot 2 |

---

## 3. UDS Service `0x22` / `0x2E` DID Map (ODIS ↔ V850 Firmware ↔ EEPROM)

### 3.1 Writable DIDs (`0x2E` Table at V850 `0x00040F40`, `FUN_0009f6a0`)

| DID | Len | ODIS ID & Name | ODIS Structure | V850 Write Handler | EEPROM Offset (`S-25C640A`) | Value in Dump (`8W0035465`) |
|---|---:|---|---|---|---|---|
| `0x0600` | 14 B | `IDE00003` **VW Coding Value** | `STRUC_VWCodinValueTextuSoundSysteGen2` | `0x000A5BD4` → `FUN_000c3d90` | `0x00E9..0x00F6` (csum `0x00F7`) | `01 94 04 08 00 AF AA AF AA 00 00 01 00 00` |
| `0x0982` | 73 B | `IDE10493` **Electrical test 2 of loudspeakers (AC-Diagnostics)** | `STRUC_CALACDiagnLoudsAMPGen2` | `0x000A5E68` | `0x0159..0x01A1` (csum `0x01A2`) | `01` + 24× `4A 38 3C` (`19000` Hz, `60/3 = 20.0` dB atten) |
| `0x0B2C` | 6 B | `MAS02809` **Check tone frequency (Acoustic LP test)** | `STRUC_CALAcousLPTestFrequ` | `0x000A5FE2` | `0x01A5..0x01AA` (csum `0x01AC`) | `0F A0` (4000 Hz), `03 E8` (1000 Hz), `00 64` (100 Hz) |
| `0x00BE` | 34 B | `IDE02185` **IKA-Key (Component Protection Key)** | `STRUC_CALIKAKey` (write-only!) | `0x000A66DC` | `0x0053..0x0096` / `0x03A5+` | Encrypted CP key material |
| `0x0500` | 1 B | `MAS00194` **Video_audio_encryption** | `STRUC_CALVideoAudioEncry` | `0x000A607E` | `0x01F7` | `01` (active) |
| `0xF198` | 6 B | `IDE00319` **Repair Shop Code / Tester Serial** | Standard Identification | `0x000A5B7C` | `0x0113` / `0x0130` | `16 11 23 B5 00 00` |
| `0xF199` | 3 B | `IDE00038` **Programming Date** | BCD `YY MM DD` | `0x000A5BA4` | `0x0113..0x0115` | `16 11 23` (2016-11-23) |
| `0xF1A0` | 11 B | `IDE00048` **VW Data Set Number** | ASCII 11B | `0x000A5D02` | `0x0124..0x012E` | `8W2909510A ` |
| `0xF1A1` | 4 B | `IDE00047` **VW Data Set Version Number** | ASCII 4B | `0x000A5D30` | `0x0141..0x0144` | `0006` |
| `0xF1A4` | 12 B | `IDE00036` **Vehicle Equipment Code & PR Number** | 12B binary | `0x000A5DD8` | `0x0100..0x010B` | `00 00 0E 2C 18 B5 F8 00 00 00 00 00` |

### 3.2 Key Corrections to Earlier Firmware Reverse Engineering

1. **DID `0x0600` (EEPROM `0x00E9`, 14 bytes) is Variant Coding (`IDE00003 VW Coding Value`), NOT Component Protection!**
   - Earlier docs (`AMP_CP_STATEMACHINE_AND_PATCH.md`, `AMP_AUDIO_DSP_TUNING.md`) misidentified `0x0600` (`FUN_000c29e0` / `FUN_000c3ca0` / `FUN_000c3d90`) as Component Protection (`gp-0x4580` "CP channel mask").
   - In reality:
     - **Byte 0 (`0x01`)**: Brand = `AUDI` (`0x01`), Brand Sound = `default` (`0x00`).
     - **Byte 1 (`0x94`)**: Car Class = `4` (B-segment / A4-A5-Q5), Car Generation = `9` (B9 / AU49X).
     - **Byte 2 (`0x04`, `gp-0x3643`)**: Car Derivate = `4` (body derivative), Supplement = `0`.
     - **Byte 3 (`0x08`)**: Aircondition installed (`bit 3`).
     - **Byte 4 (`0x00`)**: Country code.
     - **Bytes 5–8 (`AF AA AF AA`, stored in `gp-0x4580`)**: **Channels 1..16 Speaker Installation Bitmap** (`2 bits per channel: TT = Woofer/Mid, HT = Capacitively coupled Tweeter`).
       - `0xAF` (`10101111b`) on Byte 5: Channels 1 & 2 have `TT=1, HT=1`; Channels 3 & 4 have `TT=1, HT=0`.
       - `0xAA` (`10101010b`) on Byte 6: Channels 5, 6, 7, 8 have `TT=1, HT=0`.
       - `0xAF` (`10101111b`) on Byte 7: Channels 9 & 10 (Rear Doors with parallel Woofer + Tweeter) have `TT=1, HT=1`; Channels 11 & 12 have `TT=1, HT=0`.
       - `0xAA` (`10101010b`) on Byte 8: Channels 13, 14, 15, 16 have `TT=1, HT=0`.
     - **Bytes 9–10 (`00 00`)**: Channels 17..24 (`00 00` required on 16-ch amp; `FUN_000c3ca0` sets coding fault `0x5A` if non-zero).
     - **Byte 11 (`0x01`)**: `Microphone_1` (VNC roof mic R164) = `installed`.
     - **Byte 12 (`0x00`)**: `ANC_Microphone_1..4` = `not_installed`.
     - **Byte 13 (`0x00`)**: `drive_type` = `combustor`.

2. **DID `0x0982` (EEPROM `0x0159..0x01A1`, 73 bytes) is Tweeter AC-Diagnostics (`IDE10493`), NOT Per-Channel Music Gain!**
   - Earlier docs (`AMP_AUDIO_DSP_TUNING.md` §5) assumed the `5000..20000` (default `19000`) values read by `FUN_000c2a30` were per-channel music gain limits.
   - ODIS `STRUC_CALACDiagnLoudsAMPGen2` proves this is the **AC-Diagnostics High-Frequency Impedance Test** for capacitively coupled tweeters:
     - 1 byte `Test_Mode` (`0x01` = automatic)
     - 24 × 3-byte records (`u16be Test_Frequency` in Hz [`5000..20000`, default `0x4A38 = 19000` Hz] + `u8 Test_Volume` attenuation in `1/3 dB` steps relative to `14 Vpp = 0 dB` [default `0x3C = 60` = `20.0 dB` attenuation]).

3. **Actual Component Protection DIDs & Services**:
   - **DID `0x00BE` (34 bytes, write-only)** = `IDE02185 IKA-Key` (`STRUC_CALIKAKey`, V850 handler `0x000A66DC`).
   - **DID `0x03DE` (read-only)** = `Theft protection: error counter` (V850 handler `0x000A5B56`).
   - **IOControl `0x2F` ID `0x0348`** = `Theft_Protection` (`STRUC_IOContrKSAuth`, V850 handler `0x000A83FA`).

---

## 4. Verified UDS Services in V850 Firmware (`8W0035465` SW `0420`)

### ReadDataByIdentifier (`0x22`, Dispatcher `0x0009E200`, 85-entry Table at `0x00040B44..0x00040F3F`)
- `0x02F3` → `0x0009ED94` (case `0x02F3`): **Temperature of output stages / amplifier** (`gp-0x368c`..`gp-0x368f`).
- `0x0407` → `0x0009ED94` (case `0x0407`): **Optical data bus (MOST150) wake-up line diagnosis**.
- `0x18EE` → `0x0009ED94` (case `0x18EE`): **Status_speaker_2** (`STRUC_MWVLoudsStatuAMPGen2` — live per-channel DC/AC diagnosis status for Ch 1..16).
- `0x18F6` → `0x0009ED94` (case `0x18F6`): **Vehicle noise compensation (VNC) status** (reads `gp-0x369a`: `0x00` inactive, `0x01` active, `0x0E` error).
- `0x22AD` → `0x0009E6BC` (case `0x22AD`): **Audio adjustment readback** (Bass `gp-0x4566`, Mid `gp-0x4565`, Treble `gp-0x4564`, Balance `gp-0x4563`, Fader `gp-0x4562`, Subwoofer `gp-0x4560`, Surround `gp-0x455f`).

### WriteDataByIdentifier (`0x2E`, Dispatcher `0x0009F6A0`, 10-entry Table at `0x00040F40..0x00040FB7`)
- `0x0600` (14 B), `0xF198` (6 B), `0xF199` (3 B), `0xF15A` (9 B), `0x0512` (4 B), `0x0909` (1 B), `0x090A` (1 B), `0x0982` (73 B), `0x2B00` (1 B), `0x090C` (2 B).

### InputOutputControlByIdentifier (`0x2F`, Dispatcher `0x000A19D0` / `0x000A1FB4`)
Remote audio overrides require first enabling **Remote Control Status (`0x0177`)**:
1. `2F 01 77 03 01` → sets `*(byte *)(gp - 0x4573) = 0xA5` (`ShortTermAdjustment` active; `2F 01 77 00` resets to `0x5A`).
2. Active remote overrides when `gp - 0x4573 == 0xA5`:
   - `0x017B` (`0x000A21D4`): **Remote volume control** (`IDE02182`) — sets `gp-0x4572` (`-100..+26`).
   - `0x017C` (`0x000A2738`): **Remote balance/focus preset** (`IDE02183`) — sets `gp-0x4567` (`0..4`: all/FL/FR/RL/RR).
   - `0x0423` (`0x000A22B0`): **Remote_Set_Mute** (`MAS03807`) — sets bit 0 of `gp-0x4571`.
   - `0x0424` (`0x000A2370`): **Remote_Set_Sound_Settings** (`STRUC_IOContrSetSound`) — 3-byte payload `[Bass, Mid, Treble]` where `0x00 = -9`, `0x01 = 0 (neutral)`, `0x02 = +9` (written to `gp-0x4570..gp-0x456e`).
   - `0x0425` (`0x000A2470`): **Remote_Set_GALA** — sets `gp-0x456d` (`0x00 = OFF`, `1..7` = step).
   - `0x0427` (`0x000A254C`): **Remote_Set_Loudness** — sets `gp-0x456c` (`0x00` → `0x5A` Loudness OFF, `0x01` → `0xA5` Loudness ON).
   - *(Unlinked in `0x000A19D0`)* `0x0426` (`0x000A2600`): **Remote_Set_Sound_Curve** — compiled handler at `0x000A2600` sets `gp-0x4569` / `gp-0x456a` and sets `gp-0x456b = 0x5A` (`Linear` mode) when `param == 0`, but its `uVar6 == 0x0426` case was omitted from `0x000A19D0`.
- Speaker & Hardware IOControls:
   - `0x02EE` (`0x000A1CC8`): **MOST Ring Break Diagnostic** (`IDE03828`).
   - `0x02FB` (`0x000A1DBC`): **Internal Tone Generator (`100..5000 Hz`)** (`IDE03848`).
   - `0x02FC` (`0x000A1EEC`): **External Audio Input / Mute Control**.

### RoutineControl (`0x31`, Dispatcher `0x000A2878`)
- `0x0300` (`0x000A2A9C`): **Speaker AC/DC Impedance Self-Test** (`0x01` start, `0x02` stop, `0x03` results).
- `0x02EF` (`0x000A2BB4`): **MOST / Optical test routine**.
- `0x03E7` (`0x000A2D24`): **Check Memory / Programming Preconditions**.
- `0xA018` (`0x000D57CC`): **Hidden Alpine Engineering / Factory Calibration Routine** (see Section 6).

---

## 5. Sound Dataset (`8W2909510A v0006`) UDS Flash Mapping & Internal Structure

### UDS `0x34 RequestDownload` / `0x35 RequestUpload` / `0x36 TransferData` (`0x000A34D0`, `0x000A3674`, `0x000A3AA8`)
Tables `DAT_ram_00040fb8` (logical start addresses) and `DAT_ram_00040fc8` (lengths) define 4 data transfer blocks:

| Block Index (`gp-0x4774`) | ODIS Dataset Block Name (`STRUC_IDENTVWAppliDataIdentGen2`) | UDS Logical Address (`0x34`/`0x35`) | Length | Physical Target in J525 | Magic & Footer |
|---:|---|---|---:|---|---|
| `0` | `ASAM_ODX_MUX_dataset` | `0x00000200` | `0x00000023` (35 B) | EEPROM `0x01D0` (6 B ver) + `0x01D7` (25 B ID) | CRC16 at end |
| `1` | `audio_parameter_sound_dataset` (**`AD301A07`**) | **`0x000D0000`** | **`0x00010000` (64 KB)** | V850 CodeFlash **`0x00100000..0x0010FFFF`** (`addr + 0x30000`) | Header `AD301A07` @ `0x100000`; Footer `AD301A07 00 00 00 00 31 30 A7 07` @ `0x10FFF0` |
| `2` | `3D_dataset` (**`AD301B01`**) | **`0x000E0000`** | **`0x00008000` (32 KB)** | V850 CodeFlash **`0x00120000..0x00127FFF`** (`addr + 0x40000`) | Header `AD301B01` @ `0x120000`; Footer `AD301B01 00 00 00 00 31 30 E9 2B` @ `0x127FF0` |
| `3` | Factory Calibration Readback (`0x35` only) | `0x00F00000` | `0x0000002E` (46 B) | EEPROM `0x021F` | — |

### Dataset Checksum Algorithm (`FUN_ram_000c4516` at `0x000C4516`)
- **Algorithm**: Standard **CRC-16/CCITT-FALSE** (`poly = 0x1021`, `init = 0xFFFF`, `refIn = false`, `refOut = false`, `xorOut = 0x0000`).
- Computed over the entire dataset minus the final 2 bytes (`0xFFFE` bytes for `AD301A07` → `0x07A7`, stored little-endian as `A7 07` at `0x10FFFE..0x10FFFF` in Flash; `0x7FFE` bytes for `AD301B01` → `0x2BE9`, stored as `E9 2B` at `0x127FFE..0x127FFF`).
- On valid `0x36 TransferData` completion, `FUN_ram_000a3674` copies the 2-byte ASCII sub-version at `offset - 4` (`"10"` = `0x31 0x30`) into EEPROM `0x014F` (visible in DID `0xF182` / `0xF1AF` as `--1010`).

```python
import struct

def crc16_ccitt_false(data: bytes, init: int = 0xFFFF) -> int:
    crc = init
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if (crc & 0x8000) != 0:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

# Verification against CodeFlash_24082026_172027.bin:
# AD301A07 (0x100000..0x10FFFE): crc16_ccitt_false(flash[0x100000:0x10FFFE]) == 0x07A7 (bytes: A7 07)
# AD301B01 (0x120000..0x127FFE): crc16_ccitt_false(flash[0x120000:0x127FFE]) == 0x2BE9 (bytes: E9 2B)
```

### Internal Memory Map of `AD301A07` (`0x00100000..0x0010FFFF`, 64 KB)
1. **`0x100000..0x100007` (8 B)**: Magic `"AD301A07"`.
2. **`0x100008..0x100033` (44 B)**: **Sound Profile Index Map** (`3 derivatives × 0x15` bytes, read in `SelectA07ProfileAndMatrix` at `0x000C23E0`). Maps `[gp-0x4592][gp-0x456a]` to sub-profile `0..7` (or `0xFF = -1` to use the internal ROM linear tables at `0x0008DF48` and `0x0008F33C`):
   - Row 0 (`0x100008..0x10001C`, `gp-0x4592 == 0`): `FF 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`
     - `index 0 = 0xFF` (`-1`, uses internal ROM Linear fallback!)
     - `index 1 = 0x00` (Sub-profile 0)
     - `index 2 = 0x01` (Sub-profile 1)
   - Row 1 (`0x10001D..0x100031`, `gp-0x4592 == 1`): `FF 02 03 00 00 ...` (`index 0 = 0xFF`, `index 1 = 0x02`, `index 2 = 0x03`).
3. **`0x100034..0x101BB3` (`8 × 0x370` = 7,040 B)**: **8 Acoustic Sub-Profiles** (copied to RAM `gp-0x13900`).
4. **`0x101BB4..0x1021B3` (`8 × 0xC0` = 1,536 B)**: **8 Input Mixing Matrices** (`3 × 8×8` byte matrices per profile, copied to RAM `gp-0x3e20` and sent to SHARC `0xC022A` by `WriteSharcInputMatrix8x8` at `0x000A90C0`).
5. **`0x1021B4..0x102437` (`0x284` = 644 B)**: **Global DSP Config Block** (copied to RAM `gp-0x13590` by `0x000C38D0`; ROM fallback at `0x0008E2B8`). Contains the `gp-0x135f8` per-profile 20-channel gain offsets sent to SHARC `0xE0584`.
6. **`0x102438..0x103237` (`0xE00` = 3,584 B)**: **Channel EQ / Biquad / Delay / Limiter Table** (copied to RAM `gp-0x1330c` on boot by `0x000B64E0`; ROM fallback at `0x0008E53C`). Contains the `gp-0x1284c` biquad banks sent to SHARC `0xE01B4` by `0x000AC9C4`.
7. **`0x103238..0x10FFEF` (`3 × 0x50A4` = 61,932 B)**: **3 Car-Derivative Sound Blocks** (indexed by `*(byte *)(gp-0x4592) * 0x50A4`):
   - `+0x0000..+0x00A3` (164 B): **VNC / GALA speed-dependent calibration parameters** (loaded by `0x000AAD70`).
   - `+0x00A4..+0x50A3` (`0x5000` = 20,480 B = `10 × 0x200` 32-bit floats/words): **Bulk SHARC Acoustic Parameter Table** streamed directly to SHARC RAM **`0xC0A7E .. 0xC1E7E`** in 64-word SPI packets by `FUN_ram_000aaa60` (`0x000AAA60`).
8. **`0x10FFF0..0x10FFFF` (16 B)**: Footer `"AD301A07\x00\x00\x00\x0010\xA7\x07"`.

---

## 6. How `Linear` Mode (`gp-0x456b`) and Hidden Routine `0xA018` Work

### What `Linear` Mode (`gp-0x456b = 0x5A`) Does in the DSP
In `FUN_ram_000aa648` (`0x000AA648`) and `FUN_ram_000ac9c4` (`0x000AC9C4`):
- When `*(byte *)(gp - 0x456b) == 0xA5` (**Sound Curve ON**) and `*(byte *)(gp - 0x455c) == 0xA5`:
  - Profile index `uVar4 = *(char *)(gp - 0x418f) + 1` (`1..N`) is selected.
  - `0x000AA648` loads the profile's 20 channel gain offsets from `gp - 0x135f8 + uVar4 * 0x14` (plus dynamic offset `gp - 0x12aba`) and writes them to SHARC **`0xE0584`**.
  - `0x000AC9C4` loads the profile's 16 acoustic voicing biquads (`0x80` bytes) from `gp - 0x1284c + uVar1 * 0x80` and writes them to SHARC **`0xE01B4`**.
- When `*(byte *)(gp - 0x456b) == 0x5A` (**Linear Mode / Sound Curve OFF**):
  - `uVar4 = 0` and `uVar1 = 0` are forced.
  - SHARC **`0xE0584`** receives the flat `0 dB` base table (`gp - 0x135f8 + 0`), dynamic offset `gp - 0x12aba` is bypassed, and SHARC **`0xE01B4`** receives the flat bypass biquad bank (`gp - 0x1284c + 0`), while keeping speaker crossovers and delays intact.

### 3 Ways to Trigger `Linear` Mode (`gp-0x456b = 0x5A`) & Live UDS Commands
1. **Via Standard UDS `0x2F` Remote Control (`0x000A19D0`)**:
   - Enable Remote Control Status: `2F 01 77 03 01` (`gp - 0x4573 = 0xA5`)
   - Turn Loudness OFF: `2F 04 27 03 00` (`gp - 0x456c = 0x5A`)
   - Turn GALA OFF: `2F 04 25 03 00` (`gp - 0x456d = 0x00`)
   - Set Neutral Bass/Mid/Treble (`0`): `2F 04 24 03 01 01 01` (`gp - 0x4570..0x456e = 1`)
   - Return Control to ECU: `2F 01 77 00` (`gp - 0x4573 = 0x5A`)
2. **Via Hidden Alpine UDS Routine `0xA018` (`0x000D57CC` → `0x000D3A38`)**:
   - Handler `FUN_ram_000d3a38` (`0x000D3A38`, dispatched via `31 01 A0 18 09 02 <profile_idx> <20_gain_bytes>`) directly controls `gp - 0x4568`, `gp - 0x456b`, `gp - 0x418f`, and the live 20-channel gain table at `gp - 0x135f8`:
     - If `profile_idx == 0x00`: sets `gp - 0x4568 = 0x5A` and **`gp - 0x456b = 0x5A` (`Linear` mode)**, overwrites the 20-byte gain table `gp - 0x135f8 + 0`, and triggers `0x000C3990(0x16)` to update SHARC `0xE0584` and `0xE01B4` immediately.
     - If `profile_idx >= 0x01`: sets `gp - 0x456b = 0xA5` (`Sound_Curve` mode), sets `gp - 0x418f = profile_idx - 1`, overwrites `gp - 0x135f8 + profile_idx * 0x14` with the 20 supplied gain bytes, and updates SHARC in real time.
3. **Via Dataset `AD301A07` Modification (Permanent)**:
   - Setting the active derivative's profile map entry at `0x100008..0x100033` to `0xFF` (`-1`) or zeroing the voicing biquad/loudness offsets in `AD301A07` (`0x000D0000`), recalculating CRC-16/CCITT-FALSE (`0x000C4516`) at `0x10FFFE`, and flashing via UDS `0x34`/`0x36`.

---

## 7. How MIB2 SWUP Updates the J525 Amplifier over MOST150 (`FBlock 0xC2` / `AMP16bAPN`)

### 7.1 SWUP Files in `MHI2Q_US_AUG22_P5087_MU1316` (`[AMP16bAPN]`)
In `<AUDI_WORKSPACE>/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/MHI2Q_US_AUG22_P5087_MU1316/AMP16bAPN/AMP16bAPPL/11/default/` (linked for both HW `11` and HW `10` in `metainfo2.txt`):

| SWUP File | Size | Target V850 Flash Address | Version Header | Footer (`offset - 16`) | Verification |
|---|---:|---|---|---|---|
| `PA00003_1420_Pbl.bin` | `131,072` B (`0x20000`) | `0x00020000..0x0003FFFF` (shadow BL) → copied to `0x00000000..0x0001FFFF` (primary BL) | `a4 01 00 00` (`420`) @ `+0x07C0` | `AD301FLASH0\0` + `61 16` + `FF FF` @ `+0x1FFF0` | `CRC-16/CCITT-FALSE` over `[0..0x1FFFB]` = `0x6116`; `metainfo2.txt` SHA-1 = `09577d29b394332a4b4ea7003a897382754faa49` |
| `PA00003_1420_Apl.bin` | `786,432` B (`0xC0000`) | **`0x00040000..0x000FFFFF`** (V850 App + embedded SHARC loader at `+0x001014..+0x028777`) | `a4 01 00 00` (`420`) @ `+0x0000` | `AD301FLASH0\0` + `E9 9E` + `FF FF` @ `+0xBFFF0` (`0x0FFFF0`) | `CRC-16/CCITT-FALSE` over `[0..0xBFFFB]` = `0xE99E`; `metainfo2.txt` `CheckSum` (first 512 KB SHA-1) = `1d069f58...`, `CheckSum1` (last 256 KB SHA-1) = `fee293a8...` |

- Both `PA00003_1420_Pbl.bin` and `PA00003_1420_Apl.bin` match `CodeFlash_24082026_172027.bin` (`0x00000..0x3FFFF` and `0x40000..0xFFFFF`) **100% bit-for-bit**.
- **No cryptographic signature (RSA/ECDSA) exists inside the J525 bootloader or application** — the amplifier only checks the 12-byte magic `"AD301FLASH0\0"` and the 2-byte big-endian `CRC-16/CCITT-FALSE` (`poly=0x1021, init=0xFFFF`) at `offset - 4`!

### 7.2 MOST150 `FBlock 0xC2` (SWDL) Reprogramming Protocol (`0x000011B0` & `0x000D6224`)
The V850 implements the standard Audi MOST SWDL (`FBlock 0xC2`) state machine in both the Application (`0x000D6224..0x000D668E`) and the Bootloader (`0x00008F22..0x00009E24`, dispatch table at `0x000011B0`):

1. **`FktID 0xC04` (`ReprogrammingMode`)**:
   - MIB2 sends `"AMP16bAPN"` (or `"BROADCAST"`), device ID `0x0547` (ECU `0x47`) / `0x0FFF`, and mode byte:
     - `mode = 0x00` (Query): J525 responds with **`FktID 0xC02`** (`FUN_ram_000d6454` in App / `FUN_ram_00009a1a` in BL), reporting device `"AMP16bAPN"`, module `"AMP16bAPPL"`, HW version (`11` for `H11` parsed from `gp+0x8d5`), bootloader version (`*(u32*)0x000007C0 = 420`), and application version (`*(u32*)0x00040000 = 420`).
     - `mode = 0x01` (Enter Bootloader): App handler `FUN_ram_000d6224` (`0x000D6224`) writes `08 A5 A5` to EEPROM block `0xAA` offset `0` (`0x000DD75A`) and triggers a hardware reset into the V850 Bootloader (`0x0009B29A`).
2. **`FktID 0xC02` (`ModuleInfo` in Bootloader, `FUN_ram_00009e24` at `0x00009E24`)**:
   - MIB2 sends module name `"AMP16bAPPL"` and two 32-bit flags indicating whether to flash `Pbl` (`gp-0x13e8 = 1`) and/or `Apl` (`gp-0x13e8 = 2`).
   - Bootloader replies with **`FktID 0xC21`** (`FUN_ram_0000a652` at `0x0000A652`) requesting module preparation.
3. **`FktID 0xC21` (`PrepareModule` / Flash Erase, `FUN_ram_00009732` at `0x00009732`)**:
   - MIB2 sends image length (`<= 0xC0000`).
   - For `Pbl` (`1`): sets write base `0x00020000` (sectors `0x20..0x3F`).
   - For `Apl` (`2`): sets write base `0x00040000` (sectors `0x40..0xFF`), marks app invalid (`gp-0x13ed = 0x5A` in EEPROM), calls RAM self-programming routine `func_0xffff028a()` to erase the Flash sectors, and increments the flash counter in EEPROM `0x01F3` (`499`).
4. **`FktID 0xC24` (`TransferData`, `FUN_ram_00009238` at `0x00009238` → `FUN_ram_000090c6`)**:
   - MIB2 streams up to `0x1000` (4,096) bytes per packet (`[seq_hi, seq_lo, offset_u32be, payload...]`).
   - Bootloader writes each chunk to Flash via `func_0xffff020a()` and updates running `CRC-16/CCITT-FALSE` (`gp-0x13fa`) over all bytes except the final 4 bytes (`param_1 - 4`), storing the final big-endian CRC16 in `gp-0x13f0`.
5. **`FktID 0xC22` (`VerifyModule`, `FUN_ram_0000947a` at `0x0000947A`)**:
   - Verifies that the 12-byte magic at `0x03FFF0` (`Pbl`) or `0x0FFFF0` (`Apl`) matches `"AD301FLASH0\0"` (`0x01FFF0`) and that the 2-byte CRC16 at `0x03FFFC` (`Pbl`) or `0x0FFFFC` (`Apl`) matches `gp-0x13f0`.
   - If valid: sets `gp-0x13ec = 0x55` (`Pbl`, which triggers `func_0xffff011c()` on exit to mirror `0x020000..0x03FFFF` into `0x000000..0x01FFFF`) or sets `gp-0x13ed = 0xAA` (`Apl` valid) and reboots into the new application when `0xC04` sets `mode = 0x00`.

### 7.3 Other Amplifiers in MIB2 SWUP (`AMP16qAPN`, `Bose6416A`, `AMP_BAO_P3`)
1. **`AMP16qAPN` (`AD602_1420_Pbl.bin` + `AD602_1420_Apl.bin`, Q5 FY / Q7 4M `80A035223A`) vs `AMP16bAPN` (`8W0035465`)**:
   - Out of `786,432` bytes in `Apl.bin`, **only 43 bytes differ** (`19` bytes in `Pbl.bin`):
     - **0 diff bytes** in the embedded SHARC DSP loader (`+0x001014..+0x028777` is 100% bit-for-bit identical).
     - **35 bytes** at `0x094A8B..0x096FB8`: ASCII strings (`8W0035465  ` ↔ `80A035223A `, `H10PA00003A` ↔ `H03PMAD602A`, `MIB2_amp_P_B9J525` ↔ `MIB2_amp_P_Q5J525`, `AD301A07`/`AD301B01` ↔ `AD602A07`/`AD602B01`, `AMP16bAPN` ↔ `AMP16qAPN`).
     - **4 bytes** at `0x0C3F36..0x0C3FA0` (`FUN_ram_000c3ec8` at `0x000C3EC8`): EEPROM offset (`0x039E` on B9 vs `0x046D` on Q5) where the post-reflash flag is cleared.
     - **4 bytes** at `0x0FFFF2..0x0FFFFD`: Footer `"AD301FLASH0\0"` (`CRC16 = 0xE99E`) vs `"AD602FLASH0\0"` (`CRC16 = 0xC296`).
   - Thus, the B9 (`8W0035465`) and Q5 (`80A035223`) 16-channel Alpine amplifiers run the **exact same V850 & SHARC code**, differing only in their external 64 KB (`A07`) and 32 KB (`B01`) acoustic datasets.
2. **`Bose6416A` (`6416A_106200_MIB2_*.bin`, SW `0440`, `EV_AMPMst16C4Gen2BOSE`, IDs `0x160` / `0x161`)**:
   - Used on Q7 4M (`4M0035223`, `0x160` Bose 3D `AU_PREM_2`), A6/A7 C7 (`4G5035223E`/`4G8035223E`, `0x161` Bose Standard `AU_STD_1`), and Porsche Panamera (`971035223A`).
   - Stored as binary Intel-HEX records (`0x3A <len> <addr_be16> <type> <data> <csum>`):
     - `BTLDR.bin` (`12,392` B @ `0x80000000`) & `MAIN.bin` (`386,216` B @ `0x80008000`): **Atmel AVR32 (`AT32UC3`)** 32-bit big-endian MCU running OSEK OS, managing DTCP encryption, MOST150 INIC, and TI `TAS5424` / ST `TDA7569` power stages.
     - `DSP.bin` (`240,092` B @ `0x00008000`): Bit-reversed Analog Devices SHARC DSP `.ldr` image (`PM 0x0008C000..0x00093CFE`, `DM 0x000B2000..0x000E7BAA`, 52-node audio processing graph at `DM[0x000C7AE0..0x000C7ECF]`).
     - `FDB.bin` (`23,624` B @ `0x80078000`): 58-entry Function/Filter Database (`0x80078044..0x800781FC`) containing fallback SHARC DSP parameter presets (`flat_surround` FDB[55] @ `0x8007B142`, `flat` FDB[57] @ `0x8007C53C`, `bandpass_mech`, `bandpass_20db`).
     - `VDB.bin` (`328` B @ `0x8007E000`) & `INIC_FW.bin` (`125,096` B @ `0x80060000`): Version database (`Joker override`) and MOST150 INIC firmware.
3. **`AMP_BAO_P3` (`1285_S.lump` & `1429_HTDAPE.lump`, SW `0594`, `EV_AMPMst24C1Gen2BAO`, ID `0x181`)**:
   - 23-channel Bang & Olufsen Advanced (`8RF`) ICEpower amplifier with motorized acoustic lenses (`4M0035466` / `4G0035466`).
   - Compressed with B&O's proprietary `04.08` bitstream LZSS container (24-byte header `>IHBBIIII`; LSB-first bitstream at `+0x18`: `0` + 8-bit MSB literal [9 bits] or `1` + 4-bit length `+3` + 14-bit offset `src = out - off - len` [19 bits]):
     - `Block0_010000` (`1285_S.lump`, `80,748` B @ `0x00010000`): Starter / Bootloader (`SWU 1285`).
     - `Block1_040000` (nested `04.08` lump in `1429_HTDAPE.lump`, `960,548` B): **Freescale/NXP PowerPC Book-E VLE (`MPC56xx`)** main MCU firmware (`VMA 0x00140000..0x001F7040`, `.data`/`.rodata` copied to SRAM `0x40000CF0`, `_SDA2_BASE_ r2 = 0x4002A7F0`, `_SDA_BASE_ r13 = 0x40028DD0`) + bit-reversed SHARC DSP `.ldr` image (`VMA 0x00100000..0x00134AA0`, build `0587`).
     - `Block2_0c0000` (`45,456` B @ `0x000C0000`, build `0585`): Acoustic tuning dataset (`aaaa` tags: `0x0003` Complete Tuning @ `0x0C0100`, `0x0004` `DSPCL_PARAMS_STATIC` breakpoint tables @ `0x0C55A4`, `0x0002` 150-module SHARC Graph Topology @ `0x0C62CC`, `0x0005` Variation Subtags `cccc` @ `0x0C8354`).
     - `APWB` (`65,536` B @ `0xFFFD0000`, build `0532`): Renesas power-board MCU firmware.

---

## 8. Cross-Amplifier Comparison (`0x120..0x122` Internal TI C674x, `0x140` Alpine B&O, `0x160/0x161` Bose, `0x181` B&O Advanced)

Полный побайтовый и поадресный разбор прошивок всех семейств усилителей Audi MIB2 / MLB Evo:

- **Internal TI C674x DSP в RCC (`0x120` Basic 2ch, `0x121` Basic Plus 4ch, `0x122` Standard 6ch / Audi Sound System)**:
  - Образ DSP: `dsp_p5092.elf` (`AUDI_MIB_DSP.bin.bgz` @ NOR `0x3D00000`).
  - Тракт RCC: `AudioProcess`, `dev-ioamp-router` и `MIB_AudioTable_AU_G22.xml.dec`.
- **External Bose MOST150 (`0x160` Bose 3D `AU_PREM_2`, `0x161` Bose Standard `AU_STD_1`)**:
  - Образы из `Bose6416A`: MCU **Atmel AVR32 (`AT32UC3`)** `BOSE_TRICORE_80000000.bin` (`0x80000000..0x80080000`, включая базу фильтров `FDB` @ `0x80078000`) + bit-reversed образ **SHARC DSP** `DSP_bitrev.ldr` (`PM 0x0008C000..0x00093CFE`, `DM 0x000B2000..0x000E7BAA`, граф из 52 узлов @ `DM[0x000C7AE0..0x000C7ECF]`).
- **External B&O Advanced MOST150 (`0x181` `AU_ADV_1` 23ch ICEpower с выезжающими линзами)**:
  - Образы из `AMP_BAO_P3`, распакованные из контейнеров `04.08` bitstream-LZSS: MCU **Freescale/NXP PowerPC Book-E VLE (`MPC56xx`)** `mpc56xx_vma_100000.bin` (`VMA 0x00140000..0x001F7040`, `.data`/`.rodata` копируется в SRAM `0x40000CF0`, `_SDA2_BASE_ r2 = 0x4002A7F0`, `_SDA_BASE_ r13 = 0x40028DD0`) + **SHARC DSP** `bao_sharc.ldr` (`VMA 0x00100000..0x00134AA0`) + акустический датасет 45 КБ `Block2_0c0000.bin` (`VMA 0x000C0000..0x000CB190`).
- **Bentley Alpine (`0x141` `BY_PREM_1`, `0x151` `BY_STD_1`) и Bentley Naim/B&O (`0x182` `BY_ADV_1`, `0x183` `BY_ADV_2`)**:
  - Описаны в `MIB_AudioTable_BY_G22.xml.dec`; `0x141`/`0x151` используют кодовую базу Alpine V850+SHARC (`AMP16x`), а `0x182`/`0x183` — расширенный протокол `CLD22` с 5-полосным `GraphEqualizer` (`0x0462`/`0x0463`) и 8 пресетами эквалайзера (`PRESETEQ = 255`).

---

### 8.0 Предварительное уточнение по FktID MOST `FBlock 0x22` (`CLD22` в `AudioProcess` @ `0x00194AE4`)

| Parameter | MOST `FktID` | `AudioProcess` Handler | Alpine B&O (`0x140`) | Bose (`0x160`/`0x161`) | B&O Advanced (`0x181`) | Internal DSP (`0x120..0x122`) |
|---|---|---|---|---|---|---|
| **DTCP SRM** | `0x0121` | — | `0x000CF320` (`0x095A94`) | `0x8005DFBC` | `0x00194870` (`0x001C74B0`) | N/A |
| **`InputGainOffset`** (`-13..+13 dB`) | **`0x0450`** | `0x0018FA24` | `0x000C786C` (`0x095AD4`) | `0x8005E00C` → `0x80055E70` | `0x00197DD0` (`0x001C74F8`) | `0x2206` bit `0x80` (`0x87576360`) / `0x2204` (`0x875B2CC0`) |
| **`GraphEqualizerOnOff`** | **`0x0462`** | `0x0018FCD8` | `0x000C7E06` (`0x095ADC`) | **Absent** | **Absent** | N/A |
| **`GraphEqualizer`** (5-band) | **`0x0463`** | `0x0018FD54` | `0x000C7F10` (`0x095AE4`) | **Absent** | **Absent** | `0x2206` bits `0x01..0x10` (`0x87576360`) |
| **`MidRange`** (`-9..+9`) | **`0x0D26`** / **`0x0465`** | `0x0018FAC0` | `0x000C8548` (`0x095B8C`, stub) | **LIVE** at `0x8005E01C` (`0x0465`) & `0x8005E09C` (`0x0D26`) → `0x80056358` | **Absent** in `FB22` (`0x0015F9D0` dead in `DSPCL`) | **LIVE** (`0x2206` bit `0x04` → Band 2 of `zone0_uEQParams` @ `0x876012DC`) |
| **`EqualizerSetup`** | **`0x0D27`** | `0x0019220C` | `0x000CA11C` (`0x095B94`) | **Absent** | `0x0019A390` (`Get` only) | N/A |
| **`PresetSettings`** | **`0x0D28`** | `0x00190220` | `0x000CA4B8` (`0x095B9C`) | `0x8005E08C` → `0x800557D8` | `0x0019A520` (`0x001C7580`) | `PresetPos` only (`0x220E`/`0x220C`); `0x2220` (`0x875D0040`) dead |
| **`NoiseCompensation`** | **`0x0D29`** | `0x00190140` | `0x000CA96C` (`0x095BA4`) | `0x8005E0A4` → `0x800549F4` → **`0x8000BF4C`** (`0x046B`) | `0x0019AED0` (`0x001C7588`) | `0x2215` (`Amp_Gala_Set` @ `0x875D0680`) |

По таблице диспетчеризации `CLD22` в `AudioProcess` (`0x00194AE4`) и таблицам `FBlock 0x22` всех трёх внешних усилителей (`8W0035465_0420_V850_full.bin` @ `0x095A24..0x095BAC`, Bose @ `0x8005DF44..0x8005E0B8`, B&O Adv @ `0x001C7440..0x001C7590`):
- **`0x0121`** — это **DTCP SRM Key Management** (`0x000CF320` в Alpine, `0x8005DFBC` в Bose, `0x00194870` в B&O Adv).
- **`InputGainOffset` (`-13..+13 dB`)** — это **`FktID 0x0450`** (`sub_18FA24` @ `0x0018FA24` в `AudioProcess`; `0x000C786C` в Alpine; `0x8005E00C` в Bose; `0x00197DD0` в B&O Adv).
- **`MidRange` (`-9..+9`)** — это **`FktID 0x0D26`** (`sub_18FAC0` @ `0x0018FAC0` в `AudioProcess`; `0x000C8548` в Alpine; `0x8005E09C` и дубль `0x0465` @ `0x8005E01C` в Bose). А `0x0CFE` (`0x000C8362` в Alpine) — это read-only 7-байтный дескриптор возможностей (`0x096F84`).
- **`PresetSettings` (`FktID 0x0D28`)**: в `AudioProcess` (`sub_190220` @ `0x00190220`) при одиночной отправке `PosX` имеет фиксированную раскладку:
  - `PosX = 0x01` → `PresetEQ`
  - `PosX = 0x02` → `PresetPos` (`SoundFocus`: `1=All, 2=Front, 3=Rear, 5=Movie`)
  - `PosX = 0x03` → `SurroundOnOff` (`0=Off, 1=On`)
  - `PosX = 0x04` → `3DMode` (в Java `DSISound`: `1=Off, 2=Low, 3=Med, 4=High` → `AudioProcess` вычитает `1` и шлёт по MOST `0=Off, 1=Low, 2=Med, 3=High`)
  - `PosX = 0x05` → `SurroundLevel` (`-9..+9`)
  - `PosX = 0x06` → `SourceQuality`

---

### 8.1 Вопрос 1: Поведение `DSISound.setSurroundOnOff(conn, term, false)` / MOST `FBlock 0x22` `FktID 0x0D28` (`PresetSettings`)

#### 1) Bose MOST150 (`0x160` Bose 3D `Var=2` и `0x161` Bose Standard `Var=0`)
В `BOSE_TRICORE_80000000.bin` запись в `FktID 0x0D28` (`0x8005E08C`) вызывает `0x800557D8` → `0x80015110`, где вскрывается **критическое отличие Bose от Alpine**:
- **Несовпадение `PosX` в таблице `0x80065FC0`**:
  Функция `0x80015110` индексирует массив `0x80065FC0` (`[6, 5, 3, 7, 4, 8]`) по `PosX - 1`:
  - `PosX = 0x01` → `id = 6` (`BAC[0x11] = "EHCMde"` / `PresetEQ`, `0..15`, валидатор `0x800153A4`)
  - `PosX = 0x02` → `id = 5` (`BAC[0x12] = "SoundFocus"`, `1=Sym/All, 2=Front, 3=Rear`, валидатор `0x80015340`)
  - **`PosX = 0x03` → `id = 3` (`BAC[0x0E] = "SurrVol"`, диапазон `-9..+9 dB`, валидатор `0x80015238`, запись в SHARC `0x800368A8`)!**
  - **`PosX = 0x04` → `id = 7` (`3D / Surround Mode`, диапазон `0..1` для `0x160` `Var=2` и жёстко `0` для `0x161` `Var=0` в `0x80038E04`, валидатор `0x80015404`)!**
  - `PosX = 0x05` → `id = 4` (неиспользуемый слот `0..15`, валидатор `0x800152E0`)
  - `PosX = 0x06` → `id = 8` (`BAC[0x15] = "Surr Out"`, `0..3`, валидатор `0x80015460`)
- **Что происходит при вызове `setSurroundOnOff(conn, term, false)` (`PosX = 0x03, val = 0`) на Bose**:
  Поскольку на Bose `PosX = 0x03` привязан к `id = 3` (`SurrVol`), передача `0` воспринимается усилителем как **`SurrVol = 0 dB` (номинальный уровень Centerpoint, `10^(0/20) = 1.0f`)**, который через `0x800368A8` → SHARC `Entry[10] @ DM[0x000B2BE0]` (`0x0008CC24`) пишет `1.0f` в `DM[0x000B2ABF]`. То есть `setSurroundOnOff(false)` **НЕ выключает Surround на Bose**, а ставит уровень Centerpoint в `0 dB`! (А если послать `SurrVol = -9` через `PosX = 0x03, val = 0xF7`, то `DM[0x000B2ABF]` станет лишь `10^(-9/20) = 0.3548f`, а не `0.0f`).
- **Как на самом деле выключается Bose Centerpoint / SurroundStage / 3D в чистый passthrough**:
  В `0x80014EFC` (`0x80014F60..0x800150DC`) MCU вычисляет ключ акустического пресета `BAC[0x13] = "DSPMde"`:
  $$r_8 = \text{base}(\text{SoundFocus}) + \text{Mode3D}$$
  где `base(SoundFocus)` из таблицы `0x8006189C` равен `10` (`Focus=1 All`), `20` (`Focus=2 Front`) или `30` (`Focus=3 Rear`), а `Mode3D` (`PosX = 0x04`, `id = 7` @ `0x000018E6`) добавляет `0` или `1` (`0x80014F76`). Итоговые ключи `10, 11, 20, 21, 30, 31` (`0x0A, 0x0B, 0x14, 0x15, 0x1E, 0x1F`) ищутся в таблице `0x80078588` (`FDB[41]`) / EEPROM `0x8003`.
  Сравнение fallback-пресетов `FDB[55]` (`flat_surround` @ `0x8007B142`, когда `PosX=4` равен `1`) и `FDB[57]` (`flat` @ `0x8007C53C`, когда `PosX=4` равен `0`) показывает, что из 52 записей SHARC DM **51 запись совпадает побайтово**, а меняется только **Запись 11 (`DM[0x000B90A4]`, 17 слов маршрутизации для `Node[17] @ 0x000C7BF9`, `proc = 0x00092652`)**:
  - `PosX = 0x04, val = 1` (`flat_surround`): `[0, 1, 2, 5, 3, 4, 0, 1, 3, 4, 2, 5, 0, 0, 0, 0, 5]` — подаёт сгенерированные блоком **Bose Centerpoint (`Node[10..12] @ 0x000C7B87..0x000C7BA0`, `proc = 0x00090F9E`)** каналы Center (`2`), Sub (`5`) и Surround (`3, 4`) на выходные шины.
  - `PosX = 0x04, val = 0` (`flat`): `[0, 1, 13, 13, 0, 1, 0, 1, 0, 1, 13, 13, 0, 0, 0, 0, 13]` — **переводит `Node[17]` в чистый L/R стерео passthrough (`0, 1`), а все слоты Center/Sub/3D глушит на нулевую шину `13` (`zero bus`)**! При этом канальные кроссоверы и войсинг динамиков (`Node[25..26] @ 0x000C7CAA`, `0x000E6BF2`) остаются активными.
  - **Вывод для Bose (`0x160`/`0x161`)**: Для полного обхода Centerpoint/3D на Bose нужно вызывать **`DSISound.set3DMode(conn, term, 1)`** (что шлёт `PosX = 0x04, val = 0` → `flat`), а **не** `setSurroundOnOff(false)` (или вызывать оба, но на `0x160`/`0x161` не трогать `SurrVol`, если не нужно сбросить его в `0 dB`).

#### 2) B&O Advanced MOST150 (`0x181` `AU_ADV_1`, `AMP_BAO_P3`)
В `mpc56xx_vma_100000.bin` обработчик `FB22_PresetSettings_S` (`0x0019A520`, запись в таблице `0x001C7580` → `0x001C73C0`) разделяет `SurroundOnOff` (`PosX = 0x03`) и `3DMode` (`PosX = 0x04`) на **два независимых каскада** в SHARC DSP (топология в `Block2_0c0000.bin` @ `0x000C630C`):
- **`PosX = 0x03` (`SurroundOnOff` @ `0x0019A7FC` → `0x00193A00` cmd `0x9805` → `DSPCL` `0x0015FD90` `'Surround on/off: %i'`)**:
  - При `SurroundOnOff = 0` записывает `0.0f` (`0x001D72D0`) в `*(0x4002C410 + 0x1B4)`.
  - При пересчёте гейнов в `0x00161F10` (`efsmul r9, r9, r8` на `0x00161F2A`) множитель `Surround_S_gain` (`0x4002BE38 + 0xB8`, отправляемый в SHARC через `0x0015E180` `'Surround S gain updated: %f'` в модуль **`mod[50]` `label = 0x0110` @ `0x000C6E18`**) становится равным **`0.0f`**.
  - Это обнуляет разностный канал `0x87 -> 0x88` (`S = 0.5*L - 0.5*R`, сформированный в `mod[48]` @ `0x000C6DBC`) 2D-апмиксера Surround. **Но 3D-апмиксер Symphoria (`mod[63]`) при этом остаётся включённым!**
- **`PosX = 0x04` (`3DMode` @ `0x0019A820` → `0x00193A40` `tuldr_3Dintensity_cmd` + `0x00193AB0` cmd `0x9905` → `DSPCL` `0x0015FE50` `'3D Intensity %i'`)**:
  - `tuldr_3Dintensity_cmd` (`0x00193A40`) переводит `3DMode` (`0=Off, 1=Low, 2=Med, 3=High`) в `Soundset = 1..4` и шлёт `TM_FID_USER_SOUNDSET` (`0x001A2D84`) в `TuningMgr` (`0x001A2BE0`), вызывая `SendVariationTuning` (`0x001A35F0`) для **Variation Subtag 2 (`0x000C8414`)**:
    - При **`3DMode = 0` (`Off`, `Soundset = 1`, `Group 0 mask = 0x00000001` @ `0x000C841C`)**:
      - В микшер **`mod[65]` (`label = 0x0FF3` @ `0x000C7180`)**, который смешивает 6 прямых шин `[0x96, 0xAC, 0xA8, 0xB0, 0xAA, 0xB2]` с 6 выходами **3D-апмиксера Symphoria (`mod[63]`, `type = 22` @ `0x000C70E0`, буферы `[0xB4..0xBE]`)**, загружаются веса `[1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0]` — **все 6 каналов 3D-апмиксера глушатся в `0.0`**!
      - В микшер **`mod[66]` (`0x000C71DC`)** загружается `[0.0, 1.0, 0.0, 1.0]` (отключая верхние 3D-шины `0xC0, 0xC2`), а фильтр `mod[74]` (`label = 0x0FF7`) дополнительно гасит `0xC0` на `-35.0 dB`.
    - При **`3DMode = 1..3` (`Low/Med/High`, `Soundset = 2..4`, `Group 1 mask = 0xFFFFFFFE` @ `0x000C84F4`)**: `mod[65]` получает `[1.0, 1.0, ...]`, а `mod[66]` — `[1.0, 0.0, 1.0, 0.0]`, включая 3D-тракт.
  - Одновременно в `DSPCL` `0x0015FE50` при `3DMode = 0` переключает `0x00160AA0` на 2D-таблицы окружения (`+0x0C24/+0x0C78/+0x0CCC`), а при `3DMode != 0` — на 3D-таблицы (`+0x0D20/+0x0D74/+0x0DC8`).
  - При этом 12-секционные биквады акустического войсинга и кроссоверов динамиков (`mod[84..103]`, `type = 29` @ `0x000C770C..0x000C79B4`) на `0x181` **не отключаются никогда** (в отличие от `Row 0` на младшем Alpine `0x140`).
- **Вывод для B&O Advanced (`0x181`)**: Чтобы полностью отключить и 3D Symphoria (`mod[63]/[65]/[66]`), и 2D Surround (`mod[50]`), на `0x181` необходимо отправить **оба** вызова: `DSISound.set3DMode(conn, term, 1)` (`PosX=4 -> 0`) **и** `DSISound.setSurroundOnOff(conn, term, false)` (`PosX=3 -> 0`).

#### 3) Internal TI C674x DSP (`0x120 / 0x121 / 0x122`)
На внутреннем DSP в `AudioProcess` и `dsp_p5092.elf` вызовы `setSurroundOnOff` и `set3DMode` **заблокированы сразу на 3 уровнях**:
1. В `AudioProcess` (`sub_14D91C` @ `0x0014D91C`) при `area == 51` (Internal Amp) изменение `SurroundOn`, `SurroundLevel`, `Mode3D` или `SourceQuality` попадает в ветку `0x0014DD14`, которая логирует `" one of these SurroundOn SurroundLevel Mode3D SourceQuality changed"` и ставит `v46 = 0` (полный no-op). Только `PresetPos` (`SoundFocus`) вызывает `vtable[111]` (`sub_1A264C` @ `0x001A264C`), перезагружая пресеты `CAcousticEQData` (`0x220E`/`0x220C`) из датасета `0x001D00`.
2. Обёртка `sub_19CC6C` (`0x0019CC6C`, `"Amp:IoAmp:SpHEARicol:set"`, команда `0x2220`) в `AudioProcess` **не имеет ни одного xref**.
3. В самом C674x (`dsp_p5092.elf`) обработчик `Amp_Sphearicol_OnOff_Set` (`0x875D0040`, вызываемый из `dev-ioamp-router` `0x0010D7E4` по cmd `0x2220`) вызывает `Get_ASP_Chain_Position(a1, 21)` (`0x875E3FA0`), ища модуль `0x15` (`21 = SpHEARicol/Megicol`) в массиве `zone0_AspModuleChain` (`0x87612D88`, 17 элементов по 14 байт). В `zone0_AspModuleChain` модуля `21` **нет** (есть только `19, 10, 17, 15, 2, 16, 4, 5, 11, 22, 6, 1, 7, 13, 3, 20, 8`), а `CreateAudioZone0` (`0x87559780`) не инициализирует `*(a1 + 128)`, поэтому `Amp_Sphearicol_OnOff_Set` всегда возвращает код ошибки `8`.

---

### 8.2 Вопрос 2: Сложение фаз `L + R + LFE` и кроссовер сабвуфера (проблема фазового вычитания на 40–60 Гц)

**Главный вывод**: На **всех четырёх** архитектурах усилителей Audi MIB2 (`0x140` Alpine B&O, `0x160/0x161` Bose, `0x181` B&O Advanced и `0x122` Internal DSP) входная матрица складывает нефильтрованные `L + R` с дискретным каналом `LFE` **ДО** ФНЧ кроссовера сабвуфера! Поэтому синтез `LFE` на стороне MMX через ФНЧ LR4 60 Гц (дающий сдвиг фазы `-120°..-180°` в полосе `40..60 Гц`) гарантированно вызывает деструктивное вычитание фаз на **любом** усилителе MIB2.

#### 1) Bose MOST150 (`0x160` / `0x161`)
- **Принимаемые MOST Sink ID**:
  - В `AudioProcess` (`sub_193A2C` @ `0x00193A2C`, `sub_193884` @ `0x00193884`) для `AU_PREM_2` (`0x160`) и `AU_STD_1` (`0x161`) используется стандартная таблица MOST-синков: **Sink `0x21` (BW `4`, Stereo PCM)** и **Sink `0x32` (BW `26`, 6ch Multichannel PCM)**.
  - В прошивке Bose (`BOSE_TRICORE_80000000.bin`) дескриптор синков `0x8006684C` (обработчик `SinkInfo` `0x0111` @ `0x8001A1F4`) поддерживает **оба** мультиканальных синка с `BlockWidth = 26` (`0x1A`): и `SinkNr = 0x26`, и `SinkNr = 0x32` (приводя их к внутреннему индексу `3` в `0x8001AF8A`).
- **Матрица `InputMixer` и порядок кроссоверов в SHARC DSP (`DSP_bitrev.ldr`)**:
  1. При подключении 6ch потока AVR32 (`0x8004AF44`) выставляет `BAC[0x16] = "InputMixer"` в `6..9` (5.1) и шлёт SPI-команду `0x920D` (`0x0008D2B1`), которая копирует 11 float-коэффициентов из `DM[0x000B2B14 + InputMixer * 11]` в `DM[0x000B2AD5..0x000B2ADF]` для входного узла **`Node[1] @ DM[0x000C7AE9]` (`proc = 0x00092D84`)**.
  2. В `0x00092D84` (`0x00092DBD..0x00092E05`) для 5.1-режима формируются басовые шины `out[4]` и `out[5]`:
     $$\text{mono\_LR} = 0.5 \cdot (L \cdot g_L + R \cdot g_R)$$
     и тут же складываются с `C * g_C` и `LFE * g_LFE`:
     $$\text{out}[4] = \text{mono\_LR} \cdot c_5 + C \cdot c_6 + \text{LFE} \cdot c_8, \quad \text{out}[5] = \text{mono\_LR} \cdot c_7 + C \cdot c_6 + \text{LFE} \cdot c_9$$
  3. Далее после регулятора тембров (`Node[13..15]`) и маршрутизатора `Node[17]` (`0x00092652`) стоит узел сумматора сабвуфера **`Node[18] @ DM[0x000C7C01]` (`proc = 0x0009219F`)**, который повторно складывает `L + R + Sub` (`F8 = F2 + F12` на `0x000921B1..0x000921C8`).
  4. И **только после этого** сигнал поступает в **кроссовер сабвуфера `Node[21] @ DM[0x000C7C39]` (`proc = 0x00091848`, 2 канала × 5 биквадов, буфер коэффициентов `DM[0x000E5BC8]`)** и **16-канальный кроссовер динамиков `Node[25..26] @ DM[0x000C7CAA..0x000C7CC3]` (`proc = 0x00091848`, 16 каналов × 7 биквадов, буфер `DM[0x000E6BF2]`)**.
  - **Что подавать на `LFE` для Bose**: Если на `LFE` подать нефильтрованную моно-сумму `(L + R) * 0.5` с фазой **`0°`** (или `0.0`, полагаясь на внутренний сумматор `0.5*L + 0.5*R` в `0x00092D84`), сложение в `Node[1]` и `Node[18]` будет строго синфазным (`0° + 0°`), без малейшего провала на 40–60 Гц. А вот делить `L/R` и `LFE` комплементарным кроссовером (HPF на `L/R` + LPF на `LFE`) на стороне MMX **нельзя** ни в режиме `Surround On`, ни в `Surround Off`: в режиме `flat` (`PosX=4 -> 0`) `Node[17]` (`DM[0x000B90A4]`) глушит слот `Sub (5)` на нулевую шину `13`, и сабвуфер питается исключительно от суммы `L + R` в `Node[18]` (поэтому если срезать НЧ с `L/R` на MMX, при выключенном 3D/Surround бас пропадёт совсем!).

#### 2) B&O Advanced MOST150 (`0x181` `AU_ADV_1`)
- **Принимаемые MOST Sink ID**:
  По таблице `ROM 0x001F6E68` (`RAM 0x40022C20`, читаемой в `SinkInfo_G` @ `0x001964B0` → `0x00193CE0` и `0x0019F530` с таблицей `ROM 0x001C8F50`), усилитель `0x181` принимает синки `0x22`, `0x23`, `0x24` (Stereo, `AudioProcess` подключает Entertainment Stereo на **`0x24`** согласно `sub_193A2C` для `amp == 18`) и **`0x32` (6ch Multichannel, BW `26`)**.
- **Входная матрица и кроссоверы в SHARC DSP (`Block2_0c0000.bin`)**:
  Входная матрица на B&O Advanced устроена **абсолютно аналогично** младшему Alpine B&O (`0x140`):
  1. Сразу после приёмника MOST (`label = 0x0F00` @ `0x000C6934`) и входного гейна `igo` (`label = 0x0A05` @ `0x000C6B54`) стоит входная матрица **`mod[48]` (`type = 14` @ `0x000C6DBC`, параметры @ `0x000C07D4` и `Subtag 3` @ `0x000C85D4`)**, которая складывает **нефильтрованные** входы `L (0x5E)`, `R (0x5F)`, `C (0x62)` и `LFE (0x63)` в суммарную шину `0x86`:
     $$\text{buf}_{0x86} = 0.5 \cdot L + 0.5 \cdot R + 0.7 \cdot C + 1.0 \cdot \text{LFE}$$
     *(В режиме Focus `Movie`, `Subtag 3 Group 1` @ `0x000C860C`, вес `LFE` увеличивается до `1.6`; в режиме `Rear`, `Group 2` @ `0x000C86A4`, веса `C` и `LFE` обнуляются)*.
  2. Далее матрица **`mod[53]` (`type = 14` @ `0x000C6EA8`, параметры @ `0x000C08C4`)** формирует шину сабвуфера **`0x94` (`SUB`)**:
     $$\text{SUB}_{0x94} = 2.8184 \cdot \text{buf}_{0x86} + 0.5012 \cdot \text{LS}_{0x66} + 0.5012 \cdot \text{RS}_{0x67}$$
  3. И **только в конце тракта** (`mod[99]` и `mod[102]` @ `0x000C792C..0x000C7994`) стоят комплементарные кроссоверы Linkwitz-Riley 4-го порядка (`type = 29`, 12 биквадов):
     - **`mod[102]` (`SUB`, шина `0x94`)**: ФВЧ 4-го порядка (`2 × type=2`) @ **`20.0 Гц` (`Q = 0.707`)** + **ФНЧ 4-го порядка (`2 × type=1`) @ `75.0 Гц` (`Q = 0.707`)**.
     - **`mod[99]` (`FLW / FRW`, дверные вуферы `0xCA, 0xCC`)**: **ФВЧ 4-го порядка (`2 × type=2`) @ `65.0 Гц` (`Q = 0.707`)** + ФНЧ 4-го порядка (`2 × type=1`) @ **`250.0 Гц` (`Q = 0.85`)**.
  - Следовательно, на `0x181` подача сдвинутого по фазе `LFE` (после LR4 60 Гц на MMX) точно так же вызовет противофазу с `0.5*L + 0.5*R` в `mod[48]`.

#### 3) Internal TI C674x DSP (`0x122` `AU_6CH` Audi Sound System)
В `dsp_p5092.elf` цепочка обработки Zone 0 задана таблицей `zone0_AspModuleChain` (`0x87612D88`, 17 записей):
1. На позиции **Entry 8 (`0x87612E68`)** стоит основной матричный микшер **`hZone0_MainMix` (`0x87613164`, ASP Module ID `5`, 20 входов → 6 выходов `[FL, FR, RL, RR, Center, Sub]`)**, который инициализируется в **`SetupZone0Mixers` (`0x8755A050`)** из статических таблиц весов `0x876113E4` (13×6) и `0x87611BA8` (7×6):
   - Для **Stereo (`0x46E`, Sink 0, входы `ch 0 = L`, `ch 1 = R`)**:
     - `Out 0 (FL) = 1.0 * L` (`0x7FFFFFFF`), `Out 1 (FR) = 1.0 * R`
     - `Out 2 (RL) = 1.0 * L`, `Out 3 (RR) = 1.0 * R`
     - **`Out 4 (Center) = 0.5 * L + 0.5 * R`** (`0x40000000` + `0x40000000`)
     - **`Out 5 (Sub) = 0.5 * L + 0.5 * R`** (`0x40000000` + `0x40000000`)
   - Для **6ch Multichannel (`0x46F`, Sink 6, входы `ch 7..12 = L, R, C, LFE, SL, SR`)**:
     Матрица `0x876113E4` (строки `7..12`) смешивает `L (ch 7)`, `R (ch 8)` и `LFE (ch 10)` в `Out 5 (Sub)` внутри `hZone0_MainMix` (Entry 8).
2. А **ФНЧ сабвуфера и ФВЧ динамиков** находятся **ниже по течению** — на позиции **Entry 10 (`0x87612EA0`)** в модуле **`hZone0_AEQ` (`0x8761316C`, ASP Module ID `22`, Acoustic EQ, пул из 18 каналов до 7 биквадов на канал, загружаемый командами `0x220E`/`0x220C` `Amp_AcousticEQ_Set` @ `0x875AE380` из датасета `0x001D00`)**!
3. **Критическое предупреждение по патчу `0x46E -> 0x46F` (6ch) на Internal DSP (`0x120..0x122`)**:
   Пользовательский темброблок и тонкомпенсация **`hZone0_UEQ` (`0x87613154`, ASP Module ID `15`, Entry 4 @ `0x87612DF8`)** подключён **только к 2 каналам: `ch 0..1` (Sink 0 Stereo `0x46E`)**! Входы `ch 7..12` (Sink 6 Multichannel `0x46F`) входят в цепочку только на Entry 5 (`hZone0_Delay` @ `0x87612E14`) и **полностью обходят `hZone0_UEQ` (Bass / Mid / Treble / Loudness)**! Поэтому на внутренних усилителях (`0x120 / 0x121 / 0x122`) CarPlay Media **обязан оставаться на стерео-синке `0x46E` (Sink 0)**.

---

### 8.3 Вопрос 3: Зависящая от громкости тонкомпенсация (Loudness) и `setInputGainOffset` (`FktID 0x0450`)

Здесь поведение четырёх семейств усилителей делится ровно **2 на 2**:

| Семейство усилителя | Как работает динамический Loudness / лимитер | Сдвигает ли `InputGainOffset` (`0x0450`) рабочую точку Loudness? |
|---|---|---|
| **Alpine B&O (`0x140`)** | 27 банков 5-секционного EQ в блоке `C4AB7`, заваливающего мидбас на высокой громкости | **ДА**: `target = min(26, raw_vol - 100 + trim)` (`trim` из `0x0450`) |
| **Bose (`0x160` / `0x161`)** | Динамический тонкомпенсатор `Node[8] @ 0x000C7B5B` (`proc = 0x000905A1`), управляемый детектором уровня `Node[9] @ 0x000C7B70` | **ДА**: `Vol` (`0x0400`) и `GainOff` (`0x0450`) имеют общий commit-хендлер `w6 = 4` (`0x0008CBD4`), вычисляющий `F15 = min(1.0f, G_GainOff * G_Vol)` |
| **B&O Advanced (`0x181`)** | 5 кусочно-линейных кривых Loudness (`LoB, UpB, OvB, LoT, UpT`) + кривая `MaxGainAllowed` в `DSPCL` (`0x001608D0` и `0x00161AC0`) | **НЕТ**: `0x001608D0` индексирует кривые **только** по сырому `Ent_Vol` (`+0xCC`); `Ent_Gain_Offset` (`+0xD0`) уходит отдельно в SHARC `mod[39]` (`label = 0x0A05`) |
| **Internal DSP (`0x120..0x122`)** | Модуль `hZone0_UEQ` (Entry 4 @ `0x87613154`), параметры Loudness из датасета `0x001F00` (`CLoudnessData`, cmd `0x2227`) | **НЕТ**: `Amp_Volume_Set` (`0x87590500`) передаёт в `UEQ->setLoudnessVolume` **только** `Mixer->getVolume`; `GainOffset` (`0x87576360`) применяется позже в `hZone0_MainMix` (Entry 8) |

#### Детали по каждому семейству:
1. **Bose (`0x160` / `0x161`)**:
   - В AVR32 `FktID 0x0450` (`0x8005E00C` → `0x80055E70` → `0x80036818`) принимает `-12..+12` (шаг `0.5 dB`, т.е. `-6..+6 dB`), сохраняет в `BAC[0x03] = "GainOff"` и шлёт в SHARC параметр `0x81` (`Entry[9] @ DM[0x000B2BD9]`), записывающий линейный множитель $G_{\text{GainOff}} = 10^{\text{dB}/20}$ в `DM[0x000B2AC3]`.
   - Громкость `FktID 0x0400` (`Entry[1] @ DM[0x000B2BA1]`) пишет $G_{\text{Vol}}$ в `DM[0x000B2AC2]`.
   - Оба параметра вызывают один и тот же SHARC commit-обработчик `w6 = 4` (`0x0008CBD4`), который на `0x0008CBE0..0x0008CBE7` перемножает их и жёстко ограничивает сверху единицей (`0 dBFS`):
     $$F_{15} = \min\left(1.0,\; G_{\text{GainOff}} \cdot G_{\text{Vol}}\right)$$
     и записывает результат в `DM[0x000C7B36]` (`Node[6]`).
   - Следующий за ним узел **`Node[9] @ DM[0x000C7B70]` (`proc = 0x00090944`)** читает состояние `Node[6]` (`0x000C7B44`) и управляет как **динамической тонкомпенсацией Bose (`Node[8] @ DM[0x000C7B5B]`, `proc = 0x000905A1`)**, так и **AudioPilot (`Node[47] @ DM[0x000C7E7A]`)**.
   - Следовательно, на Bose `setInputGainOffset` **напрямую сдвигает рабочую точку динамического Loudness** (а при `G_Vol * G_GainOff > 1.0` упирается в потолок `1.0f`).
2. **B&O Advanced (`0x181`)**:
   - В `DSPCL` функция `0x001608D0` (вызываемая из `0x00160DBC` при каждом изменении громкости) берёт **только сырой шаг громкости `Ent_Vol`** (`*(0x4002C128 + 0xCC)`) и интерполирует через `0x0015D020` кусочно-линейные таблицы из `Block2_0c0000.bin` (`Tag 0x0004` @ `0x000C55E4`):
     - `table_id = 0x0F` (`+0x540` `Ent_Vol_dB` @ `0x000C55E4`): `(0 → -140 dB), (20 → -95 dB), (115 → 0 dB)`
     - `table_id = 0x13` (`+0x5E8` `Loudness_LoB` @ `0x000C599C`): `(0 → 0 dB), (20 → +22 dB), (86 → 0 dB), (115 → 0 dB)`
     - `table_id = 0x14` (`+0x63C` `Loudness_UpB` @ `0x000C597C`): `(0 → 0 dB), (96 → 0 dB), (115 → -3 dB)` — **выше громкости `96` режет верхний бас до `-3 dB`!**
     - `table_id = 0x15` (`+0x690` `Loudness_LoT` @ `0x000C594C`): `(0 → 0 dB), (20 → +12 dB), (80 → 0 dB), (96 → 0 dB), (115 → -2 dB)` — **выше `96` режет на `-2 dB`!**
     - `table_id = 0x39` (`+0x19EC` `Loudness_OvB` @ `0x000C59E4`): `(0 → 0 dB), (100 → -30 dB)`
     - `table_id = 0x10` (`+0x594` `MaxGainAllowed` @ `0x000C5604`): `(0 → 33 dB), (30 → 37 dB), (44 → 42 dB), (59 → 52 dB), (90 → 80 dB), (115 → 101 dB)` — используется в `0x0016176E` / `0x00161AC0` (`Limited EQ gains`) для автоматического поджатия 6 полос эквалайзера (`LoB, UpB, OvB, OvT, LoT, UpT`) на высокой громкости.
   - А `InputGainOffset` (`FktID 0x0450` @ `0x00197DD0` → `DSPCL` `0x0015F150` `'Ent Gain Offset In %i Src %u'`, хранится в `*(0x4002C128 + 0xD0)`) **не участвует ни в `0x001608D0`, ни в `MaxGainAllowed`**: он переводится в линейный гейн на `0x00161336..0x00161358` (`*(0x4002BE38 + 0xD4)`) и отправляется в SHARC отдельной командой `0x0015DF90` (`'Ent igo gain updated'`, модуль `mod[39]` `label = 0x0A05` @ `0x000C6B54`).
3. **Internal TI C674x DSP (`0x120 / 0x121 / 0x122`)**:
   - В `dsp_p5092.elf` тонкомпенсация живёт внутри `hZone0_UEQ` (`0x87613154`, Entry 4).
   - И `Amp_Loudness_Set` (`0x2214` @ `0x8758B3C0`), и `Amp_Volume_Set` (`0x2203` @ `0x87590500`) читают текущую громкость `v10 = Mixer->getVolume(hZone0_MainMix, 0)` и передают **только её** в `UEQ->setLoudnessVolume(hZone0_UEQ, v10)`.
   - `InputGainOffset` (`0x2206` бит `0x80` в `Amp_AudioSettingsLong_Set` @ `0x87576360` или `0x2204` `Amp_InputGainOffset_Set` @ `0x875B2CC0`) вызывает `Mixer->setGainOffset(hZone0_MainMix, sink, gainOffset << 19)` в микшере `hZone0_MainMix` (Entry 8, стоящем **после** `hZone0_UEQ`) и **никак не влияет на кривую Loudness**.

---

### 8.4 Вопрос 4: Поддержка `MidRange` (`0x0D26` / `0x0465`) и 5/7-полосного `GraphEqualizer` (`0x0462` / `0x0463` / `0x0D27`)

#### 1) Bose MOST150 (`0x160` / `0x161`)
- **`MidRange` (`FktID 0x0D26` и `0x0465`) — ПОЛНОСТЬЮ РЕАЛИЗОВАН И РАБОТАЕТ В ЖИВОМ DSP!**
  - В таблице `FBlock 0x22` (`BOSE_TRICORE_80000000.bin`) зарегистрированы **оба** FktID для средних частот:
    - `FktID 0x0465` (`0x8005E01C`) → дескриптор `0x800625B0`
    - `FktID 0x0D26` (`0x8005E09C`) → тот же дескриптор `0x800625B0` (`Set/SetGet = 0x80056358`, `Get = 0x800563B4`)!
  - Обработчик `0x80056358` вызывает `0x80015BD4` (`0x80015BE6`: проверка диапазона `-9..+9` через `0x8005AFF4`), сохраняет значение в `BAC[0x10] = "Mid"` (`0x80062040`) и вызывает **`0x80036860` (`Set_Mid`)**.
  - `0x80036860` отправляет в SHARC параметр **`0x86`** (`Entry[4] @ DM[0x000B2BB6]`, handler `w6 = 2` @ `0x0008CBA2`), который вычисляет коэффициенты биквада средних частот и записывает их в **`Node[15] @ DM[0x000C7BE3]` (`proc = 0x000909FC`, стоящий последовательно сразу за `Bass` `Node[13] @ 0x000C7BC7` и `Treble` `Node[14] @ 0x000C7BD5`)**!
  - То есть на **Bose (`0x160` и `0x161`)** достаточно включить `<MID>` в `MIB_AudioTable_AU_G22.xml` (или вызывать `DSISound.setMidRange`), и в усилителе заработает настоящий аппаратный полосовой фильтр средних частот!
- **`GraphEqualizerOnOff` (`0x0462`), `GraphEqualizer` (`0x0463`) и `EqualizerSetup` (`0x0D27`)**:
  В таблице `FBlock 0x22` Bose (`0x8005DF44..0x8005E0B8`) этих трёх FktID **нет вообще** (усилитель вернёт MOST `Function Not Available`).

#### 2) B&O Advanced MOST150 (`0x181`)
- **`MidRange` (`0x0D26`) и `GraphEqualizer` (`0x0462` / `0x0463`) — ОТСУТСТВУЮТ В `FBlock 0x22`**:
  - В мастер-таблице `FBlock 0x22` (`0x001C7440..0x001C7590`) и таблице нотификаций `FB22InitNtf` (`0x001E9B00..0x001E9BB8`) есть только `0x0202` (`Bass` @ `0x001977F0`), `0x0203` (`Treble` @ `0x001979F0`), `0x0402` (`Subwoofer` @ `0x00197BF0`) и read-only `0x0D27` (`FB22_GraphEqSetup_G` @ `0x0019A390`).
  - Ни `0x0D26` (`MidRange`), ни `0x0462` (`GraphEqualizerOnOff`), ни `0x0463` (`GraphEqualizer`) в `FBlock 0x22` **не зарегистрированы** (в `0x001E9B00` их слоты забиты нулями `0x0000`).
  - При этом внутри задачи `DSPCL` оставлены неиспользуемые обработчики `0x0015F9D0` (`'Ent Mid In %i, Src %u'`) и `0x0015FC00` (`'User EQ Ent set: Src %u, Band: %i, Value(dB): %i'`), но из `FBlock 0x22` к ним нет ни одного вызова (мёртвый код от общей платформы B&O/Bentley).

#### 3) Internal TI C674x DSP (`0x120 / 0x121 / 0x122`)
- **И `MidRange` (`<MID>`), и полный 5-полосный эквалайзер (`<BAS>, <EQ2>, <MID>, <EQ4>, <TRE>`) ПОЛНОСТЬЮ РЕАЛИЗОВАНЫ**:
  1. В `AudioProcess` (`sub_14D91C` @ `0x0014DBE4..0x0014DC6C` → `sub_1A1660` @ `0x001A1660` → `dev-ioamp-router` `0x0010BD24`) команда `0x2206` (`Amp_AudioSettingsLong_Set`) передаёт все 5 полос (`bass, treble, mid, midLow, midHigh`) с битовой маской `0x01..0x10`.
  2. В `dsp_p5092.elf` структура параметров `zone0_uEQParams` (`0x876012DC`) скомпилирована с **`numBands = 5`** (`offset +4 = 5`), а `Amp_AudioSettingsLong_Set` (`0x87576360`) вызывает `UEQ->setBandLevel(hZone0_UEQ, band_idx, val)`:
     - бит `0x01` (`<BAS>`) → **Band 0**
     - бит `0x02` (`<EQ2>` / `midLow`) → **Band 1**
     - бит `0x04` (`<MID>` / `mid`) → **Band 2**
     - бит `0x08` (`<EQ4>` / `midHigh`) → **Band 3**
     - бит `0x10` (`<TRE>`) → **Band 4**
  3. **Важный нюанс по датасету `0x001B00` (`CUserEQData`)**: Частоты, добротности `Q`, типы фильтров и шаг в дБ для всех 5 полос загружаются в C674x при старте из датасета `0x001B00` командами `0x2225` (`Amp_ToneCharacteristics_Set` @ `0x8759F520`) и `0x2226` (`Amp_ToneScaling_Set` @ `0x875BDF40`), формируемыми в `AudioProcess` функцией `sub_1A31EC` (`0x001A31EC`, цикл ровно на 5 полос). Если в заводском EEPROM датасета `0x001B00` для Audi (`0x120..0x122`, где с завода включены только `<BAS>` и `<TRE>`) в полосах `1..3` (`midLow, mid, midHigh`) прописан `filterType = 0` или `stepSize = 0`, то при включении `<MID>` / `<EQ2>` / `<EQ4>` в `MIB_AudioTable_AU_G22.xml` нужно также убедиться, что в `0x001B00` (или через команду `0x2225`/`0x2226`) для полос `1..3` заданы ненулевые `filterType = 2` (peaking biquad), `fc`, `Q` и `stepSize`.

---

### 8.5 Вопрос 5: GALA / VNC / AudioPilot (`DSISound.setNoiseCompensation`, `FktID 0x0D29`)

- **Как `AudioProcess` обрабатывает `setNoiseCompensation` и `requestSetAudiopilot`**:
  В `AudioProcess` (`sub_14731C` @ `0x0014731C`) и `requestSetGalapilot` (`case 0x39`), и `requestSetAudiopilot` (`case 0x10`) идут в одну и ту же функцию `sub_14E2E0`, которая выставляет свойство `0x94` у `CLD22` (`0x00194AE4`) и вызывает `sub_190140` (`0x00190140`), отправляющую **`FktID 0x0D29` (`NoiseCompensationLevel`)** всем внешним MOST-усилителям (и команду **`0x2215` `Amp_Gala_Set` @ `0x875D0680`** внутреннему C674x DSP, где `0` выключает GALA).
- **Что именно делает `FktID 0x0D29` внутри прошивки Bose (`0x160` и `0x161`)**:
  В `BOSE_TRICORE_80000000.bin` в таблице `FBlock 0x22` есть и `0x046B` (`0x8005E024` → `0x80062628`), и `0x0D29` (`0x8005E0A4` → `0x80062640`).
  Обработчик `Set/SetGet` для `0x0D29` по адресу **`0x800549F4`**:
  ```text
  0x800549f4: e0 68 8c e1   mov   r8, -29471       ; адрес варианта [0x00008CE1] (Var)
  0x800549f8: 11 19         ld.ub r9, r8[0x0]      ; r9 = Var (0 = 0x161 Bose Std, 2 = 0x160 Bose 3D, 3 = Audi6416A_SA)
  0x800549fa: 58 39         cp.w  r9, 3
  0x800549fc: c0 60         breq  0x80054a08       ; только для Var == 3 идёт в 0x80015eb0 (GALA 0..7)
  0x800549fe: 48 98         ld.w  r8, pc[0x24]     ; 0x8000BF4C (обработчик FktID 0x046B AudioPilot!)
  0x80054a00: f0 08 00 00   mov   pc, r8           ; tail-call в 0x8000BF4C!
  ```
  - На **всех усилителях Bose (`0x160` `Var=2` и `0x161` `Var=0`)** обработчик `FktID 0x0D29` (`0x800549F4`) напрямую делает tail-call в **`0x8000BF4C` (обработчик `0x046B` AudioPilot)**!
  - `0x8000BF4C` проверяет булево значение `0..1` (`0x8005B05C`), сохраняет его в `BAC[0x00] = "VNC"` (`0x80061EC0`) и вызывает `0x80036D1C` (`Set_VNC`), который шлёт в SHARC `Entry[43] @ DM[0x000B2CC7]` (`w6 = 21` @ `0x0008CC2F`), записывая **`0`** в `DM[0x000B2AC8]`, `DM[0x000C7B9E]` и `DM[0x000C7E87]` (`Node[47] @ 0x000C7E7A`, блок AudioPilot).
  - **Вывод**: Вызов **`DSISound.setNoiseCompensation(conn, term, 0)` (`FktID 0x0D29 = 0`) на 100% штатно и полностью выключает Bose AudioPilot** как на `0x160` (`AU_PREM_2`), так и на `0x161` (`AU_STD_1`), а также выключает VNC на B&O Advanced `0x181` (`0x0019AED0` → `0x0015FD00` `'VNC user amount In %u'`) и GALA на Internal DSP `0x120..0x122` (`0x875D0680`). *(Бонусная находка: в заводском `MIB_AudioTable_AU_G22.xml` для `0x161` `AU_STD_1` ошибочно прописан `NOISECOMPENSATIONLEVEL_ENT = 2` (шкала `0..24`) вместо `3` (On/Off `0..1`), из-за чего любое значение `> 1` отвергается валидатором Bose `0x8005B05C`, но `0` = OFF и `1` = ON проходят всегда).*



