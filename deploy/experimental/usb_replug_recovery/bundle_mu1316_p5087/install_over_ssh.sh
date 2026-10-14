#!/bin/sh

# Host-side installer. It stages and verifies files but never reboots the HU.

set -eu

BUNDLE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SSH_HELPER=${AUDI_SSH_HELPER:-${AUDI_WORKSPACE:-$HOME/Desktop/AUDI_2}/audi_ssh.sh}
MODE=${1:-}

usage()
{
    echo "usage: $0 ncm|si|all" >&2
    exit 2
}

case "${MODE}" in
    ncm)
        ARTIFACTS="startncm.sh stopncm.sh usblauncher_otg.lua"
        BASELINE_GUARDS="smartphone_integrator"
        ;;
    si)
        ARTIFACTS="smartphone_integrator"
        BASELINE_GUARDS="startncm.sh stopncm.sh usblauncher_otg.lua"
        ;;
    all)
        ARTIFACTS="startncm.sh stopncm.sh usblauncher_otg.lua smartphone_integrator"
        BASELINE_GUARDS=
        ;;
    *)
        usage
        ;;
esac

[ -x "${SSH_HELPER}" ] || {
    echo "REFUSE: SSH helper is not executable: ${SSH_HELPER}" >&2
    exit 1
}

"${BUNDLE_DIR}/verify_bundle.sh"

WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mhi2-usb-install.XXXXXX")
trap 'rm -rf "${WORK_DIR}"' 0 1 2 3 15

remote_path()
{
    case "$1" in
        startncm.sh) echo /mnt/system/etc/scripts/startncm.sh ;;
        stopncm.sh) echo /mnt/system/etc/scripts/stopncm.sh ;;
        usblauncher_otg.lua) echo /mnt/system/etc/usblauncher_otg.lua ;;
        smartphone_integrator) echo /mnt/app/eso/bin/apps/smartphone_integrator ;;
        *) return 1 ;;
    esac
}

state_of_file()
{
    ARTIFACT=$1
    FILE=$2
    if cmp -s "${BUNDLE_DIR}/stock/${ARTIFACT}" "${FILE}"; then
        echo stock
    elif cmp -s "${BUNDLE_DIR}/payload/${ARTIFACT}" "${FILE}"; then
        echo payload
    else
        echo unknown
    fi
}

echo "Read-only target and backup preflight"
for ARTIFACT in ${ARTIFACTS}; do
    REMOTE=$(remote_path "${ARTIFACT}")
    BACKUP=${REMOTE}.usb-replug-stock
    CURRENT=${WORK_DIR}/${ARTIFACT}.current
    "${SSH_HELPER}" exec "cat ${REMOTE}" > "${CURRENT}"
    STATE=$(state_of_file "${ARTIFACT}" "${CURRENT}")
    echo "${ARTIFACT}: target=${STATE}"
    [ "${STATE}" != unknown ] || {
        echo "REFUSE: ${REMOTE} is neither bundled stock nor payload" >&2
        exit 1
    }
    echo "${STATE}" > "${WORK_DIR}/${ARTIFACT}.state"

    if "${SSH_HELPER}" exec "[ -f ${BACKUP} ]" >/dev/null 2>&1; then
        BACKUP_COPY=${WORK_DIR}/${ARTIFACT}.backup
        "${SSH_HELPER}" exec "cat ${BACKUP}" > "${BACKUP_COPY}"
        if ! cmp -s "${BUNDLE_DIR}/stock/${ARTIFACT}" "${BACKUP_COPY}"; then
            echo "REFUSE: existing backup is not exact stock: ${BACKUP}" >&2
            exit 1
        fi
        echo "${ARTIFACT}: stock backup verified"
    elif [ "${STATE}" = payload ]; then
        echo "REFUSE: payload is installed but exact stock backup is missing: ${BACKUP}" >&2
        exit 1
    fi
done

if [ -n "${BASELINE_GUARDS}" ]; then
    echo "Independent A/B baseline guard"
fi
for ARTIFACT in ${BASELINE_GUARDS}; do
    REMOTE=$(remote_path "${ARTIFACT}")
    CURRENT=${WORK_DIR}/${ARTIFACT}.baseline
    "${SSH_HELPER}" exec "cat ${REMOTE}" > "${CURRENT}"
    if ! cmp -s "${BUNDLE_DIR}/stock/${ARTIFACT}" "${CURRENT}"; then
        echo "REFUSE: mode=${MODE} requires unselected ${ARTIFACT} to be exact stock" >&2
        exit 1
    fi
    echo "${ARTIFACT}: independent baseline=stock"
done

case "${MODE}" in
    ncm)
        "${SSH_HELPER}" exec 'mount -uw /mnt/system'
        ;;
    si)
        "${SSH_HELPER}" exec 'mount -uw /mnt/app'
        ;;
    all)
        "${SSH_HELPER}" exec 'mount -uw /mnt/system; mount -uw /mnt/app'
        ;;
esac

echo "Create immutable-name stock backups where needed"
for ARTIFACT in ${ARTIFACTS}; do
    REMOTE=$(remote_path "${ARTIFACT}")
    BACKUP=${REMOTE}.usb-replug-stock
    STATE=$(sed -n '1p' "${WORK_DIR}/${ARTIFACT}.state")
    if [ "${STATE}" = stock ]; then
        "${SSH_HELPER}" exec "[ -f ${BACKUP} ] || cp ${REMOTE} ${BACKUP}; sync"
    fi
done

echo "Upload and read back every staged payload"
for ARTIFACT in ${ARTIFACTS}; do
    REMOTE=$(remote_path "${ARTIFACT}")
    STAGE=${REMOTE}.usb-replug-new
    READBACK=${WORK_DIR}/${ARTIFACT}.stage
    "${SSH_HELPER}" exec "cat > ${STAGE}" < "${BUNDLE_DIR}/payload/${ARTIFACT}"
    "${SSH_HELPER}" exec "cat ${STAGE}" > "${READBACK}"
    if ! cmp -s "${BUNDLE_DIR}/payload/${ARTIFACT}" "${READBACK}"; then
        echo "REFUSE: staged readback mismatch: ${STAGE}" >&2
        exit 1
    fi
    echo "${ARTIFACT}: staged payload verified"
done

case "${MODE}" in
    ncm|all)
        "${SSH_HELPER}" exec '
          set -e
          sh -n /mnt/system/etc/scripts/startncm.sh.usb-replug-new
          sh -n /mnt/system/etc/scripts/stopncm.sh.usb-replug-new
          chmod 755 \
            /mnt/system/etc/scripts/startncm.sh.usb-replug-new \
            /mnt/system/etc/scripts/stopncm.sh.usb-replug-new
          chmod 644 /mnt/system/etc/usblauncher_otg.lua.usb-replug-new
          mv /mnt/system/etc/scripts/startncm.sh.usb-replug-new \
             /mnt/system/etc/scripts/startncm.sh
          mv /mnt/system/etc/scripts/stopncm.sh.usb-replug-new \
             /mnt/system/etc/scripts/stopncm.sh
          mv /mnt/system/etc/usblauncher_otg.lua.usb-replug-new \
             /mnt/system/etc/usblauncher_otg.lua
          sync
        '
        ;;
esac

case "${MODE}" in
    si|all)
        "${SSH_HELPER}" exec '
          set -e
          chmod 755 /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new
          mv /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new \
             /mnt/app/eso/bin/apps/smartphone_integrator
          sync
        '
        ;;
esac

"${SSH_HELPER}" exec \
    "echo install-${MODE} > /tmp/usb_replug_bundle_reboot_required"

echo "Installed persistent mode=${MODE}. No reboot was requested."
echo "The owner must reboot the HU, then run: ${BUNDLE_DIR}/verify_over_ssh.sh ${MODE}"
