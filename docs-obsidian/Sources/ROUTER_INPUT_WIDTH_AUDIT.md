# MPL router-input width audit

Date: 2026-09-02

> Status update (2026-09-02): this defect is fixed and confirmed on the car by making
> the MPL5 record mirror MPL1 at route-attach time. The unit now logs `channels=6`, the
> 62.5 Hz discontinuity is gone, and both discrete 5.1 and stereo play. The exact shipped
> build chain is [`../deploy/deva-ctrl-qc/BUILD_CHAIN.md`](../../deploy/deva-ctrl-qc/BUILD_CHAIN.md).
> The historical analysis below explains the failure that the fix removed.

## Result

The surviving hypothesis is confirmed in `deva-ctrl-qc.so`, with one naming
qualification: there is no separate channel-width field in the logical mixer
router. The effective width of a routed input is the channel count with which
the shared CSD stream/context is built when that DEVA timer is attached.

That value is wrong for the widened `mpl5_dio_ent` path:

```text
mpl1_int_ent  router input 1 -> timer id 1 -> min_voices 6 -> CSD 6ch / 0x4800
mpl5_dio_ent  router input 5 -> timer id 4 -> min_voices 2 -> CSD 2ch / 0x1800
                                                        ^
                                      still used as ACTIVE route geometry
```

The current patch correctly widens QSA negotiation to `max_voices=6` and
selects the live six-channel DMA fragment `0x4800`. It does not change the
independent route/session initializer, which copies the record's
`min_voices=2` and `min_fragsize=0x1800` into the active CSD session. No PCM
`acquire`, `prepare`, or `trigger` path re-provisions that session after a
six-voice open.

This is the first surviving defect that predicts all of the defining vehicle
measurements, including the correlated-six-channel test. It is established
statically; the isolated QEMU image cannot execute the MPL5 route because it
does not contain Audioman. A diagnostic six-only patch or a route-scoped
vehicle trace is still required for acoustic A/B confirmation.

The defining same-device A/B was:

| open | negotiated client fragment | measured delivery | audible result |
|---|---:|---:|---|
| MPL5, 2 voices | 3072 bytes | 2,445,312 frames / 50,831 ms = **48,107 frames/s** | excellent |
| MPL5, 6 voices | 9216 bytes | 2,445,312 frames / 50,807 ms = **48,129 frames/s** | broken |

Both clocks are consistent with 48 kHz. Discrete 5.1 and six correlated
channels were equally broken in the second row.

## Scope and binary identity

The accepted results remain accepted: APAC and the CarPlay hook, QSA widening,
the DMA-ring bytes and cursor, the shared CSD device id, the MOST connection,
and amplifier sink `0x26` were not re-investigated.

Static analysis used the exact local driver carried by the harness:

| Binary | SHA-256 |
|---|---|
| stock MU1316 `deva-ctrl-qc.so` | `58dddcabaa94c2fecf989855a6f810ec219926f49669a0ed9482ed629fdc64b9` |
| current patched `deploy/deva-ctrl-qc/deva-ctrl-qc.so` | `8b55b8d8f02b97d6f30191b54ef8a847e4706067f21cbf068d4b02a563ea84c9` |

It is an ELF32 little-endian ARM EABI5 PIC object with image base zero and GNU
build id `9168acff553fc86d6a3c991613d620fd`. In `.data`, virtual addresses are
file offsets plus `0x1000`; executable addresses used below equal file offsets.

## The table contains capability ranges, but route setup treats minima as active values

For platform id 2, `deva_mib_get_session_handles_from_platform_id @0x545a4`
returns the 31-record table at VA `0xa7d44` (file `0xa6d44`). Each record is
`0x30`/48 bytes. `deva_mib2_capabilities @0x55494` proves the relevant field
meanings by publishing record `+0x14` as `min_voices`, `+0x18` as
`max_voices`, `+0x1c` as `min_fragsize`, and `+0x20` as `max_fragsize` at
`0x5593c..0x55954`.

The records used by the two entertainment inputs are:

| field | record offset | MPL1, timer id 1, VA `0xa7d74` | MPL5, timer id 4, VA `0xa7e04` |
|---|---:|---:|---:|
| flags | `+0x00` | `0x2b3` | `0x2b3` |
| format | `+0x04` | `0x0c` (S32_LE) | `0x0c` (S32_LE) |
| rate mask | `+0x08` | `0x80` | `0x80` |
| min/max rate | `+0x0c/+0x10` | 48000 / 48000 | 48000 / 48000 |
| **min voices** | `+0x14` | **6** | **2** |
| max voices | `+0x18` | 6 | **6, patched from 2** |
| **min fragment** | `+0x1c` | **`0x4800` / 18432** | **`0x1800` / 6144** |
| max fragment | `+0x20` | `0x4800` | **`0x4800`, patched** |
| max buffer | `+0x24` | `0x2d000` | **`0x2d000`, patched** |
| max fragments | `+0x28` | 10 | 10 |
| bits/sample | `+0x2c` | 24 | 24 |

The two `min_*` values are correct as lower bounds: MPL5 must still admit a
two-voice/`0x1800` open. The defect is not the advertised range. The defect is
using those lower bounds as the active format without consulting the negotiated
stream.

## Runtime route chain

`deva_mixer_router_set_switch_routing @0x527f8` processes input/output pairs.
It calls the internal route worker at `0x53224` for disconnect and `0x53464`
for connect; the recovered worker is
`deva_mixer_router_configure_switch_routing @0x51b94`.

For a DEVA-input to CSD-output connection, the worker resolves the DEVA timer
and shared CSD session, then performs:

```text
0x51d68  session_csd_setup(csd_session, deva_timer)
0x51ddc  session_csd_attach_device(csd_session, deva_timer)
```

The router passes handles and the connect/disconnect action. It supplies no
independent width or CSD `CONFIGURE` payload.

`session_csd_setup @0x6bbdc` refreshes the session from the timer at
`0x6bddc` when a device is already attached and at `0x6bed0` while a new timer
is being attached. Both calls reach
`deva_update_session_info_from_list @0x54848`.

That function indexes the static record as
`table + 48 * deva_timer->device_id` at `0x549f4..0x54a00`, then makes these
assignments:

| instruction | source | destination | meaning |
|---:|---:|---:|---|
| `0x54a04 -> 0x54a08` | record `+0x0c` | session `+0x80` | active rate |
| **`0x54a14 -> 0x54a18`** | **record `+0x14`** | **session `+0x84`** | **active channels = `min_voices`** |
| `0x54a1c -> 0x54a24` | record `+0x2c` | session `+0x86` | active bit depth |
| **`0x54a20 -> 0x54a28`** | **record `+0x1c`** | **session `+0x6c`** | **active CSD buffer = `min_fragsize`** |

This directly supersedes the old claim that `min_voices` was used only by the
channel-map ioctl.

## What CSD does with the wrong values

`session_csd_setup_playback @0x6aba0` branches on session `+0x84`:

* at `0x6ae04..0x6ae08`, values at or below two take the stereo branch;
  `setStreamFormat2 @0x6b060` receives that channel count;
* values above two take `openAudioStreamV2 @0x6ae20`, literal-six
  `setStreamFormatPcmMultichannel @0x6aec0`, and literal-six
  `setStreamMultichannel @0x6af38`;
* a multichannel context is applied only when session `+0x84 > 2`, at
  `0x6b280`.

The other stale minimum is equally load-bearing:

* `allocateCsdBufferPool @0x6b158` receives session `+0x6c`;
* `session_csd_stream_playback @0x69084` reloads session `+0x6c` at
  `0x6919c`;
* it clears and copies exactly that many bytes at `0x691b8` and `0x6926c`;
* the same byte count is placed in the 36-byte descriptor passed to
  `csd_write @0x69318`.

Consequently the two inputs have these effective routed geometries:

| path | QSA/DMA fragment after the current patch | CSD stream | bytes copied/submitted per tick |
|---|---:|---:|---:|
| MPL1 / router input 1 | `0x4800` | six channels | `0x4800` |
| MPL5 / router input 5, two-voice open | `0x1800` | stereo | `0x1800` |
| **MPL5 / router input 5, six-voice open** | **`0x4800`** | **still stereo** | **still `0x1800`** |

## Why no six-voice open fixes it

The current driver patch at `deva_mib2_aquire @0x563c8` branches to the helper
at `0x8f2c8`. The helper reads the recovered negotiated voice word at PCM
channel `+0x10` and selects `0x4800` when it equals six. The acquire path stores
that active fragment at PCM channel `+0x94`, and stores the channel pointer at
DEVA timer `+0x11c` at `0x56504`.

Those are live values, but the route/session updater never reads them. Its only
callers are CSD session/mixer setup paths. Conversely:

* `deva_mib2_aquire @0x56060` allocates the timer DMA state and calls
  `session_deva_timer_setup`, but does not rebuild an attached CSD session;
* `deva_mib2_prepare @0x55394` is a logging no-op;
* `deva_mib2_trigger @0x5598c` starts or stops the existing timer/stream;
* release tears the state down and does not update route geometry.

There is therefore no transition from the startup/runtime-route value 2 to the
negotiated value 6.

## The exact corruption predicted by the code

The six-channel internal fragment is 18432 bytes: 768 frames x 6 channels x
4 bytes. The stale CSD copy is 6144 bytes. Per 16-ms timer tick, the code:

1. copies only the first 6144/18432 = one third of the DMA fragment;
2. advances the DMA cursor by the full `0x4800`, skipping the remaining 512 of
   the 768 source frames;
3. submits those 6144 bytes as 768 stereo S32 frames.

With discrete 5.1, the stereo consumer sees the source word pairs in the
sequence `L/R`, `C/LFE`, `Ls/Rs`, then repeats that sequence for the next source
frame. With all six source channels correlated, each source sample becomes
three consecutive stereo frames, so it is locally repeated three times; the
next tick then jumps over 512 source frames. The wall-clock duration remains
correct, but each 16-ms block is low-pitched internally and ends in a
discontinuity. The discontinuity rate is 62.5 Hz.

That prediction matches all of the defining observations: correct QSA rate,
recognisable content, low voices, continuous ticking, failure at six voices
only, and equal failure when channel separation is removed. It is a much more
specific match than a generic channel-map or clock hypothesis.

## Harness result and its limit

No harness or binary changes were needed for this audit. The accepted topology
artifacts remain:

| Artifact | SHA-256 | What it proves |
|---|---|---|
| `build/qsa-emu/qsa-topology-mpl1.log` | `e0f9f5a27a24a52f213bc2d5b4b10271377eba086eb292b879cf38b2e25f1a8a` | input 1 has a default CSD route and runs `stream_type=2` with the six-channel session |
| `build/qsa-emu/qsa-topology-mpl5.log` | `3ca3a4f2b66213ef06eaf1a4110417e9cd4a1c434c9c8522981f721db725962f` | without Audioman, input 5 stays `stream_type=1` and never creates the route whose geometry is at issue |

The six-channel CSD descriptor previously decoded in the harness belongs to
the default MPL1/shared-output route. The common startup CSD traffic in the two
images is naturally identical because it occurs before the isolated probe.
Neither observation measured the missing MPL5 runtime attach. This is a scope
correction, not a failure of the ring, descriptor, or submission validators.

## Smallest change

There are two materially different answers.

### Six-channel diagnostic only

For a controlled six-channel-only A/B, the smallest data change is to make the
MPL5 minima match MPL1:

| field | VA | file offset | change |
|---|---:|---:|---:|
| `min_voices` | `0xa7e18` | `0xa6e18` | 32-bit LE `2 -> 6` |
| `min_fragsize` | `0xa7e20` | `0xa6e20` | 32-bit LE `0x1800 -> 0x4800` |

Changing only the voice field is insufficient because the CSD buffer would
remain one third of the required size. Changing the two loads in
`deva_update_session_info_from_list` from the `min_*` offsets to the `max_*`
offsets has the same six-only effect.

Neither is a production fix. It turns MPL5 into a six-only route and either
rejects or mis-renders its known-good two-voice mode.

### Correct dual-mode behavior

The active CSD geometry must come from the live PCM channel, not a capability
bound. The relevant fields are recovered from actual call sites, not guessed:

```c
channel = deva_timer->pcm_channel;       // timer +0x11c, stored at 0x56504
active_voices = channel->voices;         // channel +0x10, used by helper 0x8f2c8
active_fragment = channel->frag_size;    // channel +0x94, stored at 0x563d4

csd_session->channels = active_voices;   // session +0x84
csd_session->buffer_size = active_fragment; // session +0x6c
```

Route-before-open must also be handled. If Audioman attached the route before
the PCM channel existed, acquire/prepare must detect that the attached session
is 2ch while the negotiated channel is 6ch, stop/reset it, and run CSD setup
again before the timer starts. Merely overwriting the two fields of a live
session is unsafe because its stream, context, and buffer pool were already
created with the old geometry.

That dynamic re-provision is the smallest correct behavioral change that
preserves both the excellent two-voice mode and the desired six-voice mode. No
implementation was made in this measurement pass, as required.

## Decisive read-only confirmation on the unit

With route-scoped slog enabled, `session_csd_setup` prints its active format at
`0x6c004..0x6c04c`. The predicted MPL5 route line contains
`sample_rate=48000, channels=2, bit_per_sample=24`. Qualcomm's existing dumps
provide a second check: the DEVA timer side should advance/capture `0x4800` per
tick while `csd_data_dumps` is called with `0x1800` at `0x694c4`.

An A/B in which only the two diagnostic minima are forced to six should remove
the corruption if this static diagnosis is acoustically complete. Until that
test, the measurement supports “the width mismatch exists and exactly predicts
the symptom,” not “the amplifier output has already been heard clean after the
fix.”
