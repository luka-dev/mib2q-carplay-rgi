# Phone tab gating — CarPlay hides the stock PHONE2 tab

**Date:** 2026-08-04

**Scope:** MHI2Q/MU1316 head unit and Audi Virtual Cockpit PHONE2 BAP domain.

**Status:** v1 did not hide the tab; v2 caused duplicate telephone icons and was rolled back. The gate is disabled pending a correct availability lever.

## Goal

While CarPlay owns the phone, hide the stock crossed-out telephone tab in the cluster. Restore it only after CarPlay disconnects and MMI HFP telephony is actually available again.

The implemented rule is:

```text
tabVisible = (mobileConnectionType == HANDS_FREE_PROFILE) && !carPlayActive
```

`mobileConnectionType == 3` is the stock HFP state. The gate is fail-open before the first phone update, so a missing callback cannot permanently remove the telephone tab.

## Failed implementation (retained here as an investigation record)

The implementation has a source, a shared gate, and a BAP lever.

### 1. Two independent state sources

`BAPPropertyTelFSGSetup.updateAsync()` publishes the real stock `mobileConnectionType` enum to the gate. This source decides when HFP has recovered and the tab may return:

- `java_patch_new_replace/de/audi/app/phone/core/bap/telephone/BAPPropertyTelFSGSetup.java:203`
- `PhoneTabGate.setMobileConnectionType(i)` at line 209.

`TerminalModeBapCombi.ActiveDeviceStateListener` supplies the early CarPlay lifecycle edge. A CarPlay device is already identifiable during ATTACHED/ACTIVATING, before the telephony bundle necessarily publishes its next FSG setup:

- `java_patch_new_replace/de/audi/app/terminalmode/combi/TerminalModeBapCombi.java:83`
- `PhoneTabGate.setProjectionActive(tmdevice.isCarplayDevice())` at line 96.

Using both sources is intentional. The terminal-mode source hides promptly; the phone source prevents an early restore until HFP is really back.

### 2. Shared gate and late-registration replay

The removed `PhoneTabGate` stored the two inputs, computed visibility and invoked a PHONE2 resend callback.

PHONE2 and terminal mode are initialized by different bundles. Therefore `registerResend()` at lines 67–74 immediately replays an already-hidden state when PHONE2 registers late. This fixes the first v1 race: CarPlay could hide the gate before the receiver existed, after which no BAP update followed.

### 3. PHONE2 BAP availability lever

The removed override of `InitializationManagerPhone2.updateHMIState()` sent PHONE2 LSG 41 (`0x29`) state `NOT_READY` (`0`) through `IDSIBAPController`.

The zero is deliberately re-sent even if the cached HMI state is already zero. PHONE2 can be constructed while the controller still targets `NullDSIBAPService`; when the real DSI proxy becomes available, the later forced update must reach the actual BAP stack. This fixes the second v1 race.

## Why v1 did not prove the mechanism

The first deployed version had three diagnostic holes:

1. A hidden transition before `InitializationManagerPhone2` registration was lost.
2. A `NOT_READY` write could land in the null DSI service and then be skipped because the cached state was already zero.
3. Errors were swallowed and the gate emitted no dedicated log lines, so an unchanged tab did not distinguish a missing source callback from an ineffective BAP lever.

The old proposal to gate `AppConnectorPhone2.updatePhoneModuleState()` is not implemented and is not supported by the VC binary analysis. In `hmi_app.elf`, `App::getTabBarIconIndex` at `0x183378` does use `phoneAvailable` (`App+960`), but `App::setPhoneAvailable` at `0x18836C` receives a separate cached receiver-availability byte in the handler at `0x17F1B0`; it is not derived directly from `PhoneModuleState_Status` or from the FSG operation-state value.

## On-unit verification

The failed v2 JAR deployed on 2026-08-04 had SHA-256:

```text
3605c795dd374f71979078a3d2534db2f1dedceea8725fe8c728119e55ba2df2
```

Its on-unit log proved an additional source bug: the gate hid PHONE2 during boot on `mobileConnectionType=0`, before CarPlay attached:

```text
[PhoneGate] PHONE2 receiver registered hidden=false
[PhoneGate] visible=false reason=mobileConnectionType=0 ...
[PhoneGate] PHONE2 apply hidden=true ... lsg=41
```

More importantly, forcing LSG 41 to `NOT_READY` did not clear the desired VC availability. The cluster showed two telephone icons simultaneously, consistent with switching/falling back between its separate Telephone and Telephone2 domains instead of hiding the shared telephone slot. Therefore this lever is rejected rather than merely changing the `mobileConnectionType=0` rule.

The cleaned rollback JAR staged on the HU has SHA-256:

```text
84165b82523e6ec17c303a2ad3e1217522687cb65186d74a10d6dade3cbdef62
```

It removes `PhoneTabGate` and both source calls. The now-redundant patch copies of `BAPPropertyTelFSGSetup` and `InitializationManagerPhone2` are also absent from the JAR, so the firmware's stock classes are used directly. PHONE2 is fully back to stock behavior. It takes effect after a normal HU reboot; Java must not be restarted manually.

The next investigation must locate the true publisher of `BAP_Telephone2_Available`/the VC receiver's availability byte, or patch the VC availability consumer. Changing the CarPlay/HFP source logic again cannot fix the rejected HMI-state lever.

## Cluster-side evidence

In the AU491 `hmi_app.elf`:

| Address | Function / result |
| --- | --- |
| `0x187EEC` | `App::processTabBar` |
| `0x183378` | `App::getTabBarIconIndex`; returns icon index 0 when `phoneAvailable == 0`, and crossed-handset index 6 when available but disconnected |
| `0x18836C` | `App::setPhoneAvailable` |
| `0x17F1B0` | PHONE2 operation-state handler that forwards a separately cached receiver-availability byte to `setPhoneAvailable` |

Thus the visual target is still exact: force VC `phoneAvailable=0`. The reboot test determines whether PHONE2 LSG `NOT_READY` is sufficient to make the BAP receiver publish that value.
