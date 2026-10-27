# iOS wireless CarPlay gates: stock 210 requires a pairing/crypto extension

_MHI2Q wireless CarPlay — technical reference, integrated 2026-09-29._

---

[Wireless overview](README.md) · [Current stock210 lifecycle](STOCK210_RECEIVER_LIFECYCLE.md)

Maintained technical reference. The evidence below distinguishes static analysis, local QNX tests and observations from the unit.

Implementation and local diagnostic paths in this record refer to the [original source directory](../../wireless/). Run `wireless/...` commands from the repository root; moving this note does not move binaries or logs.

## 📊 Component flow

```mermaid
flowchart LR
    accTitle: Inspected iOS wireless endpoint gates
    accDescr: Static iOS analysis requires pairing on the non-USB endpoint before encrypted transport and media; a feature advertisement alone cannot implement that path.
    discovery["Discover wireless endpoint"]
    capability["Recognize pairing capability"]
    pairing["Complete CU pairing"]
    verify["Verify stored peer"]
    transport["Enable authenticated transport"]
    discovery --> capability --> pairing --> verify --> transport
```

Offline static audit, 2026-09-28. This supersedes the proposed **direct wireless
stock-210 path with bit 37 enabled and bit 38 disabled** in the earlier notes.
No HU connection, binary patch, restart, or deployment was performed.

Follow-up: [airplay280_pairing_recipe.md](AIRPLAY280_PAIRING_RECIPE.md) maps
the 280.33.8 receiver source to Setup/Verify, MFi signing, the exact encrypted
transport transition, and the separate event/media keys needed around 210.
It also records the iOS distinction between StartSession's pairing identifier
(`pi`) and the Ed25519 public key (`pk`).

<a id="result"></a>

## 📋 Result

For the inspected iOS 26.6 and 27 binaries, creation of a CarPlay endpoint
requires **USB connectivity OR CoreUtils pairing/encryption support**.
Stock MU1316 AirPlay 210.81 plus feature bit 37 does not satisfy the wireless
branch. Its old MFi authentication implementation is still present in iOS,
but that does not bypass this earlier CarPlay-specific gate.

Moreover, the iOS 27 direct iAP2 StartSession path synthesizes feature bit
38 itself. Omitting that bit in our Bonjour TXT or stock `/info` is not a
way to select legacy-only wireless operation through this path. Once pairing
succeeds, iOS configures encrypted control transport using the CU key holder.

This is a protocol capability requirement, not evidence of a blacklist for
Audi/MHI2Q or a blanket rejection of the version string `210.81`. Keeping
the original library is still possible, but requires a real protocol extension
in the hook/front end, including the resulting stream security.

<a id="inputs-and-evidence-quality"></a>

## 🧪 Inputs and evidence quality

All iOS 27 addresses below are unslid ARM64 virtual addresses from build
`24A5418b`, iPhone18,1:

- `Refferences/ios27_dsc/24A5418b__iPhone18,1/aps_out/AirPlaySender`,
  base `0x250F35000`, SHA-256
  `379fb232ff228287d3b11d1253b1b942e6c50dd633d150cfab6acb9d063bead1`.
- `Refferences/ios27_dsc/24A5418b__iPhone18,1/APTransport.i64`,
  base `0x24F331000`. This part was inspected from the existing IDA database;
  its original input was `/private/tmp/ios-aptransport/APTransport`.
- iOS 26.6: `Refferences/ios266_extract/dylibs/AirPlaySender`, SHA-256
  `952d73f8e97d4697490f69bdf08067df6ccff29f5b44c60489eb117db015d832`.
- iOS 26.1: existing `.mm` decompilation under
  `Refferences/iPhone18-3_26.1_23B85_Restore/System/Library/PrivateFrameworks/CoreAccessories.framework/`.
  References below use paths relative to this directory. These pseudocode
  exports sometimes omit imported function arguments. Where this prevents
  identifying a property/bit, the 26.1 result is structural corroboration,
  not a separately verified numeric argument.

IDA decompilation was checked against ARM64 instructions for the decisive
property names. Extracts are in [output/ios_static_audit](../../wireless/output/ios_static_audit).
Imported prototypes in these extracts are incomplete: `MEMORY[...]()` calls
must not be treated as accurate C ABI declarations.

<a id="1-mandatory-wireless-pairing-gate-independent-of-discovery-method"></a>

## 🔐 1. Mandatory wireless pairing gate, independent of discovery method

`AirPlaySender::_APEndpointCarPlayCreateWithTransportDevice`
at **`0x25107EA08`** first constructs an endpoint description from the
transport device, then performs these checks:

```c
if (!description.IsCarPlay)
    fail();
if (!description.IsConnectedOverUSB &&
    !description.SupportsCoreUtilsPairingAndEncryption)
    fail(-71754);
// Only then allocate the CarPlay endpoint.
```

| Instruction / branch | Meaning |
|---|---|
| `0x25107EA78..0x25107EA8C` | Load `IsCarPlay`; reject false. |
| `0x25107EA94..0x25107EAAC` | Load `IsConnectedOverUSB`; true branches past the CU check. |
| `0x25107EAB4..0x25107EAC8` | Load `SupportsCoreUtilsPairingAndEncryption`; false branches to `0x25107F28C`. |
| `0x25114B490` | `cold.15`; `0x25114B4B0` sets low word `0xE7B6`, helper `0x250F411F4` ORs `0xFFFE0000`: signed error `-71754`. |

`_airPlayDescription_supportsCUPairingAndEncryption` at **`0x250F5C704`**
returns true for any of bits **38, 46, 43, 48**. None is in the proposed
stock-plus-37 mask `0x0000002144440B80`.

The iOS 26.6 function at **`0x2277F6590`** has the same branches:
USB property call `0x2277F6630`, CU property call `0x2277F664C`, reject
`0x2277F6650 -> 0x2277F6D58`. Property symbols are at `0x278C0B998`
(`IsConnectedOverUSB`) and `0x278C0BA80` (`SupportsCoreUtilsPairingAndEncryption`).
Its CU predicate at `0x2276E6D98` tests the same four feature bits.
The 26.1 export `System/Library/PrivateFrameworks/AirPlaySender.framework/AirPlaySender/AirPlaySender_14.mm:1653-1670`
shows the same description/three-property-check/`cold_15` structure, but
omits the property arguments; it is not an independent numeric verification.

`IsConnectedOverUSB` is an iOS transport property, not an advertised feature
bit. In APTransport, `___carSessionBrowser_handleCarPlayHelperEvent_block_invoke`
at `0x24F391E28` reads `interfaceType` at `0x24F391F60` and sets USB true
only for interface type 1 at **`0x24F39243C`**. The nearby log labels this
branch `AP_SIGNPOST_ENDPOINTADDED_USB`. The getter is
`_APTransportDeviceIsConnectedOverUSB` at `0x24F359EA8`.

Consequences: the gate permits the legacy wired path; changing sourceVersion
or choosing Bonjour discovery does not remove the requirement at CarPlay
endpoint creation. This alone does not implement USB priority or seamless
handover on the HU; our process/session ownership still has to do that.

<a id="2-iap2-startsession-synthesizes-modern-pairing-support"></a>

## 🔐 2. iAP2 StartSession synthesizes modern pairing support

`APTransport::_carPlayHelperSession_updateNetworkAndSessionState`
at **`0x24F3426A4`** processes the host supplied by CarKit and constructs
an advertiser description. In the direct-host branch it creates a new
features object and calls `APSFeaturesSetFeature` as follows:

| Call | Bit | Condition |
|---|---:|---|
| `0x24F3433E8` | 7 | Unconditional after successful allocation. |
| `0x24F3433F4` | 9 | Same. |
| `0x24F343400` | 32 | Same. |
| `0x24F34340C` | 37 | Same. |
| **`0x24F343418`** | **38** | **Same; no sourceVersion or publicKey guard.** |
| `0x24F343530` | 26 | Host does not request mutual authentication. |
| `0x24F343438` | 80 | Host requests mutual authentication instead. |

It then writes the host's `sourceVersion` to `airPlayVersion`
(`0x24F3439D4`), the synthetic object to **`featuresEx` (`0x24F343A54`)**,
and the host's public key to `publicCUAirPlayPairingIdentity`
(`0x24F343A84`). These are separate assignments; supplying `210.81` does
not suppress bit 38. The helper dispatches the resulting endpoint information
at `0x24F343C94`.

The 26.1 `APTransport/APTransport.mm:4897-4924` has the same five initial
feature calls and the mutual-authentication fork, but its export omits their
arguments. The exact bit numbers in this table were verified in iOS 27.

Our `wireless/daemon/src/control.rs` sends precisely this classic
`CarPlayStartSession` flow. Thus the discovery-only assumption “our TXT has
no bit 38, so the phone will not attempt pairing” is invalid for our design.

<a id="3-info-does-not-rescue-a-legacy-only-receiver-before-pairing"></a>

## 🔐 3. `/info` does not rescue a legacy-only receiver before pairing

`AirPlaySender::_APEndpointDescriptionAirPlayCreateWithTransportDevice`
at `0x250F45B9C` builds the initial description from transport advertiser
information, including the synthetic information above.

`_apsession_ensurePublicInfoFetched` at **`0x2510FC7DC`** returns without
fetching public info when usage is 5 (**`0x2510FC848`**), the CarPlay usage.
Otherwise it reaches `apsession_fetchInfo` at `0x2510FC9C0`. The 26.1
CarPlay activation code in `AirPlaySender_14.mm:3743` passes usage `5u` to
`APSenderSessionAirPlayCreate`; its connection code also skips initial info
for that usage (`AirPlaySender_07.mm:2804`).

In iOS 27:

1. `_apsession_ensureConnectedInternal` at `0x250FFA224` calls
   `_apsession_ensurePaired` at **`0x250FFC898`**.
2. `_apsession_ensurePaired` at `0x250FFD850` enters pairing if CU support
   is true (`0x250FFDA7C..0x250FFDB20`); its no-pairing return is not taken.
3. `_apsession_createPairingClient` at `0x250FFFF68` selects
   `APPairingClientCoreUtilsCreate` at **`0x2510002BC`** for CU support,
   versus the legacy client at `0x251000314`.
4. `_coreUtilsPairing_PerformVerification` at `0x251126714` sends request
   type `1885435493` / `pave` at `0x251126C94`. APTransport's
   `_APTransportPackageHTTPGetHTTPMessage` at `0x24F34B8D8` maps it to
   **`/pair-verify` at `0x24F34C104`**; it also maps setup to `/pair-setup`.
5. A failed verification can enter setup/retry logic. That is not a fallback
   to an unpaired legacy wireless session: setup/verify errors propagate out
   of `ensurePaired`, and the caller aborts on error at
   **`0x250FFC99C -> 0x250FFCE3C`**.

The order of the first pairing request depends on saved peer identity and
pairing state. This audit does not assume `/pair-setup` always precedes
`/pair-verify`. A missing iAP2 publicKey is accepted by the earlier parser;
it does not remove the later pairing requirement.

<a id="4-successful-pairing-changes-transport-security"></a>

## 🔐 4. Successful pairing changes transport security

The CU capability is read at `0x250FFA3B0..0x250FFA3C4` and retained by
`ensureConnectedInternal`. After pairing it calls
`_apsession_ensureKeyHolderSetUp` at `0x250FFCBA8` and
**`APTransportStreamConfigureEncryption(..., "Ctrl")` at `0x250FFCC04`**.

`_apsession_ensureKeyHolderSetUp` at `0x2510FBAD4` chooses the CU key holder
at `0x2510FBBAC` when that property is true; the legacy key holder call at
`0x2510FBBE4` is the other branch.

`_coreUtilsKeyHolder_CreateEncryptionKeysForContext` at `0x250FD48DC`
derives control/event keys using `Control-Salt`, `Control-Read-Encryption-Key`,
`Control-Write-Encryption-Key`, and the corresponding `Events-*` strings.
The default key sizes are 32 bytes. `_coreUtilsKeyHolder_CreateEncryptionKeysForStream`
at `0x250F5EED4` uses `DataStream-Salt%llu` and input/output encryption labels.

APTransport's `_httpconnection_configureEncryptionInternal` at
`0x24F36F7C8` obtains these keys, rejects lengths below 32 bytes
(`0x24F36F8B0`, `0x24F36F8C0`), configures the cryptographic transport
at `0x24F36F904` or `0x24F36F9B8`, and installs its delegate at
`0x24F36FA14`. The equivalent 26.1 function names those calls
`NetTransportChaCha20Poly1305Configure`
(`APTransport_03.mm:1230-1320`).

Therefore an HTTP route hook must also handle the transition to encrypted
records on that connection. Stream security must be carried through the
media/event setup; a pairing-only server forwarding all other bytes directly
to the stock 210 listener is incomplete. The full per-stream mapping to
210's legacy receiver is still implementation work, not proven by this audit.

<a id="5-earlier-carkit-gates-still-apply"></a>

## 📋 5. Earlier CarKit gates still apply

In the 26.1 carkitd export, classic iAP2 StartSession handler
`sub_100024B20` (`usr/libexec/carkitd/carkitd.mm:5050`) checks restrictions,
resolves the messaging accessory, and requires the expected Wi-Fi parameter
keys. `_findWirelessPairedVehicleForMessagingVehicle` must identify the
wireless paired vehicle before the handler includes wireless IP addresses
and saves Wi-Fi credentials (`sub_100025600` callback and host construction).
These observations concern the 26.1 export; its exact predicates were not
independently remapped into iOS 27 carkitd during this pass.

The classic iAP2 publicKey remains optional at parsing time. The BLE
`CARCarPlayServiceMessageStartSession` mandatory-key check is a different
protocol path. Neither observation establishes acceptance of a later
unauthenticated session. Keep a real, persistent pairing identity and use
the owner's MFi chip; do not confuse iAP2 MFi authentication, AirPlay MFi-SAP,
and CU pairing as interchangeable steps.

No minimum `sourceVersion >= 280` check was found in the traced endpoint
creation/StartSession/pairing selection path. This is a bounded finding,
not a claim that every iOS component and every possible stream negotiation
has been exhausted.

<a id="engineering-consequence-for-this-repository"></a>

## 📋 Engineering consequence for this repository

Keep the stock 210 library and wired SI/dio_manager integration if desired.
The required wireless addition is now explicit:

1. `btstack` graft: verified SDP/RFCOMM bridge only; no AirPlay crypto in
   the Bluetooth process.
2. iAP2 daemon: MFi, vehicle/Wi-Fi bootstrap, and StartSession with a real
   stable pairing identity matching the wireless receiver.
3. Hook/front end: CU pairing, encrypted control/event transport, and
   correct media-key/stream integration or translation into stock 210.
4. Receiver bridge: start/stop and single-owner arbitration with wired SI,
   including USB priority and teardown of the wireless session.

The next offline implementation task is to define the pairing/crypto boundary
around the validated stock `HTTPServerCreate` delegate seam, and account for
control, event, screen, and audio key/transport ownership. The bootstrap
reference and CatPlay RX can inform that component; neither proves that the
current hook already implements it. Advertising modern capabilities must
follow implementing them. Static proof of the gate is complete; full
interoperability of the eventual implementation remains a separate result.
