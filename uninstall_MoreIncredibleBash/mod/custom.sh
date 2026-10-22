#!/bin/sh
# M.I.B. -> Advanced Settings -> Run Custom Script.  QNX 6.5 /bin/sh is ksh.
# Self-contained CarPlay uninstaller: needs no payload tree.
#   - restore every *.carplay-stock the installer left (firmware binaries,
#     smartphone_integrator.json, all pf profiles, dio_manager.json, lang_data.zip) by renaming
#     the backup back;
#   - remove the files we added and own (hooks/.so/jar), which have no backup.
# Upgrade-safe and idempotent: re-running is a no-op once everything is restored.
# Copyright (c) 2026 LuKa (@LuKa_dev)
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin
export PATH
unset LD_PRELOAD

case $0 in */*) D=${0%/*} ;; *) D=. ;; esac
D=$(cd "$D" && pwd) || exit 1

# GEM runs on MMX. Forward a manual RCC launch (false once on MMX -> no loop).
if [ ! -d /mnt/app/eso/hmi/lsd ] && [ -d /net/mmx/mnt/app/eso/hmi/lsd ]; then
    exec on -f mmx /bin/sh "$D/custom.sh" "$@"
fi

# Files we OWN (added at install; no stock backup exists for these).
OWNED="/mnt/app/root/hooks/libcarplay_hook.so
/mnt/app/root/hooks/libcarplay_apac.so
/mnt/app/root/hooks/hssr_model.bin
/mnt/app/root/hooks/hssr_guard.bin
/mnt/app/root/hooks/hssr_mask.bin
/mnt/app/root/hooks/scene_masknet_c.bin
/mnt/app/root/hooks/maneuver_render
/mnt/app/root/hooks/altscreen_render
/mnt/app/root/hooks/flag_atlas.rgba
/mnt/app/root/hooks/carplay_startup.sh
/mnt/app/root/hooks/carplay_monitor.sh
/mnt/app/root/hooks/carplay_processes.sh
/mnt/app/root/hooks/carplay_cleanup.sh
/mnt/app/root/hooks/carplay_ncm_cleanup.sh
/mnt/app/eso/hmi/lsd/jars/carplay_hook.jar
/mnt/app/eso/hmi/lsd/jars/carplay_hook_cn.jar
/mnt/app/carplay_verbose
/mnt/app/carplay_layout.cfg
/tmp/carplay_verbose"

echo "custom.sh: CarPlay uninstall"
echo "Remounting app and system read-write..."
mount -uw /mnt/app    2>/dev/null || true
mount -uw /mnt/system 2>/dev/null || true

# 1. restore stock: every *.carplay-stock -> original (firmware, SI json, pf).
LIST=/tmp/carplay_stock.$$
find /mnt/app /mnt/system -name '*.carplay-stock' > "$LIST" 2>/dev/null || : > "$LIST"
while IFS= read -r bak; do
    [ -n "$bak" ] || continue
    orig=${bak%.carplay-stock}
    mv -f "$bak" "$orig" && echo "  restored $orig" || echo "  WARN restore $orig"
done < "$LIST"
rm -f "$LIST"

# 2. remove the files we own (no stock backup for these).
for f in $OWNED; do
    [ -e "$f" ] && rm -f "$f" && echo "  removed $f"
done
# renderers' GL program-binary cache (common/gl_program_cache.h)
rm -rf /mnt/persist/var/app/luka_carplay_altscreen /mnt/persist/var/app/luka_carplay_maneuver

sync
echo "Making sure it is saved (20 s)..."
sleep "${CARPLAY_SAVE_WAIT:-20}"   # host tests set 0
sync
echo "DONE (uninstall). Reboot the HU."
