---
title: CarplayBus - hook <-> Java localhost bus
tags: [hook, bus, ipc, verified]
status: verified-source
sources:
  - code: hook/framework/bus_protocol.h
  - code: hook/framework/bus.c
  - code: java_patch/com/luka/carplay/bus/CarplayBus.java
---

# CarplayBus - hook <-> Java localhost bus

The link that carries parsed iAP2 state from the C hook to the Java HMI patch.

## Context

> [[iap2-interception]] -> module emits event -> **bus-protocol** - TCP :19810 -> Java module
> ([[rgd-activation]], [[cover-art]]).

## Topology

TCP `127.0.0.1:19810`. **Java is the long-lived server** (alive from HMI boot); the **hook is the
client**, (re)connecting once per CarPlay session. Idempotent reconnect on either side's restart.
No application heartbeat on this leg - Java relies on TCP FIN/RST + `setKeepAlive(true)`.

## Wire frame (16-byte header, big-endian)

| off | size | field |
|---:|---:|---|
| 0 | 4 | MAGIC `0x43504842` (`'CPHB'`) |
| 4 | 4 | seq (u32, per-side monotonic) |
| 8 | 2 | type (EVT_* / CMD_*) |
| 10 | 1 | flags (`BUS_FLAG_*`) |
| 11 | 1 | reserved |
| 12 | 4 | payload len |
| 16 | ... | payload |

- **BINARY** (`0x02`) - packed struct; otherwise payload is text `key:type:value` lines.
- **STICKY** (`0x01`) - server caches the **latest** frame per type and replays it to a new client on
  `CMD_SYNC_REQ`, bracketed by `EVT_SYNC_BEGIN`/`EVT_SYNC_END`; replays carry **REPLAY** (`0x04`) and a
  server-reassigned seq. Non-sticky frames are fire-and-forget (new clients see only new ones).

## Direction

- **EVT_*** hook->Java: `EVT_RGD_UPDATE` (0x0020), `EVT_COVERART` (0x0010), `EVT_HELLO`, sync markers.
- **CMD_*** Java->hook: `CMD_SYNC_REQ` (0x0100) requests a sticky snapshot.

## Threads (hook side)

`connector` (connect + retry, reconnect on any send/disconnect error) - `writer` (drains the outbound
queue) - a 1 Hz `timer` driving `rgd_periodic_tick` (deferred `route_state=0` flush - see
[[rgd-activation]]); the timer sends no application heartbeat.
