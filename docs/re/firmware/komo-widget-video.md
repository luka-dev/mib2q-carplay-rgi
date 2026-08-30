---
title: KOMO widget video & gfxAvailable gate
tags: [re, firmware, most, video, verified]
status: partially-verified
sources:
  - firmware: libPresentationController.so, videoencoderservice, KOMOService (diag.jar/DSITracer.jar)
  - firmware: DSIKOMOGfxStreamSink (org.dsi.ifc.komogfxstreamsink), EB GUIDE ViewModeSM
verification:
  - KVS enum + komoviewstyle sizes confirmed by decompile (libPresentationController.so)
  - encoder chain confirmed as strings in videoencoderservice
  - gfx chain: DSIKOMOGfxStreamSink + updateGfxState + KOMOService found only as strings in lsd jars;
    ATTR_GFXSTATE / setGFXAvailable / ClusterViewMode not found in this extract (inferred names)
reconciles:
  - docs/reference/widget_video_architecture.md
  - docs/reference/gfx_available_root_cause.md
  - docs/reference/lvds_video_pipeline.md
---

# KOMO widget video & gfxAvailable gate

The stock cluster video pipeline (which [[compositing]] hooks into) and the flag that lets the VC
actually show it.

## Pipeline

```mermaid
flowchart LR
    pc["PresentationController<br/>renders -> framebuffer (displayable)"] --> ipte["videoencoderservice<br/>IPTE capture"]
    ipte --> h264["QC OMX H.264"] --> ts["MPEG-TS"] --> iso["MLB ISO"] --> most(["MOST"]) --> vc["VC LVDS"]
```

## Widget size - komoviewstyle

Widget render size comes from `komoviewstyle.conf`. The current MHI2Q FPK cluster widget is
**`KVS_FPK`** - DSI 2 = **210x153**, DSI 3 = **328x181** (agrees with [[compositing]]). Other styles:
`KVS_Most` **800x252** (MOST display, DSI 1), `KVS_RGI` **263x366** (old MIB1 style, DSI 255);
DSI 4-7 = `KVS_Invalid`.

`KVS_RGI2` exists only as **EB style enum ordinal 2** in the binary (switch FUN_00638d44:
`0 Invalid / 1 RGI / 2 RGI2 / 3 FPK / 4 Most / 5 Debug_MoKoInMainDisplay`) and has **no DSI
assignment** in this build's `komoviewstyle.conf`. There is no `363x260` size anywhere in the
firmware - the earlier "KVS_RGI2 363x260" claim was wrong (a scramble of RGI's 263x366).

## The gfxAvailable gate

The VC only transitions to LVDS map view (`SV_LVDS_NavMap_FPK`) when **`gfxAvailable=true`** - else the
video never shows even though it is encoded and sent. The flag is driven by the KOMO GFX-stream-sink
DSI interface, and combined with `LVDS_Available=1` (from MOST video sync) it walks the EB GUIDE state
machine to the map view:

```mermaid
flowchart LR
    ves["videoencoderservice"] --> sink["DSIKOMOGfxStreamSink<br/>(org.dsi.ifc.komogfxstreamsink)"]
    sink -->|"GFX-state DSI attr"| ks["KOMOService.updateGfxState(i,j)<br/>if j==1 -> gfxAvailable = (i==1)"]
    ks --> gate{"gfxAvailable=true<br/>AND LVDS_Available=1?"}
    lvds["MOST video sync"] --> gate
    gate -->|yes| sm["EB GUIDE ViewModeSM -> MAP<br/>BAP rgType=4 -> INTERN_Active_NavFPK_Content=Map(1)"]
    sm --> view["SV_LVDS_NavMap_FPK"]
    gate -->|no| hidden["video encoded + sent,<br/>but VC shows nothing"]
```

Verified as strings in this extract: `DSIKOMOGfxStreamSink` (traceConfig.properties + DSITracer.jar),
`updateGfxState` and `KOMOService` (diag.jar / DSITracer.jar). **Not found literally** here:
`ATTR_GFXSTATE`, `setGFXAvailable`, `ClusterViewMode` - likely EB GUIDE HMI-model symbols not shipped
in this firmware dump, so treat those exact names as inferred. None of this chain lives in
`libPresentationController.so`.

## Relevance to the patch

Publishing our maneuver stream is not enough; the cluster must also see `gfxAvailable=true`. This is
why the renderer startup handshake gates the MOST video path on a confirmed first frame before the
cluster context switch - a broken/blank stream must never be published. See [[display-contexts]] and
[[compositing]].
