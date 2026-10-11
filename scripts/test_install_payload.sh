#!/bin/sh
# The M.I.B. installer end to end in a sandbox: /mnt is redirected to a temp dir
# and mount is a no-op. Checks the flat release layout (assets dropped straight into
# mod/carplay/), the root/ tree layout, file modes, the SI child and dio_manager.json
# edits with their .carplay-stock backups, and uninstall back to stock.
# Runs under every POSIX/ksh shell available here (QNX 6.5 /bin/sh is pdksh).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
fail() { echo "FAIL ($1): $2"; exit 1; }

mode() { case $(uname -s) in Darwin) stat -f %Lp "$1" ;; *) stat -c %a "$1" ;; esac; }

stock_si='{
    "children": {
        "carplay": {
            "exec": "dio_manager",
            "path": "/mnt/app/eso/bin/apps"
        },
        "other": {
            "exec": "o"
        }
    }
}'
stock_dio='        ## 0x5000
        "MessagesSentByAccessory":["0x5000", "0x4C05"],
        ## 0x5001
        "MessagesReceivedFromDevice":["0x5001", "0x4C04"],'

ASSETS="libcarplay_hook.so maneuver_render flag_atlas.rgba carplay_startup.sh carplay_monitor.sh carplay_processes.sh carplay_cleanup.sh carplay_hook.jar"

run() {   # $1 shell, $2 layout (flat|tree), $3 action
    S=$T/$2; mkdir -p "$S/mod/carplay" "$S/mnt/system/etc/eso/production"
    sed -e "s#/mnt/#$S/mnt/#g" -e "s#\"/\\\${f\#./}\"#\"$S/\\\${f\#./}\"#" \
        -e 's#^\( *\)mount -uw#\1: mount -uw#' \
        "$ROOT/install_MoreIncredibleBash/mod/custom.sh" > "$S/mod/custom.sh"
    "$1" "$S/mod/custom.sh" "$3" > "$S/out.log" 2>&1 || { cat "$S/out.log"; fail "$1 $2" "$3 returned non-zero"; }
}

for sh in /bin/ksh /bin/mksh /bin/dash /bin/sh; do
    [ -x "$sh" ] || continue
    for layout in flat tree; do
        rm -rf "$T/$layout"; S=$T/$layout
        mkdir -p "$S/mod/carplay" "$S/mnt/system/etc/eso/production"
        printf '%s\n' "$stock_si" > "$S/mnt/system/etc/eso/production/smartphone_integrator.json"
        printf '%s\n' "$stock_dio" > "$S/mnt/system/etc/eso/production/dio_manager.json"
        cp "$ROOT/deploy/smartphone_integrator/carplay_child.json" "$S/mod/carplay/"
        for a in $ASSETS; do
            case $a in carplay_hook.jar) d=mnt/app/eso/hmi/lsd/jars ;; *) d=mnt/app/root/hooks ;; esac
            if [ "$layout" = flat ]; then echo "$a" > "$S/mod/carplay/$a"
            else mkdir -p "$S/mod/carplay/root/$d"; echo "$a" > "$S/mod/carplay/root/$d/$a"; fi
        done
        echo junk > "$S/mod/carplay/README.txt"   # unknown flat files are ignored
        J=$S/mnt/app/eso/hmi/lsd/jars; mkdir -p "$J"; echo nav > "$J/NavActiveIgnore.jar"   # M.I.B. conflict jar

        run "$sh" "$layout" install
        H=$S/mnt/app/root/hooks
        for a in libcarplay_hook.so maneuver_render carplay_startup.sh carplay_monitor.sh carplay_processes.sh carplay_cleanup.sh; do
            [ "$(cat "$H/$a")" = "$a" ] || fail "$sh $layout" "$a not installed"
            [ "$(mode "$H/$a")" = 755 ] || fail "$sh $layout" "$a mode $(mode "$H/$a")"
        done
        [ "$(mode "$H/flag_atlas.rgba")" = 644 ] || fail "$sh $layout" "atlas mode"
        [ "$(mode "$S/mnt/app/eso/hmi/lsd/jars/carplay_hook.jar")" = 644 ] || fail "$sh $layout" "jar not installed/mode"
        [ ! -e "$H/README.txt" ] || fail "$sh $layout" "unknown flat file installed"
        [ ! -e "$J/NavActiveIgnore.jar" ] || fail "$sh $layout" "NavActiveIgnore.jar not removed"
        P=$S/mnt/system/etc/eso/production
        grep -q carplay_startup.sh "$P/smartphone_integrator.json" || fail "$sh $layout" "SI child not replaced"
        grep -q '"exec": "o"' "$P/smartphone_integrator.json" || fail "$sh $layout" "other SI child lost"
        [ "$(grep -o '"0x520[0-4]"' "$P/dio_manager.json" | wc -l | tr -d ' ')" = 5 ] || fail "$sh $layout" "dio IDs"
        [ -e "$P/smartphone_integrator.json.carplay-stock" ] && [ -e "$P/dio_manager.json.carplay-stock" ] \
            || fail "$sh $layout" "stock backups missing"
        ls "$S/mnt/app/root/hooks" | grep -q carplay-new && fail "$sh $layout" "temporary file left"

        run "$sh" "$layout" install   # re-run: backups must stay stock
        grep -q '"exec": "dio_manager"' "$P/smartphone_integrator.json.carplay-stock" || fail "$sh $layout" "backup overwritten"

        run "$sh" "$layout" uninstall
        for a in $ASSETS; do
            [ ! -e "$H/$a" ] && [ ! -e "$S/mnt/app/eso/hmi/lsd/jars/$a" ] || fail "$sh $layout" "$a not removed"
        done
        [ "$(cat "$P/smartphone_integrator.json")" = "$stock_si" ] || fail "$sh $layout" "SI json not restored"
        [ "$(cat "$P/dio_manager.json")" = "$stock_dio" ] || fail "$sh $layout" "dio_manager.json not restored"
    done
    shells="${shells:-} $sh"
done
echo "M.I.B. installer flat + tree install/uninstall, NavActiveIgnore removal:$shells PASS"
