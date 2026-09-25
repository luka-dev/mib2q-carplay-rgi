#!/bin/sh

# Generation-aware replacement for the stock MU1316 startncm.sh.
# Optional arguments 4..6 are supplied by the matching usblauncher_otg.lua
# change and identify the USB enumeration that owns carplay0.

STACK=${1:-}
INTERFACE=${2:-}
IOPKT_INSTANCE=${3:-}
USB_BUSNO=${4:-}
USB_DEVNO=${5:-}
USB_IFACE=${6:-}

SOCKET=/cplay/dev/socket
IF_NAME="${INTERFACE}0"

NCM_GENERATION_DIR=${NCM_GENERATION_DIR:-/tmp}
# The lock is a directory: /tmp is /dev/shmem on the HU and mkdir there fails
# (ENOSYS).  /ramdisk is a RAM qnx4 disk, cleared at boot.  The unit has no
# rmdir binary, so the lock is removed with rm -rf.
NCM_LOCK_BASE=${NCM_LOCK_BASE:-/ramdisk}
NCM_LOG_FILE=${NCM_LOG_FILE:-/tmp/carplay_hook.log}
NCM_MOUNT_BIN=${NCM_MOUNT_BIN:-mount}
NCM_IF_UP_BIN=${NCM_IF_UP_BIN:-if_up}
NCM_IFCONFIG_BIN=${NCM_IFCONFIG_BIN:-ifconfig}
NCM_WAITFOR_BIN=${NCM_WAITFOR_BIN:-waitfor}
LOCK_HELD=0

log_ncm()
{
    echo "NCM_FENCE start $$ $*" >> "${NCM_LOG_FILE}"
}

release_lock()
{
    if [ "${LOCK_HELD}" = 1 ]; then
        rm -f "${LOCK_DIR}/pid"
        rm -rf "${LOCK_DIR}" >/dev/null 2>&1
        LOCK_HELD=0
    fi
}

acquire_lock()
{
    LOCK_TRIES=0
    while ! mkdir "${LOCK_DIR}" >/dev/null 2>&1; do
        LOCK_OWNER=
        if [ -r "${LOCK_DIR}/pid" ]; then
            read LOCK_OWNER < "${LOCK_DIR}/pid"
        fi

        case "${LOCK_OWNER}" in
            ''|*[!0-9]*)
                ;;
            *)
                if ! kill -0 "${LOCK_OWNER}" >/dev/null 2>&1; then
                    rm -f "${LOCK_DIR}/pid"
                    rm -rf "${LOCK_DIR}" >/dev/null 2>&1
                    log_ncm "reclaim_lock owner=${LOCK_OWNER} interface=${IF_NAME}"
                    # Bounded like a wait: a lock that cannot be removed must
                    # not spin forever.
                    LOCK_TRIES=$((LOCK_TRIES + 1))
                    [ "${LOCK_TRIES}" -lt 8 ] && continue
                    log_ncm "fail lock_reclaim owner=${LOCK_OWNER} interface=${IF_NAME}"
                    return 1
                fi
                ;;
        esac

        LOCK_TRIES=$((LOCK_TRIES + 1))
        if [ "${LOCK_TRIES}" -ge 8 ]; then
            log_ncm "fail lock_timeout owner=${LOCK_OWNER} interface=${IF_NAME}"
            return 1
        fi
        log_ncm "wait_lock owner=${LOCK_OWNER} interface=${IF_NAME}"
        sleep 1
    done

    if ! echo "$$" > "${LOCK_DIR}/pid"; then
        rm -rf "${LOCK_DIR}" >/dev/null 2>&1
        log_ncm "fail lock_owner_write interface=${IF_NAME}"
        return 1
    fi
    LOCK_HELD=1
    return 0
}

read_generation()
{
    NCM_READ_GENERATION=__missing__
    if [ -r "${GENERATION_FILE}" ]; then
        read NCM_READ_GENERATION < "${GENERATION_FILE}"
    fi
}

cleanup_new_interface()
{
    "${NCM_IFCONFIG_BIN}" "${IF_NAME}" destroy >/dev/null 2>&1
    read_generation
    if [ "${NCM_READ_GENERATION}" = "${GENERATION}" ]; then
        rm -f "${GENERATION_FILE}"
    fi
}

if [ -z "${STACK}" ] || [ -z "${INTERFACE}" ] || [ -z "${IOPKT_INSTANCE}" ]; then
    log_ncm "reject missing required argument"
    exit 2
fi

case "${INTERFACE}" in
    *[!A-Za-z0-9_]*)
        log_ncm "reject invalid interface=${INTERFACE}"
        exit 2
        ;;
esac

GENERATION_FILE="${NCM_GENERATION_DIR}/ncm_generation_${IF_NAME}"
LOCK_DIR="${NCM_LOCK_BASE}/ncm_lock_${IF_NAME}"

if [ -n "${USB_BUSNO}" ] && [ -n "${USB_DEVNO}" ] && [ -n "${USB_IFACE}" ]; then
    GENERATION_KEY="${USB_BUSNO}:${USB_DEVNO}:${USB_IFACE}"
else
    # Compatibility only while the launcher patch is not installed.  The
    # strong stale-removal check requires busno/devno/iface.
    GENERATION_KEY=legacy
fi

GENERATION="${GENERATION_KEY}:$$"
GENERATION_TEMP="${GENERATION_FILE}.$$"

trap 'release_lock' 0
trap 'release_lock; exit 1' 1 2 3 15

# QNX 6.5 test(1) does not provide a reliable -e here.  ls works for the
# special device node and is also what the live diagnosis used.
if ! ls "${STACK}" >/dev/null 2>&1; then
    log_ncm "reject missing_stack=${STACK} generation=${GENERATION}"
    exit 1
fi

if ! acquire_lock; then
    exit 1
fi

# A repeated start for the same enumeration is already satisfied if its
# interface still exists.  Otherwise remove any previous global-name instance
# while holding the same lock used by stopncm.sh.  This prevents mount from
# colliding with an old carplay0 during Audi's one-second removal delay.
read_generation
case "${NCM_READ_GENERATION}" in
    "${GENERATION_KEY}":*)
        if "${NCM_IFCONFIG_BIN}" "${IF_NAME}" >/dev/null 2>&1; then
            log_ncm "skip duplicate_ready interface=${IF_NAME} generation=${NCM_READ_GENERATION}"
            exit 0
        fi
        ;;
esac

"${NCM_IFCONFIG_BIN}" "${IF_NAME}" destroy >/dev/null 2>&1
rm -f "${GENERATION_FILE}"

if [ "${IOPKT_INSTANCE}" = "io-pkt1" ]; then
    echo "Waiting for ${SOCKET}..."
    "${NCM_WAITFOR_BIN}" "${SOCKET}" 2
    export SOCK=/cplay
fi

echo "<STARTING NCM DRIVER INTERFACE=\"${INTERFACE}\" STACK=\"${STACK}\" IOPKT_INSTANCE=\"${IOPKT_INSTANCE}\" GENERATION=\"${GENERATION}\">"
log_ncm "begin interface=${IF_NAME} generation=${GENERATION} stack=${STACK}"

"${NCM_MOUNT_BIN}" -T "${IOPKT_INSTANCE}" \
    -o "verbose=0,path=${STACK},protocol=ncm,usbdnet_mac=000022446688,mac=020022446688,name=${INTERFACE},npkt=10" \
    /mnt/app/armle/lib/dll/devnp-usbdnet.so
MOUNT_RC=$?
if [ "${MOUNT_RC}" -ne 0 ]; then
    log_ncm "fail mount_rc=${MOUNT_RC} interface=${IF_NAME} generation=${GENERATION}"
    cleanup_new_interface
    exit "${MOUNT_RC}"
fi

# These are ASCII hyphens.  The stock file contains UTF-8 en dashes here.
if "${NCM_IF_UP_BIN}" -p "${IF_NAME}" -s 2 -r 1; then
    "${NCM_IFCONFIG_BIN}" "${IF_NAME}" up
    IFCONFIG_RC=$?
    if [ "${IFCONFIG_RC}" -eq 0 ]; then
        # Publish only a fully mounted/up interface.  A matching removal waits
        # on LOCK_DIR until this transaction is complete.
        if ! echo "${GENERATION}" > "${GENERATION_TEMP}"; then
            log_ncm "fail generation_write=${GENERATION_TEMP}"
            cleanup_new_interface
            exit 1
        fi
        if ! mv -f "${GENERATION_TEMP}" "${GENERATION_FILE}"; then
            rm -f "${GENERATION_TEMP}"
            log_ncm "fail generation_publish=${GENERATION_FILE}"
            cleanup_new_interface
            exit 1
        fi
        log_ncm "ready interface=${IF_NAME} generation=${GENERATION}"
        exit 0
    fi
    log_ncm "fail ifconfig_up_rc=${IFCONFIG_RC} interface=${IF_NAME} generation=${GENERATION}"
else
    echo "${IF_NAME} not found"
    log_ncm "fail interface_timeout=${IF_NAME} generation=${GENERATION}"
fi

cleanup_new_interface
exit 1
