#!/bin/bash
# Build the CarPlay LD_PRELOAD hook (libcarplay_hook.so) for QNX/ARMv7 in Docker.
# Unprotected build; output lands in build/unprotected/. For a VIN-locked build
# use scripts/build_hook_protected.sh. The compile+verify live in
# scripts/build_hook_common.sh (one copy, shared by both).
#
#   ./scripts/build_hook.sh                        # default image (LOG=1)
#   QNX_TOOLCHAIN_VARIANT=8.5 ./scripts/build_hook.sh
#   LOG=0 ./scripts/build_hook.sh                  # pre-prod: logs/captures disabled
#   LOG=0 AUDIO_STATE_TRACE=1 ./scripts/build_hook.sh # compact lifecycle trace only
#   LOG=0 ALTSCREEN_PACE_TRACE=1 ./scripts/build_hook.sh # stream-111 timing only
#   LOG_RGD_PACKET_RAW=1 ./scripts/build_hook.sh   # raw RGD logging (needs LOG=1)
#   HOOK_AUDIO=0 ./scripts/build_hook.sh           # no type-103 audio module
set -e

[ "$#" -eq 0 ] || { echo "usage: ./scripts/build_hook.sh"; exit 2; }

. "$(cd "$(dirname "$0")" && pwd)/build_hook_common.sh"

OUTDIR_REL="build/unprotected" carplay_build_hook
