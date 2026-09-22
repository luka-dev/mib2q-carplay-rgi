---
name: mhi2_mu1439_port_analysis
description: Feasibility + architecture deltas for porting the MHI2Q CarPlay RGI patch to plain MHI2 (Audi MU1439 / Tegra3)
metadata: 
  node_type: memory
  type: project
  originSessionId: a880bce1-4faa-4ed2-b8d7-2c43da3c7ca4
---

Target: `/Users/luka/Desktop/AUDI_2/Firmwares/MHI2_US_AUG22_K3347_MU1439` (Audi US NAR, img_ver `CLU6_MMX2_AU_NAR_G22_055PROD`). Has Virtual Cockpit (Audi) → renderer path applies. Extraction via `Firmwares/qnx6_extract.py` (homebrew python3 has `dissect.qnxfs`). app.img = QNX6 fs; extracted app/70 → `Firmwares/MHI2_US_AUG22_K3347_MU1439/extracted/app70`. lsd.jxe lives in mifs-stage2 (LZO IFS), JSON configs in system partition — NOT yet extracted.

**Generation delta (the whole reason port is non-trivial):**
- MHI2Q (patch's current target = MU1316) = `MMX2QC` partition, **Qualcomm**, iAP2 via **Cinemo SDK `libNmeSDK.so`** in dio_manager → patch hooks libc `read()`/`recv()` to catch raw iAP2 bytes; registers RGD msg IDs in `dio_manager.json` MessagesSent/Received arrays (Cinemo-specific); runtime-patches Cinemo's outgoing Identify to inject EAGroupComponent 0x001E.
- MHI2 (MU1439, and VW MU1367) = `MMX2` partition, **Nvidia Tegra3**, iAP2 via **Apple reference stack**: `devu-iap2-tegra3-ci.so` driver + `mss-ipodiap2.so` + `libiap2client.so.1`. NO Cinemo, NO libNmeSDK. `dio_manager.json`/MessagesSent arrays trick does NOT exist here.

**Same across both:** ARM32 QNX 6.5 (EABI5, ldqnx.so.2) → C hook toolchain (QNX SDP 6.5 ARM) ports unchanged. libairplay both **210.81** (but different builds: MHI2Q 763420 B vs MU1439 — re-derive all offsets). Neither libairplay has altScreen strings → **AltScreen impossible conclusion holds for MHI2 too**.

**Decisive finds for the C-hook port (libiap2client.so.1 is NOT stripped, clean C API):**
- dio_manager links `libiap2client.so.1` and has `CDSICarplayImpl`, `gLogCategory_CarPlay`, `CSmartphoneLauncherService`, `CIAP2ClientEventListener`, and crucially **`CIAP2LocationsHandlerJob` / `CIAP2LocationsHandlerJobListener`** — route guidance = iAP2 "Location" feature, handled natively in dio_manager. MHI2 likely already advertises Location/RGD capability → the Identify-patch + msg-ID-registration step may be unnecessary (VERIFY).
- New hook point (vs MHI2Q's read/recv): exported `iap2_eap_recv`/`iap2_eap_send`/`iap2_raw_msg_send` in libiap2client, plus `iap2_nowplay_getartwork` (cover art), `iap2_gps_data_send`. LD_PRELOAD can override these named symbols directly — cleaner than raw byte interception. RGD likely arrives via raw control-msg callback registered through `iap2_connect`, surfacing in `CIAP2LocationsHandlerJob`.

**Reusable as-is:** rgd_tlv.c/.h (iAP2 message TLV format is SDK-independent), bus protocol, renderer (Audi VC + MOST), bargraph state machine. **Needs full re-derivation:** the iAP2 hook layer (point + Identify/capability enablement), every binary offset patch, and the entire Java patch (depends on MHI2 Audi lsd.jxe class structure — biggest unknown, must extract mifs-stage2 + diff against MU1316 BAPBridge/ClusterService/AppConnectorNavi).

See README "Why AltScreen is impossible" + [[qnx_tls_emutls_trap]] (no __thread in the .so).
