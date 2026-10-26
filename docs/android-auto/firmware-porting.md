# Porting another firmware

The repository uses a shared implementation with profile-selected Java overrides and exact native
fingerprints. MU0918 is installable. MU1316 P5145 already has its four differing Java classes ported,
but its native gate remains intentionally incomplete.

## 1. Collect without modifying

Collect `gal`, `libautoreceiver.so`, `lsd.jxe`, `smartphone_integrator.json` and `gal.json` from the
target firmware. Convert the owner's `lsd.jxe` locally. Run the compatibility checker and retain its
JSON output with `--json`.

## 2. Native ABI audit

Compare the target GAL/receiver implementation with `android_auto/hook/vendor/probe.c`. Resolve every
interposed symbol, vtable size, field offset and callback signature. Do not create a profile from file
size alone. First build a pass-through/logging probe, then confirm primary Android Auto behavior remains
unchanged before advertising the cluster sink.

## 3. Java seams

First compare all 13 replaced classes with the nearest profile. Put only differing implementations
under `android_auto/java/firmware/<profile-id>/src` at their normal package paths. The MU1316 P5145
profile needs four overrides: `ClusterService`, `DisplayManagerMIB2High`, `ExternalEventsListener` and
`PartialPopupManagerEvoHigh`.

Regenerate these Android Auto seams from the user's firmware only when they differ:

- `AndroidAuto2ListenerDistributor`
- `AndroidAuto2NavHandler`
- `AppConnectorTerminalMode`

Review the generated changes. The Java build then checks all replaced class APIs against the supplied
stock JAR. A passing member check does not prove method-body compatibility; inspect every reconstructed
Audi class whose implementation is shipped by the overlay.

## 4. Display integration

Confirm the target's Virtual Cockpit terminal, window IDs, display contexts and B9/B9Sport view signals.
Start with the player disabled, then an owned test window, then decoded video. Never move the stock
Android Auto centre-screen window to the cockpit display.

## 5. Add a profile

Add a JSON profile as soon as the firmware is identified, with `installable: false` while work remains.
After native ABI review and on-car validation, add full SHA-256 hashes, file sizes and POSIX cksums for
`gal`, `libautoreceiver.so` and `lsd.jxe`, then mark it installable. Test it with automatic detection and
an explicit `--profile`. Never promote a profile from file size alone.

The staged card contains a generated `profiles.list` and one JAR per supplied profile. The M.I.B.
script requires an exact three-file native match before choosing a JAR. It can identify a pending
profile for diagnostics, but cannot install it.
