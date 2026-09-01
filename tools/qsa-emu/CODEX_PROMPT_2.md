# Task: find why six-voice CarPlay audio plays an octave low on the head unit

You already built the QEMU harness in `tools/qsa-emu/` and proved the PCM is
correct. Read `tools/qsa-emu/HANDOFF.md` first; everything there still holds.

## What the car actually sounds like

Two symptoms, reported by ear from the vehicle:

1. A continuous tick, "тцык тцык тцык", not once per second but constant.
2. Voices very low pitched -- but the tempo is only mildly off, not 3x or 4x slow.

Synthesised candidates were auditioned against that memory. Results:

* A fragment repeated x2 matched the tick closely ("невероятно похожи").
* x3 and x4 repeats were rejected: too much slowdown.
* Clipping, low-24-bit extraction, centre-into-subwoofer, and a stream/context
  channel-order permutation were all auditioned and rejected -- they sound normal.

So the target is a mechanism producing **about a factor of two**, affecting pitch
more audibly than tempo.

## What is already measured and must not be re-derived

* The APAC decode is correct. Decoded PCM was played to the user and confirmed good.
* The bytes reaching deva's DMA ring are byte-exact against that known-good PCM:
  `DSP_VERIFY ring=0 source=resmgr voices=6 fragments=62 bytes=1142784 mismatches=0`.
* The client fragment is now 9216 bytes = 768 frames x 6 voices x 2 bytes S16,
  16 ms at 48 kHz. The old 3x-slow active-fragment bug is fixed.
* `snd_pcm_plugin_write` masks a failing `snd_pcm_write_internal` as zero bytes.
  That is your finding, it is real, and hook-side accounting for it is committed
  at 37e6a53. It is a strong candidate for symptom 1.

Everything from the decoder down to the ring is therefore ruled out. The fault is
downstream of the ring: deva -> CSD -> ADSP -> MOST -> amplifier.

## The hypothesis to test

The ring holds correct S16 frames. But the DSP does not infer the format from the
bytes -- deva *declares* it when it opens the CSD session. If that declaration
still describes the stock stereo path, the DSP reinterprets our buffer.

Specifically: if deva declares 32-bit (or 24-in-32) samples while the ring
actually carries S16, the DSP consumes two of our samples as one of its own. That
yields exactly half the frames per fragment -- one octave down, and 2x slow. Both
reported symptoms, one cause, at the factor of two the ear already matched.

A declared sample rate that does not match 48 kHz would do the same thing.

## What to do

`deva_csd_start_csd_session_playback` at VA 0x24F28 is the function that opens the
session and hardcodes the channel maps (see `deploy/deva-ctrl-qc/README.md`). CSD
is *stubbed by you*, so unlike the real unit you can read every parameter deva
passes across that boundary.

1. In `csd_stub.c`, log the full payload of the session-open path: `csd_open`'s
   arguments and every `csd_ioctl` command and buffer, hex-dumped, before deva
   starts streaming. Note the existing warning in HANDOFF.md: `csd_open` argument 1
   is not a string, never dereference untyped parameters.
2. Capture that trace twice: once for a 2-voice open and once for a 6-voice open.
   `qsa_probe` already takes a device argument; drive both.
3. Diff the two descriptors. Look for any field that stays at its stereo value in
   the six-voice case: bits per sample, bytes per frame, sample rate, frames per
   period, buffer size, or an interleave/format enum.
4. If you find a stale field, confirm it statically in the driver -- which code
   path writes it, and whether our caps-table widening (max_voices 2->6,
   max_fragsize 0x1800->0x4800, max_buffer 0xF000->0x2D000) fails to reach it.

## Report

State whether a format or rate field is declared inconsistently with the ring
contents. If yes, give the exact field, the value in each case, the writing
instruction's address, and what the correct value would be. If no, say so plainly
and give the next-most-suspicious field with evidence -- do not invent a fault to
have something to report.

Do not modify the hook or the deva patch. This is a measurement.
