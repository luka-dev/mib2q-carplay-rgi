# APAC on the car: what to check, and what each failure looks like

The decoder is finished and proven on the bench. Nothing in this document has been done on the
vehicle yet, because deployment is the owner's call and the head unit is never restarted from here.

What "proven on the bench" means, precisely, so the on-car result can be judged against it:

* the port decodes the live capture and differs from Apple's own decoder on **1 S16 LSB at 0.005 %
  of 24 434 688 samples**, entirely because of an FFT difference that cannot be removed — the
  library wraps Apple's closed `vDSP_DFT_Execute`, which does not exist on the head unit;
* with the port's own transform substituted into the original decoder, the difference is **exactly
  zero** on every sample, so everything above the FFT is bit-identical;
* `scripts/run_tests.sh` drives the hook's own chain — framer, decrypt, TLV split, decode,
  channel permutation — over the whole capture: 3980 frames authenticate, the 3977 APAC packets
  extracted are byte-identical to the reference, and the permutation is checked on real audio;
* `scripts/build_hook.sh` links the decoder into the shipping `.so` with
  `emutls=0 exceptions=0 rtti=0 stl=0 libstdc++=0`.

## Before deploying

Deployment replaces `/mnt/app/root/hooks/libcarplay_hook.so` and restarts `dio_manager`. That is
`scripts/deploy_unit.sh`, and it is the owner's to run. Java is not touched, so no full reboot is
involved — but a `dio_manager` restart still drops any active CarPlay session.

The profile is set by `scripts/set_spatial_profile.sh`, which edits only the four `SPATIAL_*`
exports in the startup file. `spatial-5.1` is the profile this work targets: type 103 on, format
index 65, and the type-100 PCM6 fallback left enabled underneath. `fallback-5.1` turns type 103 off
entirely and is the way back if anything below goes wrong. A profile change applies to the next
`dio_manager` generation; no reboot.

## The first question: does it decode at all

Grep the slog for `buffer103: APAC decoder up`. On a healthy start it carries the geometry the
cookie declared:

```
buffer103: APAC decoder up id=<cookie> 48000 Hz 6 ch 1024 frames/packet
```

If that line never appears, the render state says why, and each state is a different problem:

| state | meaning | where to look |
| --- | --- | --- |
| `IDLE` | type 103 never became active | the profile, and whether iOS selected index 65 at all |
| `WAITING` | key in hand, no magic cookie yet | `SETMAGICCOOKIE` handling; the stream may have torn down first |
| `NO_KEY` | SETUP carried no usable `shk` | the SETUP descriptor path, not the decoder |
| `UNSUPPORTED_FORMAT` | an index this rail cannot carry | expected for anything but 65 today |
| `UNSUPPORTED_LAYOUT` | index 66 — decodable, but its channel order is unmeasured | deliberate refusal, see below |
| `COOKIE_FAILED` | the cookie would not configure a decoder | compare the cookie against the bench one |
| `NO_MEMORY` | allocation failed inside `dio_manager` | size, not correctness |

`UNSUPPORTED_LAYOUT` is a deliberate refusal rather than a defect. Index 66 is eight-channel 5.1.2
and the decoder handles it, but its **native channel order has never been measured** — only the
six-channel case was, three independent ways. Guessing an eight-channel permutation would put the
LFE on a surround with no way to notice by ear. Measuring it needs an index-66 capture treated the
way the index-65 one was.

## The second question: does it sound right

Correct decoding and correct routing are different failures and they present differently.

* **Wrong channel order** — dialogue from a surround, bass from the wrong speaker, or a channel
  that should be low-passed carrying full-range content. The permutation is
  `spa_reorder_5_1_decoder_to_declared_s16`, and it is `C L R Ls Rs LFE` in, `L R C LFE Ls Rs` out.
  That the decoder emits the first is established three ways: measured on 74.6 s of audio, a
  locally encoded round trip, and the stream's own layout tag `0x007c0006`, which CoreAudio
  resolves as exactly that. Where it disagreed with Pioneer's format-index table, the cookie won.
* **Dropouts or judder** — a real-time problem, not a decode problem. Decoding replaced the
  receiver's artificial one-packet-per-frame-period sleep with real consumption, so backpressure
  now comes from the sink. If the ring underruns, the decoder is not keeping up. See the timing
  note below.
* **Silence with the decoder up** — the ring is filling but nothing drains it, i.e. the DIO5 side,
  not the codec.

## What the bench could not answer

1. **Real-time headroom on the actual silicon.** The stream is 46.9 packets per second of
   six-channel 48 kHz, and the decoder runs inside `dio_manager` alongside everything else. Any
   figure produced under emulation is a bound with assumptions attached, not a measurement of a
   Krait. The honest test is the car.
2. **Behaviour across a pause and resume.** An Audio Synchronization Packet is the only legal entry
   point into a running stream, so it is also the only legal resume point after a `FLUSHBUFFERED`.
   The whole captured stream is ASPs, which is why this matters and why it was ported rather than
   treated as a formality — but a real pause/resume cycle has never been driven.
3. **A long session.** The capture is 84.8 seconds. Nothing has run for an hour.
4. **Anything the content never did.** BWE synthesis is not ported and refuses; the LRVQ frame mode
   is ported and wired but the capture never enters it; a non-joint channel pair, a non-empty
   channel remap and more than one codec config are all implemented and none is exercised. A
   stream that uses any of them is new territory.

## If it goes wrong

`scripts/set_spatial_profile.sh fallback-5.1` disables type 103 and leaves the type-100 PCM6 path
exactly as it is today. That path was never modified by this work and is the reason it is safe to
try: the failure mode is the audio the car already has, not no audio.

The deploy helper keeps one `libcarplay_hook.so.bak` on the unit.
