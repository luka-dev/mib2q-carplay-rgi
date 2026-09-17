# CurrentPosition scrolling through existing BAP

Implemented locally on 2026-09-17, at the user's request. No VC modification.
Width/font evidence: [VC_CURRENT_POSITION_TEXT_FIT.md](VC_CURRENT_POSITION_TEXT_FIT.md).
Vehicle deployment and visual calibration are still pending.

## Runtime behavior

The native `updateCurrentPositionInfo(String)` interface has no pixel-offset
parameter. We send overlapping fragments through FctID 19. These are visible
text steps, with the native VC still centering/shaping each fragment; this is
not a continuous pixel marquee.

- Keep the complete **received** signpost / next-road / current-road payload;
  remove the old early 96-byte truncation. Native upstream input-buffer limits
  are unchanged. Signposts retain both `‹›` quotes on every fragment; next-road
  and time prefixes are budgeted separately.
- Normalize to NFC using embedded Unicode **17.0.0** data, independent of the
  HU's old JRE. Repair isolated UTF-16 surrogates to U+FFFD.
- Segment extended grapheme clusters, including combining sequences, Hangul,
  Indic conjuncts, emoji ZWJ sequences and regional-indicator pairs. Never cut
  inside a cluster to satisfy the transport limit.
- Fit each decorated frame to **353 px** (359 px row minus a provisional 6 px
  rounding reserve) and **96 UTF-8 bytes**. The reserve is not an EAL calibration.
- Latin/Cyrillic/CJK text advances at grapheme boundaries, about 24 px/s with
  no more than four changed timer publications per second. Hold first/last
  frames at least 1800 ms; do not insert a blank frame at the loop boundary.
- RTL and contextual scripts use overlapping pages, preferring word boundaries
  when this retains at least two thirds of the available fragment. Pages hold
  at least 1800 ms and longer when their source progress requires it. RLM/LRM
  preserves the source's first-strong paragraph direction on mixed-script pages;
  its three wire bytes are included in the budget. Never reverse the source.
- If the complete string fits, publish it once and leave no scrolling deadline.
  An explicit View refresh or recovery replay can still republish that string.
- A single cluster that cannot fit produces a static decorated `…` and a
  diagnostic instead of splitting the cluster or spinning indefinitely. Keep
  the original source in the planner. Missing firmware glyphs are separately
  diagnosed, measured with the largest extracted `.notdef` advance, and sent
  intact; the stock VC may show missing-glyph boxes.

## Compact extracted metrics

`tools/generate_vc_text_data.py` reads these five firmware faces **offline**:

- `AudiTypeDisplay-Normal.ttf` at 27 px, the verified CurrentPosition face;
- `LT_Univers440_88perc_Arabic.ttf`;
- `ZYHei_GB18030_c.ttf`;
- `SourceHanSansKR-Regular.otf`;
- `SourceHanSansJP-Regular.otf`.

The four Normal `.lur` files identify characters consistently assigned to Audi
slot 0 across EU/Arabic, China, Japanese and Korea profiles. For those characters
use Audi's advance; otherwise use the largest bound among the possible faces.
The generator includes positive kerning expansion, contextual single-glyph
substitutions (including Arabic forms), and ligature expansion. It ignores
negative kerning rather than letting it under-budget a frame. Orphan combining
clusters reserve a dotted-circle base, which a shaper can insert implicitly.

These are **conservative advance estimates**, not a replacement multilingual
shaper. Context-sensitive text can occupy less space than estimated and can
change shape at page boundaries. Mixed-script bidi, fallback choice, ink
bounds and EAL rasterizer rounding still need a vehicle check. The 5,000 host
RAQM samples are supporting extraction tests, not proof for every Unicode
string or exact VC output. Fonts absent on the VC cannot be added by scrolling;
for example the extracted collection lacks most of the tested Devanagari name.

The generated resource `java_resources/com/luka/carplay/rgd/vc-text.bin` contains
only metric ranges and Unicode properties/NFC mappings, **no font outlines**.
Its exact inputs and SHA-256 hashes are in `tools/vc-text-data/manifest.json`.
Current footprint:

| Table | Records |
| --- | ---: |
| Advance ranges | 1,531 |
| Unicode property ranges | 2,705 |
| Canonical decompositions | 2,081 |
| Canonical compositions | 961 |

Format **VCT2** stores column-wise delta/varint streams. The resource is
**33,928 bytes** before JAR compression and **12,859 bytes** inside the JAR,
down from 87,360 / 34,487 bytes. No metric precision, characters or Unicode
rules were removed: the canonical unpacked SHA-256 is unchanged.

Runtime table arrays total **58,224 bytes** plus small object/array headers,
down from 87,336 bytes. Each record packs three 21-bit fields into one positive
`long`. Binary search reads fields directly; there is no expanded three-int
copy. The loader decodes the columns directly into the final arrays once,
lazily, through a buffered stream. Tick work is unchanged.

Everything is loaded from **inside `carplay_hook.jar`**. No fonts, AWT, ICU,
HarfBuzz library, network request or external resource file is needed on the HU.
The Unicode license is included in `META-INF/UNICODE-LICENSE.txt`. Normal builds
only copy the checked-in resource into the existing JAR; only that JAR needs
updating for this Java feature.

## Scheduling and work bounds

`CurrentPositionScroll` builds an O(n) plan when the normalized source or its
decorations change. It stores offsets and dwell times, not a duplicated string
for every frame. Width and byte prefix sums allow a monotonic window scan.
NFC reorders noncanonical combining runs using stable counting sort, avoiding
quadratic insertion sorting for pathological clusters.

A tick reads the plan, constructs at most one <=96-byte fragment, and invokes
FctID 19 only if that string differs from the previous sent string. It does no
Unicode segmentation or font measurement. Failed sends retain the pending
fragment and retry after 500 ms; late wakeups never replay a frame backlog.
Backward clock corrections or forward jumps over 60 seconds rebase the hold.

The existing serialized `RouteGuidance.presentationLoop` owns the timer; there
is no new thread. Text deadlines share its wait with renderer recovery and
View/OK events. No FctID 20/22, icon, renderer packet, or full BAP sync is issued
by a scroll tick. `RouteGuidance.this` is never acquired while holding
`presentationLock`.

Ordinary distance changes and duplicate source text reuse the plan and preserve
the hold deadline. A new route/maneuver identity, changed text/decorations,
explicit View refresh or info-phase change restarts at the first fragment.
Route stop/disconnect cancels the plan. Loss of renderer readiness suspends
scrolling; cached replay restarts the first fragment on recovery. Retired worker
generations cannot publish an already-due frame, including workers that were
blocked on the route monitor when stop/start changed the generation. Queued
View/OK and readiness work runs before a due text tick.
The first visible fragment gets its full hold even when FRAME_READY arrives
before the worker observes the unready state, on both live-delta and cached
replay paths.

View/OK retries the complete FctID 20/19/22 refresh after 500 ms if any send
throws, even for a fitting string with no scroll timer or further iOS delta.
Retries read the latest requested phase, so a later View reset supersedes a
failed OK toggle. Route end/disconnect clears queued phase changes along with
the text plan; the next route starts in the normal road-text mode.

OK selects a temporary Time mode: **20 seconds after successful publication**,
the same worker automatically restores the latest exit/next-road/current-road
text and restarts its first-fragment hold. No new RGI packet is needed. Route
deltas and cached renderer replays do not extend this deadline. A second OK
returns immediately; a subsequent Time selection starts a fresh 20 seconds.
View, route end, disconnect and module stop cancel the deadline. Failed return
sends use the normal 500 ms retry. This deadline shares the worker's existing
wait with scrolling/recovery; there is no new thread or periodic polling.

## Regeneration and validation

Ordinary build (no font/UCD dependency):

```sh
bash scripts/build_java.sh
```

Offline regeneration/check with extracted firmware fonts and pinned UCD inputs:

```sh
python3 tools/generate_vc_text_data.py --download
python3 tools/generate_vc_text_data.py --check
python3 tests/test_vc_text_metrics.py
VC_UNICODE_TEST_DIR="$PWD/build/vc-ucd-17.0.0" bash scripts/test_route_info.sh
bash scripts/audit_java_stock.sh
```

Default fonts/lookup paths are the existing
`output/r2/vc-fonts-0379-20260910/{fonts,lookup-resources}`. `--fonts`, `--lookups`
and `--ucd` can override them. FontTools is needed for regeneration; Pillow
with RAQM is needed only for the independent metric audit. The generated binary
and manifest are checked in; downloaded UCD files stay in the ignored build
cache. To reproduce existing inputs, use `--download --check`, which rejects
changed hashes/data instead of replacing the recorded output.

Completed host validation:

- Packed format round-trip: all 7,278 records reconstruct the exact prior data
  (SHA-256 `7f11283d92a1ec869641553789e78acf8e55f08dea778c009513719fedbee1b4`).
  Truncated, wrong-version, oversized-varint and trailing-byte resources fail
  explicitly. Runtime tests execute from a temporary directory with only the
  test classes and the shipping JAR on the classpath; assert the data URL is
  inside the JAR, not the checkout or an external font/resource directory.

- All 766 Unicode 17 GraphemeBreakTest cases and 20,034 NormalizationTest rows
  (all five NFC conformance relations per row).
- English, Russian, Spanish, French, Chinese, Japanese, Korean, Arabic and mixed
  direction samples; NFC/NFD identity, supplementary characters, ZWJ/variation
  selectors, missing glyphs, overlarge clusters, width and byte limits, intact
  decorations and reachable final suffix.
- Real bridge/worker tests: FctID 19-only ticks, queue failure, unchanged source,
  distance deltas, cached replay, reused maneuver text, View/OK, renderer loss,
  route stop/disconnect, retired generation (including blocked worker), initial
  publication recovery, failures at each View/OK send, View-before-tick ordering
  and no timer wakeups for fitting text. The integration audit reproduced three
  failures before fixing them: lost View/OK retry, publication by a retired
  blocked worker, and a queued OK phase surviving route end. A further cold-ready
  regression probe protects the complete first visible hold on both publication
  paths after changing the worker's event ordering.
- `RouteInfoTimeoutTest`: exact 20-second deadline, automatic return without an
  RGI delta, restoration of the latest cached exit and first scroll hold, failed
  entry/return publication, manual OK/View cancellation, fresh re-entry timer,
  and bounded wait after a backward HU clock correction.
- 191 multilingual fragments pass through the real MU1316 `AppConnectorNavi`
  and `CurrentPositionInfo_Status` UTF-8 serializer, retaining their complete
  decorated text within the 97-byte total wire limit (96-byte text + length).
  The probe also checks empty FctID 20 and native/CarPlay text ownership.
- 5,000 independently shaped strings across all five extracted faces; generated
  advance bounds contain host RAQM advances (with 1/64 px/glyph quantization).
- Existing route info, maneuvers, lanes, renderer, recovery and native transport
  tests pass. Java 1.4 stock/J9 linkage audit is required after runtime changes.

Next vehicle check: visible cutoff near 353/359 px, Cyrillic/Latin accent fit,
CJK fallback, Arabic/mixed-direction page ordering, centering jitter, initial
holds and update rate. Tune the reserve/timing only from that evidence.
