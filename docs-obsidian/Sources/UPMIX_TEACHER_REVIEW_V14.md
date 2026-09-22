# V14: full-song coverage and teacher listening review

Local-only review, 2026-09-09. No vehicle writes, checkpoint promotion or new training.

## Coverage and running work

`build/reference-audio/v14-full-source/manifest.json` covers all 50 local songs:
587 non-overlapping cores, 194.6726 unique minutes. Inputs include up to two seconds
of context on either side. Short tails are retained, not discarded. Core coverage
and source-level partition disjointness are tested.

| Partition | Clips | Unique minutes |
|---|---:|---:|
| Train | 411 | 137.0067 |
| Validation | 100 | 32.8340 |
| Heldout | 76 | 24.8319 |

43 development songs and seven heldout songs remain separate. Every new item is
`training_eligible: false`. The dataset is prepared; its semantic validation is NOT complete.
Native RoFormer processing of all 587 inputs is running in `v14-full-teacher`.
Completion record: `roformer-manifest.json`; individual provenance files appear progressively.

`queue_full_song_specialist_review.py` waits for that completion, then schedules
one non-silent middle core from each song (50 total), including heldout for evaluation
only. Status/log: `build/reference-audio/v14-specialist-queue.{json,log}`.
It does NOT validate all 587 specialized results. The bounded first pass is necessary
because approximately 75 GiB was free at launch; 15 float stems for the entire corpus,
plus base stems and previews, would exceed that. The queue reserves 15 GiB and refuses
existing targets. Nothing is deleted to make room. A stalled base job times out after 12 h.

## What is actually separated

- Original stereo → Demucs6: bass, drums, vocals, guitar, piano, other.
- Demucs drums → DrumSep: kick, snare, toms, hh, ride, crash; residual preserved.
- RoFormer all-vocals → karaoke model: estimated lead; backing = all-vocals − lead.

These are teacher ESTIMATES. A vocal complement can contain lead vocal or artifacts.
Zero lead+backing reconstruction error is tautological, not semantic evidence.
Demucs/DrumSep residuals and independent vocal-teacher disagreement prioritize review,
not approval. Sources: [upstream pretrained model catalog](https://github.com/ZFTurbo/Music-Source-Separation-Training/blob/main/docs/pretrained_models.md),
[DrumSep release](https://github.com/jarredou/models/releases/tag/aufr33-jarredou_MDX23C_DrumSep_model_v0.1),
[karaoke model](https://huggingface.co/jarredou/aufr33-viperx-karaoke-melroformer-model).

All inference is native 44.1 kHz with a common input gain, restored to aligned 48 kHz
FLOAT output. Models/configs, source and every output have SHA-256 provenance.
No independent stem normalization. Intermediate parents must not be summed with their
children as separate targets (e.g. drums + hh would double-count).

Six initial V13 clips have complete specialist sets: Archers, Bellingham Slowed,
Block Out The World, Cities In Dust, Us V Them, Paradise City (all p2).
Cities In Dust has lead RMS ~0.00002, backing RMS ~0.085. This ratio alone does
not show that a lead vocal disappeared: it cannot establish that a lead was present.
Luka's listening feedback for Cities In Dust [103.0s] (2026-09-09): electronic
music with a sustained “Вуаааааааа” vocal sample, which ended up in the backing
estimate. Record this as a reported vocal sample/texture, not confirmed backing
singing or a confirmed separation error. No clean-stem approval was inferred.

Training/routing implication: source identity (vocal sample) and musical role
(foreground hook vs background texture) are distinct. The numerical complement
`backing` is neither a verified role label nor an instruction to send that sound
rearward. A foreground sampled hook should remain front-anchored in the proposed
routing; a genuinely background texture may be eligible for restrained rear ambience
after review. This distinction is a target-design requirement, not an implemented
or validated foreground/background classifier.

## Listening room

Open http://localhost:5178/ . API binds only 127.0.0.1:8766.
No media is uploaded or publicly hosted. Built using the Sites scaffold for a local
working interface, not a deployed Site.

1. Choose a clip (risk flags sort first), then HH or backing.
2. Play; compare Original / Solo / Original minus stem at the same position.
3. Mark Clean / Unsure / Error / Source absent and add a timestamped note.
4. Save locally. Refresh the catalog to see newly processed files; select a refreshed
   clip to load its current revision.

Preview files use one shared gain with subtraction headroom; quiet estimates remain
quiet. Only requested previews are generated; browser buffer cache is bounded.
JSONL notes: `build/reference-audio/teacher-review/annotations.jsonl`.
Notes bind to revision, source hash and stem hash. Changed files are rejected, including
cached previews. The review API requires loopback Host and allowlisted UI Origin for writes.
Fake test reviews are confined to temporary fixtures, not the real review journal.

`reviewed_candidates` requires exact-revision good human reviews for every requested
stem, excludes heldout and returns candidates only, with `training_approved: false`.
It is not a training manifest. No trainer has been launched on this V14 material.
A few positive examples do not approve an entire song or corpus. Full specialist
coverage/storage strategy and target admission still need a separate decision after listening.

### Restart locally (two terminals from repository root)

```sh
build/hssr-teacher-env/bin/python tools/stereo-upmix-render/teacher_review_server.py
```

```sh
cd tools/teacher-review-ui
npm run dev -- --host 127.0.0.1 --port 5178
```

### Verification and limits

Seven Python tests cover full coverage, partition isolation, review gating, shared
gain, stale audio/review rejection and local persistence. Three player tests cover
synchronized scheduling, subtraction gains, duplicate-play and pause cancellation.
HTTP tests fetched real stereo previews at 48 kHz and checked Origin/Host rejection.
Typecheck and production build passed before final documentation; rerun commands below.
No browser click/visual or actual listening QA was performed by the agent.
WebMCP read/stage tools are feature-detected; no supported browser validation context
was available, so their registration/state transitions are unverified. They cannot
submit a human evaluation or start playback.

Scaffold-wide lint contains existing UI primitive warnings. Dependency audit still
reports seven advisories in the pinned Cloudflare development dependency tree; this
is not a security-cleared public deployment. Keep it loopback-only. React, Vinext,
RSC plugin and Vite received compatible targeted updates; no forced dependency rewrite.

```sh
build/hssr-teacher-env/bin/python -m unittest discover -s tools/stereo-upmix-render -p test_teacher_review.py -v
cd tools/teacher-review-ui
node --experimental-strip-types --test lib/listening.test.ts
npx tsc --noEmit
npm run build
```
