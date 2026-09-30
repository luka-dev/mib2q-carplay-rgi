# Standalone causal spectral-context 5.1 chain

This Track B executable connects one `unified_spectral_runtime_v1` checkpoint
to the past-only FFT/WOLA frontend and copied NN-free C renderer. It has no hook
ABI and does not deploy to the head unit. Scene fields are computed but dormant;
no claim about trained instrument membership or pan follows from this replay.

## Build

From the repository root:

```sh
python3 tools/stereo-upmix-render/build_spectral_context_chain.py \
  --out /tmp/new-spectral-chain
bash tools/stereo-upmix-render/unified_spectral_chain_v1/build_qnx.sh \
  /tmp/new-spectral-chain/copies /tmp/new-spectral-chain-qnx
```

Use fresh output directories. Source/compiler/commands and copied renderer
identities are retained. Compiler failures remain visible. The QNX script uses
the existing isolated Docker toolchain and reads the reviewed source copies.
The original runtime, active Python training/helpers and Track A files remain
unchanged. A static Windows executable can be built with MinGW using
`--compiler x86_64-w64-mingw32-gcc`.

## Interface

```text
upmix_spectral_chain MODEL|identity INPUT_PCM16 OUTPUT6 SOURCE2 [SOURCE_FLOAT]
```

Input uses little-endian `UMX16I01` plus three uint32 fields: stream count,
samples per stream and channels=2, followed by stream-major interleaved PCM16.
Outputs use `UMX16O01` (six PCM16 channels), `UMX16S01` (stereo PCM16 eligible
source), and optionally `UMXFLS01` (unrounded float32 source). Every stream
starts from its own zero state. Flush emits whole512-sample hops.

The front mixture FIFO delays1536samples to align with the source estimator.
Renderer settings stay fixed at nominal surround.40, front bass−3dB, front
retention.5, rear air−9dB, body0, bed delays enabled, attackoff and default LFE
assistance. Source overflow fails; no clipping or gain rescue is selected.
Old q6d/guard/mask loads and inference abort. One256KiB-stack worker owns
processing state. Timing includes FFT/model/WOLA/C rendering, excluding IO.

## Verify

```sh
python3 tools/stereo-upmix-render/verify_spectral_context_chain.py \
  --fixture ORIGINAL_FOUR_TRAIN8S_FLOAT_INPUT \
  --checkpoint MATCHING_SPECTRAL_CHECKPOINT \
  --assets EXPORTED_SPECTRAL_ASSETS \
  --executable HOST_CHAIN_EXECUTABLE \
  --renderer INDEPENDENT_NN_FREE_C_EXECUTABLE --out NEW_AUDIT_DIRECTORY
```

The CPU audit binds the exact already inspected four TRAIN8s mixture identity.
It separately checks unrounded source arithmetic and PCM16 rounding against
PyTorch, all six channels against independent saved-source C rendering,
repeat/reset, future-prefix causality for both float/int8, exact selected dense
count, absence of old models and identity-pulse delays. Dormant scene is not
semantic routing qualification. The full TRAIN int8 reconstruction screen is
separate from this arithmetic integration check.

A passing result establishes this saved fitting checkpoint's native chain
arithmetic and state handling. Original full40/independent20 acoustic gates,
terminal int8 requalification, physical loaded Krait timing/latency and owner
blind CarPlay preference remain necessary. Native host/QEMU timing or a single
pulse cannot prove the actual ≤150M/≤85ms requirements.

## Full ARM/QNX arithmetic replay

```sh
python3 tools/stereo-upmix-render/make_spectral_chain_arm_fixture.py \
  --audit PASSED_NATIVE_CHAIN_AUDIT --assets EXPORTED_SPECTRAL_ASSETS \
  --out NEW_ARM_ASSETS
bash tools/stereo-upmix-render/unified_spectral_chain_v1/qnx_qemu_check.sh \
  COPIED_RENDERER_DIR NEW_ARM_ASSETS NEW_GUEST_OUTPUT
```

The guest executes four independent streams interleaved for the first64 hops
of each inspected TRAIN8s input. Float all-channel/source PCM16 parity requires
NSE≤1e−7 and maximum4LSB; int8 versus the corresponding native int8 reference
requires NSE≤1e−4 and reports peak differences separately. These platform
bounds are arithmetic checks, distinct from ground-truth quantization quality.
The guest is labelled `qnx-arm`; host checker preparation is labelled
`native-host`. The local guest never contacts or restarts the head unit.

In the current configuration center is zero. Independent-reference equality
for that field is a zero-output check; it does not validate an active center
scene. Front stereo, LFE, rear stereo and eligible stereo are exercised. The
report preserves channel coverage and failures alongside passing checks.
