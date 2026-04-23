# AltScreen activation — full gate analysis (MU1316 vs MHI3 vs iOS 26)

## TL;DR — closed 2026-04-23

iOS 26.1 will NOT advertise/activate `altScreen` on our Audi MU1316 head
unit even if libairplay's /info injection works, because a chain of 6
independent gates stands between the car and the `altScreen` feature.

**Final blocker found (architectural):** MU1316 uses **Cinemo's pre-2020
iAP2 SDK** which predates the `WirelessCarplayTransportComponent` /
TransportComponent schema required for Gate 3. Only two coarse flags
are exposed to dio_manager — `SUPPORTS_IAP2_CONNECTION`, `SUPPORTS_CARPLAY`
— neither of which controls subparam 17 emission. Cinemo's wire-encoder
has no concept of Param 21 or the themed-assets subparam.

Consequence: the only way to pass Gate 3 on this hardware is to raw-bytes
inject a synthetic Param 21 into the outgoing iAP2 Identification message
at wire level, which combined with Gates 5+6 (also unimplemented in
libairplay 210.81) is **80-120 hours of rewrite** with no guarantee iOS
won't impose further hidden gates. **Branch closed.**

This document captures everything found by static reverse on:
- `iPhone18-3_26.1_23B85_Restore` (iOS 26.1 — CoreAccessories.framework,
  CarKit.framework, AirPlayReceiver.framework, AirPlaySender.framework,
  ExternalAccessory.framework, carkitd)
- MHI3 firmware (`_swup_carplay_aa/extracted_elf_opt/libairplay.so`,
  `libesoiap2.so`, `dio_manager`, `tbt_renderer`)
- MU1316 firmware (`libairplay.so` in IDA, MU1316 appimg tree)

No live logs — pure static analysis.

## The full negotiation pipeline (iOS 26 expects)

```
Phone                                                  Car
─────                                                  ───

Bonjour TXT discovery                       ←──  srcvers=210.81 (MU1316)
                                                  srcvers=450.14.2 (MHI3)

iAP2 Identification message (MsgID 0xEA00)  ←──  ParamID 20/21 with
    ↓                                              subparam 17 (void)
    CoreAccessories parses via
    _parseIdentificationParams_3()
    ↓
    *(idStruct + 131) = 1
      (byte flag "isIdentifiedForThemeAssets")
    ↓
    ACCExternalAccessory (line 2020):
      if (isIdentifiedForThemeAssets)
          _eaAccessoryCapabilities |= 0x200000  ← bit 21
    ↓
    CRVehicleAccessoryManager (line 530):
      setSupportsThemeAssets: (capabilities >> 21) & 1
    ↓
    CRFeaturesAvailabilityAgent:
      Ferrite feature mask retained (includes altScreen bit 0)
    ↓
    CarKit.mm CRCarPlayFeaturesAsAirPlayFeatures():
      bit 0 → "altScreen" string added to proposed CFArray
    ↓
HTTP /info or RTSP SETUP body carries      ──→  car must echo
    proposed features incl "altScreen"           "altScreen" in
                                                  SETUP response
                                                  "enabledFeatures" CFArray
                                         ←──  enabledFeatures: [
                                                "altScreen", "viewAreas",
                                                "uiContext", "cornerMasks",
                                                "focusTransfer"]
    ↓
    Activate.mm:319
      if CFArrayContainsValue(v43, @"altScreen")
          *(derived + 63) = 1  ← altScreen enabled
    ↓
Stream SETUP with type=111                 ──→  libairplay SessionSetup
    ↓                                              stream switch:
                                                   case 100:  main audio
                                                   case 101:  alt audio
                                                   case 110:  MainScreen
                                                   case 111:  ???
    ↓
ScreenStreamProcessData with               ←──  stream data delivered
    stream=AltScreen-instance
    ↓
Car's compositor renders AltScreen
    video to cluster display
```

## Gate-by-gate breakdown

### Gate 0 — iOS device bitmask (PASS ✅)

`CRFeaturesAvailabilityAgent::_deviceFeatures` at
`carkitd/CRFeaturesAvailabilityAgent.mm:34` returns `v24 | v25`:

- `v24 = v13 | 0x338` or `v13 | 0x310` — HEVC-gated base
- `v25 = 0xC0` or `0x80` — `_os_feature_enabled_impl()` gated
- `v13 = 7` on modern chipsets (NOT t7000/s8000/s8003/t8010/t8011/t8012)
  and HEVC-capable

For iPhone 12+ (including the iPhone 18 we test on): `v13=7` path →
final = `0x33F | 0xC0 = 0x3FF` → bit 0 (altScreen) **is** set.

Old chipsets (6s/7/8 era) → `v13 != 7` → bit 0 never set on device side
alone. Not our concern.

### Gate 1 — `isCarPlayAllowed` / `isCarPlayThemeSupportEnabled` (PASS ✅)

Two user-togglable preferences. Enabled by default on every iPhone. Not
our concern.

### Gate 2 — Apple's `assetDisabler` server-side kill-switch (PASS ✅)

`_disabledCarPlayFeaturesForVehicle:` checks server-side asset disabler
for a specific `clusterAssetIdentifier`. Can remotely disable Ferrite
features. Not triggered for cars that don't advertise a
`clusterAssetIdentifier` (our case).

### Gate 3 — `supportsThemeAssets` on CRVehicle (❌ BLOCKS MU1316)

**Location:** `CarKit.framework/CRVehicleAccessoryManager.mm:530`:

```objc
[vehicleCopy setSupportsThemeAssets:(v18 >> 21) & 1];
// v18 = [accessoryCopy accessoryCapabilities] (uint64 bitmask)
```

Bit 21 of `accessoryCapabilities` is set in
`ACCExternalAccessory.mm:2020-2026`:

```c
isIdentifiedForThemeAssets = iap2_identification_isIdentifiedForThemeAssets(info);
v18 = self->_eaAccessoryCapabilities;
if (isIdentifiedForThemeAssets) {
    v18 |= 0x200000;               // bit 21
    self->_eaAccessoryCapabilities = v18;
}
```

`iap2_identification_isIdentifiedForThemeAssets` (at
`CoreAccessories/CoreAccessories_19.mm:1512`) reads byte +131 of the
parsed iAP2 Identification struct:

```c
uint64_t iap2_identification_isIdentifiedForThemeAssets(uint64_t a1)
{
    if (!a1) return 0;
    if (!iap2_feature_getFeature(a1, 1u)) return 0;
    ...
    LOBYTE(v3) = *(v3 + 131);
    return v3 & 1;
}
```

Byte +131 is populated by `_parseIdentificationParams_3` at
`CoreAccessories_15.mm:8960` during IAP2 Identification parsing:

```c
BOOL _parseIdentificationParams_3(iapMsg, parentParam, subparam)
{
    ParamID = iAP2MsgGetParamID(subparam);
    if (ParamID != 21 && ParamID != 20) { ...reject... }
    // iterates subparams
    while (subparam) {
        switch (subparam_id) {
            ...
            case 17:
                v414 = iAP2MsgIsDataVoid(subparam);  // void marker → 1
                break;
            ...
        }
    }
    *(output + 128) = v430;   // isIdentifiedForCarPlay
    *(output + 129) = v432;   // isIdentifiedForWirelessCarPlay
    *(output + 130) = v435;   // isIdentifiedForUSBCarPlay
    *(output + 131) = v414;   // isIdentifiedForThemeAssets ← THE BYTE
    *(output + 132) = v417;
    ...
}
```

Sibling functions at consecutive offsets:
- `isIdentifiedForCarPlay` → byte +128
- `isIdentifiedForWirelessCarPlay` → byte +129
- `isIdentifiedForUSBCarPlay` → byte +130
- `isIdentifiedForThemeAssets` → byte +131

**How to pass this gate (car side):**

The car's iAP2 Identification message (0xEA00) must include subparam ID
17 (as a void/empty payload) inside **ParamID 20 (USBDeviceTransport
Component)** OR **ParamID 21 (WirelessCarplayTransportComponent)**.

Wire bytes for the subparam:

```
00 04    — 16-bit length = 4 (header-only, no payload)
00 11    — 16-bit subparam ID = 0x0011 (17)
```

Verified via MHI3 libesoiap2.so reverse — see next section.

### Gate 4 — `clusterAssetIdentifier` (NOT required for altScreen!)

Initially misinterpreted as a blocker. Actually:

- `CARCarPlayServiceMessageStartSession` dict contains
  `@"clusterAssetIdentifer"` (sic — typo in iOS!) / `@"clusterAssetVersion"`
  / `@"SDKVersion"` fields for cars that want Apple-issued themed cluster
  assets (layouts/fonts/icons for OEM-specific cluster UIs).
- When received, `CRThemeAssetLibrarian receivedClusterAssetIdentifier:`
  **auto-sets `supportsThemeAssets = YES`** as a side-effect, bypassing
  Gate 3's iAP2 check.
- **Critical:** Neither MHI3 nor MU1316 binaries contain the strings
  `clusterAssetIdentifier`, `SDKVersion`, `themeAsset`, or
  `supportsThemeAssets`. MHI3 does not use this mechanism at all.
- MHI3 passes Gate 3 via iAP2 subparam 17 instead.

So: we do NOT need to fake Apple's clusterAssetIdentifier (which would
require a licensed OEM/model ID). iAP2 subparam 17 is enough.

### Gate 5 — `enabledFeatures` CFArray in SETUP response (❌ BLOCKS MU1316)

**Location:** `AirPlaySender/Activate.mm:319+`:

```objc
if (CFArrayContainsValue(v43, ..., @"altScreen"))
    *(derived + 63) = 1;   // altScreen enabled
if (CFArrayContainsValue(v43, ..., @"uiContext"))
    *(derived + 64) = 1;
if (CFArrayContainsValue(v43, ..., @"viewAreas"))
    *(derived + 62) = 1;
if (CFArrayContainsValue(v43, ..., @"cornerMasks"))
    *(derived + 65) = 1;
if (CFArrayContainsValue(v43, ..., @"focusTransfer"))
    *(derived + 66) = 1;
// ... hevc, fileTransfer, vehicleStateProtocol, etc.
```

`v43` is the negotiated feature-key list coming back from the car's
SETUP response. Car must emit a CFArray under key `@"enabledFeatures"`
in the SETUP response CFDict.

MHI3 libairplay rodata includes these CFString constants (adjacent in
the constant pool around 0x115000):

```
0x1153D0 : "altScreen"
0x1153E8 : "uiContext"
0x115400 : "cornerMasks"
0x115418 : "focusTransfer"
0x115478 : "enabledFeatures"   ← the wire key
```

Plus functions:

```
AirPlayReceiverSessionHasFeatureAltScreen @ 0x58CE0
AirPlayReceiverSessionHasFeatureViewAreas
AirPlayReceiverSessionHasFeatureUIContext
AirPlayReceiverSessionHasFeatureCornerMasks
AirPlayReceiverSessionHasFeatureFocusTransfer
AirPlayReceiverSessionHasFeatureEnhancedSiri
AirPlayReceiverSessionHasFeatureH264Level51
AirPlayCopyServerInfo @ 0x4F900
AirPlayScreenDictSetViewAreas @ 0x5F990
AirPlayAltScreenDictCreate @ 0x5F860
AirPlayInfoArrayAddScreen @ 0x5FEC0
```

MU1316 libairplay 210.81 rodata: **0 matches** for any of those strings.
210.81 predates Ferrite, does not implement this response format at all.

**Pass strategy:** hook `AirPlayReceiverSessionSetup` after it returns
the response CFDict, insert `@"enabledFeatures"` CFArray with our
feature strings. Needs runtime CFString creation.

### Gate 6 — Stream `type=111` handler (❌ BLOCKS MU1316)

MU1316 libairplay `AirPlayReceiverSessionSetup @ 0x272B8` stream-type
switch (iterating streams in SETUP body):

```c
switch (streamType) {
    case < 100:     LogInfo("Unsupported stream type: %d"); goto skip;
    case 100, 101:  handle main/alt audio;
    case 110:       sub_26F9C() — MainScreen handler;
    default:        LogInfo("Unsupported stream type: %d"); goto skip;
}
```

**`type=111 (AltScreen)` falls into default** → "Unsupported stream
type: 111" logged → skipped. libairplay will simply refuse the stream
even if iOS tries to open it.

MHI3 has a handler for type=111 (AltScreen stream setup) emitting
`&unk_9E928` (= `"streams"` key) into response dict with per-stream
setup info. MU1316 doesn't.

**Pass strategy:** hook the SessionSetup stream loop; on type=111 create
a fake AltScreen stream wrapper that routes
`ScreenStreamProcessData` frames to our own OMX/compositor pipeline
instead of libairplay's stock MainScreen handler.

## MHI3 iAP2 Identification composer — reversed

**File:** `MHI3_ER_AU_P4364_8W0906961DR/libesoiap2.so`
**Function:** `iap2::CIAP2ControlSessionModuleIdentMsgComposer::identificationInformation`
**Address:** `0x4F9A0`, size `0x6090` (24720 bytes)

Two sub-blocks were found that emit subparam 17 (themeAssets trigger),
one per transport-component top-level param:

### Loc1 — USBDeviceTransportComponent (top-level Param ID 20)

Final top-level emit at `0x51620`:
```c
sub_48370(v547, 20);   // Param ID 20 = USBDeviceTransportComponent
```

Subparams emitted before the close:
```c
// subparam 10 — 32-byte data (likely UUID), vtable A6468
sub_4A400(v723, 32);  v723[0] = &off_A6468;  sub_48370(v723, 10);
// subparam 12 — 4-byte data, vtable A6508 (void-marker family)
sub_4A400(v725, 4);   v725[0] = &off_A6508;  sub_48370(v725, 12);
// subparams 13..18 — all void markers, vtable A6508
sub_4A400(v727, 4);   v727[0] = &off_A6508;  sub_48370(v727, 13);
sub_4A400(v729, 4);   v729[0] = &off_A6508;  sub_48370(v729, 14);
sub_4A400(v731, 4);   v731[0] = &off_A6508;  sub_48370(v731, 15);
sub_4A400(v733, 4);   v733[0] = &off_A6508;  sub_48370(v733, 16);
sub_4A400(v735, 4);   v735[0] = &off_A6508;  sub_48370(v735, 17);  ← THEMEASSETS
sub_4A400(v737, 4);   v737[0] = &off_A6508;  sub_48370(v737, 18);
// followed by conditional payload population based on *(a2 + 680+)...
```

Helper functions used:

- `sub_4A400(buf, size)` — zero-fills a param/subparam buffer slot of
  the given payload size
- `buf[0] = &vtable` — sets vtable pointer (determines subparam type)
- `sub_48370(buf, id)` — writes 16-bit subparam ID big-endian into
  buffer offset +2..+3

Vtables (confirmed as iAP2 parameter-type descriptors):

- `off_A6468` — 32-byte data type (UUID/string)
- `off_A6508` — 4-byte or void type (bool-like)
- `off_A64C8` — 2-byte type (uint16)
- `off_A64E8` — void-marker type (0-byte payload)
- `off_A64A8` — 1-byte type (uint8/bool)

### Loc2 — WirelessCarplayTransportComponent (top-level Param ID 21)

Final top-level emit at `0x519C4`:
```c
sub_48370(n, 21);   // Param ID 21 = WirelessCarplayTransportComponent
```

Subparams before the close:
```c
sub_4A400(v777, 2);   v777[0] = &off_A64C8;  sub_48370(v777, 0);   // 2-byte id
sub_4A400(v779, 32);  v779[0] = &off_A6468;  sub_48370(v779, 1);   // UUID
sub_4A400(v781, 0);   v781[0] = &off_A64E8;  sub_48370(v781, 17);  ← THEMEASSETS (void)
sub_4A400(v783, 0);   v783[0] = &off_A64E8;  sub_48370(v783, 18);  // void flag
sub_4A400(v785, 0);   v785[0] = &off_A64E8;  sub_48370(v785, 20);  // void flag
```

Notable: in WirelessCarplay path, subparam 17 has payload size **0**
(pure void marker, vtable A64E8). In USBDevice path, subparam 17 has
payload size 4 bytes (vtable A6508) but iAP2MsgIsDataVoid still returns
true when the subparam is emitted without actual content.

### Wire bytes

iAP2 subparam raw encoding (big-endian):

```
[16-bit length][16-bit ID][payload...]
```

Subparam 17 as "void" marker → just header, 4 bytes total:

```
0x00 0x04 0x00 0x11
```

Inserted as part of a larger ParamID 20 or 21 payload inside the
IdentificationInformation message (MsgID 0xEA00).

## MHI3 vs MU1316 iAP2 diff

### File layout

MHI3:
- `libesoiap2.so` — 629 KB, ARM64, dedicated iAP2 library with full
  C++ namespace `iap2::`
- Includes `CMessageParameterWirelessCarplayTransportComponent`,
  `CMessageParameterUSBDeviceTransportComponent`, etc.
- Has `processWirelessCarPlayUpdate`, `wirelessCarPlayTransportComponents`
  log strings.

MU1316:
- No separate `libesoiap2.so` shipped.
- iAP2 logic is either embedded in dio_manager or present as a
  differently-named library (not yet located).
- `CMessageParameterWirelessCarplayTransportComponent` — **0 matches**
  in any MU1316 binary checked.

### String diff (MHI3 - MU1316 iAP2-relevant)

MHI3 unique:
```
CMessageParameterWirelessCarplayTransportComponent
CMessageParameterWirelessCarplayTransportComponents
processWirelessCarPlayUpdate
wirelessCarPlayTransportComponents=%s
```

## Gates — pass strategy summary

| # | Gate | Where | Current | Fix cost |
|---|------|-------|---------|----------|
| 0 | iOS `_deviceFeatures` bit 0 | iPhone 26.1 | ✅ passes | — |
| 1 | iOS user preferences | iPhone settings | ✅ default | — |
| 2 | Apple assetDisabler | Apple CDN config | ✅ irrelevant without ID | — |
| 3 | iAP2 subparam 17 (themeAssets) | car `libesoiap2` equiv | ❌ MU1316 missing | Medium — hook in iAP2 builder |
| 4 | `clusterAssetIdentifier` | — | ✅ not required | — |
| 5 | `enabledFeatures` in SETUP response | car `libairplay` | ❌ MU1316 missing | High — wrap SessionSetup, CF build |
| 6 | stream `type=111` handler | car `libairplay` | ❌ MU1316 missing | Very high — fake stream + pipeline |

Only passing Gate 3 is enough to see if iOS starts proposing
`altScreen` in the wire SETUP — that's the first diagnostic step before
committing to Gates 5 and 6.

## sourceVersion and cluster assets — **NOT gates**

- **sourceVersion** — iOS uses it purely as analytics telemetry
  (`CRCarKitServiceAgent.mm:2345` sets `SourceVersion` key in Sentry
  metrics). No comparison against minimum version anywhere.
  MHI3 = `"450.14.2"`, MU1316 = `"210.81"` — patching doesn't help.
- **clusterAssetIdentifier** — only used if car opts into themed
  cluster via `CARCarPlayServiceMessageStartSession` (wireless only).
  MHI3 never sends it. Not required.

## Actionable next steps (ordered)

1. **Locate MU1316 iAP2 Identification composer.**
   MU1316 has no `libesoiap2.so` — iAP2 logic may live in
   `dio_manager` or a differently-named library. Need to
   `strings | grep 'IdentMsgComposer\|CMessageParameter\|IdentificationInformation'`
   across all `.so` / binary files in the MU1316 appimg.

2. **Hook the Identification builder to inject subparam 17 (void) into
   ParamID 20.** Two possible approaches:

   - **Wire-level:** hook the function that writes the
     IdentificationInformation buffer. Just before final emit of ParamID
     20, insert 4 bytes `00 04 00 11`, bump the total length by 4.
   - **API-level:** if the iAP2 builder has an add-subparam API,
     construct a void subparam with ID 17 and call it.

3. **Monitor iOS behaviour via Console.app / iOS log.** Look for
   `isIdentifiedForThemeAssets` log line, `supportsThemeAssets: YES`,
   `Device supported features: altScreen|...`.

4. **If iOS starts sending `altScreen` in the proposed feature list →**
   proceed to Gate 5 — hook `AirPlayReceiverSessionSetup` response to
   add `enabledFeatures` CFArray.

5. **If Gate 5 passes and iOS opens a type=111 stream →** proceed to
   Gate 6 — hook the stream-type switch to claim type=111 and route
   data to our own OMX/compositor pipeline.

If Gate 3 passes but Gate 5 doesn't open anything new, we've hit a
deeper design constraint and should re-evaluate.

## Key file/function map

**iOS (for reference):**
- `AirPlaySender.framework/AirPlaySender/Activate.mm:319` — altScreen
  feature-key check
- `AirPlaySender.framework/AirPlaySender/AirPlaySender_07.mm:5928` — car
  SETUP request builder (`apsession_appendControlSetupRequest`)
- `CoreAccessories.framework/CoreAccessories_15.mm:8960` —
  `_parseIdentificationParams_3`; case 17 sets byte +131
- `CoreAccessories.framework/CoreAccessories_19.mm:1512` —
  `iap2_identification_isIdentifiedForThemeAssets`
- `CoreAccessories.framework/ACCExternalAccessory.mm:2020` — capabilities
  bit 21 set
- `CarKit.framework/CarKit.mm:3692` — `CRCarPlayFeaturesAsAirPlayFeatures`
  (uint16 bitmask → CFArray of strings)
- `CarKit.framework/CRVehicleAccessoryManager.mm:530` —
  `setSupportsThemeAssets:(v18 >> 21) & 1`
- `carkitd/CRFeaturesAvailabilityAgent.mm:34` — `_deviceFeatures`
- `carkitd/CRFeaturesAvailabilityAgent.mm:227` —
  `_supportedCarPlayFeaturesForVehicle:` (Ferrite mask gating)

**MHI3 libairplay (for reference):**
- `AirPlayCopyServerInfo @ 0x4F900` — /info response builder
- `AirPlayReceiverSessionSetup @ 0x63390` — SETUP handler with
  HasFeature* checks
- CFString constants 0x115000–0x115480 (altScreen, uiContext,
  cornerMasks, focusTransfer, enabledFeatures)

**MHI3 libesoiap2:**
- `identificationInformation @ 0x4F9A0` — iAP2 Identification composer
- `sub_48370 @ 0x48370` — write 16-bit subparam ID big-endian
- `sub_4A400` — zero-init subparam payload slot
- `sub_4A320` — init subparam builder
- vtables: `off_A6468` (32B), `off_A6508` (4B void), `off_A64C8` (2B),
  `off_A64E8` (0B void), `off_A64A8` (1B)

**MU1316 libairplay 210.81 (our hook target):**
- `AirPlayReceiverSessionSetup @ 0x272B8` — stream-type switch (rejects
  type=111)
- `AirPlayReceiverSessionControl @ 0x28EE0` — only handles modesChanged,
  requestUI, updateFeedback, hidSetInputMode
- `AirPlayReceiverSessionPlatformCopyProperty @ 0x1D6C0` — our existing
  hook point for /displays injection
- `AirPlayCopyServerInfo @ 0x22254` — /info response builder
- sourceVersion rodata 0x9DF2C ("210.81"), bonjour srcvers 0x9E50C

## 2026-04-23 update — MU1316 iAP2 stack architectural blocker

Previous Gate 3 analysis identified "iAP2 subparam 17 emission" as the
target fix. This update documents that the MU1316 iAP2 stack is
fundamentally too old to emit it via any legitimate API.

### MU1316 iAP2 architecture (reversed via dio_manager.i64 + libNmeSDK.so)

- **No dedicated `libesoiap2.so`** on MU1316 (MHI3 has one).
- iAP2 client/service classes (`dio::CIAP2Service`, `dio::CIAP2ServiceCinemo`,
  `dio::CIAP2ServiceCreator`, `dio::CIAP2ClientEventListener`, etc.) are
  statically linked into `dio_manager`.
- Actual iAP2 wire-protocol engine is provided by **Cinemo's IAP SDK**
  accessed through the `ICinemoIAP` COM-style interface declared in
  `libNmeSDK.so` and backed by the Cinemo runtime (embedded in
  dio_manager / loaded dynamically from Cinemo's binary plugins).
- Cinemo wire API methods: `SendIAP2(data,len)`, `ReadIAP2`,
  `SetIdentificationAccepted`, `SetIdentificationRejected`,
  `SendHIDReport`, `RequestAppLaunch`, etc. (full list in libNmeSDK
  strings @ 0x366d9..0x36896).

### Cinemo's iAP2 capability API — only 2 coarse flags

The full set of `CINEMO_METANAME_IAP_SUPPORTS_*` flags available to
dio_manager is:

```
CINEMO_METANAME_IAP_SUPPORTS_IAP2_CONNECTION  (bool)
CINEMO_METANAME_IAP_SUPPORTS_CARPLAY          (bool)
```

Confirmed via IDA string search (`SUPPORTS_CARPLAY|SUPPORTS_WIRELESS|
SUPPORTS_IAP2` regex) on both libNmeSDK.so and dio_manager — total 4
matches, all for just those two flags.

**Missing (relative to modern Cinemo):**
- No `SUPPORTS_WIRELESS_CARPLAY`
- No `SUPPORTS_THEME_ASSETS`
- No `SUPPORTS_ENHANCED_INTEGRATION`
- No `TRANSPORT_COMPONENT` schema
- No `ICinemoIAP_SetWirelessCarPlaySupport` or similar

Other ACCESSORY_CAPS-adjacent metas in libNmeSDK are dio→Cinemo
data-channels (sample rates, HID components, BT MAC, etc.) — not
feature-capability toggles.

### What this rules out

1. No config-flag path to enable Param 21 emission — Cinemo's binary
   simply doesn't contain the code to build `WirelessCarplayTransport
   Component` or the void subparam 17 flag.
2. No "set arbitrary subparam" API in Cinemo — the SDK only exposes
   pre-baked component builders (USB/BT/Location/HID) none of which
   support the 2020+ TransportComponent schema.
3. No drop-in Cinemo SDK upgrade — Cinemo's binaries are proprietary and
   the version shipped with this MU1316 head unit predates the Ferrite
   (CarPlay 4.0) era by years.

### Remaining (infeasible) attack surface

The only way to pass Gate 3 on this hardware is **wire-level byte
injection** at the iAP2 transport layer:

1. Hook `ICinemoIAP::SendIAP2(data, len)` (vtable-based virtual call from
   dio_manager into Cinemo).
2. Detect outgoing iAP2 IdentificationInformation message (MsgID 0xEA00).
3. Parse the existing message, locate the position where the top-level
   param list ends, splice in a synthetic **ParamID 21**
   (WirelessCarplayTransportComponent) with the minimum required
   subparam set: `0` (uint16 id), `1` (32-byte UUID), `17` (void =
   themeAssets), `18` (void), `20` (void).
4. Recompute message total-length header and param-count.
5. Forward the mutated buffer to the real SendIAP2.

Even if this passes Gate 3, Gates 5 (enabledFeatures CFArray in libairplay
SETUP response) and 6 (stream type=111 handler) remain unimplemented in
libairplay 210.81 and require their own substantial hooks.

**Estimated total work:** 80-120 hours of deep reverse + C hook code +
on-device debugging. No guarantee iOS won't impose additional hidden
gates (MFi certificate attestation, feature-version string gating, or
iOS-version-specific checks beyond iOS 26.1).

### Closed — decision rationale

- Architectural mismatch (old Cinemo vs new iOS-26 protocol).
- High implementation cost relative to reward.
- No gain path via normal config/API — must go wire-level for multiple
  protocol layers simultaneously.
- `altScreen` branch marked closed. task #37 completed. task #38 deleted.

Static evidence preserved for future reference: if we ever get a newer
Cinemo iAP2 runtime (e.g., via fw upgrade or replacement head unit),
this document contains every gate location needed to flip altScreen on.
