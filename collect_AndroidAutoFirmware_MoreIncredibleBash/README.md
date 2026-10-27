# Read-only MHI2Q firmware collector

This M.I.B. package collects the exact files needed to add or verify an Audi MHI2Q Android Auto
firmware profile. It **does not install anything and does not change the head unit**. It reads five
files from MMX, writes them to the SD card, and byte-compares every saved copy with its source.

## Files collected

- `/mnt/app/eso/bin/apps/gal`
- `/mnt/app/eso/lib/libautoreceiver.so`
- `/ifs/lsd.jxe`
- `/mnt/system/etc/eso/production/smartphone_integrator.json`
- `/mnt/system/etc/eso/production/gal.json`

## Use

1. Copy the entire `collect_AndroidAutoFirmware_MoreIncredibleBash` folder to the root of a writable
   M.I.B. SD card.
2. Insert the card in the car.
3. Open **GEM → M.I.B. → Advanced settings → Run custom script** and select this package if prompted.
4. Wait for `COMPLETE: all 5 required files copied and verified` before removing the card.
5. On the card, send back the newest numbered folder under
   `android_auto_firmware_backup/`—for example `android_auto_firmware_backup/001`.

No Android Auto connection, route, reboot, or installation marker is needed. Each run creates a new
numbered folder and leaves prior collections untouched. A successful folder contains `status.txt`
starting with `COMPLETE`.

`lsd.jxe` is approximately 60 MB, so keep at least 100 MB free on the card. The collected firmware
binaries should be shared privately with the developer and should not be committed to a public Git
repository.
