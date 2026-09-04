# MHI2Q Cinemo AAC thread and coexistence audit

Date: 2026-09-03

Scope: creating an AAC-LC decoder from the existing type-103 APAP receiver thread inside
MHI2Q `dio_manager`

Status: static audit complete; no hook, firmware, startup configuration or deployed binary changed

## Result

It is structurally safe to create and own one Cinemo AAC decoder from the existing APAP receiver
thread, provided creation happens after stock Cinemo configuration has completed and every call on
that decoder remains serialized on that thread.

The feared collision with a stock audio decoder in the same process is not present in this MHI2Q
image:

* `dio_manager` imports `CinemoCreateConfig`, `CinemoCreateMetapool`, `CinemoCreateIAP` and
  `CinemoErrorToString`; it does **not** import `CinemoCreateAudioCodec`;
* no other executable or shared object in the extracted image imports that public audio-codec
  factory, and `libNmeSDK.so` has no internal code caller of its exported factory;
* the host process therefore contains stock Cinemo configuration/iAP state, not a stock
  `CinemoAudioCodec` object with which the hook would share decoder state;
* each factory call allocates a new SDK wrapper, a new AAC decoder and a new PCM output object;
* the only shared component on this path is Cinemo's class/plugin registry, whose initialize,
  lookup, load and registration operations are protected by a mutex and whose loaded modules are
  reference-counted;
* AAC decode and PCM delivery are synchronous. Neither the AAC decoder nor the PCM output plugin
  creates a worker thread, and the application callback is reached by a direct `BLX` before
  `ICinemoAudioCodec_Write` returns.

`libNmeAudioAAC.so` has a `DT_NEEDED` entry for `libOmxCore.so`, but no `OMX_*` import, relocation,
`dlopen` or `dlsym` path. The AAC route is an in-process software decoder and does not allocate an
OMX component. The old conclusion that an unknown shared OMX resource made coexistence unsafe is
therefore refuted.

This closes the static concurrency risk. It does not prove real-time CPU margin or exclude a bug in
the 2017 vendor decoder; those are bounded stationary canary tests, not reasons to replace the
decoder before implementation.

## Audited artifacts

The target throughout is MHI2Q MU1316. MH2p is not used for the safety conclusion.

| Artifact | SHA-256 | Role |
|---|---|---|
| `eso/bin/apps/dio_manager` | `e9862680d76ca2e6bb2eed067658f2ca8fa9b71b5446322384b76c9735c0e794` | hook host and stock Cinemo caller |
| `armle/usr/lib/libNmeSDK.so` | `a34a968e10b5193d9bd8ee80abd89d29e987d1b573a72f44d85865fd11ace73e` | public codec wrapper |
| `armle/usr/lib/libNmeBaseClasses.so` | `d69b568cc93759d44c7ac9dfd94d069ee52d7fc28829e9feb1086627d244505b` | class factory and module registry |
| `armle/usr/lib/cinemo/libNmeAudioAAC.so` | `89536dcdfddeab67e98ce8945fc7e4346fa6d71da0de170f8c36d98475a6cd46` | AAC-LC software decoder |
| `armle/usr/lib/cinemo/libNmeEncoder.so` | `d2bdf3401dfef5ef6b2462417d50b88197c305679c781725ac1b03d4d19c82d7` | PCM output adapter |
| `armle/lib/libOmxCore.so` | `17210d4ba63820f6d3435395249f2509c769277e2e73862d40b0294172a71c02` | audited because of the nominal dependency |

The installed `cinemo_classes.xml` maps `NmeCodecAAC` to `NmeCreateCodecAAC` in
`NmeAudioAAC`, and `NmeEncodePCM` to `NmeCreatePCMEncoder` in `NmeEncoder`.

## 1. What is actually shared inside `dio_manager`

The complete direct Cinemo import set in `dio_manager` is:

```text
CinemoCreateMetapool
CinemoCreateConfig
CinemoErrorToString
CinemoCreateIAP
```

There is no `CinemoCreateAudioCodec` import. A raw-string and ELF-symbol scan across the extracted
MHI2Q image finds the name only in `libNmeSDK.so` and its analysis database. IDA finds only the
export/data reference to `CinemoCreateAudioCodec @ 0x4ea44`, not an internal SDK call site.

This distinction matters. The hook will share the already initialized Cinemo configuration and
class registry with stock iAP handling, which is intended. It will not attach to, reconfigure or
release a stock player decoder. Local media playback in another process has a separate address
space; the audited AAC route also has no hardware/OMX handle to contend for across processes.

The stock order visible in `dio_manager` is configuration creation, plugin-directory setup,
metapool/options setup and then `CinemoCreateIAP`. A type-103 SETUP and its cookie cannot arrive
until that iAP instance is active. Lazy decoder creation from SETUP/the first authenticated media
item is consequently after class-factory configuration, as required.

## 2. Shared class factory is serialized

The global object at `libNmeBaseClasses.so:0x26a8c` is a real class-factory object, not a decoder
singleton. Relevant operations are:

| Function | VA | Relevant behavior |
|---|---:|---|
| `NmeClassFactory::Create` | `0xe7a1c` | locks the factory mutex at `this+8` around first creation |
| `NmeClassFactory::Init` | `0xe7e48` | initializes the factory; stores no creator-thread identity |
| `NmeClassModules::Initialize` | `0xed8d4` | locks the module table during initialization |
| `NmeClassModules::Register` | `0xebb00` | uses the module-table mutex |
| `NmeClassModules::Lookup` | `0xeb610` | uses the same mutex |
| `NmeClassModules::LoadPlugin` | `0xeb408` | under the mutex, reuses or loads/inserts one module entry |
| `NmeClassFactory::CreateInstance` | `0xe8004` | selects a class, calls the locked loader, then invokes its object factory |
| `NmeClassDLL::AddRef` / `Release` | `0xe73dc` / `0xe7380` | interlocked module lifetime count |

`NmeFindAudioDecoder @ 0x10fe98` is only a subtype-to-class-name switch. AAC-LC subtypes resolve to
`NmeCodecAAC`; it holds no mutable decoder state.

The unsafe operation would be racing stock initialization or `NmeClassFactory::Delete @ 0xe9888`,
not creating another object after initialization. The type-103 receiver runs in the latter phase.

## 3. Every codec object is private to the caller

In `libNmeSDK.so`:

* `CinemoCreateAudioCodec @ 0x4ea44` allocates a fresh 112-byte wrapper on every call;
* `CinemoAudioCodec_InitializeImpl @ 0x4e740` stores the application's callback and context in that
  wrapper at `+0x50` and `+0x54`;
* `CinemoAudioCodec_CreateInputDecoder @ 0x4e470` obtains a new decoder instance through
  `CreateInstance` and stores it only in the new wrapper;
* `CinemoAudioCodec_CreateOutputPath @ 0x4df28` independently creates `NmeEncodePCM` for that
  wrapper.

In `libNmeAudioAAC.so`, `NmeCreateCodecAAC @ 0x4a20` allocates a fresh 7536-byte decoder object.
There is no process-global AAC codec object. In `libNmeEncoder.so`, `NmeCreatePCMEncoder @ 0x13e64`
likewise allocates a fresh `0x190`-byte PCM output object.

No creator-thread ID or thread-local ownership check was found. Calling every method from one owner
thread is still the simplest and safest policy; it avoids asking the vendor ABI to support a case
the hook does not need.

## 4. Decode and callback execution stay on the caller's stack

The recovered index-23 path is:

```text
APAP receiver thread
  -> ICinemoAudioCodec_Write
  -> NmeCodecAAC_Write                         AAC 0x4dc8
  -> NmeCodecAAC_DecodeElementaryStream       AAC 0x4540
  -> decoder callback BLX                     AAC 0x4750
  -> CinemoAudioCodec_OnDecoded               SDK 0x4eb9c
  -> output encoder Write                     direct vtable call
  -> NmeAudioEncoder_Write                    Encoder 0xb204
  -> NmeEncodePCM_Write                       Encoder 0x13848
  -> NmeAudioEncoder_DeliverPCMInline         Encoder 0xb058
  -> CinemoAudioCodec_OnPCM                   SDK 0x4e638
  -> application decoded_callback BLX         SDK 0x4e710
  -> return through the same chain
```

The AAC plugin imports no `pthread_create`, condition-variable, task-queue, `dlopen` or `dlsym`
entry. Its decode routine may consume an AU partially and may produce zero or several callback
chunks, but any callback occurs synchronously during the `Write`/`Drain` call that produces it.

The PCM output plugin also has no `pthread_create` import. At `Encoder:0xb058`, it loads the
per-object callback from `this+0xa8` and immediately calls vtable slot `+0x14`. The SDK endpoint at
`0x4e638` loads the application callback/context from wrapper `+0x50/+0x54` and executes the direct
`BLX` at `0x4e710` before returning. `libNmeEncoder.so` does contain condition-variable helpers for
other registered encoder classes, but the recovered `NmeEncodePCM` path neither calls them nor
enqueues the buffer.

The donor's 100 ms condition-variable wait is therefore not part of the MHI2Q AAC/PCM contract and
must not be copied. There is no late callback for which the hook needs a second thread or a wait.

## 5. Why the OMX dependency is not a conflict

Both `libNmeAudioAAC.so` and `libNmeSDK.so` name `libOmxCore.so` in `DT_NEEDED`. Neither binary has
an undefined `OMX_*` symbol or a relocation against one. Neither imports `dlopen`/`dlsym`, so there
is no hidden name-based OMX acquisition on the audited path. Loading the AAC plugin also does not
newly load OMX in `dio_manager`, because the host already has `libOmxCore.so` in `DT_NEEDED`.

As a secondary check, the core itself is multi-client rather than a one-owner singleton:

* `OMX_Init @ 0x20e4` takes `omx_core_lock @ 0x2380` and increments the global reference count;
* `OMX_Deinit @ 0x1f84` uses the same lock and tears down only at zero;
* `OMX_GetHandle @ 0x1c1c` takes a reference before allocating/initializing a handle;
* `OMX_FreeHandle @ 0x1a7c` deinitializes that handle and releases the locked reference.

This fallback evidence is not needed to justify Cinemo AAC, because that decoder never calls OMX.

## 6. Required integration rules

1. Create the decoder lazily after negotiated index 23 and a validated ASC are available. Do not
   call Cinemo from the preload constructor.
2. Resolve the public wrappers from the already loaded `libNmeSDK.so`. Reuse the stock registry;
   never call `CinemoCreateConfig`, change `SetPluginDirectory`, or initialize a second global
   Cinemo configuration.
3. Use exactly one decoder per type-103 stream generation. The existing APAP receiver thread owns
   `Create`, `Initialize`, all `Write`/`Drain`/`Flush` calls and `Release`; no new thread is needed.
4. Treat the callback as inline and non-reentrant. It validates format/generation and copies PCM
   into a decoder-private bounded FIFO. It must not call back into Cinemo.
5. Do not hold the APAP socket lock, QSA lock or shared render lock across a Cinemo call. After
   `Write` returns, transfer complete frames from the private FIFO into the shared render ring.
   This avoids lock inversion even though there is no asynchronous callback.
6. Permit zero or multiple callback chunks per AU and retain partial PCM frame state. Do not impose
   a one-callback-per-1024-frame assumption.
7. On `FLUSHBUFFERED`, call `Flush` on the owner thread and clear both private and pre-flush shared
   PCM. On cookie/format/generation replacement, flush, release and recreate the decoder.
8. Release the object on controlled stream teardown before its callback context is freed. Do not
   make Cinemo calls from an ELF destructor, where ordering against class-factory deletion is not
   defined. Process-exit cleanup may be left to the OS if normal stream teardown did not run.

## 7. Residual test and FDK-AAC fallback

No further static discovery gates implementation. The captured index-23 corpus already contains
601 raw AAC-LC access units (12.821333 seconds), and a standard decoder reconstructs exactly
`601 * 1024` frames of 48000 Hz stereo PCM. That corpus can exercise parsing, FIFO behavior and
expected output offline.

The first target test should remain a bounded stationary canary:

1. keep the known-good startup bundle and `AUDIO_SPATIAL=0` available for immediate rollback;
2. with stock local playback stopped, create/decode/flush/release one type-103 generation in
   `dio_manager` and require valid 48000 Hz stereo S16 geometry, sane callback counts and no
   `dio_manager`/Cinemo fault;
3. repeat ordinary local playback before and after the decoder lifetime;
4. only then advertise index 23 for an audible stationary CarPlay session.

This tests CPU margin and vendor behavior. A standalone process is weaker evidence for the original
question because it does not share `dio_manager`'s already initialized registry; it would need its
own Cinemo configuration and plugin-directory setup.

FDK-AAC remains a rollback design, not the recommended first implementation. The image's
`NmeEncodeFDK` entry is an **AAC encoder**, not a decoder, and no FDK decoder API or reusable decoder
library was found in the image. If the canary exposes a real Cinemo defect, vendor FDK-AAC as an
isolated ARMv7/QNX shared library with a small versioned C API, load it optionally exactly as
`libcarplay_apac.so` is loaded, and use MPEG-4 raw transport plus the validated ASC. It must not add
a hard `DT_NEEDED` dependency to the hook or replace the working APAC decoder.

## Confidence

| Finding | Confidence | Basis |
|---|---|---|
| no stock public audio-codec factory user exists in `dio_manager` | high/direct | ELF imports plus image-wide symbol/string scan and SDK xrefs |
| each SDK/AAC/PCM object is independent | high/direct | allocation and ownership paths in all three MHI2Q binaries |
| class/plugin registry supports post-init cross-thread creation | high/direct | mutex-protected initialize/load/register/lookup and interlocked module refs |
| AAC-to-application PCM callback is synchronous | high/direct | complete direct call chain to SDK `BLX @ 0x4e710`; no thread creation or wait on this path |
| AAC decoder uses OMX | refuted | no OMX imports, relocations or dynamic symbol lookup |
| one owner-thread decoder can coexist with stock `dio_manager` iAP state | high/static | independent objects plus serialized shared registry |
| real-time CPU margin and absence of vendor runtime bugs | unmeasured | requires only the bounded stationary canary above |
