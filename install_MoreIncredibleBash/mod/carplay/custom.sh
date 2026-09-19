#!/bin/sh
# M.I.B. -> Advanced Settings -> Run Custom Script. QNX 6.5 POSIX sh.
# Dead-simple installer: lay the ./root rootfs tree onto "/". No staging, no
# CRC, no pf/SI patching - every patched file already sits at its real path in
# ./root. Upgrade-safe: overwrites any previous version; a stock backup is kept
# once (first install only) so uninstall restores the original.
# Copyright (c) 2026 LuKa (@LuKa_dev)
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin${PATH:+:$PATH}
export PATH
unset LD_PRELOAD

case $0 in */*) D=${0%/*} ;; *) D=. ;; esac
D=$(cd "$D" && pwd) || exit 1

# GEM runs on MMX. Forward a manual RCC launch without touching RCC partitions.
if [ -z "${ON_MMX:-}" ] && [ ! -d /mnt/app/eso/hmi/lsd ] && [ -d /net/mmx/mnt/app/eso/hmi/lsd ]; then
    exec on -f mmx env ON_MMX=1 /bin/sh "$D/custom.sh" "$@"
fi

ROOT=$D/root
ACTION=${1:-install}
[ -d "$ROOT" ] || { echo "no payload tree at $ROOT"; exit 1; }

mode_for() { case $1 in *.so|*.so.*|*/maneuver_render|*/altscreen_render|*.sh) echo 755 ;; *) echo 644 ;; esac; }

echo "Remounting app and system read-write..."
mount -uw /mnt/app    2>/dev/null || true
mount -uw /mnt/system 2>/dev/null || true

cd "$ROOT" || exit 1
case $ACTION in
install)
    find . -type f ! -name .DS_Store | while IFS= read -r f; do
        dest=/${f#./}
        mkdir -p "${dest%/*}" || { echo "FAILED mkdir ${dest%/*}"; exit 1; }
        # keep the true stock once: only on first install, only if not already ours
        if [ -e "$dest" ] && [ ! -e "$dest.carplay-stock" ]; then
            cp -p "$dest" "$dest.carplay-stock" || { echo "FAILED backup $dest"; exit 1; }
        fi
        cp -p "$f" "$dest" || { echo "FAILED copy $dest"; exit 1; }
        chmod "$(mode_for "$dest")" "$dest"
        echo "  $dest"
    done
    # retired files from older versions
    rm -f /mnt/app/root/hooks/cursor_render /mnt/app/root/hooks/cursor_render.so
    sync
    echo "DONE (install). Reboot the HU to load."
    ;;
uninstall)
    find . -type f ! -name .DS_Store | while IFS= read -r f; do
        dest=/${f#./}
        if [ -e "$dest.carplay-stock" ]; then mv -f "$dest.carplay-stock" "$dest"
        else rm -f "$dest"; fi
    done
    sync
    echo "DONE (uninstall). Reboot the HU."
    ;;
*) echo "usage: custom.sh [install|uninstall]"; exit 2 ;;
esac
