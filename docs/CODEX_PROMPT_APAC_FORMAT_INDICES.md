# Task: why does the phone skip stereo tracks instead of sending them on the realtime stream?

## The original question is already answered — from our own notes

The first version of this brief asked which extended index is APAC stereo. There is none.
`docs/HANDOFF_SPATIAL_AUDIO.md` already records the enumeration iOS 27 uses, and every APAC
index is multichannel:

    65 APAC/48000/5.1     67 APAC/48000/7.1      76 APAC/48000/5.1.4
    66 APAC/48000/5.1.2   68 APAC/48000/7.1.4    77 APAC/48000/7.1.2
                                                  92 APAC/48000/9.1.6

and it calls them "CarPlay spatial-master formats". So the buffered type-103 path is
spatial-only by design, stereo is meant to travel on the realtime type-100 stream, and adding
an index is not the fix.

## The symptom that remains

With `AUDIO_SPATIAL=1` the phone skips every stereo track on connect and only plays natively
Atmos ones. It is not declining to upmix -- it appears to route all media to type 103, where
a stereo track has no representable format, and gives up on it rather than falling back.

Our /info injection is:

    {type:103, audioType:"media", audioOutputFormats:0x400000,
     audioOutputFormatsExtended:[65]}

added alongside the existing entries, which already include the realtime stream. Our own notes
flag the ambiguity: `audioType: "media"` appears on the realtime stream too, so both entries
claim media and only the `bufferStream` key distinguishes the buffered one.

## What to determine, from the iOS side

`Refferences/ios27_dsc/24A5418b__iPhone18,1` is the dyld shared cache; previous work read
`APCarPlayAudioFormatsCreate` @0x25107B82C and `APAudioFormatIndexToAPAudioFormat` out of it.

1. **How the sender chooses between a type-100 and a type-103 entry for a given track.** Is it
   per-session, or re-evaluated per item? What happens when the item has no format representable
   in the chosen stream's extended list?
2. **Whether a fallback to type 100 exists at all**, and what the receiver must advertise for it
   to be taken. Specifically: does the presence of a type-103 entry claiming `audioType: media`
   suppress the realtime entry for media, and would a different `audioType` on the buffered entry
   (or the `bufferStream` key alone) leave both usable?
3. **What Apple's own receivers advertise** when they support both stereo and spatial -- one
   entry or two, which `audioType` on each, and whether the legacy `audioOutputFormats` mask on
   the buffered entry participates in the choice. Ours is `0x400000` = AAC-LC/44.1/stereo, a
   compatibility value that must be nonzero; does the sender ever try to use it literally?
4. If the skipping is instead the sender declining content it cannot render spatially -- i.e.
   expected behaviour for a receiver that advertises a spatial buffered stream -- say so
   plainly. That would mean the fix is on our side: advertise 103 only when we want
   spatial-only playback, and treat `AUDIO_SPATIAL=1` as a mode rather than an enhancement.

## Constraints

Static analysis only. The unit is currently in a good state -- discrete 5.1 plays, all counters
clean -- and it took a long time to get there, so separate what you read from what you infer and
say which changes you would trust on a car.

## Deliverable

What decides the stream for a given item, whether a stereo track can reach the realtime stream
while a type-103 entry is advertised, and the smallest /info shape that lets both stereo and
spatial play.
