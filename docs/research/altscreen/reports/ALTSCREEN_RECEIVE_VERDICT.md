# altScreen 111 receive-path verdict — stock libairplay 210.81 + LD_PRELOAD

## Bottom line

**No: the current hook will not render a type-111 stream even after iOS offers it.** Four independent defects are proven from the shipped sources and the stock MHI2Q binary:

1. The RTSP request-bearing call to `AirPlayReceiverSessionSetup` is internal to stock libairplay (`0x2f790` through thunk `0x18c04`), so the separate negotiation work must already have redirected GOT slot `base+0xb8e80`; ordinary interposition is not authoritative for the incoming 111 request.
2. The decrypted-frame call is also internal: stock `AirPlayReceiverSessionScreen_ProcessFrames` calls the local `ScreenStreamProcessData` thunk at `0x2b7dc`. The ordinary wrapper at `altscreen_hook.c:733-763` is bypassed until GOT slot `base+0xb9740` is explicitly rewritten.
3. The hook's forwarding client defaults to TCP port **19830** (`altscreen_av.h:24-25`, `altscreen_av.c:23-24`), while `altscreen_render` listens on **19831** (`altscreen_render/protocol.h:18-22`). `altscreen_init()` does not call `altav_init()` (`altscreen_hook.c:1475-1486`), so the two processes never connect.
4. Receiver cleanup is wrong: `NetSocket_Delete` receives `&netSock` instead of the `NetSocketRef` value (`altscreen_hook.c:559,827`), and the thread never calls `AirPlayReceiverSessionScreen_StopSession` (`altscreen_hook.c:822-830`). Stock does both, in that order, at `_ScreenThread` `0x24400-0x24418`; `StopSession@0x2a2d0` owns ScreenStream, command-socket, and AES cleanup.

The smallest route to first pixels is to repair the current hybrid architecture: install the setup/frame/teardown GOT rewrites, make both ends use port 19831, fix the screen-session lifecycle, and make the forwarder preserve message framing and configuration across reconnects. The more robust production architecture is the self-contained receiver described in §6; it avoids both the frame GOT dependency and an unnecessary third hardware-decoder allocation.

## 1. Evidence and scope

Audited binaries:

| Component | File | SHA-256 |
|---|---|---|
| MHI2Q stock receiver | `Patches/carplay_vc/libairplay.so` | `01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2` |
| iOS 26.6 sender | `Refferences/ios266_extract/dylibs/AirPlaySender` | `952d73f8e97d4697490f69bdf08067df6ccff29f5b44c60489eb117db015d832` |
| MHI3 receiver cross-check | `Patches/mhi3-altscreen/work/libairplay.device.so` | `9dfea24df3fe8bb0e24e1283ead3c456752435c4b3c0eb257838d310b7f2dda4` |

The readable Apple 280 source used below is the stock tree at `/Users/luka/Desktop/AUDI_2/Refferences/Carplay`. `Patches/libairplay-mhi2q` is **not** treated as reference evidence: it is the project reconstruction. All behavior attributed to MHI2Q 210, MHI3, or iOS is backed by those binaries; stock 280 source is used only where it independently agrees with the disassembly.

Address notation is image-relative for the ELF files and unslid Mach-O VA for AirPlaySender. Hook/source line numbers refer to the current worktree.

## 2. Architecture decision

### What the hook currently implements

The current implementation is architecture **(a), hybrid**, not self-contained:

```text
iPhone TCP
    -> hook-owned listen/accepted socket
    -> stock AirPlayReceiverSessionScreen_ProcessFrames
       (read 128-byte header, read body, AES-CTR decrypt)
    -> stock internal ScreenStreamProcessData call
    -> intended hook wrapper
    -> AVCC-to-Annex-B conversion
    -> localhost TCP
    -> altscreen_render OMX AVC decoder
```

The decisive calls are `scr_start()` and `scr_process()` at `altscreen_hook.c:822-824`. `alt_thread_main` never reads the screen socket itself and contains no AES context or screen-header parser (`altscreen_hook.c:784-830`).

### Why ordinary interposition cannot complete that pipeline

Stock `AirPlayReceiverSessionScreen_ProcessFrames@0x2b154` reads and decrypts a frame, then calls `j_ScreenStreamProcessData@0x1a644` at `0x2b7dc`. That thunk dispatches through `base+0xb9740`, whose stock target is `base+0x84fec`. This is a libairplay-to-libairplay call, so the proven LD_PRELOAD boundary rule applies: the wrapper at `altscreen_hook.c:733-763` does **not** run without an explicit relocation rewrite.

The thread-identity routing itself is sound after that rewrite: the patched call remains synchronous on the alt receiver thread, so `alt_slot_for_thread()` can distinguish it from stock's main-screen receiver (`altscreen_hook.c:688-701,733-762`). The defect is reachability, not classification.

### Decision

* **Minimum change to the current code:** retain (a), rewrite `base+0xb9740`, and apply the lifecycle/sink fixes in §7.
* **Recommended production receive design:** implement (b), a self-contained socket reader/decryptor/parser. It removes the fragile internal frame seam and avoids constructing a stock `ScreenStream`/OMX decoder that will never display the cluster frame.

Architecture (b) does not require a frame GOT rewrite. Both designs still require the request-bearing SETUP seam (`base+0xb8e80`) unless the separate negotiation patch has already installed it, and both require teardown integration.

## 3. R1-R8 verdict matrix

| ID | Verdict | Evidence | Required correction |
|---|---|---|---|
| R1 | **Response shape mostly correct; error handling wrong** | Stock `_ScreenSetup@0x26f9c` emits only `{type:110,dataPort}` at `0x27144-0x27170`. Stock 280 agrees at `Refferences/Carplay/Sources/AirPlayReceiverSession.c:4753-4757`. iOS 26.6 reads `response["streams"]`, requires one matching descriptor at `0x2276e5aa4-0x2276e5b20`, and reads `dataPort` at `0x2278dafb8-0x2278dafd8`. Hook emits `{type:111,dataPort}` at `altscreen_hook.c:904-910`. | Return allocation/append failures, tear down a partially-created receiver, and propagate `alt_setup` failure from `AirPlayReceiverSessionSetup` (`altscreen_hook.c:1001-1013`). Assert the screen SETUP response has exactly one descriptor of type 111. |
| R2 | **AES derivation correct for stock 210** | Stock `_ScreenSetup` loads the 16-byte session key at session `+0x1c8` and calls `AirPlay_DeriveAESKeySHA512ForScreen` at `0x27094-0x270b0`; the helper is `0x2d5fc`, with salts `AirPlayStreamKey%llu`/`AirPlayStreamIV%llu` at `0x2d658-0x2d660`. Hook uses offset 456 and the request's `streamConnectionID` at `altscreen_hook.c:534,862-875`. | No receive fix for the AES path. Fail if `streamConnectionID` is absent, as current code does. Do not substitute type 111 into the KDF; stream type is not an input. |
| R3 | **Same screen protocol for 110 and 111; AVC path is correct when HEVC is not negotiated** | Stock 210 reads 128 bytes at `0x2b450`, then `bodySize` and body at `0x2b458,0x2b510-0x2b548`; it decrypts opcode-0 body at `0x2b794-0x2b7b0`. iOS builds the same 128-byte header at `0x2276d7034-0x2276d7050` and encrypts the body at `0x2276d7318`. MHI3 routes cases 110 and 111 together at `0x639bc`, then through the same Start/ProcessFrames path (`0x562c0,0x5632c`), reads the same 128-byte header (`0x66cf0`), and calls the same `ScreenStreamProcessData` after AES/ChaCha (`0x67338,0x67064`). | Keep AVCC-to-Annex-B conversion for AVC. Do not negotiate `hevc` unless a HEVC renderer/parser is added. For architecture (b), implement the exact parser in §6. |
| R4 | **Thread routing correct only after the GOT rewrite; `AltScreen` property is unavailable on 210** | Thread publication and lookup are locked at `altscreen_hook.c:688-701,886-901`; alt calls are swallowed and main calls pass to the captured real function at `altscreen_hook.c:738-762`. Stock `_ScreenStreamSetProperty@0x85590-0x85ca4` has no `AltScreen` key, and the binary has zero `AltScreen`/`altScreen` strings. Stock 280 always uses `ScreenCopyMain` at `Refferences/Carplay/Sources/AirPlayReceiverSessionScreen.c:711-725`. | Route solely by hook-owned socket/thread identity. Do not depend on `ScreenStreamSetProperty("AltScreen")` on stock 210. |
| R5 | **Trivial clock does not stall receive** | Stock ProcessFrames uses the supplied NTP-to-ticks callback at `0x2b700-0x2b720` only when the config flag requests timestamps; otherwise it uses `UpTicks` at `0x2b9f8-0x2ba04`. Late-frame accounting continues to decryption at `0x2b724-0x2b794`; it does not discard the frame. Hook callbacks return `UpTicks` at `altscreen_hook.c:646-649`. Renderer submits decoder-order timestamp zero at `altscreen_render/sink_omx_qnx.c:357-373`. | No first-pixel fix. Use the real session clock later if synchronized presentation becomes necessary. |
| R6 | **Software objects are independent; hardware capacity is unproven and the hybrid wastes a decoder** | Stock `AirPlayReceiverSessionScreen_Create@0x2b044` allocates an independent 504-byte object. `StartSession@0x2a3b8` creates its own command socket and `ScreenStream`, but binds it to the main screen at `0x2a3f0-0x2a414`. `ScreenStreamCreate@0x85ce4` allocates a 1 MiB buffer and a new video implementation at `0x85d70-0x85db0`; `ScreenStreamStart@0x84da0` starts/configures that decoder at `0x84dd8-0x84eb8`. Renderer creates another AVC decoder (`altscreen_render/sink_omx_qnx.c:39,211-222`). | On hybrid (a), test three concurrent AVC instances on APQ8064: stock main + unused stock alt object + external cluster renderer. Architecture (b) removes the unused middle instance and is safer. Static analysis cannot prove the BSP/OMX instance limit. |
| R7 | **Current teardown is incorrect and not reliably reached** | Hook stops alt for every teardown and declares the wrong ABI at `altscreen_hook.c:1016-1054`. Real signature is `void(session,params,reason,Boolean*)` in stock 280 at `Refferences/Carplay/Sources/AirPlayReceiverSession.c:1188-1193`; stock 210 parses per-stream teardown at `0x26be0-0x26d50`. The internal RTSP caller uses thunk `0x19264` and GOT `base+0xb90a0` (caller includes `0x30b10`). | Rewrite teardown GOT, use the exact `void`/`Boolean *` signature, and stop alt only for full teardown or a streams entry of type 111. Join before the real full teardown. Call `StopSession`, delete the NetSocket correctly, then delete the screen object and close the renderer connection. |
| R8 | **Payload conversion matches the renderer, but the live endpoint does not** | Hook writes `[u32 BE length][Annex-B payload]` at `altscreen_av.c:67-86`; renderer reads exactly that at `altscreen_render/frame_io.c:23-39`. Hook converts AVCC at `altscreen_av.c:89-127`; renderer selects AVC and marks its first message CODECCONFIG at `altscreen_render/sink_omx_qnx.c:39,211-222,466-477`. Port mismatch is `altscreen_av.h:24-25` versus `altscreen_render/protocol.h:18-22`. | Change hook default to 19831 (or call `altav_init(...,19831)`), implement `send_all`, make send success observable, resend cached SPS/PPS after each reconnect, and call `altav_shutdown()` on each alt-session teardown. |

## 4. Detailed findings

### R1 — type-111 SETUP response

#### Receiver-side shape

Stock 210's type-110 `_ScreenSetup@0x26f9c` performs this sequence:

1. Rejects a second main screen via session `+5136/+5140` at `0x26ff4-0x27000`.
2. Calls `AirPlayReceiverSessionScreen_Setup` at `0x27050-0x27058`.
3. Reads a nonzero `streamConnectionID` at `0x27060-0x27090` and installs derived security at `0x27094-0x270b0`.
4. Opens the data listener at `0x270e4-0x2711c`.
5. Appends exactly `type=110` and `dataPort=<listener>` at `0x27144-0x27170`.

There is no response echo of `streamConnectionID`, no timing field, and no latency field. The independent stock 280 source has the same response at `Refferences/Carplay/Sources/AirPlayReceiverSession.c:4728-4757`.

The hook's successful response at `altscreen_hook.c:904-910` is therefore structurally correct:

```plist
streams = (
    { type = 111; dataPort = <hook listener>; }
)
```

#### iOS 26.6 parser

For cases 110 and 111, `_apsession_setupStreamCreatingResponseInternal@0x2276e5250` enters the same screen response branch at `0x2276e568c-0x2276e5694`. It typed-gets `response["streams"]` at `0x2276e5aa4-0x2276e5ac4`, requires count `== 1` at `0x2276e5ad0-0x2276e5adc`, requires the sole item to be a dictionary at `0x2276e5ae0-0x2276e5af8`, and verifies that its `type` equals the requested type at `0x2276e5b04-0x2276e5b20`. The later screen resume block reads mandatory `dataPort` at `0x2278dafb8-0x2278dafd8`. A response `streamID` is read at `0x2276e5b28-0x2276e5b48`, but iOS generates one if absent at `0x2276e5b4c-0x2276e5bf4`.

Thus `{type,dataPort}` is sufficient. The hook defects are transactional:

* `alt_add_response_stream()` returns `void` and discards allocation/append failures (`altscreen_hook.c:765-782`).
* `alt_setup()` returns success even when `rdesc` allocation failed (`altscreen_hook.c:904-914`).
* `AirPlayReceiverSessionSetup()` logs `arc` but always returns stock's `ret` (`altscreen_hook.c:1001-1013`).

If any of those operations fail, the phone receives no valid 111 endpoint while the hook leaves a thread/listener alive. Make the helper return `OSStatus`, undo the created slot on failure, and propagate the error to the RTSP layer.

### R2 — key derivation and encryption mode

Stock 210 proves the exact legacy path. `_ScreenSetup@0x26f9c` passes `(session+0x1c8, 16, streamConnectionID, key, iv)` to `AirPlay_DeriveAESKeySHA512ForScreen@0x2d5fc` at `0x27094-0x270a0`, then calls `SetSecurityInfo` at `0x270a4-0x270b0`. The helper formats `AirPlayStreamKey%llu` and `AirPlayStreamIV%llu` at `0x2d658-0x2d660`, SHA-512 hashes salt followed by the 16-byte master key, and uses the first 16 bytes for key/IV (`0x2d6b0-0x2d6d4`). Stock 280 independently documents the same algorithm at `Refferences/Carplay/Sources/AirPlayUtils.c:570-630` and salt constants at `AirPlayCommon.h:291-297`.

The hook matches that stock-210 ABI and data:

* session key offset 456/`0x1c8`: `altscreen_hook.c:534,870`;
* request `streamConnectionID`, required nonzero: `altscreen_hook.c:862-868`;
* master-key length 16, output key/IV passed to stock `SetSecurityInfo`: `altscreen_hook.c:870-875`.

Type 111 uses the same derivation as 110. On iOS, cases 110 and 111 use the same screen setup branch at `0x2276e568c-0x2276e5694`; on MHI3, both types use the same screen thread and `ProcessFrames` at `0x562c0-0x5632c`. Neither KDF takes a stream type.

Stock 280 also contains a newer PairVerify/ChaCha alternative (`Refferences/Carplay/Sources/AirPlayReceiverSession.c:4731-4745`), and MHI3 decrypts either ChaCha or AES at `0x67634-0x676ac` and `0x6731c-0x6733c`. Stock 210's audited `_ScreenSetup` has no such branch; it always takes AES. This is not a 111-only risk: a phone selecting an incompatible modern cryptor would also break the proven stock 110 screen. The 111 clone should match the active stock-210 screen security mode, which the current KDF does.

### R3 — wire format, NAL format, and codec

The data connection is a plain TCP byte stream, not RTP. Each message is:

```c
/* Native little-endian fields on the audited ARM sender/receivers. */
struct AirPlayScreenHeader {       /* exactly 128 bytes */
    uint32_t bodySize;             /* +0 */
    uint8_t  opcode;               /* +4 */
    uint8_t  smallParam[3];        /* +5 */
    uint64_t params[15];           /* +8; fields are also viewed as f32 pairs */
};
uint8_t body[bodySize];
```

The layout and opcodes are present in the stock 280 tree at `Refferences/Carplay/Sources/AirPlayCommon.h:2366-2401`, but the deployed facts come directly from the binaries: stock 210 reads 128 bytes at `0x2b450`, then the body at `0x2b510-0x2b548`; iOS allocates/stores a 128-byte video header at `0x2276d7034-0x2276d7050`; MHI3 reads 128 bytes at `0x66cf0`.

Relevant opcodes are:

| Opcode | Body | Receive action |
|---:|---|---|
| 0 | Encrypted video access unit | Decrypt with the continuous stream cryptor, then process as AVCC NAL units. Stock 210: `0x2b700-0x2b7dc`; iOS encryption: `0x2276d7318`. |
| 1 | Unencrypted `avcC` configuration | Cache width/height and configure AVCC/NAL-length size. Stock 210: `0x2b908-0x2b9b8`; iOS config builder: `_sbpd_createConfigBBuf@0x227853f38`, header stores at `0x227853fe8-0x227853fec`. |
| 2 | No useful payload | Keepalive; consume and ignore. Stock 210 opcode switch after `0x2b58c`. |
| 4 | Optional body | Consume and ignore. |
| 5 | Body | Consume and ignore. |

AES-CTR state is initialized once and advanced only across opcode-0 video bodies; it is not reset per frame and does not cover the 128-byte header or `avcC` config. Stock 210 performs the continuous `AES_CTR_Update` at `0x2b794-0x2b7b0`.

The codec payload is AVC by default. `avcC` describes the SPS/PPS and NAL length size. Stock `_ScreenStreamSetProperty@0x85590` converts `avcC` to an Annex-B header at `0x8583c-0x858c8`, storing header pointer/length/NAL-size at object offsets `+40/+44/+52`. The hook reads exactly those offsets at `altscreen_hook.c:704-755`, and `altscreen_av.c:89-127` converts each AVCC access unit to Annex-B.

iOS 26.6 can choose HEVC only when the negotiated `hevc` feature and its screen-multicodec checks enable `UseCarPlayHEVC` (`carEndpoint_setupStreams@0x2277ff398-0x2277ff3bc`, `_APEndpointStreamScreenCreate@0x227852330-0x227852344`, `_screenstream_createVirtualDisplayActivationOptions@0x2278dc598-0x2278dc5a8`). Stock 210 has no HEVC/H.265 screen path, and the renderer explicitly configures `OMX_VIDEO_CodingAVC` (`altscreen_render/sink_omx_qnx.c:211-222`). The negotiation patch must not claim `hevc`.

There is no type-111-specific frame container or timing format. MHI3 passes the stream type to the common StartSession at `0x562c0`, then both types enter common ProcessFrames at `0x5632c`; common decrypt and ProcessData are `0x67338/0x67634` and `0x67064`.

### R4 — routing two concurrent screen streams

Once `base+0xb9740` is patched, the current thread table separates the streams correctly:

* `alt_setup` creates the receiver thread while holding `alt_lock` and publishes `threadValid` before unlocking (`altscreen_hook.c:886-901`). A fast child blocks on the same lock in `alt_slot_for_thread`, so it cannot observe a half-published `pthread_t` (`altscreen_hook.c:691-701`).
* A call on the registered alt thread forwards and invokes the completion callback without entering the stock decoder (`altscreen_hook.c:738-758`).
* A call on the unregistered stock main thread passes through to the captured real function (`altscreen_hook.c:760-762`).

Stock 210 cannot attach a useful `AltScreen` property. Its property dispatcher is `_ScreenStreamSetProperty@0x85590-0x85ca4`; unknown keys reach its error path near `0x85c48-0x85c58`, and the binary contains no `AltScreen`/`altScreen` string. The independent stock 280 implementation always `ScreenCopyMain`s and has no alternate-screen property (`Refferences/Carplay/Sources/AirPlayReceiverSessionScreen.c:694-725`). Routing must therefore be owned by the hook, not by a property tag.

The global `g_alt_session` and the single global renderer socket make the implementation one-CarPlay-session-at-a-time despite four hook slots (`altscreen_hook.c:582-600,651-677`; `altscreen_av.c:23-27`). That matches the deployment assumption but should be asserted rather than presented as general multi-session support.

### R5 — time synchronization

The hook's three-word synchronizer layout matches the stock ABI used by `SetTimeSynchronizer`: context plus two callbacks (`altscreen_hook.c:792,812-816`; stock 280 declaration at `Refferences/Carplay/Sources/AirPlayReceiverSessionScreen.h:107-119`).

When `respectTimestamps` is set by config, stock 210 maps the sender NTP value through callback 2 at `0x2b700-0x2b720`. Otherwise it uses `UpTicks` at `0x2b9f8-0x2ba04`. The late calculation at `0x2b724-0x2b790` is accounting/logging; all paths continue to decrypt at `0x2b794`. Therefore the hook's callback that simply returns current `UpTicks` (`altscreen_hook.c:646-649`) does not stall or drop the video. It loses synchronized presentation accuracy only.

That is consistent with the current sink: `_FeedInput` sets every OMX timestamp to zero in decoder-order mode (`altscreen_render/sink_omx_qnx.c:357-373`).

### R6 — concurrency and decoder resources

Creating a second `AirPlayReceiverSessionScreen` object is software-safe: stock `Create@0x2b044` allocates per-instance state and a per-instance queue. The collision is lower in the stack. `StartSession@0x2a3b8` always creates a command socket, `ScreenStream`, and display binding at `0x2a3f0-0x2a414`. `ScreenStreamCreate@0x85ce4` allocates a new 1 MiB buffer and video abstraction at `0x85d70-0x85db0`; `ScreenStreamStart@0x84da0` starts and configures the decoder at `0x84dd8-0x84eb8`.

Hybrid (a) therefore consumes:

1. stock decoder for main 110;
2. stock decoder created for the hook's 111 screen session, bound to the main display but never fed because the patched hook swallows ProcessData;
3. external `altscreen_render` AVC decoder (`altscreen_render/sink_omx_qnx.c:39,211-222`).

No binary proves the APQ8064 BSP's runtime OMX instance limit. The renderer itself is explicitly marked “ON-UNIT UNVALIDATED” at `altscreen_render/sink_omx_qnx.c:18-20`. Consequently this part is **uncertain until an on-unit test**. Required logging is: success/failure from `scr_start`, stock decoder creation, external `OMX_GetHandle`, and first output-buffer callback while the main 110 decoder is live.

Architecture (b) is materially safer because it never calls `scr_start` and thus needs only the known-good stock main decoder plus the cluster renderer.

### R7 — teardown and object lifetime

The current wrapper has two semantic bugs before reachability is considered:

* It declares/returns `int` with an integer fourth argument (`altscreen_hook.c:1018-1019,1052-1053`). The real function is `void AirPlayReceiverSessionTearDown(session, params, OSStatus, Boolean *outDone)`; stock 280 declaration is `Refferences/Carplay/Sources/AirPlayReceiverSession.c:1188-1193`, and MHI3 has the same ABI at `0x61c10`.
* It destroys the alt receiver whenever *any* teardown arrives (`altscreen_hook.c:1024-1050`). Stock 210 distinguishes per-stream teardown from full teardown at `0x26be0-0x26d50`; stock 280 parses `params["streams"]` and only performs full teardown when the stream count is zero (`Refferences/Carplay/Sources/AirPlayReceiverSession.c:1207-1258`). MHI3 explicitly recognizes `AltScreen`/111 at `0x62170` inside `AirPlayReceiverSessionTearDown@0x61c10` and sets `*outDone = (count==0)` at `0x61fcc-0x61fd8`.

The corrected wrapper must stop its alt slot only when:

* `request == NULL`, or `request["streams"]` is absent/empty (full-session teardown); or
* a listed stream descriptor has `type == 111`.

It must leave alt running for a 110-only or audio-only per-stream teardown. On full teardown it must stop/join the alt receiver **before** calling the real function, because the thread reads fields of the main session at `altscreen_hook.c:788,806-820`.

Ordinary preload interposition is not enough for the RTSP route. Stock's router calls local thunk `j_AirPlayReceiverSessionTearDown@0x19264` (one callsite is `0x30b10`), whose GOT word is `base+0xb90a0`. That word must point to the hook wrapper for the network teardown to perform hook cleanup.

#### Missing thread cleanup

Stock `_ScreenThread@0x24220` always executes:

```text
0x243f8  AirPlayReceiverSessionScreen_LogEnded
0x24400  AirPlayReceiverSessionScreen_StopSession
0x24410  NetSocket_Delete(netSock)       // the value, not &netSock
```

The stock 280 source independently shows the same order at `Refferences/Carplay/Sources/AirPlayReceiverSession.c:4880-4885`.

The hook does not call `StopSession` and declares `NetSocket_Delete` as `void(void **)` (`altscreen_hook.c:559,822-830`). Stock `NetSocket_Delete@0x7f200` dereferences its argument as the object, validates its magic at `0x7f20c-0x7f21c`, accesses its fields, and frees that object at `0x7f2a0`. Passing `&netSock` fails validation and leaks the object/native data socket.

`StopSession@0x2a2d0` is not optional: it stops/releases the ScreenStream at `0x2a304-0x2a320`, closes the command socket at `0x2a324-0x2a338`, and finalizes/clears AES state at `0x2a33c-0x2a354`. `AirPlayReceiverSessionScreen_Delete@0x2a838` does not perform that cleanup.

Resolve `AirPlayReceiverSessionScreen_StopSession`, call it exactly once on every thread exit after a possible `StartSession`, then call `NetSocket_Delete(netSock)` and set the local to null. `LogEnded` is useful for parity/diagnostics but is not required to receive frames.

Finally, call `altav_shutdown()` after the alt thread has joined. The renderer treats TCP disconnect as session end and resets OMX at `altscreen_render/main.c:107-120`; the current hook only closes this socket in its library destructor at `altscreen_hook.c:1483-1486`.

### R8 — renderer sink

The intended local protocol matches at the byte level:

* hook: one big-endian 32-bit payload length followed by Annex-B bytes (`altscreen_av.c:67-86`);
* renderer: read exactly four bytes, decode BE length, then read exactly the body (`altscreen_render/frame_io.c:23-39`);
* renderer codec: Qualcomm AVC (`altscreen_render/sink_omx_qnx.c:39,211-222`);
* renderer assumes the first message is SPS/PPS and sets `OMX_BUFFERFLAG_CODECCONFIG` (`altscreen_render/sink_omx_qnx.c:466-477`).

The active configuration is nevertheless broken: `ALTAV_DEFAULT_PORT` is 19830 at `altscreen_av.h:24-25`, while `ALTAV_PORT` is 19831 at `altscreen_render/protocol.h:18-22`. `altscreen_init()` registers control handlers but never invokes `altav_init()` (`altscreen_hook.c:1475-1486`), so the default remains 19830.

Three additional transport bugs can corrupt later attempts:

1. `av_send_locked` uses one `send` for the length and one for the body (`altscreen_av.c:67-78`). A legal short write leaves a partial framed message on the connection before it closes. Use a bounded `send_all` loop that handles `EINTR` and treats timeout/error as connection loss.
2. `altav_forward_frame` returns `void`, yet the tap marks SPS/PPS as sent even if connect/send failed (`altscreen_hook.c:744-753`; `altscreen_av.c:81-87`). Then a later reconnect begins with a video access unit, which the renderer mislabels CODECCONFIG. Return success, cache the config independently, and send it on every new connection before the first access unit.
3. A changed `avcC` cannot be represented safely by “the first message only.” On a config change, close/reopen the local session and send the new config first, or extend the local protocol with an explicit message type. The current renderer marks only its lifetime-first payload as config (`altscreen_render/sink_omx_qnx.c:474-477`).

## 5. Exact GOT rewrite set for the hybrid path

The MHI2Q ELF's `.got` begins at `0xb8d40` inside the second `R|W` load segment (`VA 0xb7740...`); there is no GNU_RELRO program header. These are aligned data writes: no `mprotect` and no instruction-cache flush are required.

All on-disk words are ARM little-endian:

| Purpose | GOT slot / relocation | On-disk bytes | Expected loaded value | Replacement |
|---|---:|---|---|---|
| Route request-bearing SETUP to hook; prerequisite normally owned by negotiation task | `base+0xb8e80`, `R_ARM_JUMP_SLOT AirPlayReceiverSessionSetup`; thunk `0x18c04`, caller `0x2f790` | `b8 72 02 00` | `base+0x272b8` | function pointer to the corrected setup wrapper |
| Route decrypted screen frames to hook tap | `base+0xb9740`, `R_ARM_JUMP_SLOT ScreenStreamProcessData`; thunk `0x1a644`, caller `0x2b7dc` | `ec 4f 08 00` | `base+0x84fec` | function pointer to `ScreenStreamProcessData` wrapper at `altscreen_hook.c:733` |
| Route RTSP teardown through hook-owned cleanup | `base+0xb90a0`, `R_ARM_JUMP_SLOT AirPlayReceiverSessionTearDown`; thunk `0x19264`, caller includes `0x30b10` | `60 6b 02 00` | `base+0x26b60` | function pointer to corrected teardown wrapper |

The replacement bytes cannot be a fixed four-byte literal because the preload function address is a runtime address and may include the ARM/Thumb state bit. Write `LE32((uintptr_t)&wrapper)`.

Implementation requirements:

1. Capture each `RTLD_NEXT` real function pointer before rewriting its slot.
2. Derive `base = (uintptr_t)real_setup - 0x272b8` and validate every expected stock pointer before any write.
3. Use aligned 32-bit stores plus a full memory barrier.
4. Patch before the first applicable RTSP request. Constructor time is feasible because the pages are already writable; if `RTLD_NEXT` is not ready under the QNX loader, install from the first proven dio-to-libairplay boundary call before advertising altScreen.
5. Fail closed: if any required slot or binary identity differs, do not advertise/accept 111.

For the receive path specifically, `base+0xb9740` is the decisive hybrid rewrite. Without it, stock decrypts the frame and then sends it directly to its own decoder; the hook forwarder sees zero frames.

## 6. Exact self-contained receiver specification

Architecture (b) can reuse the existing listener and verified KDF but must replace `scr_create/scr_start/scr_process` with the following loop:

1. Accept the 111 TCP connection.
2. `read_exact(128)` into a packed screen header. Interpret the audited fields at offsets `+0/+4/+5/+8`; reject a body larger than a bounded maximum such as 4 MiB before allocation.
3. `read_exact(bodySize)` even for ignored opcodes, preserving stream alignment.
4. On opcode 1, do **not** decrypt. Parse the `avcC` body: version/profile metadata, `lengthSizeMinusOne`, SPS count/length/data, PPS count/length/data. Convert SPS/PPS to cached Annex-B start-code form. Read width/height from `params[1].f32[0/1]` if needed. The stock-210 behavior being cloned is `0x2b908-0x2b9b8`.
5. On opcode 0, decrypt the entire body in place with the one continuous AES-CTR state initialized from the R2 key/IV. Never decrypt the header or config, and never reinitialize CTR per access unit. Convert each big-endian `lengthSize`-byte NAL length plus payload into `00 00 00 01 + NAL`.
6. Consume/ignore opcodes 2, 4, and 5. Treat unsupported/invalid opcode or malformed lengths as a connection error, matching stock's fail-closed behavior after its switch at `0x2b58c`.
7. Before the first video payload on each local renderer connection, send the cached Annex-B SPS/PPS successfully. Then send each converted access unit using complete `[u32 BE length][payload]` writes.
8. On teardown/error, close the phone socket, disconnect the renderer so it resets the session, zero/finalize AES, and release cached config. No stock `AirPlayReceiverSessionScreen`, `ScreenStream`, or frame GOT patch is needed.

The stream's association with type 111 comes from the listener returned in that descriptor's SETUP response; there is no type field on the video TCP messages. Timestamp `params[0].u64` may be logged or ignored by the current decoder-order sink. The same parser is valid for 110 and 111 because iOS and MHI3 use a common screen implementation at the addresses in R3.

If negotiation ever enables HEVC, this AVC parser must reject it rather than feed HEVC to the AVC renderer. The smallest safe policy is not to advertise/enable `hevc`.

## 7. Ranked fix list and verification

### P0 — required before any frame can display

1. **Ensure the request-bearing setup GOT patch is present.**
   * Change: `base+0xb8e80` from `base+0x272b8` to the corrected hook wrapper.
   * Why: stock RTSP dispatch calls its own thunk at `0x2f790`; ordinary LD_PRELOAD does not own the incoming 111 request.
   * Verify: wrapper log includes a request `streams[0].type=111`, then response log shows exactly one `{type=111,dataPort=N}` descriptor.

2. **Choose and implement one receive architecture.**
   * Hybrid minimum: patch `base+0xb9740` from `base+0x84fec` to the frame wrapper.
   * Robust choice: replace `scr_start/scr_process` with the §6 self-contained loop.
   * Verify hybrid: log the validated GOT old/new values and an increasing alt-tap frame counter while the main 110 pass-through counter also increases. Verify self-contained: log opcode 1/config, first successful AES-decrypted IDR NAL, and frame count.

3. **Fix the renderer endpoint.**
   * Change: `altscreen_av.h:25`, `19830 -> 19831`, or call `altav_init("127.0.0.1",19831)` from `altscreen_init` before frames.
   * Why: renderer binds 19831 at `altscreen_render/protocol.h:18-22`.
   * Verify: renderer logs `hook connected` (`altscreen_render/main.c:114`) and hook logs `connected to 127.0.0.1:19831` (`altscreen_av.c:59-61`).

4. **Make SETUP failure transactional.**
   * Change: return errors from `alt_add_response_stream`; if response construction fails, stop/join/free the created slot; return `arc` from the setup wrapper.
   * Why: iOS requires one matching descriptor and mandatory `dataPort` at `0x2276e5ad0-0x2276e5b20,0x2278dafb8-0x2278dafd8`.
   * Verify: inject allocation failure; RTSP SETUP fails and no listener/thread remains.

### P1 — required for clean teardown and repeat sessions

5. **Correct the screen-thread lifecycle.**
   * Change: resolve `AirPlayReceiverSessionScreen_StopSession`; call it in the thread cleanup; change `fn_netsock_del_t` to accept `void *` and call `netsock_del(netSock)`.
   * Why: stock `_ScreenThread` does so at `0x24400-0x24418`; current `altscreen_hook.c:559,827` is wrong.
   * Verify: StopSession log/counter once; native data fd and command fds disappear; a second CarPlay session starts without leaked decoder/socket state.

6. **Correct and route teardown.**
   * Change: exact `void(...,Boolean *)` ABI, inspect `streams[]`, patch `base+0xb90a0`, join before real full teardown, and call `altav_shutdown()` after join.
   * Why: current wrapper destroys alt on unrelated per-stream teardown and internal RTSP calls bypass ordinary interposition (`altscreen_hook.c:1016-1054`; stock `0x26be0-0x26d50`).
   * Verify: 110-only teardown leaves 111 alive; 111-only teardown closes only cluster path; full teardown returns `*outDone=true`, joins alt, disconnects renderer, and then frees stock session.

7. **Harden localhost framing/config replay.**
   * Change: bounded `send_all`; return send status; cache/replay config per TCP generation; reconnect/reset on config change.
   * Why: `altscreen_av.c:67-87` can emit a truncated message and `altscreen_hook.c:750-752` records unsent config as sent.
   * Verify: kill/restart renderer mid-session; its next connection receives config as message 1 and decodes the following IDR.

### P2 — on-unit risk reduction

8. **Remove the unused stock alt decoder by adopting architecture (b), or prove the hybrid decoder budget.**
   * Why: hybrid starts a stock decoder at `0x84dd8-0x84eb8` and then uses the external decoder as well.
   * Verify: with main 110 live, both `scr_start` and renderer `OMX_GetHandle` succeed, and output buffers arrive continuously. If not, architecture (b) is mandatory.

9. **Validate the renderer on hardware.**
   * Why: the source declares the QNX OMX path on-unit unvalidated at `altscreen_render/sink_omx_qnx.c:18-20`.
   * Verify: first config, first IDR, `FillBufferDone`, QNX Screen post, correct 1440x542 surface, and stable teardown/reconnect.

## Final verdict

Once iOS offers type 111, the current hook can open the advertised data socket and its AES derivation matches stock 210, but it **does not currently deliver video to the renderer**:

* its frame interposer is bypassed by stock's internal call;
* its forwarder targets the wrong TCP port;
* its receiver lifecycle leaks the NetSocket/ScreenStream/AES state;
* its teardown wrapper is both internally bypassed and semantically/ABI incorrect.

Hybrid (a) can be made functional with the exact GOT and code fixes above, subject to an on-unit three-decoder capacity test. Self-contained (b) is the recommended final receive architecture because the wire format is fully recoverable from stock 210, iOS 26.6, and MHI3, and it removes the two highest-risk dependencies: the internal frame dispatch and the redundant stock decoder.
