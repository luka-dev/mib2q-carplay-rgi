# bench — does the port fit in real time on the head unit?

Everything else about this port is about correctness. This is the one question that is about
cost, and the answer is written up in [`../PORT_STATUS.md`](../PORT_STATUS.md), stage k. This
directory is how it was obtained, so it can be obtained again.

## The instrument, and what it is not

The head unit is an APQ8064 Krait at about 1.7 GHz. Nothing here is a Krait. Two machines were
used and neither is the target, so each is stated for what it is:

| | what it measures | what it is worth |
| --- | --- | --- |
| **QNX 6.5 / armle-v7 under QEMU with `-icount`** | the number of ARMv7 instructions the shipping binary retires per packet — exactly, deterministically | the instruction stream is the target's own; the cycles it takes are not measured and are assumed |
| **this arm64 Mac** | wall clock, on a different compiler and a much wider core | an optimistic floor, quoted as one |

`-icount shift=N` makes QEMU advance the guest's clock by 2^N ns per instruction retired, so a
guest-side timer reads an instruction count. `apac_bench` refuses to believe that without
evidence: it times two assembly loops that differ by exactly two instructions per iteration and
one loop that is memory bound, and only converts ticks to instructions when the two exact loops
agree to within 2 % **and** the memory-bound loop is no more than 5x dearer per instruction. On
the target it prints

    the two exact loops agree to 0.12% and the memory bound one is only 1.75x dearer,
    so this clock counts instructions: 1 tick = 15.95 instructions

and on this Mac's real silicon the same check fails — the cache-missing loop comes out 30 to 180
times dearer per instruction — so no instruction conversion is offered there. That is the check
working, not a defect.

## The data

The real capture, never synthetic input: `captures/apac_5_1_live_20260831_0205`, 3980 frames of
which 3977 carry an APAC packet. `make_blob.py` folds the decoder cookie and every decrypted
packet plaintext into one file so a QEMU boot image can carry it.

**The session key is not in the blob and never reaches the target.** The AEAD is driven there
with an all-zero key over records of the real lengths — ChaCha20 and Poly1305 do work that
depends on length and not on bytes, so it costs exactly what the real key costs — and the
payload ChaCha20, which a failed authentication skips, is called explicitly. On the host that
decomposition is checked against the real thing: `--key --stream` runs the same chain over the
capture's own encrypted bytes with the session's own key, and the two agree to 0.7 % on the mean
and 0.4 % on the worst packet.

## Running it

```sh
apac_decoder/bench/make_blob.py                     # -> /tmp/apac_bench.blob

# this machine
apac_decoder/bench/build_host.sh
apac_decoder/.build-bench/apac_bench --blob /tmp/apac_bench.blob --reps 20 \
    --csv /tmp/host.csv \
    --key captures/apac_5_1_live_20260831_0205/shk.bin \
    --stream captures/apac_5_1_live_20260831_0205/spatial_apap_5.bin

# QNX 6.5 / armle-v7, instruction counts (about 20 minutes for --reps 2)
apac_decoder/bench/build_qnx_bench.sh
apac_decoder/bench/qnx_qemu_bench.sh -- --reps 2

# how much of the 1.6 MB decoder object one packet touches (host only)
c++ -std=c++11 -O2 -fno-exceptions -fno-rtti -Iapac_decoder/include -Ihook/audio \
    apac_decoder/bench/footprint.cpp apac_decoder/.build-bench/bench_apap_crypto.o \
    apac_decoder/.build/*.o -o apac_decoder/.build-bench/footprint
apac_decoder/.build-bench/footprint --blob /tmp/apac_bench.blob --count 300
```

`--reps N` keeps the per-packet **minimum** across N runs, which is what removes scheduling noise
from a wall clock without flattering the tail: it is a minimum per packet, not a minimum over
packets, so the worst packet stays the worst packet. On the target one repetition is already
deterministic — one and two repetitions differ by under 0.1 % on every figure — so `--reps 2` is
a check rather than a necessity.

## What each pass does

| pass | what it times |
| --- | --- |
| chain | framer, AEAD, ChaCha20, TLV split, decode, channel permute — one clock read either side, so the headline number carries no instrumentation |
| decode | `spa_apac_decode_s16()` alone, same shape |
| stages | the same chain with a clock read between every stage; attribution only, and the clock's own cost is printed beside it |
| split | decode again through the C++ decoder with a spectrum sink installed. The sink fires after the whole bitstream is parsed and before the first filter bank call, so it cuts a packet into frame layer and transform |
| components | six long MDCTs and six 512-point FFTs driven directly, which is what says how much of the transform is the FFT |
| stack | the deepest byte a decode and a whole chain touch, by painting the stack below the caller and scanning it afterwards |

## Two things this deliberately does not do

* **It does not touch `hook/`, `src/`, `include/` or `tools/build_qnx.sh`.** The flags in
  `build_qnx_bench.sh` are copied from `scripts/build_hook.sh` rather than shared with it, which
  means they can drift. Factoring the flag set into one file both could source would be the right
  fix and is left to whoever owns that script.
* **It does not deploy or run anything on the car.** Every number here is from this machine or
  from QEMU.
