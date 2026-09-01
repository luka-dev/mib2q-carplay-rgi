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

## Next

Two things, in order.

1. The pools stay empty, so the audio does not reach the CSD PMEM the stub owns.
   `deva_mib2_aquire` mallocs its own DMA buffer (`malloc(10 * frag_size)`) and the
   timer advances through that; the PMEM pools are a separate, smaller set
   (buffer_count=8, buffer_size=1536). The capture point has to move to whatever
   copies one into the other, or the dump has to trigger on the timer rather than
   on an ioctl.
2. The guest faults on the way out, after both tests have written. Harmless to the
   measurements taken so far, but it needs fixing before the harness can run
   unattended.
