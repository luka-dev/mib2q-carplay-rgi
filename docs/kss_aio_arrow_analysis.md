# KSS AIO_Arrow Reverse Engineering (AU491 FPK)

Binary: KSS AUTOSAR firmware from VC (AU491 FPK cluster).

## Architecture: MOST -> KSS -> EB GUIDE -> dp Items

```
HU (MHI2)                                    VC (AU491 FPK)
+--------------------------------------+     +--------------------------------------------+
| PresentationController (native C++)  |     | MOST Class 46 reception                    |
|   DSI KOMOView.setRouteInfo()        |     |   -> Class46_Mode0_PublishQueue239         |
|   -> serializes RouteInfoElement[]   |     |   -> Queue 239                             |
|   -> sends MOST Class 46 messages    |     |   -> Class34_46_49_Router                  |
+--------------------------------------+     |      |                                      |
         |                                   |      | msgId 0x2289, opType=5               |
         | MOST control channel              |      v                                      |
         +---------------------------------->|   sub_108EB66(1)                            |
                                             |     stores 5 bytes at off_108EF40[0..4]    |
                                             |     signal group 29                         |
                                             |     -> KssIpc -> EB GUIDE (fds)             |
                                             |        -> AIO_Arrow0-3_Direction dp items   |
                                             +--------------------------------------------+
```

Two independent data paths on the VC:

1. **BAP path** (text overlays + HUD):
   BAP LSG 50 -> MOST -> gssipc-kbd -> dp items -> EB GUIDE renders text/HUD

2. **MOST Class 46 path** (AIO arrows + video):
   MOST Class 46 -> KSS -> EclWrapper / Router -> signal groups -> KssIpc -> EB GUIDE -> AIO dp items

BAP and Class 46 are separate MOST message types. gssipc-kbd handles BAP.
KSS handles Class 46. They do NOT overlap for navigation data.

## KSS Function Map

| Function                                  | Address   | Purpose                                         |
|-------------------------------------------|-----------|-------------------------------------------------|
| `EclWrapper_MainTask`                     | 0x10872F0 | Main task: compass, process msgs, arrow structs |
| `EclWrapper_ProcessMsgIds500_522`         | 0x1093BD4 | Processes navigation MOST msgs 0x500-0x522      |
| `EclBuildVcPacket_FromMsgId`              | 0x1093FD2 | Builds VC packets for Queue 233 forwarding      |
| `EclCache_UpdateFromQueue239`             | 0x1094208 | Updates ECL cache from Queue 239 (Class 46)     |
| `Queue239_ConsumeToEclCache`              | 0x105ADA2 | Consumes Queue 239 into ECL cache               |
| `Queue238_BuildAndSendQueue233`           | 0x105AD7A | Builds VC packets from Queue 238 (Class 34)     |
| `Class46_Mode0_PublishQueue239`           | 0x1069824 | MOST Class 46 -> Queue 239                      |
| `Class34_Mode0_PublishQueue238`           | 0x1069790 | MOST Class 34 -> Queue 238                      |
| `Class34_46_49_Router_Consume238_239_241` | 0x105B902 | Routes messages from all three classes          |
| `KssIpc_SendCmd`                          | 0x1062ABA | Sends commands to EB GUIDE via IPC              |
| `sub_108EB66`                             | 0x108EB66 | **Arrow direction handler (msg 0x2289)**        |
| `sub_108EC68`                             | 0x108EC68 | **InfoStates handler (msg 0x515)**              |
| `sub_108F42C`                             | 0x108F42C | **InfoStates value validator -- BLOCKS value 6** |

## Queue System

```
MOST Class 46 -> Queue 239 -> ECL cache -> EclWrapper_ProcessMsgIds500_522
                           -> Router (for 0x515, 0x2289, 0x98F)

MOST Class 34 -> Queue 238 -> Router (forwarding path)
                           -> EclBuildVcPacket_FromMsgId -> Queue 233

MOST Class 49 -> Queue 241 -> Router (0x374, 0x375 distance msgs)
```

## MOST Messages (Class 46 -> VC)

### Navigation Data (0x500-0x522) -- processed by EclWrapper

| Msg ID | Decimal | KssIpc Cmd | Signal Group         | Purpose (inferred)                     |
|--------|---------|------------|----------------------|----------------------------------------|
| 0x500  | 1280    | 1          | 58 (+10,+16)         | ManeuverDescriptor part 1              |
| 0x501  | 1281    | 2          | 61 (+24,+30)         | ManeuverDescriptor part 2              |
| 0x504  | 1284    | 11         | 58 (+42/+48,+22/+36) | Distance (variant-dependent, LHD/RHD?) |
| 0x505  | 1285    | 8          | 61 (+0,+6)           | TurnTo street                          |
| 0x506  | 1286    | 9          | 61 (+12,+18)         | Current street                         |
| 0x509  | 1289    | 3+4        | 58 (+62) + 61 (+36)  | Route info (writes both SGs)           |
| 0x50C  | 1292    | 10*        | 58 (+57) + special   | Lane guidance (5 lanes, byteswap)      |
| 0x50F  | 1295    | 5          | 106 (+0,+6)          | Unknown (text data?)                   |
| 0x510  | 1296    | 6          | 58 (+70,+76)         | Distance to destination                |
| 0x522  | 1314    | 7          | 61 (+44)             | Route guidance status (1 byte)         |

*0x50C uses KssIpc dest 12317 (0x301D) instead of the default.

Each message carries 12 bytes (6 pairs of high/low bytes) except 0x522 (1 byte)
and 0x50C (special format with byteswap).

### Arrow/Status Messages -- processed by Router

| Msg ID     | Decimal | Handler       | OpType             | Purpose                    |
|------------|---------|---------------|--------------------|----------------------------|
| **0x2289** | 8841    | `sub_108EB66` | 5 (Class 46 input) | **Arrow directions**       |
| **0x515**  | 1301    | `sub_108EC68` | -                  | **InfoStates (RG status)** |
| 0x98F      | 2447    | `sub_108ECDE` | -                  | Init/reset                 |

## ROOT CAUSE: InfoStates=6 Rejected by KSS Firmware

### The Validator

`sub_108F42C` at address `0x108F42C` validates InfoStates values received via MOST 0x515:

```c
BOOL validate_infostates(unsigned int a1) {
    return a1 <= 0xF && (a1 & 5) != 4 && (a1 & 0xA) != 8;
}
```

**Value 6 (MobileDevice) FAILS validation**: `(6 & 5) = (0b0110 & 0b0101) = 0b0100 = 4`, so `(a1 & 5) != 4` is false.

The validation enforces bit-dependency rules (bit 2 requires bit 0, bit 3 requires bit 1). Value 6 = `0b0110` has bit 2 set without bit 0, which violates this rule.

```asm
; sub_108F42C -- Thumb2 ARM
108f42c  CMP   R0, #0xF       ; reject > 15
108f42e  BHI   -> return 0
108f430  AND.W R2, R0, #5     ; check bit 2 requires bit 0
108f434  CMP   R2, #4
108f436  BEQ   -> return 0    ; *** VALUE 6 REJECTED HERE ***
108f438  AND.W R0, R0, #0xA   ; check bit 3 requires bit 1
108f43c  CMP   R0, #8
108f43e  BNE   -> return 1
108f440  MOVS  R0, #0
108f442  B     locret
108f444  MOVS  R0, #1
108f446  BX    LR
```

### Valid vs Rejected Values

| Value | Binary   | (v&5)!=4     | (v&0xA)!=8 | Result   |
|-------|----------|--------------|------------|----------|
| 0     | 0000     | 0!=4 pass    | 0!=8 pass  | PASS     |
| 1     | 0001     | 1!=4 pass    | 0!=8 pass  | PASS     |
| 2     | 0010     | 0!=4 pass    | 2!=8 pass  | PASS     |
| 3     | 0011     | 1!=4 pass    | 2!=8 pass  | PASS     |
| 4     | 0100     | 4=4 FAIL     |            | FAIL     |
| 5     | 0101     | 5!=4 pass    | 0!=8 pass  | PASS     |
| **6** | **0110** | **4=4 FAIL** |            | **FAIL** |
| 7     | 0111     | 5!=4 pass    | 2!=8 pass  | PASS     |
| 8     | 1000     | 0!=4 pass    | 8=8 FAIL   | FAIL     |
| 9     | 1001     | 1!=4 pass    | 8=8 FAIL   | FAIL     |
| 10    | 1010     | 0!=4 pass    | 10!=8 pass | PASS     |

### Consequence

InfoStates=6 is blocked before it reaches signal group 29. The `BAP_NavSD_InfoStates_States` dp item is never set to 6, so the VC never transitions to `SV_NavFPK_Compass_MobileDevice`, and AIO arrows are never rendered -- even though the AIO_Arrow dp items ARE populated via MOST 0x2289.

### Why This Cannot Be Fixed

The VC/FPK (KSS AUTOSAR firmware) cannot be persistently patched:

- KSSApplication.bin runs in a secure AUTOSAR environment
- UDS WriteMemoryByAddress patches are RAM-only (lost on every reboot)
- No persistent flash write access from diagnostic session
- Would require ODIS/diagnostic tool connected at every boot

This makes the AIO arrow / MobileDevice view approach **not viable** for CarPlay.

## Message 0x2289 -- Arrow Directions (Detail)

### Format

- **MOST Class**: 46 (Navigation function block)
- **Message ID**: 0x2289 (8841)
- **OpType/Mode**: 5 (from Queue 239 extra field)
- **Payload**: 5 bytes

```
byte[0] = Arrow 0 direction
byte[1] = Arrow 1 direction
byte[2] = Arrow 2 direction
byte[3] = Arrow 3 direction
byte[4] = Extra field (purpose unknown)
```

### Validation (sub_108EB66)

Each arrow byte is validated:
- **0** = valid (no arrow)
- **1-208** = valid (direction value)
- **209-254** = invalid (rejected, causes abort)
- **255** = sentinel (arrow disabled / not available)

Additional check per arrow: a bitmask at `off_108EBCC[108]` controls which arrows
are "enabled". Bit N set = arrow N is active. If not active, the stored value is
overridden with 0xFF when forwarding.

### Storage

- Stored at `off_108EF40[0..4]` (5 bytes)
- Triggers signal group 29
- Forwarded to EB GUIDE via KssIpc

### Byte-to-AIO Mapping

The AIO_Arrow direction dp item enum:
```
NoArrow=0, Straight=1, StraightLeft=2, Left=3, SharpLeft=4,
UturnLeft=5, UturnRight=6, SharpRight=7, Right=8, StraightRight=9
```

The exact mapping from the raw byte (0-208) to AIO enum (0-9) is inside
EB GUIDE's compiled model (`fds`). Since PresentationController generates
0x2289 internally, the mapping is handled by the native pipeline.

## Message 0x515 -- InfoStates (Detail)

### Format

- **MOST Class**: 46
- **Message ID**: 0x515 (1301)
- **Payload**: 1 byte

### Handler Chain

1. `Class34_46_49_Router` receives msg 0x515 from Queue 239
2. Calls `sub_108EC68` (InfoStates handler)
3. `sub_108EC68` calls `sub_108F42C` to validate the value
4. If valid: stores at `off_108EF40[5]`, triggers signal group 29
5. Signal group 29 -> KssIpc -> EB GUIDE -> `BAP_NavSD_InfoStates_States` dp item

Step 3 rejects value 6, so steps 4-5 never execute for MobileDevice mode.

## Signal Group Layout at off_108EF40

```
[+0] byte: Arrow 0 direction  (from 0x2289)
[+1] byte: Arrow 1 direction  (from 0x2289)
[+2] byte: Arrow 2 direction  (from 0x2289)
[+3] byte: Arrow 3 direction  (from 0x2289)
[+4] byte: Extra field         (from 0x2289)
[+5] byte: InfoStates value    (from 0x515)
```

All 6 bytes share signal group 29 -> KssIpc -> EB GUIDE.

## Other Class 34 Messages -- forwarded via Queue 233

| Msg ID | Decimal | Handler       | Purpose (inferred)                             |
|--------|---------|---------------|------------------------------------------------|
| 0x108  | 264     | `sub_108F276` | Unknown                                        |
| 0x1105 | 4357    | `sub_108F1E0` | Compass/gyro sensor data (reads 0xD7FE-0xD815) |
| 0x22AE | 8878    | `sub_108F286` | Unknown                                        |
| 0x22AF | 8879    | `sub_108EFB0` | Navigation coords (16 x uint16)                |
| 0x22B0 | 8880    | `sub_108F062` | Unknown                                        |
| 0x516B | 20843   | `sub_108F1A8` | Unknown                                        |

## Class 49 Messages (Queue 241)

| Msg ID | Decimal | Purpose                         |
|--------|---------|---------------------------------|
| 0x375  | 885     | Maneuver distance (uint16 pair) |
| 0x374  | 884     | Maneuver distance (variant)     |

## Global Data References

| Symbol          | Address   | Runtime Value | Purpose                                   |
|-----------------|-----------|---------------|-------------------------------------------|
| `dword_105BADC` | 0x105BADC | 0x515         | InfoStates message ID                     |
| `dword_105BAE0` | 0x105BAE0 | 0x98F         | Init/reset message ID                     |
| `dword_105BAE4` | 0x105BAE4 | 0x2289        | Arrow direction message ID                |
| `dword_105AC64` | 0x105AC64 | runtime ptr   | Compass arrow struct array (4 x 36 bytes) |
| `dword_1093F08` | -         | runtime ptr   | Signal group 58 buffer                    |
| `off_1093F04`   | -         | runtime ptr   | Signal group 61 buffer                    |
| `dword_1094298` | -         | runtime ptr   | Signal group 106 buffer                   |
| `dword_109428C` | -         | runtime ptr   | Lane guidance / special buffer            |
| `off_108EF40`   | -         | runtime ptr   | Arrow + InfoStates data (6 bytes, SG 29)  |
| `off_108EBCC`   | -         | runtime ptr   | Arrow config (enable bitmask at +108)     |
