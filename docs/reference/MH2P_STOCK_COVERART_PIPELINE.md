# Stock MH2P CarPlay cover-art pipeline

## Verdict

Stock MH2P does **not** decode cover art in Java and does **not** run image
decode/resize directly inside a low-level `NmeTransport::Recv` callback. It
uses existing native Cinemo metadata and DIO event infrastructure:

1. an existing `airplay::CCommandThread` initializes the Cinemo metadata
   listener;
2. Cinemo publishes a completed `CINEMO_METANAME_IMAGE` object;
3. `CMetadata` filters duplicates by CRC and emits a DIO cover-art event;
4. `CDIOManager` writes/resizes the image into a rotating local cache;
5. DSI sends only the resulting local path to Java as a `ResourceLocator`;
6. Java publishes that locator through `SmartphoneProperties.CoverArt`.

There is therefore no bespoke detached “cover-art worker” comparable to the
one in our LD_PRELOAD hook. There **is** asynchronous execution, but it is
provided by stock command/event infrastructure already owned by `dio_manager`.

## Native MH2P path

Authoritative binary:

`Firmwares/HU/MH2p_US_LB636_P2712/extracted_app_mh2p_lb/eso/bin/apps/dio_manager`

| Stage | Stock evidence |
|---|---|
| Queue metadata initialization | `CIAP2ServiceCinemo_onJob_start @ 0xA2604` allocates a `CMetadataInitJob`; `airplay::CCommandThread::enqueueCommand(..., -1) @ 0xA2858` queues it on the already-existing command thread. |
| Initialize Cinemo metadata listener | `CMetadataInitJob_execute @ 0x9FFCC` calls `CMetadata_init @ 0xA0032`; callback registration occurs at `0xA5F1A`. |
| Detect cover art | `CMetadata_eventCallback @ 0xA6140` handles Cinemo event 21, reads `CINEMO_METANAME_VFS_IAP_FLAGS @ 0xA630E`, and tests cover-art bit `0x20 @ 0xA6362`. |
| Obtain completed image | It requests `CINEMO_METANAME_IMAGE @ 0xA64B6`; the object supplies data pointer, size and CRC (`0xA63BE–0xA63D0`). This is not raw FF5A/JPEG reassembly in this handler. |
| Deduplicate and dispatch | Previous CRC is compared at `0xA6400`; changed bytes are copied into a DIO event at `0xA6432`, dispatched at `0xA6438`. |
| Cache/resize/write | `CDIOManager_onEvent_updateNowPlayingCoverArt @ 0x817F4` reads path/cache/limit settings at `0x8188E–0x818C4`. It uses a default five-file rotation and default 1 MiB resize threshold; the native `CImage` write occurs at `0x81946`. |
| Notify HMI | `CDSICarplayImpl_updateCoverArt @ 0x8BAD4` is called with the completed path at `0x81958`; its listener call at `0x8BBB6` sends a path/string plus `valid=1`, not image bytes. |

The corresponding binary configuration keys are
`IAP2_COVERART_PATH`, `IAP2_COVERART_CACHE`, `IAP2_COVERART_LIMIT`,
`IAP2_COVERART_WIDTH`, `IAP2_COVERART_HEIGHT`, and
`IAP2_COVERART_RESETDELAY`.

## Java/HMI path

Authoritative stock class in `eso/hmi/lsd/lsd.jar`:

`de.audi.app.terminalmode.dsi.carplay.CarplayDSILifecycleController`

Its `updateCoverArtUrl(ResourceLocator, int)` listener validates the flag,
normalizes null to an empty locator, and calls
`smartphoneProperties.getCoverArt().accept(res)`. The property is declared as
`Property<ResourceLocator>` and created with the name `CoverArt` in
`de.audi.app.terminalmode.smartphone.SmartphoneProperties`.

The `mh2p-cluster` repository does not add a second cover-art implementation:
its extra `CarPlayClusterIntegration.updateCoverArtUrl(...)` listener is a
no-op. The original stock lifecycle listener remains responsible for the HMI
property.

## Difference from our MHI2Q hook

Our hook enters much lower in the stack:

- `hook/framework/hook_framework.c:765` interposes
  `NmeTransport::Recv(NmeArray<uchar>&)`;
- `hook/coverart/coverart_hook.c:986` receives raw FF5A link bytes;
- it must reassemble a complete JPEG itself;
- `hook/coverart/coverart_hook.c:598` decodes it through `stb_image`;
- it resizes, PNG-encodes and atomically writes the file itself.

Running those operations inline would block the Cinemo/iAP2 receive thread.
The current worker is therefore functionally necessary unless we move this
work onto some other already-existing safe executor. Simply deleting the
worker would not reproduce the MH2P architecture.

## Implemented stock-like cleanup for MHI2Q

The production hook now applies the smallest safe refinement:

1. keep the one-slot, latest-wins asynchronous queue;
2. stop creating a detached worker eagerly from the cover-art constructor;
3. register/initialize the cover-art module through normal lazy hook-framework
   initialization;
4. start one **joinable** worker lazily when the first complete image is ready;
5. stop and `pthread_join` it on teardown instead of polling a detached thread.

Implementation evidence:

- lazy queue/worker creation: `hook/coverart/coverart_hook.c:747-788`;
- callback quiescence for a concurrently copied sink pointer:
  `hook/coverart/coverart_hook.c:965-1006`;
- lazy framework-owned registration:
  `hook/framework/hook_framework.c:383-387`;
- join-before-bus-shutdown teardown:
  `hook/framework/hook_framework.c:399-406` and
  `hook/coverart/coverart_hook.c:1041-1121`.

Using an existing stock command queue would be even closer to MH2P, but it is
only valid if MHI2Q exposes a lifetime-safe queue at our real dio-to-library
boundary. The MH2P internal `airplay::CCommandThread` is not itself an
interposable API and cannot be assumed available on stock MHI2Q.
