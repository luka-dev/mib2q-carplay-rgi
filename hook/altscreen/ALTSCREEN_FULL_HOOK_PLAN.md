# AltScreen full flow via STOCK-libairplay hook — implementation plan

Goal: get the iOS-26.1 cluster altScreen video (stream 111) working on STOCK libairplay 210
purely via the LD_PRELOAD hook (path A, user-chosen 2026-08-02).

## Protocol spec (from from-source `Patches/libairplay-mhi2q/altScreen/impl_plan_fromsource.md` + MHI3 RE)
The "classic" cluster altScreen (backward-compatible, works on current iPhones):
1. **/info advertises 2 displays** — 2nd = cluster `{type:111, maxFPS:60, 1920×720 (from-source used
   this; our hook uses cluster res), phys 180×68, distinct uuid, properties:["Instrument Cluster"]}`.
   This 2-display /info IS the altScreen advertisement — there is **NO** features bit
   (`kAirPlayFeature_AltScreen` doesn't exist).
2. iOS then sends an **RTSP SETUP** containing a **stream type 111**.
3. `HasFeatureAltScreen` = a per-session byte the RTSP dispatcher sets from the SETUP dict
   (MHI3 detail; may be internal-only — the 2-display /info is the real gate).
4. HU sets up a **2nd (alt) screen session** for stream 111, decodes its H.264 → cluster.
5. `supportsThemeAssets` (iOS-26.1 CarPlay-Ultra theming) gates only the NEWER theme layer —
   **NOT needed** for cluster nav video.

## The 5 from-source pillars → mapped to stock hook interposes
| pillar | from-source | our hook (stock) | state |
|---|---|---|---|
| P4 2-display /info | `_CopyDisplayDescriptions` appends cluster | `build_displays()` + inject into `AirPlayCopyServerInfo` | **BUILT, gated OFF** (`ALTSCREEN_ADVERT_DISPLAYS`) |
| P1/P3 SETUP case 111 | `case 111 → _AltScreenSetup` clone | `AirPlayReceiverSessionSetup` interpose → `alt_find_111` + `alt_setup` | **BUILT** (dormant until displays advertised) |
| P5 route alt stream | fwd to port 19831 | `ScreenStreamProcessData` interpose routes 111 → altscreen_render :19831 | **BUILT** |
| P2 2nd screen session | `altScreenSession` created+configured | `alt_setup`: `scr_create`(new AirPlayReceiverSessionScreen)+`scr_setup`+`scr_setsec`+socket+thread; `scr_start` in `alt_thread_main` | **BUILT** (CORRECTED — not missing) |

**from-source = a SEPARATE custom lib** (leaked 280.33.8 SDK + mods), NOT stock 210. It is the
PROTOCOL blueprint only; do NOT derive stock ABI from it. Our hook targets stock 210 and its
`AirPlayReceiverSessionScreen_*` exports DO resolve on stock (`alt_resolve syms_ok=1` in the live log).

## Stock SETUP dispatch (VERIFIED by decompile, `AirPlayReceiverSessionSetup@0x272b8`)
Per-stream loop over `a2["streams"]`, `type = stream[unk_9E938]`:
```
type < 100         → "Unsupported stream type" → SKIP (no error)
type 100 / 101     → audio Main / Alt
type == 110        → sub_26F9C (_ScreenSetup, hardcodes 110)
type else (incl 111) → "Unsupported stream type: %d" → SKIP (NO -6740!)
```
**KEY:** stock silently *skips* type 111 — it does NOT itself return -6740. So the -6740 we saw
with ADVERT_DISPLAYS on originates ELSEWHERE (candidates below).

## `_ScreenSetup` (sub_26F9C) — the ONLY -6740 site found so far
Returns **-6740 when its session arg `a1` is NULL** (`if(!a1){v6=-6740;...}`). Otherwise sets up
the (single) screen receiver on a socket, hardcodes response `type=110`.
→ **-6740 hypothesis:** our `alt_setup` (clone of _ScreenSetup for 111) or the 111 path calls the
screen-session infra with a **null/absent ALT screen session** (stock has no 2nd session = P2 gap).
i.e. the crux missing pillar (P2) is exactly what produces -6740.

## The real blocker = P2 (2nd screen session)
Stock has exactly one `AirPlayReceiverSessionScreen` object at `session+5140`. The 111 stream needs a
SECOND, independent screen session (its own socket, decoder, state) — from-source created it in C
(`altScreenSession` + `boundScreen` + `SetAltScreen`). As a hook on stock we must **synthesize a 2nd
screen session** (via the exported `AirPlayReceiverSessionScreen_*` ctors) or drive `alt_setup` with a
valid alt-session object. Until then, advertising displays → 111 SETUP → alt path hits null → -6740.

## Open questions (need to resolve to finish)
1. **Exact -6740 origin with ADVERT_DISPLAYS on** — confirm it's the null-alt-session in `alt_setup`,
   not the displays builder. Needs either the LIVE -6740 slog (unit offline) OR reversing `alt_setup`
   + the screen-session ctors to see where null propagates.
2. **How to create a 2nd `AirPlayReceiverSessionScreen`** from the hook — which exported ctor/config
   calls, and where to store it (session struct has no alt slot; keep it hook-side keyed by session).
3. Does iOS actually emit the 111 SETUP once it sees the 2-display /info on stock (no HasFeatureAltScreen
   byte)? from-source plan lists this as UNVALIDATED (needs live iPhone / Apple Maps to avoid -16721).

## Status of RE this session (2026-08-02)
- Confirmed stock libairplay 210 has ZERO cluster/gauge/altScreen strings — no scaffolding.
- Decompiled `AirPlayReceiverSessionSetup@0x272b8` (dispatch) + `sub_26F9C@0x26f9c` (_ScreenSetup).
- Established the hook already has P4/P1/P3/P5; **P2 (2nd screen session) is the missing pillar** and
  the likely -6740 source.

## AUDIT: MIB2Q stock (210, arm32) vs MIB3 (450, aarch64) — 2026-08-02
MIB3 has native altScreen; used as the reference.
1. **Stock 210 = ZERO altScreen** (re-confirmed): no `HasFeatureAltScreen` / `CopyAltScreenURLs` /
   `AltScreenDictCreate` / "Instrument Cluster". Only the single 110 screen. Hook injects the whole layer.
2. **MIB3 altScreen exports:**
   - `AirPlayReceiverSessionHasFeatureAltScreen(session)` = `*(u8*)(session[+24] + 295)` — per-session
     feature byte (ctx+0x127), set by the RTSP dispatcher from the SETUP dict.
   - `AirPlayReceiverSessionCopyAltScreenURLs(session)` — returns the stored altScreen URLs.
   - `AirPlayAltScreenDictCreate(...)` keys (resolved from CFLite consts): **uuid, type=111, maxFPS,
     widthPixels, heightPixels, widthPhysical, heightPhysical, initialURL** — an altScreen URL/scene dict.
3. **KEY architectural gap:** `HasFeatureAltScreen` + `CopyAltScreenURLs` have NO in-lib code xrefs → they
   are called by **dio_manager (external)**. In MIB3 dio is altScreen-aware (queries the feature, fetches
   URLs). **Our stock dio 210 does neither** → likely the -6740 origin (stock dio can't participate in the
   negotiation the phone starts once it sees the cluster display).
4. **Geometry keys MATCH**: our `build_cluster_dict` {type=111, maxFPS, widthPixels, heightPixels,
   widthPhysical, heightPhysical, uuid} == MIB3 `AltScreenDictCreate` keys exactly. Advert schema correct.
5. **"Instrument Cluster"/"properties" gate is PHONE-side (CarKit)**, not in MIB3 libairplay — confirms our
   comment; our /info properties approach is derived from the authoritative consumer.
6. **NEW — `initialURL`**: MIB3's altScreen dict carries an `initialURL` (the scene URL). Our altScreenURLs
   handling (SetProperty ACK, store opaque) neither produces nor echoes initialURL dicts. If CarKit expects
   the HU to expose scene URLs (CopyAltScreenURLs / initialURL), the hook must emulate that too.

### Audit implication for the -6740
Two candidate models the on-unit log will disambiguate:
- **(our current) push-111-in-SETUP**: phone offers stream 111 in the RTSP SETUP → `alt_find_111`+`alt_setup`.
- **(MIB3) HasFeatureAltScreen + CopyAltScreenURLs (dio-driven)**: phone sets the feature byte + the HU
  exposes scene URLs; the 111 stream comes AFTER that handshake. Stock dio can't do this → -6740.
The `SETUP: 111 stream offered | no 111 stream in request` log line tells us which model iOS uses on stock.

## LIVE TEST 2026-08-02 (iOS 26.1 iPhone, ADVERT_DISPLAYS on) + CarKit decompile
Authoritative phone-side source found: `Refferences/iPhone18-3_26.1_23B85_Restore` = full dyld cache
decompiled to `.mm`. CarKit = `.../CarKit.framework/CarKit/CARDisplayInfo.mm`, `CARSessionConfiguration.mm`,
`CARStreamInfo.mm`.

**CarKit /info displays schema (CARDisplayInfo::initWithPhysicalScreenDictionary) — each REQUIRED or the
display returns nil → phone rejects → teardown loop:**
- `widthPixels`,`heightPixels` (CRSizeFromAirPlayDictionary) — required.
- `widthPhysical`,`heightPhysical` (CRPhysicalSizeFromAirPlayDictionary) — required.
- **`uid`** (string) — required. (Stock 210 + our cluster both used `uuid` → THE reject. FIXED.)
- `properties` (array) — optional; `bs_containsObjectPassingTest` for "Instrument Cluster" sets
  `hasGaugeClusterScreen`. (our gate — correct.)
- **`initialVideoStreams`** (present NSArray) — required (missing → nil). Stock main lacks it. FIXED
  (added to cluster + `main_disp_modernize` adds uid+initialVideoStreams to the stock main).
- Stock `CopyDisplaysInfo` main dict keys (verified): edid, features, maxFPS, widthPhysical,
  heightPhysical, widthPixels, heightPixels, **uuid** (legacy), NO initialVideoStreams.

**Result of the fix (uid + initialVideoStreams on both displays):** BIG progress —
- Before: 8s loop, main SCREEN never started (NONE→STOPPED).
- After: main decoder **`ScreenStreamStart Decoder started successfully`**, screen INITIALIZING→READY,
  session **Dur=23s** (3× longer). CarKit now ACCEPTS the displays + starts the main video.

**Remaining blocker:** session still drops after ~23s. Phone does NOT do the altScreen handshake — no
`altScreenURLs` SetProperty, no CopyAltScreen; it only polls `Property: features` repeatedly, then
`kTimeoutErr -6722` / `Interface carplay0 is not running; killing session` / `kConnectionErr -6753`.
So with the cluster advertised (hasGaugeClusterScreen=true) the phone expects the cluster serviced,
doesn't get it, times out ~23s, resets the USB link. → Need to understand why the phone polls "features"
and never starts the cluster handshake (a features bit? a missing /info field? the dio-driven
CopyAltScreenURLs the stock dio can't answer?).

## CORRECTION + more live tests 2026-08-02 (later)
- The "8m30s stable session" was **baseline (ADVERT OFF)** — mis-attributed; the empty-streams fix was
  NOT yet live then (user caught this). Retested cleanly:
- **Advertising the cluster ALWAYS → teardown**, and initialVideoStreams content changes the *speed*:
  | displays advert | initialVideoStreams | result |
  |---|---|---|
  | none (baseline) | — | STABLE (8m+) |
  | main+cluster | 1 bogus-uuid stream | Dur=**23s**, main decoder DID start, then kTimeoutErr drop |
  | main+cluster | empty [] | Dur=**0s** instant teardown (kConnectionErr) — worse |
- In ALL advert cases the phone **never initiates the altScreen handshake**: no `altScreenURLs`
  SetProperty, no 111 SETUP, `altscreen_render` stays VIDEO_OFF. It just polls `Property: features`
  then times out (~23s best case) → `Interface carplay0 is not running` / kTimeoutErr / kConnectionErr.

### The real remaining blocker (phone-side, from AirPlaySender/Activate.mm)
`carEndpoint_activateInternal`: the phone sets the altScreen flag `*(v44+63)=1` **only if the NEGOTIATED
FeatureKey list contains "altScreen"** (line 319; the list is the negotiation of HU extendedFeatures ×
phone-supported, line 313 "FeatureKey list negotiated"). THEN (line 623) `if(altScreenFlag)` →
`APCarPlay_CRFetchInstrumentClusterURLs` → if empty `-16721`, else `CFDictionarySetValue(dict,
"altScreenURLs", …)` (line 660) sent to the HU.
So the phone is NOT reaching the altScreenURLs send → either "altScreen" is dropped in FeatureKey
negotiation, or CRFetch returns empty.  → NEXT: reverse the FeatureKey negotiation (Activate.mm
~250-313, `carEndpoint_createSetupRequestFeatureList` + `carEndpoint_overrideFeatureKeyWithPrefValue`)
to see what gates "altScreen" surviving — likely a required companion key (e.g. "uiSync"/
"viewAreas"/"uiContext") or a features-bit/pref, OR the 2-display /info still isn't shaped how the
negotiation expects.  Also: the 23s-drop-after-main-starts suggests once the cluster is advertised the
phone WAITS for the cluster and gives up — so the handshake must complete, not just be advertised.

## FeatureKey negotiation reverse (phone AirPlaySender/Activate.mm + APCarPlay.mm) 2026-08-02
- `carEndpoint_createSetupRequestFeatureList` builds the PROPOSED feature list `v19` from
  **`APCarPlay_FetchSessionFeatureKeysWithIdentifier(pairedVehicleIdentifier)`** (async call into
  **carkitd**, keyed by the PAIRED-VEHICLE identifier) + `carEndpoint_overrideFeatureKeyWithPrefValue`
  for a fixed set (fileTransfer, vehicleStateProtocol, logTransfer, iAPChannel, **uiSync**, mainBuffered,
  **hevc**, sessionManagement, videoPlayback, enhancedSiri).  **"altScreen" is NOT in the pref-override
  set** → it can only enter the proposed list via the per-vehicle session feature keys from carkitd.
- `carEndpoint_activateInternal` then does the negotiation vtable call `v38(endpoint, proposed=v19, cfg,
  &negotiated=v246)` (Activate.mm:275) → `if (negotiated contains "altScreen") altScreenFlag=1`
  (line 319) → `if(altScreenFlag)` → `CRFetchInstrumentClusterURLs` → send `altScreenURLs`.
- Observed: the phone never sets the flag → **"altScreen" is not in the proposed/negotiated list.**

### Leading hypothesis (testable): per-vehicle feature keys are cached at PAIRING
`FetchSessionFeatureKeys**WithIdentifier**(pairedVehicleIdentifier)` strongly implies a stored per-paired-
vehicle capability record in carkitd.  Our unit was paired with the phone BEFORE altScreen was advertised,
so carkitd's cached feature keys for this vehicle likely lack "altScreen" → the phone never proposes it.
**NEXT TEST: forget the car on the iPhone (Settings→General→CarPlay→forget), then re-pair WITH
ALTSCREEN_ADVERT_DISPLAYS=1 on**, so carkitd captures the altScreen capability fresh at pairing.
If that makes "altScreen" survive negotiation → handshake proceeds (CRFetch → altScreenURLs → 111).
If not → carkitd may gate altScreen on a specific /info features BIT / field we don't yet emit; then
reverse carkitd's vehicle-capability ingest (what /info key promotes "altScreen" into the session keys).

Also captured (phone side): `CARSessionConfiguration` reads `altScreenSuggestUIURLs`; `CARSession`
→ `CARHandleSuggestUI(clusterSessionURLs, altScreenSuggestUIURLs, …)`; `hasGaugeClusterScreen` from displays.

## LIVE 2026-08-02 (re-pair + structure): "car won't ADD" with displays advert
- User re-paired the phone WITH ADVERT on → **the car did NOT add to CarPlay at all** (pairing/negotiation
  fails), not just "altScreen handshake skipped".  So the 2-display /info breaks the base CarPlay
  negotiation.  Re-pair did NOT make the phone propose "altScreen" → the per-vehicle-cache hypothesis is
  unconfirmed / not the (only) gate.
- `CARSessionConfiguration::initWithSessionStatusOptions:propertySupplier:` structure (iOS 26.1):
  - `_displays` built from **`displayPanels`** key (physical panels), FALLBACK to supplier
    `MEMORY[0x1E6962240]` (the AirPlay /info displays) when displayPanels absent.  `hasGaugeClusterScreen`
    = `_displays contains "Instrument Cluster"`.
  - **`displays`** key read SEPARATELY (logical screens) + processed with car capabilities.
  - `displayPlugins` key also read.
  - `if (options & 4)` → **`[CRCarPlayCapabilities fetchCarCapabilitiesWithIdentifier:]`** — per-vehicle
    capabilities (certificate/identifier based).  `carkitd` `fetchScaledDisplaysWithCertificateSerialNumber`
    reads saved scale from `vehicleStore` keyed by **certificate serial number**.
- So there are THREE coupled /info keys (displayPanels/displays/displayPlugins) — we only inject `displays`.
  Likely the advert must provide a consistent panel+logical-screen set, AND altScreen/scaled-cluster may be
  gated by the vehicle CERTIFICATE (Apple side) — unresolved which is decisive.

### Honest status: hook path is at a wall pending a REFERENCE
Blindly reversing the phone's full multi-display config contract is expensive and may hit a certificate
gate.  Highest-value unblock = a REAL altScreen `/info` capture from a working cluster-CarPlay setup (any
HU) → replicate its exact displayPanels/displays/displayPlugins byte-shape.  Ask the user if such a
capture / a known working MHI2Q altScreen mod exists.

## REFERENCE FOUND: colleague's MHI3 altScreen RE (`Patches/mhi3-altscreen/`) — synthesis 2026-08-02
The colleague fully decoded the iOS-26.1 protocol (`findings/ios_altscreen_protocol_decoded.md`,
`MASTER_KNOWLEDGE.md`, `README.md`).  Verified against our CarKit decompile:
- **Our "car won't ADD" = iOS display-count gate** (Activate.mm:844-859): altScreen-flag ⇒ require ≥2
  displays; NO altScreen-flag ⇒ require ≤1.  We advertise 2 displays but the flag is off → 2 rejected.
- **The altScreen flag is set only if the NEGOTIATED feature list contains "altScreen"** (Activate.mm:319).
  VERIFIED: iOS reads the HU `/info` **`extendedFeatures`** key (our key is CORRECT), but
  **`extendedFeatures=[altScreen]` ALONE does NOT set the flag** — proof: baseline (extendedFeatures on,
  1 display) works, which the ≥2 gate would forbid if the flag were set.  The flag needs iOS to *propose*
  "altScreen" (from **carkitd per-paired-vehicle session feature keys**) AND the HU to confirm it in the
  RTSP feature negotiation (`v38(endpoint, proposed, cfg, &negotiated)` at Activate.mm:277).
- On MHI3 the colleague got the flag by patching **`AirPlayReceiverSessionHasFeatureAltScreen @0x58CE0 →
  return 1`** (libairplay writes session byte +0x127 from the RTSP SETUP `enabledFeatures`; the getter
  reads +0x127).  **Stock 210 has NO `HasFeatureAltScreen`** (0 altScreen symbols) → that lever doesn't
  exist for us.  The colleague also used native `createMainDisplaysDictionary`/`addAltDisplay` (SDK-450) to
  build the correct 2-display dict — **stock 210 lacks those too**; we must hand-build (and match exactly).
- **THE FINAL WALL (universal, unsolved by anyone incl. the colleague on MHI3):** after iOS sends
  `altScreenURLs` in the activation request, the HU's libairplay must **process it + ACK within ~250 ms**
  or iOS times out → disconnect (our teardown/`kConnectionErr`/`kTimeoutErr`).  libairplay has the GETTER
  `AirPlayReceiverSessionCopyAltScreenURLs` (reads session+0x18+0x128) but **NO writer/command handler**
  for the incoming altScreenURLs → no ACK.  Finding/adding that handler is the crux for everyone.

### Honest assessment
Full cluster altScreen VIDEO is **not achieved by anyone yet** — the colleague reached
`[AirPlay] ### Alt Screen supported: Y` on MHI3 (native symbols + patches) but is stuck at the
altScreenURLs-ACK wall.  Stock MHI2Q 210 is *harder*: it lacks the native altScreen symbols the MHI3
approach relies on (HasFeatureAltScreen, createMainDisplaysDictionary, addAltDisplay), AND iOS won't even
set the altScreen flag for our vehicle (carkitd doesn't propose it; re-pair didn't change that) → the
display-count gate blocks us before the altScreenURLs stage.  This is a large multi-layer RE effort with a
likely phone-side (carkitd/certificate) gate we cannot force from the HU.

## BREAKTHROUGH 2026-08-02: car ADDS with displayPanels (past the hardest gate)
- Root of "car won't add" = the iOS display-count gate (Activate.mm:822 `v88 = /info["displays"]`,
  `Count = CFArrayGetCount(v88)`; :844-878 altScreen-flag⇒Count≥2 else Count≤1).  Count is the RAW
  "displays" array (unvalidated), so it's purely gated on the altScreen FLAG.
- **The altScreen flag is set only if the phone recognizes the cluster and proposes "altScreen".**
  iOS reads BOTH `/info["displays"]` (LOGICAL, `initWithLogicalScreenDictionary`) AND
  `/info["displayPanels"]` (PHYSICAL, `initWithPhysicalScreenDictionary`); `CARSessionConfiguration`
  builds `_displays` (→ `hasGaugeClusterScreen = _displays contains "Instrument Cluster"`) from the
  PHYSICAL **displayPanels**.  We were only injecting `displays`.
- **FIX THAT WORKED: also inject `displayPanels=[main,cluster]` in our physical schema (uid, widthPixels,
  heightPixels, widthPhysical, heightPhysical, properties:["Instrument Cluster"], initialVideoStreams).
  → the phone now sets the altScreen flag → the 2-display count passes → THE CAR ADDS to CarPlay (first
  time ever on 210 stock via hook).**  This is further than the colleague's documented 210 state.
- REMAINING gate (0s session teardown): after the car adds, each connect does
  `setUpStreams → startSession(noErr) → _requestProcessTearDown → Dur=0s` — the PHONE tears down instantly
  after start.  NOT -16721 (active Apple Maps navigation didn't change it) and NOT the HU (HU logs noErr).
  Cause: our `/info["displays"]` holds PHYSICAL-shaped dicts, but "displays" is parsed as LOGICAL screens
  (`initWithLogicalScreenDictionary`: reads pixel/physical size from SUB-DICTS keyed by CFString consts
  `MEMORY[0x1E6962440]`/`[0x1E6962438]` (unresolved external constants), plus `ScreenID`, streams) →
  invalid logical screens → the phone rejects the session at start.
- NEXT: build PROPER logical screens for `/info["displays"]` (2 of them: main + cluster) with the size
  sub-dict schema + ScreenID + streams, while `displayPanels` keeps the physical schema.  Need to resolve
  the two size-key CFString constants (0x1E6962440 pixel / 0x1E6962438 physical) — codex task kua43ygrf is
  reversing this.  Once "displays" logical screens are valid, the 0s teardown should clear and the phone
  should proceed to the altScreenURLs / 111 stage.

## Logical-screen schema (for /info["displays"]) — 2026-08-02, nearly complete
`CARDisplayInfo::initWithLogicalScreenDictionary` (the parser for each `/info["displays"]` element)
REQUIRES (else returns nil → phone rejects session at start → the 0s teardown):
1. a CGSize sub-dict at key `*MEMORY[0x1E6962440]` (PIXEL size).
2. a CGSize sub-dict at key `*MEMORY[0x1E6962438]` (PHYSICAL size).
3. `ScreenID` (string).
4. (streams via CARStreamInfo::initWithStreamInfoDictionary — optional.)
`CRSizeFromDictionary` == `CGSizeMakeWithDictionaryRepresentation` → the size sub-dict is the standard
CoreGraphics form **`{"Width": <num>, "Height": <num>}`** (CarKit builds such via
`CGSizeCreateDictionaryRepresentation`, e.g. under `"OverrideCanvasSize"`, `"preferredUIScale"`).
`/info["displays"]` MUST be present (Activate.mm:823 `if(!v88) → -16720`), so we can't drop it.
**BLOCKER:** the two outer CFString key names (`0x1E6962440` pixel / `0x1E6962438` physical) are external
constants not resolvable from the decompiled `.mm` (no symtab).  Need a symbol dump of CarKit / dyld
shared cache, OR resolve via codex.  Once known → build each `displays` element as:
`{ <pixelKey>:{"Width":w,"Height":h}, <physKey>:{"Width":wmm,"Height":hmm}, "ScreenID":"...", streams... }`
(2 elements main+cluster) while `displayPanels` keeps the flat physical schema.  That should clear the 0s
teardown and let the phone proceed to altScreenURLs / 111.

## MHI3 libairplay (450) real display-dict key set — 2026-08-02 (reference for a COMPLETE display)
`strings` of `Patches/mhi3-altscreen/work/libairplay.device.so` (the working iOS-26.1 reference) shows the
display dict is **FLAT** (widthPixels/heightPixels — so "displays" IS parsed physical, our type is right;
the CGSize-sub-dict `initWithLogicalScreenDictionary` is a DIFFERENT path, not "displays" — the logical-key
chase was a detour) and uses these keys:
`widthPixels, heightPixels, widthPhysical, heightPhysical, maxFPS, uuid, type, primaryInputDevice,
primaryHidInterface, cornerMasks, limitedUI, limitedUIElements, nightMode, features, safeArea, viewAreas,
viewAreaStatusBarEdge, viewAreaSupportsFocusTransfer`.
- Our cluster/main dict is a SUBSET — missing `primaryInputDevice`, `primaryHidInterface`, `cornerMasks`,
  `limitedUI(Elements)`, `nightMode`, `viewAreaStatusBarEdge`, `viewAreaSupportsFocusTransfer`.
- MHI3 uses **`uuid`** (not `uid`); it reached iOS "Alt Screen supported: Y" — so 26.1 likely accepts
  `uuid` (our `uid` change may be unnecessary; keep both to be safe). NOTE "Alt Screen supported: Y" is
  set at Activate.mm:319 (feature negotiated) BEFORE the display parse, so it doesn't prove the display
  dict is fully valid.
- **HYPOTHESIS for the 0s post-startSession teardown:** our display dict is incomplete vs a real HU's →
  the phone accepts it enough to ADD the car but rejects/aborts the SESSION at start.  NEXT: add the
  missing fields (primaryInputDevice, primaryHidInterface, cornerMasks, limitedUI/Elements, nightMode,
  viewArea* flags) to `build_cluster_dict` + `main_disp_modernize`, matching MHI3's set, and retest.
- codex task kua43ygrf FAILED (1800s idle timeout) — do the RE inline.

## Next steps
1. Reverse `alt_setup` (hook) + the `AirPlayReceiverSessionScreen_Setup`/ctor chain to find exactly
   what a valid alt-session needs; determine if a 2nd session can be synthesized from exported symbols.
2. Build the 2nd-screen-session creation in the hook (P2).
3. Enable `ALTSCREEN_ADVERT_DISPLAYS`; test on-unit with Apple Maps navigating (needs unit online).
