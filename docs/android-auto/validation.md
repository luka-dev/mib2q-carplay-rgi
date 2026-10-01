# Validation record

Validated configuration:

- Audi B9 SQ5, 2018, first-generation Virtual Cockpit
- `MHI2Q_US_AUG22_P3639`, MU0918
- direct wired USB Android Auto
- wireless Android Auto through a USB adapter
- Google Maps and Waze

Confirmed behavior:

- independent H.264 video channel 64 and input channel 65;
- GAL 4.3 request with reply adaptation and acknowledged `0x8009` updates;
- 1920x1080 stream, 1440x540 logical cockpit viewport and 1:1 presentation;
- Qualcomm OMX H.264 decode without fallback or decoder errors;
- approximately 29–30 fps decoded and 24–27 fps shown in the normal view;
- Large, Classic and Sport view changes;
- route start/end gating, HUD guidance and cockpit arrows;
- map zoom through phone relayout, theme and size controls;
- wireless-adapter reconnects do not invalidate the next GAL generation;
- stock GAL fallback and package rollback.

Host coverage includes encoded-ring ownership and resynchronization, navigation translation, UiConfig
encoding, rotary reports, Qualcomm tile conversion, menu geometry, Java member preservation, cluster
state transitions and installer refusal/rollback cases.

Personal drive logs, recorded navigation video and firmware files are intentionally excluded.
