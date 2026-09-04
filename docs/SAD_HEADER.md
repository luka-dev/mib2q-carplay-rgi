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

**Runtime qualification, audited 2026-09-04:** the SET branch at
`0x3f8a8..0x3f908` only saves these fields in the persistent MPL session. It does not issue a
CSD configure command. `session_csd_start_playback` converts and sends the saved SAD at
`0x6a048/0x6a058`; consequently another `createMetadata` call during an already-running track does
not change the live LPASS header until the next playback start. The exact allocation table and the
implications for APAC/AAC track switching are in
[`DYNAMIC_BUFFERED_CODEC_SWITCH.md`](DYNAMIC_BUFFERED_CODEC_SWITCH.md#sad-speaker-allocation-exact-values-and-update-lifetime).

**Amplifier-side qualification, audited 2026-09-04:** the far side is live even though the
head-unit setter is not. Amplifier `FUN_000D1D94` polls the INIC/MOST allocation, compares the
returned byte at `0xD1E4C` with saved `gp-0x3e21`, stores a change at `0xD1ED2`, and queues task-9
event 15 at `0xD1ED6..0xD1EDA`. The event re-evaluates and rewrites the SHARC input matrix. On the
current SAD-capable sink `0x26`, both `0xc0` and `0xfc` still select matrix bank 1 and SHARC control
value `5.0`; only raw-bank-2 sink profiles use ASAD to choose bank 0/1/2. Thus the allocation repair
was essential for keeping six source positions alive, but it was not what selected the amplifier's
5.1 matrix on sink `0x26`. Full proof and stereo/5.1 coefficients:
[`AMP_STEREO_5_1_MATRIX_DIFF.md`](AMP_STEREO_5_1_MATRIX_DIFF.md).

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
item with no usable stereo representation. There is no observed fallback to realtime type 100.

An early `[65,23]` test selected 23 initially and did not visibly return to APAC 65 for the item
called Atmos. A later iOS static pass shows that this is not a session-wide protocol rule: the
active buffered engine has a transport-format-change path and its selector chooses 65 when the
source ASBD actually has six channels. That old test did not log the source ASBD or packet field 2,
so it remains evidence about that playback, not proof that switching is impossible.

Four alternative indices were tried on the car, and the results are worth keeping because
several of them look right on paper:

| index | what it is | outcome |
|---|---|---|
| 23 | AAC-LC/48000/stereo | **selected and preferred** -- the APAP time scale was 0xBB80 (48000), and the stream carried a separate 39-byte cookie. Nothing decoded it, so playback was silent. No sample-rate conversion is needed. |
| 22 | AAC-LC/44100/stereo | **selected in the earlier test** -- skipping stopped, the phone asked for 22 and the APAP time scale was 0xAC44 (44100). It is now only a fallback because it also needs 44.1 -> 48 resampling. |
| 64 | APAC/48000/stereo | **never selected** -- advertised as `[65,64]`, the phone still requested 65. iOS gates it behind `bufferedAPACStereo`, whose static default is false. |
| 11 | PCM | not offered at all: `APAudioFormatGetBufferedPassthroughSupportedFormats` returns zero unless the device is an APM speaker, so bit 11 never reaches a CarPlay session. |

So the negotiation half of the AAC route is proven: the phone sends buffered stereo AAC-LC.
Index 23 is the correct production candidate because it matches every HU media endpoint at
48 kHz and therefore needs an AAC decoder but no resampler. The unit has Cinemo's
`libNmeAudioAAC.so`, and `dio_manager` already links `libNmeSDK.so`.

Until that exists the honest states are: `AUDIO_SPATIAL=0` and everything plays in stereo, or
`AUDIO_SPATIAL=1` with `[65]` and spatial becomes an Atmos-only mode.


### Does iOS upmix stereo into the spatial stream? No.

Worth settling, because a lot of effort went into assuming it might. Neither `AirPlaySender`
nor `AirPlaySupport` contains an upmix or spatialisation routine -- no `upmix`, no
`spatializ*`, nothing of that shape. What exists is badging:

    kAPSBadgingFormatInfoType_{AppleSpatialAudio, DolbyAtmos, DolbyAudio, Stereo, Surround}
    audioStream_updateBadgingFormatInfoAndPostNotification

"Spatial" is a label describing what is already being sent, not evidence that the sender
converted anything. The spatial rendering heard on the phone itself is a local playback path
and is not applied before transmission.

So there is no configuration under which iOS delivers an ordinary track as APAC 5.1. Non-spatial
content needs a stereo codec, which makes an AAC-LC decoder necessary. Dynamic APAC/AAC selection
is possible inside the same buffered session, but only when iOS presents a changed source ASBD.
The exact selector and receiver state machine are documented in
[`DYNAMIC_BUFFERED_CODEC_SWITCH.md`](DYNAMIC_BUFFERED_CODEC_SWITCH.md).

Exact MHI2Q framing evidence, Cinemo ABI, cookie parser boundary, threading rules and the ordered
no-drive validation plan are recorded in
[`AAC_BUFFERED_PATH_PLAN.md`](AAC_BUFFERED_PATH_PLAN.md).


### Framing of an index-23 stream, measured on the car (2026-09-03)

The last unknown in the AAC plan, settled with a one-shot probe that authenticates and opens a
packet, reports it, and decodes nothing.

The APAP extension TLVs come first, and they carry more than expected:

    02 01 17                        format index, 0x17 = 23
    11 08 0e ae 55 37 e8 3c 74 18   magic cookie id
    12 27 03 80 80 80 22 ...        the cookie itself, all 39 bytes, inline

So the decoder configuration is repeated in **every packet**, not only in SETMAGICCOOKIE. That
removes a dependency on command ordering -- a track change brings its new ASC alongside the
data -- which is the exact hazard behind the duplicate-cookie handling that cost time earlier.

The access unit that follows is raw:

    access unit 904 bytes, first: 21 4e aa fb ff fd ef fd ...

Not `ff f1` or `ff f9`, so there is no ADTS header: one raw AAC-LC access unit per APAP media
item, handed to the decoder as-is.

Combined with the cookie parse, the AAC route is now fully specified: index 23, AAC-LC at
48000 Hz stereo (ASC `11 90`, reached by walking the ES_Descriptor rather than a fixed offset),
raw access units, no resampler.
