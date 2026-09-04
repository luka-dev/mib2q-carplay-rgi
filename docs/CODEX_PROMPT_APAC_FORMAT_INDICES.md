# Task: which audioOutputFormatsExtended index is APAC stereo?

## Where this comes from

Discrete 5.1 CarPlay audio now works end to end: six channels reach the speakers, all
counters clean. Two defects were fixed today and both were the same shape -- announcing
something other than what is actually there. The SAD header declared two channels while six
slots were sent (`docs/SAD_HEADER.md`), and the `updateFeedback` response was blanked, which
removed the sender's clock reference and collapsed delivery to about one packet a second.

One symptom remains, and it is a third instance of the same shape. With `AUDIO_SPATIAL=1`
the phone skips every stereo track and only plays natively-Atmos ones. The hook's own /info
injection says why:

    probe103: /info audioFormats += {type:103, audioType:media,
                                     audioOutputFormats:0x400000,
                                     audioOutputFormatsExtended:[65]}

For the buffered type-103 media stream we advertise exactly one format, index 65 = APAC 5.1.
A stereo track has no format it can be delivered in, so the phone skips it. It is not
refusing to upmix; we left it nothing to use.

## The question

**Which extended format index is APAC stereo (and, if they exist, other non-5.1 APAC
variants), on the iOS side?**

Known so far, from our own work: 65 = APAC 48 kHz 5.1, 66 = APAC 48 kHz 5.1.2. Those come
from `docs/reference/AUDIO.md` and the format-index probing behind
`hook/audio/audio_hook.c` (`SPA_APAC_48K_5_1`).

You have the iOS side available -- the dyld shared cache under `Refferences/ios27_dsc/`,
which previous work used for AirPlay and CarKit RE. Answer from the enumeration itself
rather than from inference:

1. **The full extended-format enumeration** as iOS defines it: index -> codec, sample rate,
   channel layout. Name the symbol or table you read it from.
2. **Which indices are APAC** and which of those are stereo or otherwise below 5.1.
3. **What `audioOutputFormats` bits mean** beside the extended list -- we currently set
   `0x400000` (bit 22) for this entry. Is the legacy bitmask expected to be consistent with
   the extended list, and if so which bit pairs with each APAC index?
4. **Whether advertising several extended indices at once is the normal shape**, or whether a
   receiver is expected to publish one entry per format. If Apple's own receivers list
   several, say how they order them.

## Constraints and what would make this useful

Static analysis only; do not modify anything. A wrong index here means the phone either
refuses the stream or sends something the decoder cannot parse, and the unit is currently in
a good state that took a long time to reach -- so state your confidence per index, and say
plainly which parts are read from the enumeration and which are inferred.

If the enumeration shows that the buffered type-103 path is inherently spatial-only and
stereo content is expected to go via the realtime type-100 stream instead, that is an equally
good answer: it would mean the fix is to keep the realtime path usable alongside 103 rather
than to add an index.

## Deliverable

The index numbers with their layouts, the matching legacy bits, and a recommendation for what
`audioOutputFormatsExtended` and `audioOutputFormats` should contain for a receiver that wants
both stereo and 5.1 over the buffered stream.
