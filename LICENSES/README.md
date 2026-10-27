# Licensing map

The project is published under GPL-3.0-or-later, subject to the third-party notices below.

- New Android Auto hook, transport, player, installer and tooling under `android_auto/` and the related
  new scripts are offered under GPL-3.0-or-later unless a file says otherwise.
- The protocol hook adapts GPL-3.0 work from OneB1t/chopinwong01's
  `mhi2-android-auto-video-vc`; the combined derivative remains GPL compatible.
- FFmpeg is LGPL-2.1-or-later in the configuration documented by the build scripts. Its exact source is
  downloaded and hash-verified during the build. Distributors of a statically linked player must also
  satisfy LGPL relinking requirements.
- OpenMAX IL headers retain the Khronos notices contained in those files.
- DejaVu-derived font data retains `android_auto/hook/vendor/LICENSE-DejaVu.txt`.
- Files inherited from `luka-dev/mib2q-carplay-rgi`, including the shared renderer and portions of the
  Java/HMI integration, retain LuKa's copyright and upstream history. LuKa gave written permission on
  2026-09-30 to publish and distribute this work as a fork and stated that GPL-3.0 would be added to the
  upstream repository. See `UPSTREAM-PERMISSION.md`.

Source and binary distributions must preserve the applicable copyright and license notices. A binary
containing statically linked FFmpeg must also meet the LGPL relinking/source requirements described in
`android_auto/THIRD_PARTY.md`.
