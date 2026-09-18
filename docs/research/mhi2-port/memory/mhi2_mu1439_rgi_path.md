---
name: mhi2_mu1439_rgi_path
description: How to do iAP2 route guidance (RGI) on MHI2 MU1439 — Apple stack has no native RG; enable via cfg + intercept in mm-ipod
metadata: 
  node_type: memory
  type: project
  originSessionId: a880bce1-4faa-4ed2-b8d7-2c43da3c7ca4
---

RGI port recipe for MHI2 (MU1439), from RE of `ipod-drvr-iap2.so` (≈740KB, ARM32 QNX, the iAP2 protocol stack hosted by the **mm-ipod** process; `dio_manager` is only a libiap2client client). See [[mhi2_mu1439_port_analysis]]. Feasibility verdict: **MEDIUM (viable)** — same architecture as MHI2Q `c_hook/routeguidance/` but in a different host process and with a cleaner enable mechanism.

**Key difference vs MHI2Q:** MHI2Q's Cinemo SDK natively supported route guidance (knew msg IDs 0x5200-0x5204); patch just enabled it. MHI2's Apple reference stack (2018) has **ZERO native route-guidance support** — the control-session dispatcher `link_handle_iap2pkt` @0x12e00 has no 0x52xx cases (verified: 0 matches for Route/Guidance/Maneuver/Lane/0x52xx anywhere). Unknown msg IDs are **silently dropped** (default case returns 0, no PPS/callback forward). iAP2 `[location]` cfg feature is accessory→device NMEA only (HU sends GPS to phone), NOT route guidance reception. No `[routeguidance]` cfg section exists.

**STEP 1 — Enablement (NO binary patch needed, unlike MHI2Q's runtime Identify patch):** add to `/etc/mm/iap2.cfg`:
```
[ident]
recvmsgs=0x5201,0x5202,0x5204
sendmsgs=0x5200,0x5203
```
`ident_info_recvmsgs` @0xbD60 and `ident_info_sendmsgs` @0xc46c build the IdentificationInformation arrays feature-by-feature then **append comma-separated hex IDs from `[ident] recvmsgs=`/`sendmsgs=`** (strtoul, cap 64 IDs / 0x80 bytes, warns "Cannot add custom messages" if exceeded). So iOS sees 0x52xx in MessagesReceivedFromDevice/SentByAccessory and starts sending route guidance. (No route-guidance feature flag in `init_features` @0xe2cc; no EAGroupComponent cfg knob — OPEN: confirm on a live unit whether iOS also requires a navigation *component* declaration, like MHI2Q's EAGroupComponent 0x001E, or whether msg-ID arrays alone suffice. Likely sufficient — route guidance is a core iAP2 control feature, not an EA-app component.)

**STEP 2 — Intercept (LD_PRELOAD into mm-ipod):** `link_handle_iap2pkt` @0x12e00 is an **exported dynamic symbol** (`T link_handle_iap2pkt`) → LD_PRELOAD interposition works directly, no binary patch (verify internal calls route via PLT; if `-Bsymbolic` defeats it, fall back to hooking libc `read` which is imported (`U read`), but that gives raw pre-deframe link bytes = more work). Wrapper signature: `int link_handle_iap2pkt(int link_ctx /*a1*/, int packet_obj /*a2*/)`. After it runs (or inspect a2): payload ptr = `*(uint8_t**)(a2+36)`, len = `*(uint16_t*)(a2+40)`; within payload: [0..1]=iAP2 length, [2..3]=0x4040 control marker, [4..5]=**big-endian msg ID**, [6+]=TLV body. On msg id 0x5201/0x5202/0x5204 → parse with ported `rgd_tlv` (directly portable from MHI2Q) → TCP bus → Java. NO `__thread` ([[qnx_tls_emutls_trap]]).

**Control-session dispatch table** (`link_handle_iap2pkt` cases, useful reference): 0x1D00 ident req, 0x1D02 IdentAccepted→iap2_start_features, 0x1D03 reject, 0x4155/0x4158 telephony, 0x4C01/0x4C04 medialib, 0x4E09/0x4E0A devnotify, 0x5001 nowplay_update, 0x5702 wifi, 0xAA00..05 auth, 0xA100/0xA102 vehicle, 0xAE01 power, 0xDA01 devaudio, 0xEA00/0xEA01 EAP, 0xFFFA/0xFFFC gps.

**STEP 3 — Java/HMI side:** unchanged problem — derive BAP/VC route-guidance rendering against extracted Audi MHI2 lsd.jxe (biggest remaining unknown; needs mifs-stage2 extraction).

**Deployment OPEN:** find where `mm-ipod` (/app/armle/usr/sbin/mm-ipod) gets its env to inject LD_PRELOAD (analog of MHI2Q's smartphone_integrator.json for dio_manager) — likely the mm/usb startup script (`start_usb_device_stack.sh` / `usblauncher_otg.lua` reference mm-ipod). Warm IDA agent for ipod-drvr: a74c4fdaea4e22c26.
