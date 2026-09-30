# Standalone causal unified upmix chain

This Track B executable connects a single `unified_runtime_v2` model to its past-only audio frontend and the copied NN-free C renderer. It does not install a hook or change Track A. Scene is computed but dormant; it does not control rendering.

## Build

From the repository root:

```sh
python3 tools/stereo-upmix-render/build_unified_chain.py --out /tmp/new-unified-chain
bash tools/stereo-upmix-render/unified_chain/build_qnx.sh /tmp/new-unified-chain/copies /tmp/new-unified-chain-qnx
```

Use a new output directory for each build. The host builder records source/compiler/command identities and preserves compiler failures. A MinGW compiler can build a static Windows executable on the Mac. The QNX script requires the installed toolchain Docker image and reads reviewed renderer copies without changing their sources.

## Native interface

```text
upmix_unified_chain MODEL|identity INPUT_PCM16 OUTPUT6 SOURCE2 [SOURCE_FLOAT]
```

Input: little-endian `UMX16I01`, then three uint32 fields: streams, samples per stream, channels=2; stream-major interleaved PCM16. Each stream has independent zero-start state. Outputs use the same header layout: `UMX16O01` with six PCM16 channels, `UMX16S01` with stereo PCM16 source, optionally `UMXFLS01` with stereo float32 source. Flush rounds up to full512-sample hops. Input/eligible overflow fails without gain or clipping rescue; partial artifacts remain.

The front mixture FIFO delays1536samples to align it with eligible WOLA output. Rendering uses fixed nominal surround.40, front bass−3dB, front retention.5, rear air−9dB, body0, bed delays enabled, attack disabled and default LFE assistance. Legacy model loads/inference abort. One256KiB-stack worker owns processing state. Timing includes FFT/model/WOLA/C rendering; file IO is excluded.

## Verify

```sh
python3 tools/stereo-upmix-render/verify_unified_chain.py \
  --fixture ORIGINAL_FOUR_TRAIN8S_FLOAT_INPUT \
  --checkpoint MATCHING_CHECKPOINT \
  --assets EXPORTED_V2_ASSETS \
  --executable HOST_CHAIN_EXECUTABLE \
  --renderer INDEPENDENT_NN_FREE_C_EXECUTABLE \
  --out NEW_AUDIT_DIRECTORY
```

Verification uses CPU only. It checks unrounded source math separately from PCM16 rounding, every output channel against independent saved-source C rendering, repeat/reset, future-prefix and identity pulse delays. Model/fixture/executable hashes are recorded. The actual trained expanded20-step fixture passes; this is numerical integration evidence, not terminal acoustic quality, device timing, loaded hook latency or owner listening. The original all-source int8 quality failure remains open.

See [full evidence and failed iterations](../../../docs/audio/UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30.md#full-causal-pcm16-chain-one-model-and-nn-free-real-c).
