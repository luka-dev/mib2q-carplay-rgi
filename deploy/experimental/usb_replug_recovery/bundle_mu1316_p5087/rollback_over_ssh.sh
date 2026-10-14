#!/bin/sh

# Host-side exact-stock rollback. It never reboots the HU.

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
        ;;
    si)
        ARTIFACTS="smartphone_integrator"
        ;;
    all)
        ARTIFACTS="startncm.sh stopncm.sh usblauncher_otg.lua smartphone_integrator"
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

WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mhi2-usb-rollback.XXXXXX")
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

echo "Read-only rollback preflight"
for ARTIFACT in ${ARTIFACTS}; do
    REMOTE=$(remote_path "${ARTIFACT}")
    CURRENT=${WORK_DIR}/${ARTIFACT}.current
    "${SSH_HELPER}" exec "cat ${REMOTE}" > "${CURRENT}"
    if cmp -s "${BUNDLE_DIR}/stock/${ARTIFACT}" "${CURRENT}"; then
        echo "${ARTIFACT}: already stock"
    elif cmp -s "${BUNDLE_DIR}/payload/${ARTIFACT}" "${CURRENT}"; then
        echo "${ARTIFACT}: payload -> stock"
    else
        echo "REFUSE: ${REMOTE} is neither bundled stock nor payload" >&2
        exit 1
    fi
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

echo "Stage and verify exact bundled stock"
for ARTIFACT in ${ARTIFACTS}; do
    REMOTE=$(remote_path "${ARTIFACT}")
    STAGE=${REMOTE}.rollback-new
    READBACK=${WORK_DIR}/${ARTIFACT}.stage
    "${SSH_HELPER}" exec "cat > ${STAGE}" < "${BUNDLE_DIR}/stock/${ARTIFACT}"
    "${SSH_HELPER}" exec "cat ${STAGE}" > "${READBACK}"
    if ! cmp -s "${BUNDLE_DIR}/stock/${ARTIFACT}" "${READBACK}"; then
        echo "REFUSE: rollback readback mismatch: ${STAGE}" >&2
        exit 1
    fi
done

case "${MODE}" in
    ncm|all)
        "${SSH_HELPER}" exec '
          set -e
          sh -n /mnt/system/etc/scripts/startncm.sh.rollback-new
          sh -n /mnt/system/etc/scripts/stopncm.sh.rollback-new
          chmod 755 \
            /mnt/system/etc/scripts/startncm.sh.rollback-new \
            /mnt/system/etc/scripts/stopncm.sh.rollback-new
          chmod 644 /mnt/system/etc/usblauncher_otg.lua.rollback-new
          mv /mnt/system/etc/scripts/startncm.sh.rollback-new \
             /mnt/system/etc/scripts/startncm.sh
          mv /mnt/system/etc/scripts/stopncm.sh.rollback-new \
             /mnt/system/etc/scripts/stopncm.sh
          mv /mnt/system/etc/usblauncher_otg.lua.rollback-new \
             /mnt/system/etc/usblauncher_otg.lua
          sync
        '
        ;;
esac

case "${MODE}" in
    si|all)
        "${SSH_HELPER}" exec '
          set -e
          chmod 755 /mnt/app/eso/bin/apps/smartphone_integrator.rollback-new
          mv /mnt/app/eso/bin/apps/smartphone_integrator.rollback-new \
             /mnt/app/eso/bin/apps/smartphone_integrator
          sync
        '
        ;;
esac

"${SSH_HELPER}" exec \
    "echo rollback-${MODE} > /tmp/usb_replug_bundle_reboot_required"

echo "Restored persistent stock mode=${MODE}. No reboot was requested."
echo "The owner must reboot the HU, then run: ${BUNDLE_DIR}/verify_over_ssh.sh stock"
