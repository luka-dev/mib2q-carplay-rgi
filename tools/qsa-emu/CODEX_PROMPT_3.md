# Task: measure what is pulled OUT of the ring, not what sits in it

Read `tools/qsa-emu/HANDOFF.md` and your own audit in `tools/qsa-emu/README.md`
first. This continues directly from the CSD descriptor pass, whose negative
result is accepted: rate, voices, bit width, interleave and buffer geometry are
all derived from the live negotiated format, and there is no stale stereo field.

## The gap in the existing validation

`DSP_VERIFY ... voices=6 fragments=62 bytes=1142784 mismatches=0` proves the
*contents* of the ring are byte-exact. It does not prove anything about the
*order or multiplicity* in which those contents are consumed.

A cursor that submits the same region twice, or advances by half a fragment,
leaves every byte in the ring perfectly correct while the DSP receives each
fragment twice. The existing check cannot see that class of fault by construction.
That is the remaining ×2 mechanism and it is unmeasured.

## Why ×2 specifically

You named the two candidate values yourself. They are not arbitrary:

    0x2400 = 9216  bytes = 768 frames x 6 voices x 2 bytes   <- the S16 client fragment we write
    0x4800 = 18432 bytes = 768 frames x 6 voices x 4 bytes   <- the 24-in-32 CSD buffer after widening

Both sizes are live in this path at the same time, they differ by exactly two,
and QSA's S16->S32 widening sits between them. Any place that reaches for the
client size where it needs the container size, or the reverse, produces a factor
of two. The ear evidence from the vehicle independently landed on ×2: a fragment
repeated twice matched the tick closely, while ×3 and ×4 were rejected as too slow.

## What to measure

The boundary you identified but did not execute:

* `session_csd_stream_playback` at 0x69084
* `csd_write` at 0x69318
* playback cursor advance via `dma+0x94` at 0x6e864-0x6e874, inside
  `session_deva_timer_thread` at 0x6E85C

1. In `csd_stub.c`, record every `csd_write`: the source address, the length, and
   the ring-relative offset it corresponds to. Also record the cursor value at
   each timer advance and the delta between consecutive advances.
2. Over a 62-fragment run, answer three questions with numbers:
   - Is the per-advance delta 0x4800, 0x2400, or something else?
   - Is each distinct ring region submitted exactly once, or more than once?
   - Does the total submitted byte count equal the total written byte count?
3. Reconstruct the stream **in submission order** -- concatenate what csd_write
   actually hands over, in the sequence it hands it over -- and render it to WAV
   through the carve pipeline you already built. Compare it against the known-good
   reference `captures/broken_5_1_20260901_drive/session6_5_1_declared_order.wav`.

Item 3 is the deliverable. Contents-at-rest already passed; a rendering of the
submitted sequence is the artifact that would expose a repeat, and it can be
listened to.

## Report

Give the three numbers from item 2 and the path to the rendered WAV. State plainly
whether the submitted sequence is a faithful single pass over the written data or
whether any region repeats. If it is faithful, say so -- that closes the entire
ring-and-submission path and moves the fault downstream of csd_write, which is a
useful result. Do not manufacture a fault.

Measurement only. Do not modify the hook or the deva binary patch.
