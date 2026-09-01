# Handoff: finishing the QSA/deva harness

Everything here is built and runs. This is what is left, and what has already been
paid for so it does not get paid for twice.

## Run it

    ./tools/qsa-emu/build_ifs.sh                    # stages + mkifs, all in Docker
    QSA_STOCK_DEVA=1 ./tools/qsa-emu/build_ifs.sh   # same, with the unpatched driver

    /Users/luka/Desktop/AUDI_2/Tools/qemu-src/build/qemu-system-arm \
      -M virt -m 512 -cpu cortex-a15 -icount shift=auto \
      -device loader,file=build/qsa-emu/qsa.bin,addr=0x40200000,force-raw=on,cpu-num=0 \
      -display none -serial stdio -monitor none

Give it ~90 s. Everything lands on the serial console: the guest's own slog, the
probe's output, and the CSD stub's trace.

## What works

The real firmware `io-audio` and `deva-ctrl-qc.so` run with no Qualcomm hardware.
The card registers, all 27 PCM devices appear, `mpl5_dio_ent` (playback dev 3)
opens at six voices and takes 62 fragments of 768 frames, the CSD stub answers
every call the driver makes, and playback sessions start
(`setStartSession: SUCCESS session handle 277`).

## The one job left

**Get audio out of the driver, so it can be compared against what went in.**

The driver already has the mechanism. `session_deva_timer_thread` @0x6E85C calls,
every tick:

    deva_timer_data_dumps(timer, dma_base + cursor, frag_size)

which `write()`s the fragment to `/var/log/<timer_name>_<n>.bin`. Three gates:

    if (!*(BYTE*)(timer + 336)          /* per-timer enable */
     || *(DWORD*)(timer + 328) != 2     /* session type must be CSD */
     || !*(DWORD*)(timer + 332))        /* session pointer */
        return 0;

* `+336` is set **only** by `deva_data_dump_ctrl` @0x30c88, a runtime command:
  word 0 of its second argument enables (non-zero) or disables (zero), offset 12
  optionally overrides the path. It walks timer and CSD lists gathered by a
  "get_fn" held in `off_B0C54` @0xb0c54.
* `off_B0C54` is assigned in `deva_data_dump_init` @0x2fd0c, which refuses unless
  `platform_id` is 2, 5 or 7 (`(1 << id) & 0xA4`).
* **In the harness `deva_data_dump_init` is never called** — no line from it in any
  run — so `get_fn` is null and `deva_data_dump_ctrl` would fail with
  "get_fn not initialized!". Finding what should call it is the first thread to
  pull.
* The slot at 0x9b710 that holds `deva_data_dump_ctrl` is a relocation filled at
  load time: a callback in a table something outside the driver invokes. On the
  head unit that something is the audio service behind `/dev/audio_service`, which
  here is only `devstub` answering that the path exists.

Second thread, if the dump stays out of reach: the stub already **owns the audio
buffer**. A `malloc` interposer (only effective with the stub preloaded into
io-audio, not merely pulled in as the driver's dependency) catches
`deva_mib2_aquire`'s ring:

    DMA candidate 0: 184320 bytes   (10 x 18432, six-voice fragment)

What is missing there is a moment to read it: `csd_ioctl` and `csd_read` both go
quiet once the timer takes over. A `memcpy` interposer filtered on a destination
inside that ring is written and in place but has not fired — worth checking
whether the copy is done by something other than `memcpy`, or by io-audio rather
than the driver.

## Traps already paid for

* Firmware binaries unpack without `+x`, and mkifs copies the host file mode.
* `io-audio` needs `CACHE_NOP=1`, same as the dio harness.
* The firmware is built with stack protector; the BSP libc does not export
  `__stack_chk_guard`/`__stack_chk_fail`, so the stub does. `rtpd_init` too.
* deva blocks in `Resource_BlockWait` until `/dev/audio_service` answers.
* `csd_query_driver_version()` takes **no arguments** and returns the version.
* `csd_open`'s first argument is **not** a string. Never dereference a parameter
  whose type has not been recovered from a call site — printing it as `%s` crashed
  the driver.
* `ALLOC_PMEM` (0x11420) word 3 is an **address**, not a descriptor: the caller
  passes it to `mem_offset64(addr, -1, ...)`. It must be a mapping of anonymous
  physical memory (`shm_ctl(fd, SHMCTL_ANON|SHMCTL_PHYS, 0, size)`).
* `csd_read` during capture start is a probe: the driver logs
  "csd_read() returned [36] bytes, when expecting 0!" and fails with rc 0x24.
  Return 0.
* Waiting on `/dev/snd` races the driver — the directory appears long before the
  27 devices do. Wait for a node.
* Playback and capture have separate device numbering.
* `sse_bundle_1` (capture 27) is **not** the playback path: its session is
  `csd_capture_sse`, it captures *from* the DSP. Reads return EIO against a stub.
* Its plugin path refuses `snd_pcm_plugin_params` with ENOENT; `snd_pcm_channel_params`
  accepts the identical geometry.
* A blocking read hangs the probe, and `sloginfo` runs after it returns — every
  blocked run is blind on the driver side. Use `snd_pcm_nonblock_mode`.
* **Do not create a thread inside io-audio.** A watcher thread in the stub
  destabilised the guest and killed the run before the trace printed.
* The CSD PMEM pools stay empty: `POOLSUM(all zero) 0/18432 ...`. Playback does not
  travel through them in this configuration.

## What this harness already settled

QSA's six-voice widening is correct, byte for byte: S16 `0x1000 0x2000 ...`
becomes S32 `0x10000000 0x20000000 ...`, a left shift by 16 with channel order
intact. It also showed that `snd_pcm_plugin_setup` reports the *client's*
geometry — a stereo-only device answered "6 voices" — which is why
`hook/audio/audio_hook.c` now checks `snd_pcm_channel_info` instead. Neither of
those needs redoing.
