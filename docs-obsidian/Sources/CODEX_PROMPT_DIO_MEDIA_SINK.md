# Task: find who connects the CarPlay virtual channel to the amplifier, and with which sink

## What was proven today, on the car — do not re-litigate this

A standalone player (`tools/qsa_play.c`, built for QNX, on the unit at
`/mnt/app/root/hooks/qsa_play`) wrote a known-good six-channel PCM file straight into
`/dev/snd/mpl5_dio_ent`. CarPlay, libairplay, the APAC decoder, our ring, our hook and the
2->6 expansion were all out of the path. The device granted the correct geometry:

    device limits: voices 2..6 rate 48000..48000 frag 6144..18432 formats 0x1000
    device:        6 voices, 48000 Hz, frag 9216, frags 10
    TOTAL 2445312 frames in 50822 ms -> 48115 frames/s (expected 48000)

The content is the decoded APAC capture from this car, already confirmed clean on a Mac.
Through `mpl5_dio_ent` the driver reported a correct clock and **it sounded broken in
exactly the way CarPlay sounds broken** — constant ticking, wrong pitch, recognisable
content. Verified by ear: the channel-ID test track announced "Front Right".

Meanwhile RetroArch (`Tools/retroarch-qnx`) writes six voices to `/dev/snd/mpl1_int_ent`
on this same unit and sounds correct.

So: same deva, same DSP, same MOST, same amplifier; two different virtual channels; one
clean, one broken. **The fault is bound to the CarPlay virtual channel, not to the shared
six-channel path, and not to anything in the CarPlay stack.**

Everything upstream is already measured clean and must not be re-investigated — see
`docs-obsidian/Sources/AUDIO_5_1_STATE.md` for the full evidence table, including the QEMU harness proving
the PCM is byte-exact and correctly ordered all the way to `csd_write`.

## The hypothesis to test

`docs-obsidian/Sources/reference/AUDIO.md` has said from the beginning that discrete 5.1 needs two halves:

1. the deva caps patch, so `mpl5_dio_ent` accepts six voices — **done, and confirmed
   loaded on the unit** (`voices 2..6`);
2. the B&O / mixer agent provisioning the CarPlay **virtual channel** for six voices routed
   to the amplifier — **never done**. Stock provisions it as stereo.

The amplifier's selectable MOST sinks (`Firmwares/SoundAmp/AMP_ENTERTAINMENT_7_1_MOST.md`):

    0x21  Entertainment_2.0      BlockWidth  4   2 x 16-bit
    0x2A  Entertainment_2.0_SAD  BlockWidth  8   2 x 24-bit
    0x26  Entertainment_5.1_SAD  BlockWidth 14   6 x 16-bit
    0x32  Entertainment_7.1_SAD  BlockWidth 26   8 x 24-bit

If the CarPlay connection is established against a stereo sink while six channels are being
streamed, the amplifier reads 4- or 8-byte frames out of a 14-byte-wide stream. The frame
boundary slides continuously, which is audible as ticking, a wandering pitch, and
recognisable audio coming from the wrong speakers. That matches every reported symptom.

## What to find

The amplifier connection ids are known (`Tools/retroarch-qnx/src/audio/dsi_audio_client.h`):

    DSI_CONN_ENT_MEDIA_MFP   20   internal media  (the one that sounds correct)
    DSI_CONN_ENT_GAL_MEDIA  156   Android Auto
    DSI_CONN_ENT_DIO_MEDIA  162   CarPlay         (the one that sounds broken)

Answer these, with addresses and evidence:

1. **Which component on the HU issues the MOST Connect** for the amplifier — the message is
   `FBlock 0x22, FktID 0x0112, StartResult`, payload
   `SenderHandle(2) SinkNr(1) BlockWidth(2) ConnectionLabel(2)`, big-endian. Candidates worth
   checking first: `armle/bin/audio_service`, the DSI audio proxy behind
   `/eso/lib/factories/lib_07302fa7-ae8e-5815-b570-33d89f0cfeaf.so`, and whatever owns the
   MOST Nav/Amp FBlocks. Firmware tree:
   `/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/`.
2. **Which SinkNr and BlockWidth it uses per connection id.** Specifically: is the sink a
   per-connection constant, a table indexed by connection id, or derived from the negotiated
   PCM voice count? If it is a table, give its address and contents.
3. **Whether connection 162 can be made to use `0x26`** (six channels, BlockWidth 14) — a
   table byte, a config file, a runtime property, or a code path. State which, with the exact
   location. If it is derived from the voice count and simply never re-derived after our caps
   patch, say so and name the function that would have to re-run.
4. If the sink turns out to be correct and six-channel already, say so plainly — that
   falsifies this hypothesis and moves the fault to the DSP/MOST framing below deva, which
   is a useful result. Do not manufacture a cause.

## Constraints

* Static analysis only. Do not modify any binary, and do not propose flashing the amplifier —
  the suspected fault is on the head-unit side and that is where the fix belongs.
* Do not touch the hook, the deva patch, or anything in the CarPlay stack. Today's
  measurement exonerated all of it.
* Prefer `Tools/retroarch-qnx/src/audio/dsi_audio_client.c` as the map of the DSI audio proxy
  ABI: it already documents the factory path, the vtable offsets, and the reply codes, and it
  is proven working on this unit.

## Deliverable

A short report: the component and function that establishes connection 162, the sink it
selects and where that value comes from, and the smallest change that would make it select a
six-channel sink — or a clear statement that it already does, with the evidence.
