#!/bin/sh
# Install our fixed startncm.sh over the stock one, persistently.
# /etc/scripts is a ramdisk copy of /mnt/system/etc/scripts (startup.sh
# move_etc_to_ramdisk), so the source on flash is the persistent target.
# Same partition/model as install_persistent_pf.sh. Idempotent, .stock backup.

DST=/mnt/system/etc/scripts/startncm.sh
SRC=/mnt/app/root/hooks/startncm.sh   # deploy our copy here first

mount -uw /mnt/system 2>/dev/null
[ -r "$SRC" ] || { echo "ERROR: $SRC not deployed"; exit 1; }

# back up the stock once
[ -f "$DST.stock" ] || cp "$DST" "$DST.stock" 2>/dev/null

# refuse if the stock we're replacing doesn't look like the known one
if ! grep -q 'protocol=ncm' "$DST" 2>/dev/null; then
    echo "ERROR: $DST does not look like the stock startncm.sh - not replacing"; exit 1
fi

cp "$SRC" "$DST" && chmod 755 "$DST" && echo "installed fixed startncm.sh (backup at $DST.stock)"
# note: takes effect on the next NCM bringup (next OTG device-mode start), no reboot needed
