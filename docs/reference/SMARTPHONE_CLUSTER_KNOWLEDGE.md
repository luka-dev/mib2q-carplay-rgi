# CarPlay/AA → A5 cluster (Smartphone tab) — self-verified knowledge

**Goal:** show smartphone projection (CarPlay/Android Auto) on the **A5 virtual cockpit**
in its own **Smartphone tab** (not by stealing the stock map slot).
**Everything below is verified directly from the firmware** (decompiled Java, native ELF
symbols/strings, live `dmdt` dump), not from prior docs. Sources cited inline.

A5 HU = **MU1316** (`Tools/jxe2jar/out/MU1316-combined-vf/`, native `Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/`).
A5 VC = **AU491**; TT VC = **AU334**; R8 VC = **AU724** (`Firmwares/VC/…/extracted/`).
TT HU = **MU1339** (`Tools/jxe2jar/out/MU1339-lsd-full-final-vf/`).

---

## 1. HU display architecture (who declares / who creates)

### Context list (the "screen list") — declared by ONE component
`DisplayManagerMIB2High.defineContexts()` (LSD, `lsd.jxe`) builds `dc[]` and
`DisplayManager.declareContexts(dc[])` sends it to the native DM.
- `de/audi/tghu/fwhmi/DisplayManager.java:130-131` → `defineContexts()` then `dsiDispMgmt.declareContexts(this.dc)`.
- `DisplayManagerMIB2High.defineContexts()` hardcodes 87 (`kombiType!=4`) / 158 (`==4`) contexts.
- Key contexts: `dc[72]={33}`, `dc[74]={20,102,101,33}`, `dc[76]={58}`, `dc[78]={16,59}`.

### Displayables (surfaces) — created by owner render processes
A displayable = a **QNX Screen window** registered with the native DM.
- `libdm_modMain.so`: `CGLRenderer::registerDisplayable(CDisplayable)`; source = `CSurfaceSource(_screen_window)`; store = RB-tree `int id → SharedPtr<CSurfaceSource>`.
- Owners (verified by strings/paths in `Firmwares/HU/.../extracted/app/`):
  | displayable | owner |
  |---|---|
  | 16 HMI (Software) | LSD/HMI via `libeal.so` |
  | 17 REAR_VIEW_CAM (Overlay) | video overlay |
  | 19 MAPVIEWER, 20 ROUTE_GUIDANCE, **33 KOMBI_MAP_VIEW** (Software) | `app/navigation/libPresentationController.so` (`RdvMapViewer`, `MI_KOMBI_MAP`) |
  | 101/102 (Image, KDK bg) | `DisplayManagerMIB2High.configureDM()` → `createImageDisplayable` (from `987.png`) |
  | CarPlay/projection | `gal` + `libairplay.so` (create screen windows) |

**Rule:** a context can *declare* displayable N, but N only exists at runtime if some owner
creates its screen window. Declared-but-ownerless = dead.

---

## 2. Live A5 `dmdt` dump (ground truth) — `logs/dmtd.log`

`dmdt gd` → **7 displayables only**: 16(HMI 1024×480), 17(rearcam Overlay), 19(1024×436),
20(328×181), **33 KOMBI_MAP_VIEW 1440×455**, 101(328×180), 102(210×153). **No 59, no 58, no 36/44.**

`dmdt gs` → **2 displays**:
- **Main Display / LVDS1 / 1024×480 / context 10** = {16 HMI, 19 MAPVIEWER} → the HU's own **central MMI screen**.
- **Cluster Display / LVDS2 / 1440×542 / context 74** = {20 route, 102, 101, **33** KOMBI_MAP_VIEW} → the HU's **feed to the cluster**. The actual video over MOST = **displayable 33 (stock map)**. The VC draws gauges/tabs itself; the HU only feeds map+widgets.

`dmdt gc` → 87 contexts incl. **`dc[78]={16,59}`** — declared, but **59 has no owner on A5 → never instantiated** ⇒ context 78 is dead. **This is "the беда."**

### A5 is the FPK config, NOT kombiType==4
- `DisplayManagerMIB2High.configureDM()` creates KDK bg 101/102 only `if getSysConst(541)==2` (`DisplayManagerMIB2High.java:193`). A5 HAS 101/102 ⇒ **A5 = sysConst(541)==2 (FPK)**.
- `DisplayManager.setupImageDisplayable` is a no-op only `if getKombiType()==4`. A5 created 101/102 (Image) ⇒ **A5 is NOT kombiType==4** ⇒ **`createImageDisplayable` WORKS on A5**; cluster positioning terminal = **1** (FPK path).
- (Corrects earlier assumption that MU1316=="TT/kombiType4".)

---

## 3. HU→VC video transport = MOST H.264 (NOT plain LVDS)

- HU: `videoencoderservice` = `CASIVideoEncodingSrv` — `setActiveDisplayable(displayID, displayable)`, `setUpdateRate`; output `_most.ts` (MOST transport stream); `CDisplayLink::changeConnection`.
- VC: `libnvtvmr.so` (Tegra) has a full **H.264 decoder** (`TVMRVideoDecoderCreate/Render`, `H264_DEC_DECODE_ENTRY`). (Earlier agent missed this lib.)
- Chain: `switchContext(display4)` → `CContextManager::preContextSwitchHook` → (display has no terminal / MOST) → `CASIMostEncoder::setActiveDisplayable(4, context.displayables[0])` → OMX H.264 → MPEG‑TS → MOST → VC `libnvtvmr` decode → shown in the active VC context.
- **One MOST video channel.** The VC composites that single decoded video into whatever tab/context is active. VC state names `LVDS_Smartphone`/`LVDS_NavMap` are the layer names (legacy "LVDS"), fed by the decoded MOST video.

---

## 4. VC (cluster) HMI = the "LSD of the cluster"

- **`fds`** = EB GUIDE HMI runtime. **The state machine / view list is compiled into `fds`**, not the KZB.
- `app_fs/dp/dp_monitor.cfg` = **schema only** (items/states/views/events/enums).
- `gss_ipc` = BAP → dp gateway (writes dp items from BAP over SPI).
- `mostcoreapp` = MOST NetworkService (ring/SSO/coding) — **not** video/display.
- `libnvtvmr.so` = H.264 decode. `kdgui_2dmgr_client` / `libeal` = Kanzi render.
- A5/TT/R8 VC firmware **identical** context enum: 103 `eHMIDisplayContextID*` (0x0–0xef). Same for `eHMIApplicationContextIDSmartphone/eHMIDisplayContextIDSmartphone 0x75`.

### Smartphone tab (context 0x75) in the VC — verified in `app_decompressed.img`
- Display path: `state LVDS_Smartphone 0x89`, `state SV_LVDS_Smartphone 0x8a`, `view SV_LVDS_Smartphone 0x54` (+ `_NA_Init/_NA_Off` = no phone).
- Tab entry: `state MainWizard 0x72`, `view SV_MainWizard 0x49`; label `SV_MainWizard_Smartphone_labelText 0x3000025`.
- Activation event: `INTERN_ContentActivate_Smartphone 0x146`.
- **Enable = runtime status from HU:** `MMI_Status_Stat_Smartphone 0x20002e6` (Integer, set via BAP). No smartphone *availability* coding item exists — the tab is gated purely on this HU status. Not-connected → `INTERN_Popup_NoSmartphoneConnected 0x181` / `SV_LVDS_Smartphone_NA_*`.

### The coding item `CODING_Timer_tSammelReiter_SmartPhoneActive`
- **It is a TIMER (`Long`, ms), NOT an enable flag.** Addr `0x20000d4` (TT) / `0x20000c8` (A5).
- One of three "Sammelreiter" (collective-tab) hold timers: `tSammelReiter 0x…d2` (default), `tSammelReiter_MMI_Off 0x…d3`, `tSammelReiter_SmartPhoneActive 0x…d4` (while a phone is projecting).
- Meaning: how long the collective tab-bar stays expanded when smartphone is active (UX auto-collapse timing). **Does not turn the tab on.** (Corrects an earlier claim.)
- Related timers: `CODING_Timer_tdLVDSLock 0x…da`, `CODING_Tabbar_tabAnimationDuration 0x…c7`.

### HU↔VC display-status sync
- `DSIKombiSync` (v2.11.0): `ATTR_KOMBIMESSAGESTATEDISPLAYSTATUS`, flags `DISPLAYSTATUS_LVDS_DM_ACTIVE=8192` (LVDS video/DM shown), `LVDS_HMI_ACTIVE=4096`, `PROTOCOL_DM_OK=2048`, `PROTOCOL_HMI_OK=1024`.
- `DSICarKombi` (v2.11.30): `DCDisplay1/2/3MainSelection` = `DCMainItems` = per-zone content — but `DCMainItems` has **no smartphone/carplay value** (only navigation, audioMedia, telephone, nightvision…). So the Smartphone tab is NOT driven via the CarKombi preset.
- HU-side cluster HMI sync framework: `de/audi/atip/mmicombi/` (`IMMICombiScreen`, `IMMICombiScreenChangeManager`, `MMICombiDisplayExchangePacket`, `MMICombiDisplayStatus`). Present in MU1316 (framework), but not wired to push the smartphone context.

---

## 5. Why it doesn't work on A5 (gap analysis)

The VC (A5) is fully capable — identical to TT, has the Smartphone tab + LVDS_Smartphone +
H.264 decode, waiting only for HU status. **The entire gap is HU-side (MU1316):**
1. **No projection displayable.** `gal`/`libairplay` composite CarPlay into the central HMI (16), not as a separate cluster displayable ⇒ 59 never created ⇒ nothing to send.
2. **No terminal-mode → cluster integration.** MU1316 terminal-mode registers only `evohigh` (central) screens; **0 `mmikombi`** dirs anywhere. TT (MU1339) has `evohighmmikombi` — the exact diff is `terminalmode/hmi/Activator.java`: `evohigh`/`"EvoHigh"` (MU1316) vs `evohighmmikombi`/`"EvoHighMMIKombi"` (MU1339), via `System.getProperty(PARAMETER_VARIANT,…)`.
3. **No HU report of smartphone status to VC.** Nothing sets `MMI_Status_Stat_Smartphone` on the cluster (`CoMaCarplayHandler` sends nothing to kombi).
4. **No context switch to 0x75.** Nothing drives `IMMICombiScreenChangeManager` to the Smartphone context.

Result: A5 has one active LVDS tab (Navi), so anything `setActiveDisplayable(4,X)` lands in the **Navi tab**, never Smartphone.

---

## 6. What it would take (the full chain)

To show CarPlay in the A5 Smartphone tab without touching the map:
1. **HU: create a separate projection displayable** (owner renderer that opens a QNX screen
   window with a new id, e.g. reuse 59). Not by rendering into 33. Template = how
   `libPresentationController` opens the window for 33.
2. **HU: switch the cluster to a context whose displayables[0] = that new id** →
   `preContextSwitchHook` → `setActiveDisplayable(4, newId)` → MOST → VC.
3. **HU: report smartphone active** → set `MMI_Status_Stat_Smartphone` on the VC via BAP →
   VC raises the Smartphone tab in MainWizard.
4. **HU: switch the VC's active display context to `Smartphone 0x75`** via
   `IMMICombiScreenChangeManager` (the mmikombi terminal-mode path, present in MU1339).
5. **HU: set `DSIKombiSync DISPLAYSTATUS_LVDS_DM_ACTIVE`** so the VC shows the video.
6. **VC: nothing to code** for enable (tab is runtime-gated). Optionally tune
   `CODING_Timer_tSammelReiter_SmartPhoneActive` (hold time) — not required.

All HU steps map to what `evohighmmikombi` terminal-mode (MU1339) does and MU1316 lacks.

### Reversibility
All HU changes are LSD **classpath-override jars** (like `aa_split_center`) → reversible
(remove jar = stock). VC coding is UDS-reversible. No irreversible flashing required for the
HU experiments.

---

## 7. Open verification items (next)
- `libdm_modMain::CContextManager::preContextSwitchHook` — confirm it takes `displayables[0]` with no validation/gating (would let a new id go).
- Who on HU (which BAP LSG/FktId) can set `MMI_Status_Stat_Smartphone` on the VC, and why MU1316 doesn't.
- `IMMICombiScreenChangeManager` send path — the exact "activate context N" call to insert 0x75.
- `gal`/`libairplay`: do they ever open a *separate* projection screen window (any id), or only composite into HMI 16.
- Any hidden smartphone-availability bit inside a packed VC coding `Byte_xx` (vs a standalone item).
