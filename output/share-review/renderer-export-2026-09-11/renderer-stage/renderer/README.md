# Renderer source subset

This directory contains substantial excerpts adapted from the actual receiver
and OpenMAX decoder, not a fictional renderer API demonstration.

Included: framed TCP reception; bounded worker queue; per-connection begin/end;
stale queued-frame rejection; sink lifecycle; configuration caching; component
state transitions; input-buffer recycling; output callbacks; bounded waits and
callback draining; a real host counter/capture backend.

## Host receiver

`make -C renderer` builds `renderer/video_receiver` from the host backend.
Run it with one positional TCP port argument; it binds loopback only. There is
no production endpoint built into this subset. No file is written by default.
Set `VIDEO_CAPTURE_FILE` to a path to create/truncate a raw elementary-stream
capture. Use one session per capture when you need one independently playable
stream. Capture counts accepted payload bytes; it does not claim decoded output.

Send records as `[u32 big-endian size][payload]`. Each nonempty payload must be
plain Annex-B data, with separate SPS/PPS configuration before complete video
access units. A zero-length record is ignored. This is the local receiver's
record format, not a phone-facing protocol. The caller supplies the input.

The queue is bounded; overload ends that connection. The next connection starts
a fresh sink session. Shutdown discards pending work, so wait for the session-end
log before terminating if you need all already-submitted capture bytes flushed.

## Decoder and output boundary

`sink_omx.c` is the adapted real OpenMAX core. It is not part of the host capture
build. It requires your platform's OpenMAX headers/libraries and implementations
of the functions declared in `output_backend.h`. No fake implementations are
supplied: the interface is the explicit boundary where your integration begins.

That backend selects/configures the component, owns suitable output buffers,
exposes their pointers/capacity, consumes decoded output and releases resources.
Component-specific memory layout, extension setup and display binding are not
provided. No graphics presentation implementation, layer registration, loader
assets, display control channel, SDK headers or deployment scripts are included.

The adapted core expects output buffers configurable before the initial state
transition. Dynamic output reconfiguration is reported as an error; full platform
recovery was omitted. Runtime errors require a fresh sink session. Input timestamp
policy remains the excerpt's zero timestamps; a timed-output adapter needs its
own integration. Mixed configuration+picture records are not supported.

The worker queue and core are useful source starting points, not a claim that
arbitrary OpenMAX components can be driven without adaptation. Source-level
adaptation and host/mock checks are distinguished from hardware validation in
the top-level README. The output backend has not been supplied or tested here.
