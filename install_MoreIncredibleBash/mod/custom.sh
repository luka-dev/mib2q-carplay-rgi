#!/bin/sh
# M.I.B. -> Advanced Settings -> Run Custom Script.
# QNX 6.5 /bin/sh is ksh (pdksh) - this script stays inside its portable subset.
# Copy into /mod/ on the M.I.B. SD; resources live in /mod/carplay/root/.
#
# Dead-simple installer: lay the carplay/root rootfs tree onto "/". No staging,
# no CRC, no pf/SI patching - every patched file already sits at its real path
# under carplay/root/. Upgrade-safe: overwrites any previous version; a stock
# backup is kept once (first install only) so uninstall restores the original.
# Copyright (c) 2026 LuKa (@LuKa_dev)
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin
export PATH
unset LD_PRELOAD

case $0 in */*) D=${0%/*} ;; *) D=. ;; esac
D=$(cd "$D" && pwd) || exit 1

# GEM runs on MMX. Forward a manual RCC launch (the MMX-local dir check below is
# false once we are on MMX, so this never loops).
if [ ! -d /mnt/app/eso/hmi/lsd ] && [ -d /net/mmx/mnt/app/eso/hmi/lsd ]; then
    exec on -f mmx /bin/sh "$D/custom.sh" "$@"
fi

ROOT=$D/carplay/root
ACTION=${1:-install}
[ -d "$ROOT" ] || { echo "no payload tree at $ROOT"; exit 1; }

# Explicit mode per destination - never rely on the tree's bits (protect.py
# rewrites the .so via python and drops the exec bit; git/macOS may not preserve
# it either). Matches the old installer: hook files 755, data 644.
mode_for() {
    case $1 in
        *.so|*.so.*)                          echo 755 ;;
        */maneuver_render|*/altscreen_render) echo 755 ;;
        */eso/bin/apps/*)                     echo 755 ;;  # firmware exec (smartphone_integrator)
        *.sh)                                 echo 755 ;;
        *)                                    echo 644 ;;  # hssr_model.bin, flag_atlas.rgba, .jar
    esac
}

echo "custom.sh: CarPlay $ACTION (rootfs -> /)"
echo "Remounting app and system read-write..."
mount -uw /mnt/app    2>/dev/null || true
mount -uw /mnt/system 2>/dev/null || true

cd "$ROOT" || exit 1

# Build the file list to a temp, then read it with the while loop in the CURRENT
# shell (redirect, NOT a pipe). In ksh a `find | while` runs the loop body in a
# subshell, so an `exit` inside would abort only the subshell and the script
# would keep going - errors must abort, so we avoid the pipe entirely.
LIST=/tmp/carplay_files.$$
find . -type f > "$LIST" 2>/dev/null || { echo "FAILED listing payload"; rm -f "$LIST"; exit 1; }

case $ACTION in
install)
    while IFS= read -r f; do
        case $f in *.DS_Store) continue ;; esac
        dest=/${f#./}
        dir=${dest%/*}
        mkdir -p "$dir" || { echo "FAILED mkdir $dir"; rm -f "$LIST"; exit 1; }
        # keep the true stock once: first install only, never overwrite a backup
        if [ -e "$dest" ] && [ ! -e "$dest.carplay-stock" ]; then
            cp -p "$dest" "$dest.carplay-stock" || { echo "FAILED backup $dest"; rm -f "$LIST"; exit 1; }
        fi
        # atomic replace: stage a sibling on the SAME fs, then rename over $dest.
        # rename() is atomic, so a running smartphone_integrator / deva-ctrl-qc.so
        # / .so is never seen half-written, and a crash mid-copy leaves the old
        # file intact. A plain cp over $dest would not be atomic.
        tmp=$dest.carplay-new.$$
        cp -p "$f" "$tmp" || { echo "FAILED copy $tmp"; rm -f "$tmp" "$LIST"; exit 1; }
        chmod $(mode_for "$dest") "$tmp" || { echo "FAILED chmod $tmp"; rm -f "$tmp" "$LIST"; exit 1; }
        mv -f "$tmp" "$dest" || { echo "FAILED activate $dest"; rm -f "$tmp" "$LIST"; exit 1; }
        echo "  $dest"
    done < "$LIST"
    rm -f /mnt/app/root/hooks/cursor_render   # retired renderer from older versions
    rm -f "$LIST"
    sync
    echo "DONE (install). Reboot the HU to load."
    ;;
uninstall)
    while IFS= read -r f; do
        case $f in *.DS_Store) continue ;; esac
        dest=/${f#./}
        if [ -e "$dest.carplay-stock" ]; then
            mv -f "$dest.carplay-stock" "$dest" || echo "WARN restore $dest"
        else
            rm -f "$dest"
        fi
    done < "$LIST"
    rm -f "$LIST"
    sync
    echo "DONE (uninstall). Reboot the HU."
    ;;
*)
    rm -f "$LIST"
    echo "usage: custom.sh [install|uninstall]"; exit 2
    ;;
esac
