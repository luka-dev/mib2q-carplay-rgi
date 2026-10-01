# Porting another firmware

The MU0918 implementation is a reference port, not a universal binary patch.

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

Decompile these classes from the user's firmware and pass the source root with `--firmware-source`:

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

After on-car validation, add a JSON profile containing full SHA-256 hashes, file sizes and POSIX cksums
where the on-unit installer uses them. Test it with `scripts/check_firmware.py --profile your-profile.json`
and add a separate installer profile instead of weakening existing checks.
