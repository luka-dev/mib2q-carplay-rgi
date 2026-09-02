# Building the working deva — three steps, in order

Discrete 5.1 CarPlay audio works on the car with the binary produced by this chain.
Confirmed by ear on 2026-09-02, after a month in which every other layer was measured
clean and exonerated.

    ./patch_deva.sh                 # stock -> deva-ctrl-qc.so
    ./patch_deva_csd_live.sh        # -> deva-ctrl-qc-csdlive.so
    ./patch_deva_mirror_mpl1.sh     # -> deva-ctrl-qc-mirror.so     <- deploy this one

Deploy `deva-ctrl-qc-mirror.so` to `/mnt/app/armle/lib/dll/deva-ctrl-qc.so`. The driver is
mmap'd by a running `io-audio`, so an in-place overwrite is refused: copy to `<path>.new`,
`chmod 755`, then `mv` over it. It becomes active at the next reboot.

## What each step does, and why all three exist

**1. `patch_deva.sh` — accept six voices at all.** Widens `mpl5_dio_ent`'s caps maxima
(`max_voices` 2->6, `max_fragsize` 0x1800->0x4800, `max_buffer` 0xF000->0x2D000) and adds the
conditional helper at `deva_mib2_aquire`, because that function reads the *active* DMA
fragment from `min_fragsize`. Without the helper a six-voice open advances 0x1800 per 16 ms
and plays exactly one third speed.

**2. `patch_deva_csd_live.sh` — prefer the live channel over the table.**
`deva_update_session_info_from_list @0x54848` copies `min_voices` into CSD session `+0x84` as
the active channel count and `min_fragsize` into `+0x6c` as the active buffer size. This step
redirects that to a helper that reads the attached PCM channel instead (timer `+0x11c` ->
channel, `+0x10` voices, `+0x94` fragment), falling back to the record.

With step 3 in place this is redundant, since the record already carries the right values. It
is kept because it is correct, because it costs nothing, and because it is the right shape if
the attach ordering ever changes.

**3. `patch_deva_mirror_mpl1.sh` — make the attach-time geometry right.** This is the step
that actually fixed the car. Step 2 alone did not, and the unit's own slog says why:

    00:00:38  session_csd_setup: ... DEVICE_ID = <4> is being attached
    00:00:38  pcm format => sample_rate=48000, channels=2, bit_per_sample=24
    00:00:39  session_deva_timer_start
    00:00:52  [hook] setup actual=6ch/48000Hz/9216-byte fragment

Audioman attaches the route and starts the timer **fourteen seconds before the PCM channel
exists**, so the helper legitimately takes its fallback and the session is built stereo.
Nothing rebuilds it afterwards: acquire only stores the channel, prepare is a logging no-op,
and trigger just starts the existing timer. CSD then copied 0x1800 of every 0x4800 fragment --
256 frames of 768 -- and tore every 16 ms. That is 62.5 Hz, which is the continuous tick that
was heard, with the sample clock left perfectly correct, which is what was measured.

Rebuilding the session late would need `session_csd_teardown(session)` then
`session_csd_setup(session, timer)`, and both pointers live in
`deva_mixer_router_configure_switch_routing`'s stack frame — not reachable from the PCM path
without substantially more reverse engineering on a driver that lives in a car. Making the
minima match `mpl1_int_ent` makes the geometry right at attach time, so no late rebuild is
needed at all.

## The objection that turned out to be wrong

Earlier notes said the minima must never be widened, because a stereo open would fall below
`min_voices` and fail. Two things contradicted that, and the car settled it: the QEMU harness
showed QSA quietly inserting a channel conversion rather than refusing, and `mpl1_int_ent` has
served stereo clients with `min == max == 6` all along. With every open landing at six voices,
`channel->voices` is always 6, so the step-1 helper keeps selecting 0x4800 and the whole
geometry is self-consistent instead of split between a widened DMA side and a stereo session.

## Reverting

Rerun `patch_deva.sh` for the asymmetric record, or restore the stock driver. Any change needs
a reboot to take effect, since `io-audio` maps the driver once at start-up.
