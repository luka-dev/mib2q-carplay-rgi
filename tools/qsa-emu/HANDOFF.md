# Handoff: completed QSA/deva DMA capture

The harness now captures and validates the PCM that `io-audio` copies into
deva's DMA ring. The last open job from the previous handoff is complete.

## Run it

    ./tools/qsa-emu/build_ifs.sh                    # patched driver (default)
    QSA_STOCK_DEVA=1 ./tools/qsa-emu/build_ifs.sh   # stock-driver A/B

    /Users/luka/Desktop/AUDI_2/Tools/qemu-src/build/qemu-system-arm \
      -M virt -m 512 -cpu cortex-a15 -icount shift=auto \
      -device loader,file=build/qsa-emu/qsa.bin,addr=0x40200000,force-raw=on,cpu-num=0 \
      -display none -serial stdio -monitor none

The useful run completes in about 30 seconds. Success is printed before the final
marker:

    RESMGR_DMA open /tmp/resmgr_dma_0_184320.bin rc=18432 size=18432 offset=16 ...
    RESMGR_VERIFY ring=0 voices=6 calls=1 bytes=18432 mismatches=0 FULL_FRAGMENT_PASS
    ...
    === AUDIO STACK PROBE DONE ===

The minimal guest faults during shutdown after that marker. This is a teardown
problem, not a probe failure; stop QEMU with Ctrl-C after the result appears.

## What is proved

The six-channel resource-manager copy contains one complete 768-frame fragment:

    768 frames * 6 voices * 4 bytes = 18,432 bytes = 4,608 S32 samples

Every word matched the deterministic input pattern. Client channel `c` contains
S16 `0x1000 * (c + 1)`, and the DMA ring contains S32
`0x10000000, 0x20000000, ... 0x60000000` in the same channel order. The final
six-channel result was 4,608 comparisons and zero mismatches.

Only the first six-channel write crosses `resmgr_msgread`; the other 61 do not
call that import, consistent with reuse of the already mapped buffer path. All
62 probe fragments are deliberately identical, so a complete first-fragment
comparison settles the byte-for-byte result for this harness. The stereo control
crossed the hook repeatedly and reached at least 20 full fragments with zero
mismatches.

This establishes the entire observable path from the probe through QSA's
S16-to-S32 conversion and into deva's DMA ring. QSA widens with a left shift by
16 and preserves all six channels.

## Why the old hooks did not fire

`ado_pcm_write_block` at `io-audio+0x11094` computes:

    destination = ring_base + fragment_index * fragment_size

It then calls imported `resmgr_msgread` at `io-audio+0x1111c0`. QNX copies the
client message directly into the supplied ring address, so there is no libc
`memcpy` to interpose. The CSD stand-in is already preloaded into `io-audio`; its
`resmgr_msgread` wrapper now:

1. calls the real function through `RTLD_NEXT`;
2. recognizes destinations inside the recorded malloc-backed DMA rings;
3. appends the post-copy fragment to `/tmp/resmgr_dma_<ring>_<ring-size>.bin`;
4. compares every S32 word with `qsa_probe`'s known channel pattern; and
5. prints `FULL_FRAGMENT_PASS`, `PASS_SO_FAR`, or `FAIL` on the serial console.

The diagnostic `memcpy` wrapper remains in the stub, but it is not the PCM path.

## Driver dump path: useful on the car, not this trigger

The driver also has its own lower-level capture point. At `0x6e85c`,
`session_deva_timer_thread` calls:

    deva_timer_data_dumps(timer, dma_base + cursor, frag_size)

That route writes `/var/log/<timer_name>_<n>.bin`, but it is gated by a per-timer
enable byte, session type 2, and a non-null session pointer. The enable is set by
`deva_data_dump_ctrl`; its callback is initialized by `deva_data_dump_init`,
which the harness never calls. The emulated play timer starts but does not
receive fragment ticks, so the driver's dump is not a reliable harness trigger.

On a real head unit, the audio service behind `/dev/audio_service` is the likely
owner of that control path. It remains the better point for measuring what the
driver sends onward to the real DSP.

## Things not to rediscover

* Firmware binaries unpack without `+x`, and mkifs preserves host modes.
* `io-audio` needs `CACHE_NOP=1`.
* The BSP libc lacks the firmware's stack-protector exports; the CSD stub supplies
  them, along with `rtpd_init`/`rtpd_deinit`.
* deva blocks in `Resource_BlockWait` until `/dev/audio_service` answers.
* `csd_query_driver_version()` takes no arguments and returns the version.
* `csd_open` argument 1 is not a string; do not dereference untyped parameters.
* `ALLOC_PMEM` word 3 is an address used with `mem_offset64`, not a descriptor.
* `csd_read` during capture start is a zero-byte probe. Returning 36 makes the
  driver fail with rc `0x24`; return zero.
* Wait for a real `/dev/snd` node, not merely the directory.
* Playback and capture device numbering are separate.
* `sse_bundle_1` is a DSP-to-host capture route, not the playback output; reads
  return EIO against the DSP stub.
* Do not add a thread inside `io-audio`; a watcher destabilized the guest.
* CSD PMEM pools remain empty. Playback client PCM enters the malloc-backed DMA
  ring through `resmgr_msgread`.
* `build/qsa-emu/qsa_probe` is a working ignored build artifact. Do not try to
  relink it against the extracted firmware `libasound.so.2` with the GNU cross
  linker; that library is not usable that way.

## Files involved

* `csd_stub.c`: DMA-ring discovery, `resmgr_msgread` capture, and word validator.
* `qsa_probe.c`: deterministic six-channel and stereo test patterns.
* `qsa.build`: boots the real stack, runs the probe, and prints the completion
  marker without post-probe utilities that destabilize this minimal guest.
* `build/qsa-emu/libcsdIpcClient.so`: rebuilt ARM shim used by the current IFS.
