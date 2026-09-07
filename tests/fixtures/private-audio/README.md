# Private HSSR music corpus

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
python3 tools/stereo-upmix-render/prepare_local_stereo_stress_corpus.py \
  --output build/reference-audio/private-local-stereo-heldout-corpus \
  tests/fixtures/private-audio/heldout/*.mp3
```
