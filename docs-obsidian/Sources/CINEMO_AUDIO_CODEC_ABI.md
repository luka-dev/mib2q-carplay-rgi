# Cinemo audio-codec ABI

How to create an AAC-LC decoder from Cinemo's SDK, recovered from the firmware binaries and
confirmed on the unit with `tools/cinemo-probe`. Addresses are file offsets in
`Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app/armle/usr/lib/`.

## Why this exists

`dio_manager` took SIGABRT the first time the buffered AAC path asked for a decoder. The cause
is at the bottom of this document, and it is not what the crash looked like.

## The call sequence

All three steps are required. Skipping either of the first two produces a specific error, which
is how each was identified.

```c
CinemoCreateConfig(&cfg);                                   /* else the class factory is null */
ICinemoConfig_SetPluginDirectory(cfg, "/mnt/app/armle/usr/lib/cinemo");
CinemoCreateAudioCodec(&codec);
ICinemoAudioCodec_Initialize(codec, ctx, callback, &inputType, &outputType);
```

`dio_manager` already does the first two for its own use, so the hook inherits a configured SDK
and only needs the last two. The probe stands alone and must do all four.

## ICinemoAudioCodec_Initialize

Exported shim at `libNmeSDK.so:0x9eedc`, 36 bytes. It forwards r0-r3 unchanged and copies one
stack argument, then calls vtable slot 5:

```
ldr ip, [r0]       ; the object's vtable
ldr lr, [sp, 0x10] ; exactly one stack argument
str lr, [sp]
ldr ip, [ip, 0x14] ; slot 5
blx ip
```

So the arity is **five**, and the implementation is `libNmeSDK.so:0x4e740`, which stores its
arguments as:

| arg | register | stored at | meaning |
| --- | --- | --- | --- |
| 1 | r0 | — | the codec object |
| 2 | r1 | this+0x54 | context passed back to the callback |
| 3 | r2 | this+0x50 | callback; **NULL returns 11** |
| 4 | r3 | — | input `CinemoMediaType *`, to `0x4e470` |
| 5 | stack | — | output `CinemoMediaType *`, to `0x4df28` |

The callback is stored next to the context and never dereferenced as an object: it is a plain C
function pointer, not a COM interface. The media types are passed by pointer — `0x4e470` hands
its argument straight to `NmeMedia::NmeMedia(CinemoMediaType const&)`.

Input is configured first and its error returned immediately; the output call is a tail call.
Any error you see from Initialize while the input is malformed therefore comes from the input.

## Error codes, and what each one means here

Recovered from the guards themselves, not from a table:

| code | name | condition |
| --- | --- | --- |
| 11 | Argument | the callback argument is NULL (`0x4e77c`) |
| 12 | Resources | an allocation failed, **or** the class-factory global is null (`0x4e4a4`, `0x4e510`) |
| 20 | — | `NmeFindAudioDecoder(subtype)` found nothing (`0x4e53c`) |
| 23 | Interface | no registered class implements `INmeAudioCodec::iid` |

Error 20 is the useful one for validating constants: because we never saw it, the subtype
`0x101a` for AAC-LC is confirmed correct. The lookup reads the subtype from **input+4**, which
confirms that offset too.

Error 23 originates in the class factory, not in the plugin. `NmeCreateCodecAAC`
(`cinemo/libNmeAudioAAC.so:0x4a20`) has its own 23 at `0x4a5c`, guarding a `strcmp` of the
requested interface id against its own, but both sides relocate to the same symbol
(`_ZN14INmeAudioCodec3iidE`) and so always agree. The factory answers 23 while it knows no class
at all — which is what an unset plugin directory looks like. Loading `libNmeAudioAAC.so` by hand
with `dlopen` does **not** fix it; only `SetPluginDirectory` does.

## The crash: the codec owns the header

`CinemoMediaType` carries the AudioSpecificConfig as a pointer at +0x78 with its length at
+0x7c. **The codec takes ownership of that pointer and frees it when released.**

Passing a pointer into our own handle aborted the process:

```
free malloc object that is not allocated: .../lib/c/alloc/dlist.c:1121
```

That is the `dio_manager` SIGABRT. The stock unit has its plugin directory configured, so
Initialize there succeeded and the process died later, at teardown, which is why the crash did
not look like a decoder problem at all.

The header must be its own allocation, handed over and never freed by us. Ownership transfers
only on a **successful** Initialize: if it fails, the buffer is still ours to free.

Both behaviours were confirmed by running the probe each way: a heap ASC exits 0, a static one
aborts in release.

## Confirmed layout

Only the fields the code was seen to touch are listed. The rest of the 0x80-byte structure is
untested.

| offset | field | evidence |
| --- | --- | --- |
| 0x04 | subtype | read by the decoder lookup at `0x4e518` |
| 0x74 | header type (5 = ASC) | accepted in a working run |
| 0x78 | header pointer | freed by the codec — see above |
| 0x7c | header length | accepted in a working run |

Media type, sample format, rate and channel fields were accepted in a working configuration but
never observed being read, so their offsets rest on the donor, not on the disassembly.

## Reproducing

`tools/cinemo-probe/cinemo_probe.c`, built for QNX and run on the unit. It touches no audio
device and is safe while the car is playing.

```
PROBE_CONFIG=1 ./cinemo_probe          # the working sequence, exits 0
PROBE_STACK_ASC=1 ./cinemo_probe       # reproduces the ownership abort
PROBE_PLUGIN=<path> ./cinemo_probe     # dlopen a plugin by hand (does not help)
```
