# On-unit changes

The installer is deliberately narrow and prints this plan during every audit.

## Added

- `/mnt/app/root/sq5_android_auto/`
  - `libsq5_cluster_live.so`
  - `cluster-player`
  - `maneuver_render`
  - `flag_atlas.rgba`
  - `gal_startup.sh`
  - renderer monitor/process/cleanup scripts
  - the installer backup and generated installed configuration
- `/mnt/app/eso/hmi/lsd/jars/mib2q_android_auto_cluster.jar`

## Edited

- `/mnt/system/etc/eso/production/smartphone_integrator.json`
  - `children.gal.exec` becomes `gal_startup.sh`
  - `children.gal.path` becomes `/mnt/app/root/sq5_android_auto`

The parser preserves the remainder of the file byte for byte and refuses duplicate keys, unsafe path
characters, unsupported structure or oversized input.

## Never replaced

- `/mnt/app/eso/bin/apps/gal`
- `/mnt/app/eso/lib/libautoreceiver.so`
- `/ifs/lsd.jxe`
- `gal.json`
- LTE, amplifier, navigation database or component-protection data

## Rollback

The original smartphone-integrator configuration is saved once. Uninstall proceeds only when the live
file still matches either the installer's generated copy or its original backup. If another tool edited
the file afterwards, uninstall refuses instead of destroying that work.
