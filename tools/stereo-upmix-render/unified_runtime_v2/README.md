# Unified runtime v2 — two explicit graphs

Isolated Track B copy; no hook ABI or device qualification. Original
`../unified_runtime/` and its evidence remain unchanged.

Supported blob dimension tuples `(width, depth, local, global, decoder)`:

- `(24, 2, 16, 16, 16)`: original graph, 432 named tensors.
- `(24, 3, 24, 24, 24)`: function-preserving capacity expansion, 452 tensors.

The existing dimension-tagged `UMXNN001` layout remains version 1. The old
loader rejects expanded dimensions; this loader rejects every other tuple.
State is caller-owned and padded for the larger graph (16,240 bytes). Prediction
and PCM processing allocate no heap memory. Do not mix v1 and v2 state layouts.

Export with `../export_unified_upmix_runtime_v2.py`. Both float reference and
row-wise int8 assets are generated. `umx_predict` computes all training outputs;
`umx_predict_eligible` computes eligible plus all scene fields, zeroing protected
mask fields. Dense cost is 91.5555/132.2505 M MAC/s respectively for the two
selected graphs, excluding transforms, nonlinearities, DSP and scheduling.

`../audit_unified_runtime_v2.py` compares fixed TRAIN spectra to PyTorch, checks
old default C outputs byte-for-byte, and runs four interleaved C states through
all versus selected output paths. The larger graph is also checked after actual
20-step training, exercising nonzero new outgoing links. These numerical gates
do not establish source quality or int8 recovery. Scene is still dormant.

`build_qnx.sh NEW_OUTPUT` compiles standalone neural and PCM binaries without
writing `hook/audio`. The measured compiler stack report is about 52 KB for
prediction alone plus nested calls. Any future worker must explicitly account
for stack, caller-owned states, PCM delay, and loaded chain deadlines.
