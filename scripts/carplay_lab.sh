#!/usr/bin/env bash
# Active guest platform: vphone. The retained Inferno entry points are explicit.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
action="${1:-doctor}"
if (( $# )); then shift; fi
qnx_dir="build/vphone-lab/qnx-network"
case "$action" in
    bootstrap)
        exec python3 -m wireless.vphone_lab.bootstrap "$@" ;;
    doctor|plan|create|recover-create|start|debug-start|stop|status|snapshot|diagnose|unlock|screenshot)
        exec python3 -m wireless.vphone_lab.lab "$action" "$@" ;;
    usb)
        exec python3 -m wireless.vphone_lab.usb "$@" ;;
    usb-kernel)
        exec python3 -m wireless.vphone_lab.usb_kernel "$@" ;;
    iap-smoke)
        exec python3 -m wireless.vphone_lab.iap "$@" ;;
    mfi-probe)
        exec build/vphone-lab/venv/bin/python -m wireless.vphone_lab.mfi_probe "$@" ;;
    ethernet)
        exec python3 -m wireless.vphone_lab.ethernet "$@" ;;
    moving-route)
        exec python3 -m wireless.vphone_lab.moving_route "$@" ;;
    input)
        exec python3 -m wireless.vphone_lab.carplay_input "$@" ;;
    qnx-build)
        source scripts/build_hook_common.sh
        capture_args=(--capture-video)
        for option in "$@"; do
            if [[ "$option" == --pacing-only || "$option" == --capture-memory ]]; then capture_args=(); fi
        done
        OUTDIR_REL=build/wireless-alt-qemu HOOK_WIRELESS=1 HOOK_AUDIO=0 \
            WIRELESS_ALT_QEMU=1 QNX_TOOLCHAIN_VARIANT=8.5 carplay_build_hook
        exec env QNX_TOOLCHAIN_VARIANT=8.5 python3 -m wireless.lab_common.qnx_receiver \
            build --lab-dir "$qnx_dir" --pairing --media --software-mfi \
            --request-ui "${capture_args[@]}" --paired-altscreen "$@" ;;
    qnx-start|qnx-stop|qnx-status|qnx-info)
        exec python3 -m wireless.lab_common.qnx_receiver "${action#qnx-}" \
            --lab-dir "$qnx_dir" --transport socket "$@" ;;
    mfi)
        exec python3 -m wireless.lab_common.mfi_sap --qnx-dir "$qnx_dir" "$@" ;;
    extract)
        exec python3 -m wireless.lab_common.extract_native_video --lab-dir build/vphone-lab "$@" ;;
    pacing)
        exec python3 -m wireless.lab_common.frame_pacing --lab-dir build/vphone-lab "$@" ;;
    wire-pacing)
        exec python3 -m wireless.lab_common.screen_wire_pacing --lab-dir build/vphone-lab "$@" ;;
    *)
        printf 'Unknown action: %s\n' "$action" >&2
        exit 2 ;;
esac
