#!/bin/bash
# Offline stock-210 pairing/control module, embedded in the existing hook.
# Separate output preserves the ordinary wired build. No install or HU access.
set -e
[ "$#" -eq 0 ] || { echo "usage: ./scripts/build_hook_wireless.sh"; exit 2; }
. "$(cd "$(dirname "$0")" && pwd)/build_hook_common.sh"
HOOK_WIRELESS=1 OUTDIR_REL=build/wireless carplay_build_hook
