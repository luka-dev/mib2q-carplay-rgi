# Stock 210.81 paired media adapters

Offline analysis and implementation, 2026-09-29. No head-unit access or firmware edits.
Target: MU1316 P5087 `eso/lib/libairplay.so`, SHA-256
`01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2`.
Offsets below are **bytes**, ARM32 little endian; function addresses are ELF virtual addresses.
Evidence: fresh IDA decompilation in session `6817b577`; SDK 280 source is the protocol reference,
not a binary-layout reference. The hook uses stock CF objects and a private, namespaced crypto backend.

## Verified seams

| Stock function | Address | Relevant behavior |
|---|---:|---|
| `_requestProcessSetup` | `0x2f198` | HTTP connection `+8` -> context `+244` -> receiver session. Creates session before calling exported SessionSetup. Legacy `et` handling precedes that call. |
| `AirPlayReceiverSessionSetup` | `0x272b8` | Control setup, audio 100/101, screen 110. May start a newly added stream immediately if the session was already started. Keys must exist **before** real Setup. |
| `AirPlayReceiverSessionStart` | `0x25278` | Accept events fd, then calls HTTPClientCreateWithSocket with output pointer `session+704` at `0x2530c`. |
| `HTTPClientCreateWithSocket` | `0x6a1d0` | Creates stock HTTPClient, stores socket at client `+36`. |
| `_HTTPClientRunStateMachine` | `0x69b50` | Uses exported SocketWriteData (`0x69bb0`) and HTTPMessageReadMessage (`0x69c14`), the same seams used for control HTTP. |
| `_ControlTearDown` | `0x26aa8` | HTTPClientInvalidate + CFRelease is asynchronous; does not mean accepted fd is already closed. |
| `AirPlayReceiverSessionTearDown` | `0x26b60` | Stops/joins screen and audio workers before return. New framework `on_teardown_final` releases stream sidecars after this point. |
| `_ScreenSetup` | `0x26f9c` | Gets streamConnectionID; calls legacy AES Screen_SetSecurityInfo at `0x270b0`. |
| `AirPlayReceiverSessionScreen_SetSecurityInfo` | `0x2a9d8` | AES context screen `+200`, validity byte `+484`. Paired hook requires fresh AES state and suppresses AES initialization. |
| `AirPlayReceiverSessionScreen_ProcessFrames` | `0x2b154` | Reads via NetSocket `+20`, header128 -> body -> decoder. Hook temporarily substitutes only that instance's read callback. |
| `_MainAltAudioThread` | `0x261c4` | SocketRecvFrom at `0x26448`, max1472; RTP header12 then Swap16Mem then stock jitter insertion. |
| `SocketRecvFrom` | `0x7dbac` | Exported UDP receive helper; adapter authenticates before returning to stock audio worker. |
| `AirPlayReceiverSessionWriteAudio` | `0x23948` | Copies microphone PCM to ring, swaps to network byte order, sends RTP via sendto. Original maximum PCM payload1440. |
| `NetSocket_ReadInternal` | `0x7da2c` | Exact/minimum-length reads with timeout and cancellation. Adapter retains it as its underlying read callback. |

`stock210_fingerprint.h` includes 16-byte prologues for the HTTP and new media seams. Hook activation
still requires the stock HTTP delegate address and all pinned fingerprints. The original ELF is unchanged.

## Stock layout used

| Object | Offset | Width | Meaning |
|---|---:|---:|---|
| HTTP connection | 8 | 4 | AirTunes context pointer |
| AirTunes context | 244 | 4 | AirPlayReceiverSession pointer |
| Receiver session | 700 / 701 / 702 | 1 each | Control initialized / use events / session started |
| Receiver session | 704 | 4 | Events HTTPClient pointer |
| Receiver session | 708 / 712 | 4 each | Events listen fd / listen port |
| Receiver session | 720 / 2896 | embedded | Main / alt audio contexts |
| Audio context | 0 / 4 / 16 | 4 each | Session pointer / stream type / UDP fd |
| Audio context | 20 / 24 | 4 each | Thread ID / thread marker pointer |
| Audio context | 32 | embedded | RTP jitter buffer |
| Audio context | 1592 / 1596 / 1600 / 1604 | 4 each | Sample rate / channels / bytes per frame / bits per sample |
| Receiver session | 5136 / 5140 / 5144 | 1 / 4 / 4 | Screen initialized / screen object / listen fd |
| Screen object | 72 | 128 | Original wire screen header |
| Screen object | 76 / 78 | 1 each | Opcode / config flags |
| Screen object | 80 | 8 | Frame timestamp |
| Screen object | 88 / 92 | float each | Configuration dimensions |
| Screen object | 200 / 484 | embedded / 1 | Legacy AES context / valid flag |
| Screen object | 488 / 492 | 4 each | Command socket / ScreenStream pointer |
| NetSocket | 20 | 4 | `int read(net,min,max,buffer,size_t *out,int flags,int timeout)` |

No 280 receiver, screen, HTTP, or CF structure is grafted over these objects.

## Key direction and packet formats

Source reference: `Refferences/Carplay/Sources/AirPlayReceiverSession.c`, `_GetStreamSecurityKeys`,
`_ControlStart`, `_MainAltAudioProcessPacket`, `AirPlayReceiverSessionWriteAudio`;
`AirPlayReceiverSessionScreen.c`, video-frame processing. Pair-Verify shared secret derives all keys.

* Events: HKDF-SHA512, salt `Events-Salt`. Receiver's HTTP **client** reads with
  `Events-Read-Encryption-Key`, writes with `Events-Write-Encryption-Key`.
  Independent control-style record transport, two-byte LE length AAD, 16-byte tag, separate counters.
* Streams: salt `DataStream-Salt` followed by decimal unsigned streamConnectionID.
  Phone -> receiver: `DataStream-Output-Encryption-Key`; receiver -> phone:
  `DataStream-Input-Encryption-Key`. Each key is 32 bytes.
* Screen opcode0: ChaCha20-Poly1305 64x64, nonce8 starting at zero, AAD is the **original entire
  128-byte header**, including tag-inclusive wire body length. Body = ciphertext + tag16.
  Adapter reduces the stock-visible body length, verifies before stock decoder access, increments
  nonce only after success. Config/control opcodes remain stock plaintext. Maximum accepted body16MiB.
* Modern iOS audio: clear RTP header12, encrypted PCM, tag16, explicit nonce8 trailer.
  AAD is timestamp+SSRC (header bytes4..11). The older full-header AAD protocol is not implemented.
  Received audio is authenticated before stock byte swapping and jitter insertion.
* Microphone: stock's payload1440 must be split to at most1416 plaintext bytes (rounded to whole
  PCM frames), preserving the SDK's IPv6 packet bound1452 after RTP12 + tag16 + nonce8. The adapter
  maintains output RTP sequence, adjusts sample timestamps per fragment, and consumes a new nonce
  on every encryption attempt, including a failed send. It reports original plaintext length only
  when all fragments were sent.

## Lifetime and enablement

`MHI2Q_DIO_OWNER_MODE=wireless`, `MHI2Q_CP_WIRELESS_PAIRING=1`, and
`MHI2Q_CP_WIRELESS_MEDIA=1` are all required. Pair-Verify must have completed and its plaintext M4
must have drained before a SETUP can reach this adapter. Without the media flag, encrypted SETUP
still gets461. No feature/TXT/receiver-ready advertisement is enabled by this work.

Before stock dispatch, a mutable stock plist copy removes obsolete `et`, `ekey`, and `eiv`.
Only the in-memory body is replaced; raw header/URL pointers and pipelined extra bytes are preserved.
The stock parser then creates/fetches its session. Pre-Setup callback finds the verified parent by
HTTP context `+244`, validates every requested stream and derives sidecar keys before real Setup.
Unsupported stream types/formats and zero/reused IDs are rejected before real Setup.

Current supported media scope: screen110, main100/alt101 with one of the stock16-bit PCM formats.
No AAC, APAC/type103, 24-bit PCM, or old-iOS full-RTP-header AAD is claimed. Stream IDs are retained
in a bounded history (32) until control close; duplicates are rejected to avoid key+nonce reuse.
After full TEARDOWN the verified control connection is retired for media; a fresh Pair-Verify
connection is required to restart media. Selective teardown permits new stream IDs.

Keys are owned by the module, never raw PairingSession pointers. Stream cleanup runs after stock
joins workers. HTTP final cleanup forgets the sidecar only after the stock connection finalizer.
Events copy their own keys and outlive control/session sidecars when necessary: the narrow `close`
interposition removes the transport only when its actual fd is relinquished. Matching event I/O
and close share a mutex. There is no global read/write replacement.

## Offline fixture

`stock210_media_qemu.c` (capture fixture only) supplies fake AudioStreamPrepare/Start, because the
QEMU image has no HMI audio acknowledgement/hardware. Stock Setup, Start, RTP worker, jitter buffer,
WriteAudio, events HTTPClient and screen processing remain real. A fake OMX endpoint checks decrypted AVC
input delivery; it does not prove physical display operation. The base fake OMX incorrectly frees caller-owned `UseBuffer` data; the fixture
wraps that fake component's UseBuffer/FreeBuffer to leave screen-owned memory
alone. It also makes fake FillThisBuffer input-only: no fabricated NV12 frames
or FillBufferDone callbacks. The base fake otherwise keeps recycling output
headers across its immediate Loaded transition and stock render teardown.
Thus this fixture tests authenticated AVC delivery and teardown with **no
pending rendered output**, not the real decoder's drain/display behavior.
No fixture code is built into the hook.

`stock210_media_phone_qemu.c` implements the independent phone-side events/microphone assertions.
The phone and hook do not share PairingSession/HTTP structures. Test status and exact evidence will
be recorded in `stock210_pairing_validation.md` after successful execution.

## Lifecycle boundary observed in QEMU

The unmodified stock receiver/DIO lifecycle stops the server and releases
`dio_manager` after a full media TEARDOWN. The media test therefore performs
stored-peer Pair-Verify reconnect **before** its one media session. A successful
TEARDOWN is not evidence that a second media session can start in the same DIO
process. Production readiness must integrate receiver exit/restart with the
existing wireless owner supervisor, its generations, and wired SI priority.
