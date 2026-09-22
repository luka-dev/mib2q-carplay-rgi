# iOS 26.6 altScreen / stream-111 adversarial verification

## Bottom line

**The current hook, as written, should not be expected to make iOS 26.6 offer stream 111.** The decisive bug is in FIX 1: the control-SETUP request proposes keys under `features`, but the phone reads the accepted array from the SETUP response key **`enabledFeatures`**, not `features`. The hook currently writes `response["features"]` at `altscreen_hook.c:968-984`; that value never becomes the negotiated array tested by `carEndpoint_Activate`, so endpoint byte `+0x3E` remains clear unless the receiver independently returns `enabledFeatures:["altScreen"]`.

This is hook-fixable. On the control SETUP, merge **the receiver's existing `response["enabledFeatures"]`** with only `"altScreen"`, and write the result back under `enabledFeatures`. Do not echo the request's entire `features` proposal. After that correction, the 26.6 binaries force the low-level path to create a stream-111 descriptor when the `/info` raw `displays` array has at least two valid displays and the cluster's raw `uuid` differs from display 0's `uuid`.

The suspected phone-side certificate ceiling is not present on the actual cluster-URL service path. `CarPlayServices` returns a non-null URL array with nil error, including an empty array when no application library is available. Geo support changes the array contents; it is not an authorization failure. A service transport failure or anomalous null reply can still abort activation, but the binaries do not show a certificate or entitlement check in this path.

## Scope and provenance

The findings below were re-derived from the extracted iOS 26.6 (23G71, iPhone18,2) Mach-O files, not inherited from the 26.1 reports or IDA comments.

| Image | SHA-256 |
|---|---|
| `AirPlaySender` | `952d73f8e97d4697490f69bdf08067df6ccff29f5b44c60489eb117db015d832` |
| `CarKit` | `f7c09ecce816e5228331645cb17bc88da8187cf59f74b180474c3b7b847c21f5` |
| extracted `CarPlayServices` | `0205ea0609db5dfeb991f1139e832b8b47e7896f6e7a024b6d84c4a3c2296e4f` |
| extracted `carkitd` | `e8918a92b52867b64a8f459af32dd7b4d617be7e21bd5d20dba3341b973f4229` |

The temporary audit databases were opened as `aps266_verify`, `carkit266_verify`, `carplayservices266_verify`, and `carkitd266_verify`; the pre-existing project databases were not modified.

## Claim verdicts

| Claim | Verdict | Binary/source evidence | Correction or qualification |
|---|---|---|---|
| **A1** | **CONFIRMED** | In `___carEndpoint_Activate_block_invoke_2` (`AirPlaySender` `0x2277F9014`), X26 is counted at `0x2277F9754-0x2277F9758`, searched with `CFArrayContainsValue` at `0x2277F9768-0x2277F9770`, and the Boolean is stored by `STRB W8,[X27,#0x3E]` at `0x2277F977C`. CFString object `0x2838239C8` points to cstring `0x22793EECA`, exactly `altScreen`. Stub `0x22C826050` resolves to CoreFoundation `_CFArrayContainsValue` (`0x18353D638`). | `+0x3E` (decimal 62) is the altScreen byte. The same X26 array drives `uiContext` (`0x2838239E8` -> `0x22793EED4`, store `+0x3F` at `0x2277F97A8`), `viewAreas` (`0x28381D088` -> `0x227919088`, store `+0x3D` at `0x2277F97D4`), and `enhancedSiri` (`0x2838239A8` -> `0x22793EE95`, store `+0x39` at `0x2277F9808`, subject to the adjacent prerequisite at `+0x38`). |
| **A2** | **CONFIRMED** | `_carEndpoint_setupStreams` starts at `0x2277FF304`. It obtains `_kAPEndpointDescriptionProperty_DisplayDescriptionArray` around `0x2277FF600-0x2277FF840`, calls `CFArrayGetCount` at `0x2277FFA9C`, tests count >= 2 at `0x2277FFAA0`, then reads the same endpoint byte `[X19,#0x3E]` at `0x2277FFAAC`. `CSINC` at `0x2277FFAB4` selects full count only when that byte is set; otherwise it selects one. | The count source is the parsed raw `/info` **`displays`** array, not the later scaled-display result. `_airPlayDescription_copyDisplayDescriptionArray` (`0x227812DA4`) reads CFString `displays` (`0x283823AC8` -> cstring `0x22793F3F6`) at `0x227812E20-0x227812E30` and creates one `APEndpointDisplayDescription` per entry at `0x227812E50-0x227812EA0`. `_APEndpointDisplayDescriptionCreateWithDisplayInfo` (`0x2277BAC7C`) accepts and retains a dictionary; it does not pre-scale it. |
| **A3** | **CONFIRMED** | `_APEndpointStreamScreenCreate` (`0x227850B74`) gets display 0 at `0x227850E28-0x227850E78`, retrieves the first and current UUIDs at `0x227850E8C-0x227850EC4`, and compares them with `CFEqual` at `0x227850EC8-0x227850ECC`. At `0x227850ED0-0x227850ED8`, `MOV W8,#0x6E; CINC W8,W8,EQ` produces 110 for equality and 111 for inequality; the value is stored at stream `+0x58` at `0x227850F50`. | The raw display dictionary's `type:111` does not select stream 111. A non-first display receives type 111 only when its raw UUID is non-null and differs from display 0's UUID. |
| **A4** | **REFUTED** | `_apsession_appendControlSetupRequest` (`0x227785570`) inserts the proposal at `0x227785EA4-0x227785EB8` using CFString `features` (`0x283819648` -> cstring `0x2278FE4F4`). `_apsession_ensureControlStreamSetup` (`0x22786DB80`) later extracts the accepted array from a different response key at `0x22786DD14-0x22786DD54`. The `+0x3E` store is solely from this returned array (`0x2277F9754-0x2277F977C`). Separately, `_airPlayDescription_extendedFeaturesHasKey` (`0x227813494`) examines the parsed `/info` extended-features array at `0x2278134AC-0x2278134D8`; it has no dataflow to `+0x3E`. | It is RTSP control SETUP, not `/info`, but the exchange is asymmetric: **request `features` -> response `enabledFeatures`**. The claim's response-key half was wrong. `/info extendedFeatures` remains separate endpoint/session-configuration metadata and does not set byte `+0x3E`. |
| **B1** | **REFUTED — DEFINITIVE** | `_apsession_ensureControlStreamSetup` (`0x22786DB80`) sends/receives via `_apsession_requestControlSetupWithResponse` at `0x22786DC18-0x22786DC34`. On the relevant mode-5 path (`CMP W8,#5` at `0x22786DD0C`), it obtains the CFArray type ID at `0x22786DD14`, loads CFString object `0x283819388` at `0x22786DD1C`, performs a typed dictionary get at `0x22786DD2C`, and retains the resulting array directly into the output at `0x22786DD4C-0x22786DD54`. `0x283819388` points to cstring `0x2278FE1DC`, exactly **`enabledFeatures`**. | The SETUP response key is `enabledFeatures`, not `features`. No proposal/response intersection exists in this parser; the response array is trusted directly. This is the controlling correction for FIX 1. |
| **C1** | **CONFIRMED** | `_APEndpointDisplayDescriptionCopyProperty` (`0x2277BAE7C`) dispatches raw properties through its block at `0x2277BAFE0`. Its UUID branch compares the requested property at `0x2277BB038` and loads raw-key CFString object `0x28381B728` at `0x2277BB044`; that object points to cstring `0x22790CC0B`, exactly **`uuid`**. | AirPlaySender's raw `displays` path consumes lowercase `uuid`. It does not use CarKit's physical-panel key `uid` for the stream-type comparison. |
| **C2** | **REFUTED in part** | `-[CARDisplayInfo initWithPhysicalScreenDictionary:]` (`CarKit` `0x1CAFBED6C`) loads `_CRUIDKey` (`0x1E77F8978`) and validates a non-null NSString at `0x1CAFBEDF0-0x1CAFBEE98`: physical panels do use `uid`. `-[CARSessionConfiguration initWithSessionStatusOptions:propertySupplier:]` (`0x1CAFBA0DC`) loads `_CRDisplayPanelsKey` (`0x1E77F89B8`) at `0x1CAFBAF8C-0x1CAFBAF9C`, parses the panel array, and stores it at configuration `+0x78` at `0x1CAFBB0AC-0x1CAFBB0BC`. However, the predicate invoked at `0x1CAFBB1D4-0x1CAFBB1E4` is block `0x1F4111948`, whose invoke `0x1CAFBE77C` calls `supportsDDPContent`; `-[CARDisplayInfo supportsDDPContent]` (`0x1CAFC04D0`) reads byte `+0xA`. The properties parser at `0x1CAFDE9CC` maps `Instrument Cluster` at `0x1CAFDEA70` to a different byte, `supportsInstrumentClusterContent` at `+0xB`. | `uid` and the `displayPanels` source are confirmed. The stated `hasGaugeClusterScreen = contains Instrument Cluster` rule is wrong in 26.6: the stored predicate result is based on **DDP content**, not the `Instrument Cluster` property. This CarKit semantic flag is not the A2/A3 low-level stream-111 gate. |
| **C3** | **REFUTED in part** | C1 and C2 prove that a dictionary reused under both arrays needs both identifiers. But CarKit's physical parser also rejects missing pixel size (`0x1CAFBEDC8-0x1CAFBEDCC`), physical size (`0x1CAFBEDD8-0x1CAFBEDDC`), invalid/missing `uid` (`0x1CAFBEDF0-0x1CAFBEE98`), or missing/non-array `initialVideoStreams` (`0x1CAFBEF80-0x1CAFBEFD0`). `_CRSizeFromAirPlayDictionary` (`0x1CAFBF110`) requires `widthPixels` and `heightPixels` at `0x1CAFBF13C-0x1CAFBF190`; `_CRPhysicalSizeFromAirPlayDictionary` (`0x1CAFBF590`) requires `widthPhysical` and `heightPhysical` at `0x1CAFBF5BC-0x1CAFBF610`. AirPlaySender independently rejects zero pixel dimensions and invalid area at `0x2277FA8C4-0x2277FBA00`, and invalid normal physical dimensions at `0x2277FA58C-0x2277FBC10`. | Both `uuid` and `uid`, plus a distinct cluster raw `uuid`, are necessary for the reused dictionary, but they are not alone sufficient. Valid non-zero dimensions and a valid physical-panel `initialVideoStreams` array are also required. The current cluster builder supplies those fields at `altscreen_hook.c:248-256,261,308-315`; the main dictionary is augmented at `:327,340-344`. |
| **D1** | **REFUTED** | Current hook `altscreen_hook.c:958-984` creates key `features` (`:968`), copies every request-proposed feature (`:970-978`), appends `altScreen` (`:979-981`), and sets the response using the wrong key (`:982`). B1 proves the phone reads `enabledFeatures`. A1 proves every string placed in the accepted array can set a sibling endpoint byte. | Change the response key to `enabledFeatures`. Merge the existing **response `enabledFeatures`**, not the request proposal, then add only `altScreen`. Replacing the response with `["altScreen"]` would be sufficient for the 111 gate, but can silently discard legitimate receiver-enabled features. Echoing all proposed features is less safe because it can falsely enable `viewAreas`, `uiContext`, or `enhancedSiri`. |
| **D2** | **UNCERTAIN as a whole; key usage is confirmed** | C1/C2 prove that using the same textual identifier under two different keys is acceptable: AirPlaySender reads `uuid`; CarKit reads `uid`. The stream selector only applies `CFEqual` (`0x227850EC8-0x227850ED8`) and performs no UUID-format validation. The hook writes the same cluster string to `uid` and `uuid` at `altscreen_hook.c:256,261`. But display 0 comes from the runtime stock result at `altscreen_hook.c:350-351`; the hook neither reads nor compares its raw `uuid` against the fixed cluster value. The readable SDK lineage only conditionally inserts the main UUID after `ScreenCopyProperty(kScreenProperty_UUID)` (`AirPlayReceiverSessionScreen.c:498-505`). | `cluster.uid == cluster.uuid` is safe. A collision with the main UUID is highly implausible but is not proven impossible by either binary or current code. Read and log `displays[0]["uuid"]` at injection time; reject/replace a missing or equal value. The only format constraint shown by the selector is non-null plus inequality. |
| **E1** | **REFUTED** | AirPlaySender reads `+0x3E` at `0x2277F9FE8` and calls `_APCarPlay_CRFetchInstrumentClusterURLs` at `0x2277F9FF4`; a nonzero status (`0x2277F9FF8`) or null array (`0x2277FA000`) aborts at `0x2277FC088/0x2277FC090`, while a non-null empty array passes and is encoded as `altScreenURLs` at `0x2277FA014-0x2277FA090`. CarKit `_CRFetchInstrumentClusterURLs` (`0x1CAFFE6B8`) uses `CARInstrumentClusterURLController`; its Mach endpoint is `com.apple.CarPlayApp.service` (`CFString 0x1F4116770` -> `0x1CB0292A5`), and the reply block at `0x1CAFFC0DC-0x1CAFFC1A8` forwards the URL array/error. The actual server, `-[CRSAppHistoryService serviceFetchInstrumentClusterURLs:]` in `CarPlayServices` (`0x24A121C1C`), updates geo support and returns `instrumentClusterURLs` with nil error at `0x24A121F60-0x24A121F88`. `_appLibraryUpdated:` (`0x24A122510`) stores either generated URLs or an empty-array singleton at `0x24A1227CC-0x24A1227F8`. No certificate or entitlement test occurs on this path. | The certificate serial number belongs to the **scaled-display** helper, not the cluster-URL authorization path. `_CRFetchScaledDisplaysForCertificateSerialNumber` is at `0x1CAFFEBC0`, but AirPlaySender falls back to the unscaled display array on fetch failure at `0x22780013C-0x227800190`. Cluster URL transport/error/null remains a theoretical fatal condition; the extracted service implementation does not make certificate/entitlement eligibility such a condition. |

## B1: exact negotiation dataflow

The response-key question is fully settled by a backward trace from X26.

1. `_apsession_appendControlSetupRequest` (`0x227785570`) places the phone's proposal in the RTSP control-SETUP request under **`features`** at `0x227785EA4-0x227785EB8`.
2. `_apsession_requestControlSetupWithResponse` (`0x227785334`) constructs and sends that SETUP at `0x2277853E8-0x227785474` and returns the response dictionary.
3. `_apsession_ensureControlStreamSetup` (`0x22786DB80`) receives it at `0x22786DC18-0x22786DC34`. On the CarPlay mode-5 path, it typed-gets a CFArray using CFString **`enabledFeatures`** at `0x22786DD14-0x22786DD2C`, then retains that array directly at `0x22786DD4C-0x22786DD54`.
4. That returned array becomes X26 in `___carEndpoint_Activate_block_invoke_2`; `altScreen` membership is tested and stored at endpoint `+0x3E` at `0x2277F9754-0x2277F977C`.

There is no intersection with the proposal between steps 3 and 4. Therefore a receiver can set `+0x3E` by returning `enabledFeatures:["altScreen"]` even if `altScreen` was absent from the request proposal. That is what the binary does; whether advertising an unproposed capability is protocol-conservative is a separate policy question.

The correct hook behavior is conceptually:

```c
/* Only for the control SETUP; request["features"] identifies that exchange. */
accepted = mutable_copy_or_empty(response["enabledFeatures"]);
append_if_absent(accepted, "altScreen");
response["enabledFeatures"] = accepted;
```

Do not populate `enabledFeatures` by copying every entry in `request["features"]`: the request is a proposal, not proof that this receiver accepted or implements each feature.

## Display-property audit

### What AirPlaySender reads from raw `displays`

`_APEndpointDisplayDescriptionCopyProperty` and its block (`0x2277BAE7C`, `0x2277BAFE0`) expose the following raw dictionary mapping. Most missing values propagate as null/default; unsupported property requests return an error. The entries marked fatal become explicit activation or stream-construction gates elsewhere.

| Raw key(s) | Purpose | Absence fatal to 111? |
|---|---|---|
| `uuid` | Display identity and 110/111 selector | **Yes.** Display 0 and the cluster both need non-null UUIDs; equality selects 110, inequality selects 111 (`0x227850E8C-0x227850ED8`). |
| `widthPixels`, `heightPixels` | Synthesizes pixel size | **Yes in the normal schema.** Missing/zero dimensions fail validation (`0x2277FA8C4-0x2277FB9D0`); area over the selected limit also fails (`0x2277FA930-0x2277FBA00`). |
| `widthPhysical`, `heightPhysical` | Synthesizes physical size | **Yes for the concrete physical-size form used by this hook.** Zero/invalid values fail the activation checks (`0x2277FA58C-0x2277FBC10`). |
| `widthPixelsMax`, `heightPixelsMax` | Maximum pixel size | No shown mandatory check for this injected shape. |
| `maxFPS` | Maximum FPS and refresh-rate source | No shown non-null gate. Current hook supplies it. |
| `features` | Input-capability bitfield (knob/touch/touchpad properties) | No shown 111 gate. |
| `viewAreas`, `initialViewArea`, `adjacentViewAreas` | View-area metadata | No shown 111-construction gate. |
| `primaryInputDevice` | Primary input | No shown 111 gate. |
| `rotation` | Rotation support | No shown 111 gate. |
| `edid` | EDID | No shown 111 gate. |
| `HDRInfo`, `receiverSupports444`, `receiverSupportsHDRMirroring` | HDR/chroma capability | No shown 111 gate. |
| `overscanned`, `zoomFactor` | Display geometry behavior | No shown 111 gate. |
| `cornerMasks` | Corner-mask metadata | No shown 111 gate. |
| `nightMode`, `limitedUI`, `limitedUIElements` | UI-state metadata | No shown 111 gate. |
| `url`, `initialURL` | Application/initial URLs | No shown 111 gate. |
| `type` | Receiver-advertised stream type | **Ignored by the selector.** UUID equality chooses 110/111. |

### Additional CarKit physical-panel requirements

When the same dictionary is put under `displayPanels`, `-[CARDisplayInfo initWithPhysicalScreenDictionary:]` (`0x1CAFBED6C`) additionally requires:

- a non-null string `uid` (`0x1CAFBEDF0-0x1CAFBEE98`);
- `widthPixels` and `heightPixels` (`_CRSizeFromAirPlayDictionary`, `0x1CAFBF110-0x1CAFBF190`);
- `widthPhysical` and `heightPhysical` (`_CRPhysicalSizeFromAirPlayDictionary`, `0x1CAFBF590-0x1CAFBF610`); and
- an `initialVideoStreams` NSArray (`0x1CAFBEF80-0x1CAFBEFD0`), whose entries must themselves parse as stream information.

The current cluster dictionary has all four categories (`altscreen_hook.c:248-256,308-315`). Reusing one object shape under both `displays` and `displayPanels` is therefore valid provided it carries both `uuid` and `uid`.

The 26.6 binary does **not** support the current comments that `properties:["Instrument Cluster"]` is “the actual hasGaugeClusterScreen gate” (`altscreen_hook.c:262`) or that this property makes CarKit negotiate altScreen (`:435-443`). `Instrument Cluster` sets `supportsInstrumentClusterContent`; the `hasGaugeClusterScreen` predicate tested in this initializer calls `supportsDDPContent`. Neither is the low-level stream-count/UUID gate.

## Phone-side cluster URL and scaled-display behavior

The URL stage remains ordered between flag negotiation and the follow-up `/info` request:

1. Endpoint `+0x3E` is tested at `0x2277F9FE8`.
2. `_APCarPlay_CRFetchInstrumentClusterURLs` is called at `0x2277F9FF4`.
3. A nonzero status or null array aborts at `0x2277FC088/0x2277FC090`; a non-null empty array passes.
4. The returned array is placed in the follow-up `/info` request under `altScreenURLs` at `0x2277FA014-0x2277FA090`.

The service implementation behind `com.apple.CarPlayApp.service` does not consult a certificate serial number or an entitlement in `serviceFetchInstrumentClusterURLs:` (`0x24A121C1C`). It updates geographic support, then returns its maintained URL array and nil error (`0x24A121F60-0x24A121F88`). Its app-library update routine stores an empty array rather than null if no library is available (`0x24A1227CC-0x24A1227F8`). Maps URLs are included only when Maps is present and geo-supported; eligible third-party applications are selected by `supportsCarPlayInstrumentClusterScene`. Those conditions control content, not whether the returned object exists.

The similarly named carkitd method `-[CRCarKitServiceAgent fetchInstrumentClusterURLsWithReply:]` (`carkitd` `0x1000724B0`) returns an error/empty result, but it is **not** the connection used by `CARInstrumentClusterURLController`; the controller's embedded endpoint string identifies `com.apple.CarPlayApp.service`. Treating that carkitd method as the active URL ceiling would be a false match.

The certificate serial number appears in the scaled-display call (`_CRFetchScaledDisplaysForCertificateSerialNumber`, `0x1CAFFEBC0`). That result does not control the raw display count and is not fatal: after `_APCarPlay_CRFetchScaledDisplaysForCertificateSerialNumber` at `0x22780012C`, AirPlaySender reconstructs an unscaled array from the original descriptions on failure/null at `0x22780013C-0x227800190`.

Useful phone-side log anchors are:

- CarKit URL reply block: `received urls: %@ error: %@` around `0x1CAFFC118-0x1CAFFC170`;
- CarPlayServices `serviceFetchInstrumentClusterURLs:` and `URLs supported` logging around `0x24A121C1C-0x24A121F58`;
- AirPlaySender URL-fetch failure paths at `0x2277FC088/0x2277FC090`; and
- AirPlaySender scaled-display failure/fallback around `0x2278001C4-0x2278001E8`.

## Ranked corrections and remaining risks

1. **FIX 1 is wrong and is the present blocker — hook-fixable.** At `altscreen_hook.c:958-984`, replace response key `features` with `enabledFeatures`; seed the new array from the existing response `enabledFeatures`, and append only `altScreen`. Scope this mutation to the control SETUP (the request's `features` key is the direct discriminator shown at `0x227785EA4-0x227785EB8`) so later media SETUP responses are not decorated accidentally.

2. **Do not echo the whole request proposal.** The accepted array controls at least `viewAreas` (`+0x3D`), `uiContext` (`+0x3F`), `altScreen` (`+0x3E`), and conditionally `enhancedSiri` (`+0x39`) at `0x2277F9754-0x2277F9808`. Claiming features merely because the phone proposed them can activate receiver behavior that stock 210 does not implement. Replacing with `["altScreen"]` is enough for stream 111, but merging the receiver's pre-existing accepted array preserves legitimate stock behavior.

3. **Prove UUID distinctness at runtime.** The cluster's `uid == uuid` is safe, but the fixed cluster value is never compared with the runtime main raw UUID. At `altscreen_hook.c:350-351`, retrieve `mainDisp["uuid"]`, log it together with the cluster UUID, provide a stable fallback if it is absent, and guard against equality. AirPlaySender aborts/chooses incorrectly on missing/equal identifiers at `0x227850E8C-0x227850ED8`.

4. **Correct misleading CarKit comments; do not add DDP as a speculative 111 fix.** `altscreen_hook.c:262,418-443` attributes negotiation to `hasGaugeClusterScreen` and `Instrument Cluster`; the 26.6 binary refutes that relationship. `displayPanels`, `uid`, physical dimensions, properties, and initial streams remain useful/required for the CarKit physical model, but the low-level 111 gate is the accepted `altScreen` flag plus raw `displays` count and UUID inequality.

5. **Ensure the advert gate is actually enabled.** Two raw displays are injected only when both `ALTSCREEN_HANDLE_ALT` and `ALTSCREEN_ADVERT_DISPLAYS` evaluate true (`altscreen_hook.c:405,423-453`). Without the second environment variable, the A2 count stays one even after FIX 1 is corrected.

6. **Phone-side residual risk is transport/null, not certificate eligibility.** A URL-service error or null array is fatal, but the real service implementation normally supplies a non-null array and nil error, including the no-app case. Capture the three URL-stage logs above only if negotiation is proven and 111 still does not appear.

## Deployment expectation and verification

### Current source, unchanged

**No.** It writes `response["features"]`, while iOS 26.6 reads `response["enabledFeatures"]`. The expected observable remains `SETUP: no 111 stream in request` (`altscreen_hook.c:995`) unless another component supplies the correct response key.

### After the one required negotiation correction

**Yes, the authoritative phone binaries are expected to emit type 111**, provided:

- `ALTSCREEN_HANDLE_ALT=1` and `ALTSCREEN_ADVERT_DISPLAYS=1`;
- the control SETUP response contains `enabledFeatures:[...,"altScreen"]`;
- raw `/info displays` contains at least two valid entries;
- display 0 and cluster `uuid` values are present and different; and
- the existing stream-111 receive path can complete its response/transport setup after the request arrives.

Verify in this order:

1. Hook log: `control SETUP: response enabledFeatures contains altScreen` (add this exact log after writing the corrected key). Also log the previous accepted count and whether `altScreen` was already present.
2. `/info` log/dump: raw `displays` count 2, nonzero pixel/physical dimensions, and the two raw `uuid` values present and unequal; `displayPanels` entries also have `uid` and `initialVideoStreams`.
3. Phone log: successful `serviceFetchInstrumentClusterURLs:` and CarKit `received urls: ... error: (null)`; an empty URL array is acceptable.
4. Hook/RTSP log: the media SETUP request's `streams` array contains a dictionary whose `type` is 111, replacing the current `SETUP: no 111 stream in request` message with `SETUP: 111 stream offered` (`altscreen_hook.c:989-993`).

This report supersedes two specific assumptions in the prior reports: the SETUP response key is not an elided/symmetric `features` key, and the active cluster-URL service is not a certificate-gated carkitd path.
