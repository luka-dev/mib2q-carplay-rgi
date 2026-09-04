#!/bin/sh
# Host-side tests for the shipping hook.  The APAC decoder port has its own
# suite (`make -C apac_decoder check`); this runs what lives under hook/.
#
# The integration test needs a live session capture, which is never committed.
# Without one it skips, so this script is safe to run anywhere.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

printf '%-32s ' spatial_downmix_test
cc -std=c99 -O2 -Wall -Wextra -Werror -Ihook \
    tests/spatial_downmix_test.c hook/audio/spatial_downmix.c -o "$OUT/downmix"
"$OUT/downmix" && echo OK

printf '%-32s ' stereo_upmix_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/stereo_upmix_test.c hook/audio/stereo_upmix.c -o "$OUT/upmix"
"$OUT/upmix"

printf '%-32s ' buffered_pcm_switch_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/buffered_pcm_switch_test.c hook/audio/spatial_downmix.c \
    hook/audio/stereo_upmix.c -o "$OUT/switch"
"$OUT/switch"

printf '%-32s ' apac_integration_test
c++ -std=c++11 -O2 -w -fno-exceptions -fno-rtti \
    -Iapac_decoder/include -Ihook -Ihook/audio -x c++ \
    tests/apac_integration_test.c hook/audio/apap_crypto.c \
    hook/audio/spatial_downmix.c hook/audio/apac_bridge.cpp \
    apac_decoder/src/*.cpp -o "$OUT/integration"
"$OUT/integration"

printf '%-32s ' card_install_test
sh tests/card_install_test.sh
