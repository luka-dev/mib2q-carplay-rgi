# Hook release-safety audit

Date: 2026-09-07

Scope: the `libcarplay_hook.so` release review after the buffered AAC/APAC
lifecycle and HSSR work. This note records code invariants; it is not an
on-vehicle validation report.

## Closed findings

1. Cover-art input is bounded before stb allocates or decodes it. JPEG EXIF
   orientation is parsed with checked segment/TIFF offsets, and images above
   4096 pixels on either axis or 8 million pixels total are rejected. The parser
   has a deterministic malformed-input corpus plus 50,000 ASan/UBSan fuzz cases.

2. Buffered PCM ownership now crosses QSA transactionally. `peek` reserves one
   whole period without moving the ring head; `commit` advances only by the
   complete frames QSA reports accepted. Zero/error retains the whole period and
   a short write retains the exact tail. Flush/stop waits for an outstanding
   reservation before rewriting or freeing the ring.

3. AltScreen socket sends use flags zero because this QNX io-pkt rejects
   `MSG_NOSIGNAL`. Timeouts used by the renderer link use `CLOCK_MONOTONIC`.

4. AltScreen SETUP no longer waits up to 1.8 seconds to reap an earlier
   receiver. A new SETUP retires the old session-address generations (stop plus
   listener close), allocates a different slot, and publishes an exact global
   AV generation only after the response descriptor is complete. Only a thread
   which has already published `threadExited` is joined by GC. Command lookup is
   `(session pointer, AV generation)`, so a retired receiver cannot target a
   newly reused session address. If all four slots are still retiring, SETUP
   fails immediately instead of blocking the RTSP transaction.

5. Bus signal policy is reversible. `sigaction` saves dio_manager's previous
   SIGPIPE and fatal-signal dispositions before installing the hook policy.
   Fatal diagnostics use only async-signal-safe operations, restore the exact
   predecessor and re-raise into it. Clean shutdown and partial-init failure
   restore all dispositions. A host regression checks custom-handler chaining
   and clean restoration.

6. The main hook is compiled with `-fvisibility=hidden` and linked through
   `hook/carplay_hook.exports.map`. The release build asserts the exact dynamic
   symbol set: 24 intentional LD_PRELOAD seams in a quiet build, or 26 when the
   two trace-only HID seams are enabled. The former 208-symbol surface (module
   definitions, framework/audio helpers and stb functions) is no longer
   preemptible.

7. A successful QSA timeline flush must be paired with a successful prepare.
   Missing/failed prepare sets a persistent prepare-needed flag. Both detached
   and stock writers execute that prepare before peeking/consuming buffered PCM;
   a reclaimed handle preserves the flag, and the normal prepare interposer
   clears it only after a successful real prepare. A failed recovery leaves PCM
   queued and reports failure.

8. Cinemo AAC callbacks can no longer silently truncate PCM beyond the 4096
   frame FIFO or the caller's output capacity. Callback framing and capacity are
   checked atomically. Overflow returns `SPA_APAC_ERR_OUTPUT_SPACE`, emits no
   partial decoded result, and flushes the codec to an explicit recovery
   boundary.

## Verification

- `scripts/run_tests.sh`: pass.
- `scripts/test_hook_audio.sh`: pass, including APAP framing/decrypt, AAC/APAC
  switching, trim and accepted APAC reference decode.
- ASan/UBSan: cover-art safety, PCM transaction/switch, AAC FIFO and signal
  guard tests pass.
- QNX ARMv7 default toolchain: quiet build passes with exactly 24 exports;
  state-trace build passes with exactly 26 exports.
- QNX ARMv7 GCC 8.5 toolchain: quiet build passes with exactly 24 exports.
- Link guards remain clean: no emutls, exception/RTTI/STL runtime,
  `libstdc++`, APAC hard dependency, or eager module constructor.

## Runtime boundary

These changes still require a normal on-unit reconnect/track-change/seek pass.
No binary was deployed as part of this audit, and none of the fixes changes the
negotiated PCM6/APAC/AAC formats or HSSR coefficients.
