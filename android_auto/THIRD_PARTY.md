# Sources and attribution

The receiver hook's protocol adaptation derives from chopinwong01/OneB1t's
https://github.com/chopinwong01/mhi2-android-auto-video-vc (GPL-3.0).
Its transport and player were replaced with a bounded shared-memory queue and
an MHI2Q QNX Screen/Qualcomm decoder implementation.

`vendor/cluster_surface.c` and `.h` are the existing local managed-window
helpers, copyright 2026 LuKa (@LuKa_dev). The maneuver renderer, Java cluster
foundation and selected resources also retain their upstream attribution.

FFmpeg 6.1.5 (LGPL-2.1-or-later configuration, H.264 decoder/parser only) is
statically linked. The build downloads the exact unmodified archive from
ffmpeg.org, verifies SHA-256, and keeps the configuration and player source
needed to rebuild against a modified FFmpeg. The QNX SDK/toolchain and target
system libraries are not redistributed. No NVIDIA/Tegra player binary is used.

New native transport/player/installer sources are GPL-3.0-or-later. See
`../LICENSES/README.md` for the mixed-source licensing map.
