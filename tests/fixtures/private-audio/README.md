# Private upmix music corpus

These commercial stereo masters are private-repository test assets. Do not
publish or redistribute them outside the private repository.

- `training/`: 43 tracks used by the V11 offline separator-supervision work.
- `heldout/`: 7 tracks reserved before training and used only for blind
  generalization evaluation.

The repository stores only the original MP3 sources. Deterministic 30-second,
48 kHz stereo excerpts, teacher stems, render WAVs and feature caches are
generated under ignored `build/` paths and must not be committed.

Example corpus preparation:

```sh
# prepare_local_stereo_stress_corpus.py was retired with the Universal renderer on 2026-10-09
# (git tag upmix-universal-last-2026-10-09); the scene tools take the files directly:
python3 tools/stereo-upmix-render/scene_car_kit.py OUT_DIR name=tests/fixtures/private-audio/heldout/<song>.wav   # 48 kHz stereo WAV in, blinded 6-ch kit out
```
