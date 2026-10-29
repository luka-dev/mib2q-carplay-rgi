# SCENE stereo-to-surround algorithm

This reference describes the implemented SCENE renderer and fine mask net c. The filename is retained for existing source references; the original proposal and numbered implementation journal are in the [historical design paper](../archive/internal-reference-2026-10-29/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md).

SCENE is active on eligible stereo media in the six-channel path. MMI controls Off, Normal and Intense. It does not process discrete APAC 5.1 as stereo. Installation gates, settings and signal-chain order are defined in [Audio runtime](STEREO_TO_5_1_UPMIX.md); timing and benchmarking are in [Compute budget](UPMIX_COMPUTE_BUDGET.md).

## Engine contract

The engine accepts interleaved stereo float32 at 48 kHz and emits `L R C LFE Ls Rs`. Its own C and LFE outputs are zero; the surrounding upmix backend supplies bass processing. Synthesized stereo has no centre feed, while the separate discrete path preserves the source centre channel.

Analysis uses a sine window, 1024-point FFT and 256-sample hop. Paired real transforms implement stereo analysis and four-channel synthesis with three complex FFTs per hop. Front subtraction and rear signals are reconstructed by overlap-add; the front output is delayed input minus the reconstructed subtraction. The engine has 1024 samples of streaming delay, plus any configured pair delay. The hook's current rear delay is zero.

The C engine allocates no memory after initialization. The port contract uses float32 without fused multiply-add and compares function/stream fixtures against [scene_upmix.py](../../tools/stereo-upmix-render/scene_upmix.py). The header specifies intermediate tolerance `1e-4` relative and output error below −60 dB relative to reference RMS. These are comparison criteria, not a claim that a new fixture run occurred during this documentation review.

## Spatial statistics and stable places

For each frequency bin, the engine tracks left/right power and complex cross-power with exponential averages. The update coefficient is:

```text
alpha(tau) = 1 - exp(-hop / (sample_rate * tau))
stat_next  = stat + alpha * (observation - stat)
```

The implementation keeps separate 43 ms statistics for decomposition, 8 ms statistics for fast protection and 300 ms statistics for direction. Direction uses a bin's own averaged covariance; a single instantaneous stereo sample would make every bin appear rank one. ERB pooling supports stable pan estimation without assigning a neighbouring instrument's direction to a centred drum bin.

[scene_clusters.c](../../hook/audio/scene_clusters.c) maintains a 21-bin pan histogram, admits persistent off-centre places and fades departing places. Current defaults include a 3 s histogram time constant, 0.34 s selection interval, 1.5 s admission and up to three places. Membership and held pan/transfer values keep places from jumping with every FFT frame. A track-change reset fades places over 1.5 s and clears song memory; a stream reset clears the entire streaming state.

## Object extraction and transfer

The active unmix branch separates a place from the centre using a two-source model:

```text
[L; R] = [a_c; b_c] * s + [1; 1] / sqrt(2) * m
 a_c = sqrt((1 - p_c) / 2)
 b_c = sqrt((1 + p_c) / 2)
 det = a_c - b_c
 s   = (L - R) / det
```

Near the centre (`abs(det) <= 0.5`), the inverse becomes `det / (det² + lambda)` with `lambda = 0.05`. The extracted object's energy is capped at the bin's input energy. Exactly centred input (`L == R`) therefore gives zero extracted object for every place.

The object leaves the front at its original pan and enters the rear at the widened place pan. For transfer fraction `a`, the energy-conserving rear gain is:

```text
g_rear = sqrt(1 - (1 - a)^2)
```

Held membership, high-pass shape, protection and the startup/front-anchor gain bound the transfer. The non-unmix projection branch remains available in the standalone engine; it is not the hook's selected spatial mode.

## Semantic protection and instrument transfer

[scene_mask.c](../../hook/audio/scene_mask.c) analyses 2048 samples every 512 input frames. Features are left/right log power and cosine/sine interchannel phase. The network produces vocal, drum-core, bass and other posteriors on a 257-bin grid. Eligibility and protection rows are mapped to the renderer's frequency bins.

Protection attenuates transfer with `D = duck_depth * smoothed_protection`. Instrument eligibility is exponent-shaped and smoothed before applying it to the **remaining front signal**:

```text
O = eligibility * (input - spatial_subtraction)
front_subtraction += a * O
rear              += sqrt(1 - (1 - a)^2) * O
```

The conserving gain uses the transfer fraction, not `a * eligibility`; putting eligibility inside the square root would amplify weak eligibility. Normal/Intense tuning, low-frequency exponent and front retention are applied by `spa_configure_vehicle_scene()` and the live level setter, not inferred from the standalone engine's defaults.

The persistent mask worker supplies rows asynchronously. If no mask is installed, the engine disables semantic transfer and mask-based ducking and continues spatial rendering. A busy worker retains the last row. New-stream generations reject stale jobs/results. The synchronous provider-free path is used by standalone tools/fixtures and is the logged fallback if worker startup fails; it is not an `AUDIO_SCENE_MASK_SYNC` production environment switch.

## Ambience, wide pairs and front retention

The ambience path uses the residual orthogonal to the bin's averaged primary direction:

```text
u = b * L - a * exp(-i * theta) * R
```

Frequency shaping and independent fixed phase tables produce the ambient rear pair. The low-frequency sign rule avoids opposite-polarity rear energy below 550 Hz. The wide-pair path transfers eligible remaining stereo content using an ERB-group envelope measure, while protection limits vocal/bass/drum transfer. Bleed cancellation subtracts a shaped part of the remaining front from the rears.

The front anchor prevents a sparse, wide mix from leaving the front too quiet. It tracks one-second midband sums for input `X`, full-transfer subtraction `S`, and their cross term. The retained front energy is:

```text
E_front(a) = E_X - 2*a*R_XS + a^2*E_S
keep       = 10^(-front_keep_db / 10)
E_front(a) >= keep * E_X
```

The smaller quadratic root bounds the permitted transfer where the energy would cross the retention threshold. The bound is clamped to `[0,1]`, follows reductions over 0.3 s and recovers over 2 s. It scales transfer and ambience together. The hook uses 1.5 dB front retention for Normal and 2.8 dB for Intense.

## Backend and output boundaries

The surrounding [stereo_upmix.c](../../hook/audio/stereo_upmix.c) owns wet/dry control, LFE, cabin protection and conversion to the native output layout. Full Off delays dry L/R, zeros the synthesized extra channels and skips the network. LFE steering uses bass/drum posteriors; it is not part of `scene_engine_frame()`.

The cabin guard precedes SoundCheck in the output path. SoundCheck is owner-confirmed working; that ordering is not an open playback defect. Its positive gain is capped from metadata headroom before rounding. See [SoundCheck](STEREO_TO_5_1_UPMIX.md#-soundcheck-and-output-order) for the precise metadata contract.

## Implementation and evidence

| Source | Responsibility |
| --- | --- |
| [scene_engine.h](../../hook/audio/scene_engine.h) / [scene_engine.c](../../hook/audio/scene_engine.c) | State, defaults, per-bin processing, FFT/overlap-add and stream API |
| [scene_clusters.c](../../hook/audio/scene_clusters.c) | Place admission, membership, fading and memory |
| [scene_mask.h](../../hook/audio/scene_mask.h) / [scene_mask.c](../../hook/audio/scene_mask.c) | Blob format, features and inference |
| [scene_mask_worker.c](../../hook/audio/scene_mask_worker.c) | Bounded async jobs/results and generations |
| [audio_hook.c](../../hook/audio/audio_hook.c) | Production configuration, worker lifecycle and AirPlay latency |
| [scene_port_fixtures.py](../../tools/stereo-upmix-render/scene_port_fixtures.py) | Python reference fixtures |
| [scene_mask_fixtures.py](../../tools/stereo-upmix-render/scene_mask_fixtures.py) | Mask reference fixtures |

The historical paper retains alternative derivations, listening comparisons and dated port measurements. Its stage numbers cited in source comments are historical references, not pending implementation tasks.
