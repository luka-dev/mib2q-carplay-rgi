# AltScreen — from-source libairplay implementation plan (REOPENED 2026-07-11)

## STATUS 2026-07-11: P1-P5 IMPLEMENTED + COMPILES (175/175, 58/58 contract intact, 0 new hw deps)
All edits landed in `vendor/carplay-sdk/Sources/` + `src/screenstream_sw.c`:
- P1 `kAirPlayStreamType_AltScreen 111` + ToString (AirPlayCommon.h); SETUP `case 111 → _AltScreenSetup` (Session.c).
- P2 `altScreen*` slots (Priv.h:453); alt session created + configured (Session.c:606) + `SetAltScreen(true,NULL)`.
- P3 `_AltScreen{Setup,TearDown,Start,Thread}` cloned from `_Screen*` (Session.c ~4705+); wired into SETUP switch, start-hook (IsValidSocket altScreenSock), teardown switch (case 111), both finalize paths. NTP callbacks shared.
- P4 2nd cluster display appended in `_CopyDisplayDescriptions` (AirPlayReceiverPOSIX.c:1378): `{name,type:111,maxFPS:60,1920×720,phys 180×68, distinct uuid, properties:["Instrument Cluster"]}`.
- P5 `boundScreen`+`altScreen` fields + `AirPlayReceiverSessionScreen_SetAltScreen(session,bool,void*)` setter (Screen.c/.h); StartSession binds `boundScreen ?: ScreenCopyMain()` + tags stream `CFSTR("AltScreen")`; Finalize releases boundScreen. `screenstream_sw.c`: alt stream forwards to **SS_FWD_PORT_ALT (default 19831)** so main(19830)+alt don't mix.
- Feature-advertisement (net-new #3): RESOLVED = P4. `AirPlayGetFeatures@Server.c:868` has NO altScreen bit (`kAirPlayFeature_AltScreen` doesn't exist) — confirming MHI3 RE: altScreen advertised via the 2-display /info, not a features bit. Nothing to add there.
- GOTCHA: whole tree (Session.c / POSIX.c) is Non-ISO extended-ASCII → default grep silently emits nothing. Use `LC_ALL=C grep -a`.

### On-unit display binding — NATIVE recipe (reversed from stock, replaces maneuver_render assumption)
Stock `dio::CScreenRender` binds the CarPlay video window to a display like this (MHI2Q 210.81):
- `config()@0x8bcc8`: `screen_create_context` + `screen_create_window` + `screen_create_window_group("dio_manager.airplay.qnx.screen_render")` (OWNS the group) + props: ID_STRING(20), class="media"(7), FORMAT(14), VISIBLE(45)=1, SIZE(40), POSITION(35), GLOBAL_ALPHA(16)=255, BUFFER_SIZE(5). NO display binding here.
- **`selectDisplay()@0x8aa80` = THE display selector**: `screen_get_context_property_iv(ctx,DISPLAY_COUNT=59,&n)` → `..._pv(ctx,DISPLAYS=60,arr)` → pick by index or TYPE (default INTERNAL=30304) → **`screen_set_window_property_pv(win, SCREEN_PROPERTY_DISPLAY=11, &disp[i])`**. That single `disp[i]` is the entire "which display" knob.
- "screenId=4" was a red herring (command-queue depth). Stock has ZERO cluster/second-display code — `selectDisplay` just picks the first attached INTERNAL panel.
- **CAVEAT (vindicates maneuver_render):** `SCREEN_PROPERTY_DISPLAY` only reaches QNX-Screen-enumerated panels. If the MU1316 cluster is driven over MOST/videoencoderservice (separate display-manager — as the LVDS notes indicate), it is NOT in the DISPLAYS array → the native path can't reach it and the alt stream MUST route through the display-manager/MOST encoder (maneuver_render's displayable-20/dmdt approach). One on-unit run tells us which: does Screen enumerate the cluster?

**IMPLEMENTED in `src/screenstream.c` (on-unit OMX backend) 2026-07-11:** rewrote `_MakeWindow` to the faithful native recipe (group + all props) + added `_SelectDisplay` (enumerate DISPLAYS, main=index0/internal, alt=cluster via `SS_CLUSTER_DISPLAY_INDEX` env, default last) + `AltScreen` property handler (`me->altScreen`). screen shim already has all needed symbols. **Compile UNVERIFIED — build VM (192.168.64.16) SSH timed out; re-run `MFI_SW=1 AUDIO_NULL=1 DEV=1 sh scripts/build_all.sh` when VM is up.** If Screen doesn't enumerate the cluster on-unit, fall back to the maneuver_render/MOST route for the alt stream.

### REMAINING before on-unit cluster picture
- On-unit cluster ScreenRef: `SetAltScreen(altScreenSession, true, <clusterScreenRef>)` — create via `ScreenCreate`+`ScreenRegister` with a platformLayer backed by maneuver_render (hardware; ScreenUtils has only ScreenCopyMain, so boundScreen bypasses that). Currently NULL → SW path (forward to 19831) works; on-unit renders to main until wired.
- Smoke test: does iOS offer/negotiate 111 given the 2-display /info? (needs live iPhone or replay). Test on Apple Maps (cluster scene; avoids -16721).



Supersedes the "closed / 80-120h Cinemo injection" verdict in `gate_analysis.md`.
See memory `altscreen_gate3_correction.md` for the full evidence chain.

## Settled facts (high confidence)

1. **altScreen is ALL libairplay. Cinemo/iAP2 is NOT the blocker.** (MHI3 aarch64 RE of
   both `libairplay.so` + `libesoiap2.so`.) The old "Gate 3 = Cinemo subparam-17" verdict
   misidentified iAP2 param 20/21 (which is *Vehicle Information*, not theme-assets).
   - MHI3 `HasFeatureAltScreen@0x58ce0` reads per-session byte `ctx+0x127`, written by the
     **RTSP request dispatcher** (`sub_50990@0x51f1c`) from the incoming RTSP SETUP dict.
     → the altScreen capability exchange is over the **AirPlay/RTSP channel = libairplay**.
   - MHI3 `libesoiap2.so` has ZERO altScreen/themeAssets/enabledFeatures. Only a
     transport-level `carPlay=bool`. MHI2Q Cinemo already advertises `USBHostTransportComponent
     carPlay=true` — that's all iAP2 needs to do.
2. **Classic mechanism is backward-compatible — works on current iPhones** (user-confirmed).
   Basic cluster altScreen (video, stream 111) = /info 2nd display + `HasFeatureAltScreen`
   session byte + stream 111. `supportsThemeAssets` (iOS 26.1) gates only the NEWER
   theme-assets / CarPlay-Ultra dashboard-theming layer — **not needed** for cluster nav.
3. Our leak (280.33.8) has NO altScreen (verified) → implement from scratch on top of the
   leak's existing single-screen stack. MHI3 (SDK 450.14.2) is the logic/schema blueprint
   (aarch64 — reference only, not portable).

## The 5 pillars (what to build, all in libairplay)

| # | Pillar | What it does |
|---|---|---|
| 1 | Stream-type 111 constant + accept in SETUP | add `kAirPlayStreamType_AltScreen 111`; SETUP stream dispatch must handle 111 (compiled MHI2Q `sub_26F9C@0x272b8` hardcodes 110 → add the case) |
| 2 | 2nd screen session slot | mirror `screenSession/screenSock/screenThread/screenThreadPtr/screenInitialized` (Priv.h:449-453) → `altScreen*` |
| 3 | `_AltScreenSetup` + thread | clone the screen setup + receiver thread; per-stream AES already keys on `streamConnectionID` (no crypto work) |
| 4 | 2-display /info | `displays` array must carry a 2nd (cluster) display dict; iOS 26.1 keys the cluster on `properties` containing `"Instrument Cluster"` (`CARDisplayInfo.mm:409`); ≥2 displays → `hasGaugeClusterScreen` (`CARSessionConfiguration.mm:956`). No `altScreenInfo` key. |
| 5 | Bind alt session to cluster sink | alt `ScreenStream` → cluster `ScreenRef` (`maneuver_render`) instead of `ScreenCopyMain()` |

## Alt-dict schema (MHI3 `AirPlayAltScreenDictCreate@0x5f860`, works on current iPhones)
`{ name, type:111, maxFPS, widthPixels, heightPixels, widthPhysical, heightPhysical, [altScreenURLs] }`
Cosmin's proven call: `(name="Audi AltScreen", 60, 1920, 720, 180, 68, NULL)`.
Distinct UUID from main (stream binds to display by UUID). altScreenURLs = skip in v1
(HU is receiver, not producer; -16721 empty-URLs is app-side: Apple Maps yes, Waze no).

## GOTCHA (cost hours): `AirPlayReceiverSession.c` is non-ISO extended-ASCII (smart quotes in
the Apple license header). Default-locale `grep` treats it as binary and silently emits NOTHING.
**Always `LC_ALL=C grep` on this tree** or you'll conclude functions are "missing" when they're not.

## Mapping DONE (exact insertion points, all `Sources/` unless noted)
Architecture: altScreen = a **parallel SLOT on the existing session** (`screenSession/Sock/Thread`),
NOT a 2nd session. 110 + 111 arrive in the SAME SETUP `streams[]` array, dispatched by `type` in
one switch. `_AddResponseStream@4976` appends → 111 desc coexists automatically.

- SETUP dispatch: **`AirPlayReceiverSessionSetup@1049`** (Session.c), per-`type` switch @1104-1136
  (`case kAirPlayStreamType_Screen`=110 → `_ScreenSetup`). ← add `case 111` @~1130.
- Screen lifecycle (all in-library): `_ScreenSetup@4705` → `_ScreenStart@4811` → `_ScreenThread@4830`
  → `AirPlayReceiverSessionScreen_ProcessFrames@4877`. Teardown: switch @1218-1240 (`case Screen@1229`
  → `_ScreenTearDown@4785`) + finalize @1255. Start hook: `AirPlayReceiverSessionStart@1313-1317`.
- Screen session CREATED at **Session.c:606** (`AirPlayReceiverSessionScreen_Create(&me->screenSession)`)
  + SetEventHandler/Overscan/IFName/TransportType @609-617.
- Screen slots: **Priv.h:447-453** (`screenInitialized/screenSession/screenSock/screenThread/screenThreadPtr`).
  Screen does NOT use `AirPlayAudioStreamContext` (that's audio-only).
- `/info` displays is an **ARRAY** built in **`AirPlayReceiverPOSIX.c` `_CopyDisplayDescriptions@1365`**:
  `CFArrayCreateMutable@1372` → append `CopyDisplaysInfo(screenSession)@1375-1378`. ← append 2nd dict @1378.
- Ephemeral port: `kAirPlayPort_RTPScreen==0` (AirPlayCommon.h:135) → each `ServerSocketOpen` gets a
  distinct port for free; alt path reuses the identical call.

## 5-pillar edits (mechanical clones — full spec in agent report)
- **P1** AirPlayCommon.h: `#define kAirPlayStreamType_AltScreen 111` after :314 + ToString branches
  @322/330; Session.c: `case kAirPlayStreamType_AltScreen: _AltScreenSetup(...)` @~1130.
- **P2** Priv.h after :453: `altScreenInitialized/altScreenSession/altScreenSock/altScreenThread/altScreenThreadPtr`;
  Session.c:606: `AirPlayReceiverSessionScreen_Create(&me->altScreenSession)` + mirror @609-617.
- **P3** clone `_ScreenSetup/_ScreenStart/_ScreenThread/_ScreenTearDown` → `_AltScreen*` (screen*→altScreen*,
  response type=111); wire Start@1317, teardown switch, finalize@1255. AES keys off streamConnectionID — verbatim.
- **P4** AirPlayReceiverPOSIX.c:1378: build+append cluster dict `{name,type:111,maxFPS:60,widthPixels:1920,
  heightPixels:720,widthPhysical:180,heightPhysical:68, uuid:<distinct>, properties:["Instrument Cluster"]}`.
- **P5** Screen.c:714 `ScreenCopyMain()` → bind alt session to a **cluster ScreenRef** (add `boundScreen` field).

## NET-NEW / real work (NOT mechanical — the actual blockers)
1. **Cluster `ScreenRef` (P5) — the one substantial gap.** `ScreenUtils.h` has only `ScreenCopyMain`
   (no by-index/UUID). Must `ScreenCreate`+`ScreenRegister` a cluster screen backed by `maneuver_render`/Display
   Manager and hand it to the alt session. Everything else is cloning.
2. **`"properties"`/`"Instrument Cluster"`** — no key constant exists; literal `CFSTR`.
3. **Feature advertisement (PREREQUISITE, not in P1-P5).** P1-P5 make libairplay *accept/serve* 111, but
   iOS only *offers* it if the device advertises the altScreen feature: **`AirPlayGetFeatures@Server.c:377`**
   + `kAirPlayProperty_ExtendedFeatures@Server.c:362` (this SDK has no `HasFeatureAltScreen` symbol). Set the
   altScreen feature bit here or the new `case 111` never fires. Classic mechanism (backward compatible) — no
   `supportsThemeAssets` needed.

## Order of work
1. **Feature advertisement** (net-new #3) → confirm iOS starts offering a 111 stream.
2. **P1+P2+P3** (accept 111 + parallel slot + clone lifecycle) → reach RTSP RECORD on 111.
3. **P4** (2-display /info) → iOS `_hasGaugeClusterScreen`=true.
4. **Cluster ScreenRef** (net-new #1) + **P5** → picture on the cluster via maneuver_render.
Test on **Apple Maps** (has cluster scene; avoids app-side -16721).
