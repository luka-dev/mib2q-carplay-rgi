# Task: why does deva render six voices correctly on mpl1_int_ent and wrongly on mpl5_dio_ent?

## Accepted, do not redo

Your sink audit stands: connection 162 resolves to `eSRC_ENT_AMP_DIO_MEDIA_MC` +
`eSNK_DTCP51`, physical `SinkNr 0x26`, `BlockWidth 14`, 6 x 16-bit protected PCM. The
amplifier connection is already six-channel and needs no change. That hypothesis is closed.

Also closed, from earlier passes: the deva caps table (all eleven fields checked against
`mpl1_int_ent`), the CSD session descriptor geometry, ring contents, and submission order
through `csd_write`. And on the car, the whole CarPlay stack — see
`docs/AUDIO_5_1_STATE.md` §11.

## The one fact that now carries everything

On the unit, with **no CarPlay in the path at all**:

    qsa_play <known-good 6ch file> -> /dev/snd/mpl5_dio_ent   -> sounds broken
    RetroArch 6 voices             -> /dev/snd/mpl1_int_ent   -> sounds correct

Same deva, same DSP, same MOST, same amplifier, same six-channel sink. The player reported
`6 voices, 48000 Hz, frag 9216, frags 10` and a measured 48115 frames/s against 48000
expected, so geometry and clock are right. The corruption is in **rendering**, and it is
**bound to the virtual channel**.

So the question is narrow: what does deva configure differently for these two devices?

## Where to look

`deva_csd_init` logs `enableDevice(dev_id=%d)`, so each virtual channel maps to a CSD
device id, and the ADSP topology and ACDB calibration (`mmx2_8064`) hang off that id. A
device id provisioned as stereo, fed six interleaved channels, gives exactly the observed
signature: the DSP walks its input with the wrong stride, so the frame boundary slides —
audible as constant ticking, a wandering pitch, and recognisable audio from the wrong
speakers, with the sample clock still perfectly correct.

Answer, with addresses and evidence:

1. **Which CSD `dev_id` does each device use** — `mpl5_dio_ent` (index 4) versus
   `mpl1_int_ent` (index 1)? Give the table or the code that maps virtual channel to
   `dev_id`.
2. **What channel count / topology is attached to each of those ids** at
   `csd_open(CSD_OPEN_AUDIO_CONTEXT)` and `CSD_DEV_CMD_ENABLE` time. Is it a constant per
   id, an ACDB lookup, or derived from the stream?
3. **Does anything in the six-voice path re-provision that device**, or does
   `deva_csd_start_csd_session_playback` only configure the *stream* while the *device*
   keeps whatever `deva_csd_init` set at start-up? That asymmetry — stream widened, device
   not — would explain everything measured so far.
4. If the two ids are provisioned identically, say so plainly and name the next difference
   you can see between the two devices. Do not manufacture a cause; a clean negative moves
   the fault to the ADSP image or ACDB data, which is itself useful.

## You already have the tool for this

`tools/qsa-emu/` runs the real `io-audio` and the real `deva-ctrl-qc.so` with only the eight
CSD symbols stubbed, and your `csd_stub.c` already traces `csd_open` and every `csd_ioctl`
payload. So drive a six-voice open against **`mpl5_dio_ent`** and against
**`mpl1_int_ent`** in the harness and diff the device-side CSD traffic — not the stream
descriptor, which you already proved correct, but the `CSD_OPEN_AUDIO_CONTEXT` and
`CSD_DEV_CMD_ENABLE` calls and their parameters.

Traps already paid for are listed in `tools/qsa-emu/HANDOFF.md`; re-read it first.

## Constraints

Static analysis plus the harness. Do not modify the hook, the deva patch, or any binary.
Measurement only.

## Deliverable

The `dev_id` for each device, what channel configuration each carries and where that value
comes from, and whether the six-voice path re-provisions the device or leaves it at its
start-up configuration. If a stereo device provisioning is confirmed, give the exact field
and the smallest change that would widen it.
