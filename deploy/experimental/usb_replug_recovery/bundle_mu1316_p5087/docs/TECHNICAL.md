# How the bundle works

## Why there are four files

The two fixes are independent:

```text
old stopncm                            SI after retries
sleep 1                               final cleanup
destroys the new carplay0             SwitchPort(direction=0)
        ↓                                     ↓
no working NCM                        start_stack::none forever
```

The NCM fix necessarily consists of three files. Lua passes
`busno/devno/iface`; `startncm.sh` and `stopncm.sh` use them as the identity
of one USB enumeration and take a shared per-interface lock. If stop wins, it
finishes teardown before the new mount. If start wins, a late stop sees a
different generation and does not destroy the new `carplay0`.

The new start also:

- checks `/dev/io-usb/otg_dev` via `ls`;
- does not continue after a failed mount;
- publishes the generation only after mount/up;
- fixes the UTF-8 en-dash to ASCII `-s 2 -r 1`;
- logs its decisions to `/tmp/carplay_hook.log` as `NCM_FENCE`.

## SI re-arm

After CarPlay `TIMEOUT_STARTUP` retries are exhausted, stock MU1316 P5087 calls:

```text
MediaConnectorHandler_Cleanup
→ EnqueueSwitchPort(direction=0)
→ MediaConnectionService_OnJobSwitchPort
→ cleanup + start_stack::none
→ job complete without a new start
```

One ARM instruction in the binary changes:

```text
VA:          0x17FDB4
file offset: 0x7FDB4

stock bytes:   00 30 a0 e3    MOV R3, #0
payload bytes: 2e 33 09 e0    AND R3, R9, LR, LSR #6
```

At this point `R9` holds the cleanup error flag, and the CarPlay branch gets
`CONNECTION_FLAG_REPORT_CARPLAY_SUPPORTED == 0x40`. Only the combination
`error=1 + CarPlay` gives direction 1. Direction 1 already exists in stock SI and
does activate-CarPlay, switch-port and `start_stack::device,2`.

The patch does not increase the retry counter, does not add a loop and does not
touch ownership of `dio_manager`.

## Scope per projection system

| System | SI app ID | Effect of the SI patch |
|---|---:|---|
| CarPlay | 0 | error-cleanup changes from direction 0 to 1 |
| Android Auto / GAL | 1 | goes down a different cleanup branch; the instruction is not reached |
| MirrorLink | 2 | goes down the same separate branch as GAL |
| CarLife | 3 | CarPlay flag is zeroed; direction stays 0 |

The matching Lua enables strong generation identity only for CarPlay. CarLife
keeps the stock-compatible no-argument teardown. Therefore the bundle does not
claim to fix a separate CarLife rapid-replug case until such a live defect is
proven and a separate identity contract is designed.

## What the bundle does not fix

If the OTG `usblauncher` itself dies, the one-shot flag of the soldered-on
`0x2530` hub may prevent the main launcher from creating it again. Previously an
external kill/relaunch brought the USB node back "behind the owner's back", but
did not bring up CarPlay. That is why the bundle has no `rc.local` watchdog,
port-reset loop or second SI owner.

The bundle also does not change:

- `verifyPortReset` state 12/14;
- `maxSetupRetries`, startup/restart timing or child config;
- audio hook, APAC, altScreen, renderers, Java or PF;
- Tailscale, Wi-Fi, LTE and memory/CPU policy.

## Exact image boundaries

All eight files are checked via `MANIFEST.sha256`. SI additionally has a size of
`802798` bytes. The full hashes are in the bundle root; an unknown target is
never overwritten automatically.
