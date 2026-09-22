# V14: independent audio-content audit and karaoke input ablation

2026-09-09. Six 20-second development excerpts; NOT all 587 corpus clips.
No human listening by the agent, no clean reference stems, no training or vehicle changes.
Actual local audio inference was completed using two models distinct from the separators:
[MIT AST / AudioSet](https://huggingface.co/MIT/ast-finetuned-audioset-10-10-0.4593)
and [LAION CLAP](https://huggingface.co/laion/clap-htsat-unfused).
Neither is a validated judge of foreground/background musical role or stem purity.

## Main conclusion

Do not convert separator filenames into training truth or rear-routing instructions.
The strongest demonstrated issue is input sensitivity of the karaoke teacher on
Us V Them: feeding the original mixture instead of the already-separated all-vocals
changes the lead waveform substantially. Its RMS rises 11.50 dB and the remaining
vocal residual falls 5.47 dB. Independent content evidence also shifts toward singing
in the new lead, but this is not proof that either lead/backing pair is clean.

The initial hierarchy remains a diagnostic candidate, not an approved teacher pipeline.
Do not silently replace the teacher for every song based on this six-excerpt test.

## Method and provenance

1. Verified SHA-256 of original stereo, base teacher provenance and every specialist stem.
2. Analyzed all 18 available waveforms per excerpt (mix, two base stems, 15 specialist
   stems), plus `mix − hh` and `mix − backing`. Parent/child stems are NOT summed as
   independent sources.
3. Used two consecutive 10-second windows, with common-level and explicitly declared
   diagnostic-level views. Diagnostic gain targets mono RMS 0.05, capped at +20 dB and
   peak 0.95; it is NEVER applied to teacher files or the listening UI.
4. AST uses native 16 kHz and reports multi-label sigmoid scores. CLAP uses 48 kHz,
   two fixed text prompts per category and reports mean cosine similarity, not softmax
   probabilities. Filenames, song titles and intended stem names are not model inputs.
5. Retained stereo/mono energy, frequency-band energy and aligned waveform correlation.
   A shared waveform is evidence of overlap, NOT a percentage of contamination.
6. Ran silence, sine, synthetic kick and noise-hat controls. The first pass contains
   474 music views + four controls; the input-ablation follow-up adds 140 music views
   + four repeated controls. These are repeated views of six excerpts, not 622 songs
   or independent examples.

Model revisions, local file hashes, packages, gains, window times, prompts and scores:

- `build/reference-audio/v14-independent-audit-a/results.json`
- `build/reference-audio/v14-independent-direct-ablation-a/results.json`
- Direct-input WAVs/provenance: `build/reference-audio/v14-karaoke-direct-a/<id>/`

AST revision `f826b80d28226b62986cc218e5cec390b1096902`;
CLAP revision `8fa0f1c6d0433df6e97c127f64b2a1d6c0dcda8a`.
Inference stayed local on CPU (two threads); only model weights were downloaded.
Karaoke ablation used the original native-44.1k/shared-gain policy, overlap four,
and identical verified karaoke model/config hashes. It ran on MPS.

## Findings per excerpt

| Excerpt | Evidence | Interpretation / conservative action |
|---|---|---|
| **Us V Them, 203.9 s** | Initial backing is 13.41 dB above lead; only 1.0% side energy; waveform cosine with Demucs vocals 0.914. Direct-mix input increases lead 11.50 dB, decreases residual 5.47 dB. | Strong input-path sensitivity; initial backing is not established as a spatial background object. Preserve front anchoring; compare the two teacher paths before admission. |
| **Block Out The World, 74.1 s** | HH has AST Hi-hat scores 0.179 / 0.449 in the two diagnostic windows; CLAP hi-hat 0.445 / 0.594. HH waveform overlap with vocal teacher is low (cosine 0.030), but that cannot exclude all leakage. | Best supported HH candidate in this six-clip sample, NOT a purity pass. Backing is about as loud as lead and receives strong singing descriptors; backing role remains unresolved. |
| **Archers, 101.4 s** | HH is −35.2 dB relative to mix; CLAP predictions vary with level and include processed-vocal/drum descriptions; AST Hi-hat remains low. Guitar identity has support from both models. Direct-mix karaoke residual shifts CLAP toward harmony descriptors (0.621 vs lead 0.574). | Do not normalize the quiet HH into a strong training-positive source. The direct-input residual merits comparison, but harmony retrieval does not prove separate backing singers. |
| **Paradise City, 162.2 s** | HH is −32.4 dB relative to mix, with unstable snare/drum/vocal-like descriptors. Guitar has support from both models. Direct-mix karaoke raises residual 4.13 dB and changes its content descriptors. | Quiet HH ambiguous; no automatic positive HH label. More residual energy is not automatically more useful ambience. |
| **Bellingham (Slowed), 57.3 s** | Backing is 12.68 dB above lead and has waveform cosine 0.916 with Demucs vocals. Descriptors include processed vocals/speech. AST supports guitar, while CLAP favors snare for the guitar-labeled stem. Direct-mix karaoke changes lead only +0.14 dB. | Input-path change does not resolve this case. Processed/slowed vocals and instrument ambiguity must remain unknown-role rather than forced backing/guitar truth. |
| **Cities In Dust, 103.0 s** | Luka reports a sustained wordless vocal sample. Initial lead is ~−95.7 dBFS. Direct-mix lead rises to ~−67.4 dBFS but is still very quiet; vocal residual stays ~0.085 RMS and almost unchanged. CLAP favors singing descriptors even for the reported sample. | No evidence that an ordinary lead part disappeared. Treat as vocal sample/texture with unresolved routing role. A large dB change from near-zero is not evidence of meaningful lead recovery. |

### Piano warning

In five excerpts piano is only −47 to −62 dB relative to the mixture. In Us V Them
it is more substantial (−20.3 dB), but CLAP consistently favors processed-vocal
descriptions (~0.60) rather than piano. AST does not independently settle its identity.
This is a strong reason to withhold automatic piano labels, not a definitive claim
that this track contains no piano. The [Demucs authors explicitly describe the
six-source piano output as artifact/leakage-prone](https://github.com/facebookresearch/demucs#demucs-music-source-separation).

## Karaoke input ablation: all six

Positive delta means the direct-original-mix estimate is louder than the vocal-only-input estimate.

| Excerpt | Lead RMS delta dB | Vocal residual RMS delta dB | Old/direct lead waveform cosine |
|---|---:|---:|---:|
| Archers | −0.34 | +0.56 | 0.984 |
| Bellingham | +0.14 | −0.04 | 0.964 |
| Block Out The World | +0.74 | −0.65 | 0.936 |
| Cities In Dust | +28.27 | ~0.00 | ~0.000 (near-silence estimates) |
| Paradise City | −0.62 | +4.13 | 0.940 |
| Us V Them | +11.50 | −5.47 | 0.302 |

On Us V Them, CLAP lead-singing cosine changes from 0.278 to 0.574 in the
lead output (diagnostic-level window mean); AST singing score changes 0.011→0.069.
The new residual still receives singing descriptors. Thus the test supports
input sensitivity, not a completed solution to vocal-role separation.

Direct residual is `RoFormer all-vocals − karaoke lead(original mix)`. A lead
estimated from a different input may introduce cancellation or non-vocal artifacts
into that residual. No new residual was relabeled as verified backing.

## Why not let AST/CLAP approve the teachers?

- Silence control: AST's top label is Silence (0.608), but Music also scores 0.272.
  CLAP chooses snare (cosine 0.392) from the fixed musical description set on silence.
  It must retrieve something even when none of those descriptions is appropriate.
- AST recognizes the sine control as Sine wave (0.889); CLAP identifies the synthetic
  noise-hat (0.672) and synthetic kick (0.465). These checks establish useful behavior
  on limited controls, not calibration on real separated music.
- A low AST hi-hat score is not evidence of absent hi-hat: AST's 16 kHz input loses
  frequencies above 8 kHz. Some tested HH stems have 25–53% of spectral energy above
  8 kHz. CLAP also has a bounded spectral frontend; neither replaces full-band listening.
- Mono model input may lose side information. Paradise City's quiet `other` stem has
  96.2% side energy and should not be judged from its weak mono fold-down.
- A backing singer and a lead singer can have the same timbre. Contrastive text-audio
  similarity cannot establish foreground role, intelligibility or spatial intent.
- Gain-dependent rankings and quiet residuals require abstention, not confident labels.

## Consequences for the next upmix design (proposals, not implemented changes)

1. **Separate source identity, musical role and confidence.** Vocal sample, lead singer,
   HH and piano are not rearability labels. Unknown role should not become a rear-positive.
2. **Retain a conservative vocal-protection target.** The union of uncertain vocal
   evidence may veto extraction; do not force a karaoke residual rearward. Using a
   conservative union can over-protect instruments, so validate the tradeoff.
3. **Use class-specific teacher confidence and explicit unknown/ignore labels.** Do not
   hard-label all Demucs piano, all DrumSep HH or all karaoke residuals. Near-silent
   stems must not become equal-weight instrument exemplars through normalization.
4. **Audit teacher input context.** Compare original-mix and vocal-only karaoke inputs;
   retain disagreements. Do not select whichever output is louder as the winner.
5. **Do not equate width with background.** Wide guitars can carry the main hook, while
   a narrow backing vocal can still be backing. Slow phrase/context cues need separate
   supervision; short time-frequency masks alone cannot decide musical importance.
6. **Architecture after labels.** Current code is 368 features → two 160-unit GRUs →
   264 outputs over 32 bands. The V13 routing-only pilot freezes recurrent/semantic
   heads and uses 192 control frames with 64 burn-in; it did NOT retrain those heads
   from the two-stem pseudo-taxonomy. Assess a slower context/confidence branch only
   after creating role labels; increasing hidden size cannot repair teacher semantics.
7. **Respect cabin constraints.** Stock 3D OFF is not bypass; rear-door woofer/tweeter
   share a circuit. We cannot send HH only to a rear tweeter or remove bass solely
   through a class name. Validate rear-band energy, center protection and LF budget
   through the known electrical matrix, without pretending it is a complete acoustic model.

## Reproduction and continuing work

```sh
build/hssr-teacher-env/bin/python tools/stereo-upmix-render/audit_teacher_audio_content.py --output build/reference-audio/NEW-AUDIT-DIRECTORY
PYTORCH_ENABLE_MPS_FALLBACK=1 build/hssr-teacher-env/bin/python tools/stereo-upmix-render/audit_karaoke_input_ablation.py --output build/reference-audio/NEW-ABLATION-DIRECTORY
build/hssr-teacher-env/bin/python -m unittest discover -s tools/stereo-upmix-render -p test_audio_content_audit.py -v
```

The six-clip audit and ablation are complete. Four diagnostic-unit tests pass, and
all reported AST/CLAP score ranges and finite values were checked. Existing teacher
and playback files, human annotations, model checkpoints and car files were unchanged.

The 587-clip base pass continues separately. The existing 50-song specialist queue
still produces review-only estimates using the original hierarchy. A new bounded
diagnostic queue waits for those 50 selected excerpts, then processes five clips per
subprocess to limit RAM. Status: `build/reference-audio/v14-independent-full50/status.json`.
This is queued analysis, NOT completed full-corpus validation. No automatic training follows.
