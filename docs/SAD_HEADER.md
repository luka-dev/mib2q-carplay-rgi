# The SAD header — what the head unit tells the amplifier is in the stream

Written 2026-09-02, after the six-voice rendering defect was fixed and a one-hot test still
put only L and R through to speakers.

## What it is

Beside the PCM on MOST rides a small **speaker-allocation** header. The PCM slots are always
formed — six of them, whatever the content — and the SAD says **how many of them are
meaningful**. Announce two, and there is no reason for anything downstream to route the other
four to speakers.

deva logs what it receives:

    convert_sad_header_to_csd: media_type=.. codec_type=.. emi=.. asad=0x%x cgms=.. bitrate=.. sample_width=0x%x

Observed on this car:

| path | asad | sample_width |
|---|---|---|
| internal media / RetroArch on `mpl1_int_ent`, six voices, correct | `0xfc` | `24` |
| CarPlay / AirPlay on `mpl5_dio_ent`, six voices, only L+R audible | `0xc0` | `16` |

`asad` reads as a slot mask, most significant bit first:

    0xfc = 1111 1100   slots 1..6 active, 7 and 8 not   -> 5.1
    0xc0 = 1100 0000   slots 1..2 active                -> stereo

That reading is inferred from the two observed values and the arithmetic, not from a
specification we have read. It is self-consistent and matches the channel count in both cases.

## Where it is built

`dio::CQNXSoundIF<CLockingPolicy>::createMetadata` in `libairplay.so`, which documents its own
arguments in its log string:

    "[%s]<AUDIO>nChannels: %u, nBitsPerChannel: %u, nSampleRate: %u"

so the signature is `createMetadata(this, nChannels, nBitsPerChannel, nSampleRate,
en_encryption_t, en_codec_t)`. It pushes the result to the driver with
`DCMD_PCM_SET_SAD_METADATA`, which `deva`'s `pcm_devctl @0x3f0a0` handles.

The devctl numbers are **not** compile-time constants: `pcm_devctl` compares against values
held in the driver context at `+92` and `+96`, registered at start-up. A userspace client
cannot hardcode them, which is why the fix belongs at `createMetadata` and not at a raw
`devctl` call.

The metadata struct is 20 bytes; the GET path at `0x3f748` copies it field by field out of the
driver context, which gives the layout:

    [0] media_type  [1] codec_type  [2] emi  [3] asad  [4] cgms
    [8] bitrate (dword)  [12] sample_width  [16] dword

## The defect

We widen the ASBD to six channels and six TDM slots are formed, but this call still carried the
stock stereo count, so the header announced `asad=0xc0`. Six channels sent, two declared.

Both template instantiations are exported weak and libairplay reaches them through its PLT, the
same seam `AirPlayReceiverSessionSetup` already uses, so the hook interposes them with no new
mechanism. With Spatial armed it now announces six channels and 24 bits, matching what the
slots carry and what the working path declares. `AUDIO_SAD_CHANNELS` and `AUDIO_SAD_BITS`
override; `0` disables. Both the original and substituted values are logged.

## Why this also retracts an earlier conclusion

`tools/qsa_play.c` sets no SAD at all, so it inherited whatever the last CarPlay session left —
`0xc0`. The standalone player was therefore announcing stereo too, and its L/R-only output was
**this bug, not evidence against the CarPlay stack**. That result was read as proof the fault
lay below deva, and it sent the search into the ADSP, the MOST framer and the amplifier for two
days. Everything found there was correct, which is consistent: the amplifier was doing what it
was told.

Consequence for testing: verify **through CarPlay**, not with `qsa_play`. The player still sets
no SAD and will only show whatever the session left behind.

## Confirmed on the car, 2026-09-03

Both sides of the seam, on a fresh boot with the interposer deployed:

    [hook]   createMetadata(locked): announcing 6ch/24bit instead of 2ch/16bit at 48000 Hz
    [deva]   convert_sad_header_to_csd: media_type=0x0 codec_type=0x1 emi=0x3
             asad=0xfc cgms=0x0 bitrate=0x697800 sample_width=0x18

Exactly the predicted values, and the channels arrive: the channel-ID track now plays centre
and surrounds, where before only L and R reached speakers.

Quality is unaffected. Over 8192 writes: `zeroWrites=0 shortWrites=0 devUnderruns=0
ringRead=768/768 odd=0`, and `underrunFrames` sits at 256 -- one 5.3 ms gap at start-up that
does not grow between samples, so it is priming rather than a leak.

Note on height channels: a 7.1.4 test track's "TopSide" announcements come from behind and
below, which is expected rather than a defect. The transport is 5.1 and carries no height
channels, and the amplifier declares positions 7 and 8 as BCL/BCR -- rear, not height, with no
`0x0D`/`0x0F` codes in any profile. Eight channels are a hardware limit here, which is why the
8->6 fold is mandatory.

## What was unproven before that test

That the far side honours `asad` at all. deva's own line settles it in one drive:

    asad=0xfc, sample_width=0x18   the interposer took effect
    asad=0xc0, sample_width=0x10   it did not; look for another caller

And separately: whether announcing six is sufficient, or whether something downstream also has
to be told. Only the car answers that.

## Related

* `docs/AUDIO_5_1_STATE.md` — the whole investigation, with the negatives.
* `docs/AFE_TDM_MOST_SLOT_AUDIT.md` — TDM slot geometry and where this asymmetry was found.
* `deploy/deva-ctrl-qc/BUILD_CHAIN.md` — the rendering fix that came first.


---

## The other half: updateFeedback carries the sender's clock (2026-09-03)

Announcing six channels got the channels to the speakers, but with `AUDIO_SPATIAL=1` the
buffered session then played nothing and the phone skipped tracks as though they were
unplayable. Same shape of mistake as the SAD header, one layer up.

The hook replaced the `streams` array of every `updateFeedback` response with `[]` for an
active type-103 session. The stock response is:

    {'streams': [{'sr': 48004.0625, 'type': 100}]}

`sr` is a **measured** rate, not a nominal one -- the receiver's clock as the sender should
see it, and how the phone paces delivery. Blanking it did not merely omit a stream list, it
took away the only clock reference the sender had. Delivery collapsed to about one packet a
second where the stream needs forty-seven, so flush boundaries armed at one moment cleared
three minutes later and everything in between was discarded by design.

Measured in the failing session: boundary armed 00:44:31, crossed 00:47:42, 212 packets in
191 seconds.

Blanking is now opt-in behind `AUDIO_FEEDBACK_BLANK`. With it off an Atmos track plays, all
six channels carry content, `underrunFrames` is zero and every ring read returns a full
fragment.

The stock response never mentions our type-103 stream -- the phone is fed clock feedback for
the realtime type-100 stream only, and that is enough. Synthesising an entry for the buffered
stream was the fallback plan and proved unnecessary.

**Still open:** on connecting, the phone skips a few tracks before settling on one it plays.


---

## Why non-spatial tracks are skipped, and what each candidate did (2026-09-03)

With `AUDIO_SPATIAL=1` the phone skips ordinary tracks and plays only natively-Atmos ones. It
is not declining to upmix: advertising `audioOutputFormatsExtended:[65]` leaves a non-spatial
item with no format it can arrive in, so the sender's per-item format choice fails and it moves
on. The choice is made per track inside an already-selected type 103, and the researched sender
code has no fallback to the realtime type 100.

Three second indices were tried on the car, and the results are worth keeping because two of
them look right on paper:

| index | what it is | outcome |
|---|---|---|
| 22 | AAC-LC/44100/stereo | **selected** -- skipping stopped, the phone asked for 22 and the APAP header carried 0xAC44 (44100). Nothing decoded it, so playback ran silent. |
| 64 | APAC/48000/stereo | **never selected** -- advertised as `[65,64]`, the phone still requested 65. iOS gates it behind `bufferedAPACStereo`, whose static default is false. |
| 11 | PCM | not offered at all: `APAudioFormatGetBufferedPassthroughSupportedFormats` returns zero unless the device is an APM speaker, so bit 11 never reaches a CarPlay session. |

So the negotiation half of the AAC route is proven -- the phone will send ordinary content as
AAC-LC/44.1 over the buffered stream when index 22 is advertised. Playing it needs an AAC-LC
decoder and 44.1 -> 48 resampling, since every HU media endpoint is 48 kHz only. The unit has
Cinemo's `libNmeAudioAAC.so` and `dio_manager` already links `libNmeSDK.so`.

Until that exists the honest states are: `AUDIO_SPATIAL=0` and everything plays in stereo, or
`AUDIO_SPATIAL=1` with `[65]` and spatial becomes an Atmos-only mode.
