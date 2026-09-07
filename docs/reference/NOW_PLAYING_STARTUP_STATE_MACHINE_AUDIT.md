# CarPlay startup / Now Playing state-machine audit

Date: 2026-09-05
Status: static Pioneer + iOS 27 audit complete; bounded cold-connect recovery
implemented and deployed in the compact state-trace build; vehicle cold-start
validation pending

## Question and observed failure

The remaining vehicle symptom is not the already-fixed PCM6 rendering path:

- immediately after connecting the phone, Apple Music can show playback on the
  phone while the HU has no audio and no track metadata;
- Play and Next in CarPlay do not recover it;
- opening Apple Music on the phone recovers the session;
- in another failure, audible content advanced while the HU kept the preceding
  track's metadata.

The last observation proves that audio transport and displayed Now Playing
state can advance independently. This audit therefore follows three separate
rails instead of treating "CarPlay playback" as one state machine.

| Rail | Head-unit entry | iOS owner | Observable result |
|---|---|---|---|
| iAP2 Now Playing | `0x5000` request / `0x5001` update | `accessoryd` plus `AccessoryNowPlaying` XPC service | HU title, artist, playback state and commands |
| AirPlay MediaRemote binding | control SETUP / endpoint activation | `AirPlaySender` | command sink and metadata source bound to a routing context |
| Audio | type 100/103 SETUP, RTP/APAP, feedback, QSA | `AirPlaySender` and the selected media app | decoded PCM at `/dev/snd/mpl5_dio_ent` |

The static result is three genuine recovery asymmetries distributed across the
two metadata rails. None alone proves the complete vehicle symptom. A small
passive trace is required before changing behavior.

## Exact images

The Pioneer target is MU1316 MHI2Q US P5087, not MH2p:

| Image | SHA-256 |
|---|---|
| `app/eso/bin/apps/dio_manager` | `e9862680d76ca2e6bb2eed067658f2ca8fa9b71b5446322384b76c9735c0e794` |
| `app/armle/usr/lib/cinemo/libNmeVfs.so` | `7cb704189842a03e07cc1360813eda0e9175150df85facc65d6bac5427726fad` |

iOS is build `24A5418b`, iPhone18,1:

| Image | SHA-256 |
|---|---|
| `AccessoryNowPlaying` from the dyld shared cache | `5870bd01e1166f40a705d050c8762f56e7b067f62d13d8f7ffc77dabf52da35c` |
| `accessoryd` | `3c736704d7edd2dbc3aa4d7adfacb3dae14ad7e80e2b0a6d78d4e23e1081f9ef` |
| `ACCNowPlayingFeature.xpc/ACCNowPlayingFeature` | `cc8a052cb72e2719d67628204f190be57dd2dae41f55ef3190493e897a9ee509` |
| `AirPlaySender` from the dyld shared cache | `379fb232ff228287d3b11d1253b1b942e6c50dd633d150cfab6acb9d063bead1` |

The annotated IDBs are next to the extracted images under
`Refferences/ios27_dsc/24A5418b__iPhone18,1/state_machine_out/` and
`aps_out/`. The Pioneer `libNmeVfs.so.i64` is next to the MU1316 library.

## Pioneer: `0x5000` is a one-shot physical-identification action

### Construction and only caller

`libNmeVfs.so:NmeIAP2_StartNowPlayingUpdates @ 0xD907C` creates message
`0x5000`. Its requested media attributes are:

```text
0, 1, 4, 6, 7, 8, 9, 0x0A, 0x0C, 0x10, 0x12,
0x15, 0x16, 0x17, 0x18, 0x1A, 0x1B
```

Its playback list is `0..0x10`. The completed semantic message is queued by
the normal control-session send path at `0xD8320`.

IDA finds exactly one code xref to this routine:

```text
NmeIAP2Device_CompleteIdentification @ 0xE7A00
    0xE7A58: call NmeIAP2_StartNowPlayingUpdates
```

The complete creation chain also has one caller at every step:

```text
NmeIAPDevice_OpenIAP              @ 0x10DB64
  0x10DDEC -> NmeIAPDevice_ConnectDevice @ 0x10C0D0
    0x10C640 -> NmeIAP2Device_Create     @ 0xE7DF8
      0xE7FE8 -> CompleteIdentification @ 0xE7A00
        0xE7A58 -> StartNowPlayingUpdates @ 0xD907C
```

`NmeIAP2_StopNowPlayingUpdates @ 0xD8B58` has no code or data xrefs. There is
no periodic resubscribe, response timeout, or retry call to `0xD907C`.

### The apparent reconnect branch is not an iAP2 recovery path

`NmeIAPDevice_OpenIAP @ 0x10DB64` has a second branch:

```text
0x10DE78: call NmeIAPDevice_ReConnect @ 0x106714
```

At `0x1067CC` the reconnect routine reads the existing device's iAP2 flag. If
set, `0x1067F0` returns error 162 and logs that iAP2 devices must not be
reconnected (its string cites specification R23 section 2.13). Only the iAP1
case reaches device recreation. Consequently this branch cannot resend
`0x5000` for an existing iAP2 device.

### `dio_manager` ownership does not add a retry

`CIAP2ServiceCinemo::onJob_connect @ dio_manager:0x17C3F8` opens the Cinemo
iAP object only when its member at `+0x9C` is null. Its service-level start
sequence enables communication/call/power updates; it does not call a public
StartNowPlaying routine.

The concrete ARM sites are:

```text
0x17C588  load this->m_pIap (+0x9C); non-null takes "Already connected"
0x17D330  call CinemoCreateIAP; returned object is stored at +0x9C
0x17D5A4  prepare Open flag 0x0E (Identify | Device | ForCarPlay)
0x17D5BC  call the object's Open method
0x17DAC8  StartCommunicationsUpdates (vtable +0x7C)
0x17DB18  StartCallStateUpdates           (vtable +0x74)
0x17DBAC  StartPowerUpdates               (vtable +0x68)
```

There is no adjacent public Now Playing start call. That action occurs inside
`libNmeVfs` only after the physical iAP2 identification chain shown above.

`CIAP2ServiceCinemo_iap2Stop @ 0x179798` closes and releases that outer
object. `CDIOManager_notifyiAP2DeviceDisconnected @ 0x15410C` only clears the
connected flag at `+484`; `CDIOManager_closeIAP2Connection @ 0x1554EC`
enqueues a close only while that flag is still set. The AirPlay teardown calls
the latter from `CDSICarPlay_tearDownTheSession @ 0x15FA58`.

This does **not** prove a dangling object bug: `libNmeVfs` owns physical device
children internally. It does prove there is no second Pioneer Now Playing
subscription associated with a new AirPlay media session. The subscription is
tied to physical iAP2 identification.

### Receive side

`NmeIAP2_OnControlData @ 0xE4864` dispatches message `0x5001` to
`NmeIAP2_OnNowPlayingUpdate @ 0xE1E00`, which decodes the attributes and invokes
registered callbacks. `CIAP2ServiceCinemo::onNowPlayingUpdate @
dio_manager:0x17AA90` then aggregates title, duration, album, artist, genre,
composer, status, elapsed time, shuffle/repeat and application fields.

The Pioneer side can therefore remain stale indefinitely if its sole `0x5000`
does not result in a usable `0x5001`. Static code provides no self-healing
timer.

## iOS `accessoryd`: a fresh `0x5000` is self-healing

`NowPlaying_CreateFeature @ accessoryd:0x100146AC8` creates the per-link
feature object and increments the shared `AccessoryNowPlayingClient` user
count. `NowPlaying_DestroyFeature @ 0x100146C84` removes active subscribers and
decrements it.

`_startNowPlayingUpdatesHandler @ 0x10014788C` handles `0x5000`:

1. it validates feature 14 and parses the two requested attribute groups;
2. at `0x100147E00` it removes subscribers from the preceding request;
3. it replaces the media/playback arrays, stores flags and marks the request
   active;
4. it copies default/current media and playback dictionaries through
   `platform_nowPlaying_copyDefaultMediaItemAttributes @ 0x10008EC10` and
   `platform_nowPlaying_copyDefaultPlaybackAttributes @ 0x10008F000`;
5. at `0x100148060` it constructs and sends an immediate initial `0x5001`;
6. asynchronous blocks add live `PlaybackAttributes` (`0x1000812C0`) and
   `MediaItemAttributes` (`0x100081360`) subscribers and request their current
   values.

The immediate response is produced before live registration completes and
does not depend on opening the Music UI. Even if the current MediaRemote
dictionaries are empty, the conversion routines supply explicit defaults.

`_stopNowPlayingUpdatesHandler @ 0x1001487D8` pauses its timer, removes
subscribers and clears the arrays/flags. A new `0x5000`, rather than a new
audio SETUP, is what rebuilds this state.

This yields a decisive negative: if iOS receives a genuinely fresh `0x5000`,
the old XPC subscriber state cannot by itself explain a permanent blank HU.
The handler sends an initial `0x5001` and its remove/add mutations force the
client to reconnect.

## `AccessoryNowPlaying`: interruption heals, invalidation does not

The shared client connects to
`com.apple.accessories.now-playing-feature` in
`_connectToServiceOnQueue @ 0x22619105C`.

Two NSXPC failure callbacks are materially different:

| Event | Function | State transition |
|---|---|---|
| interruption | `AccessoryNowPlaying_XPCInterruptionHandler @ 0x226191628` | clears the remote object, sets `needsSubscriberListResync`, schedules reconnect after 1 second (`0x226191784`) |
| invalidation | `AccessoryNowPlaying_XPCInvalidationHandler @ 0x226191538` | clears the invalidation handler, remote object and service connection; sets no resync flag and schedules no reconnect |

`_updateSubscriberListOnQueue @ 0x226192020` always calls connect first and,
when a remote exists, sends the complete current subscriber list. Add/remove
subscriber operations call it. Current media/playback requests also call
connect first. Therefore any later subscription mutation heals the invalidated
connection; a stable nonzero user/subscriber set with no mutation does not.

The user count connects only on its transition to one (block at `0x226191AB8`)
and disconnects on transition to zero (`0x226191BA8`). This is a real static
recovery hole, but a fresh handled Pioneer `0x5000` exercises remove/add and
therefore heals it.

## iOS XPC service: Music UI is not a startup prerequisite

`ACCNowPlayingFeature` is the actual XPC server executable. `main @
0x100001A98` constructs the feature/server and starts the feature before
starting the listener.

`-[ACCNowPlayingFeature startFeature] @ 0x100002270`:

- creates an `MPMusicPlayerController` client named
  `com.apple.accessoryd.mediaplayer`;
- registers MediaRemote observers;
- enables Now Playing and supported-command notifications;
- immediately primes playback state, now-playing information, playback
  application and supported commands.

No branch launches Music or waits for its UI. The server's current-value RPCs
at `0x10000E48C` and `0x10000E634` directly call the delegate's
`currentMediaItemAttributes` / `currentPlaybackAttributes` and reply; they do
not gate on the subscriber list. The feature converts its stored MediaRemote
state in `currentMediaItemAttributes @ 0x10000642C` and
`currentPlaybackAttributes @ 0x100007374`, supplying defaults for missing
keys. `subscriberListDidUpdate: @ 0x100008994` only logs the new list.

The server keeps one global subscriber list. Unexpected loss of its last XPC
client arms a 60-second grace timer; clean loss releases the activity
transaction immediately. The grace event at `0x10000CFDC` releases that
transaction but does not itself clear the subscriber list. Media/playback
updates (`0x10000E708` / `0x10000E980`) broadcast to live client connections;
only artwork and playback-queue-list delivery consult specific subscription
entries.

Thus opening Apple Music can publish a new MediaRemote now-playing application
or new attributes, but it cannot directly reconnect an invalidated
`AccessoryNowPlayingClient` when there are zero XPC client connections. If
opening Music healed the HU, another concurrent lifecycle mutation must also
have occurred, or the XPC connection was still live. Static evidence does not
identify which.

## `AirPlaySender`: routing-context objects are not rebound on every update

AirPlay has a second, independent MediaRemote binding:

- `_endpoint_Activate @ 0x25116D480` reads activation options before creating
  endpoint services;
- `endpoint_readActivationOptions @ 0x2510D183C` reads
  `_kFigEndpointActivateOptionKey_RoutingContextUUID` and retains it at
  activation-state `+0x40`, which is endpoint-state `+0xC8` at its callers;
- `endpoint_createMetadataSourceAndCommandSinkIfNeeded @ 0x25116E278` creates
  `APMediaRemoteCommandSink` and `APMetadataSource` bound to that UUID.

The creation helper's exact guard at `0x25116E2AC` is:

```c
if (old_features == 0 && new_features != 0) {
    create_command_sink(... routingContextUUID ...);
    create_metadata_source(... routingContextUUID ...);
}
```

With routing-context mode selected, a missing UUID returns the function's
explicit error instead of falling back to a PID binding.

`endpoint_updateFeaturesInternalWithContext @ 0x251170018` has the latent
stale-binding case:

```text
0x2511700EC  ORR  combined, oldFeatures, newFeatures
0x2511700F0..0x2511700F8  equal masks -> exit
0x25117028C  non-equal accepted update may reread activation options
0x2511702F4  call creation helper(oldFeatures, newFeatures)
```

An equal feature mask exits before options are reread. A nonzero-to-nonzero
subset/superset change may update the stored UUID, but the helper still refuses
to recreate the already-existing command/metadata objects because `old_features`
is nonzero. Only full feature deactivation reaches the stop/reset paths that
release those services.

This branch is statically real, but its use on the failing CarPlay lifecycle is
not established. The feature-update entry is exposed through an indirect
interface rather than a direct in-image caller. Conversely, both
`_endpoint_dissociateInternal @ 0x250F70870` (call at `0x250F70AB0`) and
`_endpoint_deactivateInternal @ 0x2510D6E28` (call at `0x2510D7380`) invoke
`_endpoint_stopMediaRemoteAndMetadataServices @ 0x250F71104`. A full
dissociate/deactivate therefore clears the binding before the next activation;
only endpoint reuse through the nonzero-to-nonzero update path can preserve it.
The vehicle's choice between those paths requires the passive trace below.

This is a second real recovery asymmetry: commands/metadata can remain bound
to an old routing context across a nonzero feature update. It can explain
metadata or controls diverging from live audio. It does not by itself prove why
the phone did not open an audio stream on the reported cold connection.

## What is proved, inferred, and falsified

### Proved statically

- Pioneer sends `0x5000` once per successful physical iAP2 identification and
  has no retry caller.
- Pioneer receives `0x5001` independently of the AirPlay audio pipeline.
- A fresh iOS `0x5000` handler invocation sends an initial `0x5001`, then
  rebuilds live subscribers.
- XPC interruption reconnects; XPC invalidation alone does not.
- Later subscriber mutation reconnects and resynchronizes the complete list.
- The iOS Now Playing XPC service starts and primes MediaRemote without the
  Music UI.
- AirPlay command/metadata objects are created only on a zero-to-nonzero feature
  transition and can keep an older routing-context binding.
- Audio and HU metadata can legitimately advance independently.

### Falsified

- Opening the Apple Music UI is not a coded prerequisite for starting the iOS
  Now Playing service.
- A new AirPlay media SETUP does not cause Pioneer to resend `0x5000`.
- The Pioneer `ReConnect` branch is not an iAP2 resubscribe mechanism.
- XPC invalidation alone cannot remain stuck after a fresh handled `0x5000`.
- The symptom does not prove an AAC/APAC decoder, QSA, upmixer or PCM6 failure.

### Inference, not yet proof

- The cold failure is consistent with the sole Pioneer `0x5000` being absent,
  queued but not delivered, or delivered without a returned `0x5001`.
- Stale metadata while audible audio advances is consistent with either the
  iAP2/XPC rail being stale or the AirPlay command/metadata objects retaining
  an old routing context.
- Opening Music likely changes MediaRemote application/session publication or
  route activation. Static code does not show which additional event heals the
  observed unit.

## Minimum safe runtime discriminator

Do not add a functional retry yet. Add one compact, generation-scoped passive
trace, usable independently of the verbose audio logs. It needs only counters,
timestamps and IDs; no payload dumps and no private UUID text.

| Marker | Required fields |
|---|---|
| physical iAP2 identification | physical generation, Identify complete |
| outbound Now Playing start | generation, `0x5000`, stock send result, link sequence/ACK if observable |
| inbound Now Playing update | generation, first `0x5001`, media/playback attribute counts |
| RGI control | first `0x5200`, first `0x520x`; correlation only, not a dependency assumption |
| AirPlay control | SETUP generation, proposed/enabled features, full activation versus feature update |
| audio | type 100/103 SETUP, selected format, SETRATE 0/1, FLUSH, teardown |
| render | QSA open and first successful write |

Decision table:

| Trace result | Boundary selected |
|---|---|
| physical Identify completes, no outbound `0x5000` | Pioneer one-shot lifecycle |
| `0x5000` queued/sent, no link ACK | iAP2 transport/lifetime |
| `0x5000` ACKed, no inbound `0x5001` | iOS `accessoryd`/Now Playing handler boundary |
| first `0x5001` is present but stays default/empty | MediaRemote publication/XPC state |
| nonempty `0x5001` arrives but HU remains blank | Pioneer callback/HMI propagation |
| metadata works, but no type 100/103 SETUP | iOS media-route/application selection |
| audio SETUP/SETRATE arrive but QSA never writes | hook/render rail |
| audio advances while metadata/commands remain stale | compare iAP2 generation with full AirPlay activation; test the stale routing-context branch |

The current `LOG=0` preproduction build deliberately removes the evidence
needed for this discrimination. The diagnostic should be a narrow
`AUDIO_STATE_TRACE=1` mode (or an always-small bounded state ledger), not a
return to packet dumps. The unit can stay on its known-good audio behavior
while this trace is collected.

## Implemented compact trace

The discriminator is implemented as a build-time-only diagnostic:

```sh
LOG=0 AUDIO_STATE_TRACE=1 ./scripts/build_hook.sh
./scripts/deploy_over_ssh.sh
./scripts/carplay_state_trace.sh clear
```

`LOG=0` still removes every ordinary `LOG_*` call and all historical
APAP/RTCP/plist/cookie/elementary-stream/PCM file captures. The independent
state-trace switch retains only the existing bounded asynchronous log writer
and writes `/tmp/carplay_state_trace.log`; a stock callback enqueues one short
record and never performs filesystem I/O. No protocol response, decoder,
rendering decision, retry or timeout is changed.

The `clear` helper truncates the current inode instead of unlinking it: it is
safe to run after the deployment has restarted `dio_manager` and opened the
file. Trace-only shutdown drains records already in the bounded queue, but the
wait remains capped, so a blocked `/tmp` cannot turn process exit into a
watchdog hang.

Each record has a stable machine-readable prefix:

```text
ST ev=<process-event> mono_ms=<monotonic-time> pid=<process-id> gen=<physical-Identify-generation> name=<event> ...
```

The implemented markers cover:

- Identify start/accepted/end and authentication;
- semantic `0x5000` construction, its stock `NmeTransport::Send` result and
  exact FF5A sequence/ack/session fields;
- bounded raw FF5A receive-header observations after `0x5000`, including both
  `ack == tx_seq` and `ack == tx_seq + 1` without guessing which convention the
  live stack uses;
- every `0x5001`, with payload hash and the two nested group counts confirmed
  from Pioneer: top-level parameter 0 is `MediaItemAttributes`, parameter 1 is
  `PlaybackAttributes`;
- the first `0x5200` and first other `0x520x` per physical generation;
- AirPlay receiver session edge, SETUP ordinal/result/timing, proposed and
  enabled feature counts, `mainBuffered`, every stream's type/format/index,
  data port and buffer size, `/info`, feedback and teardown;
- buffered cookie changes, decoder/index/geometry, SETRATE, FLUSH and render
  stop;
- DIO5 QSA open/reclaim, params, setup, first successful stock/detached write,
  write anomalies, flush and close.
- every HID report that actually reaches `HIDDevicePostReport` (bounded to 256
  per physical generation), plus decoded Consumer usage/assert/release from
  `HIDRemotePlaybackFillReport` for HU Play/Next/Previous controls.

`pid` is part of the analyzer's generation key. A `dio_manager` restart resets
its local generation counter, so grouping on `gen` alone would silently merge
two different CarPlay lifetimes. The raw iAP2 receive reassembler holds one
complete 16-bit-length FF5A packet; it logs header fields and hashes/counts,
never the packet body.

The HID signatures are not guesses. In the exact MHI2Q `dio_manager` image,
`CDIOManager_sendButtonHIDReport` at `0x1566c8` calls the four-argument
`HIDRemotePlaybackFillReport` at `0x15694c`; on success, `0x156960`--`0x15697c`
copies exactly one report byte into a `CBlob` and queues it. The asynchronous
`CHIDService_onJob_sendHIDReport` at `0x176f70` loads the pointer/length from
that blob at `0x176fb0` and calls
`HIDDevicePostReport(device, data, length)` at `0x176fb4`. The IDA database now
contains these names, types and call-site comments. A touchscreen Play tap is
observable as a generic HID post (coordinates are intentionally not logged);
a native HU Play/Next/Previous action additionally yields the decoded Consumer
usage in `HID_REMOTE_FILL`.

The HU cannot directly observe whether iOS called `AirPlaySender`'s internal
feature-update entry. `session_edge=new|existing`, SETUP ordinal and full
receiver teardown are therefore the exact observable proxy; the audit does not
mislabel an ordinary second stream SETUP as a proved sender-side feature update.

For the decisive capture, leave Apple Music closed, connect the phone, wait
about 15 seconds, try HU Play and Next, and only then open Apple Music once so
the same trace contains the healing edge. If playback starts, also perform one
track change and one lock/unlock cycle. Do not create `carplay_verbose`; it is
unneeded and would mix the compact trace with unrelated logs. Pull and classify
the result with:

```sh
./scripts/carplay_state_trace.sh pull
```

The helper copies the current file and rotations into a timestamped
`captures/state-trace/` directory and runs
`tools/analyze_carplay_state_trace.py`. Its boundary result is deliberately
conservative: raw ACK fields are reported, while a retry/rebind is not selected
until the observed failure crosses the corresponding line in the decision
table above.

## Vehicle capture: cold buffered startup healed by opening Music

The compact trace build was deployed on 2026-09-05 and captured one physical
CarPlay generation continuously before and after the user opened Apple Music.
The complete capture is
`captures/state-trace/20260905-110501/carplay_state_trace.log`.  This is a
measurement of one failure/healing cycle, not an inference from the static
state machines.

### Failed state before opening Music

At `00:27:24.995` Pioneer built and successfully sent its single `0x5000`.
iOS immediately returned the expected initial `0x5001` burst: the first packet
contained all 17 requested media attributes and 16 playback attributes.  The
raw link header also acknowledged the request.  Thus the original broad
hypotheses "the HU did not send `0x5000`" and "no `0x5001` crossed iAP2" are
false for this occurrence.

The buffered audio rail was also established:

```text
00:27:27.221  SETUP type=103, format index 23 (AAC-LC/48 kHz/stereo)
00:27:27.282  39-byte codec cookie stored
00:27:27.306  AAC decoder ready, 2-channel input -> 6-channel music-stage output
00:27:27.348  SETRATE 1
```

Nevertheless the HU showed no current track/audio context and
`mpl5_dio_ent` was not opened.  From then until the healing action, iOS sent
hundreds of `0x5001` packets, but almost all were a 12-byte playback group with
one changing progress attribute.  There was no new media-item group after the
five-packet initial burst.  Every `updateFeedback` call during this interval
returned success with no response object and no realtime clock:

```text
response=0 realtime_clock=0 buffered_active=1
```

The missing QSA open is downstream of the missing stock media activation in
this capture.  It is also a real limitation in the hook's current cold-start
design: decoded type-103 PCM is drained only through a QSA handle first opened
by stock type 100.  The detached buffered writer can retain and drive that
handle after type 100 closes, but it cannot bootstrap a handle when type 103 is
the first and only media stream.  The feedback fallback has the same hole: it
can publish a detached measured/nominal clock or replay a cached stock clock,
but a cold session has neither, so it leaves the response blank.

### Healing edge after opening Music

Opening Apple Music on the same connected phone, without clearing the trace or
starting a new AirPlay session, produced this sequence:

```text
00:32:44.215  0x5001 media group, 82-byte payload, 6 attributes
00:32:44.758  0x5001 media group, one attribute
00:32:47.843  0x5001 media group, one attribute
00:32:56.067  SETUP type=100 requested in the existing AirPlay session
00:32:56.930  mpl5_dio_ent opened successfully
00:32:56.957  QSA expanded from stereo source geometry to PCM6
00:32:56.977  first 9216-byte QSA write, 768 buffered frames consumed
00:32:57.258  updateFeedback response=1 realtime_clock=1
00:33:01.262  stock type 100 closed; the same handle passed to detached type 103
00:33:01.288  first successful detached buffered write
```

The user heard audio immediately after this edge and confirmed that title,
artist and artwork appeared on the HU at the same recovery.  The QSA write
explicitly reported `buffered_frames=768`, so the recovered sound came from
the already running type-103 decode ring rather than proving that type 100
supplied the music bytes.  This also rules out a persistent Pioneer
post-decode/HMI publication failure for this occurrence: once iOS published a
changed media item, Pioneer displayed it.

This establishes correlation and ordering, but not that the 82-byte media
update itself caused type-100 SETUP: foregrounding Music can independently
refresh MediaRemote publication and activate another AirPlay stream.  The
current raw trace is before `NmeIAP2_OnNowPlayingUpdate`'s decoded callback and
does not record individual attribute IDs or value lengths.  One more compact
trace revision should record those non-content fields and the decoded Pioneer
callback before choosing between a Now Playing resubscribe, a buffered-QSA
cold-start path, or both.

### Confirmed receive/callback condition

Static reinspection of the exact `libNmeVfs.so` confirms the downstream
condition.  `NmeIAP2_OnNowPlayingUpdate @ 0xE1E00` clones the cached media item,
applies `MediaItemAttributes` through `sub_E09F4`, and calls the registered
listener at vtable slot `+0x34` only when `MediaItem::compare` reports a change
(`0xE2010`--`0xE212C`).  The target in the exact Pioneer executable is
`CIAP2ServiceCinemo::onNowPlayingUpdate @ 0x17AA90`, present in the
`CIAP2ServiceCinemo` vtable at `0x1A5764`.  That callback copies title, duration,
album, artist, genre and composer and updates playback status, elapsed time,
shuffle, repeat and application state.  Therefore receiving a syntactically
full default/stale `0x5001` is not equivalent to Pioneer observing a changed
current media item.

## Static verdict

There is no static basis to change AAC/APAC, PCM6, QSA, the upmixer, or the
amplifier for this symptom.

The Pioneer/iOS comparison exposes three real but distinct recovery holes:

1. Pioneer issues Now Playing `0x5000` only once and never retries it.
2. `AccessoryNowPlaying` loses autonomous XPC recovery after invalidation
   unless a later subscriber/current-value operation reconnects it.
3. `AirPlaySender` can retain command/metadata objects bound to a stale routing
   context across a nonzero-to-nonzero feature update.

The captured cold failure narrows, but does not yet choose among, those holes.
It is not explained by a missing `0x5000`, missing ACK, missing initial
`0x5001`, failed type-103 negotiation, or failed AAC decode: all of those
completed.  The observed broken contract is instead `buffered_active=1` with
no feedback clock, no stock type-100 activation and therefore no QSA consumer,
while iOS publishes playback progress without a changed media item.  Opening
Music repairs both the media publication and the stock audio-context/QSA
bootstrap.  Because one foreground action can independently cause both edges,
the trace does not prove which edge is causal.

### Implemented cold-connect recovery and deployed A/B

The cold path now has three bounded layers:

1. when type 103 is active and stock, detached-QSA and cached clocks are all
   absent, `updateFeedback` publishes the endpoint's negotiated nominal
   realtime clock `{type=100, sampleRate=48000}`;
2. after the first real type-103 media frame decodes, the hook queues one exact
   Pioneer `0x5000` Now Playing request through the retained `ICinemoIAP`
   object. Cinemo still owns link sequence and ACK state. If the semantic
   injection context is not ready, the existing APAP receiver thread retries
   at one-second intervals, at most eight times, and stops after the first
   successful queue;
3. after the buffered PCM ring reaches its prime threshold, that same existing
   receiver thread opens and configures `mpl5_dio_ent` directly as PCM6/S16LE/
   48 kHz with 768-frame periods. No thread is added inside `io-audio`. A later
   stock type-100 open atomically reclaims the same handle under `ioLock`, so
   there is never a second ring consumer.

The `0x5000` builder reproduces `NmeIAP2_StartNowPlayingUpdates @ 0xD907C`:
144 payload bytes, media attributes
`0,1,4,6,7,8,9,10,12,16,18,21,22,23,24,26,27`, playback attributes `0..16`,
and FNV-1a payload hash `0ee9e0ae`, identical to the captured stock request.
The shared rollback for layers 2 and 3 is `AUDIO_COLD_CONNECT=0`; the feedback
negative control remains `AUDIO_FEEDBACK_BLANK=1`.

The compact trace now distinguishes `IAP_NOW_PLAYING_RETRY`, a successful
`QSA_OPEN action=cold_buffered`, `QSA_COLD_BOOTSTRAP` failure stages, the active
`BUFFERED_RENDER_START upmix=...`, and the packet/ring/cadence/device health
counters. It records no title, artist, artwork, cookie or audio payload.

All host tests, including the real-capture APAC byte-exact integration test,
pass. The QNX build passes the emutls, constructor, exception/RTTI/STL and
export guards. On 2026-09-05 the compact trace build and the requested HSSR
profile were installed on the unit and independently read back byte-identical:

```text
build/libcarplay_hook.health-trace.so
  SHA-256 b831166f920d42685716ea951867c05938c6ca3d1444ffb1fb70c1c8998afb1c
build/libcarplay_apac.so
  SHA-256 a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
build/hssr/hssr_model.bin
  SHA-256 fa46e2651197cf3790c9de5baaebe90675a5e943d75cf84d0ad06744a93e7abe
deploy/smartphone_integrator/carplay_startup.sh
  SHA-256 e291dd82fb3f720e32a3c7e139f9f46f3ccdf089bfb5ade2dfc0e9694314bf3a
```

The deployed process policy is `hssr-neon`, model
`/mnt/app/root/hooks/hssr_model.bin`, rear gain `0.25`, LFE `assist`, buffered
priority 18, and buffered format indices `[65,23]`. The trace was cleared before
the new generation. Installation is proved; the decisive test remains a cold
phone connection without manually opening Music.

## Vehicle A/B: QSA bootstrap alone is silent; stock audio-type routing is required

That decisive cold test was run on 2026-09-05. The complete before/after trace
is `captures/state-trace/20260905-125527/carplay_state_trace.log`; the cold-only
snapshot is `captures/state-trace/20260905-125323/carplay_state_trace.log`.

Before Music was opened, type 103 was not merely connected: APAC index 65 had
decoded continuously to six-channel PCM, the hook had opened and configured
`mpl5_dio_ent`, and its detached writer completed 10,861 full writes with zero
short writes, zero zero-byte writes, zero recovery calls, zero device underruns,
and no decoded zero-PCM records. Feedback carried a realtime 48-kHz clock. The
user nevertheless heard silence.

Opening Music caused this exact edge in the existing AirPlay session:

```text
02:23:19.881  SETUP request: type=100, format=32768
02:23:21.249  QSA_OPEN action=reclaim handle=0x747490 detached_writes=10861
02:23:21.280  first stock-scheduled write, buffered_frames=768
02:23:21.281  SETUP response: type=100, dataPort=7070
```

The stock path reused the same live QSA handle and its first write still read
the already-decoded type-103 ring. The user immediately heard audio. Therefore
the original cold bootstrap proved transport but did not activate the HU media
source: opening/writing QSA is necessary and not sufficient. A fresh `0x5000`,
nonempty `0x5001`, decoder output, feedback clock and direct QSA writes all
preceded the healing edge and cannot explain it.

Static analysis of the exact MU1316 binaries identifies the missing call:

- `libairplay.so` SHA-256
  `01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2`,
  `AudioStreamPrepare @ 0x4A9D0`: for stream type 100, instructions
  `0x4AA14..0x4AA28` load callback `stream+0x74`, pass audio type from
  `stream+0x70` and context from `stream+0x78`, and call it. Instructions
  `0x4AA2C..0x4AAEC` then invoke the audio-availability waiter at `stream+0x80`
  and require ACK result `1` before opening QSA.
- `dio_manager` SHA-256
  `e9862680d76ca2e6bb2eed067658f2ca8fa9b71b5446322384b76c9735c0e794`
  exports `dio::CDIOManager::s_audioTypeCallback(int, void*) @ 0x154E58`.
  The callback does not read its second parameter. It packages
  `CAirPlayServiceEvent_UpdateMainAudioType` and broadcasts it through the
  existing active-object mediator.
- `CDIOManager::onEvent_updateMainAudioType @ 0x1584E8` maps
  `kAudioTypeMedia` (`1`) to DSI main-audio type `2`, loads the existing
  `CDSICarplayImpl` at `CDIOManager+0x1E0`, and calls
  `CDSICarplayImpl::updateMainAudioType(type=2, add=true) @ 0x168AEC`.
  The latter dispatches virtual slot `+0x74` to the HMI/DSI peer. This is the
  media routing/focus declaration skipped by a hook-owned type-103 stream.
- The matching exported teardown callback is
  `dio::CDIOManager::s_audioTornDownCallback(void*) @ 0x154DBC`.

The cold path now resolves and calls the exact exported audio-type callback as
`s_audioTypeCallback(kAudioTypeMedia, NULL)` before its QSA open. It neither
constructs nor dereferences a Pioneer/DSI object. `AUDIO_ROUTE action=request`
records the queue operation. If stock type 100 later arrives, ownership is
handed to stock (`action=handoff`) because its normal prepare already emitted
the same declaration; otherwise type-103 teardown invokes the exact exported
audio-torn-down callback (`action=release`). Failure to resolve either symbol
is explicit. `AUDIO_COLD_CONNECT=0` remains the rollback for the whole bounded
cold path.

This repair was ABI/static-verified and host-tested before the vehicle pass
recorded below. It supersedes the sentence above that called the direct-QSA
deployment itself the final test: that test was performed and falsified
QSA-only activation.

## Vehicle log check: occasional frame-scale glitches

While index-23 AAC stereo was playing successfully, the user reported rare
frame-scale noises resembling an underrun on some songs.  The uninterrupted
compact capture through `00:53:05` is
`captures/state-trace/20260905-112450/carplay_state_trace.log`.

This capture is negative for every failure the deployed trace can currently
observe:

- no `QSA_WRITE_ANOMALY`, zero/short write, prepare/recovery, additional flush
  or close occurred after the continuous stock-to-detached handoff;
- no decoder reject, decoder rebuild, format switch or buffered render stop
  occurred; the session remained AAC-LC index 23, 48 kHz stereo into PCM6;
- every post-start `updateFeedback` carried a realtime clock.  Its largest
  observed call-to-call interval was 2089 ms against the normal approximately
  2-second polling cadence;
- system `sloginfo` contained no underrun/xrun/error for `mpl5_dio_ent`.
  `deva_timer_play_mpl5_dio_ent` adjusted its 16-ms period gradually in 400-ns
  steps, and did not show a reset;
- the heartbeat reported about 5% `dio_manager` CPU, with the priority-18 APAP
  receiver/QSA writer using about 2.3--2.7%.  This is process-local load only;
  the first log check did not yet correlate it with the heartbeat's whole-unit
  CPU figure.

Therefore this log does **not** establish a QSA underrun.  It also cannot rule
out a loss before QSA: the deployed compact build records initial decoder
geometry and write failures, but does not periodically export the existing
`authFailed`, `tlvFailed`, `decodeFailed`, ring-fill/minimum-fill,
`shortRingFallbacks` or sampled QSA device-underrun counters.  A subsequent
diagnostic build must expose those counters in bounded `BUFFERED_HEALTH` and
`QSA_HEALTH` snapshots before changing buffering, priority or DSP policy.

### Timestamped recurrence at a track boundary

The user reported another audible frame-scale event immediately before the
`20260905-112612` pull.  The nearest semantic edge is a changed media item at
`00:54:02.433` (92-byte `0x5001`, six media attributes).  There is still no
`FLUSHBUFFERED`, `SETRATE`, new cookie, decoder rebuild, format switch, QSA
flush/close or `QSA_WRITE_ANOMALY` at that edge.

This recurrence also supplies the whole-unit load omitted above: the heartbeat
reported 99% CPU at `00:53:58` and 98% at `00:54:03`.  Load stayed in the
91--99% range throughout the retained `00:52:53`--`00:54:58` window, so this is
not proved to be a unique spike at the click.  It does, however, keep scheduler
starvation viable.  The known QSA/libasound behavior can recover an underrun
internally and still return a full plugin write, so the absence of a short or
zero write does not rule that case out.

The bridge never intentionally sends a mini fragment: device writes are whole
768-frame/9216-byte PCM6 periods.  A producer shortage occurs one boundary
earlier.  `spa_pcm_sink_drain_detached()` does not submit anything while the
decoded ring contains fewer than 768 frames; if the already queued eight QSA
periods drain before more AAC produces PCM, the device can underrun without a
short write.  Cinemo may also return zero or fail for an access unit.  The
current trace cannot distinguish these cases from an internally hidden QSA
underrun, which is why ring/decode/device counter snapshots are required.

## Vehicle acceptance: cold type 103 now activates audio without Music

The fresh cold-connect test passed on 2026-09-05. The user connected the phone
and did **not** open Apple Music manually; playback started automatically. The
complete compact trace and analyzer output are in
`captures/state-trace/20260905-131355/`.

This was the intended path, not an accidental recovery through stock type 100:

```text
audio_stream_types: [103, 110, 111]       # no type 100 in the generation
02:41:30.228 AUDIO_ROUTE action=request owner=cold_buffered audio_type=1
02:41:30.231 QSA_OPEN action=cold_buffered channels=6 rate=48000
02:41:30.232 first complete detached periods: result=9216 expected=9216
```

Across the retained interval the detached writer reached 2,560 periods with
zero short/zero writes, zero recovery calls, zero device underruns and zero
ring waits. APAC index 65 remained six-channel; authentication, TLV parsing,
decode, zero-PCM, sequence-gap, drop and upmix-failure counters remained zero.
One 248-ms APAP arrival gap occurred during startup before the steady cadence;
it did not produce a ring wait, QSA cadence gap or device underrun. The seven
initial `QSA_WRITE_ANOMALY` records are full 9216/9216 writes carrying
`clock_reset=1`; they are startup clock-estimator resets, not partial writes or
underruns.

The deployed/read-back hook is 553,898 bytes with SHA-256
`7e8869875293f02952669186f7c4f183986003c4f42e7f5622ecc3db31a31754`.
The unchanged APAC codec and HSSR model retain the hashes recorded above. This
closes the cold-connect defect: the missing operation was the Pioneer Media
route/focus declaration, and calling the exact stock callback before cold QSA
bootstrap is now vehicle-confirmed.

## Vehicle A/B: stock-to-buffered route ownership was one-way

A later cold connection exposed the inverse ownership edge. The failed trace
is `captures/state-trace/20260905-media-start-failed-live/`; the successful
unplug/replug control is
`captures/state-trace/20260905-media-start-replug-success/`.

The failed generation (`pid=238407815`, generation 1) was not an iAP, metadata,
decoder or QSA-start failure:

```text
03:45:51.139  Identify accepted; nonempty 0x5001 followed immediately
03:45:53.660  type-100 SETUP request
03:45:55.812  stock QSA open (the SETUP spent 2247 ms in stock code)
03:45:55.912  first full stock QSA write
03:45:56.353  AAC-LC index 23 ready, stereo -> PCM6
03:46:10.423  stock QSA close retained for active type 103
03:46:10.441  first full detached write
```

After the retained close, type 103 continued through 17,061 full detached QSA
writes until physical disconnect, with no decoder failures, short/zero writes,
recoveries or device-underrun increments. However, there was no later
`AUDIO_ROUTE request`. This matters because stock `AudioStreamStop @ 0x4A2F8`
waits for its realtime worker and then calls the callback stored at
`stream+0x90`/`stream+0x94`; that callback is the matching audio-torn-down
notification. Retaining the worker's QSA handle does not retain the Pioneer
Media route. The old ownership machine handled `hook -> stock` on a later
type-100 open, but not `stock -> hook` when type 100 disappeared first.

The unplug/replug control created `pid=245895303`, generation 1 with stream
types `[103,110,111]` and no type 100. It took the already-confirmed cold path:

```text
03:53:34.910  AUDIO_ROUTE request owner=cold_buffered
03:53:34.916  QSA open owner=cold_buffered
03:53:34.917  first full detached write
```

The user confirmed that this generation was audible. Thus the A/B localizes
the failure to Media-route ownership, while the valid PCM continued below it.

The local repair adds the missing inverse transition. A retained stock close
schedules route reacquisition for 50 ms later, after stock's synchronous
close-adjacent torn-down callback can be queued. The existing APAP receiver/QSA
writer performs the request; no thread and no second QSA handle are added. A
stock reopen before the deadline cancels the request and owns the route. A
failed symbol/callback resolution retries after one second. Hook-owned
type-103 teardown still emits exactly one matching release.

`tools/analyze_carplay_state_trace.py` now reports this state explicitly as
`route_reacquire_missing` when detached rendering follows a retained stock
close without either a hook route request or a stock reopen. The actual failed
trace is classified accordingly. Host tests and the QNX build pass. This
repair has **not yet been deployed or vehicle-accepted**.

## 2026-09-06 cold regression: queued decode stopped below its own prime

The first vehicle run after the exact stopped-queue lifecycle deployment
reproduced cold silence while the iPhone UI still reported advancing playback.
The ordinary diagnostic build retained enough evidence to separate that event
from a MediaRemote/autoplay failure:

- type-103 SETUP selected AAC-LC index 23 for `com.apple.Music`;
- SETRATE selected rate one and `updateFeedback` continuously returned the
  nominal 48-kHz descriptor;
- the APAP capture contains 33 complete authenticated records, sequence
  `9482929..9482961`, with media time advancing by exactly 1024 at timescale
  48000; every record has audio and none carries trim fields 4/5;
- no cold Media-route request or QSA open followed.

The defect was local and deterministic. Packets accumulated while rate was
zero. After rate one, `spa103_render_drain_one_queued()` stopped decoding when
the PCM ring reached the steady-state high-water mark, 250 ms / 12,000 frames.
Cold QSA bootstrap, however, exposes pending PCM only after the initial prime,
400 ms / 19,200 frames. Thus the new FIFO path enforced `high < prime` and
made its own bootstrap condition unreachable. The earlier direct decode path
did not have this unconditional queued-drain guard, which explains why the
same 250/400-ms configuration had previously passed on the vehicle.

The local repair makes the pre-prime FIFO decode limit
`max(high_water, prime_target)`, then restores ordinary high-water behavior
after priming. It also retries cold bootstrap while a primed/high-water queue
is waiting, so a transient first open failure no longer requires another
decoded packet to progress. A host regression uses the production 1024-frame
codec, 12,000-frame high-water and 19,200-frame prime values and proves that
queued decode reaches 19,456 frames before stopping. Vehicle acceptance of
this repair is pending.
