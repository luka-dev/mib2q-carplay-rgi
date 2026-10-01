# Build and installation

## Private inputs

Obtain these from a unit or firmware package you are entitled to use and keep them outside the repository:

- `gal`
- `libautoreceiver.so`
- `lsd.jxe`
- a converted `lsd.jar` for fidelity/API checks
- a compile form of the same JAR with `InnerClasses` restored, when the converter produces one

Run `scripts/check_firmware.py` before building. An exact match is required for the native hook. A JAR
that merely contains the expected classes is useful for porting, but it is not install authorization.

## Build sequence

```bash
./scripts/build_android_auto_native.sh
./scripts/build_renderers.sh
./scripts/build_android_auto_java.sh --stock-jar /private/lsd.jar --compile-jar /private/lsd_ic.jar
python scripts/stage_android_auto.py
```

The Java build compiles target 1.4 bytecode for the unit's IBM J9 runtime. It verifies every replaced
class header and public/protected member against the supplied stock JAR. Any mismatch fails the build.

## SD card

Copy the staged `install_AndroidAuto_MoreIncredibleBash` directory into the M.I.B. SD root.

1. With no marker present, run M.I.B. → Advanced settings → Run custom script. Review the audit.
2. Create empty `mib2q_aa_install` in the SD root and run the script again.
3. Remove the marker, disconnect Android Auto and reboot the MMI.
4. Connect Android Auto and start a navigation route. The native Audi map remains when no phone route
   is active; the Android Auto cluster map occupies the navigation area during guidance.

## Off and diagnostic markers

| Marker in SD root | Effect |
| --- | --- |
| `mib2q_aa_off` | Run stock GAL for that boot |
| `sq5_omx_off` | Force FFmpeg software decoding |
| `sq5_cluster_aap17` | Diagnostic fallback to the older protocol request |
| `sq5_cluster_relayout_off` | Disable live Large/Classic/Sport updates |
| `sq5_cluster_noui` | Do not send the Android Auto UiConfig extension |
| `sq5_menu_off` | Disable the cockpit options menu |

## Uninstall

Remove the install marker, add empty `mib2q_aa_uninstall`, and run the same script. It restores the
saved configuration and removes its own files. Remove the marker and reboot.
