# Stock Navigation Readiness (MHI2Q): who reports "ready", through what, and what it waits for

*How the HU's stock navigation announces readiness end to end — from `servicemgr` spawning `navStartup`
to the Virtual Cockpit dropping "Initialising navigation system. Please wait..." — including the decoded
run-mode bit word, every handshake timeout, and the signals a boot timeline can be measured with.
Research note, 2026-10-18; static analysis only, no code changes. Companion to
[Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md), which covers the VC/BAP presentation side in depth.*

Firmware: MHI2Q US P5087 / MU1316 (`~/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/`).
IDA databases used: `navStartup.i64` (image base 0x100000), `servicemgrmibhigh.i64` (base 0x100000),
plus strings analysis of `AppStartATF`, `libPresentationController.so`, `libPathfinderApp.so`.
All IDA addresses below are IDB addresses in the respective database.

---

## 📋 Summary

| Question | Answer |
|---|---|
| Who decides "navigation is ready"? | `navStartup` composes a **run-mode bit word** and reports it to `servicemgr` via `servmngt::Application::reportAppRunMode`; `servicemgr` publishes it as the status of DSI domain 6 ("navigation"). |
| What is in the bit word? | 0x100 DR (positioning child), 0x200 COMM (ESO comm), 0x400 NAV (navi engine), 0x800 TTI (traffic), plus derived flags 0x4 = COMM∧NAV and 0x8 = COMM∧NAV∧TTI. 0x1 = "app alive, (re)starting". |
| Which signal ends INITIALIZING on the VC? | The **0x704 report** (DR+COMM+NAV+0x4) at ~26 s: domain 6 ⊇ 0x700 flips the HMI "Navi" component to started (bundled with `AppNavi`/`adaptermanager` activation and the `HMINavi`/`SMNavi` guide module, which proceed in parallel), which lets `getGeneralOperationState()` return NORMAL once the BAP init sequence reaches state 9. The later 0xF0C (+TTI) upgrade is *not* required; the SDS 0x300 dependency is asynchronous and does not gate. |
| What does the native side wait for? | The STARTERREQ handshake with the navi engine (`libPresentationController` NavStarter): map-DB consistency check, FEC check, engine init — each of the three handshakes (startup-counter ack, INITNAV, RUNNAV/PresCon) has a **60 s timeout**; the engine answers READY4NAV / CHECK FAILED / NO DATABASE. |
| Where can it stall for minutes? | Missing/corrupt map SD (`NO DATABASE`), failed DB consistency or FEC check (`CHECK FAILED`, check restarts after 5 s on new FEC list), engine freeze (watchdog → EBNavFreezelog → autorestart, up to 7 tries), SWDL markers on `/mnt/persist`. |

## 🔢 The app run-mode word, fully decoded

navStartup keeps the achieved mask at `*(app+200)`/`+0xC8` and reports it through
`sub_118400` ("Report %x back to ESO", navStartup 0x118400 → `servmngt::Application::reportAppRunMode`,
import 0x104DBC). [decompile]

| Bit | Meaning | Set by |
|---|---|---|
| 0x1 | "app started" placeholder — reported while (re)starting children and before/after restarts | `WorkingLoop` death path (0x1241A8), `ReStartPrescon` (0x121458), autorestart (0x124470) [decompile] |
| 0x4 | **derived: COMM ∧ NAV running** — report sender ORs it when `(mode & 0x600) == 0x600` | `sub_118400` 0x118414–0x118418 [decompile] |
| 0x8 | **derived: COMM ∧ NAV ∧ TTI running** — ORed when `(mode & 0xE00) == 0xE00` | `sub_118400` 0x118430 [decompile] |
| 0x100 | **DR** — dead-reckoning/positioning child (`StartModusDR`, PMon group 1 → `DRController deadrec`) | `StartTransitiveModi` 0x120E1C via `sub_11DB30` [decompile] |
| 0x200 | **COMM** — ESO comm child (`StartModusCOM`, PMon group 0 + IPC accept); seeded when launcher init requests runmode 512 and whenever `system_run_mode == 1` | 0x120E50 via `sub_120B20`; seed at 0x120DE8 and in `initApplication` 0x11BD04 [decompile] |
| 0x400 | **NAV** — the navi engine (`AppStartATF` procname `navi`); set after the STARTERREQ INITNAV/RUNNAV handshake succeeds | 0x120E8C via `sub_11D1C0` [decompile] |
| 0x800 | **TTI** — traffic; requested bit is copied to the achieved mask without a dedicated start call | 0x120CF4 ("Started TTI") [decompile] |

Observed values map cleanly: 0x704 = 0x100|0x200|0x400 + 0x4 (DR, COMM, NAV up); 0xF0C = 0x700|0x800 + 0x4|0x8
(TTI added later). The dsistartup launcher's `init` command passes `apprunmode: 0`
(`dsistartup.json:312-317`, [doc]) and navStartup itself defaults a zero run mode to **512 = 0x200**
(`initApplication` `sub_11BC8C`, 0x11BD04: `if (!a3) a3 = 512` — "ESOComm SystemUpdate AppRunMode now %x");
the same 512 seed applies whenever `system_run_mode == 1`. `sub_1180F0` (navStartup 0x1180F0) extracts
**only** the 0xF00 bits from any request into the "requested procs" mask — low flag bits in requests are
ignored by navStartup. [decompile] (The other apps' apprunmode values in dsistartup.json — 256 speech,
17664 media = 0x4500, 921600/925184 connectivity — are seed masks in the same bit language. [doc])

Dependency ordering is enforced by `StartTransitiveModi` (`sub_120C18`, navStartup 0x120C18):
requesting TTI implies NAV; requesting NAV implies COMM and DR. [decompile]

## ⛓️ The readiness chain, end to end

Numbers in brackets = stage times on the P5092 reference boot sloginfo (same software family). [doc]

1. **[~7 s] HMI idle.** Domain 6 status is 0; the HMI logs `updateDomainState(Nav, 0x0)`. [doc]
2. **[~15 s] HMI requests the domain.** The startup manager calls `startDomain(6, 0x700)` — 0x700 = DR|COMM|NAV,
   matching the `required_state` of the "Navi" component in `hmi_startup.json`. [code] [doc]
   `servicemgr` traces `[startDomain] domain 'navigation' state 0x700`, spawns `/navigation/navStartup`
   per `framework.json:318-338` (`navigationProc`, id 311, `singleapp: "navigation"`, env
   `NAV_MAP_PATHS=/fs/sda0/navDatabase:/mnt/navdb`, `STYLE_DB_PATHS=/mnt/app/navigation`, TCP 21311) and runs
   the dsistartup launcher `on_start` list (`dsistartup.json:309-349`): `init` with `apprunmode: 0`
   (→ navStartup's 512 default), `timeout 30000`, `blocking: false`, then `run`, `timeout 60000`,
   `blocking: false`. `on_restart` (sleep 500 ms → `prockill` → init blocking → run blocking) and
   `restart_param` (`restart_cnt: 3`, `restart_cnt_reset_time: 600`, `on_restart_failed: "kill_process"`)
   bound the retry storm; servicemgr's own job watchdog is `job_final_timeout: 60000`
   (`servicemgr.json`). [doc] Navigation is **not** in `on_startup_normal` — it is started exclusively by the
   `startDomain` request (it appears with apprunmode 512 only in `on_startup_download`). [doc]
3. **[~15–20 s] navStartup boot.** `initApplication` (`sub_11BC8C`, 0x11BC8C) logs
   `ESO Fwk triggering initApplication with runmode = %x sys %x` (runmode defaults to 512) and starts the
   control thread that runs `ESOComm::WorkingLoop` (`sub_123CD8`, 0x123CD8). WorkingLoop:
   `setAppState(1)`; `CleanupCode` (`sub_11E050`) slays leftover `DRController`/`AppStartATF`;
   `fetchSwdlMode` (`sub_1182DC`) stats `/mnt/persist/SWDL_SD1|SWDL_SD2|SWDL_OTA` to pick the map medium
   `/fs/sda0` vs `/fs/sdb0`; PMon children are registered
   (`PMon: ADD /navigation/DRController deadrec …`, `IconProviderApp iconprv`, `PNavApp pnav`,
   `PMon: ADD /navigation/AppStartATF navi <SignalMapper-1.5.cfg>`). [decompile]
4. **DR + COMM first.** `Initial RunRequested Procs %x %x` → `StartTransitiveModi` starts the DR child
   (PMon group 1) and the COMM child (PMon group 0) and accepts the engine IPC
   (`sub_120800`, "ReStartPrescon: IPC OK" path). [decompile]
5. **[~20–26 s] NAV: the STARTERREQ handshake with the engine.** The transport is a **localhost TCP socket**
   between navStartup and the `navi` process — navStartup logs `ESOComm::Accept: Socket accepted. Navi is
   alive.` (`DRC::Accept`/`StartIPCConnection`, `127.0.0.1`), the engine side is `PC_MAIN_STARTER_LAUNCHERCOMM`
   in `libPresentationController` ("StartIPCConnection: Count not connect to navStarter, yet.").
   [code] `StartModusNAV` (`sub_11D1C0`) logs slogf(10001,5) `NAVI: Startup SDK in progress`, then
   `StartupNaviSDK` (`sub_11CD48`):
   - send `STARTERREQ_STARTUP_COUNTER_SEND` (msg 18) with the boot counter, wait **60 000 ms** for
     `STARTERREQ_STARTUP_COUNTER_ACK` (msg 19); value 1 = "Received StarterResponseOK", else result
     `RESETCOUTERLIMITREACHED` (3); timeout → "Did not receive reset counter acknowledge." → `ERROR` (1). [decompile]
   - then `sub_11C758`: send `STARTERREQ_INITNAV` (msg 0), wait **60 000 ms** for the engine response
     (`libPresentationController` NavStarter):
     | Response | Engine log | StartupResult |
     |---|---|---|
     | 0 / 1 | "SDK started up OK." | 0 OK |
     | 17 / 1 | "StartupPresCon: Ready for navigation" | 2 READY4NAV |
     | 20 / 1 | "StartupPresCon: Check has failed" | 4 CHECK FAILED |
     | 21 / 1 | "StartupPresCon: No Database available" | 5 NO DATABASE |
     | other | "Navigation System did not respond OK. cmd = %d dir = %d" | 1 ERROR |
     [decompile]
   - **The engine side** (`libPresentationController.so` carries the NavStarter: `PCStarterImpl::WorkingLoop`,
     `Received STARTERREQ_RUNNAV`, `Send STARTERREQ_INITNAV ldir=`, `SendReady4Nav`, `SendNoDatabase`,
     `SendCheckFailed`, `STARTERREQ_INITNAV: No Database found!`, `STARTERREQ_INITNAV: Navigation not
     enabled!`) — this is where the readiness decision is made, and it is the dominant chunk of the
     15 s→26 s window (AppStartATF EGL init ≈ 21 s in the reference slog). [code] [doc]
     The engine's own gate chain, from its strings: GEM enable switch (`/navigation/FSID_Navi_Enabled` /
     `FSID_Navi_Disabled`, "Navigation not enabled! Shutdown..."), map-path discovery
     (`NAV_MAP_PATHS`, `Not enough map paths found ( ). Try with ScanDir`, persisted SD-scan path list,
     "SDCard: Persisted paths different to SD card. Persist scanned paths and restart navi"), then the
     **DB check** (`PC_DBChecker` channel: "DB on SD card found. Do early check",
     `CheckVersion/Signature/Consistency/ISO` task families, `StartFecCheckTask`/`RunFecCheckTask` FEC
     validation, "DB Check finished successfully with: ", retries: "busy -> retry in 5s", "canceled ->
     retry in 60s", "Late check is not allowed (cyclic reset detection has been triggered"), reported as
     `NavStateOfOperation` (`…_CHECKING_MEDIUM`, `…_READY4NAV`, `…_FULLY_OPERABLE`, `…_NO_COMPLETE_DATA`,
     "Change NavStateOfOperation to %d") and the `BENCH_STARTUP` bench lines "Startup Navigation" /
     "Starting NavSDK" / "NavSDK started". `PCStarterImpl::calcAppStatus` tracks `PCSC_SDK`,
     `PCSC_DSIMapControl`, `PCSC_DSINavigation` OK/NOK ("ApplicationStatus has changed from"). The nav core
     itself (`libPathfinderApp.so`) logs `event[in]: START_NAVIGATION_SYSTEM` →
     `event[out]: NAVIGATION_SYSTEM_STARTED|NAVIGATION_SYSTEM_FAILED` and per-component
     "CFA: Startup finished of <name>. Done: ". [code]
   - Caveat: `StartModusNAV` returns success for **every** StartupResult except `ERROR` — so READY4NAV,
     CHECK FAILED and NO DATABASE all still set the 0x400 bit in the achieved mask. A bad database does not
     hold back the domain-status report; it only shows up in logs and later engine behaviour. [decompile]
6. **RUNNAV / PresCon.** On the restart path (`ReStartPrescon`, `sub_12141C`) and the initial run phase,
   navStartup sends `STARTERREQ_RUNNAV` (msg 1) and waits **60 000 ms**: response 17/1 →
   "StartupPresCon: Ready for navigation"; 1/1 → "PresCon running. APP set to running."; anything else →
   "Navigation System did not respond OK"; timeout → "Did not receive Start Response." Failure →
   `setAppState(7)` = AS_FAILED. [decompile]
7. **[~26 s] The report that ends INITIALIZING.** `sub_118400` ("Report %x back to ESO") ORs 0x4
   (COMM∧NAV) into 0x700 and reports **0x704**. `servicemgr` traces
   `[SM] APP_RUN_MODE 1796 reported for navigation` (handler `sub_12A598`, 0x12A598, channel
   `BENCH_Startup`), queues a `SetAppRunmodeJob` (event 17; `SetAppRunmodeJob::start` `sub_15EA98` applies
   it only when the app is in a fitting state — in AS_LOADING it logs "wait for AS_INITIALIZED before
   runmode can be set"), updates the app run mode (`[updateAppRunMode] %s to 0x%ld overwrite=%d`,
   `sub_13E85C`) and publishes the domain status: the per-domain dispatcher (jumptable at 0x113FC4,
   domain cases 0–36) logs `[updateDomainStatus] domain 'navigation' state 0x704` (string 0x175D58) and
   `[INFO] on=… event='updatedomainstatus' domainstate='0x704'`, then forwards over DSI via
   `listener_updateDomainStatusNav` (string 0x18272C) to the HMI peer. [decompile]
8. **Java side.** The DSI service is **`org.dsi.ifc.startup.DSIStartup`**, instance 0; the HMI's
   `DomainHandler` (implements `DSIStartupListener`) subscribes notifications for the domain-id
   "attributes" `{2,3,4,5,6,10,11,16,…}` at `initDSI` (`DomainHandler.java:201-208`, wait-for-service
   5 000 ms). The Nav callback is `updateDomainStatusNav(state, attr)` (`DomainHandler.java:495-497`) →
   `updateDomainStatus` (`:436-461`: requires attr == 1, maps 0 → 1, remaps exact values 4→6 and 8→14 —
   bit masks like 0x704 pass through) → `Domain.updateState` + fan-out to `IDomainListener`s →
   `StartupManager.updateDomainState` (`StartupManager.java:873`, registered `:461`) →
   `AppStateManager.updateDomainState` (`AppStateManager.java:776-786`) → `ComponentState["Navi"].
   updateDomainState` (`ComponentState.java:738-785`). Domain ids: 6 = "Nav", 10 = "SDS"
   (`DomainHandler.java:20-59`); inclusion test `isIncluded: (required & ~updated) == 0` (`:93-95`).
   [decompile — tree `Tools/jxe2jar/out/MU1316-vf/`]
9. **Java component start.** "Navi" in `hmi_startup.json:484-521`: `state_model_id 361`,
   `domain {id 6, required_state 0x700}`, bundles `adaptermanager` + `AppNavi`, guide module
   `HMINavi`/`SMNavi`, dependency `{id 10 (SDS), required_state 0x300, start_asynchron: true}`.
   On the domain reaching 0x700 the ComponentState (re)starts its bundles (a `doStartDomain(6, 0x700)`
   that itself waits up to **20 s**, `MAX_DOMAIN_START_WAIT`, `DomainHandler.java:63-66`, then enqueues
   bundle starts); "Navi" flips to started (`componentStarted`, `ComponentState.java:899-910` →
   ChoiceModel 361 = 1 → `appStateChanged("Navi", 1)`) when **all of**: domain 6 status ⊇ 0x700,
   both bundles OSGi-ACTIVE (state 32), and the `HMINavi`/`SMNavi` guide module registered
   (`isStarted` = `checkBundlesActive && GUIDEModuleState.isActive && checkDependencies`,
   `ComponentState.java:404-431`). **The SDS 0x300 dependency does not gate**: it is
   `start_asynchron`, so it only issues a non-blocking `doStartDomainAsync(10, 0x300)` and is never
   consulted by `isStarted()` — a correction to
   [Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md), which listed it as a requirement.
   [decompile] Consumers of "Navi"=1: `InitializationManagerNavi.appStateChanged` (BAP operation state),
   `JokerKeyComponentEvo` (nav hard-key entries), `FactoryResetHandler.isNavInitialized`, lastmode 5
   validity. [decompile]
10. **BAP NavSD leaves INITIALIZING.** In parallel, the AppNavi bundle publishing
    `CombiBAPServiceNaviListener` → `ServiceManagerNavi.addingService` →
    `AppConnectorNavi.setAppServiceListener` → `notifyAppServiceChanged(true)` → `updateHMIState` →
    `setHMIState(lsgID 15)` → VC acknowledge → `startInitialization()` — the only external trigger of
    the init sequence (`AbstractBAPModuleInitializationManager.java:146-162`). The sequence
    FctList (state 3) → OperationState → Properties (state 4) → Arrays →
    `CommandFinishInitializationSequence` (state 9, which checks nothing and only marks completion) is
    pure BAP command/ack processing; retries ≤ 5 on error 0x42. `getGeneralOperationState()`
    (`AbstractBAPModuleInitializationManagerFSG.java:123-152`) then returns NORMAL iff power on, no
    SWDL, `appIsReady`, `appState == 1` and `initState >= 9` — `isRequiredDataForOpStateNormalAvailable()`
    is the constant-`true` base for NavSD (not overridden). `InitializationManagerNavi.
    setFSGOperationStateValue` maps 0/1/2/3 → BAP 0/1/3/15 and the VC leaves the NA screen. [decompile]
11. **[~37 s] The upgrade report.** When the traffic stack is up, TTI is added and the report becomes
    **0xF0C** (`updateDomainState(Nav, 3852)` = 0xF0C in the HMI). This is *not* needed for the VC map;
    0x700 is the gate. [decompile] [doc]
12. **Beyond the gate — the map itself.** The AppNavi side has its own readiness ladder, all DSI-driven:
    the nav DSI trio `DSINavigation`/`DSIBlocking`/`DSICombinedRouteList` (instance 0) unblocks
    `NavigationStartup.checkStartup` → `setNavigationMainInitialized(true)`; `DSIMapViewerControl`
    attribute 1 (**MapReady**, `MVResponseControl.updateReady` → `CtxInit.updateReady`) →
    `OperationManager.setMapReady`; `MapKombi.mapInitialized` → `ClusterService.updateKombiMapReady(true)`
    → cluster map context 8 / init-screen handling; the native `NavstateOfOperation == 5` (FULL_OP —
    the same value as the engine's `NAVSTATEOFOPERATION_FULLY_OPERABLE`) plus all flags →
    `handleFullyOperableState` → NavSD InfoStates. None of this feeds back into the FctID 15
    operation state; it governs the cluster map rendering. [decompile]

### Sequence diagram

```mermaid
sequenceDiagram
    accTitle: Stock navigation readiness chain
    accDescr: servicemgr spawns navStartup on the HMI startDomain request; navStartup starts DR and COMM children, handshakes the navi engine over STARTERREQ with 60 s timeouts, reports the run-mode word; servicemgr publishes it as domain 6 status over DSI; the HMI starts the Navi component and the BAP NavSD operation state leaves INITIALIZING.
    participant HMI as HMI Java (StartupManager)
    participant SM as servicemgrmibhigh
    participant NS as navStartup (WorkingLoop)
    participant ENG as navi engine (AppStartATF + libPresentationController NavStarter)

    HMI->>SM: startDomain(6, 0x700)  [~15 s]
    SM->>NS: spawn /navigation/navStartup (launcher init 0→512, run 60 s, restart ×3)
    Note over NS: setAppState(1), slay leftovers,<br/>fetchSwdlMode (/mnt/persist/SWDL_*),<br/>PMon: DRController, IconProviderApp,<br/>PNavApp, AppStartATF "navi"
    NS->>NS: StartTransitiveModi(0x200→0x700)<br/>DR (PMon grp 1) + COMM (PMon grp 0)
    NS->>ENG: STARTERREQ_STARTUP_COUNTER_SEND (18) — wait 60 s
    ENG-->>NS: STARTUP_COUNTER_ACK (19, v1)
    NS->>ENG: STARTERREQ_INITNAV (0) — wait 60 s
    Note over ENG: module load + EGL (~21 s),<br/>CheckNavDBConsistency, FEC check
    ENG-->>NS: 17/1 READY4NAV (or 20 CHECK FAILED / 21 NO DATABASE)
    NS->>ENG: STARTERREQ_RUNNAV (1) / PresCon — wait 60 s
    ENG-->>NS: 17/1 "Ready for navigation" (or 1/1 "PresCon running")
    NS->>SM: reportAppRunMode(0x704)  ("Report %x back to ESO", +0x4 COMM∧NAV)  [~26 s]
    SM->>SM: [SM] APP_RUN_MODE 1796 reported for navigation<br/>SetAppRunmodeJob → updateAppRunMode
    SM->>HMI: DSI [updateDomainStatus] domain 'navigation' state 0x704
    Note over HMI: ComponentState["Navi"]: 0x700 ⊆ 0x704,<br/>AppNavi + adaptermanager ACTIVE,<br/>HMINavi/SMNavi up → appStateChanged("Navi",1)
    Note over HMI: AppNavi service → updateHMIState → VC ack →<br/>init sequence → initState 9;<br/>getGeneralOperationState(): appState 1,<br/>initState ≥ 9, appIsReady → NORMAL
    HMI-->>HMI: NavSD FctID 15 OP_State 3 → 0<br/>VC leaves "Initialising navigation system"
    NS->>SM: reportAppRunMode(0xF0C) (+TTI)  [~37 s, optional]
```

## ⏱️ Where the time goes (Q3)

| Stage | Function / site | Wait / timeout |
|---|---|---|
| Launcher `init` phase | `dsistartup.json:311-317` "navigation" `on_start` | apprunmode 0 (→ navStartup default 512), **30 000 ms**, non-blocking [doc] |
| Launcher `run` phase | `dsistartup.json:318-320` | **60 000 ms**, non-blocking; `on_restart`: sleep 500 ms → prockill → init/run blocking; `restart_param`: 3 restarts / 600 s reset window / `kill_process` [doc] |
| Startup-counter ack | `StartupNaviSDK` `sub_11CD48` → `sub_13143C(…, 60000)` | **60 000 ms** [decompile] |
| INITNAV (engine init incl. DB/FEC check) | `sub_11C758` → wait for msg 0/17/20/21 | **60 000 ms** [decompile] |
| RUNNAV / PresCon | `sub_11B500` (and `ReStartPrescon` `sub_12141C`) | **60 000 ms** [decompile] |
| Map medium / SWDL state | `fetchSwdlMode` `sub_1182DC` (stat `/mnt/persist/SWDL_SD1|SD2|OTA`) | fast, but selects `/fs/sda0` vs `/fs/sdb0` [decompile] |
| Engine module load | `AppStartATF` HmiModuleLoader loads `libPresentationController.so` + `libPathfinderApp.so` (24 MB), EGL, ATF modules | ≈ 21 s mark in the reference slog [doc] |
| DB consistency check | engine `PC_DBChecker`: early check on SD mount, `CheckVersion/Signature/Consistency/ISO` + FEC tasks | unbounded by navStartup (60 s handshake may expire first); internal retries "busy -> retry in 5s", "canceled -> retry in 60s"; new FEC list mid-check restarts the check after 5 s [code] |
| FEC / licence check | `StartFecCheckTask` / `RunFecCheckTask`, "FEC check failed" (in `libPresentationController`, not PathfinderApp) | unbounded [code] |
| Engine freeze | WorkingLoop watchdog: "GetWDReport" (`sub_11B864`), "NAVI: FREEZE DETECTED." → EBNavFreezelog → PMon restart | 1 s poll loop; PMon dead report → autorestart |
| navi child autorestart | `sub_12115C` ("Autorestart of NAVI Try %d", "KILL NAVI # %d") | **7 tries** [decompile] |
| Persistence flush | WorkingLoop `RequestPersistence` path | **10 retries** then give up [decompile] |
| Startup-counter window | WorkingLoop `+232` / `+236` | reset after **300 000 ms** [decompile] |
| Failure modes | `setAppState(7)` = AS_FAILED ("StartupNaviSDK failed. -> AS_FAILED", "StartupPresCon failed. -> AS_FAILED") | servicemgr restarts per policy [decompile] |
| HMI: wait for DSIStartup service | `DomainHandler.waitForDSIStartup` (`DomainHandler.java:163-183`) | 5 000 ms [decompile] |
| HMI: `doStartDomain(6, 0x700)` when the component first needs the domain | `DomainHandler.java:280`, `MAX_DOMAIN_START_WAIT` `:63-66` | **20 000 ms** block before bundle starts proceed [decompile] |

The 60 s PresCon timeout from the earlier note is one of **three** 60 s handshakes; a boot can therefore
stall up to ~3 minutes inside navStartup before servicemgr's own timeouts/restarts kick in. The dominant
normal-boot cost is the engine INITNAV stage (module load of the 24 MB `libPathfinderApp.so` +
`libPresentationController.so`, EGL, map-DB and FEC checks): 15 s → 26 s in the reference slog. [decompile] [doc]

## 📡 Observable signals on a live unit (Q4)

QNX `slogf` lines (grep `sloginfo`):

- `NAVI: Startup SDK in progress` / `NAVI: Startup SDK success.` / `NAVI: Startup SDK failed.` — navStartup 0x11D1C0, slogf code 10001, severity 5 [decompile]
- `NAVI: Startup PresCon (DSI) in progress.` / `NAVI: Startup PresCon (DSI) success.` / `NAVI: Startup PresCon (DSI) failed.` — same [decompile]
- `NAVI: FREEZE DETECTED.` / `NAVI: FREEZE INFORMATION IN EBNavFreezelog file.` / `NAVI: FREEZE NO EBNavFreeze script. Normal restart.` — WorkingLoop [decompile]
- servicemgr traces (channel `BENCH_Startup` and the startup channel, `tracing::Channel::logCV` → slog): `[SM] APP_RUN_MODE %d reported for %s` (0x1792C0), `[updateDomainStatus] domain 'navigation' state 0x…` (0x175D58), `[INFO] on=… state=… event='updatedomainstatus' domainstate='0x…'` (0x175A78), `[SM] AS_LOADED reported for %s` (0x17FB78), `[SetAppRunmodeJob]: app … in AS_LOADING, wait for AS_INITIALIZED …` (0x180354), `[updateAppRunMode] %s to 0x%ld overwrite=%d` (0x17C418) [decompile]

navStartup `printf` lines (console of the nav process; the nav children share `/fs/sda0/console-navi.log` —
AppStartATF opens it, DRController/PNavApp reference it [code]):

- `ESO Fwk triggering initApplication with runmode = %x sys %x` (0x11BD14)
- `Initial RunRequested Procs %x %x` (0x123D88)
- `StartTransitiveModi : About to start DR|COMM|NAVI` / `Started DR|COMM|NAV|TTI: %x` / `COULD NOT START …` (0x120E08–0x120E8C)
- `Report %x back to ESO` (0x118438) — **the run-mode word itself**
- `StartupPresCon: Ready for navigation` / `PresCon running. APP set to running.` / `Did not receive Start Response.` (0x11B500)
- `StartupNaviSDK: Received StarterResponseOK|NOK` / `Did not receive reset counter acknowledge.` (0x11CD48)
- `ReStartPrescon OK runMode %d` (0x12150C), `ESOCOM: AUTORESTART From %x to %x.` (0x1241D0), `RESTARTED` (0x124210)

Transport caveat: navStartup/servicemgr format strings log through `tracing::Channel` into the ESO trace
backend (CTracer → `libTrace_eso.so` → traceserver; `tracing.json:1064-1076` sets `navigationProc` channels
`ESOComm`, `BENCH_Startup`, `Starter`, `SwdlProvider` to `info`, everything else `warning`), while the
`NAVI: …` lines use `slogf(10001, 5)` and go to slog directly. On the P5092 reference capture the
`APP_RUN_MODE` / `updateDomainStatus` lines did appear in sloginfo, so grep both sinks. [doc] [code]

Additional engine-side lines worth grepping (trace backend / nav console): `PCStarterImpl Init Completed`,
`PCStarterImpl Run Completed`, `SendReady4Nav` / `SendNoDatabase` / `SendCheckFailed`,
`STARTERREQ_INITNAV: No Database found!|Navigation not enabled!`, `DB Check finished successfully with: `,
`Starting DB Check`, `DB on SD card found. Do early check`, `MMX BENCH_STARTUP: NavSDK started`,
`Change NavStateOfOperation to %d`, `Navigation Application ENABLED|DISABLED via GEM!`,
`PCStarterImpl::calcAppStatus. PCSC_SDK is OK|NOK`, and from the nav core
`event[out]: NAVIGATION_SYSTEM_STARTED|NAVIGATION_SYSTEM_FAILED`. [code]

Files: `/fs/sda0/console-navi.log` (nav console; on the map SD), `/mnt/persist/SWDL_SD1|SWDL_SD2|SWDL_OTA`
(SWDL markers that redirect the map medium / block checks), `/fs/sda0/version-cfg.txt` (map version),
`/tmp/NONAVIWD` (navStartup: WD-autorestart debug disable), `/mnt/app/core/DeadlockInfo.txt`,
`/tmp/SDKDUMP` (engine dump marker), `/navigation/FSID_Navi_Enabled|Disabled` (GEM navi enable switch,
engine side), `/mnt/persist/NAVI_START_DB|NAVI_START_POI` (navStartup startup params: force map/POI scan),
persisted SD-scan path list (engine: "Persist scanned paths and restart navi"). [code]

No PPS objects are used anywhere in the navStartup readiness path (no `/pps` strings). [code]

One-shot boot-timeline grep (QNX grep has no `-o`/`\|` — use multiple `-e`):

```
sloginfo | grep -e startDomain -e APP_RUN_MODE -e updateDomainStatus -e "NAVI: Startup" \
          -e StartupPresCon -e StartupNaviSDK -e "Report .* back to ESO" -e StartTransitiveModi
```

Healthy boot reference: `startDomain` ≤ 20 s → `APP_RUN_MODE 1796` ≈ 26 s → domain state 0xf0c ≈ 37 s. [doc]

Java side (sloginfo via the HMI's `SlogSink` on `/dev/slog`, configured by lsd.sh's
`-DSLOG=Ext.Startup=5,Fw.Error=2`; `SlogSink.java:160-167`, `LogServImpl.log` threshold rule): on a
stock unit **only** the `Ext.Startup` startup-event lines and `Fw.Error` lines are visible — all
`Fw.*`/`App.*` channel lines are silenced. The visible ones are the `logStartupEvent` free-form lines
(`StartupManager.java:128-145`, level 1000000 ≤ threshold 10000000), e.g. [decompile]:

- `startDomain( 6, 0x700 )` / `-> DSIStartup.startDomain( 6, 0x700 )` (`DomainHandler.java:297-298`)
- `OperationManager#taskCompleted( TASK_MAP_READY )` — the cluster-map-ready event
- `[Startup] NavigationStartup#setStartupFront( 0, true )` — the nav DSI trio arrived
- `CtxInit#enter() - start map initialization (set notification for MapReady)`

Deeper Java traces (`ComponentState[…] flag component as started`, `initState for lsgID=15 changed to 9`,
`updateOperationStateBAP new operation state … is 0`) exist but need widened `LOG`/`SLOG` properties
(see [Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md) for the production values). [decompile]

## 🪝 How an external module could learn "nav is ready" (Q5 — description only)

1. **sloginfo tail (cheapest, read-only).** `NAVI: Startup PresCon (DSI) success.` or
   `[SM] APP_RUN_MODE 1796 reported for navigation` / `[updateDomainStatus] domain 'navigation' state 0x704`
   are exact, grep-able readiness markers; the first is the engine-level one, the second the framework-level
   one. Parsing `Report %x back to ESO` from the nav console gives the full bit word.
2. **DSI client on the startup service.** The canonical machine-readable channel is the same one the HMI
   uses: subscribe to **`org.dsi.ifc.startup.DSIStartup` (instance 0)** with notifications on the domain-id
   attributes and watch the `updateDomainStatusNav(state, 1)` callback for state ⊇ 0x700 (native peer:
   `listener_updateDomainStatusNav`). Weightier than a log tail (DSI client libs, session setup), but
   structured and push-based; `de/mib/swdiagnosis/StartUpDiag.java:74` shows the same entry point can even
   be driven diagnostically. [decompile]
3. **Console log file.** `tail -F /fs/sda0/console-navi.log` for `Ready for navigation` — but it lives on
   the map SD (absent if the SD is missing, which is exactly a failure case) and is shared by the nav
   children.
4. **Files.** No dedicated ready-marker file exists in stock (the `/tmp/carplay_nav_ready` marker in the
   kamgurgul tree is third-party). `/tmp/NONAVIWD` and the `/mnt/persist/SWDL_*` markers are *inputs*, not
   signals — do not touch them from an integrator module.
5. **Starting earlier is not on the table from outside.** The gate is owned by navStartup's bit word and
   servicemgr's domain status; the only external levers are the input conditions (map SD present and
   consistent, no SWDL markers). Everything else (lowering `required_state`, faking the report) is native
   or Java-framework surgery and is rejected in
   [Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md) (Fix C) for good reason.

## 🔗 Related

- [Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md): VC state tree, BAP init states, the Java side of this chain with file:line.
- [NavSD FctID Matrix](NAVSD_FCTID_MATRIX.md): FctID 15/38 ownership.
- [VC FPK State Machine](VC_FPK_STATE_MACHINE.md): how OP_State becomes the NA screen.

Provenance: 2026-10-18 static analysis — IDA (`navStartup.i64`, `servicemgrmibhigh.i64`, functions cited by
address above), strings of `AppStartATF`/`libPresentationController.so`/`libPathfinderApp.so`, configs
`system/etc/eso/production/{framework.json, dsistartup.json, tracing.json, servicemgr.json}` (line numbers
as cited), P5092 reference boot sloginfo. Nav build identity (all nav binaries):
`CLU10_17123MIB2MAIN_QC_B18.07.2.100345`, ESO framework 5.51.6, PNAV core 10.2.9. Java claims carried from
[Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md) and re-verified against `Tools/jxe2jar/out/MU1316-vf/`
(see its "readiness chain end to end" section for the file:line citations). No unit was touched.
