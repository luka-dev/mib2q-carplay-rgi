# Stock 210.81 altScreen / LD_PRELOAD verdict

## Verdict

**A normal `LD_PRELOAD` symbol interpose cannot make stock libairplay 210.81 negotiate or carry CarPlay stream 111.** The first deterministic blocker is **(a)**, but it is more specific than “the `/info` advert was ignored”:

* iOS sends its proposed string feature keys in the **initial control RTSP `SETUP` request** under `features` and takes the negotiated feature-key array from that `SETUP` response (`AirPlaySender_07.mm:5486-5539`, `5925-5929`; `AirPlaySender_20.mm:1242-1321`).
* Stock 210's request processor calls `AirPlayReceiverSessionSetup` internally at `0x2f790`. That function creates the response at `0x27300-0x2733c`, but it neither reads nor writes `features`; it handles control ports, client metadata, and `streams`, then returns the response at `0x2796c-0x27978`. The stock `features` CFL string object at `0x9dff0` has no code xrefs.
* Consequently the returned negotiated array is null/empty, `Activate.mm:314-322` does not set endpoint byte `+63`, the phone does not enter the cluster-URL branch at `Activate.mm:615-668`, and the stream builder deliberately limits itself to display zero at `AirPlaySender_14.mm:5726-5746`. This fully explains the observed `SETUP: no 111 stream in request`.

**(b) is also true, as a downstream stock-210 ceiling.** This binary contains no `altScreen` string, no `HasFeatureAltScreen` function, no type-111 compare, no alt-screen session slot, and no independent second screen receiver. Its only screen setup state is the singleton byte/pointer at session `+5136/+5140` (`_ScreenSetup` at `0x26f9c`). Its SETUP switch accepts only 100, 101, and 110; type 111 follows the unsupported path at `0x2757c-0x27580`. Thus there is no 210 equivalent of MHI3's `ctx+0x127` byte or `HasFeatureAltScreen@0x58ce0` to patch.

**(c) remains a real but currently untested phone-side gate.** It is downstream of (a): only after byte `+63` is set does iOS call its own `APCarPlay_CRFetchInstrumentClusterURLs`; an error or null result aborts activation (`Activate.mm:615-642`). The current failure cannot be attributed to CRFetch because stock 210 deterministically prevents iOS from reaching it.

The smallest production fix within the shipped architecture is:

1. Keep stock `libairplay.so` on disk.
2. From `libcarplay_hook.so`, explicitly rewrite stock's already-relocated, writable GOT entry at `base+0xb8e80` so the **internal** SETUP call reaches the hook's three-argument session-setup wrapper.
3. In that wrapper, add `features = [ "altScreen" ]` to the initial successful SETUP response, then keep the existing `alt_find_111`/`alt_setup` handling for the later media SETUP.
4. Explicitly rewrite `ScreenStreamProcessData` GOT entry `base+0xb9740`; otherwise the existing frame-forwarding interposer is bypassed internally. Rewrite teardown GOT entry `base+0xb90a0` as well so the hook-owned receiver is stopped before the stock session is freed.

This is an **in-memory binary patch applied by the hook**, not symbolic interposition and not a replacement libairplay.

Audited stock binary:

```text
Patches/carplay_vc/libairplay.so
SHA-256 01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2
ARMv7 little-endian, image base 0
```

## Source notation

* `IOS_APS` — `.../AirPlaySender.framework/AirPlaySender`
* `IOS_CK` — `.../CarKit.framework/CarKit`
* `IOS_CKD` — `.../usr/libexec/carkitd`
* `HOOK` — `Patches/mhi2-carplay/hook/altscreen/altscreen_hook.c`
* Stock addresses are virtual addresses in the audited `libairplay.so`; add its runtime load base.

## 1. The actual iOS 26.1 negotiation exchange

### 1.1 The `/info` advert and feature negotiation are different messages

The hook does successfully put `extendedFeatures=["altScreen"]` into an EndpointInfo dictionary (`HOOK:387-411`) and puts the two-display shapes under `displays` and `displayPanels` (`HOOK:418-449`). iOS exposes EndpointInfo's `extendedFeatures` as its `ExtendedFeatures` endpoint property (`IOS_APS/AirPlaySender.mm:8871-8886`), and CarKit copies that property into `CARSessionConfiguration.extendedFeatures` (`IOS_CK/CARSessionConfiguration.mm:574-624`). These facts establish that the advert is real; they do **not** make it the negotiated response used at `Activate.mm:314-322`.

The activation-time proposal comes from phone state:

1. `Activate.mm:119-137` calls `APCarPlay_FetchSessionFeatureKeysWithIdentifier` for the paired vehicle, applies preference overrides, and logs `FeatureKey list proposed` at `:141-180`.
2. The implementation asks `CRFeatureAvailability` for `fetchSupportedAirPlayFeaturesForVehicleIdentifier` and waits for its result (`IOS_APS/AirPlaySender_15.mm:7307-7397`). carkitd obtains stored supported CarPlay features and converts them to AirPlay strings (`IOS_CKD/carkitd.mm:6161-6187`); the conversion adds the literal `altScreen` at `IOS_CK/CarKit.mm:3692-3699`.
3. `Activate.mm:267-278` passes that proposed array through endpoint vtable operation `+104` and receives `v246`, the negotiated array logged at `:309-312`.

The vtable operation is the initial control SETUP, not GetInfo:

* `apsession_requestControlSetupWithResponse` builds a setup dictionary, sends a plist request, and returns the plist reply (`IOS_APS/AirPlaySender_07.mm:5486-5539`).
* `apsession_appendControlSetupRequest` inserts the proposal under the literal key `features` (`IOS_APS/AirPlaySender_07.mm:5672-5674`, `5914-5929`).
* `apsession_ensureControlStreamSetup` calls that request routine (`IOS_APS/AirPlaySender_20.mm:1242-1259`), extracts an optional typed CFArray from the response for CarPlay session type 5, and returns it through its sixth argument (`:1295-1321`).
* The stage-one caller passes the candidate list in, receives that CFArray as `v91`, and returns it to activation (`IOS_APS/AirPlaySender_01.mm:8163-8197`).

Thus the wire exchange that controls byte `+63` is:

```text
iPhone -> RTSP SETUP plist:  { features = [proposed keys], ... }
HU     -> RTSP SETUP plist:  { features = [accepted keys], ... }
                                      |
                                      +-- contains "altScreen"
                                          -> endpoint byte +63 = 1
```

The exact response-key argument is elided by this decompiler at `AirPlaySender_20.mm:1298-1299`, but the request uses the literal `features`, the response accessor requires a CFArray, and the returned array is the one logged and tested as the negotiated FeatureKey list (`AirPlaySender_07.mm:5925-5929`; `AirPlaySender_20.mm:1295-1321`; `Activate.mm:309-322`).

### 1.2 What byte `+63` unlocks

The returned array alone controls the plain alt-screen flag: `CFArrayContainsValue(...,"altScreen")` sets byte `+63` at `Activate.mm:314-322`. There is no theme-asset test in this flag write or in the next alt-specific branch.

Once set:

1. iOS calls its own `APCarPlay_CRFetchInstrumentClusterURLs`; error or null array aborts (`Activate.mm:615-642`).
2. It serializes those URL strings into the follow-up GetInfo dictionary under `altScreenURLs` (`Activate.mm:644-668`) and invokes endpoint vtable operation `+112` (`:738-749`). `apsession_fetchInfo` sends that supplied dictionary as the GetInfo plist request (`AirPlaySender_20.mm:1742-1796`). It is **not** a SetProperty operation.
3. It validates the GetInfo response's `displays` and enforces `altScreen==1 -> display count >=2`; the inverse case rejects two displays (`Activate.mm:805-880`).
4. With count at least two and byte `+63` set, it creates a stream for every display; otherwise it creates only one (`AirPlaySender_14.mm:5726-5746`).
5. It compares each selected display UUID with display zero: same UUID gives 110, different UUID gives 111 (`AirPlaySender_18.mm:9209-9225`). The current advert supplies distinct UUIDs; the raw display dictionary's own `type:111` is not this selector.
6. It appends the selected type to the RTSP `streams` array and sends the media SETUP (`AirPlaySender_01.mm:2438-2577`).

This orders the three candidate blockers unambiguously: missing negotiated response **(a)**, then possible CRFetch failure **(c)**, then media SETUP/receiver handling **(b)**.

## 2. Stock 210 request path and first divergence

### 2.1 Initial/control SETUP is wholly internal

Stock `AirTunesServerHTTP_ProcessRequest@0x302e0` dispatches RTSP SETUP to `sub_2f198` at `0x30bec`. That handler:

* parses the request body through `CFCreateWithPlistBytes` at `0x2f320`;
* removes the encryption key/IV at `0x2f764-0x2f780`;
* loads the session/request/response-out arguments and calls `j_AirPlayReceiverSessionSetup` at `0x2f784-0x2f790` (call bytes at `0x2f790`: `1b a5 ff eb`);
* serializes the returned dictionary through `sub_2f040` at `0x2f7f4-0x2f804`.

The callee's real ABI is three arguments:

```c
OSStatus AirPlayReceiverSessionSetup(
    AirPlayReceiverSessionRef session,
    CFDictionaryRef request,
    CFDictionaryRef *responseOut);
```

The readable SDK declaration is `AirPlayReceiverSession.h:303-313`, and the stock entry at `0x272b8` saves only `R1` and `R2` at `0x272d8-0x272dc`. The hook's current four-argument typedef and wrapper at `HOOK:940-949` are therefore incorrect and must be changed to three arguments.

### 2.2 Stock creates no negotiated feature response

`AirPlayReceiverSessionSetup@0x272b8` creates its response dictionary at `0x27300-0x2733c`. On first setup, its control setup creates timing/events state and inserts the timing/event ports (`0x27b4c-0x27dac`), then marks control setup complete at session `+700` (`0x27ca4`). It subsequently reads client OS/model/UDID and the `streams` array (`0x27350-0x27498`). It never reads the request's `features` array and never inserts a response `features` array. The stock CFL object for `features` at `0x9dff0` has no code xref.

After stream processing, the function calls the platform-control hook at `0x27938-0x27968`, then stores the already-built response dictionary to `*responseOut` at `0x2796c-0x27978`. The platform-control call does not receive that dictionary, so dio cannot add the missing negotiated array through that callback.

This is the first server divergence. Numeric `AirPlayGetFeatures@0x20d24` is not a substitute: it already ORs the 64-bit mask `0x0000000144440b80` at `0x20d70-0x20d80`, whereas iOS's alt gate consumes the string array returned by control SETUP (`Activate.mm:309-322`).

### 2.3 `/info` is a later, separate internal handler

Stock GetInfo handler `sub_30034@0x30034` parses its request at `0x30088`, reads the qualifier but not `altScreenURLs` at `0x300ac-0x300dc`, internally calls `j_AirPlayCopyServerInfo` at `0x301e0`, and serializes the response at `0x3020c`. This is the endpoint-info operation corresponding to `Activate.mm:738-749`, after feature negotiation and CRFetch.

The hook's `AirPlayCopyServerInfo` interpose is a proven dio-to-libairplay boundary seam (`HOOK:359-451`), and its advert reaches iOS. It cannot edit the earlier control SETUP response. Conversely, the request-bearing call at stock `0x301e0` is internal and is not made interposable merely because the target is exported.

The current `SetProperty("altScreenURLs")` code and its URL opener (`HOOK:1264-1293`, `1393-1424`) implement the wrong wire direction for iOS 26.1. The only shown iOS send is the GetInfo request body (`Activate.mm:644-668`; `AirPlaySender_20.mm:1742-1796`). Leaving this handler compiled is harmless, but it cannot negotiate or unlock 111.

## 3. Does stock 210 have MHI3-style altScreen capability state?

No.

The audited 210 binary has:

* zero string/regex hits for `altScreen`, `altScreenURLs`, `viewAreas`, or `Instrument Cluster`;
* zero named functions matching `HasFeatureAltScreen`, `AltScreenSetup`, or `CopyAltScreen`;
* zero instruction-immediate hits for decimal 111 / `0x6f`;
* no code xref to its generic `features` CFL object at `0x9dff0`.

Its stream switch is explicit:

```asm
0x27568  CMP   R0, #100
0x27574  CMP   R0, #101
0x2757c  CMP   R0, #110       ; bytes 6e 00 50 e3
0x27580  BNE   0x27510        ; bytes e2 ff ff 1a -> Unsupported stream type
0x27584  MOV   R1, R10
0x27590  BL    sub_26f9c      ; _ScreenSetup
```

`_ScreenSetup@0x26f9c` proves this is a single-screen implementation:

* session byte `+5136` is tested at `0x26ff0`; if already set, it returns `-6719` at `0x27000`;
* the sole screen-session pointer is session `+5140`, used at `0x27044-0x27058`;
* it hard-codes response stream type **110** at `0x2714c`, irrespective of the request descriptor;
* it sets the singleton setup byte at `0x27184`.

Therefore the superficially analogous branch patch is **not a production fix**:

| Address | Original | Tempting patch | Effect |
|---|---|---|---|
| `0x27580` | `e2 ff ff 1a` (`BNE 0x27510`) | `00 00 a0 e1` (`NOP` / `MOV R0,R0`) | Makes every type above 101 fall into `_ScreenSetup`, not just 111. With both 110 and 111 present, the second call fails the `+5136` singleton check; any successful call still replies as type 110. |

The NOP encoding was independently assembled for ARMv7 little-endian. It is useful only as a parser smoke test. MHI3's `B.EQ -> B` worked against a newer binary that already contained altScreen state and downstream logic (`Patches/mhi3-altscreen/findings/MASTER_KNOWLEDGE.md:250-252`, `350-379`). Those prerequisites do not exist in 210, so copying either MHI3 address or branch semantics would be wrong.

## 4. LD_PRELOAD boundary verdict

There is no symbol-only mechanism that can complete this on stock 210 under the proven call-boundary rule.

| Candidate symbol | Relevant caller in stock 210 | Valid normal interpose? | Verdict |
|---|---|---:|---|
| `AirPlayCopyServerInfo(void *session, void *qualifier, void *mac, OSStatus *err)` @ `0x22254` | A genuine dio call exists and is already intercepted at `HOOK:387-451`; stock GetInfo also has an internal call at `0x301e0`. | **Yes, for the dio call only.** | Sufficient to advertise EndpointInfo; cannot modify the earlier SETUP response. |
| `AirPlayReceiverSessionSetup(session, request, responseOut)` @ `0x272b8` | Request-bearing RTSP call is from stock `sub_2f198` at `0x2f790` through local thunk `0x18c04`. | **No.** | This is the missing negotiation/111 seam, but its caller is libairplay itself. A wrapper log from another callsite does not change the static request-bearing callsite. |
| `CFBinaryPlistV0CreateWithData` @ `0x52088` / `CFCreateWithPlistBytes` @ `0x58520` | Stock request parsers call them internally (`0x2f320`, `0x30088`). | **No.** | Matches the on-unit `seam OK count 0`; cannot observe or modify `altScreenURLs` by ordinary preload. |
| `AirPlayReceiverSessionSetProperty` @ `0x28e20` | SET_PARAMETER route calls it internally; iOS sends `altScreenURLs` in GetInfo, not SET_PARAMETER. | **No and wrong message.** | The handler at `HOOK:1393-1424` cannot fix the flag or URL handshake. |
| `AirPlayReceiverServerSetProperty` @ `0x218d0` | Core/config setter, not the control-SETUP response builder. | No request-bearing dio call was found. | Cannot manufacture the negotiated response. |
| `_ScreenSetProperty`, `_ScreenStreamSetProperty`, `_HTTPServerControl` | No audited dio call carries the SETUP request/response dictionary through these symbols. | **No demonstrated seam.** | They do not sit on the feature-array or type dispatch shown above. |
| `ScreenStreamProcessData` @ `0x84fec` | `AirPlayReceiverSessionScreen_ProcessFrames` calls local thunk `0x1a644` at `0x2b7dc`. | **No.** | Current frame interpose at `HOOK:704-755` is bypassed unless the stock GOT word is explicitly rewritten. |

So the definitive answer is:

* **Hook as ordinary LD_PRELOAD interposer only:** **No.**
* **The same shipped hook plus explicit in-memory patching of stock libairplay:** **Yes, subject to the phone-local CRFetch test.**

The latter is not a “yes mechanism” based on dio calling an exported symbol. It is a deliberate binary rewrite of stock's locally bound dispatch slots.

## 5. Exact minimal binary patch set

### 5.1 Recommended patch: three writable GOT words

The stock ELF maps `.text` in an `R|E` load segment, but maps `.got` (`0xb8d40...`) in its second `R|W` load segment. There is no GNU_RELRO program header. Therefore these pointer replacements require neither `mprotect` nor an instruction-cache flush.

All words below are ARM little-endian:

| Purpose | Stock VA / relocation | On-disk original bytes | Loaded original word | Patched word |
|---|---:|---|---|---|
| Route the internal request-bearing SETUP call through the hook | `base+0xb8e80`, `R_ARM_JUMP_SLOT AirPlayReceiverSessionSetup`; thunk `0x18c04`, caller `0x2f790` | `b8 72 02 00` | `LE32(base+0x272b8)` | `LE32(&hook_session_setup)` |
| Route decrypted frames through the hook tap | `base+0xb9740`, `R_ARM_JUMP_SLOT ScreenStreamProcessData`; thunk `0x1a644`, caller `0x2b7dc` | `ec 4f 08 00` | `LE32(base+0x84fec)` | `LE32(&hook_ScreenStreamProcessData)` |
| Route internal teardown through hook cleanup | `base+0xb90a0`, `R_ARM_JUMP_SLOT AirPlayReceiverSessionTearDown`; thunk `0x19264`, callers include router `0x30b10` | `60 6b 02 00` | `LE32(base+0x26b60)` | `LE32(&hook_session_teardown)` |

The patched bytes are expressed as `LE32(pointer)` because both the stock load base and preload-library function addresses are runtime relocations. Giving four fixed hexadecimal bytes for those values would be false on an ASLR-capable load. The table is nevertheless exact: it specifies the precise destination address, required original value, and replacement value. The installer must refuse to patch if any loaded word differs from its expected stock target.

A robust installer does not need `dladdr`. Resolve the stock entry once, derive and cross-check the base, then patch:

```c
real_setup = dlsym(RTLD_NEXT, "AirPlayReceiverSessionSetup");
base = (uintptr_t) real_setup - 0x272b8;

require(*(void **)(base + 0xb8e80) == real_setup);
require((uintptr_t)dlsym(RTLD_NEXT, "ScreenStreamProcessData") - base == 0x84fec);
require((uintptr_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionTearDown") - base == 0x26b60);

atomic_store(base + 0xb8e80, hook_session_setup);
atomic_store(base + 0xb9740, hook_ScreenStreamProcessData);
atomic_store(base + 0xb90a0, hook_session_teardown);
```

Use aligned 32-bit stores plus a full memory barrier. Keep the three real entry pointers captured **before** replacement; the wrappers must call those direct pointers, not the patched GOT slots.

Install in `altscreen_init` before its current constructor returns (`HOOK:1427-1437`). If QNX loader ordering makes `RTLD_NEXT` temporarily unavailable there, retry at the beginning of the proven `AirPlayCopyServerInfo` boundary call (`HOOK:387-393`); that advert call necessarily precedes activation's control SETUP in the tested flow. Log the three old/new pointers and fail closed—do not advertise altScreen—unless every required slot validates and patches.

### 5.2 Required SETUP-wrapper behavior

Correct `HOOK:942-949` to the three-argument ABI. On every successful stock setup:

1. If this is the initial control SETUP and altScreen handling/advertising is enabled, insert a CFArray containing only `altScreen` into `(*responseOut)["features"]`. `build_extfeatures()` already creates exactly that array (`HOOK:130-143`). Returning only the implemented key avoids falsely accepting unrelated proposed features.
2. Log whether the request's proposed `features` array contained `altScreen`, but do not depend on that log for the deterministic bring-up patch. The shown iOS code consumes the returned array directly and tests it at `Activate.mm:314-322`; it does not show a second intersection with the proposal.
3. On a later request containing a type-111 descriptor, let stock process 100/101/110 and skip 111, then run existing `alt_find_111` and `alt_setup` (`HOOK:919-957`). Return the `alt_setup` error instead of merely logging it. Also make response-descriptor allocation failure in `alt_setup` fail rather than returning success without `{type:111,dataPort}` (`HOOK:896-906`).

The minimal response mutation is:

```plist
features = ( "altScreen" )
```

This makes the **phone's** endpoint altScreen-capable. Stock 210 has no capability byte to set; the hook's per-session `altslot_t` becomes the receiver-side capability/state owner when 111 arrives (`HOOK:574-592`, `825-906`).

### 5.3 Why no on-disk `.text` patch is required

The GOT is already writable and constructors run before the first RTSP request, so an on-disk modified `libairplay.so` is unnecessary. This preserves the deployed stock library file and confines the change to the existing preload hook.

If someone nevertheless patches `.text`, the page containing `0x27580` is `R|E`: it would require page-aligned `mprotect(PROT_READ|PROT_WRITE|PROT_EXEC)`, the write, an ARM instruction-cache clear, and restoration to `PROT_READ|PROT_EXEC`. More importantly, the `0x27580 -> NOP` change remains functionally insufficient because of the singleton/hard-coded-110 facts at `0x26ff0-0x27184`. It must not be described or shipped as the fix.

## 6. Is the existing `alt_setup` / `alt_thread` path sufficient?

It is structurally the right independent 111 receiver, but it is not sufficient as currently reached solely by symbolic interposition.

What is already correct:

* `alt_find_111` scans request `streams[]` for type 111 (`HOOK:919-938`).
* `alt_setup` creates a separate `AirPlayReceiverSessionScreen`, configures it with the 111 descriptor, derives security from `streamConnectionID`, opens a distinct TCP receive socket, starts a receiver thread, and appends `{type:111,dataPort:<port>}` to the response (`HOOK:825-906`). This avoids stock `_ScreenSetup`'s singleton at session `+5136`.
* `alt_thread_main` accepts the phone connection, installs timing/security state, starts the screen session, and calls `ProcessFrames` (`HOOK:776-822`).
* The frame wrapper identifies the hook-owned alt thread, forwards AVCC/SPS-PPS to the alt renderer, and passes main-screen frames to the real function (`HOOK:704-755`).

What must change:

1. **Force the internal SETUP call through the corrected wrapper** via GOT `0xb8e80`; otherwise neither negotiation response mutation nor 111 handling is authoritative.
2. **Force the internal frame call through the existing wrapper** via GOT `0xb9740`. Stock decrypts at `0x2b7a0-0x2b7b8` and calls `j_ScreenStreamProcessData` at `0x2b7dc`; normal preload does not intercept it. Once redirected, `HOOK:730-754` correctly separates the hook's alt thread from the stock main thread.
3. **Force internal teardown through hook cleanup** via GOT `0xb90a0`; the RTSP router calls the local thunk at `0x30b10`. Otherwise a teardown can free stock session state while the hook's listener/receiver thread remains live. The cleanup implementation is already present at `HOOK:968-1005`.
4. Correct the setup ABI and propagate `alt_setup`/response-allocation errors as described above.

With those changes, no Cinemo, Java, iAP2, or from-source libairplay deployment is needed for the RTSP transport path.

## 7. Verification and the remaining phone-only test

### Before enabling the advert

The installer must log and validate:

```text
stock base=<base> sha/build=210.81
GOT setup  @+0xb8e80: <base+0x272b8> -> <hook_session_setup>
GOT frames @+0xb9740: <base+0x84fec> -> <hook_ScreenStreamProcessData>
GOT td     @+0xb90a0: <base+0x26b60> -> <hook_session_teardown>
```

If any original pointer is different, leave `ALTSCREEN_HANDLE_ALT` dark and do not inject the two-display advert.

### Expected successful sequence

1. Hook: `SETUP negotiation: proposed-alt=<0|1>, response features=[altScreen]`.
2. Phone: `FeatureKey list negotiated` contains `altScreen` (`Activate.mm:309-322`).
3. Phone successfully completes `APCarPlay_CRFetchInstrumentClusterURLs` and sends the follow-up GetInfo request with `altScreenURLs` (`Activate.mm:615-668`). Stock will still ignore that request key at `sub_30034@0x30034`, which is acceptable to the shown iOS response validation (`Activate.mm:805-1017`).
4. Hook: `SETUP: 111 stream offered` (`HOOK:953-957`). The request dictionary must contain a second `streams[]` element whose `type` is 111, matching the sender construction at `AirPlaySender_18.mm:9209-9225` and `AirPlaySender_01.mm:2438-2577`.
5. Hook response: a `streams[]` element `{type:111,dataPort:<nonzero>}` (`HOOK:896-905`).
6. Hook: `alt-screen receiver started`, followed by forwarded frame activity (`HOOK:814-821`, `730-750`).
7. Teardown: `altScreen torn down for session=...` before stock frees the session (`HOOK:968-1005`).

### The one result that still requires a phone-side replay/log

After the forced SETUP response proves `negotiated:[altScreen]`, a failure before the media SETUP points to **(c)**. iOS explicitly aborts on CRFetch error/null (`Activate.mm:626-642`), and ordinary HU-side interposition cannot observe the internal GetInfo request body. Capture the phone log around `APCarPlay_CRFetchInstrumentClusterURLs` / `AP_SIGNPOST_CAR_INFOREQUESTFEATURELIST_START`. If CRFetch succeeds, the next missing point is the GetInfo display-validation block at `Activate.mm:805-880`; if it fails, the HU cannot repair the phone-local CarPlayApp service.

That replay is a validation of the remaining external gate, not a reason to defer the stock-210 fix: the missing SETUP `features` response and the three internal-call bypasses are statically proven in the audited binary.
