# iOS 26/27 selection of stock AirPlay 210 MFi-SAP

**Correction after tracing the enclosing CarPlay path (2026-09-28):**
the selector below survives, but does not establish a usable legacy-only
wireless path. iOS 26.6/27 CarPlay endpoint creation requires USB **or**
CoreUtils pairing/encryption; iOS 27 iAP2 StartSession synthesizes bit 38
before pairing and skips the preliminary public `/info` fetch. The earlier
proposal to enable only bit 37 is insufficient. See
[ios_wireless_session_gates.md](ios_wireless_session_gates.md) for the
verified branches, addresses, and required hook/crypto work.

Static, offline audit of the owner's iPhone 18,3 iOS 26.1 decompilation,
`ios266_extract/dylibs/AirPlaySender`, and iPhone 18,1 iOS 27.0 extracted
`aps_out/AirPlaySender` and feature flags. It answers whether the inspected
client code categorically rejects the MU1316 stock 210.81 authentication
path. It is not an iPhone interoperability result.

## AirPlay authentication choice

The pinned MU1316 `/info` mask, with only CarPlay Control bit 37 added, is
`0x0000002144440B80`. It retains **bit 26** and omits **bit 38**. Bit 26 is
already present in stock; bit 37 is our proposed advertisement.

| iOS | Function | Static result |
|---|---|---|
| 26.1 | `AirPlaySender_02.mm:1139`, `airPlayDescription_copyAuthenticationType` | Checks 80, then **26 or 51 -> type 8 (MFi)**, then 14/23/31. `AirPlaySender_10.mm:7843` creates `APAuthenticationClientMFi` for type 8; `AirPlaySender_17.mm:6133-6203` calls `MFiSAP_Create/Exchange`. |
| 26.6 | `AirPlaySender` `0x2276F2D2C` | Same feature tests and type-8 branch (`0x2276F2D64-0x2276F2D84`); `APAuthenticationClientMFiCreate` at `0x22783ABF4`. |
| 27.0 | `AirPlaySender` `0x250F6980C` | Same feature tests and type-8 branch (`0x250F69844-0x250F69864`); `APAuthenticationClientMFiCreate` at `0x2510C5BB0`, `mfiAuthentication_AuthenticateEndpoint` at `0x2510C5F40`. |

In both 26.6 and 27.0, `airPlayDescription_supportsCUPairingAndEncryption`
checks feature bits **38, 46, 43, 48** (`0x2276E6D98` and `0x250F5C704`).
All are absent from our stock-210 mask. The 26.1 session code in
`AirPlaySender_20.mm:1140-1190` retains both `APKeyHolderLegacyCreate` and
`APKeyHolderCoreUtilsCreate`; the MFi authentication path is not dead code.
This proves preservation of the legacy authentication selector. It does
**not** justify advertising bit 37 alone for wireless CarPlay: the subsequently
traced endpoint-creation gate requires CU support for a non-USB endpoint.

## Do not confuse Bluetooth/iAP2 with BLE StartSession

The iOS 26.1 `CoreAccessories_15.mm:5269` handler
`iap2_CarPlayStartSession` parses iAP2 message `0x4301`; param 4 is an
optional string (`CoreAccessories_15.mm:5760-5810`). It calls
`platform_CarPlay_startSession`, which inserts `publicKey` in the properties
dictionary only when supplied (`CoreAccessories_08.mm:4338-4493`). In
`carkitd.mm:5050-5180`, the classic iAP2 connection request copies **present**
properties into a `CARSessionRequestHost`; its parser
(`CarKit/CARSessionRequestHost.mm:55-130`) accepts a missing key as nil.

By contrast, `carkitd/CARCarPlayServiceMessageStartSession.mm:388-405`
rejects a missing `publicKey`, and the iOS 27.0 version has the same null
branch at `0x100025CE4-0x100025D14`. This class belongs to
`CARBluetoothCarPlayService`'s BLE message-class map, not the classic iAP2
`0x4301` decoder. Its rejection must **not** be projected onto our RFCOMM
path. The current Rust daemon leaves `public_key` optional; that is not a
proven iAP2 parser blocker from these call paths. A real pairing identity is
nevertheless needed for the required wireless pairing implementation; iOS 27
sets bit 38 itself in the direct StartSession host description.

## Current conclusion and remaining gates

The exact stock feature mask routes to type-8 MFi in the 26.1/26.6/27.0
authentication selector. The inspected feature flag plists have no explicit
entry disabling legacy MFi-SAP. However, the enclosing wireless CarPlay path
has an earlier, mandatory CU pairing/encryption gate, now verified statically.
The direct stock-210-plus-bit37 proposal fails that gate; the presence of
legacy MFi-SAP is consistent with continued wired support. The next work is
pairing/crypto integration around stock 210, including encrypted control and
stream key handling, as detailed in `ios_wireless_session_gates.md`.

## Independent dongle references (28 September 2026)

`/Users/luka/Desktop/AUDI_2/Refferences/catplay` at commit `13d174e`
provides useful independent implementations of the Bluetooth/iAP2 and
AirPlay endpoints, but its two roles must be kept separate:

- Its **receiver**, which talks to a real iPhone, advertises AirPlay
  `srcvers=320.20` (`carplay/catplay_carplay/src/common/mod.rs:9`,
  `carplay_rx/sink/receiver.rs:400,534`). It exposes `/auth-setup` and
  `/pair-setup`/`/pair-verify` (`receiver.rs:296-299`), advertises both MFi-SAP
  feature bit 26 and HomeKit pairing bit 38 (`receiver.rs:345-363`), and uses
  the MFi chip for `/auth-setup` (`pairing/pairing_helper_rx.rs:209-239`). Its
  claimed iOS 26+ testing therefore does **not** validate direct iPhone use of
  the stock MHI2Q 210.81 receiver with bit 38 absent.
- Its **transmitter**, which emulates a phone to a wired car, explicitly
  distinguishes pre-HomeKit 210 receivers from later ones
  (`carplay_tx/transmitter.rs:361-367`). It may send SETUP without
  `/auth-setup` and downgrade stream encryption to none
  (`transmitter.rs:369-413`); this is dongle-to-car behavior, not evidence of
  what iOS 26/27 will send to our 210.81 receiver. The current upstream README
  reports a TechniSat MIB2/MST2 210.81 car compatible, but says the HARMAN
  MIB2/MHI2 210.81 path has USB power/role-switch issues and pending fixes.
- Its Bluetooth profile advertises the same iAP2 server/client UUIDs as the
  donor image, with an RFCOMM SDP channel (`bluetooth/catplay_bt/src/iap2.rs:
  14-19,27-49`), while BlueZ handles socket acceptance
  (`bluez_manager.rs:140-220`). The UUID and state-machine behavior are useful
  references; its BlueZ object model and hard-coded channel 19 cannot be used
  as the QNX Blue SDK channel layout or allocation recipe.

`/Users/luka/Desktop/AUDI_2/Refferences/wireless-carplay-dongle-reverse-engineering`
contains a 1.4 GB `.git` directory but no checked-out files or valid HEAD,
so there was no local code to inspect. Its public upstream README describes
Carlinkit USB wireless-to-wired adapters, not an in-head-unit patch. Neither
reference supplies a complete, validated in-HU integration. The later static
iOS gate audit establishes why a pairing/crypto extension is required for
direct wireless operation around stock 210.
