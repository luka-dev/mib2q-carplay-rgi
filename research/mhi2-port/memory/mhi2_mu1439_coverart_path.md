---
name: mhi2_mu1439_coverart_path
description: How CarPlay cover art flows (and is dropped) in MHI2 MU1439 dio_manager — porting target for coverart feature
metadata: 
  node_type: memory
  type: project
  originSessionId: a880bce1-4faa-4ed2-b8d7-2c43da3c7ca4
---

RE of `MHI2_US_AUG22_K3347_MU1439/extracted/app70/eso/bin/apps/dio_manager` (Audi MHI2, Tegra3, ARM32 QNX). Cover-art situation for the port (see [[mhi2_mu1439_port_analysis]]):

**dio_manager exposes a DSI CarPlay interface to the HMI:** service `dsi.carplay.DSICarplay`, UUID `B60FF3E0-C276-5843-8023-5ABA5E2AC150`, impl = `CDSICarplayImpl` (284 B, `operator new(0x11C)`; created in `CDIOManager::startDSIInterface` sub_13A458, stored at DIOManager+0x228 = m_pCarPlayDSI; registered in sub_18089C). All now-playing metadata flows over it during a CarPlay session:
- `onEvent_updateNowPlayingTrackInfo` ~0x141700 → CDSICarplayImpl::updateTrackState sub_14B1BC: sets title/album/artist/composer/duration/genre.
- `onEvent_updateNowPlayingPlaybackInfo` ~0x142A00 → sub_14B0B4: status/repeat/shuffle/appName.
- All gated on m_pCarPlayDSI (CarPlay-only; assert "NULL == m_pCarPlayDSI" DIOManager.cxx:0x6AA).

**Cover art = DSI attribute ID 6 `coverArtUrl`, but it is DEAD skeleton code — never populated.** The generated server (unnamed fn 0x17EF0C; reportAttribute sub_17D394 case 6 reads value at +0xB0; setNotification sub_17D9FC; addClient sub_17DB58 subscribes all 10 attrs incl ID 6; listener dispatch via vtable +0x24, error str "listener_updateCoverArtUrl: unable to forward call to peer!: %s" @0x199e1c). No `onEvent_*CoverArt*` handler exists, no coverArtUrl in the PPS metadata key tables (CPPSNowPlayingMetadataObject sub_16D164, tables off_19E6E8/off_19E6B8). **`iap2_nowplay_getartwork` is NOT imported/called by dio_manager at all** — artwork from the incoming NowPlayingTrackInfo is simply discarded.

**Verdict:** native artwork NOT reusable as-is (attribute never set; and even if set it carries a URL string for the HMI to fetch, not image bytes). HMI almost certainly doesn't render attr 6 either (mirrors MHI2Q blank-art problem).

**Artwork fetch API (RE of libiap2client.so.1, fully exported, thin QNX-IPC shim — no image logic, just MsgSendsv to the iap2 daemon):**
- `int iap2_nowplay_getartwork(int *handle, void *buf, int buflen)` @0x18c4, msgcode 0x2100. **NO artwork/track id param — implicitly current now-playing track.** Caller-allocated buffer (caller owns). Returns server status word (=byte length on success, 0-path) / negative IPC error. **Two-phase:** call buflen=0 → required size; alloc; call again. Self-contained (daemon does the iAP2 file-transfer internally; do NOT chain filexfer_read). Format opaque — verify magic (FF D8 FF=JPEG, 89 50 4E 47=PNG); iOS usually JPEG.
- `int iap2_filexfer_read(int *handle, int xfer_id, void *buf, int buflen)` @0x15cc, msgcode 0x2900 — generic file-transfer reader by explicit id; NOT needed for artwork path.
- `iap2_connect(const char *devpath, int oflag)` @0x2cec → opaque 0x18-B handle {fd/coid, aux}; handshake MsgSend code 0x9999 expects reply status==2. `iap2_ppsdir_get` @0x153c gives the iap2 PPS dir. No NowPlayingUpdate struct / callback / "artwork available" flag in this lib — availability is discovered via **PPS** (PPS_IAP2_NOWPLAYING_METADATA, event type 30) which is exactly how dio_manager learns track changes.

**CORRECTION:** dio_manager DOES link libiap2client.so.1 (in NEEDED) and imports iap2_connect/iap2_eap_recv/iap2_eap_send/iap2_eap_open/iap2_gps_data_send/iap2_disconnect — so an LD_PRELOAD hook in dio_manager can call `iap2_nowplay_getartwork` directly (symbol already resolvable, NO dlopen). dio_manager just never calls getartwork itself (uses PPS for metadata). It holds a live iap2 handle from its own iap2_connect; our hook can open its own connection if the daemon allows a 2nd client.

**Recommended port (cleaner than MHI2Q chunked-JPEG reassembly):** LD_PRELOAD hook in dio_manager watches PPS nowplaying for track change → `iap2_nowplay_getartwork` (probe+fetch) → verify magic → decode→PNG→tmpfs → signal HMI over TCP bus. **DECIDING UNKNOWN (runtime-only):** does the iap2 daemon's getartwork return bytes during a CARPLAY session, or does CarPlay artwork ride the AirPlay path (making getartwork empty)? Only answerable by (i) reversing the iap2 *server* daemon binary (msgcode 0x2100 handler — NOT in this shim) or (ii) a live-unit probe. If empty during CarPlay → fall back to manual reassembly by hooking iap2_eap_recv/iap2_filexfer_read on the CarPlay nowplaying file-transfer stream (port MHI2Q approach). HMI/Java side (rendering art on VC) needs its own patch regardless — derive against extracted Audi MHI2 lsd.jxe.
