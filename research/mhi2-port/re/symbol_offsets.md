# Symbols & offsets — MHI2 (MU1439) iAP2 stack

All addresses are file/VA offsets in the respective ARM32 QNX ELF (VA = file off for these mappings).

## `ipod-drvr-iap2.so` (RGI binary-detour target)

Path: `extracted/app70/armle/lib/dll/ipod-drvr-iap2.so` (≈740 KB, ARM32, stripped of debug but retains 180 dynamic symbols incl. T-symbols listed below).

| Sym / VA | Role |
|---|---|
| `link_handle_iap2pkt` @ **0x12e00** | T; main control-session dispatcher (large `switch` on 16-bit msgid). **Detour target for RGI.** Internal driver function; due to DT_SYMBOLIC, LD_PRELOAD interpose is DEFEATED. Patch the .so on disk. |
| `link_handle_reply` @ 0x11a2c | T; called from link_handle_iap2pkt before the switch (reply path) |
| `ident_info_recvmsgs` @ 0xbD60 | T; builds `MessagesReceivedFromDevice` array of the Identification message; iterates per-feature msg tables, then **appends comma-separated hex IDs from `iap2.cfg [ident] recvmsgs=`** via `ipod_cfg_find(cfg,"ident","recvmsgs")` + strtoul. Capacity 0x80 / ≤64 IDs. Log: "Cannot add custom messages" on overflow. |
| `ident_info_sendmsgs` @ 0xc46c | T; symmetric for `MessagesSentByAccessory` / `[ident] sendmsgs=`. |
| `iap2_ctrl_msg_table` @ 0xace80 | D; sparsely-populated runtime struct (mostly zeros); NOT the static msg-ID dispatch table — the dispatch is the hardcoded switch in link_handle_iap2pkt. |
| `nowplay_getartwork` @ 0x1ba3c | T; server-side artwork getter (the IPC peer of libiap2client's `iap2_nowplay_getartwork`). |
| `nowplay_update_recv` @ 0x1bb18 | T; incoming NowPlayingUpdate handler (msgid 0x5001). |
| `init_features` @ 0xe2cc | T; sets feature bitmask from cfg per-feature `enable=yes` flags. NO route-guidance feature exists — confirmed absent. |

### Driver's control-session message switch in `link_handle_iap2pkt` (the full case set — no 0x52xx)

For reference (helps the detour design — picking the default arm to patch):

| msg id | Handler |
|---|---|
| 0x1D00 | `ident_info_request` (StartIdentification) |
| 0x1D02 | IdentificationAccepted → `iap2_start_features` |
| 0x1D03 | IdentificationRejected |
| 0x4155 / 0x4158 | telephony |
| 0x4C01 / 0x4C04 | medialib |
| 0x4E09 / 0x4E0A | devnotify |
| 0x5001 | `nowplay_update_recv` |
| 0x5702 | wifi |
| 0xA100 / 0xA102 | vehicle |
| 0xAA00..05 | auth |
| 0xAE01 | power |
| 0xDA01 | devaudio |
| 0xEA00 / 0xEA01 | EAP session |
| 0xFFFA / 0xFFFC | gps start/stop |
| **0x52xx** | **none — silently dropped via default arm; this is what we hijack** |

### Packet object layout (per prior RE; verify on prologue trace)

- `pkt + 0x24` → `uint8_t *payload` (pointer to control-session payload buffer)
- `pkt + 0x28` → `uint16_t length`
- Within payload: `[0..1]` = u16 length; `[2..3]` = `0x4040` control-session marker; `[4..5]` = **big-endian msg id**; `[6..]` = TLV body

### Dynamic flags (the interposition-defeat finding)

`readelf-equivalent` dump of `.dynamic`:
- `DT_SYMBOLIC(16)` **present** → internal references to driver's own globals bind LOCALLY at link time.
- `DT_BIND_NOW(24)` not present, `DT_FLAGS=0` (no `DF_SYMBOLIC`/`DF_BIND_NOW`), `DT_FLAGS_1=0` (no `DF_1_NOW`) — the `-Bsymbolic` semantics are driven by the legacy `DT_SYMBOLIC` tag.

Consequence: LD_PRELOAD interposition of `link_handle_iap2pkt` does NOT work for the driver's own internal call chain. → binary detour required.

(Cross-check: `libiap2client.so.1` does NOT have DT_SYMBOLIC, but we don't need to interpose its symbols — we CALL them from our LD_PRELOAD'd .so in dio_manager, which works regardless.)

## `libiap2client.so.1` (cover-art API in dio_manager)

Path: `extracted/app70/armle/usr/lib/libiap2client.so.1` (≈18 KB, fully-symboled thin QNX-IPC shim; every API marshals into a header + `MsgSendsv` to the iap2 daemon).

| Sym | VA | Notes |
|---|---|---|
| `iap2_connect` | 0x2cec | `int *iap2_connect(const char *devpath, int oflag)` → 0x18-byte handle `{int fd; void* aux; ...}`. Internally `open(devpath, oflag)` + handshake `MsgSend` msgcode 0x9999 expects reply status `==2`. Default devpath: `/dev/ipod0`. |
| `iap2_disconnect` | 0x2c58 | counterpart |
| `iap2_eap_open` / `iap2_eap_close` / `iap2_eap_send` / `iap2_eap_recv` / `iap2_eap_file_descriptor` / `iap2_eap_event_arm` | 0x2e00, 0x2c88, 0x13b8, 0x1660, 0x127c, 0x1efc | EAP (External Accessory Protocol) — third-party app channel |
| `iap2_filexfer_read` | 0x15cc | msgcode 0x2900; generic file-transfer reader by explicit `xfer_id` |
| `iap2_gps_data_send` | 0x1f20 | HU→device NMEA |
| `iap2_hid_*` | 0x1444, 0x14c0, 0x1a64, 0x2264 | HID |
| `iap2_medialib_*` | 0x1f a4 et al. | media library sync/control |
| `iap2_nowplay_getartwork` | **0x18c4** | **msgcode 0x2100**. `int iap2_nowplay_getartwork(int *handle, void *buf, int buflen)`. NO id arg — implicit current track. Two-phase: `buflen=0` → returns size; alloc; call again. Caller owns buf. Self-contained (server does the iAP2 file-transfer). |
| `iap2_nowplay_getuidlist_num/str` / `iap2_nowplay_set` | 0x182c, 0x2b24, 0x253c | |
| `iap2_play` / `iap2_pause` / `iap2_next` / `iap2_prev` / `iap2_playpause` / `iap2_shuffle` / `iap2_repeat` / `iap2_fast*` / `iap2_audiobook_speed` | 0x1ea4, 0x1e4c, 0x1df4, 0x1d9c, 0x1b2c, 0x1b84, 0x1bdc, 0x1c94/0x1cec/0x1d44, 0x1c34 | playback control |
| `iap2_power_set` / `iap2_bt_status_set` / `iap2_language_set` | 0x1ac4, 0x1954, 0x2388 | |
| `iap2_ppsdir_get` | 0x153c | msgcode 0x2703; returns iap2 PPS dir path into caller buffer. Used internally to build `/ramdisk/pps/iap2/...` paths. |
| `iap2_raw_msg_send` | 0x131c | generic raw control-session send (HU→device) — useful if we ever need to push a custom msg |
| `iap2_vehicle_update` / `iap2_app_launch[_ext]` | 0x240c, 0x2378, 0x22f0 | |

Handle layout (post-`iap2_connect`): `calloc(1, 0x18)`, `[0] = fd/coid` (QNX connection), `[1] = aux ptr` freed on disconnect.

## `dio_manager` (CarPlay/DSI app — cover-art hook host)

Path: `extracted/app70/eso/bin/apps/dio_manager` (≈657 KB, ARM32, stripped but retains C++ mangled dynamic syms).

| Sym (demangled) / detail | Role |
|---|---|
| `dio::CDIOManager::startDSIInterface` @ 0x13A458 | creates `CDSICarplayImpl` (operator new 0x11C = 284 B); stores at `DIOManager + 0x228` (= `m_pCarPlayDSI`). Asserted in `DIOManager.cxx:0x6AA` ("NULL == m_pCarPlayDSI"). |
| `dio::CDSICarplayImpl::startDSI` @ 0x18089C | registers DSI service `"dsi.carplay.DSICarplay"` (str @ 0x19a104) with UUID `B60FF3E0-C276-5843-8023-5ABA5E2AC150` (constructed at 0x1809C4) |
| `dio::CDSICarplayImpl::addClient` @ 0x17DB58 | subscribes new client to all 10 DSI attributes incl. ID 6 = `coverArtUrl` |
| `dio::CDSICarplayImpl::setNotification` @ 0x17D9FC | per-attr notification setup; reports attr ID 6 via `sub_17D394` (`reportAttribute`) reading value at `a1 + 0xB0` |
| (unnamed) listener fan-out @ 0x17EF0C (with err str "listener_updateCoverArtUrl: unable to forward call to peer!: %s" @ 0x199e1c, called at 0x180554/0x180558) | DSI-skeleton listener-loop dispatching `updateCoverArtUrl` via vtable slot +0x24 |
| `onEvent_updateNowPlayingTrackInfo` ~0x141700 | sets title/album/artist/composer/duration/genre via `CDSICarplayImpl::updateTrackState` (`sub_14B1BC` @0x14B1BC). Called at ~0x1418D4. **The natural integration trigger for cover-art fetch.** Gated on `m_pCarPlayDSI != NULL` (LDR/CMP/BEQ at 0x1418BC–0x1418C8) → CarPlay-only path. |
| `onEvent_updateNowPlayingPlaybackInfo` ~0x142A00 | status/repeat/shuffle/appName via `sub_14B0B4`. Same CarPlay gate. |
| `onEvent_updateNowPlayingElapsedTime` | elapsed time |
| `CPPSNowPlayingMetadataObject::…` @ 0x16D164 | iterates per-attr PPS key tables `off_19E6E8` / `off_19E6B8` — these tables contain title/duration/album/artist/genre/composer/playback.{status,shuffle,repeat,appName} only. **No coverArtUrl key — confirms the DSI skeleton's coverArtUrl path is dead code in dio_manager.** |
| imported (NEEDED): `libiap2client.so.1`, `libairplay.so`, `libnvaudio.so` | imports include `iap2_connect`, `iap2_eap_*`, `iap2_gps_data_send`, `iap2_disconnect`, but **NOT** `iap2_nowplay_getartwork` (dio_manager never fetches artwork itself — it discards the artwork field from NowPlayingTrackInfo). Our hook supplies the missing call. |

## `mm-ipod` (iAP2 daemon — RGI hook host)

Path: `extracted/app70/armle/usr/sbin/mm-ipod` (≈40 KB ARM32, NOT stripped, BuildID `85ad8230b269addaef1a74ef1e198b71`).

- Loads driver via dlopen with name pattern `ipod-drvr-%s.so` → `ipod-drvr-iap2.so` (also loads `ipod-acp-%s.so`, `ipod-transport-%s.so`).
- Does NOT import `link_handle_iap2pkt` dynamically → external interposition not viable via that path.
- Has a config-file based init that reads `/etc/mm/iap2.cfg`.

## `iap2.cfg` schema

Source `system/etc/mm/iap2.cfg` (matches the MU1367 proxy). Driver string at "[ident]" / "recvmsgs" / "sendmsgs". The `[location]` feature is HU→device NMEA only (`types=GPGGA,GPRMC` typical) — NOT route guidance reception. No `[routeguidance]` section exists; the Apple stack has no native RG feature.

Cover-art and similar are governed by `[nowplaying] enable=yes` + `metadata=...` (the metadata list includes `ArtworkFileTransferIdentifier` per MU1367 sample — confirming the daemon receives artwork via file-transfer).
