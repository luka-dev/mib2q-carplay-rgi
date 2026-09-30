# Unified upmix standalone runtime

## 🧩 Scope

Track B's default causal shared graph, separate from the hook. One immutable model contains all four source operators and six family heads. Every stream owns an `UmxState`; reset it on a new stream. Prediction allocates no heap memory. Scene heads in the first exported checkpoint are **untrained**.

The float reference matches PyTorch on four frozen TRAIN crops. The first post-training int8 export **fails the drum SDR screen**. Neither build qualifies deployment, device timing or unseen-song acoustic quality. See the [evidence report](../../../docs/audio/UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30.md).

## 🛠️ Build and export

Native reference:

```sh
cc -O3 -std=c99 -Wall -Wextra -Werror \
  unified_runtime.c unified_runtime_cli.c -lm -o unified_runtime_cli
```

Run the exporter from the parent tools directory using the existing PyTorch environment:

```sh
python export_unified_upmix_runtime.py --checkpoint /path/to/last.pt --out /new/assets
```

The CLI accepts `MODEL INPUT_SPECTRA OUTPUT`; the Python verification tool writes its input fixture and checks source waveforms, scene fields, source quality and counted dense MACs. ARM builds enable the NEON int8 dot kernel when `__ARM_NEON__` is defined. Compile success alone does not validate that kernel or head-unit deadlines.

## 📦 File and arithmetic contracts

All fixture/blob numbers are little-endian. The current CLI assumes a little-endian host with IEEE float32. Model header: magic `UMXNN001`, version1, mode0/1, dimensions24/2/16/16/16,48bands, frozen TRAIN log mean/deviation, level EMA alpha and432 named parameter tensors. Each tensor records its name, rank, rows, columns and storage. Float mode stores float32 parameters. Int8 mode stores matrix row scales plus symmetric signed weights in[-127,127]; layer norms and biases remain float32. The loader rejects unsupported dimensions, duplicate names, nonfinite values and trailing/truncated data.

Each matrix input is dynamically quantized per vector to[-127,127], rounding ties to even. Int32 accumulation is exact; row weight and input scales dequantize the result. GRUs use PyTorch reset/update/new order and apply reset after the recurrent affine and bias. Nonlinearities and recurrent state remain float. No fast-math reassociation is permitted in parity builds.

Input fixture magic `UMXSP001`, uint32 stream count/frame count/4100, then float32 `[stream, frame,1025,L/R,real/imag]`. Output magic `UMXOT001`, stream count/frame count/17552, then flattened source masks `[4,1025,2,2]`, presence, membership, persistent pan and update `[48,6]` each. The CLI resets state between streams and reports neural-only timing to stderr; file IO is outside the timing.

## 🔌 Integration boundary

Input must be an actual past-only Hann2048/512 stereo spectrum at48kHz, with real DC/Nyquist and the same PCM scale as training. The shared level, two temporal GRU layers, global GRU and family state carry frame to frame. Current-frame frequency recurrence sees all bands but no future audio.

Apply source masks to the **same mixture spectrum**, then zero imaginary DC/Nyquist before any inverse real FFT. C neural masks themselves can contain nonreal endpoints. The reference causal WOLA has1536samples/32ms array delay and up to512samples/10.667ms acquisition delay; align front and rear paths explicitly. FFT, WOLA, source attribution, bass/LFE semantics,48-to-renderer-band mapping, DSP and scheduler costs are outside this library.

The header is an isolated Track B API, not q6d's32-band ABI. The loaded full chain, int8 quality, arbitrary stream interleaving on ARM, memory ownership, device MAC/time budget and live CarPlay behavior remain deployment gates. Do not insert these weights into the shipping hook by treating the two interfaces as interchangeable.
