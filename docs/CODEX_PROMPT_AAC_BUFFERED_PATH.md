# Task: decode AAC-LC/48 kHz on the buffered stream so non-spatial tracks play

> **2026-09-03 measurement update:** this brief originally named index 22 and assumed
> per-track selection. The car subsequently selected index 23
> (`AAC-LC/48000/stereo`), and `[65,23]` selected 23 once for the whole session.
> The corrected static result and implementation plan are in
> [`AAC_BUFFERED_PATH_PLAN.md`](AAC_BUFFERED_PATH_PLAN.md). The text below is kept as
> the original task statement, with the now-known corrections folded in.

## The problem, stated exactly

With `AUDIO_SPATIAL=1` the hook advertises a buffered type-103 media stream and CarPlay plays
natively-Atmos tracks in discrete 5.1 -- that part works end to end, six channels at the
speakers, counters clean. Every other track is skipped.

The cause is settled, and the following candidate formats were tried on the car:

| index | what it is | outcome |
|--:|---|---|
| 65 | APAC/48000/5.1 | works; the only thing we can currently decode |
| 22 | AAC-LC/44100/stereo | selected and sent, APAP header carries 0xAC44; would need a resampler |
| 23 | AAC-LC/48000/stereo | **selected and sent at the sink's own rate** -- APAP header carries 0xBB80, and its magic cookie is a separate 39-byte blob with its own id. This is the route. |
| 21 | ALAC/48000/24/stereo | breaks the negotiation outright: the /info entry goes out and no stream setup follows at all, playback hangs |
| 64 | APAC/48000/stereo | **never selected**: iOS gates it behind `bufferedAPACStereo`, static default false |
| 11 | PCM | not offered: `APAudioFormatGetBufferedPassthroughSupportedFormats` returns zero unless the device is an APM speaker |

The phone's tested behavior is session-wide, not per-track: `[65,23]` selected 23 for all
content in that type-103 session, including Atmos material. There is still no observed fallback
to realtime type 100. Index 23 is therefore the production AAC route for ordinary content and
requires decoding, but merely advertising 65 and 23 together does not provide automatic
APAC-spatial/AAC-stereo switching.

## What has to be built

An AAC-LC decode path for the type-103 receiver, alongside the existing APAC one. **No
resampler is needed** -- see the index table below: advertising index 23 instead of 22 gets the
stream at 48000 Hz, which is exactly what the HU media endpoints want.

The pieces on the unit:

* `libNmeAudioAAC.so` -- Cinemo's AAC plugin;
* `libNmeSDK.so` -- exports `CinemoCreateAudioCodec`, and `dio_manager` already has it in
  `DT_NEEDED`, so no new link is required;
* `hook/audio/apac_bridge.h` -- the shape the existing decoder is wrapped in
  (`create`/`getInfo`/`decodeS16`/`reset`/`destroy`), which an AAC wrapper should mirror so the
  render path can dispatch on the negotiated index rather than growing special cases.

`docs/HANDOFF_SPATIAL_AUDIO.md` records prior static work on exactly this: the MH2p AirPlay 320
library's `AudioConverter` adapter and both AAC-LC and AAC-ELD decoder construction paths, with
the AAC-LC constructor passing a 48 kHz rate and eight-channel count into `CinemoCreateAudioCodec`
without a discovered channel clamp. Treat that as the starting reference, not as verified.

## What to determine, before any code

1. **The framing.** Inside APAP, is an index-23 payload raw AAC-LC access units, ADTS, or
   something else? The capture in `captures/spatial_silent_20260903/` is APAC, so a fresh
   index-23 capture is needed -- say what to capture and how to recognise the framing.
2. **The Cinemo contract.** Exact `CinemoCreateAudioCodec` arguments for AAC-LC, what it returns,
   how PCM comes out, and whether it can be driven from a hook thread inside `dio_manager`
   without disturbing the codec instances Cinemo already owns for local media. That last point is
   a real risk: the same library is serving the stock media path.
3. **Resampling.** Confirm that index 23 needs none. For the index-22 fallback only, determine
   where 44.1 -> 48 would happen, which existing implementation is reusable, and why it belongs
   before the ring rather than in the 48 kHz-only sink.
4. **Whether the magic cookie differs** for index 23 and what it carries -- the live session
   measured a distinct 39-byte cookie, but its bytes still need to be captured. The existing path
   builds its decoder from the cookie, and the render code now replaces a stored cookie when its
   bytes change, so an AAC cookie must not be mistaken for an APAC one.

## Constraints

Static analysis and a written plan; no code in this pass. The car is in a working state for
Atmos and that state is expensive -- say clearly which steps can be tested without risking it,
and which need a drive.

Do not replace the APAC path. Both decoder branches must coexist and dispatch on the negotiated
index: 65 remains the spatial/APAC branch and 23 is the 48 kHz AAC branch. This implementation
requirement does not override the separate vehicle finding that iOS selected only one of them for
an entire session.

## Deliverable

The framing, the Cinemo call contract, the resampling decision with a named existing
implementation if one exists, and an ordered plan with the risky steps marked -- enough that
the first coding pass is transcription rather than discovery.
