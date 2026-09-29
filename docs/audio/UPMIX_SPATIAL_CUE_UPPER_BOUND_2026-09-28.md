# Stereo Spatial-Cue Upper Bound

_2026-09-28. Frozen Track B mechanism test before measuring results. Privileged true-stem fit; no candidate or car claim._

## Question and controlled comparison

The causal shared, role, complex, dual-head and two-tap outputs have all missed the fixed source/entry gates. The latest [long-context audit](UPMIX_CONTINUOUS_CONTEXT_AUDIT_2026-09-28.md) found no large 8 s reset mismatch. All trained complex outputs so far are **diagonal** in stereo: each rear-eligible output channel multiplies only the corresponding input channel. A 2×2 complex L/R matrix could, in principle, suppress a centered vocal while retaining an off-center instrument. Test whether the owner's stereo material has enough stable geometry to make that architectural change worth training.

For each of the **first 12 fixed, already-inspected MoisesDB DEV songs**, fit two frequency-dependent linear least-squares operators on the true eligible stem and full stereo mixture from **0–29.9 s**: (a) a diagonal 2×2 complex matrix, (b) a full 2×2 complex matrix with L/R cross terms. Both use identical 1%-of-bin-power ridge regularization and the same target. Freeze the operators and evaluate only **30.1–60 s**, leaving a 0.2 s gap around the split. Apply each frozen mixture-fitted operator separately to aligned true eligible, vocal, drums and bass stems; report eligible energy transmission, per-song protected/eligible ratios and normalized complex eligible error. Also report the full matrix after one per-song positive gain that matches its eligible transmission to the diagonal baseline; that gain is a comparison tool, not a deployable calibration. Song-specific true stems in the first half make both rows privileged upper bounds, even though second-half audio is separate.

The matrix fitter was smoke-tested on a synthetic stereo mixture with panned eligible audio and centered vocal: full-matrix held-out error **−25.94 dB** versus diagonal **−2.92 dB**, and vocal/eligible transmission **0** versus **.523**. This confirms that the implementation can detect a spatial advantage when one exists; it says nothing about real music.

## Frozen decision rule

Consider a causal cross-channel model only if the held-out second-half full matrix achieves **all** of: median songwise vocal/eligible ratio ≤ **80%** of the diagonal baseline; median normalized complex-error improvement ≥ **.3 dB** and improvement in ≥ **8/12 songs**; median raw eligible transmission no more than **.03** below diagonal; median drum/eligible and bass/eligible each no more than **+.01** above diagonal. Require at least **8 songs with valid energy** for each protected-family ratio; a silent second-half stem is excluded from that family's ratio. Report individual songs and matched-eligible error regardless. If this oracle bound fails, do not spend another 4k GPU run on a cross-channel head without a different cue; if it passes, precommit and train a budgeted causal student, then apply the existing source/entry/C gates. These are architecture-selection criteria, not the product's full40, independent-set, device or listening acceptance gates.

## Results

The first execution exposed a missing-value bug after five songs: the sixth song's second half has too little vocal energy for a valid vocal/eligible ratio, and the summary formatter assumed every song had one. The five rows printed before the exception are exploratory and **not a 12-song verdict**. The code now records `null` for a silent protected family, saves partial rows, and requires the protected-family coverage above. Performance thresholds were not changed. The full 12-song run is pending.
