# carplay_trace

On-unit capture hook → emulatable trace → run **from-source `dio_manager`** on QEMU with no phone.

Standalone LD_PRELOAD `.so`. Run it on the **real unit** against **stock libs** + a real
iPhone; it records what `dio_manager` sees during a CarPlay session into one jsonl trace.
That trace replays into our from-source `dio_manager` on QEMU.

## Why this boundary (not a phone-socket replay)

CarPlay media is MFi-encrypted with **ephemeral, non-deterministic** session keys (ECDH
per connect; MFi is out of scope). Recorded wire bytes are **ciphertext** — they won't
decrypt under from-source's fresh keys, so replaying the phone socket dies at pair-verify.

The **replayable** boundary is **libairplay → dio, post-decrypt**: stock libairplay decrypts
on the unit and calls dio's delegate callbacks. Record that callback stream, replay it *into*
from-source dio → dio runs its full CarPlay path, crypto + phone bypassed.

Two layers land in the trace:
- **L2 `cb.*`** — the libairplay→dio delegate callbacks (`sessionCreated`, `sessionScreenCreated`,
  `audioType`, `copyProperty`, teardown, …). **This is the replay driver's input.**
- **L1 wire** — raw phone sockets (`recv/send/read/write/...`), full hex. **Reference only**
  (ciphertext), for protocol structure/timing.

### `cb.copyProperty` — value fidelity (`valType` / `valData`)

`copyProperty` is dio asking libairplay for a property; dio returns a **value** (`CFTypeRef`):
`model="Audi"` (CFString), `oemIcon` image (CFData), feature/flag values (CFNumber/CFBoolean/
CFArray/CFDictionary), or NULL when it can't answer (`err=-6714`). The hook now serializes that
value so a future replay can make **from-source dio** answer these properties **faithfully**
instead of guessing. The value is captured **after** the real callback returns, so it stays
passive on the live path (≈21×/session, not per-frame).

Serialization uses libairplay's own exported CFLite (dlsym'd from `libairplay.so`, VAs from the
staged unstripped copy):

| `valType` | how | fields |
|---|---|---|
| `bplist` | `CFPropertyListCreateData(NULL, r, 200, 0, NULL)` @`0x7ab48` — binary property-list v1.0 (RE: the func branches on `cmp r2,#200`, binary-only). Covers CFString/CFData/CFNumber/CFBoolean/CFArray/CFDictionary uniformly. | `valData` = the binary-plist bytes as **hex** (replay: `CFPropertyListCreateWithData` @`0x7ace0` reconstructs the exact `CFTypeRef`), `valLen` = full byte length, `valTrunc` = 1 if capped. |
| `null` | `r == NULL` (the `err=-6714` path) | — |
| `string`/`data`/`number`/`boolean`/`array`/`dictionary`/`other` | fallback for a non-plist CFType (`CFPropertyListCreateData` → NULL): `CFGetTypeID(r)` @`0x77fb0` vs the exported `CF*GetTypeID` getters | `valTypeId` = numeric `CFTypeID`. No content (this libairplay exports **no** `CFCopyDescription`); type identity is the honest record. |

These live inside the event's `"ret"` object, e.g.
`"ret":{"value":"o#5","err":0,"valType":"bplist","valLen":34,"valTrunc":0,"valData":"62706c..."}`.

Blob cap: `PROBE_IO_CAP` if set, else a sane **256 KB** default (a full `oemIcon` stays
full-fidelity; a pathological value can't bloat RAM). The `CFData` created for serialization is
`CFRelease`'d — no leak; `r`'s own refcount is untouched (returned to dio unchanged).

## Build

```sh
sh build.sh              # → libcarplay_trace.so (QNX ARMv7, needs only libc.so.3)
```

## Capture (on the UNIT, stock libs, real phone) — do this once

```sh
LD_PRELOAD=/path/libcarplay_trace.so dio_manager
# then: plug in iPhone, start CarPlay, use it ~30s, disconnect.
```
No `PROBE_LOG` → writes a fresh per-run file **`/tmp/carplay_<pid>.jsonl`** (two runs never
collide) and prints the chosen path to stderr. Set `PROBE_LOG=/path` to force a specific
file (appended). Pull the `.jsonl` off the unit — replays in QEMU as many times as you want.

Env knobs:
- `PROBE_WIRE=1` — enable L1 wire (socket) capture. **OFF by default**: it's the highest-rate
  capture, on CarPlay's hot real-time network threads, and full-hex-under-lock there starves
  the live session (black screen / early teardown). The wire is reference-only ciphertext the
  replay driver never uses, so leave it off for a real capture; turn it on only to study the
  wire on a throwaway session.
- `PROBE_MAX=N` — **total** bytes budget for the BULK writers (media frames + wire).
  Default **64 MB**. `/tmp` on the unit is RAM (tmpfs) and full decoded video is big, so
  this stops the capture bloating memory and wedging the unit — the tiny lifecycle events
  keep recording after the budget is spent, so the session skeleton is never lost.
  Set `PROBE_MAX=0` for unlimited (only when `PROBE_LOG` is on real disk / USB).
- `PROBE_IO_CAP=N` — cap payload bytes/record (default 0 = full). Set only to peek at
  structure; a capped video frame is not replayable.

`/tmp` note: fine for a first/control run under the 64 MB budget. For a long full-media
dump, point `PROBE_LOG` at a USB/persistent mount and raise/clear `PROBE_MAX`.

## Replay (QEMU) — the loop this feeds

The QEMU mesh harness (`Tools/qnx-carplay-emu/carplay/`) already boots from-source dio to
FULL IDLE. The replay driver reads `carplay.jsonl` and, in `seq` order, **calls from-source
dio's registered `cb.*` callbacks** (from its `AirPlayReceiverServerSetDelegate`) with the
recorded args → dio walks its CarPlay session path without a phone.

## Status / what's missing for full video replay

- ✅ L1 wire capture, L2 lifecycle/control callbacks (slots reversed) — built, compiles.
- ✅ **`cb.copyProperty` value fidelity** — the returned `CFTypeRef` is serialized to a binary
  property-list (`valData` hex + `valType`); see the table above. Replay-deposit side (making
  from-source dio return these values) is the next step, after a re-capture.
- ✅ **L2-media**: decoded **screen H.264 frames** captured (post-decrypt) as `cb.screenFrame`.
  See the RE map below.
- ❌ **audio interpose REMOVED** — the `Read/WriteAudio` interposers used a GUESSED 8-arg
  signature on the live audio path and broke the session (-6722 right after audio negotiation);
  they captured 0 frames anyway (dio never consumes audio → PCM passthrough to ALSA). Recover
  the REAL signature (idalib) before ever re-adding.
- ⚠️ **replay driver**: build against the first real trace (the exact callback fields to
  reconstruct become concrete once you see a capture — build it blind = guesswork).

### L2-media RE map (stock MHI2Q `libairplay.so`, unstripped VM copy — VAs cited)

The task's "session screen/audio **delegate frame slot**" model does **not** hold in this
build. Verified in idalib:
- `AirPlayReceiverSessionScreenSetDelegate` **@0x248dc is a dead stub** — spills its two args
  to stack locals and returns; the delegate is never stored. **No screen-frame slot exists.**
- `AirPlayReceiverSessionSetDelegate` **@0x241358** copies a **12-word (0x30) delegate to
  `session+0x18`**, but that is the **control** delegate (modes/UI/Siri/nightmode) — no
  audio/video **frame** slot. Audio frames route through libairplay's own POSIX bridge
  (`_AudioInput/OutputCallBack`) into the exported Read/WriteAudio, never a dio delegate
  (matches "210 audio = PCM passthrough → ALSA").

So the real **post-decrypt decoded-frame boundaries are three EXPORTED functions**, interposed
directly (fully passive — call real, then log; no struct-layout dependency):

| trace `fn` | interposed export (VA) | signature (arg → field) | payload |
|---|---|---|---|
| `cb.screenFrame` | `ScreenStreamProcessData` @0x257d8 | `(priv, uint8_t* data, size_t len, uint64_t pts, CFLDictionary* opts, void(*done)(void*), void* ctx)` → `Forward(priv,70,data,len)`; sig = embedded `__PRETTY_FUNCTION__` | `data` = decrypted **H.264 AVCC** (4-byte length-prefixed NALs). Fields: `nal`=first NAL type (`data[4]&0x1f`: 5=IDR,1=P,7=SPS,8=PPS), `pts`=64-bit stream ts, `len`, `data`(hex). |
| `cb.audioFrame` (rx) | `AirPlayReceiverSessionReadAudio` @0x2442e8 | `(session, int type, a3, a4, a5, a6, uint8_t* buf, size_t len)`; `buf`=arg7 **filled by the call**, `len`=arg8, `type`=arg2 | phone→HU **decoded PCM**. Fields: `dir:"rx"`, `streamType` (`'d'/'e'/'f'`=100/101/102), `len`, `data`(hex). PCM format is fixed (no in-lib AAC decode). |
| `cb.audioFrame` (tx) | `AirPlayReceiverSessionWriteAudio` @0x244cc8 | same shape; `src`=arg7 (input), `len`=arg8 | HU mic→phone PCM. Fields: `dir:"tx"`, `streamType`, `len`, `data`(hex). |

Frame `data` is full-length, `PROBE_IO_CAP`-capped (default 0 = full). The replay driver
reconstructs screen frames by feeding `data`+`pts` into from-source dio's screen pipeline in
`seq` order; audio is PCM-passthrough so `cb.audioFrame` is a reference stream (dio does not
consume audio frames on the real unit).

Lineage: interposer bodies lifted from `from-source/tools/oracle/probe/probe.c` (same proven
pattern); this is the standalone on-unit cut.
