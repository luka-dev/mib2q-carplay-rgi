# Handoff: QSA/deva router-width audit complete

## Latest result (CODEX_PROMPT_ROUTER_INPUT_WIDTH)

Full audit:
[`../../docs/ROUTER_INPUT_WIDTH_AUDIT.md`](../../docs-obsidian/Sources/ROUTER_INPUT_WIDTH_AUDIT.md).

The surviving width hypothesis is confirmed statically. The logical mixer
router has no standalone width field; it resolves a DEVA timer and a shared CSD
session. On connect, `deva_mixer_router_configure_switch_routing @0x51B94`
calls `session_csd_setup @0x6BBDC`, whose
`deva_update_session_info_from_list @0x54848` copies the timer record's
**minimum** values into the active session:

```text
record +0x14 min_voices   -> session +0x84 channels
record +0x1c min_frag     -> session +0x6c CSD buffer bytes
```

MPL1 router input 1 uses timer id 1 and record `6 / 0x4800`. MPL5 router input
5 uses timer id 4 and record `2 / 0x1800`, even after the current patch widens
its QSA maximum to six and dynamically gives its DMA timer a `0x4800`
fragment. Acquire, prepare and trigger never rebuild the already-created CSD
stream/context/buffer pool.

`session_csd_stream_playback @0x69084` copies exactly session `+0x6c` and puts
that same count in the descriptor passed to `csd_write @0x69318`. The broken
six-voice path therefore advances `0x4800` in the DMA ring but copies/submits
only `0x1800`, configured as stereo. It reads one third of each 768-frame
fragment and skips 512 frames. With six correlated channels this repeats each
sample three times inside the 16-ms block, then jumps ahead at 62.5 Hz. That
predicts the car's correct total duration, low pitch, ticking, recognisable
audio, and equal failure with correlated channels.

Do not repeat the old conclusion that `min_voices` is used only by
`deva_mib2_channel_map`; `0x54A14..0x54A18` is the load-bearing playback
consumer. The capability minima themselves are valid. A static `min_voices=6`
plus `min_fragsize=0x4800` is only a six-channel diagnostic and breaks/rejects
the clean stereo mode. The production fix must use the recovered live PCM
fields (`timer+0x11c -> channel`, voices `channel+0x10`, active fragment
`channel+0x94`) and rebuild CSD when width changes before playback starts.

No hook, harness source, deva binary, or patch was changed in this measurement
pass. The existing QEMU limit matters: MPL5 has no Audioman route and stays
`stream_type=1`, so the earlier six-channel CSD descriptor belongs to MPL1's
default route. That result remains valid but did not measure MPL5's runtime
attach.

## Latest result (CODEX_PROMPT_DEVICE_TOPOLOGY)

Full audit: [`../../docs/DEVICE_TOPOLOGY_AUDIT.md`](../../docs-obsidian/Sources/DEVICE_TOPOLOGY_AUDIT.md).

The per-virtual-channel CSD-**device-id** hypothesis is false. `mpl1_int_ent` (ENT
router input 1) and `mpl5_dio_ent` (input 5) both route through output 128,
`TDM_OUT_MPL`, to the same CSD playback session and `dev_id=0x4e` (78). The
shared device is 48 kHz / 24-bit. Its stream/context width is selected at route
attach and is not necessarily shared: the later audit above proves input 1 is
six-channel while input 5 remains stereo.
`CSD_DEV_CMD_ENABLE` contains no channel count, only id/rate/bit depth.

The important asymmetry is route lifetime. MPL1 has a default input-1 to
output-128 route and enters CSD-backed `stream_type=2`. MPL5 has no default
route; a bare QEMU open, without Audioman, remains `stream_type=1` and
deliberately discards buffers. It therefore produces zero post-open CSD calls.
On the car, Audioman's runtime switch route attaches MPL5 to the same output
session. Do not interpret the isolated zero-call trace as a hidden second CSD
device.

The opt-in reproducer is:

```sh
QSA_DEVICE_TOPOLOGY=1 QSA_PROBE_DEVICE=0 QSA_PROBE_VOICES=6 \
  ./tools/qsa-emu/build_ifs.sh
QSA_DEVICE_TOPOLOGY=1 QSA_PROBE_DEVICE=3 QSA_PROBE_VOICES=6 \
  ./tools/qsa-emu/build_ifs.sh
```

It changes no hook, deva binary, or binary patch. Default harness behaviour is
unchanged when `QSA_DEVICE_TOPOLOGY` is absent.

## Latest result (CODEX_PROMPT_3)

Full audit: [`SUBMISSION_ORDER_AUDIT.md`](SUBMISSION_ORDER_AUDIT.md). Read it
before changing submission instrumentation; it records the failed callback,
ELF-interposition, lazy-GOT, startup-prefill, and serial-export approaches.

The remaining ring-consumption hypothesis is closed. The checked 62-fragment
window advances by `0x4800` on all 61 consecutive transitions, submits all 62
written generations exactly once, and submits 1,142,784 ring bytes for 1,142,784
written bytes. No `0x2400` half-step, duplicate generation, drop, or reordering
occurs.

```text
TIMER_CURSOR_SUMMARY ring=0 advances=62 delta_0x4800=61 delta_0x2400=0 delta_other=0
CSD_REGION_SUMMARY written_generations=62 distinct_submitted=62 submitted_once=62 repeated_generations=0 duplicate_submissions=0 missing_generations=0 unknown=0
CSD_BYTE_SUMMARY submitted=1142784 written=1142784 difference=0
CSD_SUBMIT_RENDER_SUMMARY fragments=62 frames=47616 samples=285696 pcm_bytes=571392 reference_mismatches=0 overflow_calls=0
```

Checked artifacts:

* serial: `build/qsa-emu/qsa-submit-final2.log`, SHA-256
  `6d051876b2eec395a4f04781333fd29f5a1798d1dc92ee677589122d3d4f7d92`;
* WAV: `build/qsa-emu/submission-order-final2/dsp_submission_order.wav`,
  SHA-256 `4daa9fc9915ce9929fa1874d2a4c5bf531d996be4c733cb3e12d5feae4909112`;
* PCM: bit-identical to the first 47,616 known-good reference frames, zero
  mismatches, SHA-256
  `f0f37e6c1f2e5d32d0c938053b9f2f70fbac25868e61f6e1fe2342ae0c90c312`.

Two zero-filled CSD startup-prime writes occur before the first ring pull and are
counted separately. A stale slot may be pulled after `DSP_SUBMIT_COMPLETE` while
flush races STOP; it is outside the frozen 62-fragment window and is not in the
WAV. The requested path through `csd_write` is a faithful single pass. Move the
vehicle investigation downstream of `csd_write`.

Implementation notes: the CSD stub queues registered BUF_DONE callbacks and
recycles them at io-audio's first imported mutex call inside `dma_interrupt`
(`0x10f7a8`), after deva advances the cursor. Only io-audio's resolved GOT slot
`0x124f78` is redirected; the saved libc target is called afterward. No deva
patch, opaque-parameter dereference, or io-audio thread was added. The probe uses
`SND_PCM_START_FULL` to avoid a trace-induced startup underrun and remains silent
while serial base64 is emitted.

The harness now captures a continuous 0.992-second six-channel run from deva's
DMA ring, renders both 32-bit-word interpretations, exports them over serial, and
carves them into WAV files on the host. The requested measurement is complete.

## Run and carve it

    ./tools/qsa-emu/build_ifs.sh                    # patched driver (default)
    QSA_STOCK_DEVA=1 ./tools/qsa-emu/build_ifs.sh   # stock-driver A/B

    qemu-system-arm -M virt -m 512 -cpu cortex-a15 -icount shift=auto \
      -device loader,file=build/qsa-emu/qsa.bin,addr=0x40200000,force-raw=on,cpu-num=0 \
      -display none \
      -serial file:$PWD/build/qsa-emu/qsa-render-serial.log \
      -monitor none

    python3 tools/qsa-emu/carve_renders.py \
      build/qsa-emu/qsa-render-serial.log build/qsa-emu/renders

The checked run is `build/qsa-emu/qsa-render-serial-20260901-14.log`. Its useful
result is printed before the known teardown fault:

    DSP_VERIFY ring=0 source=resmgr voices=6 fragments=62 bytes=1142784 mismatches=0 CONTINUOUS_PASS
    DSP_RENDER_SUMMARY fragments=62 frames=47616 samples=285696 bytes_per_stream=571392 mismatches=0
    DSP_RENDER_B_STATS peak=32768 saturated=99 total=285696 ppm=346
    === DSP_RENDER_A_HIGH16_CORRECT_S16LE_BASE64_BEGIN ... ===
    ...
    === DSP_RENDER_A_HIGH16_CORRECT_S16LE_BASE64_END ===
    === DSP_RENDER_B_LOW24_DSP_S16LE_BASE64_BEGIN ... ===
    ...
    === DSP_RENDER_B_LOW24_DSP_S16LE_BASE64_END ===
    === DSP_RENDER_COMPLETE ===

The guest may fault during the later stereo setup/teardown. That is the known
teardown problem, not a render failure; all required bytes precede
`DSP_RENDER_COMPLETE`.

## Result

The host carver produced 47,616 frames, 285,696 samples, and 571,392 PCM bytes
per stream (6 channels, 48 kHz, S16, 0.992 seconds).

Interpretation A, signed S16 from word bits 31..16:

    bit_identical=True mismatches=0 peak=12526 rms=1598.896
    PCM SHA-256: f0f37e6c1f2e5d32d0c938053b9f2f70fbac25868e61f6e1fe2342ae0c90c312

That PCM hash is identical to the first 47,616 frames of
`captures/broken_5_1_20260901_drive/session6_5_1_declared_order.wav`.

Interpretation B, signed 24-bit value from word bits 23..0, normalized to S16:

    peak=32768
    saturated=99/285696 = 0.034652%
    different_from_input=76907/285696
    rms=6331.105

The two WAVs are:

* `build/qsa-emu/renders/dsp_render_a_high16.wav`
* `build/qsa-emu/renders/dsp_render_b_low24.wav`

A is the already human-confirmed known-good audio and therefore does not resemble
the car fault. B is severely corrupted, reaches full scale, and is the only one
qualitatively compatible with recognisable but distorted/wrong audio. However,
the literal low-24-bit interpretation does **not** produce broad hard clipping:
only 0.034652% of all samples are exactly full scale. Therefore this measurement
supports B as a distortion mechanism, but does not prove that the car's DSP uses
this exact low-24 extraction or that it is the car's fault.

## Why the old run captured only one fragment

The old probe printed “wrote 62 fragments,” but it counted calls rather than
accepted bytes. Return-value instrumentation showed the actual sequence:

    fragment 0: snd_pcm_write_internal rc=18432, plugin_write rc=9216
    fragment 1: snd_pcm_write_internal rc=-5 (EIO), plugin_write rc=0

The no-DSP guest never advances the playback timer. After the first accepted
fragment the raw write returns EIO, while the plugin layer converts that into a
zero-byte return. The original loop treated zero as success, reread the next file
fragment, and only the first call ever reached `resmgr_msgread`.

`qsa_probe` now requires a full-fragment return. On zero it calls
`snd_pcm_plugin_prepare` and retries the **same** source buffer; it does not read
the next fragment until QSA accepts the current one. Each successful retry
crosses the proven resource-manager copy into the ring. No io-audio thread and no
firmware binary patch are used. The staged `io-audio` checksum remains:

    b38fafad979fa2e320e7111d122cd5ec1f4d7737744b29e2cbf767d1b6b875ef

## DSP renderer definition

For each captured S32 word `w`, the stub writes:

    A = (int16_t)(w >> 16)
    low24 = sign_extend(w & 0x00ffffff, 24)
    B = low24 / 256

Both outputs retain the original six-channel interleave and are serialized as
base64 lines prefixed `DSP64 `. The host carver accepts only payload between the
exact labeled BEGIN/END markers and validates base64 before writing WAV headers.

## Exact ring capture point

`ado_pcm_write_block` at io-audio virtual address `0x111094` computes:

    destination = ring_base + fragment_index * fragment_size

It calls imported `resmgr_msgread` at `0x1111c0`. QNX copies the client message
directly into that destination, bypassing libc `memcpy`. The preloaded CSD stub's
wrapper calls the real function, verifies `rc == size`, recognizes the destination
inside the malloc-backed ring, validates the post-copy S32 words, and renders
them before returning.

## CSD stream/context descriptor audit (CODEX_PROMPT_2)

The stub now emits full sequence-numbered hex blocks for every typed
`csd_open` payload and every `csd_ioctl` payload before command handling.
`QSA_PROBE_VOICES=2|6 ./tools/qsa-emu/build_ifs.sh` produces isolated runs.
Final logs:

* `build/qsa-emu/qsa-csd-2.log` (SHA-256
  `97eb5076548974ec4a840baca2ce3445194bb8df59e12ea6827b7c5535c789f7`)
* `build/qsa-emu/qsa-csd-6.log` (SHA-256
  `d0fec5c806e662cadecba99e0099a193253db9c18055140a57cee53062842720`)

The CSD trace through sequence 196 is byte-identical in both modes. CSD streams
are created persistently during io-audio startup; a later two- versus six-voice
client open does not rebuild them. The relevant coherent startup descriptors
are:

* six: handle 257, sequence 3, 48000 Hz, 6 channels, 24 bits, signed,
  interleaved, map `01 02 03 06 08 09`, seven 18432-byte buffers;
* stereo: handle 271, sequence 129, 48000 Hz, 2 channels, 16 bits, signed,
  interleaved, seven 3072-byte buffers.

Both are exactly 768 frames / 16 ms. Six-channel bytes/frame is 24, i.e. six
four-byte containers; no stereo rate, count, stride, period, buffer, or
interleave value survives in the six descriptor. **The stale downstream CSD
descriptor hypothesis is false in this binary/run.**

Static confirmation in patched `deva-ctrl-qc.so` (ELF base zero, text VA=file):
`deva_csd_start_playback` copies the live PCM rate/voices/bit width/sign/
interleave into CSD config at `0x26948`, `0x26954`, `0x26978`, `0x26960`, and
`0x26984`, then calls the CSD start routine at `0x26A64`. The callee reads those
fields at `0x253E0..0x253F0`; its multichannel branch passes literal six at
`0x25400` and calls the formatter at `0x25414`. The widened caps fields at file
`0xA6E1C/0xA6E24/0xA6E28` are not mirrored CSD fields and need no parallel edit.

The formerly unexecuted `session_csd_stream_playback` / cursor boundary is the
CODEX_PROMPT_3 result at the top of this handoff. It is now measured through
`csd_write`: the cursor advances by `0x4800` and every written generation is
submitted once.

Binary fingerprint: GNU build ID `9168acff553fc86d6a3c991613d620fd`, SHA-256
`8b55b8d8f02b97d6f30191b54ef8a847e4706067f21cbf068d4b02a563ea84c9`.

## Things not to rediscover

* Firmware binaries unpack without `+x`, and mkifs preserves host modes.
* `io-audio` needs `CACHE_NOP=1`.
* The BSP libc lacks the firmware's stack-protector exports; the CSD stub supplies
  them, along with `rtpd_init`/`rtpd_deinit`.
* deva blocks in `Resource_BlockWait` until `/dev/audio_service` answers.
* `csd_query_driver_version()` takes no arguments and returns the version.
* `csd_open` argument 1 is not a string; never dereference untyped parameters.
  Its recovered stream/context wrappers pass a typed descriptor pointer in
  argument 2 and an exact length in argument 3; only that pair is hex-dumped.
* `ALLOC_PMEM` word 3 is an address used with `mem_offset64`, not a descriptor.
* `csd_read` during capture start is a zero-byte probe. Returning 36 makes the
  driver fail with rc `0x24`; return zero.
* Wait for a real `/dev/snd` node, not merely the directory.
* Playback and capture device numbering are separate.
* `sse_bundle_1` is a DSP-to-host capture route, not the playback output; reads
  return EIO against the DSP stub.
* Do not add a thread inside `io-audio`; a watcher destabilized the guest.
* Do not interpose pthread mutex functions with a dlsym-based wrapper; loader
  recursion faults in libc's `dlsym` during io-audio startup.
* CSD PMEM pools remain empty. Playback client PCM enters the malloc-backed DMA
  ring through `resmgr_msgread`.
* A zero return from `snd_pcm_plugin_write` is not an accepted fragment. Inspect
  `snd_pcm_write_internal`: in this guest it is the plugin-masked EIO path.
* `snd_pcm_plugin_prepare` recovery must retry the same file buffer. Advancing the
  reference or input index before a full write creates harness-only mismatches.
* Malloc reuses the six-channel ring address for stereo. Retire the old ring
  record when the address is reused or stereo bytes look like false six-channel
  mutations.
* The serial result must be emitted synchronously before teardown; a destructor
  is too late and an io-audio worker thread is unsafe.
* The staged music is generated directly from the known-good WAV; do not depend
  on a stale `/tmp/music6.s16`.
* `build/qsa-emu/qsa_probe` is an ignored build artifact. Build it with qcc in
  the Docker toolchain, not the GNU cross linker against extracted libasound.

## Files involved

* `csd_stub.c`: ring discovery, continuous validator, two DSP interpretations,
  CSD submission generations/cursor audit, callback recycling, and serial
  base64 export.
* `qsa_probe.c`: sequential file playback, same-fragment EIO recovery, preferred
  CSD sink selection, START_FULL prefill, and silent export coordination.
* `tap_preload.c`: input/output previews and raw/plugin return-value evidence.
* `carve_renders.py`: marker extraction, WAV wrapping, A/reference comparison,
  B statistics, and `csd_write`-order PCM comparison.
* `SUBMISSION_ORDER_AUDIT.md`: full CODEX_PROMPT_3 static recovery, measurement
  design, rejected approaches, diagnostic log ledger, hashes, and conclusion.
* `build_ifs.sh`: reproducible qcc rebuild, known-good PCM extraction, and IFS.
* `qsa.build`: boots the real stack and runs the probe.
