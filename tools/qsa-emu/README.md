# MHI2Q audio stack under QEMU

Runs the head unit's real `io-audio` and `deva-ctrl-qc.so` on a machine with no
Qualcomm ADSP, so the layer between `snd_pcm_plugin_write` and the hardware can be
inspected without the car.

## Why

On the car every counter above the driver is clean — correct 48 kHz timing, 768
frames per 16 ms fragment, zero underruns, and PCM that is provably correct
(decoded offline and listened to). The sound is still wrong. Everything left was
below the write: QSA's S16->S32 conversion at six voices, io-audio's mixer, and
deva's DMA. None of it was observable from a log on the unit.

## How

`libcsdIpcClient.so` is the only substitute. deva imports exactly eight symbols
from it and builds every higher-level CSD call (`openAudioStreamV2`,
`setStreamMultichannel`, `setStreamFormatPcmMultichannel`, `allocateCsdBufferPool`)
on top of them, so standing in for those eight runs the whole driver. Preloading
the stand-in into `io-audio` also lets it locate deva's DMA ring and capture the
resource-manager copy into that ring. The stand-in now also renders every
accepted S32 word under both plausible sample-width interpretations and exports
the resulting six-channel S16 streams as base64 on the serial console.

    ./build_ifs.sh          # stages, runs mkifs in the qnx65 Docker image
    # boot: qemu-system-arm -M virt -m 512 -cpu cortex-a15 -icount shift=auto \
    #   -device loader,file=build/qsa-emu/qsa.bin,addr=0x40200000,force-raw=on,cpu-num=0 \
    #   -display none -serial stdio -monitor none

No SDP VM: `mkifs`, `mkifsf_elf` and the ARM toolchain are all in the Docker image.
`startup-virt` comes from the emu harness (`build/re/`), because the BSP's own
extracted copy lost its ELF header to dumpifs and mkifs cannot identify it.

## Where it got to

The whole stack runs. `io-audio` stays up as a resource manager, `deva-ctrl-qc.so`
initialises against the stub, and the driver creates all 27 PCM devices —
including `mpl5_dio_ent`, the CarPlay one:

    mpl1_int_ent mpl3_mirror_lnk mpl4_sdis_a2ls mpl5_dio_ent mpl6_gal_ent
    mpl7_bcl_ent ann1_int_nav ... sse ent1 mic1 ... sse_bundle_1

The serial console records the conversation the driver believes it is having
with the DSP:

    csd_init
    csd_query_driver_version -> 0x10000
    csd_open(0x5 0 0 0x58) -> 256
    csd_open(0xc 0xff978 0x28 0x2) -> 257
    csd_ioctl(h=257 cmd=0x11410 len=88)
    csd_ioctl(h=257 cmd=0x11425 len=8) ...

What it took, in order, each found by reading the guest's own slog:

| symptom | cause |
|---|---|
| `Unable to start "io-audio" (2)` | the firmware was unpacked without `+x` and mkifs copies the host mode |
| `Failed to initialize cache library` | needs `CACHE_NOP=1`, the same workaround the dio harness uses |
| `unknown symbol: __stack_chk_guard` | firmware built with stack protector; the BSP libc does not export it, so the stub does |
| `Unresolved symbol "rtpd_init"` | lazily bound Harman platform symbol; stubbed |
| blocked in `Resource_BlockWait` | deva waits for `/dev/audio_service` before it will mount a card — `devstub` answers it |
| driver logged version FAILED | `csd_query_driver_version()` takes NO arguments and returns the version; recovered from `csd_log_driver_version` @0x28ba8 |
| crash inside `csd_open` | the stub printed argument 1 as a string and it is not one — never dereference a parameter whose type has not been recovered |

## What it has already established

The driver states its own stream geometry, which no log on the car exposed:

    session_csd_setup: pcm format => sample_rate=48000, channels=6,
                       bit_per_sample=24, sign=1, interleave=1
    setStreamFormatPcmMultichannel: num_ch=<6>, bits_per_sample=<24> ... SUCCEEDED

So the caps advertise `format 0xc` = `SND_PCM_SFMT_S32_LE`, a 32-bit container,
while the DSP is told the samples carry **24 significant bits**. Whether QSA's
S16->S32 widening leaves them where the DSP expects them was the remaining
question. The two renderings at the end of this README now measure both layouts.

## What it proved

The stack runs end to end. The card registers, every device enumerates by name,
and both configurations open and write:

    snd_cards() = 1
    card 0 dev 3: 'mpl5_dio_ent'
    device geometry: voices 2..6, rate 48000..48000, frag 6144..18432, formats 0x1000
    plugin_params(S16,6ch,48000Hz,frag=9216) -> 0
    setup readback: 6 voices, 48000 Hz, format 4, frag 9216, frags 10
    wrote 62 fragments of 768 frames

The final line now means 62 **full accepted writes**, not merely 62 calls. The
old probe missed that the raw second write returned EIO and the plugin reduced it
to zero bytes; its loop counted zero as success. The current probe requires the
full 9,216-byte client result, prepares the channel after zero, and retries the
same source fragment before advancing the file.

`formats 0x1000` is `1 << SND_PCM_SFMT_S32_LE`: the device accepts **only** S32,
so QSA always converts, and `voices 2..6` is the patched driver's widened range.

### QSA's setup readback cannot be trusted

Running the same probe against the **stock** driver (`QSA_STOCK_DEVA=1`) is the
finding that matters:

    device geometry: voices 2..2, frag 6144..6144     <- device does stereo only
    plugin_params(S16,6ch,...) -> 0                   <- accepted anyway
    setup readback: 6 voices, frag 9216               <- and setup claims six

The plugin layer inserted a channel conversion and reported the *client* geometry
back through `snd_pcm_plugin_setup`. So a six-voice open "succeeding" and a setup
readback saying six voices prove nothing about what the hardware is doing.

That matters directly on the car: the hook's guards — the renegotiation check and
the fragment-capacity check — both read `snd_pcm_plugin_setup`. On a unit whose
deva patch had not taken effect they would see six voices, report everything
healthy, and let a hidden downmix run. Every clean counter in the last drive's log
was gathered through exactly that call.

## The callback contract, recovered

`registerCallBack` @0x2c8e8 is one ioctl:

    csd_ioctl(h, 0x11401, {callback_fn, context}, 8)

and the trace captures both values live:

    csd_ioctl(h=257 cmd=0x11401 len=8) [0x780298b0 0x13cba0]

`csdUserCallBack` @0x290f4 takes `(event, buf_desc[2], _, context)` and handles
three events — 71169 `BUF_DONE`, 71170 `BUF_READY`, 71171 `EOS` — releasing the
buffer back to the pool and signalling a condvar. So the playback loop is: deva
fills a PMEM buffer, submits it, and waits for the DSP to call back. A stub that
registers the callback and never calls it stalls that loop after `buffer_count`
buffers.

That callback contract is recovered, but this emulated playback route does not
exercise it: all CSD PMEM pools remain empty. The working capture point is the
resource-manager copy into deva's separate malloc-backed DMA ring, described
below.

## The device map, from the guest itself

Playback and capture carry separate numbering, which is why a playback-only sweep
stops at `sse` and never shows the bundle:

    playback  0 mpl1_int_ent   1 mpl3_mirror_lnk  2 mpl4_sdis_a2ls  3 mpl5_dio_ent
              4 mpl6_gal_ent   5 mpl7_bcl_ent     6 ann1_int_nav    7 ann2_int_sds
              8 ann3_dio_ann   9 SW MIX PCM      10 ann4_dio_voice ... 20 sse
    capture  21 ent1  22 mic1  23 mic2_dio  24 mic3_gal  25 mic4_mirror_lnk
             26 mic5_bcl  27 sse_bundle_1

Geometry, straight from `snd_pcm_channel_info`:

| device | voices | rate | fragment | formats |
|---|---|---|---|---|
| `mpl5_dio_ent`, patched deva | 2..6 | 48000 | 6144..18432 | `0x1000` = S32_LE only |
| `mpl5_dio_ent`, stock deva | 2..2 | 48000 | 6144..6144 | `0x1000` |
| `sse_bundle_1` (capture) | 3..3 | 48000 | 4608..4608 | `0x10` = S16_LE only |

The playback devices take **only S32**, so QSA is always converting under CarPlay.
The bundle is S16 and three-voiced — 4608 bytes is 768 frames of 3-channel S16,
the same 16 ms period everything else runs on.

## Why the audio still stops short of CSD

The slog names two timers, `deva_timer_play_mpl5_dio_ent` and
`deva_timer_cap_sse_bundle_1`, so the path is client -> mpl5 -> software mixer ->
bundle -> CSD. The play timer starts: `deva_trigger` fires for device_id 4
`<mpl5_dio_ent>`, `session_deva_timer_start` returns 0, and `deva_aquire` finds
the device. What never happens is an emulated per-fragment tick or submission.
After one accepted fragment, `snd_pcm_write_internal` therefore returns `-EIO`;
`snd_pcm_plugin_write` masks it as a zero-byte return. Preparing the channel and
retrying the same fragment gives the harness another accepted resource-manager
copy without inventing a timer or adding a thread inside `io-audio`. The command
histogram still does not move beyond the 64 pool allocations.

Opening `sse_bundle_1` from the client does not start it either: the open
succeeds but `snd_pcm_plugin_params` returns ENOENT with `why_failed = 0`, for
both S32 and its own declared S16. That reads like a device deva drives itself
rather than one a client is meant to claim, which fits it having its own capture
timer.

## The measurement this was built for

Preloading a two-function tap into the probe captures both sides of QSA's
conversion inside the client process, where it happens. Feeding it a pattern
whose position in the widened word is unmistakable -- channel c carrying the
constant 0x1000*(c+1) -- gives:

    setup readback: 6 voices, 48000 Hz, format 4, frag 9216
    TAP in  len=9216:  00100020 00300040 00500060 ...
    TAP out len=18432: 00000010 00000020 00000030 00000040 00000050 00000060 ...

Little-endian: the input is S16 `0x1000 0x2000 0x3000 0x4000 0x5000 0x6000`, the
output is S32 `0x10000000 0x20000000 0x30000000 0x40000000 0x50000000 0x60000000`.

**QSA's widening is exactly a left shift by 16, with channel order preserved.**
No permutation, no loss, no misalignment at six voices.

That clears the last layer anyone could still suspect. Taken with what was already
established -- the decoded PCM is correct and was confirmed by ear, deva's caps
and channel maps match the working six-channel device, and the timing measured on
the car is exactly 48 kHz -- everything from the APAC decoder down to the bytes
handed to the driver is now accounted for.

It also settles a pending experiment: `AUDIO_SPATIAL_S32` makes the hook do this
widening itself instead of letting QSA do it. Since QSA's result is bit-for-bit
what a left shift produces, that switch would change nothing. It should not be
deployed expecting a fix.

## How far the audio path gets

Playback works end to end up to the driver. What does not happen is the audio
leaving for the DSP.

| step | state |
|---|---|
| client writes to `mpl5_dio_ent` at six voices | 62 full fragments accepted with same-buffer prepare/retry recovery |
| QSA converts S16 -> S32 | works, measured correct above |
| resource manager copies into deva's DMA ring | 62 fragments / 1,142,784 bytes captured; zero mismatches |
| deva play timer starts | `deva_trigger` fires for device_id 4, but the emulated timer receives no fragment ticks |
| playback CSD session starts | `setStartSession: SUCCESS session handle 277` |
| capture session on the bundle starts | `session_csd_start_capture() SUCCEEDED`, `deva_trigger` fires for device_id 30 |
| reading the bundle | returns EIO -- it captures *from* the DSP, so with a stub DSP there is nothing to capture |
| **the DSP's PMEM pools** | **always zero** |

That last row is the finding. The stub owns every pool the driver asked for and
reports their contents on a schedule:

    POOLSUM(all zero) 0/18432 0/18432 0/18432 ... 0/768 0/768

Seven pools at 18432 bytes -- exactly the six-voice fragment -- and five at 768,
never holding a single non-zero byte across a run that writes 62 fragments. So
the playback path does not move audio through CSD PMEM in this configuration at
all; it stays in the DMA buffer `deva_mib2_aquire` mallocs, and whatever consumes
that buffer is not reached here.

Two corrections worth keeping, both cheap to get wrong again:

* `sse_bundle_1` is **not** the path playback takes to the DSP. Its CSD session is
  `csd_capture_sse` -- it captures the mix *from* the DSP, which is why reads
  return EIO against a stub that has nothing to give.
* `csd_read` is a probe during capture start, not a transfer: the driver logs
  `csd_read() returned [36] bytes, when expecting 0!` and fails the whole start
  with rc 0x24. Returning zero is what makes `session_csd_start_capture` succeed.

## The driver's own lower capture point

`session_deva_timer_thread` starts at 0x6E2D0. Its call at 0x6E85C shows what the
timer does with each fragment:

```c
switch (session_type) {              /* timer + 328 */
  case 1: session_void_stream_buffer(...);          /* discards it */
  case 2: session_csd_stream_buffer(...);           /* sends it to the DSP */
  case 4: session_loopback_deva_stream_buffer(...);
}
deva_timer_data_dumps(timer, dma_base + cursor, frag_size);
dma_interrupt(...);
```

The dump call is a **capture point built into the driver**. `deva_timer_data_dumps`
@`deva_data_dump.c` writes the fragment straight to a file, and
`deva_timer_create_data_dump_files` names it:

```c
snprintf(s, 256, "%s%s_%d.bin", "/var/log/", timer_name, counter);
open(s, O_WRONLY|O_CREAT|O_TRUNC, 0644);
```

so a run produces `/var/log/deva_timer_play_mpl5_dio_ent_0.bin` — the raw audio,
exactly as the timer hands it on, at the moment it hands it on.

The driver's own usage text names the switch:

    - data_dump_path=path              Path on target for the data dump.
    # io-audio -d qc default_device=six_channel_speakerp,log_level=high,data_dump_path=/var/log/

Setting it is necessary but nowhere near sufficient. The dump is gated three ways:

    if (!*(BYTE*)(timer + 336) || *(DWORD*)(timer + 328) != 2 || !*(DWORD*)(timer + 332))
        return 0;

a per-timer enable byte, a session type of exactly 2 (CSD), and a non-null
session pointer. Only `deva_data_dump_ctrl` sets that byte, and it is a runtime
command, not a command-line option:

```c
deva_data_dump_ctrl(dev, cmd)
    if (cmd+12 non-empty)  strlcpy(g_file_path, cmd+12, 256);
    off_B0C54(dev, cmd, timers, csds);        /* "get_fn": collects the lists */
    if (*(DWORD*)cmd == 0)  disable: close files, clear +336
    else                    enable:  create the file, set +336, csd_enable_data_dumps
```

`off_B0C54` is assigned in `deva_data_dump_init`, which refuses outright unless
the platform is one of three:

```c
platform_id = *(DWORD*)(cfg + 12);
if (platform_id > 7 || ((1 << platform_id) & 0xA4) == 0)   /* bits 2, 5, 7 */
    return -1;                       /* "Un-supported platform_id: %d!" */
```

and in the harness `deva_data_dump_init` is never called at all -- no line from it
appears in any run -- so `get_fn` stays null and `deva_data_dump_ctrl` would fail
with "get_fn not initialized!". The pointer at 0x9b710 that holds
`deva_data_dump_ctrl` is a relocation filled at load time, i.e. a callback slot in
a table something else invokes; on the head unit that something is the audio
service.

**This matters more on the car than here.** There the session really is a CSD one
and the audio is real, so enabling this dump captures what the driver sends to the
DSP — a measurement point below everything the hook can see, and one we did not
know existed.

## The DMA ring, captured and verified

Preloading the stub into `io-audio` itself -- not merely letting it be pulled in
as the driver's dependency, where its symbols do not preempt libc -- makes a
`malloc` interposer effective, and the rings fall out immediately:

    DMA candidate 0: 184320 bytes    (10 x 18432, the six-voice fragment)
    DMA candidate 1:  61440 bytes    (10 x 6144, the stereo one)

The missing trigger was not a driver timer call or `memcpy`. In the firmware
`io-audio`, `ado_pcm_write_block` at `io-audio+0x11094` computes
`ring_base + fragment_index * fragment_size` and passes that address to imported
`resmgr_msgread` at `io-audio+0x1111c0`. The QNX kernel copies the client's
message directly into the ring, bypassing libc `memcpy` entirely.

The preloaded CSD stub now interposes `resmgr_msgread`, calls the real function,
recognizes a destination inside either recorded ring, appends the copied bytes to
`/tmp/resmgr_dma_<ring>_<ring-size>.bin`, and validates every S32 word before the
ring can be changed. The six-channel run reports:

    DSP_VERIFY ring=0 source=resmgr voices=6 fragments=62 \
      bytes=1142784 mismatches=0 CONTINUOUS_PASS

That is 62 complete 768-frame fragments: 1,142,784 bytes, or 285,696 S32
samples. Every word matched the next sample from the known-good six-channel car
capture after the expected `S16 << 16` widening.

The earlier “only first fragment” result was a harness accounting bug, not a
mapped-transfer discovery. Return-value logging showed:

    call 1: snd_pcm_write_internal rc=18432, plugin_write rc=9216
    call 2: snd_pcm_write_internal rc=-5,    plugin_write rc=0

The plugin's zero was counted as a successful fragment even though it never
crossed the resource manager. The probe now calls `snd_pcm_plugin_prepare` after
zero and retries the same source buffer. The retry reaches `resmgr_msgread`; only
then does the probe read the next file fragment. No `io-audio` thread or firmware
binary patch is involved, and the staged `io-audio` remains byte-identical to the
firmware (`b38fafad...b875ef`).

## Verified continuously on real audio

The flat pattern -- channel `c` holding a constant -- is identical in every frame,
so it cannot catch a duplicated, dropped or reordered frame. Replaying the car
capture in the declared order the sink hands to QSA removes that blind spot.
`qsa_probe` keeps the file open and reads the next fragment only after the current
one has been accepted:

    DSP_CAPTURE reference: 288000 music samples
    DSP_VERIFY ring=0 source=resmgr voices=6 fragments=62 \
      bytes=1142784 mismatches=0 CONTINUOUS_PASS

The result covers 47,616 frames, 285,696 samples, and 0.992 seconds. The complete
run reaches deva's DMA ring byte for byte; the old post-first-fragment validator
false positives are gone because input and reference now advance together.

## The DSP interpretations

The stub renders every captured S32 word `w` two ways while preserving the
six-channel interleave:

```c
A = (int16_t)(w >> 16);
low24 = sign_extend(w & 0x00ffffff, 24);
B = low24 / 256;
```

It emits each raw S16 stream between unambiguous BEGIN/END markers as `DSP64 `
base64 lines. `carve_renders.py` extracts them, writes 6-channel/48-kHz/S16 WAV
headers, compares A with the reference, and measures B:

    frames=47616 duration=0.992000s samples=285696
    A_HIGH16_CORRECT bit_identical=True mismatches=0 peak=12526 rms=1598.896
    B_LOW24_DSP peak=32768 saturated=99/285696 \
      saturated_percent=0.034652 different_from_input=76907/285696 rms=6331.105

Interpretation A's PCM SHA-256 is
`f0f37e6c1f2e5d32d0c938053b9f2f70fbac25868e61f6e1fe2342ae0c90c312`,
identical to the first 47,616 reference frames. It is the already
human-confirmed known-good audio and does not resemble the car fault.

Interpretation B is severely corrupted and reaches full scale, so it is the only
one qualitatively compatible with recognisable but distorted/wrong audio. The
literal low-24 extraction does **not**, however, create widespread hard clipping:
only 99 samples (0.034652%) are exactly full scale. This measurement therefore
supports B as a possible distortion mechanism but does not prove that the car's
DSP extracts those exact bits, nor that B is the car's precise failure mode.

The checked artifacts are:

* `build/qsa-emu/qsa-render-serial-20260901-14.log`
* `build/qsa-emu/renders/dsp_render_a_high16.wav`
* `build/qsa-emu/renders/dsp_render_b_low24.wav`

## CSD descriptor audit: no stale stereo geometry

The CSD stub now prints every `csd_open` pointer/length payload and every
`csd_ioctl` payload in full, before it mutates commands such as `ALLOC_PMEM`.
Each block has a monotonically increasing sequence number and explicit
`BEGIN`/`HEX`/`END` records. `qsa_probe` can be built as an isolated two- or
six-voice run so the known teardown fault cannot hide the second case:

```sh
QSA_PROBE_VOICES=2 ./tools/qsa-emu/build_ifs.sh
QSA_PROBE_VOICES=6 ./tools/qsa-emu/build_ifs.sh
```

The resulting serial logs are
`build/qsa-emu/qsa-csd-2.log` and `build/qsa-emu/qsa-csd-6.log`. The 694 CSD
trace lines through sequence 196, immediately before the isolated PCM write,
are byte-identical in both runs (SHA-256
`7281e96c729d7a871d82881cda63cada0dada2be980852836bb69792977bbf1e`).
That is architectural, not a missing trace: io-audio creates persistent CSD
streams during driver startup; opening `mpl5_dio_ent` at two or six client
voices does not construct another format descriptor. Only the QSA/deva DMA
geometry changes:

| isolated probe | QSA S16 fragment | deva S32 fragment | captured run |
|---|---:|---:|---:|
| 2 voices | 3072 | 6144 | 62 fragments, 380928 bytes, zero mismatches |
| 6 voices | 9216 | 18432 | 62 fragments, 1142784 bytes, zero mismatches |

The startup graph contains coherent reference descriptors for both geometries.
The six-channel stream is CSD handle 257 (`open` sequence 2, format sequence 3):

```text
format=0x1c  rate=48000  channels=6  bits_per_sample=24
sign=1  interleaved=1  map=01 02 03 06 08 09
7 buffers * 18432 bytes; 18432 / (6 * 4) = 768 frames = 16 ms
```

The stereo stream is handle 271 (`open` sequence 128, format sequence 129):

```text
format=1  rate=48000  channels=2  bits_per_sample=16
sign=1  interleaved=1
7 buffers * 3072 bytes; 3072 / (2 * 2) = 768 frames = 16 ms
```

The matching `openAudioContextV2` payloads also say 48000 Hz and respectively
24 and 16 bits. There is no retained stereo rate, channel count, byte stride,
period, buffer size, buffer count, or interleave flag in the six-channel CSD
descriptor. In particular, the 18432-byte six-channel allocation is 24 bytes
per frame, proving that this interface uses a four-byte container for each of
its six 24-bit samples; it is not describing packed S16 downstream data.

Static recovery explains why the caps widening cannot leave a separate CSD
format field stale. `deva_csd_start_playback` at VA/file `0x26810` copies the
live negotiated PCM format into the CSD structure immediately before calling
`deva_csd_start_csd_session_playback`:

| instruction | CSD field | source |
|---:|---|---|
| `0x26948` | sample rate (`config+0x54`) | live PCM format `+0x0c` |
| `0x26954` | voices (`config+0x58`) | live PCM format `+0x10` |
| `0x26978` | bits/sample (`config+0x5a`) | `ado_pcm_format_bit_width(format)` |
| `0x26960` | signed (`config+0x5c`) | constant 1 |
| `0x26984` | interleave (`config+0x5e`) | live format flag bit 0 |
| `0x26a64` | start call | `config = device+0x188` |

The callee reads those fields at `0x253e0..0x253f0`; when voices are greater
than two, `0x25400` explicitly passes six and `0x25414` calls
`setStreamFormatPcmMultichannel`. The caps patch at file offsets `0xA6E1C`,
`0xA6E24`, and `0xA6E28` widens negotiation/allocation; it does not need a
parallel CSD-format edit because the CSD values are derived from the live
format, not another static caps record.

This closes the stale-CSD-descriptor hypothesis. The downstream
`session_csd_stream_playback` / cursor boundary described below is now measured
as well.

Static-analysis fingerprint: ELF32 little-endian ARM EABI5 PIC, image base 0,
`.text` VA/file `0x84F8`, GNU build ID
`9168acff553fc86d6a3c991613d620fd`, SHA-256
`8b55b8d8f02b97d6f30191b54ef8a847e4706067f21cbf068d4b02a563ea84c9`.

## CSD submission-order audit: faithful single pass

The complete static recovery, harness chronology, rejected approaches, exact
commands, hashes, and remaining boundary are preserved in
[`SUBMISSION_ORDER_AUDIT.md`](SUBMISSION_ORDER_AUDIT.md).

The stub now executes the CSD-backed preferred output and associates each
`session_csd_stream_playback` copy at `0x6924c..0x6926c` with the following
`csd_write` at `0x69318`. It records the ring-relative source offset, payload
length, ring-slot generation, and the high-16 PCM in actual submission order.
`csd_write` returns descriptor word 1 (18432 audio bytes), not the 36-byte
descriptor size.

Seven CSD buffers are recycled through the registered BUF_DONE callback at the
existing post-cursor timer boundary. deva calls io-audio's `dma_interrupt` at
`0x6e890`, after updating the cursor at `0x6e864..0x6e874`; the first imported
call inside `dma_interrupt` is `pthread_mutex_lock` at io-audio `0x10f7a8`.
Because the main executable's `dma_interrupt` takes ELF symbol precedence, the
harness redirects only io-audio's already-resolved mutex GOT slot `0x124f78`,
requires the same `pthread_self()` that called `csd_write`, drains the queued
callbacks, and passes through to the saved libc target. It does not dereference
the opaque subchannel, modify deva, or add a thread.

`SND_PCM_START_FULL` prefills the ring before the traced timer starts; without
that, serial logging can manufacture a slot-1 startup underrun. The frozen
62-fragment window in `build/qsa-emu/qsa-submit-final2.log` reports:

```text
TIMER_CURSOR_SUMMARY ring=0 advances=62 delta_0x4800=61 delta_0x2400=0 delta_other=0
CSD_REGION_SUMMARY written_generations=62 distinct_submitted=62 submitted_once=62 repeated_generations=0 duplicate_submissions=0 missing_generations=0 unknown=0
CSD_BYTE_SUMMARY submitted=1142784 written=1142784 difference=0
CSD_SUBMIT_RENDER_SUMMARY fragments=62 frames=47616 samples=285696 pcm_bytes=571392 reference_mismatches=0 overflow_calls=0
```

Thus every consecutive cursor delta is `0x4800`, generations 1 through 62 are
each submitted exactly once, and ring-backed submitted bytes equal written
bytes. Two zero-filled 18432-byte CSD startup-prime writes precede the first ring
pull and are reported separately; they are not ring consumption and are not in
the 62-fragment comparison WAV.

The carver produced
`build/qsa-emu/submission-order-final2/dsp_submission_order.wav`. It is 6-channel
48-kHz S16, 47,616 frames (0.992 seconds), and its PCM is bit-identical to the
first 47,616 reference frames: zero mismatches, PCM SHA-256
`f0f37e6c1f2e5d32d0c938053b9f2f70fbac25868e61f6e1fe2342ae0c90c312`.
The WAV SHA-256 is
`4daa9fc9915ce9929fa1874d2a4c5bf531d996be4c733cb3e12d5feae4909112`.

After `DSP_SUBMIT_COMPLETE`, the timer can pull one stale slot while the client
flush races STOP; that post-marker teardown pull is outside the frozen window.
It is not included in the statistics or WAV. This closes the ring cursor and
submission-order path through `csd_write`; the measured 62-fragment playback is
a faithful single pass, so the vehicle fault lies downstream of this boundary.

## Final result

The harness accounts continuously for the bytes from `qsa_probe`, through QSA's
conversion, into deva's DMA ring, and through both requested DSP interpretations.
Six-channel widening is a left shift by 16, channel order is intact, and A is
bit-identical to the input.

`=== DSP_RENDER_COMPLETE ===` is printed before the known minimal-guest teardown
fault. The result is complete at that marker; stop QEMU with Ctrl-C afterward.

## Virtual-device topology audit: one shared CSD device

The complete static recovery, payload diff, id-namespace table, hashes and
remaining boundary are in
[`../../docs/audio/QNX_AUDIO_TOPOLOGY.md`](<../../docs/audio/QNX_AUDIO_TOPOLOGY.md>).

`mpl1_int_ent` and `mpl5_dio_ent` are DEVA input timers. The mixer router maps
ENT inputs 1 and 5 through the same output 128 (`TDM_OUT_MPL`) to one CSD
device with `dev_id=0x4e` (78). The measured default MPL1 stream/context are
six-channel, 48 kHz, 24-bit. Device enable has no channel member; it contains
only id, rate and bit depth. There is no separate stereo MPL5 CSD/ACDB device,
but the later audit below proves that MPL5 builds a stereo stream on the shared
device.

An isolated six-voice QEMU open on MPL1 produces
`CONFIGURE(78) -> ENABLE(78,48000,24) -> START` because platform init installs a
default MPL1-to-TDM route. The same open on MPL5 produces no CSD traffic and
slog reports `stream_type=1`: Audioman is absent, so the runtime MPL5 route was
never installed and its timer intentionally consumes through
`session_void_stream_buffer`. When routed, MPL5 resolves to the same output
session as MPL1.

Use the topology mode only for this comparison:

```sh
QSA_DEVICE_TOPOLOGY=1 QSA_PROBE_DEVICE=0 QSA_PROBE_VOICES=6 \
  ./tools/qsa-emu/build_ifs.sh
QSA_DEVICE_TOPOLOGY=1 QSA_PROBE_DEVICE=3 QSA_PROBE_VOICES=6 \
  ./tools/qsa-emu/build_ifs.sh
```

The mode suppresses the unrelated capture outlet and dumps slog; without it,
the established submission-order harness remains the default.

## Router-input width audit: MPL5 remains stereo at CSD setup

The complete static trace and fix constraints are in
[`../../docs/audio/QNX_AUDIO_TOPOLOGY.md`](<../../docs/audio/QNX_AUDIO_TOPOLOGY.md>).

The effective router width is selected when a DEVA timer is attached to the
shared CSD session. `deva_update_session_info_from_list @0x54848` treats the
timer record's capability minima as active values:

```text
record min_voices   (+0x14) -> session channels (+0x84)
record min_frag     (+0x1c) -> session buffer   (+0x6c)
```

That yields six channels/`0x4800` for MPL1, but two channels/`0x1800` for MPL5.
The current dynamic acquire helper correctly makes a negotiated six-voice
MPL5 DMA fragment `0x4800`; it does not update the CSD session. Nothing in
acquire, prepare, or trigger re-provisions the stream.

This distinction also scopes the earlier harness result. The six-channel CSD
descriptor and faithful `csd_write` submission belong to MPL1's default route.
The isolated MPL5 run cannot install Audioman's route and therefore cannot
measure its descriptor. Do not infer MPL5 route width from common startup
traffic or from the MPL1 session.

On the real runtime path, `session_csd_stream_playback @0x69084` uses the stale
session buffer size for both `memcpy` and `csd_write`: it copies/submits only
`0x1800` of each `0x4800` DMA fragment. For correlated six-channel input this
is a threefold sample repeat inside each 16-ms block followed by a 512-frame
jump, producing a 62.5-Hz seam while preserving total duration.

No harness source or binary was changed for this audit. A static six-only test
can force MPL5 `min_voices=6` and `min_fragsize=0x4800`, but the production fix
must read the recovered live PCM voice/fragment fields and rebuild CSD on a
2-to-6 or 6-to-2 transition so the clean stereo mode remains supported.
