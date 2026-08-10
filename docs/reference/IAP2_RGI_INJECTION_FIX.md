# iAP2 RGI injection: link-layer hang fix

## Observed failure

In the failed run the same `dio_manager` completed iAP2 Identify and authentication, then attempted
`StartRouteGuidanceUpdates` (`0x5200`) nine times for more than two minutes without receiving the
first `0x5201`.  During that interval RTSP streams 110/111 continued, but route guidance and later
media metadata did not advance.  A physical reconnect created a new `dio_manager`/iAP2 link; its
`0x5200` received `0x5201` about 1.19 seconds later and both RGI and metadata resumed.

## Root cause in the old hook

The old `NmeTransport::Send` interpose copied the FF5A prefix of the last outgoing packet and called
the real transport a second time with a hook-built `0x5200`.  Recomputing length/checksum did not
allocate a new iAP2 link sequence number: `ctrl`, `seq`, `ack`, and `session` were replayed verbatim.
The first injection ran recursively from the callback observing the current `0xFFFB`; later 1 Hz
retries necessarily reused an already-transmitted prefix.  A receiver may drop the duplicate, and a
second writer operating outside the stock link state machine can disturb acknowledgements for other
services on the same iAP2 link.

Relevant stock symbols (MU1316 NME image/database):

- `NmeIAP2Message::Create` at `0x11fd78`;
- `NmeIAP2Message::AddParam` at `0x11fde8`;
- `NmeIAP2Message::Encode` at `0x11ffe8`;
- `NmeTransport::Send` at `0x186e60`;
- `ICinemoIAP_SendIAP2` at `0x9b748` in `libNmeSDK.so` (vtable dispatch at `+0x3c`; no live handle is
  exposed at the dio/lib boundary).

`Encode` produces the `0x4040` session message in `NmeArray<unsigned char>`; the downstream stock
path wraps that array in FF5A and owns link sequence/ack/checksums.  The already-proven Identify
patch uses this same exported Encode boundary.

## Fix

`hook/framework/hook_framework.c` now queues only a validated semantic `0x4040` message.  At the next
outgoing LocationInfo (`0xFFFB`) `NmeIAP2Message::Encode` result, it replaces the encoded array with
the queued RGI request and updates the array length/capacity.  It never creates or resends an FF5A
header.  LocationInfo is periodic and expendable; only the one carrier frame is replaced.

The final `NmeTransport::Send` interpose is observation-only.  The log proving the safe path is:

```
Semantic iAP2 send: replaced carrier 0xFFFB with 0x5200 ... stock owns FF5A seq/ack
```

Incoming `0x1D00` (Identify start) also clears queued data and all per-session RGI retry/response
state so a fast reconnect in a surviving `dio_manager` cannot inherit `got_520x` or a pending request
from the old link.

## On-unit acceptance

1. Exactly one semantic-send line must precede the first `0x5201`.
2. The first `0x5201` should arrive within the normal one-to-two-second window.
3. Song metadata/cover art must continue changing while RGI is active.
4. A fast unplug/replug must produce a new Identify-start reset and a new successful `0x5200`.
5. There must be no old `Injected frame with prefix` / `Injected raw frame` log line.
