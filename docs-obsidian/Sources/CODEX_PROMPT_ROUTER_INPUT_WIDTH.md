# Task: mpl5_dio_ent is correct at two voices and broken at six — find what is still two channels wide

## The measurement that defines the task

On the car, one device, one tool, one piece of content, nothing else changed:

    /mnt/app/root/hooks/qsa_play <file> 2 48000 /dev/snd/mpl5_dio_ent
        device: 2 voices, 48000 Hz, frag 3072, frags 10
        TOTAL 2445312 frames in 50831 ms -> 48107 frames/s     -> sounds excellent

    /mnt/app/root/hooks/qsa_play <file> 6 48000 /dev/snd/mpl5_dio_ent
        device: 6 voices, 48000 Hz, frag 9216, frags 10
        TOTAL 2445312 frames in 50807 ms -> 48129 frames/s     -> sounds broken

The six-channel file was tested in two forms: discrete 5.1, and the same music fanned so all
six channels are correlated. **Both are equally broken**, so this is not about channel
separation. Meanwhile RetroArch plays six voices into `/dev/snd/mpl1_int_ent` on this same
unit and sounds correct.

The sample clock is right in every case. The corruption is in rendering, it appears only at
six voices, and only on this virtual channel.

## Everything already excluded — do not revisit

* The whole CarPlay stack: libairplay, APAC, our ring, our hook, the 2->6 expansion.
  `qsa_play` has none of it and reproduces the fault (`docs-obsidian/Sources/AUDIO_5_1_STATE.md` §11).
* The tool itself, and any concurrent open by libairplay — either would corrupt the
  two-voice case too, and it is clean.
* The deva caps table: all eleven fields checked against `mpl1_int_ent`.
* The CSD stream descriptor, ring contents, submission order through `csd_write`.
* The amplifier connection: `docs-obsidian/Sources/DIO_MEDIA_SINK_STATIC_AUDIT.md` — connection 162 already
  resolves to `eSNK_DTCP51`, `SinkNr 0x26`, `BlockWidth 14`, six channels.
* The CSD device and topology: `docs-obsidian/Sources/DEVICE_TOPOLOGY_AUDIT.md` — `mpl1_int_ent` and
  `mpl5_dio_ent` share `dev_id 0x4e/78` and `128 TDM_OUT_MPL`, with byte-identical startup
  CSD traffic.

## What is left

Your own device audit recorded the one asymmetry that survives: the **router input** is
**1 for `mpl1_int_ent` and 5 for `mpl5_dio_ent`**, both feeding `128 TDM_OUT_MPL`.

Hypothesis: router input 5 is provisioned two channels wide. Six interleaved channels
written into a two-channel input make the DSP walk the buffer with the wrong stride, so the
frame boundary slides continuously — which is exactly what is heard, and it leaves the sample
clock untouched, which is exactly what is measured.

Find, with addresses and evidence:

1. **Where the per-input width of the ADSP router is set.** Candidates: deva's router setup
   (`deva_mixer_router_set_switch_routing`, `deva_setup_mixer`, `build_playback_mixer`), the
   CSD `CONFIGURE` payload for the input, or ACDB data (`mmx2_8064`).
2. **What width inputs 1 and 5 actually carry**, and whether that value is a constant, a
   table entry, or derived from the opened stream's voice count.
3. **Whether anything re-provisions the input when a six-voice stream is opened.** If the
   stream widens but the router input does not, that is the fault, and it matches every
   measurement taken so far.
4. The smallest change that would widen input 5 to six channels — a table byte, an ACDB
   field, or a call that must be made. If the width is already six on both, say so plainly
   and name the next surviving difference; a clean negative is a real result.

## Constraints

Static analysis, plus the QEMU harness you extended for the topology audit. Do not modify the
hook, the deva patch, or any binary. Measurement only.

Note the harness limit you documented: without Audioman, `mpl5` stays `stream_type=1` and its
data goes to `session_void_stream_buffer`, so an isolated open produces no CSD traffic. On the
car the route plainly exists — the audio is audible, merely wrong — so the router
configuration is reachable at runtime and is what needs describing.
