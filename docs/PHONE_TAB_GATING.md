# Phone tab gating — show the cluster's telephone tab only when MMI telephony works

**Date:** 2026-08-01
**Scope:** MHI2Q (MU1316) head unit, Audi Virtual Cockpit tab bar (the "tongue").
**Status:** designed, not yet implemented. Both sides of the wire read from the images;
nothing below has been measured on a car.
Companion docs: `SMARTPHONE_CLUSTER_KNOWLEDGE.md`, `DSI_CARKOMBI_KOMBISYNC2_REFERENCE.md`.

---

## 0. The problem

The telephone segment of the cluster tab bar exists whenever the head unit declares the
domain, whether or not a call can be placed. With CarPlay connected the cluster draws a
crossed-out handset and raises a note:

> CarPlay mode. Not possible to make calls via MMI. Connect iPhone using Bluetooth.

The advice is wrong in the common case — the phone is usually *already* paired over
Bluetooth — because CarPlay takes telephony regardless of what else is connected. The head
unit says so itself: `DSITelephone` and `DSINAD` declare
`RESULT_ERROR_VOICE_NOT_POSSIBLE_CARPLAY_ACTIVE = 17`, and `DSIBluetooth` declares
`RESULT_ERROR_CARPLAY_ACTIVE = 22`. So the tab advertises a function that is unavailable,
and the note tells the driver to do something already done.

**Wanted:** the tab exists only when telephony through the MMI genuinely works.

---

## 1. The rule

```
tabVisible = hfpActive && !projectionActive

hfpActive        = the mobile-equipment topology holds a device with USAGE_HFP
                   AND its profile is up
                   (telMode == TELMODE_HFP, or telActivationState operational)

projectionActive = swapStatus carries any of
                       SWAPSTATUS_MIRRORLINK (1)
                       SWAPSTATUS_CARPLAY    (2)
                       SWAPSTATUS_GAL        (4)   // Android Auto
                       SWAPSTATUS_CARLIFE    (8)
```

Spelled out rather than written as the mask `0x0F`: the four names say what is being
excluded, and nobody has to look up which bit is Android Auto.

`swapStatus` is preferred over a device's `connectionMethod` because it reports which
projection currently *holds the slot*, not merely what is plugged in.

---

## 2. Where it goes

Two seams, because two different things are being controlled.

### 2.1 Content — `AppConnectorPhone2` (primary seam)

`de/audi/app/combi/bap/app/phone2/AppConnectorPhone2.java` is what feeds the cluster's
telephone state over BAP. It implements `CombiBAPServicePhone2` and carries:

```
updatePhoneModuleState(moduleState, moduleSupportedServices, moduleActiveServices, simState)
updateRegisterState(...)      updateSignalQuality(...)
updateLockState(...)          updateMobileServiceSupport(boolean[])
updateNetworkProvider(...)    updateAutomaticCallForwarding(...)
```

Each builds a `*_Status` struct and calls `sendPropertyStatus(<id>, status)` — property 23
for the module state. **`updatePhoneModuleState` is the narrowest place to gate:** it is
the call the cluster's `avail` / `connected` derive from.

Implement it exactly like the route-guidance gate already in this patch set —
`com/luka/carplay/rgd/GatedCombiService.java` wraps `CombiBAPServiceNavi` and drops native
calls while CarPlay owns route guidance. A `GatedPhoneService` wrapping
`CombiBAPServicePhone2` follows the same shape: when the rule is false, report the module
as unavailable instead of passing the real state through; otherwise delegate untouched.

`CombiModulePhone2` exposes the pair to wrap (`getPhone2Service()`,
`getPhone2ServiceListener()`), so the wrapper installs without fighting the architecture.

### 2.2 Existence — the function list

Whether the segment *occupies a slot* is decided a level up, where the domain
`Cluster_TabBar_Tel_iconList0` (with `_iconList1`, `_text0`) is declared —
`AbstractFunctionListPhone2` / `ServiceManagerPhone2` in the same package. Gating only the
connector leaves an empty slot in the strip; gating only the list can race with in-flight
state. **Do both, driven by the one condition.**

### 2.3 Source of the projection flag

`de/audi/app/terminalmode/dsi/smartphoneintegration/SmartphoneIntegrationDSILifecycleController.java`
already receives `updateSWaPStatus(int, int)` — today it only completes a startup
requirement (`SWAPSTATUS_CARPLAY`). Route that value to the gate, or subscribe the gate
directly to `DSISmartphoneIntegration` (`ATTR_SWAPSTATUS`).

---

## 3. What the cluster does with it

The telephone domain is the one whose icon the **cluster** picks; every other domain takes
an icon list plus an index from us. In `hmi_app.elf` (AU491 2018):

| Address | Function |
| --- | --- |
| `0x187EEC` | `App::processTabBar` |
| `0x183378` | `App::getTabBarIconIndex` |
| `0x180A48` | `setTabBarDataForUI` |

`setTabBarDataForUI` writes five data points:

| DPID | Meaning |
| --- | --- |
| `0x020003d0` | `DP_ID_TabBar_Tel_Icon_Index` |
| `0x020003d1` | `DP_ID_TabBar_Tel_SignalQuality_visible` |
| `0x020003d2` | `DP_ID_TabBar_Tel_Text_Index` |
| `0x020003ce` | `DP_ID_TabBar_Tel_DynamicText` |
| `0x020003cf` | `DP_ID_TabBar_Tel_DynamicText_AlternativeText` |

`getTabBarIconIndex` decompiles into seven branches over `FSG:[avail, connected]`,
`Phone:[OpState, TelState, RegState]`, unread SMS / e-mail / missed-call counts and lock
state:

| Index | Condition |
| --- | --- |
| **0** | `avail == 0` — returns immediately, **nothing is drawn** |
| 6 | `avail == 1, connected == 0` — the crossed-out handset seen in CarPlay today |
| 3 | `RegState == 3` (registration denied); also clears signal quality |
| 2 | no service, or lock state outside {0, 7, 13, 14, 15}; clears signal quality |
| 5 | as 2, with unread messages or missed calls |
| 1 | normal operation |
| 4 | normal operation, with unread messages or missed calls |

Index 0 is the cluster's own "draw nothing", which is why the gate reports the module
unavailable rather than trying to supply a blank icon. The tab bar cannot be gated from the
cluster side: `AU491_TabBar.kzb` lives in the A02-compressed half of the image, which the
repacker cannot write — of all scenes only the two Tubes packages are replaceable.

---

## 4. Timing

Drop immediately when the condition goes false; restore with roughly a second of delay when
it goes true. The phone needs that long to re-establish HFP after a cable is pulled, and
without the delay the tab blinks as CarPlay tears down and Bluetooth comes back.

---

## 5. Limits, stated rather than discovered later

**The note is a separate job.** It is not tied to the tab or the icon: its text lives in the
cluster (four length variants, shortest is the one on screen) and the head unit sends only a
popup identifier. Suppress it where it is raised, under the same condition — with no
telephony and no tab, there is nothing left to advise about.

**Embedded modem.** The rule keys on HFP alone, so a car with `USAGE_NAD` and eSIM telephony
also loses the tab although calls remain possible. If unwanted, the left-hand side becomes
`hfpActive || nadTelephonyAvailable`.

**The older stack.** `de/audi/app/combi/bap/app/phone/` (`CombiModulePhone`,
`AppConnectorPhone`) is the pre-Phone2 path; if a target build uses it, the gate must be
duplicated there.

**Do not gate diagnostics.** `de/mib/swdiagnosis/combi/CombiDiagnosisConnectorPhone2.java`
calls the same `updatePhoneModuleState` from the diagnosis path; filtering it would break
test mode.

---

## 6. Provenance

Head-unit side: decompiled MU1316 image at `Tools/jxe2jar/out/MU1316-combined-vf`.
Cluster side: `hmi_app.elf` from the AU491 2018 package, decompiled at the addresses above;
data-point names from the studio's signal catalogue. The same analysis, from the cluster's
point of view, is in the VC studio repo as `RE/hmi_tabbar_phone_visibility.md`.
