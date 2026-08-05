# C-hook port plan for MHI2 (target MU1439)

## Architectural delta from MHI2Q

On MHI2Q, iAP2 transport + control session is **Cinemo SDK** inside `dio_manager`. The patch hooks at the Cinemo message layer via LD_PRELOAD symbol interposition (`dlsym(RTLD_NEXT, "_ZN14NmeIAP2Message6DecodeEPKhi")` etc., from `framework/hook_framework.c:61+`).

On MHI2, iAP2 is split:
- **`/app/armle/usr/sbin/mm-ipod`** (ARM ELF, NOT stripped) — daemon hosting the Apple iAP2 reference stack. **`dlopen`s** `ipod-drvr-%s.so` / `ipod-transport-%s.so` / `ipod-acp-%s.so` by pattern. All iAP2 control-session message parsing (incl. RGD if enabled) happens here.
- **`dio_manager`** — CarPlay/DSI app layer; NEEDS `libiap2client.so.1`, registers nowplaying/CarPlay listeners, exposes `dsi.carplay.DSICarplay` DSI service. Pure client.

So the hook split:

| Feature | Host process | Mechanism |
|---|---|---|
| **RGI** | `mm-ipod` | **binary detour** of `ipod-drvr-iap2.so` + LD_PRELOAD `.so` for the runtime + cfg edit |
| **Cover art** | `dio_manager` | **LD_PRELOAD `.so`** that calls `iap2_nowplay_getartwork` (libiap2client export) |

## Why RGI needs binary detour, not LD_PRELOAD interposition

Verified empirically (see [`re/symbol_offsets.md`](re/symbol_offsets.md)):

- `ipod-drvr-iap2.so` has **`DT_SYMBOLIC` set** in its dynamic section. With `-Bsymbolic` linkage, the driver's references to its OWN global symbols (e.g., internal call to `link_handle_iap2pkt`) bind LOCALLY at link time, bypassing the global symbol scope. An LD_PRELOAD definition of `link_handle_iap2pkt` therefore does NOT intercept the driver's internal call paths.
- `mm-ipod` does NOT import `link_handle_iap2pkt` either (it dlsyms only a few driver entry points by name) — so there is no external caller whose call we could interpose. The packet dispatcher is reached purely via driver-internal call chains.
- libc-level interposition (`read`/`recv`/`MsgReceive`) IS possible (libc syms are globally interposable regardless of driver flags) but gives RAW pre-deframe link-layer bytes, requiring us to re-implement iAP2 link-layer deframing. Much more work than binary-detouring the post-deframe dispatcher.

→ **The clean route is to binary-patch `ipod-drvr-iap2.so` on disk.** It lives in `/app/armle/lib/dll/ipod-drvr-iap2.so`, which is on the persistent `/app` partition (writable, like `libPresentationController.so` which we already binary-patch).

(MHI2Q didn't hit this because Cinemo's `libNmeSDK.so` is NOT DT_SYMBOLIC and `NmeIAP2Message::Decode` is called from `dio_manager` externally through the PLT — cleanly interposable.)

## RGI — step-by-step

### Step 1: Enable iAP2 route-guidance messages (config-only)

Edit `/etc/mm/iap2.cfg` on device (cfg file inside the system partition; for MU1439 the cfg path matches the MU1367 proxy at `system/etc/mm/iap2.cfg`):
```ini
[ident]
recvmsgs=0x5201,0x5202,0x5204
sendmsgs=0x5200,0x5203
```
The driver's `ident_info_recvmsgs@0xbD60` / `ident_info_sendmsgs@0xc46c` walk per-feature tables and then append comma-separated hex IDs from the cfg `[ident]` section (capacity 0x80 bytes / ≤64 IDs; warns "Cannot add custom messages" if exceeded). Verified by the cfg-injection RE.

No binary patch needed for this step. iOS then includes 0x52xx in MessagesReceivedFromDevice and starts sending route-guidance messages. OPEN: confirm on a live unit whether iOS also requires a navigation **component** declaration (MHI2Q needed EAGroupComponent 0x001E); likely not (RG is a core iAP2 feature, not an EA-app component), but verify.

### Step 2: Binary detour `link_handle_iap2pkt@0x12e00` in `ipod-drvr-iap2.so`

The driver's per-message dispatcher (the big switch on the 16-bit msgid in `link_handle_iap2pkt`) has no 0x52xx cases — undeclared messages fall to the default arm and are silently dropped (returns 0, no log, no forward). Confirmed: zero `Route`/`Guidance`/`Maneuver`/`Lane`/`0x52xx` strings in the entire driver.

**Detour design** (proposed; precise prologue bytes pending — see "Next RE step"):

After `link_handle_iap2pkt` decodes the link frame, it sets:
- `*(packet_obj + 0x24)` = pointer to raw control-session payload
- `*(packet_obj + 0x28)` = payload length (u16)
- Within the payload: `[0..1]`=length, `[2..3]`=0x4040 control marker, `[4..5]`=**big-endian msg ID**, `[6..]`=TLV body

Two viable detour points:
- **A) Function-entry detour** — overwrite the first 1–2 ARM instructions of `link_handle_iap2pkt` with a branch to our trampoline. The trampoline saves regs, calls our handler (after letting the original function run to populate +0x24/+0x28), then returns. Simplest but requires re-executing the displaced instructions.
- **B) Default-case detour** — patch the dispatcher's default/drop branch (where unknown msgids land) to instead branch to our trampoline. Requires precisely locating the default arm. More surgical; doesn't affect known-msgid paths.

I lean toward **B** for surgical reasons; A as fallback if the default arm is hard to isolate.

The trampoline does NOT itself parse RGD or talk to sockets. It performs (once) `dlsym(RTLD_DEFAULT, "carplay_on_iap2_msg")` against the LD_PRELOAD'd handler, caches the function pointer in a static slot, and calls it. Cached pointer = ~1 ARM load + branch on subsequent calls. This puts all heavy logic in normal C, in our `.so`.

### Step 3: LD_PRELOAD `libcarplay_hook.so` into `mm-ipod`

Our hook `.so` lives in `/app/armle/lib/` (or `/mnt/app/root/hooks/`) and is LD_PRELOAD'd into mm-ipod. It:
1. Exports `void carplay_on_iap2_msg(const uint8_t *payload, int len)` — the entry the trampoline calls.
2. On `payload[4..5] == 0x5201|0x5202|0x5204`: parse with `rgd_tlv` (REUSED unchanged from current `c_hook/routeguidance/rgd_tlv.c`) → push event onto TCP bus client (REUSED from `framework/bus.c`).
3. Sends nothing to the driver — we're a passive sniff.

**Injection point:** `mm-ipod`'s launch env. On MU1367 proxy, `system/etc/scripts/start_usb_device_stack.sh` launches `io-usb-dcd -d devu-iap2-tegra3-ci.so` but mm-ipod itself is launched by a separate mm init (probably `system/etc/mm/mm-sync.conf` or a script). **OPEN deployment item:** locate the exact mm-ipod launch in MU1439's extracted system partition (we now have the imagefs at `extracted/mifs-stage2.full.imagefs`) and add `LD_PRELOAD=/mnt/app/root/hooks/libcarplay_hook.so` to its env. Analog of MHI2Q's `smartphone_integrator.json` env edit for dio_manager.

## Cover art — step-by-step

### Step 1: LD_PRELOAD a hook into `dio_manager`

`dio_manager` already:
- NEEDS `libiap2client.so.1` (so `iap2_nowplay_getartwork` is resolvable from our `.so`)
- Receives `CIAP2ClientEvent_UpdateNowPlayingTrackInfo` events (track changes)
- Exposes `dsi.carplay.DSICarplay` DSI w/ a **dead** `coverArtUrl` attribute (the existing infrastructure isn't used — see [`memory/mhi2_mu1439_coverart_path.md`](memory/mhi2_mu1439_coverart_path.md))

Existing `smartphone_integrator.json` already declares `LD_PRELOAD` to inject into dio_manager (MHI2Q deployment) — same edit applies.

### Step 2: Fetch artwork via the exported API

`libiap2client.so.1` (NOT stripped, normal exports) provides:
```c
int iap2_nowplay_getartwork(int *handle, void *buf, int buflen)  // @0x18c4, msgcode 0x2100
```
Self-contained: caller-allocated buffer, two-phase usage (`buflen=0` → returns required size; alloc; call again → fills buf with image bytes). No artwork id needed — implicitly targets the current now-playing track. Daemon (`mm-ipod`'s `nowplay_getartwork@0x1ba3c`) performs the iAP2 file-transfer internally.

Result is opaque bytes — verify magic: `FF D8 FF` = JPEG, `89 50 4E 47` = PNG. iOS overwhelmingly ships JPEG over CarPlay.

### Step 3: Trigger and pipeline

Trigger options (pick one based on cleanliness on-device):
- **Interpose PPS read** on `/ramdisk/pps/iap2` (the path dio_manager polls for nowplaying metadata) — detect track change in the metadata diff → call getartwork. PPS path matches MHI2 driver-side: `/ramdisk/pps/iap2` + `PPS_IAP2_DEVICE_METADATA`.
- **Poll thread** in our `.so` that periodically calls `iap2_nowplay_getartwork` and compares the JPEG CRC against the last seen one (similar to the existing MHI2Q coverart_hook CRC dedup).

After fetch: decode → resize 256×256 → PNG → write atomically to `/var/app/icab/tmp/37/coverart.png` → push `EVT_COVERART` over the existing TCP bus to Java. All of this is unchanged from the current `c_hook/coverart/coverart_hook.c`.

**OPEN runtime check:** confirm `iap2_nowplay_getartwork` actually returns bytes during a CARPLAY session (vs only iPod/USB-audio mode). If empty during CarPlay (because CarPlay artwork rides the AirPlay/CarPlay nowplaying path, separate from iAP2 file-transfer), fall back to reassembly: hook `iap2_eap_recv`/`iap2_filexfer_read` on the CarPlay nowplaying file-transfer stream (port MHI2Q approach). Decisive without a live unit by reversing the iap2 daemon's msgcode 0x2100 handler (in `mm-ipod` itself).

## What reuses from the existing c_hook tree

✅ Reuse as-is:
- `framework/bus.c` + `framework/bus.h` — TCP bus client to Java
- `framework/logging.c` — log facility
- `framework/iap2_protocol.c/.h` — iAP2 message TLV framing
- `routeguidance/rgd_tlv.c/.h` — RGD parser (iAP2 format is SDK-independent)
- `routeguidance/rgd_hook.c` — STATE part (slot mapping, dirty tracking, PPS publish) reusable; **HOOK ENTRY** part must be re-pointed at the new interception (`carplay_on_iap2_msg` instead of the Cinemo-decode wrapper)
- `coverart/coverart_hook.c` — decode/CRC/PNG/tmpfs/bus pipeline reusable; entry rewired to libiap2client-call instead of byte-stream reassembly

🔄 Rewrite/adapt:
- `framework/hook_framework.c` — strip the Cinemo `dlsym(RTLD_NEXT, "_ZN14NmeIAP2Message6DecodeEPKhi")` interpositions. Replace with: (1) `carplay_on_iap2_msg(payload, len)` exported entry for RGI; (2) PPS-read interpose OR poll thread for cover-art trigger; (3) a dispatcher to call into the per-feature hooks.
- `main.c` — module registration unchanged.

➕ New:
- **Detour patcher** for `ipod-drvr-iap2.so` (separate Python tool, in the style of the existing `patch_libpresentationcontroller.py`): signature-based search for `link_handle_iap2pkt` prologue or the dispatcher default-arm, write the ARM branch + (if appended) the trampoline body. Verify hash before/after.

❌ Drop:
- `altscreen/` — impossible on MHI2 just as on MHI2Q (same libairplay 210.81 gates).
- `cursor/` — orthogonal feature, port mechanically.

## Next RE step

The decisive missing item is the exact ARM detour at `link_handle_iap2pkt@0x12e00`:

1. **Disassemble the prologue** (first ~6 instructions / 24 bytes): identify what we displace, what we must preserve / re-execute in the trampoline.
2. **Locate the dispatcher default arm** (the silent-drop path for unrecognized msgid): preferred patch point, more surgical than function-entry. Likely a branch at the end of the long if/else chain that returns 0.
3. **Verify position of payload ptr / len writes** (currently noted as `*(pkt+0x24)` / `*(pkt+0x28)` via the prior agent's RE; confirm these by decompilation, and confirm they are populated BEFORE the dispatch — so they're already valid when our handler runs after the unknown-msgid branch is taken).
4. **Pick + size the trampoline location**: simplest is appending bytes to a non-executed region or extending a code section. Alternative is a separate `libcarplay_drvr_glue.so` that ipod-drvr loads (requires adding a DT_NEEDED). Direct append is cleanest.

After that, the C-side implementation is mechanical reuse plus the small new bits described above.
