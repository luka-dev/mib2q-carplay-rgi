# CarPlay Upmix Listening Protocol

_Prepared 2026-09-28 for the owner's Audi B&O system. This is a score sheet and procedure, not a listening result._

## Conditions that can be heard in the car

The phone sends stereo PCM through live CarPlay; the head-unit hook creates the six channels. Today the car can compare **front-only** (`AUDIO_STEREO_UPMIX=off`) with **Track A's deployed configuration**. A future candidate joins only after Track A ports and profiles it on the head unit. The non-causal teacher cannot run there. Six-channel WAV playback and `qsa_play` do not exercise the CarPlay chain and cannot establish an in-car verdict.

Offline shared-mask and teacher renders may be used as a separate **headphone preview** to decide whether a port is worth attempting. Record those impressions separately; the preview cannot stand in for the B&O amp and cabin.

A four-song rear-only diagnostic is at `D:\upmix\runs\listening\rear_preview_sharedmask4k\clips` (slots 01, 03, 05, 09). Each A/B WAV is the C renderer's Ls/Rs output from either the 4k shared-mask student or cached teacher `other`, with the same stereo input and a common gain within each pair. `clips_level_matched` attenuates the louder rear to the quieter one's RMS for voice-leak and timbre comparisons. Keep `private_key.json` closed until notes are recorded. Listen for recognizable vocal content, missing instruments, and a jump at the vocal transition; **rear-only headphones cannot answer whether an instrument sounds behind the driver**. The files are 24 seconds at 48 kHz stereo and are never sent through CarPlay as a six-channel test.

## Prepare the 8–10 song list

The list below was selected from the owner's `D:\music` library using the existing htdemucs four-stem cache. The entry and exit times were verified with `diag_transitions.events` on the teacher vocal stem. The instrumental, dense, and exposed passages were screened from teacher stem energy. The four-stem cache labels everything other than vocals, bass, and drums as `other`; it cannot certify whether a particular sound is guitar, piano, keys, or strings. Those descriptions identify what the listener should check by ear. Match the exact recording/version on the phone; if its timing differs, locate the same musical event before scoring.

Play the same 24-second section for each condition. All ten songs are outside the MoisesDB DEV harness and were chosen before hearing either live condition; do not change the list after seeing a result.

| Slot | Situation | Song / app | Section start–end | What to track |
| --- | --- | --- | --- | --- |
| 01 | Vocal entry at 0:43 | Audioslave — Show Me How to Live | **0:33–0:57** | Distorted guitar depth before and after the voice enters |
| 02 | Vocal entry at 0:22 | Lewis Del Mar — H.D.L. | **0:12–0:36** | Guitar and texture under the first line |
| 03 | Vocal exit at 1:09 | The Animals — House of the Rising Sun | **0:59–1:23** | Organ/guitar depth after the voice stops |
| 04 | Vocal exit at 2:46 | Ozzy Osbourne — Crazy Train | **2:35–2:59** | Whether the guitar jumps rearward at the exit |
| 05 | Sustained instrumental | 2CELLOS — Smells Like Teen Spirit | **2:06–2:30** | Cello/string presence and steadiness |
| 06 | Sustained instrumental | The Pretty Reckless — Sweet Things (Acoustic) | **2:42–3:06** | Acoustic guitar presence and steadiness |
| 07 | Dense chorus | Rag'n'Bone Man — Human | **2:10–2:34** | Keys/piano, voice anchor, and percussion leakage |
| 08 | Dense passage | Woodkid — Pale Yellow | **3:12–3:36** | Orchestral strings/keys under voice and drums |
| 09 | Exposed vocal | Bishop Briggs — The Way I Do | **3:14–3:38** | Any recognizable voice behind the listener |
| 10 | Exposed vocal / piano | Soap&Skin — Me and the Devil | **1:18–1:42** | Voice anchor and piano/strings depth |

Teacher-cache IDs for reproducibility, slots 01–10: `t00084`, `t00522`, `t00435`, `t00628`, `t00105`, `t00377`, `t00122`, `t00110`, `t00028`, `t00104`. The exact detector's event times for slots 01–04 are 0:42.75, 0:22.00, 1:09.25, and 2:45.50 respectively. The 24-second windows were rounded to whole seconds for phone playback.

## Run the live comparison

1. An assistant assigns concealed codes **X** and **Y** to front-only and Track A, then keeps the key away from the listener. If the owner must switch the config and knows which is which, record the result as *sighted* rather than blind.
2. Keep MMI Surround, Focus, Bass, Sub, 3D, seat, phone app, playback quality, SoundCheck, and source sections fixed. Use the same phone volume. Disable crossfade and any app EQ that could vary between passes.
3. Set each configuration through the established deployment procedure; it takes effect on the **next phone connect**. Do not reboot the head unit or restart j9. Confirm the intended mode from the hook's health/status line before scoring.
4. Match perceived level before scoring. Prefer a phone-mic recording at the driver's head position of the **same 60-second section** and MMI setting for X and Y; adjust the MMI level until the measured average differs by at most about 0.5 dB, then lock the levels for that pass. Record the actual MMI level and measured difference. A by-ear match is acceptable for a personal screen, but mark it as such.
5. Play slots 01–10 in condition X, then the identical sections in Y. Repeat slots 01, 03, 07, and 08 in reverse condition order to check whether the preference survives familiarity and order. A helper should operate the config and conceal the key; the listener should avoid reading status or file names until the form is complete.
6. For each section, answer the four questions below immediately. After both conditions, choose the preferred code for that section. Open the key only after the form is saved.

The first live pass answers whether Track A is preferable to front-only and which failures remain. It does **not** certify a shared-mask student or the teacher. After a candidate is ported, run the same list, level match, and questions against Track A, with front-only retained as the control.

## Score sheet

Use one row per slot and condition; copy the table for the repeated passages. Score rear instruments and movement from 1 to 5: **1 = absent/no movement; 5 = clearly behind/obvious movement**. For entry slots 01–02, score a change when singing starts; for exit slots 03–04, score a change when singing stops; for the other slots, score movement across the passage, or write N/A if there is no event to judge. A good candidate has high instrument score and low movement score. For voice, mark yes if any recognizable vocal appears behind. "Daily" is the owner's practical decision.

| Slot | Code X/Y | Instruments behind 1–5 | Voice behind? Y/N | Movement/ducking 1–5 or N/A | Would use daily? Y/N | Notes |
| --- | --- | ---: | --- | ---: | --- | --- |
| 01 | X |  |  |  |  |  |
| 01 | Y |  |  |  |  |  |
| 02 | X |  |  |  |  |  |
| 02 | Y |  |  |  |  |  |
| 03 | X |  |  |  |  |  |
| 03 | Y |  |  |  |  |  |
| 04 | X |  |  |  |  |  |
| 04 | Y |  |  |  |  |  |
| 05 | X |  |  |  |  |  |
| 05 | Y |  |  |  |  |  |
| 06 | X |  |  |  |  |  |
| 06 | Y |  |  |  |  |  |
| 07 | X |  |  |  |  |  |
| 07 | Y |  |  |  |  |  |
| 08 | X |  |  |  |  |  |
| 08 | Y |  |  |  |  |  |
| 09 | X |  |  |  |  |  |
| 09 | Y |  |  |  |  |  |
| 10 | X |  |  |  |  |  |
| 10 | Y |  |  |  |  |  |

| Slot | Preferred code X/Y/tie | Confidence 1–5 | Reason |
| --- | --- | ---: | --- |
| 01 |  |  |  |
| 02 |  |  |  |
| 03 |  |  |  |
| 04 |  |  |  |
| 05 |  |  |  |
| 06 |  |  |  |
| 07 |  |  |  |
| 08 |  |  |  |
| 09 |  |  |  |
| 10 |  |  |  |

Record the date, car firmware/build identity, hook and mask hashes, config/health line, phone model and app, MMI settings, level-match method and result, condition key, listener, and any interruption alongside the completed sheet. The unit's restart or a different playback route invalidates the pass.

## References

- [Separator handoff](UPMIX_SEPARATOR_HANDOFF.md) — goal, results, and Track A ownership.
- [Upmix listening and deploy log](UPMIX_LISTENING_AND_DEPLOY_LOG.md) — prior sighted verdicts and live level-check guidance.
- [Landmines](../../LANDMINES.md) — CarPlay playback and `qsa_play` traps.
