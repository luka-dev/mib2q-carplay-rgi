# Discrete 5.1 CarPlay audio — everything known, and what is still open

Status as of 2026-09-02. The fault is **not fixed**, but it is now localised.

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
| deva caps record | All 11 fields compared against `mpl1_int_ent`; only the two `min` fields differ, and both are correct (see §5). |

**Therefore: everything from the APAC decoder down to `csd_write` is measured clean.**
Any remaining fault is downstream of that boundary.

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

## 5. The deva caps record is finished

`mpl5_dio_ent` (CarPlay) versus `mpl1_int_ent` (the device that already runs six
channels). Every field matches byte for byte except two, and both must stay as they are:

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
edit. Writing `0x4800` into the table breaks stereo.

**`min_voices` is inert for playback.** Its only consumer is `deva_mib2_channel_map`,
which is never called directly — it is installed as a device op in
`deva_pcm_and_mixer_device_init_platform_mib` — and it acts on just three commands:
`QUERY_CHANNEL_MAP`, `GET_CHANNEL_MAP`, `SET_CHANNEL_MAP`. Its switch reads
`aLe1[48*i + 268]`, and `0xA7C4C + 268 = 0xA7D58` = `caps_base + 20`, confirming the
field. Raising it to 6 would land in the ADO caps as a **lower bound** and fail every
stereo open.

---

## 6. Why the emulator cannot find this

`tools/qsa-emu/` runs the real `io-audio` and the real `deva-ctrl-qc.so` under QEMU, with
only the eight CSD symbols stubbed. That is what made §2 possible.

It is also the ceiling: **CSD is exactly the component that is stubbed**, so nothing below
`csd_write` — the DSP, MOST, the amplifier — can be observed there. The harness has now
cleared everything it is capable of clearing.

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

## 8. The measurement gap — the most important section

`frag=1920->9216` in the hook log is `params->buf.block.frag_size` -> `adjusted.buf.block.frag_size`:
**our request**, not what the device granted. The 3x-slow bug was declared fixed on the
strength of that line.

Worse, our own `snd_pcm_plugin_setup` interposer edits the copy it returns to the caller,
so deva's true negotiated geometry has **never been observed on the car**.

The hook already contains honest instrumentation for all of this. None of it has ever run
in the vehicle: the unit carries an older hook with `AUDIO_SPATIAL=0`, and the dynamic deva
build was staged on 2026-08-31 and, per its own README, is not active until `io-audio`
reloads — which needs a reboot that has not happened.

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

`AUDIO_SPATIAL` is back to `0` on the unit and in this repo, so CarPlay runs its clean stock
stereo until the virtual channel is fixed.

---

## Sources

* `tools/qsa-emu/` — the QEMU harness, `HANDOFF.md` and `README.md` carry the traps.
* `deploy/deva-ctrl-qc/` — `README.md` (caps + dump), `RE_TIMING.md` (the 16 ms timer).
* `docs/reference/AUDIO.md` — topology, the CarPlay audio path, the Atmos analysis.
* `captures/broken_5_1_20260901_drive/` — the vehicle capture set.
* `Firmwares/SoundAmp/AMP_ENTERTAINMENT_7_1_MOST.md` — amplifier sinks and channel layout.
