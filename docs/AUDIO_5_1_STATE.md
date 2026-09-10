# Discrete 5.1 CarPlay audio — everything known, and what is still open

2026-09-10 call/Siri lifecycle update: buffered Media route claims now yield to
stock main audio from Prepare through Stop. See [call-routing evidence and
validation](AUDIO_CALL_ROUTING.md). Host-tested; live HU call verification is
still pending.

Status as of 2026-09-06. Discrete 5.1 CarPlay works end to end in the vehicle.
The last two faults were both declaration/control mismatches rather than PCM
corruption: the SAD header announced two active channels over six populated slots,
and the hook blanked `updateFeedback`, removing the sender's clock reference. Both
are corrected and vehicle-confirmed (§19). The historical fault-localisation below
is retained to prevent those closed paths from being rediscovered.

Ordinary buffered content is also vehicle-confirmed through AAC-LC/48 kHz/stereo
index 23 beside APAC 5.1 index 65. Codec changes are selected by the changed
inline cookie, not APAP fields 2/8; both switch directions play continuously
(§20). The current source retains only HSSR Universal for stereo-to-PCM6,
with rear gain `0.25` and Cabin-Aware LFE V2. `off` remains the exact
`L,R,0,0,0,0` safety bypass. Superseded profile discussions below are retained
as dated investigation history, not selectable current implementations.

> ## Localised on 2026-09-02: the CarPlay stack is not at fault
>
> A standalone player wrote a known-good six-channel file straight into
> `/dev/snd/mpl5_dio_ent`, with CarPlay, libairplay, the APAC decoder, our ring, our hook and
> the 2->6 expansion all out of the path. It sounded broken in exactly the way CarPlay sounds
> broken. RetroArch writes six voices to `mpl1_int_ent` on the same unit and sounds correct.
>
> Same deva, same DSP, same MOST, same amplifier; two virtual channels; one clean, one broken.
> **The fault is bound to the CarPlay virtual channel.** See §11 — read it before §8 and §10,
> which were written before this was known. This document exists because the
search has produced a large amount of solid negative knowledge that is expensive to
re-derive and easy to re-derive *wrongly*.

Read this before proposing a cause. Most obvious causes are already excluded by
measurement, and two of them were excluded only after being confidently asserted.

---

## 1. The symptom, as reported from the vehicle

By ear, from the driver, across several sessions:

* A **constant tick** — "тцык тцык тцык". Not once per second: continuous.
* **Voices very low pitched.**
* Tempo only **mildly** off. Explicitly *not* 3x or 4x slow.
* Sound arrives **from the correct speakers** — channel routing works.
* Content stays **recognisable**: TikTok audio, music, individual voices are identifiable.
* **Stereo CarPlay is always clean.** Only the six-channel path is affected.

The last two points matter more than they look. Recognisable-but-wrong excludes every
hypothesis that destroys the waveform, and a working stereo path on the same device
excludes anything shared by both widths.

---

## 2. Proven correct — do not re-investigate

Each line here is a measurement, not an argument.

| What | Evidence |
|---|---|
| APAC decode | Decoded PCM played to the user offline and confirmed good by ear. |
| Hook fragment expansion | `frag=1920->9216` = 768 frames x 6 voices x 2 bytes. |
| Bytes in deva's DMA ring | `DSP_VERIFY ring=0 source=resmgr voices=6 fragments=62 bytes=1142784 mismatches=0` |
| Consumption order and multiplicity | 62 cursor advances, every delta `0x4800`, each region submitted exactly once, submitted bytes == written bytes (1142784). |
| The submitted stream itself | Reconstructed in submission order and rendered: bit-identical to the known-good reference, PCM SHA-256 `f0f37e6c1f2e5d32d0c938053b9f2f70fbac25868e61f6e1fe2342ae0c90c312`. |
| CSD session descriptor | Built from the live negotiated format, not from stale constants: 48000 Hz, 6 voices, 24-bit in 4-byte containers, 18432-byte buffer, 768 frames, 16 ms, interleaved, channel map `01 02 03 06 08 09`. |
| deva caps record | All 11 fields compared against `mpl1_int_ent`; the two differing `min` fields are correct as capability bounds, but route setup incorrectly reuses them as active geometry (see §5 and §14). |

**Therefore: the PCM byte transport from the APAC decoder through the DMA ring
is measured clean, and the default shared CSD route submits it faithfully.**
Those runs did not execute Audioman's runtime MPL5 attach: in QEMU, MPL5 stays
on the void stream. They therefore did not measure the per-input geometry chosen
when router input 5 is attached. Section 14 closes that gap statically without
reopening any byte-transport result.

---

## 3. Ruled out, and how

### By ear, against synthesised candidates

The known-good six-channel PCM was corrupted in a specific way, folded to stereo, and
auditioned. Renders live in `build/qsa-emu/renders/` (gitignored).

| Candidate | Verdict |
|---|---|
| Hard clipping | not it |
| Low-24-bit extraction, short clip | not it |
| Low-24-bit extraction, 20 s with voice (`dsp_reads_low24_music.wav`) | **far too broken** — peak pinned to full scale, RMS 3.5x; the car stayed recognisable |
| Centre channel into the low-passed path (`voice_into_sub.wav`) | sounds normal — not it |
| Stream order read as context order (`order_stream_read_as_context.wav`) | sounds normal — not it |
| Fragment repeated x3, x4 | too much slowdown |
| **Fragment repeated x2** | **closest match — "невероятно похожи"** |

The x2 repeat is the only positive match ever obtained. Note it reproduces the tick
convincingly but brings its own slowdown, which the driver judged milder in the car.

### By measurement

* Stale stereo geometry in the CSD descriptor — refuted, every field is derived live.
* Ring content corruption — refuted, byte-exact.
* Cursor repeating or half-advancing a region — refuted, single faithful pass.
* deva caps table — refuted, see §5.

---

## 4. The amplifier (`Firmwares/SoundAmp`)

Selectable MOST sinks on the B&O amplifier `8W0035465`:

| Sink | Profile | BlockWidth | Format |
|---|---|---|---|
| `0x21` | Entertainment_2.0 | 4 | 2 x 16-bit |
| `0x26` | Entertainment_5.1_SAD | 14 | 6 x 16-bit |
| `0x2A` | Entertainment_2.0_SAD | 8 | 2 x 24-bit |
| `0x32` | Entertainment_7.1_SAD | 26 | 8 x 24-bit |

Two consequences worth keeping:

* The 5.1 sink is **16-bit**, while deva's six-channel path is 24-in-32 internally. Some
  conversion happens below deva. `mpl1_int_ent` uses the same 4-byte containers, so this
  is not by itself evidence of a fault.
* **Discrete 5.1.2 is impossible on this amplifier.** Positions 7 and 8 are declared
  BCL/BCR; the height codes `0x0D`/`0x0F` appear in no profile. The 8→6 fold on our side
  is mandatory, not an optimisation.

---

## 5. The caps range is correct; its use as active route geometry is not

`mpl5_dio_ent` (CarPlay) versus `mpl1_int_ent` (the device that already runs six
channels). Every field matches byte for byte except two. Both must stay as they
are **as capability minima**:

```
flags 0x2b3   format 0xc   rates 0x80   min/max_rate 0xbb80   max_buffer 0x2d000   max_frags 0xa
min_voices     mpl1 = 6      mpl5 = 2        <- differs, correct
min_fragsize   mpl1 = 0x4800 mpl5 = 0x1800   <- differs, correct
```

**`min_fragsize` cannot be made symmetric.** The timer is a fixed 16 ms, so one value
cannot serve both widths:

| voices | fragment | frames / 16 ms | effective rate |
|---|---|---|---|
| 2 | `0x1800` | 768 | 48000 — correct |
| 2 | `0x4800` | 2304 | 144000 — 3x fast |
| 6 | `0x1800` | 256 | 16000 — 3x slow |
| 6 | `0x4800` | 768 | 48000 — correct |

That is why the fix is a conditional helper keyed on `channel->voices == 6`, not a table
edit. ~~Writing `0x4800` into the table breaks stereo.~~ **Wrong, disproven on the car
2026-09-02:** both minima were raised to the `mpl1` values and stereo still works, because QSA
converts 2->6 rather than refusing the open. That is the shipped fix -- see
`deploy/deva-ctrl-qc/BUILD_CHAIN.md`.

The former conclusion that **`min_voices` is inert for playback was wrong**.
`deva_mib2_channel_map` is one consumer, but
`deva_update_session_info_from_list @0x54848` is the load-bearing second one.
At `0x54a14..0x54a18` it copies record `+0x14` (`min_voices`) to session `+0x84`
as the active CSD channel count. At `0x54a20..0x54a28` it likewise copies
`min_fragsize` to session `+0x6c` as the active CSD buffer size.

The values are not bad caps. The code is wrong to treat lower bounds as the
negotiated format. Raising both minima to the MPL1 values is a valid six-only
diagnostic, but it would reject/break the known-good stereo mode. The production
fix must select the live negotiated voice count and fragment dynamically; see
§14 and [`ROUTER_INPUT_WIDTH_AUDIT.md`](ROUTER_INPUT_WIDTH_AUDIT.md).

---

## 6. Why the earlier emulator runs could not find this

`tools/qsa-emu/` runs the real `io-audio` and the real `deva-ctrl-qc.so` under QEMU, with
only the eight CSD symbols stubbed. That is what made §2 possible.

The lower boundary remains: **CSD is exactly the component that is stubbed**, so
nothing below `csd_write` — the DSP, MOST, the amplifier — can be observed there.
There is also a higher, route-lifecycle limit: without Audioman, a bare MPL5 open
stays `stream_type=1` and never executes the runtime input-5 to output-128 attach.
The six-channel CSD descriptor and submission run therefore exercised MPL1's
default shared-output route, whose static minimum is already six. They could not
expose MPL5's `min_voices=2` route setup.

One artefact to remember: in the harness, `snd_pcm_write_internal` returns `-EIO` after the
first fragment and `snd_pcm_plugin_write` masks it as zero bytes. That is a consequence of
the stubbed DSP never advancing the playback timer, **not** proof the car does the same.
It is still worth measuring on the car, which is why the accounting in §8 exists.

---

## 7. Why CarPlay cannot simply use `mpl1_int_ent`

Tempting, since that device already runs six channels. It does not work:

* Each source owns a **virtual channel** with its own amp connection id (CarPlay media is
  `DIO_MEDIA=162`), and audibility depends on taking focus through
  `dsi.audio.DSIAudioManagement`.
* Writing CarPlay PCM into the internal-media channel while focus sits on CarPlay lands in
  a channel that is ducked or unrouted. Making it audible would require impersonating the
  media source, which breaks ducking, volume and announcements.
* `mpl1_int_ent` has `min_voices == max_voices == 6`, so it cannot open stereo at all.

The device name comes from `get_audio_path(type)` @`0x49398` and reaches
`CAudioDriverIFFactory::create("mpl5_dio_ent")`, so the hook *could* substitute it via the
same PLT/GOT interposition used for `SessionSetup`. Keep this as a plan B only.

---

## 8. The measurement gap — historical, superseded by §§11–21

`frag=1920->9216` in the hook log is `params->buf.block.frag_size` -> `adjusted.buf.block.frag_size`:
**our request**, not what the device granted. The 3x-slow bug was declared fixed on the
strength of that line.

Worse, our own `snd_pcm_plugin_setup` interposer edits the copy it returns to the caller,
so deva's true negotiated geometry has **never been observed on the car**.

At that stage the hook already contained honest instrumentation, but none had run
in the vehicle: the unit carried an older hook with `AUDIO_SPATIAL=0`, and the
dynamic deva build had only been staged. This paragraph is retained as experiment
history; the later reboot, measurements and accepted fixes are recorded below.

| Log line | Answers |
|---|---|
| `device reports voices X..Y` | `2..2` = the deva patch is not loaded; `2..6` = loaded. From `snd_pcm_channel_info`, i.e. the hardware's own limits. |
| `setup actual=<n>ch/<r>Hz/<f>-byte fragment` | What the device actually granted, printed on the success path too. |
| `zeroWrites` / `shortWrites` / `lastWriteResult` | Whether the device accepts whole fragments, or silently refuses them as zero bytes. |

**So the leading remaining possibility is not exotic: the fix for the 3x slowdown may
simply never have run.** That would produce a low pitch and seams at fragment boundaries,
which is what the car reports.

---

## 9. Ground truth is available from the unit

`deva-ctrl-qc.so` ships Qualcomm's own PCM dump, and it can be armed on a running unit:

* `data_dump_path=<path>` is a documented driver command-line key; files land as
  `<path><name>_<n>.bin`.
* `deva_mixer_switch_data_dump_set` calls `deva_data_dump_ctrl`, so dumping is a **mixer
  switch** — it toggles at runtime, with no `io-audio` restart and therefore no reboot.
* `deva_timer_data_dumps` records what the playback timer actually consumes;
  `csd_data_dumps` / `csd_sad_dumps` sit at the CSD boundary.
* Symbols: `deva_data_dump_init @0x2fd0c`, `deva_data_dump_ctrl @0x30c88`.

This is the one capture that crosses the boundary the harness cannot, on the real unit with
the real amplifier in the loop.

---

## 10. Next steps, cheapest first

1. **Control test, costs nothing.** Play a real 5.1 file from internal media, bypassing
   CarPlay entirely. Ticking and low voices there too means the fault is in the shared
   six-channel path and has nothing to do with CarPlay; clean playback means it is specific
   to `mpl5_dio_ent`. This has never been done and it halves the problem.
2. **Deploy the current build and connect once.** The three log lines in §8 report whether
   the deva patch is loaded, what geometry was actually granted, and whether writes are
   accepted. All three are already written and have never executed in the car.
3. **Arm the stock dump** (§9) and listen to what deva really plays.

Deploying files is fine; restarting `io-audio` or `j9` is not — always ask first.

---

## 11. The decisive measurement — 2026-09-02

### What was done

`tools/qsa_play.c`, built for QNX and deployed to `/mnt/app/root/hooks/qsa_play`, plays a raw
interleaved file into a named QSA device and **measures** the achieved frame rate rather than
leaving it to the ear. Its input was `session5_5_1_declared_order.wav` stripped to raw PCM:
the decoded APAC capture from this car, six channels, 48 kHz, already confirmed clean on a Mac.

Two details made it work where the earlier attempt failed. `snd_pcm_plugin_set_disable(pcm,
PLUGIN_DISABLE_MMAP)` must be called before `snd_pcm_plugin_params`, exactly as RetroArch's
proven `qnx_qsa.c` does, or params fails with ENOENT on this firmware. And the device only
provisions while its source holds the amplifier focus — pausing CarPlay leaves `DIO_MEDIA`
focus held, which is what made `mpl5_dio_ent` openable. `mpl1_int_ent` reports
`voices 0..0 formats 0` while focus sits elsewhere.

### Result

    device limits: voices 2..6 rate 48000..48000 frag 6144..18432 formats 0x1000
    device:        6 voices, 48000 Hz, frag 9216, frags 10
    TOTAL 2445312 frames in 50822 ms -> 48115 frames/s (expected 48000)

Correct geometry, correct clock — and **it sounded broken in exactly the way CarPlay sounds
broken**: constant ticking, wrong pitch, recognisable content. Confirmed by ear on the
channel-ID test track, which announced "Front Right".

Meanwhile RetroArch (`Tools/retroarch-qnx`) writes six voices to `/dev/snd/mpl1_int_ent` on
this same unit and sounds correct.

### What it means

Everything in the CarPlay stack is exonerated: libairplay, the APAC decoder, the ring, the
hook, the 2->6 expansion, the type-103 path. Sections 8 and 10 were written before this and
their priorities are superseded.

The shared six-channel path is also exonerated: deva, the DSP, MOST and the amplifier are the
same for both devices, and `mpl1_int_ent` is clean. What remains is the **CarPlay virtual
channel** itself.

### The hypothesis this lands on

`docs/reference/AUDIO.md` always described two halves. The deva caps patch is done and
confirmed loaded (`voices 2..6` read from the hardware). The second half — the B&O / mixer
agent provisioning the CarPlay virtual channel for six voices toward the amplifier — was
never done; stock provisions it as stereo.

Against the amplifier's sink table (§4), a stereo sink means the amplifier reads 4- or 8-byte
frames out of a 14-byte-wide stream. The frame boundary slides continuously, which is audible
as ticking, a wandering pitch, and recognisable audio from the wrong speakers — every reported
symptom, from one cause.

Next step is therefore to find which component issues the amplifier's MOST Connect
(`FBlock 0x22, FktID 0x0112, StartResult`) and which `SinkNr`/`BlockWidth` it uses for
connection id 162. Brief: `docs/CODEX_PROMPT_DIO_MEDIA_SINK.md`.

### Left on the unit

`qsa_play` at `/mnt/app/root/hooks/`, and `test51.s16` plus FLAC/AAC/WAV copies under
`/fs/sda0/AUDIOTEST/`. This is the only tool in the whole investigation that produced an
unambiguous answer: it plays arbitrary audio into an arbitrary device with no CarPlay in the
loop. Reach for it first next time.

At that point `AUDIO_SPATIAL` was back to `0` on the unit and in the repo. This
state is superseded by the accepted fixes and current `[65,23]` policy in §§19–21.

---

## 12. Static result: DIO media already selects the 5.1 amplifier sink — 2026-09-02

The follow-up static audit falsified §11's stereo-MOST-sink hypothesis. For DSI connection
`162` and amplifier terminal `1`, MIBRoot maps the external-amplifier/B&O branch to
`eSRC_ENT_AMP_DIO_MEDIA_MC` plus `eSNK_DTCP51`. The amplifier's corresponding physical
profile is `SinkNr 0x26`, `BlockWidth 14`, protected 6 x 16-bit PCM.

Section 16 later separates that advertised sink profile from the controller-supplied
Connect field. The latter is `BlockWidth 26`, not 14.

This is a static connection/coding table, not a value derived from QSA voices. No sink patch
is needed or justified: the relevant MIBRoot word is already `eSNK_DTCP51`. The detailed
addresses, raw table records, ownership chain, and binary hashes are in
[`DIO_MEDIA_SINK_STATIC_AUDIT.md`](DIO_MEDIA_SINK_STATIC_AUDIT.md).

What this changes: the “stock provisions DIO media as stereo” statement in §11 was a
hypothesis, not an established fact, and is now superseded. The wrong-amplifier-sink geometry
cannot explain the symptom on the stock external-amplifier route. This result does **not** by
itself locate the remaining DIO-channel defect.

---

## 13. Device-topology result: MPL1 and MPL5 share CSD device 78 — 2026-09-02

The next static-plus-QEMU audit also produced a clean negative. `mpl1_int_ent`
and `mpl5_dio_ent` are mixer-router inputs, not separate CSD/ACDB devices. ENT
inputs 1 and 5 both resolve through output 128 (`TDM_OUT_MPL`) to the same CSD
playback session and OEM device id `0x4e` (78). Its established stream/context
geometry is 48 kHz, 24-bit, six channels with map `01 02 03 06 08 09`.

`CSD_DEV_CMD_ENABLE` has no channel-count field: it carries only opcode,
`dev_id`, rate and bits/sample. Six-channel geometry is set separately on the
stream and context from the live session voice count. On a CSD-backed start,
deva re-sends the TDM SAD configure request and
`ENABLE(78, 48000, 24)`; there is no retained per-`mpl5` stereo device
configuration. Section 18 later qualifies the SAD half of that sentence: the
device configuration is shared, but the SAD metadata itself is persistent and can
retain a two-active-channel ASAD mask independently of the CSD stream width.

An isolated QEMU open explains the one apparent asymmetry. MPL1 has a default
boot route to output 128, enters `stream_type=2`, and produces device
configure/enable/start traffic. MPL5 has no default route; without the vehicle's
Audioman it stays `stream_type=1`, which deliberately discards buffers, and
produces no CSD traffic. In the vehicle the runtime mixer route attaches MPL5 to
the same output session.

Therefore a different CSD device id, device-side stereo channel field, or
per-input ACDB selection is not the cause. The next measurable difference is
the dynamic Audioman route lifecycle (boot-attached MPL1 versus runtime-attached
MPL5), followed by processing inside the ADSP/calibration boundary if that
route emits the same device traffic. Full addresses, payloads, run hashes and
namespace table: [`DEVICE_TOPOLOGY_AUDIT.md`](DEVICE_TOPOLOGY_AUDIT.md).

The “next measurable difference” statement above records the conclusion of
that audit and is superseded by §14, which resolves the route lifecycle
statically.

---

## 14. Root-cause candidate found: MPL5 route setup reuses stereo minima — 2026-09-02

The controlled same-device follow-up measured 48,107 frames/s at two voices
(clean) and 48,129 frames/s at six voices (broken), both against 48,000
expected. That isolates width from device, content, and sample clock.

The router-width audit found the missing distinction inside deva. Inputs 1 and
5 do share output 128 and CSD device 78, but the shared session is rebuilt from
the attached timer's static record. `deva_update_session_info_from_list
@0x54848` copies:

```text
record min_voices    (+0x14) -> session active channels (+0x84)
record min_fragsize  (+0x1c) -> session CSD buffer      (+0x6c)
```

For MPL1 those values are `6 / 0x4800`; for MPL5 they remain `2 / 0x1800` so
stereo still opens. The current dynamic patch changes MPL5's active DMA
fragment to `0x4800` when the negotiated PCM channel has six voices, but it
does not update or rebuild the already-attached CSD session. Acquire, prepare
and trigger contain no such re-provisioning.

The resulting six-voice geometry is therefore:

```text
DEVA timer advances:       0x4800 bytes / 16 ms, six-channel S32
CSD stream alloc/copy:     0x1800 bytes / 16 ms, configured as stereo
```

`session_csd_stream_playback @0x69084` copies and submits exactly session
`+0x6c`. It takes only the first third of each 768-frame six-channel fragment,
skips the remaining 512 frames when the timer advances, and presents the copied
words as 768 stereo frames. With six correlated input channels, every source
sample is repeated three times inside the block and the next 16-ms block jumps
ahead. That preserves total wall-clock duration while producing low-pitched
content and a 62.5-Hz discontinuity — the exact unusual combination measured
in the car.

This width mismatch is statically proven and is the leading root cause. Its
acoustic completion still needs a controlled A/B. A six-only diagnostic can
set MPL5 `min_voices 2->6` and `min_fragsize 0x1800->0x4800`; that is not a
production fix because it removes the clean stereo mode. The correct fix must
derive CSD channels/buffer size from the live PCM channel and rebuild the CSD
stream/context/buffer pool on a 2<->6 transition before playback starts.

Full call chain, addresses, record bytes, symptom math, harness scope and fix
constraints: [`ROUTER_INPUT_WIDTH_AUDIT.md`](ROUTER_INPUT_WIDTH_AUDIT.md).

This candidate was subsequently fixed and confirmed on the vehicle. Section 15 records
the result and the next, narrower delivery boundary.

---

## 15. Rendering fixed; HU selects MC; logical SHARC matrix recovered — 2026-09-02

Mirroring MPL5's attach-time caps record to MPL1 fixed the six-voice rendering defect on
the car. The unit now logs `channels=6` at route attach, the continuous 62.5 Hz tick is
gone, and both discrete 5.1 and stereo play. The deployed sequence is documented in
[`../deploy/deva-ctrl-qc/BUILD_CHAIN.md`](../deploy/deva-ctrl-qc/BUILD_CHAIN.md).

A subsequent standalone one-hot test wrote six-channel PCM directly to
`/dev/snd/mpl5_dio_ent`, with no CarPlay in the path, at 48,243 frames/s against 48,000
expected. Source positions L and R were audible; C, LFE, Ls and Rs were silent. This is
now a channel-delivery problem after correct six-channel rendering.

The MC-selection audit recovered the exact head-unit condition. Connection 162 has two
table outcomes: internal coding selects `eSRC_ENT_AMP_DIO_MEDIA + eSNK_SPEAKERS`, while
external coding selects `eSRC_ENT_AMP_DIO_MEDIA_MC + eSNK_DTCP51`. The only selector is
`Amp.Amplifier.ExternalAmplifierCoded`, derived from RCC EEPROM/basic-coding field
`Sound_System` at E2P record `0xF1`, byte 11 (`1=internal`, `2=external`). On this B&O
external-amplifier car, the MC pair is unconditional.

Neither `DSIAudioManagement.requestConnection` nor the CarPlay service configuration
contains a source channel count. The request is connection ID, terminal and group; QSA's
six-voice negotiation does not feed the MIBRoot table mapper. Therefore no HU policy or
sink-selection patch is justified by the one-hot result.

The next static pass recovered the logical SHARC input mixer from `AD301A07` in the
available V850 CodeFlash. Sink `0x26` selects profile 0/bank 1, an 8×8 signed-dB matrix
written to SHARC parameters `0xC022A..0xC0269`. Its C/LFE/Ls/Rs rows all have non-muted
routes, including contributions to the already-audible L/R buses. Therefore this static
matrix does not match the one-hot result and is a clean negative for matrix-row muting.

The vehicle result plus those non-muted routes closes matrix-row muting as an explanation:
C/Ls/Rs would contribute to the already-audible L/R buses if they reached the amplifier
matrix. Their total silence puts the loss upstream of that matrix. Full coefficients,
selector code and reproducible CSV:
[`SHARC_INPUT_OUTPUT_MATRIX.md`](SHARC_INPUT_OUTPUT_MATRIX.md). Head-unit addresses and
the runtime coding chain remain in [`MC_SELECTION_AUDIT.md`](MC_SELECTION_AUDIT.md).

---

## 16. MOST source allocation is 26 bytes, not stereo width — 2026-09-02

The follow-up static audit separates source allocation from sink declaration. On the
external-amplifier branch, logical source `eSRC_ENT_AMP_DIO_MEDIA_MC` (`0x46f`) maps to
`DSI:MmxSourcePresCtrl/7`. Source 7 has `srcID 52` and a fixed bandwidth/BlockWidth of
`26` bytes per synchronous frame. AudioProcess preserves that value through source
allocation and serializes it into the MOST Connect request.

The actual stock Connect fields for connection 162 are therefore:

```text
SinkNr 0x26, BlockWidth 0x001a
payload offsets 2..4: 26 00 1a
```

The amplifier's sink `0x26` still advertises `BlockWidth 14` in `SinkInfo.Get`. That is a
separate sink-profile record; the earlier audits incorrectly inferred it was also the
Connect value. The observed source/sink mismatch (`26` requested, `14` advertised) is real,
but the follow-up V850 audit proves that the amplifier's Connect handler does not reject it.
`FUN_000C4B30` checks only that the width is in `1..372`, then stores the received value;
it never compares against the selected profile's own width.

The later AFE audit proves the physical packing which was still unknown here. Device 78 uses
six three-byte slots at byte offsets `2,5,8,11,14,17` in a 32-byte shared TDM frame; the
32-bit words exist only on the internal deva/LPASS side. Therefore the former
`2 + 6*4 = 26` explanation is wrong for the TDM wire layout. Qualcomm's MMXF platform table
independently confirms eight potential positions at `2,5,8,11,14,17,20,23`, so
`2 + 8*3 = 26` exactly covers their range. The exact Mmx-source-to-AFE-group binding remains
an inference. See §18.

This is a clean negative for the proposed stereo-width MOST allocation. The decisive next
measurement is the one-hot signal at Qualcomm/LPASS `TDM_OUT_MPL` and in the allocated MOST
synchronous slots. Missing C/Ls/Rs already at `TDM_OUT_MPL` establishes loss in Qualcomm
ADSP routing/packing after deva; presence there but absence on MOST moves it to the
INIC/MLB/MOST formatter. Details, addresses, table bytes and hashes:
[`MOST_SOURCE_BLOCKWIDTH_AUDIT.md`](MOST_SOURCE_BLOCKWIDTH_AUDIT.md).
Amplifier instructions, exact HU constant sites, and the 14/20/26 reconciliation:
[`CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md`](CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md).

---

## 17. CSD/ACDB boundary: six-channel fields, stereo COPP topology — 2026-09-02

The previously unopened `libcsdIpcClient.so`, `audio_service`, platform OEM binding,
ACDB files and signed LPASS image were audited for a two-channel cap on CSD device 78.

The direct counts are not stereo. The IPC client passes the caller's six-channel map
unchanged; `audio_service` accepts up to eight channels; dev 78 has ACDB
`ChannelConfig=6`; its newer slot-I2S property declares six 24-bit channels at offsets
`2,5,8,11,14,17`; and both ADM COPP-open packet versions send
`dev_num_channel=6`. The legacy scalar named `slot_channel_allocation` is not present for
this device because it uses the newer explicit-offset property.

The audit found one topology distinction. For all four application modes,
`Speaker_cal.acdb` selects topology `0x10314`. Qualcomm's public downstream definitions
name that ID `SPEAKER_STEREO_AUDIO_COPP` (also the default COPP). The name alone does not
prove that this signed implementation narrows the stream.

The internal LPASS registry first sharpened the result. `0x10314` and proposed replacement
`0x10be3` both point to the same RX descriptor `0xc04e981c`, an 11-module graph, so that
four-dword ACDB edit is not a meaningful graph A/B. `0x10312` points to a genuinely
different three-module RX graph, but no stock six-channel device uses it; the widest
stock user is three channels.

The subsequent Hexagon disassembly closes the static module question. Resampler,
delay, SoftVolume, ComboPP, RX gain and Sample Slip accept up to eight channels.
PBE, QEnsemble and SPA are stereo-only, but they return a media-format failure on six
channels; GenTopo disables that individual module and continues. Device-78 ACDB also
sets QCPR, PBE, channel IIR and MBDRC enable values to zero. `audproc_svc` preserves
the configured six-channel output count and map, and its generic downmixer explicitly
bypasses `5.1 -> 5.1`.

Consequently, the `SPEAKER_STEREO_AUDIO_COPP` name is **not the last proved 6ch fault**.
No static `6 -> 2` operation remains in the stock COPP path. Variant `0x10312` is
statically six-channel-safe, but is publicly NULL/NONE COPP and remains experimental.
The better graph A/B is `0x1031b` (`SPEAKER_MCH_PEAK_VOL`): resampler, Peak Meter and
SoftVolume, all accepting eight channels. Section 18 subsequently proves the static AFE
slot packer and finds a separate two-channel SAD/ASAD declaration at the same boundary.

Full property records, file offsets, ARM instructions, packet layouts and hashes:
[`CSD_ACDB_CHANNEL_AUDIT.md`](CSD_ACDB_CHANNEL_AUDIT.md).
Hexagon topology records, module validators, ACDB enable values and output-media code:
[`LPASS_COPP_TOPOLOGY_AUDIT.md`](LPASS_COPP_TOPOLOGY_AUDIT.md).

---

## 18. AFE/TDM slots are correct; SAD can still declare only L/R — 2026-09-02

The signed LPASS image closes the physical slot question. CSD device 78's ACDB property is
the Qualcomm explicit slot-map parameter: six 24-bit slots at **byte** offsets
`2,5,8,11,14,17`. `audio_service` forwards the record unchanged. LPASS validates all six,
uses a 32-byte shared group frame, and its TX packer loops over six input samples.

For each 24-bit slot the packer arithmetic-shifts the internal 32-bit word right by eight
and writes three bytes. Since QSA/deva left-align S16 in bits 31..16, the TDM result is the
original signed 16-bit sample in the high 16 bits of a 24-bit slot with eight zero low bits.
This is a deliberate 16-in-24 representation, not low-24 extraction or a 256x gain error.

The important new result is a second, independent description carried in the custom
TDM/SAD header. It contains `sample_width` and ASAD, the active-speaker bit mask. LPASS
parses the mask and records its popcount as `num_chan_from_header`, but does not use it to
shorten the six-slot PCM copy loop.

The persistent MPL session defaults to `ASAD=0xfc` (six active positions) and width 24.
However, AirPlay explicitly sends `DCMD_PCM_SET_SAD_METADATA` using its original input
geometry. The existing trace shows:

```text
createMetadata nChannels:2, nBitsPerChannel:16
convert_sad_header_to_csd: ... asad=0xc0 ... sample_width=0x10
TDM SAD Header Payload: 0x3c01, 0xb210, 0x0, 0xc0, ...
```

A saved vehicle slog for the known-good six-channel RetroArch/MPL1 path provides the
counterexample: it sends `ASAD=0xfc`, `sample_width=0x18`, and payload
`0x3c00, 0xb218, 0x6000, 0xfc, ...`. The two header states therefore occur in real
starts and distinguish the clean six-channel path from the observed stereo metadata path.

The PCM hook widens the QSA stream but does not modify this devctl. The standalone
`qsa_play` does not send SAD metadata at all, so it inherits the session default or the
previous writer's value. Consequently, the car's direct one-hot test did not control this
lower channel declaration: it could have sent six PCM slots while declaring only L/R active.

This is the leading remaining HU-side mechanism, not yet the final hardware conclusion.
The unavailable INIC/MOST formatter is the consumer whose response to inactive ASAD bits is
not visible statically. The smallest decisive vehicle check needs no MOST analyser: capture
the existing `convert_sad_header_to_csd` line during one six-channel start. `ASAD=0xc0`
establishes the mismatch on our side; `ASAD=0xfc` clears it and leaves the honest boundary
after LPASS packing.

Exact ACDB bytes, 32-byte frame arithmetic, ARM/Hexagon function addresses, existing slog,
width reconciliation and provenance:
[`AFE_TDM_MOST_SLOT_AUDIT.md`](AFE_TDM_MOST_SLOT_AUDIT.md).

---

## 19. Vehicle closure: SAD and feedback now match the stream — 2026-09-03

The leading §18 mechanism was real. The active stream contained six valid PCM
slots, but the SAD sideband still described the original two-channel source. The
fixed path sends the six-channel SAD/ASAD geometry with the widened stream. With
that declaration corrected, C/LFE/Ls/Rs are no longer discarded before the
amplifier matrix.

A second independent control mismatch was also real. The hook's experimental
`updateFeedback` rewrite replaced the stock stream list with an empty list. That
removed the clock/rate reference used by the sender: delivery collapsed to roughly
one packet per second. The production behavior now preserves the stock feedback
response; the vehicle reports a clock reference of `sr=48004.0625`, type 100, and
normal packet delivery.

After both corrections, discrete 5.1 CarPlay plays correctly end to end. This
accepts and closes the earlier byte transport, CSD topology, AFE slot packing, MOST
allocation, amplifier sink selection and SHARC matrix audits as clean boundaries.

At this intermediate stage the remaining symptom was content selection: the
type-103 entry offered only index 65, so ordinary stereo had no selectable format.
The iOS enumeration then exposed the stereo candidates. Section 20 supersedes this
state: index 23 was selected, the AAC bridge landed, and `[65,23]` now switches on
the vehicle in both directions. The original enumeration evidence remains in
[`APAC_FORMAT_INDEX_AUDIT.md`](APAC_FORMAT_INDEX_AUDIT.md).

---

## 20. Buffered AAC-LC static closure and session policy — 2026-09-03

The car selected extended index 23 (`AAC-LC/48000/stereo`) when it was offered.
Its APAP time scale was `0x0000bb80`, and the setup delivered a distinct 39-byte
magic cookie. This supersedes index 22 as the production candidate: index 23
matches the HU's 48 kHz endpoints and needs no resampling.

Static analysis on the MHI2Q MU1316 binaries recovered the complete Cinemo call
boundary. `libNmeSDK.so` creates a fresh audio-codec wrapper; the registered
`libNmeAudioAAC.so` plugin accepts media type 5/subtype `0x101a`, expects a raw
MPEG-4 AudioSpecificConfig, and returns 48 kHz stereo S16 through a callback.
The working MH2p adapter proves one AAC access unit per `Write` call with trailing
arguments `(1, pts=0, 1)`. MH2p is an ABI oracle only; the implementation target
and all library hashes in the plan are MHI2Q.

The subsequent vehicle capture closes the framing boundary directly. It retained
601 raw AUs (376..1322 bytes), plus the complete 39-byte MPEG-4 descriptor chain.
Tag 5 contains ASC `11 90`; adding ADTS headers without stripping or changing an
AU byte produces 12.821333 seconds and exactly `601 * 1024` frames of standard-
decoded 48 kHz stereo PCM. The inline APAP TLVs repeat the cookie ID and complete
cookie with every media item, so track configuration does not depend on
SETMAGICCOOKIE ordering. A later byte-level two-session comparison corrected the
field map: APAP field 2 is the active `APAudioFormatIndex` (`23` for AAC, `65`
for APAC), while field 8 is `spf=1024` in both. The iOS APAP encoder confirms
the same ownership.

The remaining Cinemo coexistence concern is also closed statically. `dio_manager`
does not import `CinemoCreateAudioCodec`; it shares only stock Config/Metapool/iAP
state. The SDK creates a fresh wrapper, AAC decoder and PCM output object, while
the shared class/plugin registry is mutex-protected and reference-counted. The AAC
plugin has no OMX imports or relocations despite its nominal dependency, and the
complete AAC-to-application callback chain is synchronous on the calling thread.
One decoder can therefore be owned by the existing APAP receiver thread after
stock initialization; no new thread or callback wait is required. Real-time CPU
margin remains a bounded stationary canary, not a static discovery question.

The apparently session-constant format in the early `[65,23]` run was an
instrumentation error: it read `spf` as an index. iOS's active buffered engine
does reselect transport when the source ASBD changes, but the receiver observes
that change through the new inline cookie, not a packet index. It tries the
current decoder and then its alternate; the successful decoder plus decoded
channels/rate/layout selects stereo upmix or APAC 5.1 reorder. The unmeasured
APAC PCM8/5.1.2 policy additionally requires the explicit legacy SETUP=66 gate.

This cookie-driven APAC <-> AAC switching has now played in both directions on
the vehicle inside one type-103 session with no dropout or new underrun. Host
regression also pins SETUP independently at 65 and 23 and verifies both action
orders, native reorder and fresh upmix state after every cookie boundary. See
[`DYNAMIC_BUFFERED_CODEC_SWITCH.md`](DYNAMIC_BUFFERED_CODEC_SWITCH.md).

The exact framing/cookie confidence ledger, C ABI, callback/thread restrictions,
index-22 resampler fallback and ordered risk gates are in
[`AAC_BUFFERED_PATH_PLAN.md`](AAC_BUFFERED_PATH_PLAN.md). The Cinemo object,
registry, callback and OMX proof is in
[`CINEMO_AAC_THREAD_SAFETY_AUDIT.md`](CINEMO_AAC_THREAD_SAFETY_AUDIT.md). The
checked-in supervisor now uses the accepted `[65,23]` advertisement with
`AUDIO_SPATIAL=1`; stereo DSP remains independently reversible through
`AUDIO_STEREO_UPMIX=off`.

---

## 21. Amplifier stereo/5.1 matrix and live ASAD — 2026-09-04

The amplifier does observe the incoming SAD allocation dynamically. V850
`FUN_000D1D94` polls the INIC, stores a changed allocation at `gp-0x3E21`, and
queues task-9 event 15. The event re-evaluates the selected A07 bank and rewrites
the SHARC input-matrix groups.

The target sink `0x26` has record `01 A5 10 00`: raw bank 1, SAD-capable,
16-bit. Raw bank 1 is not remapped, so `ASAD=0xc0` and `ASAD=0xfc` both retain
bank 1. ASAD-based bank 0/1/2 switching exists only on raw-bank-2 sink profiles
such as `0x32`.

Bank 0 is not the advertised “clever stereo upmix” in the recovered 8x8 layer.
It passes L to Bus L and R to Bus R and folds any other populated input positions
into those two buses. Profile-0 bank 1 maps L/R to their own buses at -2 dB and
also to Bus C at -6 dB per side; it has no direct L/R-to-surround route.

There is nevertheless a second, proven DSP-mode difference. A bank refresh
writes SHARC control `0x10` from `0x6EAA0`: bank 0 sends float `2.0`, while banks
1 and 2 send float `5.0`. The image supplies no semantic name, so this cannot be
called a Symphoria switch until the extracted SHARC program is semantically
disassembled or runtime evidence identifies it. The extraction is documented in
[`SHARC_APPLICATION_EXTRACTION.md`](SHARC_APPLICATION_EXTRACTION.md).
On sink `0x26`, changing only ASAD keeps the value at `5.0`.

Consequently the exact compatibility/off baseline `L,R,0,0,0,0` is valid PCM,
but announcing `ASAD=0xfc` does not preserve the fact that its source was stereo. A separate-start
`0xfc` versus `0xc0` A/B test can isolate allocation-aware behavior, but neither
case enters the true bank-0/control-`2.0` mode on sink `0x26`. That mode requires
a different stereo sink or a raw-bank-2 connection, which changes the MOST
contract and is not a codec-hook-only edit. The adopted shipping policy is therefore
one fixed `PCM6/48 kHz/S16` stream with `ASAD=0xfc` for the whole buffered session;
only the APAC/AAC decoder changes between tracks. There is no live amplifier/SAD
reconfiguration at a codec boundary. Full audit:
[`AMP_STEREO_5_1_MATRIX_DIFF.md`](AMP_STEREO_5_1_MATRIX_DIFF.md).

The resulting head-unit upmix design is recorded separately in
[`STEREO_TO_5_1_UPMIX_DESIGN.md`](STEREO_TO_5_1_UPMIX_DESIGN.md). Its sole
profile leaves C zero, routes bounded separated objects into Ls/Rs, applies a
fixed Ls/Rs-only +3 dB high shelf with a 4-kHz midpoint before the linked rear
limiter, and uses Cabin-Aware LFE V2: a phase-safe 48-Hz LR4 sub-band, V11 bass/drum
steering, a Q15 `4096` ceiling, and a `0.958` paired front trim. Its
per-sample limiter accounts for every recovered J525 bus gain, including LFE
leak and the bank-1 +6 dB surround gain. The design also records Pioneer's independently verified CarPlay
stereo-to-5.0 engine as a host oracle/clean-room rear-generator candidate; its
explicit center is deliberately not mapped because bank 1 already derives the
same mid signal.

The retained `hssr-universal` renderer and its Cabin-Aware LFE V2 adapter are
implemented after the common decoder and before the PCM6 ring. Analysis,
masking and synthesis remain fixed-memory C99 on the existing receiver thread;
only its quantized GRU controller runs on a persistent lock-free worker.
The final rear-presence shelf voices only the already separated Ls/Rs signal;
the dry-front object-transfer sidechain remains pre-EQ, so this does not create
a second separation profile or copy full-band front audio into the rears.
Generation-tagged results are rejected after every stream/flush/rate/cookie/
format boundary, and native APAC 5.1 remains on its byte-identical reorder
path. The old backwards stereo widening was
also replaced by bounded forward chunks, closing its latent overflow when
Cinemo returns more than one 1024-frame callback. Host suites, ASan/UBSan and
QNX ARMv7 builds pass. The checked-in vehicle policy now explicitly selects
`AUDIO_STEREO_UPMIX=on`, rear `0.25`, and
`AUDIO_STEREO_LFE=assist`; exact rollback is `AUDIO_STEREO_UPMIX=off` followed
by a `dio_manager`/CarPlay service restart. This remains an A/B candidate: the physical
sub polarity/crossover, the narrow Center-bus true-peak margin and codec-switch
clicks need a stationary car A/B. It must not be described as the final quality
winner. Exact implementation, metrics, center/LFE rationale and the host WAV
renderer are in the linked design document.

HQ uses a causal 1024-point sqrt-Hann STFT at hop 256, 24 Bark-like smoothed
covariance bands, a confidence-weighted principal-image subtraction, a
120 Hz--7 kHz rear mask, and the same unequal Schroeder/linked-limiter output
adapter. Its rear latency is exactly 1024 frames/21.333 ms; dry fronts and LFE
remain undelayed. In the retained pre-HSR-TD build, state/runtime sizes were
`59432/55304` bytes, the QNX
process frame is 208 bytes, and object text/unstripped size is `8180/72904`
bytes. Current host timing for 1024 frames is mean/p99
`144015/162042 ns`. QEMU icount is mean/p99
`4034828/4035856` estimated retired ARM instructions, not
Krait cycles or a realtime proof. The final retained fold renders very quiet
Ls/Rs: peak `-40.398/-43.864 dBFS`, RMS `-68.918/-71.926 dBFS`; a stereo
channel swap swaps those RMS values, confirming source-relative rather than
fixed-left leakage. That level and image must be judged in the stationary A/B,
not promoted to a quality conclusion from static metrics.

The ordinary AAC capture `spatial_aac.wav` instead measures HQ Ls/Rs RMS
`-37.827/-37.436 dBFS`, versus lite `-39.803/-40.183 dBFS`. Together the rows
show content-dependent selectivity, not a constant HQ rear level.
The final delay-allpass denormal snap zeros output/feedback below `1e-20`;
normal-level PCM checksums are unchanged, while QEMU's estimated instruction
mean/p99 rose about 1.44% relative to the preceding stable-eigen build.

HSR-TD updates real per-band covariance on every sample and refreshes its
principal projector at a 6 kHz control rate. Positive coherent sources remain
in dry L/R; the unexplained residual goes directly to Ls/Rs without FFT,
Schroeder decorrelation, or block-fill latency. The implementation is an
independent algorithm in the broad public time-domain HSR class, not a DAM
Upmix reproduction or parity claim. With QNX GCC 8.5, calibrated QEMU icount is
mean/p99 `1089350/1091467` estimated retired instructions per 1024 frames,
3.62x fewer mean instructions than HQ in the same build. Its tuned covariance
attack/release is about 1 ms and 8 ms; a new transition regression shows the residual
rear tail in the 20--100 ms interval after diffuse audio becomes mono falling
from -48.07 to -61.92 dB, with exact mono still digital zero.

Nine development genres plus 24 held-out multitrack fold-downs give HSR-TD
held-out rear waveform correlation `0.395` and SI-SDR `-8.20 dB`, versus
`-0.010/-14.98 dB` for lite, but lite retains better envelope, image-error,
energy-restraint, and fold-back figures. The shared LFE assist scores held-out
reference-envelope correlation `0.603`, SI-SDR `1.13 dB`, energy ratio `0.769`,
and maximum visible recovered SoundAmp bus peak `0.503`. It follows bass in
L/R; it cannot recover discrete LFE omitted from the stereo fold. These mixed
metrics did not establish a universal winner; the user subsequently selected
`hsr-td` for the vehicle A/B, so startup now carries it without claiming a
quality conclusion. Full data and limitations are in
[`../tools/stereo-upmix-render/bench/results/2026-09-04-hsr-td.md`](../tools/stereo-upmix-render/bench/results/2026-09-04-hsr-td.md).

---

## 22. Phone-lock/background lifecycle — measured 2026-09-04

The reported stop on iPhone lock or Apple Music backgrounding is not an HQ DSP
regression: the unit still ran the 461990-byte hook with `ambient-lite`.  A live
trace showed stock type 100 releasing `mpl5_dio_ent` while type 103 continued to
decode.  The ring reached 24000 frames and dropped data; at the same boundary
stock `updateFeedback` changed from
`{streams:[{type:100,sr:48003.5546875}]}` to a null response.  About eight
seconds later iOS sent rate zero, flush and explicit type-103 teardown.

The fix transfers the already configured QSA handle to the existing APAP
thread during that gap and reuses the exact last measured feedback plist for
the same active session.  It opens no second QSA handle and adds no thread.  A
separate QSA I/O mutex also closes the old state-lock re-entry debt, and the
62-Hz setup-log flood is removed.  Host tests and the QNX ARMv7 build passed.
The deployed vehicle test then survived lock/background and the return to the
foreground: 2412 detached writes completed with no zero/short write, the drop
counter did not grow, measured feedback continued, and iOS sent no rate-zero,
flush, or type-103 teardown.  A brief transition jump was audible, but playback
continued normally; the former stop is therefore fixed.  Full evidence,
ownership rules, artifact hashes and the vehicle trace are in
[`BUFFERED_BACKGROUND_LIFECYCLE.md`](BUFFERED_BACKGROUND_LIFECYCLE.md).

The longer trace later isolated one nonfatal 256-frame synthesized zero tail
during an explicit iOS `rate=0`/flush AAC-to-APAC transition.  A follow-up
whole-period patch now uses either all 768 buffered frames or the realtime
fallback for the entire QSA period; it cannot concatenate buffered audio with
a zero tail.  New `shortRingFallbacks`, `avoidedSilenceFrames`, and `lastReady`
telemetry make that decision visible while `underrunFrames` becomes a strict
zero invariant.  A vehicle lock/unlock trace then held every one of those
counters at zero while the brief interruption remained, ruling the ring-period
policy out for that symptom.

The same trace localized the interruption to QSA state changes.  Stock's only
two `snd_pcm_plugin_flush` callers in the accepted `libairplay.so` are the
close methods at `0x47744` and `0x48a94`, each immediately followed by close at
`0x4774c`/`0x48a9c`.  The first handoff retained that flushed handle and had to
prepare it; foregrounding then closed it and performed a new
open/params/prepare.  The deployed experimental follow-up suppresses only that
close-adjacent playback flush while type 103 is active and hands the same live
handle back to the new stock object.  Compatible duplicate params/prepare are
acknowledged without touching QSA; real setup still validates the physical
geometry, and any mismatch falls back to the real calls.  This adds neither a
thread nor a second device handle.  Host, sanitizer, integration and QNX builds
pass.  The first vehicle run used one unchanged handle through lock, unlock and
a second lock and was initially reported audibly excellent.  Its only zero write followed
two explicit iOS `FLUSHBUFFERED` requests and status 4/QSA underrun; every later
write was full.  A built micro-follow-up now performs real prepare immediately
for that status and skips it only while QSA is still running.  Exact hashes,
instruction evidence, trace and deployment state are in the lifecycle document.

A later full connection restart changed the acceptance state.  PID `72769671`
performed orderly APAP/QSA/alt-screen teardown with full detached writes and
final reason `-6753` (`kConnectionErr`), produced no new core, and was replaced
by PID `85991498`.  This is not evidence of a hook segfault, but it leaves the
same-live-handle experiment under audit.  In the replacement session,
next/previous tests isolated a separate definite defect: a SETUP negotiated at
AAC-LC index 23 was answered with APAC's `audioBufferSize=21168000` because the
buffer-family helper recognized only old AAC indices 40/42.  AAC indices
22/23/40/42 now select the receiver's recovered 5,000,000 default; APAC
64/65/66 retain 21,168,000.  Measured AAC flush-boundary waits were 1.097 s and
0.668 s versus 0.085 s for APAC. The corrected build passed the host and QNX
tests, was deployed with byte-identical readback, and a fresh index-23 SETUP
returned the correct 5,000,000 value.

That long run also exposed one device-level `STOP -> prepare -> GO` at
`01:26:53` despite full 9216-byte detached writes and stable ring counters. It
coincided with 98% CPU: the existing APAP receiver/QSA writer was still at QNX
priority 10, while stock's audio receiver is priority 18 and the eight-fragment
QSA queue covers only 128 ms. Candidate `6aee7e03...` raises that existing
thread to `SCHED_RR` priority 18 and records detached prepares/status underruns;
it is deployed with `hsr-td-active`, surround 0.20 and LFE assist, pending the
next live zero-recovery run. Full evidence is in the lifecycle document.

That first run subsequently passed 16,384 detached periods (262.144 seconds)
with QSA `RUNNING`, full writes, `underruns=0` and `recoveries=0`, beyond the
old 144-second failure point. Two reported brief freezes left APAP, PCM, ring,
QSA and deva counters continuous and overlapped full slog extraction at
94--100% system CPU; they are not attributable to the audio path from this
measurement. Live monitoring now avoids full slog dumps during playback.

A subsequent natural item transition separated a real buffered failure from
those diagnostic-load events.  iOS sent `FLUSHBUFFERED`, new cover art and a
new APAC cookie; the process and QSA stayed healthy, but decoded APAC stopped
for about 45.3 seconds after its first four seconds.  The active type-103 had
been SETUP at AAC index 23 with 5,000,000 bytes even though the persistent
stream advertises `[65,23]` and changes codec without another SETUP.  An older
21,168,000-byte session had already sustained the same AAC-to-APAC transition
for more than three minutes.  Candidate `9a6a73bd...` therefore preserves
5,000,000 as the AAC-only default but returns the maximum requirement of the
whole offer, 21,168,000 for `[65,23]`.  It is deployed byte-exact; a fresh
transition is the remaining live proof.  Exact control, PCM and feedback
timing is in `docs/BUFFERED_BACKGROUND_LIFECYCLE.md`.

A fresh vehicle session has now accepted 21,168,000 and completed APAC-to-AAC
in place under `music-stage` with continuous PCM/QSA counters and no recovery,
underrun or short-ring fallback.  The reverse AAC-to-APAC transition is still
the final mixed-buffer acceptance direction.

The following long session also kept every QSA write complete and all device,
ring and recovery counters at zero, but exposed a feedback-clock hazard after
a rapid eight-FLUSH/item-change storm.  Stock briefly measured 48080.2266 Hz;
type 100 then disappeared and the preceding hook replayed that transient for
about 276 seconds instead of following the retained QSA sink.  The replacement
emits the nominal 48000-Hz contract during a 256-write warmup,
then measures 1024 successful detached 768-frame QSA periods against
`CLOCK_MONOTONIC`.  Gaps, recovery, short writes and implausible windows
invalidate the result.  Host tests and the QNX ARMv7 build pass; candidate
hook hash is `8ef70b59...`.  It is now deployed with byte-identical readback
and mapped in the new `dio_manager`, but has not yet completed the live handoff
test and therefore is not called a vehicle fix.  Full timing, ABI evidence and
deployment distinction are in the lifecycle document.

That diagnostic artifact has since been replaced by the `LOG=0` pre-production
build `7ff56c94...`.  The quiet build compiles out the logger thread and all
audio probe file writers while retaining identical receive/decode/render and
feedback-clock logic.  SCP readback matched, the verbose marker was removed,
and the live `dio_manager` mapped the quiet hook and unchanged APAC library;
no hook log or probe file was created after startup.  This is the current unit
state, still awaiting the same audible full-song/handoff validation.

The later compact cold-start A/B closes that QSA-only assumption. Type 103 was
decoding APAC/6 and the hook-owned `mpl5_dio_ent` handle completed 10,861 full
writes with no write, ring, decoder or device failure, yet the vehicle was
silent. Opening Music caused stock type-100 SETUP to reclaim the same handle;
its first period still came from the type-103 ring and sound began immediately.
Static analysis shows why: before QSA, stock `AudioStreamPrepare @ 0x4A9D0`
calls exported `CDIOManager::s_audioTypeCallback(kAudioTypeMedia)`, waits for
the HMI audio-availability ACK, and only then opens the sink. The callback
publishes `CAirPlayServiceEvent_UpdateMainAudioType`; its Pioneer handler maps
Media to DSI main-audio type 2 and calls
`CDSICarplayImpl::updateMainAudioType(type=2, add=true)`. Direct QSA skipped
that media-route/focus declaration. The cold path now invokes that exact
fully-recovered callback, hands ownership to a later stock type 100, and uses
the exact exported torn-down callback only if type 103 still owns the route.
This revision is host/QNX verified. Its fresh cold vehicle acceptance is
recorded below; full instructions, hashes and trace evidence are in
[`reference/NOW_PLAYING_STARTUP_STATE_MACHINE_AUDIT.md`](reference/NOW_PLAYING_STARTUP_STATE_MACHINE_AUDIT.md).

## Cold-connect media-route acceptance (2026-09-05)

The last cold-start requirement is now vehicle-confirmed. A fresh connection
started playback automatically without opening Apple Music. The generation had
type 103/110/111 only, so stock type 100 did not heal the session. The hook
issued the exact Pioneer `s_audioTypeCallback(kAudioTypeMedia)` declaration,
then cold-opened `mpl5_dio_ent` as six-channel/48 kHz. The retained trace reached
2,560 complete QSA periods with no short/zero write, recovery, ring wait, decode
failure, drop or device underrun. Evidence and analyzer output:
`captures/state-trace/20260905-131355/`; deployed hook SHA-256:
`7e8869875293f02952669186f7c4f183986003c4f42e7f5622ecc3db31a31754`.

## Remaining route-ownership edge found (local fix, not deployed)

The next failed media-start occurrence was captured before unplugging the
phone, with its successful replug as a control. In the failed generation stock
type 100 opened QSA and type 103 decoded AAC to PCM6 correctly. When type 100
later stopped, the hook retained the live QSA handle and completed 17,061 full
detached writes, but stock emitted its audio-torn-down notification and no hook
Media-route request followed. Replugging created a type-103-only generation;
the cold route request ran and the user confirmed sound.

This is the missing inverse transition in the ownership machine: `hook ->
stock` existed, while `stock -> hook` did not. The local repair schedules the
exact Pioneer Media-route callback 50 ms after a retained stock close, cancels
it if stock reopens, retries resolution failure after one second, and releases
the route on final buffered teardown. It uses the existing APAP receiver/QSA
writer and adds neither a thread nor a second handle. Full timestamps, static
`AudioStreamStop` order, captures and validation are in
[`reference/NOW_PLAYING_STARTUP_STATE_MACHINE_AUDIT.md`](reference/NOW_PLAYING_STARTUP_STATE_MACHINE_AUDIT.md).
Host tests and the QNX build pass; vehicle acceptance is still pending.

## AAC reserve pump and repeated-seek drain (deployed)

A live interruption is retained in
`captures/state-trace/20260905-live-aac-stutter-followup/`. AAC decoding stayed
clean, but the detached writer had reduced the 400-ms startup ring to the
0/256/512-frame granularity remainder. At the reported event iOS explicitly
sent `FLUSHBUFFERED`; the next APAP interval was 612 ms and the next QSA write
was 1167 ms later with hidden prepare/retry. This separates two causes: an
ordinary jitter gap is exposed by drain-to-empty, while a real item/seek flush
must discard old PCM and cannot be made fully gapless if iOS sends no new media.

The deployed reserve correction makes the existing APAP thread a one-QSA-period render
pump and holds the software ring near the 12,000-frame/250-ms high-water mark.
It also separates cold prime (400 ms) from post-flush re-prime (7,680 frames,
160 ms, matching the ten granted QSA fragments). No decoder, PCM geometry,
thread, QSA handle, SAD or upmix algorithm changes. Host audio tests, the full
regression suite, QNX ARMv7 build and diff check pass. The reserve behavior is
vehicle-confirmed: the retained ring stayed near 11--13k frames for more than
six minutes with no decode failure, QSA cadence gap, ring wait, recovery, short
write or device underrun. Implementation and the precise limit of the 612-ms sender gap
are documented in `docs/BUFFERED_BACKGROUND_LIFECYCLE.md`. The trace-enabled,
compact hook is installed on the unit (readback verified), 559540 bytes, SHA-256
`084e9730be045befea369c629678dccce18dd9ddfa70f2cd8a7abb93c6d5808d`;
APAC remains `a6a7bc8c...`.

The same vehicle run isolated repeated-seek lag to a different policy: while a
`FLUSHBUFFERED` boundary was pending, the receiver discarded each stale AAC
packet with the normal 21.33-ms realtime producer sleep. Several seeks could
therefore extend the invalid sequence range faster than it was drained, while
PCM stayed empty even though APAP delivery, AAC decoding and QSA remained
healthy. The installed hook now authenticates/decrypts and discards only that
explicitly invalid range without pacing, restores normal pacing at the first
post-boundary record, and traces the crossing plus discarded-packet count. It
adds no thread and does not change the working APAC, upmix or QSA paths. A fresh
multi-seek audible/trace run is the remaining acceptance test.

The subsequent B-metadata/C-audio report exposed a separate unsafe bridge
policy: whenever active type 103 lacked a complete period, the hook substituted
the independent type-100 PCM timeline. Empty-ring substitutions were not even
included in the old fallback counter. A host/QNX-verified local candidate now
keeps a single owner: buffered PCM or one whole silent QSA period, never
type-100 content under type-103 ownership, with `QSA_BUFFERED_SILENCE`
telemetry. It is not yet deployed, and the reboot removed the uncollected
`/tmp` trace, so this remains a code-proved hazard rather than a
vehicle-confirmed cause of that exact B-to-C event.

## Type-103 stopped queue and exact FLUSH lifecycle (local, not deployed)

The exact iOS 26.1/Pioneer audit then proved two independent receiver defects:
the renderer started in `playing=1` before iOS's first rate-one command, and it
destroyed every authenticated AU received during `rate=0` even when no flush
occurred. One retained handoff accounts exactly for 30 lost AUs / 30,720 frames
/ about 640 ms. The old full-ring treatment of every bounded flush was also a
real semantic mismatch.

The local repair replaces the duplicate play booleans with one lifecycle
object initialized stopped. The existing APAP/QSA thread retains validated raw
records under rate zero and drains them once, in order, under rate one. Its
memory bound is the SETUP `audioBufferSize` plus one complete record, after
which TCP backpressure applies. Authenticated sequence/RTP tags now follow
decoded PCM, so compressed records, in-flight ownership and the PCM ring all
obey the same inclusive wrapping FLUSH range. Until-only flush remains a
decoder/upmix-resetting prefix; bounded from/until preserves outside data.
Stop/generation and cookie-generation reads were synchronized at the same time.
No thread, QSA handle, decoder, format, upmix, SAD or CSD behavior changed.

The full host suite, a dedicated ASan/UBSan lifecycle regression and the QNX
ARMv7 quiet trace build pass. The candidate is intentionally not installed.
Vehicle acceptance must first show no buffered QSA write before rate one,
lossless queue drain across a no-flush pause, and exact seek/AutoMix flush
counts. Anchor-to-QSA timestamp scheduling remains an open measurement, not an
implemented speculative change. Full evidence and test gates are in
[`reference/IOS_26_1_BUFFERED_AUDIO_LIFECYCLE_AUDIT.md`](reference/IOS_26_1_BUFFERED_AUDIO_LIFECYCLE_AUDIT.md).

## APAP metadata and SoundCheck closure (local, not yet vehicle-accepted)

The complete retained AAC/APAC field map is now parsed. A byte recheck corrected
an earlier documentation error: TLV 2 is the active transport index (23/65),
while TLV 8 is `spf=1024`. The decoder still treats the inline cookie and
returned PCM geometry as authoritative, so an unsupported announcement cannot
tear down working audio. TLV 10/19/20/21 are bounded telemetry; legacy TLV 7 is
shape-validated and counted; trim 4/5 remains active.

The type-103 SETUP `soundCheckMediaKind={"1":-16}` and APAP 11/12 tuple now feed
the exact recovered Pioneer MainLoudnessParameters calculation. The retained
AAC tuple produces -9 dB and the retained APAC tuples +1 dB. Gain precedes trim,
reorder and stereo upmix, with a 20 ms item-change ramp. The new SETUP gate also
rejects formats, framing, key, rate geometry or compression type that this fixed
48-kHz rail cannot render. Host, real-capture integration, ASan/UBSan and QNX
ARM builds pass; no unit deployment or listening acceptance is claimed here.
Full map, formula, policy switches and limits are in
[`APAP_IGNORED_FIELDS_AND_SOUNDCHECK.md`](APAP_IGNORED_FIELDS_AND_SOUNDCHECK.md).

---

## Sources

* `tools/qsa-emu/` — the QEMU harness, `HANDOFF.md` and `README.md` carry the traps.
* `deploy/deva-ctrl-qc/` — `README.md` (caps + dump), `RE_TIMING.md` (the 16 ms timer).
* `docs/reference/AUDIO.md` — topology, the CarPlay audio path, the Atmos analysis.
* `captures/broken_5_1_20260901_drive/` — the vehicle capture set.
* `Firmwares/SoundAmp/AMP_ENTERTAINMENT_7_1_MOST.md` — amplifier sinks and channel layout.
* `docs/DIO_MEDIA_SINK_STATIC_AUDIT.md` — connection 162 to logical/physical MOST sink audit.
* `docs/DEVICE_TOPOLOGY_AUDIT.md` — virtual PCM to shared CSD device/topology audit.
* `docs/ROUTER_INPUT_WIDTH_AUDIT.md` — runtime MPL1/MPL5 CSD width root cause, now fixed and confirmed.
* `docs/MC_SELECTION_AUDIT.md` — connection-162 MC selector, EEPROM/basic-coding origin, and SHARC boundary.
* `docs/SHARC_INPUT_OUTPUT_MATRIX.md` — recovered 8×8 SHARC input mixer and the remaining physical-output boundary.
* `docs/SHARC_APPLICATION_EXTRACTION.md` — embedded ADSP-2148x loader/application extraction, hashes and exact block-format proof.
* `docs/MOST_SOURCE_BLOCKWIDTH_AUDIT.md` — DIO MC source allocation, Connect BlockWidth, and the next data-plane measurement.
* `docs/CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md` — amplifier width acceptance, HU literal/table sites, and CSD/MOST packing boundary.
* `docs/CSD_ACDB_CHANNEL_AUDIT.md` — CSD client/service limits, dev-78 ACDB channel/slot records, and COPP topology selection.
* `docs/AFE_TDM_MOST_SLOT_AUDIT.md` — byte-unit slot offsets, LPASS packing, grouped frame layout, and the independent SAD/ASAD declaration.
* `docs/APAC_FORMAT_INDEX_AUDIT.md` — complete iOS 27 format-index enumeration, APAC stereo index 64, legacy-mask semantics, and buffered/realtime selection boundary.
* `docs/AAC_BUFFERED_PATH_PLAN.md` — MHI2Q AAC-LC framing, Cinemo ABI, cookie contract, thread model and safe implementation plan.
* `docs/DYNAMIC_BUFFERED_CODEC_SWITCH.md` — active iOS format reselection, APAP boundary contract and receiver switch state machine.
* `docs/APAP_IGNORED_FIELDS_AND_SOUNDCHECK.md` — corrected APAP field map, Pioneer SoundCheck gain, SETUP validation and optional safety policies.
* `docs/AMP_STEREO_5_1_MATRIX_DIFF.md` — live amplifier ASAD handling, exact stereo/5.1 matrix differences and bank-dependent SHARC mode control.
* `docs/BUFFERED_BACKGROUND_LIFECYCLE.md` — measured type-100/type-103 ownership split, QSA/feedback handoff, and vehicle-confirmed cold media-route activation.
* `docs/reference/IOS_26_1_BUFFERED_AUDIO_LIFECYCLE_AUDIT.md` — exact iOS/Pioneer lifecycle contract, implemented stopped queue/exact FLUSH repair and remaining vehicle gates.
