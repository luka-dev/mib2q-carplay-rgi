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

| | |
|---|---|
| guest boots, script runs | yes |
| `io-audio` runs as a resource manager | yes, needs `CACHE_NOP=1` |
| `deva-ctrl-qc.so` loads | yes, once the stub also exports `__stack_chk_guard`/`__stack_chk_fail` and `rtpd_init` |
| `/dev/audio_service` | `devstub`, borrowed from the dio harness — deva blocks on it before mounting a card |
| CSD init through the stub | `deva_csd_init: LEAVE (rc = 0x0)` — playback and capture init both succeed |
| MIB platform selected | `deva_init_platform_mib: ENTER` with `platform_id=mib2` — this is the caps table holding `mpl5_dio_ent` |
| capture probe records calls | `/tmp/csd_trace.log` gets `csd_init`, `csd_query_driver_version` |
| `/dev/snd` appears | **not yet** |

## The blocker

io-audio exits right after `deva_init_platform_mib: csd_init() SUCCEEDED`, and the
trace shows why the stub is not good enough yet:

    csd_query_driver_version(out=64 len=268763137)

Those arguments are nonsense, so the stub's signature does not match the real
one. deva almost certainly gates initialisation on the version it reads back.
Recovering the true prototypes — from the call sites in the IDA database, which
is already open on this binary — is the next step; every other CSD entry point
will need the same treatment before the card mounts.
