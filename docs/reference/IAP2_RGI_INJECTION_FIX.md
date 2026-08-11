# iAP2 RGI injection: preserve the stock LocationInfo carrier

## Observed failure

In the failed run the same `dio_manager` completed iAP2 Identify and authentication, then attempted
`StartRouteGuidanceUpdates` (`0x5200`) nine times for more than two minutes without receiving the
first `0x5201`.  During that interval RTSP streams 110/111 continued, but route guidance and later
media metadata did not advance.  A physical reconnect created a new `dio_manager`/iAP2 link; its
`0x5200` received `0x5201` about 1.19 seconds later and both RGI and metadata resumed.

## Failed semantic-carrier experiment

Commit `687596f` tried to avoid copied FF5A sequence state by replacing the next outgoing
LocationInfo (`0xFFFB`) Encode result with `0x5200`. On-unit logs disproved the assumption that this
carrier is expendable during startup: Identify and authentication completed, the replacement fired,
iOS commonly replied with `STOP_LOCATION`, and AirPlay never reached control SETUP across repeated
generations. Known-good logs preserve LocationInfo, inject `0x5200` as an extra packet, and reach
control SETUP roughly one second after authentication.

Relevant stock symbols (MU1316 NME image/database):

- `NmeIAP2Message::Create` at `0x11fd78`;
- `NmeIAP2Message::AddParam` at `0x11fde8`;
- `NmeIAP2Message::Encode` at `0x11ffe8`;
- `NmeTransport::Send` at `0x186e60`;
- dio's PLT call to `CinemoCreateIAP` at `dio_manager:0x17d330`, with the out-parameter block at
  `wrapper+0x9c` and the resulting `ICinemoIAP*` at block offset `+0x00`;
- `CinemoCreateIAP` at `libNmeSDK.so:0x5848c`;
- `ICinemoIAP_AddRef` / `Release` / `SendIAP2` at
  `libNmeSDK.so:0x9b60c` / `0x9b620` / `0x9b748`;
- the concrete ICinemoIAP `SendIAP2` vtable target at `libNmeSDK.so:0x58914`;
- `IAPPassthru::SendIAP2` at `libNmeVfs.so:0x49d60`, which transports the semantic payload with
  explicit `SESSION` and `SIZE` fields; its receiving dispatch is
  `libNmeVfs.so:0x60554` and the local vtable forwarder is `0x64800`.

`Encode` produces the `0x4040` session message in `NmeArray<unsigned char>`; the downstream stock
path wraps that array in FF5A and owns link sequence/ack/checksums.  The already-proven Identify
patch uses this same exported Encode boundary.

## Fix

`hook/framework/hook_framework.c` preserves every stock semantic message.  The hook interposes the
real dio-to-libNmeSDK `CinemoCreateIAP` factory, retains the returned live `ICinemoIAP` with
`AddRef`, and captures only the active link-session byte from a successfully-sent stock FF5A packet.
After stock LocationInfo has completed its own `NmeTransport::Send`, the RGI trigger submits the
additional semantic `0x5200` through `ICinemoIAP_SendIAP2(iap, session, frame, frameLen)`.

This is not a raw retransmit: Cinemo's stock stack builds a new FF5A packet and owns its current
sequence, acknowledgement and checksums.  The injection path takes a transient `AddRef` while sending,
so a concurrent factory replacement/shutdown cannot free the object.  Identify-start invalidates the
captured link-session context; no copied prefix or dio-owned transport pointer survives a reconnect.

The log proving the restored path is:

```
Captured stock ICinemoIAP=... via CinemoCreateIAP
Injected semantic frame via stock SendIAP2 session=... len=...
```

Incoming `0x1D00` (Identify start) also clears captured transport state and all per-session RGI retry/response
state so a fast reconnect in a surviving `dio_manager` cannot inherit `got_520x` or a pending request
from the old link.

## On-unit acceptance

1. Both proof lines above must appear, followed by control SETUP without suppressing stock LocationInfo.
2. The first `0x5201` should arrive within the normal one-to-two-second window.
3. Song metadata/cover art must continue changing while RGI is active.
4. A fast unplug/replug must produce a new Identify-start reset and a new successful `0x5200`.
5. There must be no `Injected frame with prefix` or `Semantic iAP2 send: replaced carrier` log line.
