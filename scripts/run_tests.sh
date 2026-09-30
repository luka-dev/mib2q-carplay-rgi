#!/bin/sh
# Host-side C tests for the RGI hook framework and maneuver renderer.
# Java/renderer suites: test_route_info.sh, test_java_transports.sh,
# test_maneuver_native.sh.  Safe to run anywhere (no HU access).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
if [ "$(uname)" = Darwin ]; then DEAD_STRIP=-Wl,-dead_strip; else DEAD_STRIP=-Wl,--gc-sections; fi

printf '%-32s ' maneuver_surface_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-function \
    -ffunction-sections -fdata-sections -Imaneuver_render/hostcheck -Icommon \
    tests/maneuver_surface_test.c "$DEAD_STRIP" -lpthread -lm -o "$OUT/maneuver_surface"
"$OUT/maneuver_surface"

# Reroute bug hunt: real renderer command loop (main.c) with scripted commands and
# stubbed GL/platform.  Non-fatal: prints "REPRODUCED <case>" or "PASS" and never
# aborts the rest of this script.
printf '%-32s ' maneuver_reroute_clear
if cc -std=gnu99 -O1 -Wall -Wno-unused-function -Wno-unused-variable \
        -Imaneuver_render -Icommon \
        tests/maneuver_reroute_clear_test.c -lpthread -lm -o "$OUT/maneuver_reroute_clear"; then
    "$OUT/maneuver_reroute_clear" || echo "maneuver_reroute_clear: FAILED (non-fatal)"
else
    echo "maneuver_reroute_clear: BUILD FAILED (non-fatal)"
fi

printf '%-32s ' gl_program_cache_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-function \
    -Imaneuver_render/hostcheck -Icommon tests/gl_program_cache_test.c -o "$OUT/gl_program_cache"
"$OUT/gl_program_cache"

printf '%-32s ' coverart_safety_test
cc -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/coverart_safety_test.c hook/coverart/jpeg_safety.c \
    -o "$OUT/coverart_safety"
"$OUT/coverart_safety"

printf '%-32s ' coverart_stream_test
cc -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -Ihook \
    tests/coverart_stream_test.c hook/coverart/coverart_stream.c \
    hook/framework/iap2_protocol.c -o "$OUT/coverart_stream"
"$OUT/coverart_stream"

printf '%-32s ' coverart_pipeline_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
    -Ihook "-DCOVERART_DIR=\"$OUT/artwork\"" tests/coverart_pipeline_test.c \
    hook/coverart/jpeg_safety.c hook/coverart/coverart_stream.c \
    hook/framework/iap2_protocol.c -lpthread -lz -lm -o "$OUT/coverart_pipeline"
"$OUT/coverart_pipeline"

printf '%-32s ' rgd_tlv_test
cc -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -O1 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function \
    -DENABLE_LOGGING=0 -Ihook tests/rgd_tlv_test.c hook/routeguidance/rgd_tlv.c \
    -o "$OUT/rgd_tlv"
"$OUT/rgd_tlv"

printf '%-32s ' inject_generation_test
cc -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -O1 -Wall -Wextra -Werror -Wno-unused-function \
    -DENABLE_LOGGING=0 -Ihook -Ihook/framework tests/inject_generation_test.c -o "$OUT/inject_generation"
"$OUT/inject_generation"

printf '%-32s ' state_trace_test
cc -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -Ihook \
    -DENABLE_LOGGING=0 -DENABLE_STATE_TRACE=1 \
    tests/state_trace_test.c hook/framework/state_trace.c \
    hook/framework/iap2_protocol.c -o "$OUT/state_trace"
"$OUT/state_trace" && echo OK

printf '%-32s ' signal_guard_test
cc -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/signal_guard_test.c hook/framework/signal_guard.c \
    -o "$OUT/signal_guard"
"$OUT/signal_guard"

printf '%-32s ' bus_transport_test
cc -std=gnu99 -O2 -Wall -Wextra -Werror \
    -Wno-unused-variable -Wno-unused-but-set-variable -Ihook \
    tests/bus_transport_test.c hook/framework/signal_guard.c \
    -lpthread -o "$OUT/bus_transport"
"$OUT/bus_transport"

printf '%-32s ' state_trace_logger_test
cc -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -Ihook \
    -DENABLE_LOGGING=0 -DENABLE_STATE_TRACE=1 \
    tests/state_trace_logger_test.c hook/framework/logging.c \
    -lpthread -ldl -o "$OUT/state_trace_logger"
"$OUT/state_trace_logger" && echo OK

printf '%-32s ' local_protocols
python3 scripts/check_local_protocols.py

printf '%-32s ' install_dio_test
sh scripts/test_install_dio.sh

printf '%-32s ' logging_mib_test
sh scripts/test_logging_mib.sh

printf '%-32s ' supervisor_lifecycle_test
sh scripts/test_supervisor_lifecycle.sh

printf '%-32s ' install_listing_test
sh scripts/test_install_listing.sh

printf '%-32s ' install_payload_test
sh scripts/test_install_payload.sh
