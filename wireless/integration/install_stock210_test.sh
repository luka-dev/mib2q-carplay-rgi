#!/bin/sh
# Stock210 test files only. No service control, PF reload, radio query or autostart.
set -eu
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/usr/bin:${PATH:-}
export PATH
umask 077
ACTION=${1:-}
STAGE=${MHI2Q_STAGE:-}
TARGET_ROOT=${MHI2Q_TARGET_ROOT:-}
case $ACTION in preflight|install|rollback) ;; *) echo 'usage: install_stock210_test.sh preflight|install|rollback' >&2; exit 2;; esac
[ -n "$TARGET_ROOT" ] || [ "${MHI2Q_ENABLE_HU_INSTALL:-0}" = 1 ] || {
    echo 'live filesystem requires MHI2Q_ENABLE_HU_INSTALL=1' >&2; exit 2;
}
[ -n "$STAGE" ] && [ -f "$STAGE/checksums.txt" ] || exit 2
fail() { echo "stock210 test: $*" >&2; return 1; }
dest() { printf '%s%s' "$TARGET_ROOT" "$1"; }

replaced() {
    while read -r payload path reference mode; do
        "$1" "$payload" "$path" "$reference" "$mode" || return 1
    done <<'MAP'
bin/btstack /mnt/app/eso/bin/apps/btstack reference/btstack 755
pf/pf.conf /mnt/system/etc/pf.conf reference/pf.conf 644
pf/pf.ecm0.conf /mnt/system/etc/pf.ecm0.conf reference/pf.ecm0.conf 644
pf/pf.mlan0.conf /mnt/system/etc/pf.mlan0.conf reference/pf.mlan0.conf 644
pf/pf.ppp0.conf /mnt/system/etc/pf.ppp0.conf reference/pf.ppp0.conf 644
integration/carplay_startup.sh /mnt/app/root/hooks/carplay_startup.sh reference/carplay_startup.sh 755
MAP
}
owned() {
    while read -r payload path mode; do
        "$1" "$payload" "$path" "$mode" || return 1
    done <<'MAP'
bin/mhi2q-wireless-daemon /mnt/app/root/hooks/mhi2q-wireless-daemon 755
bin/dio_owner_exec /mnt/app/root/hooks/dio_owner_exec 755
bin/wireless_dio_supervisor_bin /mnt/app/root/hooks/wireless_dio_supervisor_bin 755
bin/wireless_runtime /mnt/app/root/hooks/wireless_runtime 755
bin/mhi2q_mdns_probe /mnt/app/root/hooks/mhi2q_mdns_probe 755
lib/libdio_receiver_bridge.so /mnt/app/root/hooks/libdio_receiver_bridge.so 755
lib/libcarplay_wireless_stock210.so /mnt/app/root/hooks/libcarplay_wireless_stock210.so 755
lib/libcarplay_wireless_apac.so /mnt/app/root/hooks/libcarplay_wireless_apac.so 755
integration/wireless_dio_supervisor.sh /mnt/app/root/hooks/wireless_dio_supervisor.sh 755
integration/wireless_carplay.env /mnt/app/root/hooks/wireless_carplay.env 600
MAP
}
verify_bundle() {
    while read -r expected relative; do
        case $relative in ''|/*|..|../*|*/../*|*/..) fail 'unsafe checksum path'; return 1;; esac
        [ -f "$STAGE/$relative" ] && [ ! -L "$STAGE/$relative" ] || return 1
        digest=$(openssl dgst -sha256 "$STAGE/$relative") || return 1
        [ "${digest##* }" = "$expected" ] || { fail "checksum mismatch: $relative"; return 1; }
    done < "$STAGE/checksums.txt"
}
check_original() {
    path=$(dest "$2")
    [ -f "$path" ] && [ ! -L "$path" ] && [ ! -e "$path.stock210-prev" ] && [ ! -L "$path.stock210-prev" ] || {
        fail "missing source or existing backup: $2"; return 1;
    }
    cmp -s "$path" "$STAGE/$3" || { fail "unexpected current bytes: $2"; return 1; }
}
check_owned() {
    path=$(dest "$2")
    [ ! -e "$path" ] && [ ! -L "$path" ] || { fail "owned path exists: $2"; return 1; }
    [ -d "${path%/*}" ] && [ -f "$STAGE/$1" ] || return 1
}
check_untouched() {
    # Hash-only guards: never replace stock receiver or existing wired hooks.
    while read -r expected path; do
        file=$(dest "$path")
        [ -f "$file" ] && [ ! -L "$file" ] || { fail "missing dependency: $path"; return 1; }
        digest=$(openssl dgst -sha256 "$file") || return 1
        [ "${digest##* }" = "$expected" ] || { fail "dependency differs: $path"; return 1; }
    done < "$STAGE/untouched.txt"
}
check_config() {
    (
        set -a
        . "$STAGE/integration/wireless_carplay.env"
        [ "${MHI2Q_CP_RECEIVER:-}" = stock210 ] &&
        [ "${MHI2Q_CP_SOURCE_VERSION:-}" = 210.81 ] &&
        [ "${MHI2Q_CP_AIRPLAY_PORT:-}" = 5000 ] &&
        [ "${MHI2Q_CP_IE_MONITOR_SECONDS:-}" = 0 ] &&
        [ "${MHI2Q_CP_WIRELESS_PAIRING:-}" = 1 ] &&
        [ "${MHI2Q_CP_WIRELESS_MEDIA:-}" = 1 ] &&
        [ "${MHI2Q_REQUIRE_BRIDGE_QUIESCE:-}" = 1 ] &&
        [ "${MHI2Q_CP_WIRELESS_HOOK:-}" = /mnt/app/root/hooks/libcarplay_wireless_stock210.so ] &&
        [ "${MHI2Q_CP_WIRELESS_APAC:-}" = /mnt/app/root/hooks/libcarplay_wireless_apac.so ] &&
        [ "${MHI2Q_CP_PAIRING_DIR:-}" = /mnt/persist/mhi2q-carplay/pairing ] &&
        [ "${OWNER_FILE:-}" = /mnt/persist/.mhi2q_dio_generation ] || exit 1
        "$STAGE/bin/mhi2q-wireless-daemon" --check-config
    ) || { fail 'invalid or incomplete first-session configuration'; return 1; }
}
check_pending() {
    [ ! -e "$(dest /tmp/mhi2q_wifi_control.pending)" ] || { fail 'radio control pending; verify HU recovery'; return 1; }
}
backup_one() {
    path=$(dest "$2")
    cp -p "$path" "$path.stock210-prev" && cmp -s "$path.stock210-prev" "$STAGE/$3"
}
drop_backup() { rm -f "$(dest "$2").stock210-prev"; }
write_one() {
    path=$(dest "$2")
    mode=$3
    [ "$#" = 3 ] || mode=$4
    if [ -n "$TARGET_ROOT" ] && [ "${MHI2Q_TEST_FAIL_PATH:-}" = "$2" ]; then return 1; fi
    # QNX4 component names are limited to 48 bytes; do not append to .so names.
    temporary=${path%/*}/.mhi2q-stock210-new.$$
    [ ! -e "$temporary" ] && [ ! -L "$temporary" ] || return 1
    if cp "$STAGE/$1" "$temporary" && chmod "$mode" "$temporary" &&
       cmp -s "$temporary" "$STAGE/$1" && mv -f "$temporary" "$path" &&
       cmp -s "$path" "$STAGE/$1"; then return 0; fi
    rm -f "$temporary"
    return 1
}
check_restore() {
    path=$(dest "$2")
    [ ! -L "$path" ] && [ ! -L "$path.stock210-prev" ] || { fail "symlink in rollback path: $2"; return 1; }
    if [ -f "$path.stock210-prev" ]; then
        cmp -s "$path.stock210-prev" "$STAGE/$3" || { fail "backup differs: $2"; return 1; }
    else
        cmp -s "$path" "$STAGE/$3" || { fail "missing backup: $2"; return 1; }
    fi
    cmp -s "$path" "$STAGE/$1" || cmp -s "$path" "$STAGE/$3" || {
        fail "changed since installation: $2"; return 1;
    }
}
check_remove() {
    path=$(dest "$2")
    [ ! -e "$path" ] && [ ! -L "$path" ] && return 0
    cmp -s "$path" "$STAGE/$1" && [ ! -L "$path" ] || { fail "changed owned file: $2"; return 1; }
}
restore_one() {
    path=$(dest "$2")
    [ ! -f "$path.stock210-prev" ] || mv -f "$path.stock210-prev" "$path"
}
remove_one() { rm -f "$(dest "$2")"; }
restore_all() {
    # Validate every file before restoring any: preserve concurrent manual edits.
    replaced check_restore && owned check_remove && replaced restore_one && owned remove_one
}
prepare_writable() {
    [ -n "$TARGET_ROOT" ] || { mount -uw /mnt/app && mount -uw /mnt/system; }
}
LOCK_HELD=0
INSTALLING=0
finish() {
    code=$1
    trap - 0
    if [ "$INSTALLING" = 1 ]; then
        restore_all || echo 'stock210 test: rollback incomplete; retain bundle and backups' >&2
    fi
    [ "$LOCK_HELD" = 0 ] || rmdir "$(dest /mnt/persist/.mhi2q_stock210_install.lock)"
    exit "$code"
}
trap 'finish $?' 0
trap 'exit 1' 1 2 15
verify_bundle
if [ "$ACTION" != rollback ]; then
    read -r baseline_status < "$STAGE/baseline-status.txt"
    [ "$baseline_status" = verified ] || { fail 'HU baseline not captured; rebuild with --baseline'; exit 1; }
    check_pending
    check_untouched
    replaced check_original
    owned check_owned
    check_config
    if [ "$ACTION" = preflight ]; then
        echo 'stock210 test: input/config checks passed; no target files changed'
        exit 0
    fi
fi
# Serialize mutations on the persistent filesystem. Preflight remains read-only.
mkdir "$(dest /mnt/persist/.mhi2q_stock210_install.lock)" || { fail 'cannot acquire installer lock'; exit 1; }
LOCK_HELD=1
if [ "$ACTION" = rollback ]; then
    replaced check_restore
    owned check_remove
    prepare_writable
    restore_all
    echo 'stock210 test: original files restored; running processes were not changed'
    exit 0
fi
check_pending
check_untouched
replaced check_original
owned check_owned
MHI2Q_DIO_OWNER_LOCK=$(dest /mnt/persist/.mhi2q_dio_owner.lock)
MHI2Q_DIO_WIRED_PRIORITY_LOCK=$(dest /mnt/persist/.mhi2q_dio_wired_priority.lock)
export MHI2Q_DIO_OWNER_LOCK MHI2Q_DIO_WIRED_PRIORITY_LOCK
"$STAGE/bin/dio_owner_exec" --check-locks
prepare_writable
if ! replaced backup_one; then
    replaced drop_backup
    fail 'backup failed; no payload applied'; exit 1
fi
INSTALLING=1
if ! owned write_one || ! replaced write_one; then
    fail 'installation failed; attempting automatic rollback'; exit 1
fi
INSTALLING=0
echo 'stock210 test: files verified and staged; activate manually after a clean Bluetooth restart'
