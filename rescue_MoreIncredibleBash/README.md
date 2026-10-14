# Rescue card: HMI stuck on the boot logo

For a unit whose HMI no longer boots after a CarPlay install: the splash screen keeps coming back and GEM and M.I.B. are unreachable. This card removes the CarPlay jar without opening the unit, and puts the stock `usblauncher_otg.lua` back if the installer patched it. It was confirmed on the car on 2026-09-26 (issue #24).

## Prepare the SD card

1. Put the **M.I.B. 3.7.1** build ([release](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/releases/tag/V3.7.1)) on a FAT32 SD card as usual.
2. Copy this folder's files over it, replacing the existing ones:
   - `Swdlautorun.txt` goes in the card root. It enables SWDL autorun.
   - `Launcher/stuff.sh` replaces M.I.B.'s. It is M.I.B.'s own script with two blocks added at the start: one deletes `carplay_hook.jar` and `carplay_hook_cn.jar` from `/mnt/app/eso/hmi/lsd/jars`, the other copies `/mnt/system/etc/usblauncher_otg.lua.carplay-stock` over the patched `usblauncher_otg.lua` (only when that backup exists; the backup stays for the uninstaller).

## Use

1. Battery off.
2. Insert the SD card.
3. Battery on.
4. Ignition on, or start the engine.
5. Leave it for a while. On the confirmed car the screen went splash → black → splash → black, and then a hard reset with the button combination was needed.
6. The red SWDL menu appears on the screen. Press **Continue**, then **Cancel**, and remove the SD card.
7. The HMI boots normally.

Afterwards, install CarPlay again with the current installer (it patches the Lua again from its kept backup). It reads the firmware version from E2P and installs the matching jar, or none.
