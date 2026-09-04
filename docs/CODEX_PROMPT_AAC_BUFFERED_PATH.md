# Task: decode AAC-LC/44.1 on the buffered stream so non-spatial tracks play

## The problem, stated exactly

With `AUDIO_SPATIAL=1` the hook advertises a buffered type-103 media stream and CarPlay plays
natively-Atmos tracks in discrete 5.1 -- that part works end to end, six channels at the
speakers, counters clean. Every other track is skipped.

The cause is settled, and three candidate second formats were tried on the car:

| index | what it is | outcome |
|--:|---|---|
| 65 | APAC/48000/5.1 | works; the only thing we can currently decode |
| 22 | AAC-LC/44100/stereo | selected and sent, APAP header carries 0xAC44; would need a resampler |
| 23 | AAC-LC/48000/stereo | **selected and sent at the sink's own rate** -- APAP header carries 0xBB80, and its magic cookie is a separate 39-byte blob with its own id. This is the route. |
| 21 | ALAC/48000/24/stereo | breaks the negotiation outright: the /info entry goes out and no stream setup follows at all, playback hangs |
| 64 | APAC/48000/stereo | **never selected**: iOS gates it behind `bufferedAPACStereo`, static default false |
| 11 | PCM | not offered: `APAudioFormatGetBufferedPassthroughSupportedFormats` returns zero unless the device is an APM speaker |

The format is chosen per track inside an already-selected type 103, and the researched sender
code has no fallback to the realtime type 100. So index 22 is the only route that makes
ordinary content arrive at all, and it needs decoding on our side.

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

1. **The framing.** Inside APAP, is an index-22 payload raw AAC-LC access units, ADTS, or
   something else? The capture in `captures/spatial_silent_20260903/` is APAC, so a fresh
   index-22 capture is needed -- say what to capture and how to recognise the framing.
2. **The Cinemo contract.** Exact `CinemoCreateAudioCodec` arguments for AAC-LC, what it returns,
   how PCM comes out, and whether it can be driven from a hook thread inside `dio_manager`
   without disturbing the codec instances Cinemo already owns for local media. That last point is
   a real risk: the same library is serving the stock media path.
3. **Resampling.** Where 44.1 -> 48 should happen and at what quality: inside the decode wrapper
   before the ring, or in the sink. Say which existing code is closest and whether anything on
   the unit already resamples that we could reuse rather than writing one.
4. **Whether the magic cookie differs** for index 22 and what it carries -- the existing path
   builds its decoder from the cookie, and the render code now replaces a stored cookie when its
   bytes change, so an AAC cookie must not be mistaken for an APAC one.

## Constraints

Static analysis and a written plan; no code in this pass. The car is in a working state for
Atmos and that state is expensive -- say clearly which steps can be tested without risking it,
and which need a drive.

Do not propose replacing the APAC path. The two must coexist: index 65 stays for spatial
content, 22 handles the rest, and the render path dispatches on the negotiated index.

## Deliverable

The framing, the Cinemo call contract, the resampling decision with a named existing
implementation if one exists, and an ordered plan with the risky steps marked -- enough that
the first coding pass is transcription rather than discovery.
