# MHI2Q audio stack under QEMU

Runs the head unit's real `io-audio` and `deva-ctrl-qc.so` on a machine with no
Qualcomm ADSP, so the layer between `snd_pcm_plugin_write` and the hardware can be
inspected without the car.

## Why

On the car every counter above the driver is clean — correct 48 kHz timing, 768
frames per 16 ms fragment, zero underruns, and PCM that is provably correct
(decoded offline and listened to). The sound is still wrong. Everything left is
below the write: QSA's S16->S32 conversion at six voices, io-audio's mixer, and
deva's DMA. None of it is observable from a log on the unit.

## How

`libcsdIpcClient.so` is the only substitute. deva imports exactly eight symbols
from it and builds every higher-level CSD call (`openAudioStreamV2`,
`setStreamMultichannel`, `setStreamFormatPcmMultichannel`, `allocateCsdBufferPool`)
on top of them, so standing in for those eight runs the whole driver — and makes
the stub the natural capture point: what deva hands down is exactly what would
have reached the DSP.

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

`/tmp/csd_trace.log` records the conversation the driver believes it is having
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

## Next

The driver now runs complete playback sessions against the stub:

    allocateCsdBufferPool: ENTER (buffer_count=8, buffer_size=1536) -> LEAVE (rc=0x0)
    openAudioContextV2: SUCCEEDED (dev_id=11)
    streamAttach: SUCCEEDED
    enableContext: SUCCEEDED
    setStartSession: SUCCESS session handle 277

64 PMEM pools are handed out, and the first ones come back at 18432 bytes —
0x4800, exactly the six-voice fragment the deva patch produces, which is a useful
independent confirmation that the driver really is running the six-channel
geometry here.

What made that work is the contract recovered from `allocateCsdBufferPool`
@0x2e374: `csd_ioctl(h, 0x11420, {size, pid, 0, 0}, 16)` and the DSP side is
expected to write a shared-memory **file descriptor** into word 3. The driver
then calls `mem_offset64` on it, so an ordinary `shm_open` is not enough — the
object has to be anonymous physical memory (`shm_ctl(fd, SHMCTL_ANON|SHMCTL_PHYS,
0, size)`). Returning success without filling word 3 is what made the driver
memset a null pointer.

It still dies a little later, the same way:

    io-audio terminated SIGSEGV at libc.so.3@memset+0x28, ref=0000003e

`0x3e` is not a pointer, so another ioctl output field is being consumed as one.
The method is now established: find the caller in the IDA database, read what it
expects back, fill it in. Each one moves the driver further.

Once it survives, `qsa_probe` writes its pattern and the pools — which this stub
owns and keeps mapped — hold exactly what would have reached the DSP. Comparing
six voices against two, the configuration that works on the car, is then a diff.
