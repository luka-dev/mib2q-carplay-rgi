# `smartphone_integrator`: rapid-replug recovery and terminal cleanup

Offline analysis performed on 2026-09-04. No head-unit commands or deployment were used.

## Verdict

The missing `io-usb-dcd` observed after a failed reconnect is not proof that the
device stack crashed first. The stock SI state machine can deliberately produce
that terminal state.

For CarPlay on the new `0x0424:0x2530` media connector, SI does this:

1. `TIMEOUT_STARTUP` consumes the finite setup-retry budget.
2. While retries remain, SI runs its normal device-stack restart job:
   `start_stack::none`, a 5,000 ms delay, then `start_stack::device,2`.
3. When the retries are exhausted, SI enters final cleanup.
4. New-connector cleanup resets the media-connector switch and, if the OTG
   device stack exists, writes `start_stack::none`.
5. That cleanup job has no matching device-stack start. SI now waits for a new
   device/connector event.

If the phone has already been reinserted, the event capable of starting the new
session may already be over. The internal `0x2530` hub remains present, so the
main `usblauncher.lua` insertion handler does not run again. This explains the
stable dead state with the phone physically connected, no `otg_dev`, and the
last `usb_ctrl1` command equal to `start_stack::none`.

The exact initiating event in the captured failure is still not proven. Two
independent stock race windows can feed the state machine into this result.

## MHI2Q SI evidence

Analyzed image:

`Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/bin/apps/smartphone_integrator`

SHA-256:
`a25dc981e5591cb3c6b09f00fd95de23803dd3daa0f562e7e8d60b6d0e1b0d02`

Relevant functions in the ARM image:

| Address | Function | Relevant behavior |
|---|---|---|
| `0x172C14` | app-name mapper | `0=CARPLAY`, `1=GAL`, `2=MIRRORLINK`, `3=CARLIFE` |
| `0x186B00` | `CSIStateRunning_OnEventAppState` | consumes setup retries for state 14, then selects retry or final cleanup |
| `0x1800B0` | `MediaConnectorHandler_Prepare` | a CarPlay retry on connector type 2 enqueues the restart-device-stack job |
| `0x193544` | `MediaConnectionService_OnJobRestartDeviceStack` | stop command, configured 5 s delay, CarPlay device-start command |
| `0x17FC48` | `MediaConnectorHandler_Cleanup` | final CarPlay cleanup on connector type 2 enqueues the cleanup job |
| `0x19409C` | `MediaConnectionService_OnJobCleanup` | new-connector branch resets the switch and stops a running device stack |
| `0x193264` | stop-command writer | writes configured `start_stack::none` |
| `0x1933FC` | start-command writer | writes `start_stack::device,2` for CarPlay |
| `0x19B8F0` | switch-reset command builder | builds `reset switch 250 0` in this cleanup path |

The `OnJobCleanup` new-connector branch calls the stop-command writer at
`0x1941A0`. There is no call to the start-command writer anywhere in that
function. The start occurs only in the separate retry job.

### Why `verifyPortReset` is not the CarPlay root cause

`SI_VerifyPortResetForAppState` at `0x184030` contains the real condition
`portResetNeeded = (appState == ERROR_RUNTIME)`. Thus state 14 does not set the
flag for the app types handled by that assignment.

However, CarPlay is app ID 0. The verifier returns immediately for app ID 0,
and the CarPlay context initializes `portResetNeeded` to one when cleanup
finishes (`0x187290`). CarPlay then follows a special connector-type branch in
`MediaConnectorHandler_Cleanup`. Patching the comparison from state 12 to state
14 would therefore not fix this CarPlay failure and could regress recovery for
the other app types.

## Race window 1: stale `stopncm.sh` destroys the new interface

The stock NCM removal script explicitly delays its destructive action:

```sh
# To workaround race condition between removal/unmount
sleep 1
ifconfig ${INTERFACE} destroy
```

The interface is selected only by the global name `carplay0`; the script carries
no USB device identity or connection generation.

A rapid replug can therefore execute this order:

1. old-phone removal launches `stopncm.sh`;
2. it sleeps;
3. the new phone enumerates and `startncm.sh` creates a new `carplay0`;
4. the old removal script wakes and destroys that new interface.

The race exists statically. It is a strong candidate for the rapid-replug
trigger, but the available live capture does not timestamp both shell scripts
well enough to prove that this exact ordering occurred.

`startncm.sh` adds two more weaknesses: it does not verify that
`/dev/io-usb/otg_dev` exists before mounting, and it does not stop on mount
failure. The UTF-8 en-dashes in the `if_up` arguments are malformed options, but
existing `if_up` reverse engineering shows they become harmless extra operands;
they are not the reconnect root cause.

## Race window 2: final cleanup can overtake reinsertion

The production settings make overlapping generations plausible:

| Setting | Value |
|---|---:|
| `deviceRemovalDelay` | 1,250 ms |
| NCM removal sleep | 1,000 ms |
| `cleanupTimeout` | 2,500 ms |
| CarPlay `startupTimeout` | 10,000 ms |
| `restartOtgStackDelayTime` | 5,000 ms |
| CarPlay `restartDelay` | 2,000 ms |
| CarPlay `maxSetupRetries` | 2 |

An insertion inside the removal/NCM-cleanup window can belong to the new
generation while old-generation work is still queued. After the retry budget is
spent, final new-connector cleanup resets the switch and stops the OTG stack.
The stock policy assumes another detection event will rebuild it; a phone that
is already physically present does not guarantee such an event.

This also explains why the stock behavior can look like SI "stopped trying": it
did. The setup retry counter is finite. After the last retry, the state machine
performs terminal cleanup and waits for external topology change rather than
running an unlimited recovery loop.

## MH2p comparison

Analyzed image:

`Firmwares/HU/MH2p_US_LB636_P2712/extracted/app/eso/bin/apps/smartphone_integrator`

SHA-256:
`e7d9480f5952884d00e12aa94a8552b8a4ae4de6b24e58127d783aeb7724cdc7`

The later binary has different layout and additional app/connector support, but
the recovery policy is materially the same:

| MH2p address | Verified behavior |
|---|---|
| `0xBCEE8` | `SI_VerifyPortResetForAppState`; the assignment still tests state 12 only |
| `0xB4554` | CarPlay retry enqueues a restart-device-stack job |
| `0xB5238` | final connector cleanup selects soft cleanup or a recovery job |
| `0xCB008` | new-connector cleanup resets the switch and writes the stop-stack command when the stack was running |
| `0xC8F90` | writes the configured OTG stop command |

MH2p's `usblauncher.lua` is improved at connector insertion: it calls an
explicit host/device-mode setup script, binds the child launcher to
`/dev/io-usb/otg_dev`, and supports connector `0x4940`. It still uses the
`/tmp/usblauncherFlag_mediaconnector_*` one-shot guard and a 10 s launcher
timeout. It does not remove the SI terminal-cleanup behavior.

The comparison therefore does not provide a ready-made fixed SI implementation
to copy. It confirms that this is a shared recovery-policy limitation rather
than a one-off MHI2Q binary corruption.

## Other events that can reach the same terminal state

Rapid unplug/replug is only the easiest trigger. The same chain can begin with:

- a short cable or power interruption during USB role reversal;
- the phone failing to enable both iAP2/NCM interfaces before the 8 s cinemo or
  10 s CarPlay startup deadlines;
- a slow/custom `dio_manager` startup that consumes SI's 10 s budget;
- an NCM mount failure or a stale NCM removal;
- a delayed device-removal or cleanup job overtaking a new insertion;
- a media-connector power cycle whose child OTG launcher reaches its control
  timeout before SI publishes the required command path.

They are different initiating failures, but all can end in state 14, exhaust the
same retry counter, and reach the same terminal `start_stack::none` cleanup.

## Fix direction

Do not restore the wrapper that killed/relaunched the OTG `usblauncher`, and do
not patch `verifyPortReset` from state 12 to state 14 for CarPlay.

The least invasive correction should be developed in two stages:

1. Add a connection-generation fence to NCM teardown so an old `stopncm.sh`
   cannot destroy a `carplay0` created by a newer `startncm.sh`.
2. Change final CarPlay/new-connector recovery from terminal stop-only cleanup
   to an SI-owned reset followed by a delayed `start_stack::device,2` when the
   phone/topology still requires CarPlay.

Increasing `maxSetupRetries` is useful only as a diagnostic. It makes SI execute
more proper stop/delay/start cycles, but it does not remove either race and can
turn a permanent hardware/role-swap failure into a long restart storm.

Before another live test, the offline work should produce two independent,
reversible artifacts: an NCM generation-fence patch and a narrowly scoped SI
patch or interposition for the final new-connector cleanup. Test them one at a
time; otherwise a successful reconnect will not identify which race was fixed.

The existing `/mnt/app/root/rc.local` from `Patches/wifi_fix` is a usable
persistent launch point for passive logging or a carefully latched monitor. Its
`respawn()` wrapper restarts a child forever after every exit, so it must not
directly run an unlatched USB reset loop. Any future recovery child placed there
needs per-session generation tracking, a success latch, and a cooldown; SI must
remain the only owner of `dio_manager`.
