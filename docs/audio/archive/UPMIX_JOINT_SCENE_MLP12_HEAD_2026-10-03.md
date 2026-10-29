# Joint Scene: Nonlinear Operator Head Comparison

*2026-10-03 America/Chicago · Track B · Owner selected architecture alternative 1; launched October4 UTC.*

## 🧭 Hypothesis and fixed control

The [linear-head control](UPMIX_JOINT_SCENE_8192_CONTINUATION_2026-10-03.md) ended at rear NMSE .318857/.348871 after 8192 fixed-crop updates. The owner selected a targeted change: replace its affine 20→20 operator layer with **20→12→20 and SiLU**. This tests extra output nonlinearity while preserving the encoder, causal state, coefficient bounds and dry projection.

This is not evidence that every deeper decoder improves scene recovery. A hidden width 12 also changes the intermediate representation; the test measures this specific recipe. The same-crop fitting gate remains both rear NMSE ≤.10 and dry PCM relative null RMS≤1e-5. Report all front/centre/rear metrics and compute cost; a rear improvement with front regression is a tradeoff, not a joint win.

```mermaid
flowchart LR
  accTitle: Isolated nonlinear operator head
  accDescr: The unchanged shared encoder feeds a twelve-unit hidden layer and SiLU before twenty real operator outputs. The original complex operator conversion and joint downmix projection still produce five channels for the same fixed TRAIN target.
  fixed_stereo["Same fixed TRAIN stereo"] --> shared_encoder["Unchanged causal encoder"]
  shared_encoder --> twenty_features["20 local and context features"]
  twenty_features --> hidden_twelve["Affine 20 to 12"]
  hidden_twelve --> silu_activation["SiLU"]
  silu_activation --> twenty_outputs["Affine 12 to 20"]
  twenty_outputs --> original_projection["Original bounded complex operators<br/>and dry projection"]
  original_projection --> five_outputs["FL, FR, C, Ls, Rs"]
```

## 🗂️ Protocol and initialization

| Item | Frozen setting |
|---|---|
| Run | `runtime/scene-joint-head-mlp12-fixed-8192-20261003-c` under the NAS private training root |
| Launch | 2026-10-04T01:16:04.726292+00:00; PID 417087 at launch |
| CPU | Three threads; no other upmix Python tenant at launch |
| Architecture | `joint-scene-band-global-mlp12-v1` |
| Initialization | Fresh seed 20261003; no trained checkpoint |
| Shared body | Bit-identical initial state to the fresh same-seed linear anchor |
| Hidden layer |20→12 affine, default PyTorch initialization, SiLU |
| Final layer |12→20 affine, weight std .005, bias 0; same final small-weight convention as baseline |
| Original operator constraints | tanh coefficient bound 2.0, real DC/Nyquist endpoints, joint weighted downmix projection |
| Crop | TRAIN albumdb-01, start sample1,354,304, length33,536 (.698667 s) |
| Frames |128 total,32 warm,96 scored; recurrent state reset each update |
| Data/normalization | Same hash-verified authored target, common gain and original TRAIN power normalization |
| Loss | Unchanged parent complex MSE plus .05 magnitude MAE |
| Optimizer | AdamW lr .001, decay .0001, gradient clip5 |
| Horizon |8192 total fresh updates; matched comparisons at 2048 and8192 |
| Checkpoint retention |128-step monitors, best weights, paired 2048 model+optimizer, terminal 8192 model+optimizer |
| Partitions | No DEV/heldout audio decoded |
| Physical scope | Five learned channels; no LFE/effects, hook port, int8 or deployment |

The subclass constructs the complete baseline before replacing its head, preserving the seeded shared body. Added head initialization consumes RNG afterward. Outputs at step0 are consequently not identical; their metrics are retained. There are no stochastic operations or augmentations after initialization, and the selected samples never change. The shared encoder/loss source hashes match the previous runs; the new head and runner source hashes are recorded separately.

## 🧮 Cost and checks

| Model | Total parameters | Head parameters | Estimated dense neural MAC/s |
|---|---:|---:|---:|
| Linear anchor |68,044 |420 |103,418,625 |
| MLP12 head |68,136 |512 |111,113,625 |

The increase is92 parameters and7.695 M dense MAC/s. The estimate excludes FFT/WOLA, feature construction, projection, complex application, normalization, nonlinearities including the added SiLU, and LFE DSP. Neither architecture has a complete device measurement; being below150 M for the dense neural part does not establish the head-unit budget.

The MLP contract test passed locally and on NAS. It checks identical seeded body state, parameter/MAC counts, invariance to future-frame changes, chunk/state equivalence, wet-effect subtraction/downmix preservation, and front/rear gradients reaching the hidden layer and all five output groups. The existing optimizer-resume trajectory/rejection test also passed locally and on NAS after the runner change. No packages were installed or Track A files modified.

## 📊 Matched results

<!-- MLP12_RESULT_START -->
**Completed 8192/8192 updates in 1522.4 s.** Terminal fitting failed; best monitored fitting failed (best step8192).

Lower NMSE means lower target reconstruction error. It is not rear share or protected-source leakage. Negative delta means the candidate improved.

| Updates | Channel | Linear NMSE | MLP12 NMSE | MLP12 minus linear |
|---|---|---:|---:|---:|
| 2048 | FL | 0.054893 | 0.056632 | +0.001739 |
| 2048 | FR | 0.061866 | 0.064152 | +0.002286 |
| 2048 | C | 0.014306 | 0.014230 | -0.000076 |
| 2048 | Ls | 0.448540 | 0.468725 | +0.020184 |
| 2048 | Rs | 0.469496 | 0.516417 | +0.046921 |
| 8192 | FL | 0.039713 | 0.031377 | -0.008335 |
| 8192 | FR | 0.044830 | 0.035131 | -0.009700 |
| 8192 | C | 0.010183 | 0.008274 | -0.001909 |
| 8192 | Ls | 0.318857 | 0.284776 | -0.034081 |
| 8192 | Rs | 0.348871 | 0.314774 | -0.034097 |

At 8192, rear error reduction relative to the matched control is 10.69% for Ls and 9.77% for Rs. Both rear errors must still be ≤.10 to pass the fixed-crop gate.

| Terminal metric | Linear @8192 | MLP12 @8192 |
|---|---:|---:|
| Ls energy / target | 0.682199 | 0.726945 |
| Rs energy / target | 0.637116 | 0.703845 |
| Ls target cosine | 0.825314 | 0.845737 |
| Rs target cosine | 0.806973 | 0.827858 |
| Loss | 0.022677 | 0.0195743 |
| Dry PCM relative null RMS | 7.15701e-07 | 5.45734e-07 |

Checkpoint hashes and protocol bindings verified; optimizer counters are [8192] at terminal and [2048] at the paired checkpoint. All 8192 recorded gradients are finite; 0 exceeded the clipping threshold. Raw optimizer history remains on NAS.

A target-informed independent scalar-gain diagnostic gives rear gains 0.991938/0.986773 and minimum NMSE 0.284729/0.314651. This tests only uniform amplitude mismatch; it is not a deployable correction, does not preserve the joint projection if applied independently, and cannot establish target reachability for a different operator. No extra inference/training was run.

### Complete matched monitor curve

| Updates | Linear Ls | MLP12 Ls | Linear Rs | MLP12 Rs |
|---|---:|---:|---:|---:|
| 128 | 0.846115 | 0.891279 | 0.849977 | 0.884246 |
| 256 | 0.797418 | 0.865163 | 0.788613 | 0.847210 |
| 384 | 0.740063 | 0.833318 | 0.744591 | 0.808537 |
| 512 | 0.672033 | 0.741309 | 0.680524 | 0.751949 |
| 640 | 0.635124 | 0.680538 | 0.630798 | 0.707950 |
| 768 | 0.603226 | 0.645107 | 0.584328 | 0.676755 |
| 896 | 0.581734 | 0.620889 | 0.560209 | 0.658341 |
| 1024 | 0.553072 | 0.579892 | 0.539669 | 0.623282 |
| 1152 | 0.531118 | 0.555254 | 0.524400 | 0.601574 |
| 1280 | 0.513549 | 0.535501 | 0.512321 | 0.583487 |
| 1408 | 0.498445 | 0.516818 | 0.499838 | 0.565529 |
| 1536 | 0.484721 | 0.503659 | 0.491410 | 0.555265 |
| 1664 | 0.474201 | 0.493932 | 0.484641 | 0.544079 |
| 1792 | 0.464951 | 0.486070 | 0.479333 | 0.536170 |
| 1920 | 0.457104 | 0.477373 | 0.474238 | 0.525191 |
| 2048 | 0.448540 | 0.468725 | 0.469496 | 0.516417 |
| 2176 | 0.441331 | 0.458821 | 0.465076 | 0.506477 |
| 2304 | 0.434271 | 0.447782 | 0.460880 | 0.494645 |
| 2432 | 0.427855 | 0.444693 | 0.457084 | 0.492682 |
| 2560 | 0.423428 | 0.428517 | 0.456119 | 0.474691 |
| 2688 | 0.415804 | 0.420591 | 0.449122 | 0.467724 |
| 2816 | 0.411495 | 0.413523 | 0.447584 | 0.459721 |
| 2944 | 0.413651 | 0.405096 | 0.451188 | 0.451429 |
| 3072 | 0.405542 | 0.398241 | 0.444500 | 0.445187 |
| 3200 | 0.400364 | 0.392241 | 0.440734 | 0.439022 |
| 3328 | 0.395652 | 0.385921 | 0.437484 | 0.432189 |
| 3456 | 0.391624 | 0.380663 | 0.434606 | 0.425902 |
| 3584 | 0.390570 | 0.375581 | 0.435106 | 0.419867 |
| 3712 | 0.385779 | 0.370694 | 0.431413 | 0.414490 |
| 3840 | 0.382136 | 0.368646 | 0.428196 | 0.413436 |
| 3968 | 0.376289 | 0.362994 | 0.421888 | 0.407464 |
| 4096 | 0.373661 | 0.359372 | 0.420225 | 0.402050 |
| 4224 | 0.370156 | 0.353434 | 0.416168 | 0.396106 |
| 4352 | 0.366752 | 0.351480 | 0.412995 | 0.394278 |
| 4480 | 0.366748 | 0.346795 | 0.413737 | 0.389247 |
| 4608 | 0.361153 | 0.342182 | 0.407033 | 0.383405 |
| 4736 | 0.358489 | 0.339118 | 0.404152 | 0.379773 |
| 4864 | 0.355250 | 0.339507 | 0.400247 | 0.384710 |
| 4992 | 0.352932 | 0.332630 | 0.397385 | 0.372896 |
| 5120 | 0.354894 | 0.329667 | 0.401781 | 0.369768 |
| 5248 | 0.348909 | 0.326930 | 0.392125 | 0.367147 |
| 5376 | 0.346323 | 0.325598 | 0.388681 | 0.364477 |
| 5504 | 0.427731 | 0.320963 | 0.453379 | 0.360920 |
| 5632 | 0.343191 | 0.319393 | 0.384664 | 0.359687 |
| 5760 | 0.341422 | 0.315759 | 0.382211 | 0.355508 |
| 5888 | 0.340028 | 0.313806 | 0.380233 | 0.353205 |
| 6016 | 0.341192 | 0.311783 | 0.380877 | 0.351391 |
| 6144 | 0.337164 | 0.309324 | 0.375981 | 0.348491 |
| 6272 | 0.335741 | 0.307492 | 0.374240 | 0.345877 |
| 6400 | 0.334220 | 0.305263 | 0.372008 | 0.343250 |
| 6528 | 0.332611 | 0.303931 | 0.370000 | 0.341917 |
| 6656 | 0.331484 | 0.304156 | 0.368245 | 0.341486 |
| 6784 | 0.330064 | 0.299887 | 0.366096 | 0.336585 |
| 6912 | 0.329325 | 0.301200 | 0.364373 | 0.337850 |
| 7040 | 0.328687 | 0.301289 | 0.363849 | 0.339396 |
| 7168 | 0.327956 | 0.298149 | 0.361967 | 0.333308 |
| 7296 | 0.326230 | 0.295546 | 0.359033 | 0.331605 |
| 7424 | 0.324595 | 0.298938 | 0.357678 | 0.336875 |
| 7552 | 0.323293 | 0.292104 | 0.355743 | 0.325678 |
| 7680 | 0.322330 | 0.295600 | 0.354167 | 0.331198 |
| 7808 | 0.323192 | 0.288908 | 0.354268 | 0.320294 |
| 7936 | 0.320646 | 0.287332 | 0.351353 | 0.317852 |
| 8064 | 0.319591 | 0.286041 | 0.349932 | 0.316186 |
| 8192 | 0.318857 | 0.284776 | 0.348871 | 0.314774 |

[Machine-readable evidence](UPMIX_JOINT_SCENE_MLP12_HEAD_2026-10-03.json) retains hashes, protocol, initial/best/terminal metrics, complete monitor histories and matched comparisons.
<!-- MLP12_RESULT_END -->

The terminal verdict must use matched update counts and per-channel errors. The candidate starts fresh; its 8192 updates are compared with the control's 2048+6144 restored trajectory, whose replay was verified. Changes in output initialization are part of the selected head recipe. A single seed and one short TRAIN crop cannot demonstrate robust architecture superiority or generalization.

## 📁 Implementation and evidence

- [scene_joint_head_net.py](RETIRED_TOOLS.md#tool-0b3365f5d354): isolated subclass replacing only the output head and updating its specification.
- [fit_scene_fixed_nas.py](RETIRED_TOOLS.md#tool-68f8855859f8): explicit `--head mlp12`; unchanged crop/loss/optimizer; additional paired 2048 checkpoint.
- [test_scene_joint_head.py](RETIRED_TOOLS.md#tool-49dabc5a3cb5): initialization, signal/state and training-path contracts.

The immutable NAS run has source snapshots, launch/test/train logs, frozen protocol, all monitors and checkpoints. An owned `run/STOP` marker requests a stop with retained outputs. No interband, gated-context, task-specific decoder or additional dataset experiment is queued.

## 🔬 Interpretation boundary

The selected MLP12 recipe improves every channel at the matched 8192 horizon, whereas at 2048 it was worse in both rears. This is a positive fitting result for one seed/crop, not a complete solution. It trades 7.44% more estimated dense neural work for 10.69%/9.77% lower rear error. The last monitored checkpoint is best and the curve is still improving; neither run proves an asymptotic error floor.

The original single-crop ≤.10 gate still fails. A proposed next diagnostic is a target-informed upper bound using the exact bounded complex operators and joint projection, without a neural encoder: can this output representation fit the crop at all? This separates output constraints from learning/encoding limitations before another architecture trial. A second seed and another preselected TRAIN crop would then test whether the MLP gain reproduces before broader training. These are recommendations, not queued experiments. Simple uniform rear amplification cannot close the current error gap.

The altered head initialization is part of this recipe, so this single-seed experiment cannot attribute the gain exclusively to SiLU. An authored diffuse target may need different supervision than coherent relocated music; this trial does not settle that question.

Original full40/untouched20 source protection, rear-share, entry/stability, independently mastered stereo transfer, complete causal int8 device cost/latency and owner CarPlay preference remain untested by this short-crop experiment.
