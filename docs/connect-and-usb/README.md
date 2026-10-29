# Connect and USB

Wired CarPlay uses USB role swap, an NCM network interface (`carplay0`), iAP2 identification and a session owned by `smartphone_integrator` (SI).

## 🔌 Connection and recovery

The phone connects through the head unit's soldered USB hub. The device-mode stack exposes NCM, then iAP2 authenticates the accessory and starts CarPlay. SI owns startup, retry and teardown. The installer applies the SI re-arm patch, fingerprint-gated NCM generation fence and OTG cleanup integration.

The CarPlay startup wrapper loads the hook and starts the renderer monitor. USB stack control stays with SI and the native launcher.

## 📚 Technical guides

| Guide | Content |
| --- | --- |
| [USB and NCM Connect](USB_AND_NCM_CONNECT.md) | Enumeration, role swap, interface creation and timing |
| [USB Replug Recovery](USB_REPLUG_RECOVERY.md) | Current recovery components and diagnostic states |
| [SI Stack and Replug Race](SI_STACK_AND_REPLUG_RACE.md) | Stock retry state machine and teardown ownership |
| [Session Lifecycle](SESSION_LIFECYCLE.md) | Control SETUP, generations and teardown |
| [PF Firewall](PF_FIREWALL.md) | Interface rulesets and required port changes |
| [HMI Startup and Identity](HMI_STARTUP_AND_IDENTITY.md) | Main-menu identity and context activation |
| [Vehicle Data to Phone](VEHICLE_DATA_TO_PHONE.md) | Speed, vehicle status, phone battery and Siri controls |
| [Now Playing Cold Start](NOW_PLAYING_COLD_START.md) | Media metadata and startup synchronization |
| [Wireless implementation](../wireless-carplay/README.md) | SI-owned wireless transport and integration status |

For a failed connection, start with interface/process state and the current diagnostic table in USB Replug Recovery. Earlier version comparisons and recovery experiments are in [the archive](../archive/README.md).
