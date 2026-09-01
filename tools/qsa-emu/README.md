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
resource-manager copy into that ring.

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
S16->S32 widening leaves them where the DSP expects them is exactly the open
question, and it is now answerable here rather than by ear in a moving car.

## What it proved

The stack runs end to end. The card registers, every device enumerates by name,
and both configurations open and write:

    snd_cards() = 1
    card 0 dev 3: 'mpl5_dio_ent'
    device geometry: voices 2..6, rate 48000..48000, frag 6144..18432, formats 0x1000
    plugin_params(S16,6ch,48000Hz,frag=9216) -> 0
    setup readback: 6 voices, 48000 Hz, format 4, frag 9216, frags 10
    wrote 62 fragments of 768 frames

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
the device. What never happens is an emulated per-fragment tick or submission —
over a run where the probe writes 62 fragments the command histogram does not
move at all beyond the 64 pool allocations.

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
| client writes to `mpl5_dio_ent` at six voices | works, 62 fragments of 768 frames |
| QSA converts S16 -> S32 | works, measured correct above |
| resource manager copies into deva's DMA ring | captured; a full six-channel fragment has zero mismatches |
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

    RESMGR_DMA open /tmp/resmgr_dma_0_184320.bin rc=18432 size=18432 offset=16 ...
    RESMGR_VERIFY ring=0 voices=6 calls=1 bytes=18432 mismatches=0 FULL_FRAGMENT_PASS

That is one complete 768-frame fragment: 18,432 bytes, or 4,608 S32 samples.
Every word matched `qsa_probe`'s deterministic six-channel input after the
expected `S16 << 16` widening. The stereo control reached at least 20 complete
captured fragments with zero mismatches.

Only the first six-channel write crosses `resmgr_msgread`; the remaining writes
do not call that import, consistent with reuse of the already mapped buffer path.
The probe deliberately writes the same channel constants in all 62 fragments,
so comparison of the complete first fragment is the definitive byte-for-byte
result for this deterministic test.

## Final result

The harness job is complete. It accounts for the bytes from `qsa_probe`, through
QSA's conversion, and into deva's DMA ring. Six-channel widening is a left shift
by 16, channel order is intact, and the DMA fragment has zero mismatches.

The final `=== AUDIO STACK PROBE DONE ===` marker appears before a known fault in
the minimal guest's shutdown path. The verification is complete before that
teardown fault; stop QEMU with Ctrl-C after the marker.
