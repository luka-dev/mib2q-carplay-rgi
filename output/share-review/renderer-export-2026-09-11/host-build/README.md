# altScreen renderer source subset

Selected real receiver and decoder sources, extracted and adapted for sharing.
The host receiver builds and captures incoming data. The OpenMAX core is included
as source; display integration is an explicit, unimplemented backend boundary.

| Area | Included behavior |
|---|---|
| `renderer/main.c` | Framed TCP receiver, bounded worker queue, generation changes and session cleanup |
| `renderer/sink_omx.c` | Configuration cache, decoder states, input recycling, output callbacks and teardown |
| `renderer/output_backend.h` | Contract for component configuration, native buffers and decoded output |
| `renderer/sink_capture.c`, `sink.h`, `Makefile` | Working host receiver/capture build and sink interface |
| `altscreen/avcc.c`, `.h` | Length-prefixed H.264 NAL units to Annex-B conversion |
| `altscreen/frame_io.c`, `.h` | Caller-bounded records over a POSIX descriptor |
| `hook/interpose_example.c`, `.h` | Lazy original-symbol lookup, forwarding, result counters and errno preservation |
| `hook/process_scope.c`, `.h` | Exact process-name matching with an explicit unknown result |

Build with `make -C renderer`; run `./renderer/video_receiver PORT` with a port
you choose. It listens on loopback and logs session counters. Optional
`VIDEO_CAPTURE_FILE` creates/truncates a raw capture. The caller must provide
already-plain Annex-B records; the receiver does not discover a device or obtain
a stream by itself. See [renderer contracts](renderer/README.md) before use.

The platform output backend, graphics presentation, display registration,
phone-session setup, production hook entry points, control channel, SDK headers,
startup assets and deployment tooling are outside this source selection.
`sink_omx.c` is not linked into the host build. Integrating that core requires
your own implementation of `output_backend.h` and component-specific adaptation.

The receiver/decoder and video helpers are adapted local implementation excerpts.
The hook examples are small educational adaptations of public mechanisms, using
a fictional API. They are independent examples and are not wired into the
receiver. [Provenance](PUBLIC_SOURCE.md) distinguishes these two sources.

## Validation of this selection

Host C99 builds use strict compiler warnings. Address/undefined-behavior sanitizer
checks exercised fragmented input, empty records, reconnects, exact capture
bytes, oversized/truncated records and shutdown with an idle connected client.
A synthetic OpenMAX component exercised configuration isolation across two
sessions, input/output recycling, rejected and oversized input, output-change
error handling and resource-release order. This does not validate a codec,
real asynchronous hardware behavior or display output.

The helper checks and fictional interposer checks passed, including 256 calls
across four threads, original argument/result/errno preservation and absent
provider behavior. Test fixtures, SDK files and test binaries are not included.
`CONTENTS.sha256` covers every other file in this source-only selection.
