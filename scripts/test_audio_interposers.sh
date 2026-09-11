#!/bin/sh
# Same production C interposers with injected QSA/AirPlay/CF/loader boundaries.
# Optional: SANITIZE=address,undefined or SANITIZE=thread (separate runs).
set -eu
cd "$(dirname "$0")/.."
audio_test_dir=$(mktemp -d)
trap 'rm -rf "$audio_test_dir"' EXIT HUP INT TERM
set --
if [ -n "${SANITIZE:-}" ]; then
    set -- "-fsanitize=$SANITIZE" -fno-omit-frame-pointer
fi
"${CC:-cc}" -std=gnu99 -O1 -g -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
    -Wno-unused-function "$@" -Itests/mocks -Ihook \
    tests/audio_interposer_test.c \
    hook/audio/stereo_upmix.c hook/audio/hssr_dsp.c hook/audio/hssr_neon.c \
    hook/audio/hssr_control_worker.c hook/audio/soundcheck.c \
    hook/audio/spatial_downmix.c hook/audio/apap_lifecycle.c \
    hook/audio/apap_crypto.c hook/audio/aac_bridge.c hook/framework/iap2_protocol.c \
    -lpthread -lm -o "$audio_test_dir/audio_interposers"
"$audio_test_dir/audio_interposers"
