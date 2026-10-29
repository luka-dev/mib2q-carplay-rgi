# Stock210 receiver lifecycle

The receiver remains MU1316 `libairplay.so` 210.81, SHA-256 `01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2`. The wireless hook/bridge adds pairing and transport support around the stock receiver. SI owns its DIO process.

[Ownership](DIO_OWNERSHIP.md) · [SI protocol](SI_STAGE2_OFFLINE.md) · [Acceptance ledger](../carplay-lab/WIRELESS_COMPLETION_PLAN.md)

<a id="production-components"></a>

## Components and readiness

[advertisement.c](../../hook/wireless/advertisement.c) supplies persisted pairing identity and capability advertisement. The paired media path uses [media.c](../../hook/wireless/media.c); buffered 103 delegates to the existing audio module. Capability masks must describe the enabled build and available codec/media seams. The old PCM-only 100/101 advertisement is a historical milestone, not the current wireless audio contract.

[bridge.c](../../wireless/receiver_bridge/bridge.c) supports the stock210 profile on port 5000. Readiness requires the guarded native receiver, listener and advertised identity; pairing alone is insufficient. IDENTITY is bound to the active receiver generation. Rust uses that identity in iAP2 StartSession. The `experimental280` profile remains an explicit research choice, not the stock210 default.

DNS-SD registration means the API accepted an advertisement. Physical multicast discovery and radio behavior require their own test. See [media validation](../carplay-lab/WIRELESS_MEDIA_VALIDATION.md) for current audio admission and explicit models.

<a id="verified-stock-seams"></a>

## 📋 Verified stock seams

Addresses below are in the target210 shared object unless marked dio_manager.

| Address | Verified behavior / use |
|---|---|
| `0x20D24` AirPlayGetFeatures | uint64 result in r0/r1, stock platform feature bits ORed with its base mask. |
| `0x21054`, `0x223F4` | Bonjour and `/info` consume that exported feature function. |
| `0x211C8` → LR `0x211CC` | TXTRecordSetValue(srcvers,6,"210.81"). The guarded interposer appends pi to the same TXT record. |
| `0x21214` | DNSServiceUpdateRecord consumes the rebuilt TXT. The injection also covers updates and embedded txtAirPlay data, not only initial registration. |
| `0x201E4` SetDelegate | Copies **15 words / 60 bytes** to receiver+16. The 280 bridge's seven-word temporary is insufficient. |
| `0x2E9AC` EnsureCreated | Creates an independent AirTunesServerQueue and the stock11-word HTTP delegate. |
| `0x2EB20`, `0x6C094` | Assign that queue through HTTPServerSetDispatchQueue. Hook captures/retains it after the guarded create. |
| `0x20218` GetDispatchQueue | Returns receiver+12, the main queue; this is **not** the HTTP queue used for admission/drain. |
| `0x31764` StopAllConnections → `0x31684` | Synchronously traverses HTTP connection list. Must execute on its HTTP queue. |
| `0x2EBC4` ConnectionFinalize | Receiver context is HTTP+8; live session at context+244. Tears down/releases session, then clears it. |
| `0x220D0` ServerControl / `0x204E0` StopServers | Stock stopServer path, preserved. |
| `0x2E8E8` EnsureDeleted | Schedules HTTP stop with CFObjectSetProperty flags8; releases server/queue and frees AirTunes state. |
| dio `0x154AD8` | signalStopAirPlayMainThread dispatches the thread's stop at vptr+16. |
| dio `0x158928`, call `0x158978` | Stop calls CFRunLoopStop(CFRunLoopGetMain()). No suppression is installed. |

Functions were read in IDA, annotated and relevant types/locals corrected.
New hook seams extend the pinned instruction fingerprint list. The bridge
separately checks the complete 15-word copy sequence and existing DIO manager
callsite/delegate/property synchronization guards. All 15 fields are copied;
only existing property/session callback slots3/5/6 are wrapped.

<a id="queue-and-owner-lifecycle"></a>

## 🔄 Queue and owner lifecycle

`carplay_wireless_receiver_control(server,operation)` accepts only the current
stock gAirPlayReceiverServer and a captured/retained HTTP queue:

1. Idle stop: on that queue, check all tracked contexts for a live receiver
   session. Return BUSY if any exists. Otherwise close admission before invoking
   stock stopServer. A SETUP queued behind this check receives503.
2. Resume: reopen admission on that queue before the bridge restarts the stock
   server. A recreated HTTP server replaces the retained queue reference.
3. Wired drain: close admission and invoke stock StopAllConnections on its own
   queue. Stock finalizers own media teardown. The bridge permits this only
   while a process holds the wired-priority filesystem lock.

Bridge QUIESCE drains its property callbacks and verifies that the receiver
listener closed before the wired helper may signal the wireless owner. It
uses the same configured AirPlay port as START, including5000 for stock210.
The helper still obtains the owner PID from the kernel lock, not a PID file.

SI owns DIO creation and termination; the runtime keeps bootstrap and mDNS available across child generations. Managed wireless teardown preserves the runtime-owned responder. Stock full media teardown may exit DIO; new media waits for native cleanup and a new admitted generation. See [SI ownership](DIO_OWNERSHIP.md) for current process authority.

`/tmp` is writable RAM, but it does not supply the filesystem lock/rename
semantics used by the native owner helper in this QNX fixture. The stock210
wrapper and configuration put the generation marker at
`/mnt/persist/.mhi2q_dio_generation`, alongside the existing persistent lock
files. Both wireless and wired wrappers export that path to their monitors.
The marker is advisory; the kernel locks determine ownership. Pairing private
keys stay under `/mnt/persist/mhi2q-carplay/pairing`.

## Reproduce and interpret

```sh
python3 wireless/si/build_si_review.py --full-cycle
```

For focused bridge profiles, use [wireless/receiver_bridge](../../wireless/receiver_bridge/). Preserve profile, firmware and candidate hashes with each result. The [original three-generation supervisor fixture](../archive/internal-reference-2026-10-29/wireless-carplay/STOCK210_RECEIVER_LIFECYCLE.md) remains historical protocol evidence; it does not accept current SI admission or real HMI focus.
