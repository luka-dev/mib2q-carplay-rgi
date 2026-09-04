# CUpGAN-A disclosed-core benchmark

This directory answers one narrow question: how expensive is the dense
ConvNeXt core disclosed for CUpGAN-A when compiled for the head unit?

It is **not an audio renderer and not a CUpGAN implementation**. The paper
publishes neither trained weights nor enough detail to reproduce the IBN and
spectral output conversion exactly. The benchmark therefore implements only
the fully dimensioned portion of Figures 3 and 4:

- tensor shape `21 x 513 x T`;
- four causal ConvNeXt blocks;
- per block: depthwise `3 x 3`, pointwise `21 -> 84 -> 21`, layer norm, GELU,
  FiLM and residual addition;
- 512-sample STFT hop.

STFT/iSTFT, the six-to-21 IBN, the final six-channel projection and the control
network are excluded. Results are a lower bound for a complete backend.

The source is C. Uhle, P. Weyers, and J. Zimmermann, “Implementing Two
Upmixing Concepts With One Conditional GAN,” AES 2026 Automotive Audio
Conference, paper 9: <https://aes.org/publications/elibrary-page/?id=23403>.
Do not commit the paper PDF; AES supplies it through its open-access download
endpoint but retains copyright.

## Host

```sh
tools/cupgan-bench/build_host.sh
build/cupgan/cupgan_bench_host --frames 128 --warmup 4
```

This is a wall-clock floor on the local machine, not a head-unit result.

## QNX 6.5 / ARMv7 instruction count

```sh
tools/cupgan-bench/build_qnx.sh --variant 4.9 --codegen scalar
tools/cupgan-bench/qnx_qemu_bench.sh \
  --variant 4.9 --codegen scalar -- --frames 1 --warmup 2
```

`scalar` uses the hook's shipping C flags. `neon-auto` is an experiment using
auto-vectorization and NEON. `neon-fast` additionally allows reassociation and
other fast-math transformations that are normally required to vectorize these
reductions. `neon-manual` packs weights by four output channels and explicitly
uses NEON for the two pointwise layers. None is a shipping configuration yet:

```sh
tools/cupgan-bench/build_qnx.sh --variant 4.9 --codegen neon-auto
tools/cupgan-bench/qnx_qemu_bench.sh \
  --variant 4.9 --codegen neon-auto -- --frames 1 --warmup 2

tools/cupgan-bench/build_qnx.sh --variant 8.5 --codegen neon-fast
tools/cupgan-bench/qnx_qemu_bench.sh \
  --variant 8.5 --codegen neon-fast -- --frames 1 --warmup 2

tools/cupgan-bench/build_qnx.sh --variant 8.5 --codegen neon-manual
tools/cupgan-bench/qnx_qemu_bench.sh \
  --variant 8.5 --codegen neon-manual -- --frames 1 --warmup 2
```

The runner boots the target's real QNX 6.5 userspace under QEMU `-icount`.
Inside the guest, two known-length assembly loops and a memory-stride loop
classify `ClockCycles()` as an instruction counter before the program converts
ticks. QEMU still does not model Krait pipelines or caches, so conversion from
instructions to CPU load uses explicit IPC assumptions, as in the APAC bench.

Recorded results live under [`results/`](results/).

## Interpretation guardrail

The four disclosed blocks alone conventionally count as 7,627,284 MAC per STFT
frame, or about 715.1 million MAC/s at 48 kHz and a 512-sample hop. This is
already 2.49 times the paper's total figure of 264 million MAC/s at 44.1 kHz
when that figure is scaled to 48 kHz. Until the authors' implementation or a
clarification is available, quote both figures and label the discrepancy. Do
not substitute the benchmark's deterministic pseudo-weights for trained audio
weights.
