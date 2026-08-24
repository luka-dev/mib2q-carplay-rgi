# The connect failure, as stated by the phone

Settled 2026-08-24 from the iOS 26.6.1 sysdiagnose (`23G83`, captured 2026-08-21 23:38, the wife's
phone during the failing evening). No theorising needed — `airplayd` names the reason.

## The verdict

    airplayd [com.apple.airplay:APBrowserCarSessionHelper] [Bonjour/USB] StartBonjourForUSB
    airplayd [com.apple.airplay:APBrowserCarSessionHelper] [Bonjour/USB] WiFi eligible: N (<disabled>),
                                                                        USB eligible: N (No Network Interface)

Every eligibility verdict in the whole capture — 257 of them — is **N**. The distribution:

| verdict | count |
|---|---:|
| `N (No Network Interface)` | **180** |
| `N (<disabled>)` | 68 |
| `N (Session Start Timeout, No Remote IP)` | 3 |
| `N (Session Start Timeout, No Remote IP, No Local IP)` | 1 |
| `N (No Local IP)` | 1 |

**The USB network interface — our NCM function — is simply not there.** iAP2 is fine throughout:
`accessoryd` logs `Protocol [iAP2] detected`, `Link state changed! (Connected: 1)`, the MFi
certificate chain parses. `carkitd` is fine too: it builds the display configuration
(`displayScaleMode=2`) and asks for advertising every cycle. Then `airplayd` has no network to
advertise on.

## The one success of the evening confirms the gate

    23:36:57.177  USB eligible: N (No Local IP)                        <- interface appeared, no address yet
    23:36:57.184  USB eligible: N (Session Start Timeout, No Remote IP)
    23:36:58.179  USB eligible: N (Session Start Timeout, No Remote IP)
    23:37:00.034  Created APEndpointCarPlay                            <- session up
    23:37:16.237  USB eligible: N (Session Start Timeout, No Remote IP)

The interface came up, addressing lagged ~3 s, the session established. That is the only time all
evening it got that far — 5 verdicts out of 185 even saw an interface.

## Why, and it is our defect

`deploy/smartphone_integrator/README.md` already documents the hazard:

> rapid OTG stop/start cycles can leave the iAP2 NCM accessory with **only one of its two
> interfaces enabled**

That is exactly `No Network Interface`: the composite gadget comes back with iAP2 but without NCM.
And the ~15 s dio generation death we measured on the unit **is** a rapid stop/start cycle. The
chain:

    dio generation dies at ~15 s
      -> OTG stop/start churn
        -> gadget re-enumerates with the iAP2 interface only, NCM missing
          -> phone: iAP2 up, displayConfig built, advertising requested
            -> airplayd: USB eligible: N (No Network Interface), no session
              -> repeat every ~15 s

It also explains the two field observations that looked contradictory: a head-unit reboot gives
one clean enumeration (so it works for a while), and `carplay0` carrying only ~5 packets of IPv6
Neighbour Discovery per cycle is exactly a link with no addressing.

## Retracted

Three theories built during this investigation are **wrong** and are recorded here so they are not
rebuilt:

1. *"The head unit is not advertising its Bonjour service."* Wrong direction — the **phone**
   advertises (`Starting USB Bonjour advertising with host`, `StartBonjourForUSB`); the unit
   discovers. What the unit's `dns-sd -B` browse showed was the phone's service.
2. *"iOS 27's `updateBonjourHost` repairs a malformed host from the accessory."* Neither version
   takes a host from the accessory; both construct it on the phone.
3. *"iOS <= 26's single car-wide `zoomFactor` cannot reconcile our cluster/main density mismatch."*
   The zoom never reaches that decision — and the capture shows the display configuration being
   built successfully every single cycle. The physical-size and `maxFPS` fixes
   (`292 x 110`, `30`) remain correct on their own merits, but they are not this bug.

The iOS-version dependence is still unexplained: this capture is iOS 26 only, so why iOS 27
tolerates the same churn is open. It is now a narrow question, and secondary — the churn is ours.

## Where to work

Stop the generation churn, or make the NCM function survive it. Relevant existing knobs:
`restartDelay: 3000`, `stopTimeout: 8000`, `portResetTime: 250 ms`, and the guarded USB reset in
`carplay_processes.sh` (`cp_usb_stuck_pre_setup`, armed in c4912d5). Measured previously and worth
remembering: `reset port 3 250 1` makes it **worse** — `drivers_running` went 1 -> 0.

Capture kept locally at `logs/` (gitignored: ~1 GB and it is the owner's device log). Re-query with:

    log show --archive logs/system_logs.logarchive --style compact \
        --predicate 'eventMessage CONTAINS "USB eligible"'
