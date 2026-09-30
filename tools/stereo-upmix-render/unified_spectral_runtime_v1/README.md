# Standalone spectral-context runtime v1

Track B C runtime for the fixed `unified-spectral-context-prototype-v1`
graph: width 48, depth 2, local/global/decoder 24, four 12-wide causal GRUs
per band and block, seven-band depthwise convolution, and eight current-frame
48-by-48 frequency mixers. It accepts only this exact graph and its 480 tensors.
The `UMXSPN01` blob magic prevents old graph loaders from accepting it.

No hook integration or head-unit deployment is performed here. Scene outputs
are replayed structurally; these checks do not establish trained scene semantics.

## Export and native check

From the repository root, export a saved spectral fitting/broad checkpoint:

```sh
python3 tools/stereo-upmix-render/export_spectral_context_runtime.py \
  --checkpoint /path/to/spectral_checkpoint.pt --out /new/assets
cc -O3 -std=c99 -Wall -Wextra -Werror \
  tools/stereo-upmix-render/unified_spectral_runtime_v1/unified_runtime.c \
  tools/stereo-upmix-render/unified_spectral_runtime_v1/unified_runtime_cli.c \
  -lm -o /new/runtime
cc -O3 -std=c99 -Wall -Wextra -Werror \
  tools/stereo-upmix-render/unified_spectral_runtime_v1/unified_runtime.c \
  tools/stereo-upmix-render/unified_spectral_runtime_v1/unified_runtime_prune_check.c \
  -lm -o /new/prune_check
python3 tools/stereo-upmix-render/audit_spectral_context_runtime.py \
  --checkpoint /path/to/spectral_checkpoint.pt --assets /new/assets \
  --fixture /path/to/train64.spectra --fixture-manifest /path/to/fixture_manifest.json \
  --executable /new/runtime --checker /new/prune_check --out /new/audit
```

The fixture must be the first 64 saved GPU input STFT frames of the four
already inspected TRAIN fitting crops, with its checkpoint/hash manifest.
The audit checks float reconstruction against PyTorch, all-output versus
eligible-only state/scene equivalence, four independent interleaved streams,
repeat/reset and changed-future prefix. It also prepares a 16-frame ARM fixture.
It does not load DEV or reserved audio. Outputs are preserved on failure.

Full four-crop waveform/quantization diagnostics can run on CPU using
`verify_spectral_context_runtime_train.py`; its paths correspond to the
original frozen TRAIN manifest and cache. Int8 promotion there requires
eligible median SDR loss no more than 0.3 dB, no crop loss over 1 dB, and each
present protected reconstruction median loss no more than 0.3 dB. Component
energy is diagnostic, not a literal audible leakage criterion. These narrow
TRAIN checks cannot promote an acoustic candidate.

## Runtime contract and compute

- One caller-owned state per stream; no prediction-time allocation.
- `umx_predict` evaluates all four training source heads and scene fields.
- `umx_predict_eligible` skips protected final output rows while preserving
  eligible output, scene and complete state bit exactly; protected output
  fields are zero. Training heads remain in the blob.
- Float uses the same GRU reset-after-hidden-affine order as PyTorch.
- Int8 uses per-output-row symmetric weights, per-input-vector dynamic
  activation quantization, ties to even, signed range -127 through 127,
  exact int32 sums, ARMv7 NEON without dot-product extensions.
- Depthwise weights `[48,1,7]` are serialized as `[48,7]`; each channel
  consumes its own zero-padded seven-band vector with its own activation
  scale. The exporter retains original tensor shapes in the manifest.
- Dense counts: all sources 1,540,128 MAC/frame (144.387 M/s); selected
  1,244,928 MAC/frame (116.712 M/s), at 93.75 frames/s. Counts include padded
  depthwise multiplies. They exclude FFT, nonlinearities, norms, DSP,
  scheduling and memory traffic, and are not measured head-unit performance.

Float/int8 blobs for the completed 20k fitting checkpoint are arithmetic
qualification assets, not a broad or deployable winner. Changing the graph
or quantization requires a fresh export and replay.

## QNX preparation

`build_qnx.sh NEW_DIRECTORY` creates ARMv7/QNX neural and audio-stream binaries
using the existing isolated Docker toolchain. `qnx_qemu_check.sh ASSETS
NEW_DIRECTORY` executes the neural graph in a local QNX guest and compares
against the native float/int8 reference fixture. The assets directory must
contain both blobs, `arm_fixture.bin`, and a manifest including its provenance.
These operations do not contact or restart the Audi.

Full 5.1 integration, quantized broad source/real-renderer gates, actual loaded
Krait timing/latency and live CarPlay listening remain required independently.
