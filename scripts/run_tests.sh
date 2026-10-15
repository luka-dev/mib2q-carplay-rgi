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
cc -std=gnu99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    -DSPA_STEREO_UPMIX_TESTING \
    tests/stereo_upmix_test.c hook/audio/stereo_upmix.c \
    hook/audio/scene_engine_math.c hook/audio/scene_clusters.c hook/audio/scene_engine.c hook/audio/scene_mask.c \
    hook/audio/scene_mask_worker.c -lm -lpthread -o "$OUT/upmix"
"$OUT/upmix"

printf '%-32s ' rt_upmix_guard_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/rt_upmix_guard_test.c -o "$OUT/rtguard"
"$OUT/rtguard"

printf '%-32s ' buffered_pcm_switch_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/buffered_pcm_switch_test.c hook/audio/spatial_downmix.c \
    hook/audio/stereo_upmix.c \
    hook/audio/scene_engine_math.c hook/audio/scene_clusters.c hook/audio/scene_engine.c hook/audio/scene_mask.c \
    -lm -o "$OUT/switch"
"$OUT/switch"

printf '%-32s ' media_route_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/media_route_test.c -lpthread -o "$OUT/media_route"
"$OUT/media_route"

printf '%-32s ' pcm_io_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/pcm_io_test.c -o "$OUT/pcm_io"
"$OUT/pcm_io"

printf '%-32s ' media_transport_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/media_transport_test.c -o "$OUT/media_transport"
"$OUT/media_transport"

printf '%-32s ' receiver_wait_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/receiver_wait_test.c -lpthread -o "$OUT/receiver_wait"
"$OUT/receiver_wait"

printf '%-32s ' audio_interposer_test
sh scripts/test_audio_interposers.sh

printf '%-32s ' aac_fifo_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/aac_fifo_test.c -o "$OUT/aac_fifo"
"$OUT/aac_fifo"

printf '%-32s ' apap_lifecycle_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/apap_lifecycle_test.c hook/audio/apap_lifecycle.c \
    -o "$OUT/apap_lifecycle"
"$OUT/apap_lifecycle"

printf '%-32s ' soundcheck_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/soundcheck_test.c hook/audio/soundcheck.c -lm \
    -o "$OUT/soundcheck"
"$OUT/soundcheck"

printf '%-32s ' coverart_safety_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
    tests/coverart_safety_test.c hook/coverart/jpeg_safety.c \
    -o "$OUT/coverart_safety"
"$OUT/coverart_safety"

printf '%-32s ' coverart_stream_test
cc -std=c99 -O2 -Wall -Wextra -Werror -Ihook \
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

printf '%-32s ' altscreen_av_test
cc -std=gnu99 -O2 -Wall -Wextra -Werror \
    -Wno-unused-variable -Wno-unused-but-set-variable -Ihook \
    tests/altscreen_av_test.c -lpthread -o "$OUT/altscreen_av"
"$OUT/altscreen_av"

for suite in async wire queue_policy control health_wire negotiation idle_clock control_wire policy; do
    printf '%-32s ' "altscreen_${suite}_test"
    cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-variable \
        -Ihook -Icommon "tests/altscreen_${suite}_test.c" altscreen_render/frame_io.c \
        -lpthread -o "$OUT/alt_$suite"
    "$OUT/alt_$suite"
done
printf '%-32s ' altscreen_command_policy_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror tests/altscreen_command_policy_test.c -o "$OUT/alt_command_policy"
"$OUT/alt_command_policy"
if [ "$(uname)" = Darwin ]; then CMD_STRIP=-Wl,-dead_strip; else CMD_STRIP=-Wl,--gc-sections; fi
for suite in worker reversal age refresh night; do
    cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
        -Wno-unused-but-set-variable -ffunction-sections -fdata-sections -Ihook -Icommon \
        "tests/altscreen_command_${suite}_test.c" "$CMD_STRIP" -lpthread -lm -ldl -o "$OUT/alt_command_$suite"
    "$OUT/alt_command_$suite"
done
printf '%-32s ' altscreen_deadline_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-variable -Ihook \
    tests/altscreen_deadline_test.c -lpthread -o "$OUT/alt_deadline"
"$OUT/alt_deadline"
cc -std=gnu99 -O1 -Wall -Wextra -Werror -Ihook tests/altscreen_recovery_test.c -o "$OUT/alt_recovery"
"$OUT/alt_recovery"
cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable -Ihook \
    tests/altscreen_io_batch_test.c -lpthread -o "$OUT/alt_io_batch"
"$OUT/alt_io_batch"
cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable \
    -Icommon -Itoolchain/qnx65-abi/include/OMX tests/altscreen_mailbox_test.c -lpthread -o "$OUT/alt_mailbox"
"$OUT/alt_mailbox"

printf '%-32s ' altscreen_pace_trace_test
cc -std=c99 -O2 -Wall -Wextra -Werror tests/altscreen_pace_trace_test.c -o "$OUT/alt_pace"
"$OUT/alt_pace"

printf '%-32s ' altscreen_sink_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare \
    -Itoolchain/qnx65-abi/include/OMX -Icommon \
    tests/altscreen_sink_test.c -lpthread -o "$OUT/altscreen_sink"
"$OUT/altscreen_sink"

printf '%-32s ' altscreen_worker_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror \
    -Icommon tests/altscreen_worker_test.c altscreen_render/frame_io.c \
    -lpthread -o "$OUT/altscreen_worker"
"$OUT/altscreen_worker"

printf '%-32s ' altscreen_readiness_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror \
    -Icommon tests/altscreen_readiness_test.c altscreen_render/frame_io.c \
    -lpthread -o "$OUT/altscreen_readiness"
"$OUT/altscreen_readiness"

printf '%-32s ' altscreen_timing_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror tests/altscreen_timing_test.c -o "$OUT/altscreen_timing"
"$OUT/altscreen_timing"

printf '%-32s ' gl_program_cache_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-function \
    -Ialtscreen_render/hostcheck -Icommon tests/gl_program_cache_test.c -o "$OUT/gl_program_cache"
"$OUT/gl_program_cache"

printf '%-32s ' altscreen_present_test
if [ "$(uname)" = Darwin ]; then ALT_DEAD_STRIP=-Wl,-dead_strip; else ALT_DEAD_STRIP=-Wl,--gc-sections; fi
cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-function \
    -ffunction-sections -fdata-sections -Ialtscreen_render/hostcheck -Icommon \
    tests/altscreen_present_test.c "$ALT_DEAD_STRIP" -lpthread -lm -o "$OUT/altscreen_present"
"$OUT/altscreen_present"

printf '%-32s ' maneuver_surface_test
cc -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-function \
    -ffunction-sections -fdata-sections -Imaneuver_render/hostcheck -Ialtscreen_render/hostcheck -Icommon \
    tests/maneuver_surface_test.c "$ALT_DEAD_STRIP" -lpthread -lm -o "$OUT/maneuver_surface"
"$OUT/maneuver_surface"

printf '%-32s ' maneuver_reroute_clear
bash scripts/test_maneuver_reroute.sh

printf '%-32s ' rgd_tlv_test
cc -std=c99 -O1 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function \
    -DENABLE_LOGGING=0 -Ihook tests/rgd_tlv_test.c hook/routeguidance/rgd_tlv.c \
    -o "$OUT/rgd_tlv"
"$OUT/rgd_tlv"

printf '%-32s ' tel_hook_test
cc -std=c99 -O1 -Wall -Wextra -Werror -Wno-unused-function \
    -DENABLE_LOGGING=0 -Ihook tests/tel_hook_test.c hook/telephony/tel_hook.c \
    hook/framework/iap2_protocol.c -o "$OUT/tel_hook"
"$OUT/tel_hook"

printf '%-32s ' vehicle_hook_test
cc -std=c99 -O1 -Wall -Wextra -Werror -Wno-unused-function \
    -DENABLE_LOGGING=0 -DVEHICLE_HOOK_TESTING -Ihook tests/vehicle_hook_test.c hook/vehicle/vehicle_hook.c \
    hook/framework/iap2_protocol.c -o "$OUT/vehicle_hook"
"$OUT/vehicle_hook"

printf '%-32s ' inject_generation_test
cc -std=c99 -O1 -Wall -Wextra -Werror -Wno-unused-function \
    -DENABLE_LOGGING=0 -Ihook -Ihook/framework tests/inject_generation_test.c -o "$OUT/inject_generation"
"$OUT/inject_generation"

printf '%-32s ' state_trace_test
cc -std=c99 -O2 -Wall -Wextra -Werror -Ihook \
    -DENABLE_LOGGING=0 -DENABLE_STATE_TRACE=1 \
    tests/state_trace_test.c hook/framework/state_trace.c \
    hook/framework/iap2_protocol.c -o "$OUT/state_trace"
"$OUT/state_trace" && echo OK

printf '%-32s ' signal_guard_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
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
cc -std=c99 -O2 -Wall -Wextra -Werror -Ihook \
    -DENABLE_LOGGING=0 -DENABLE_STATE_TRACE=1 \
    tests/state_trace_logger_test.c hook/framework/logging.c \
    -lpthread -ldl -o "$OUT/state_trace_logger"
"$OUT/state_trace_logger" && echo OK

printf '%-32s ' alt_pace_logger_test
cc -std=c99 -O2 -Wall -Wextra -Werror -Ihook \
    -DENABLE_LOGGING=0 -DENABLE_ALTSCREEN_PACE_TRACE=1 \
    tests/state_trace_logger_test.c hook/framework/logging.c \
    -lpthread -ldl -o "$OUT/alt_pace_logger"
"$OUT/alt_pace_logger" && echo OK

printf '%-32s ' state_trace_analyzer_test
trace_summary=$(python3 tools/analyze_carplay_state_trace.py \
    tests/fixtures/state_trace_sample.log)
printf '%s\n' "$trace_summary" | grep -q \
    'buffered_pre_rate_qsa_writes: 0'
printf '%s\n' "$trace_summary" | grep -q \
    'compressed_queue_failures: 0'
printf '%s\n' "$trace_summary" | grep -q \
    'flush_applied_events: 0'
printf '%s\n' "$trace_summary" | grep -q \
    'pid 4242 generation 0: Pre-Identify process events only'
printf '%s\n' "$trace_summary" | grep -q \
    'pid 4242 generation 1: The traced transport/render path and stock media-route declaration completed through QSA.'
printf '%s\n' "$trace_summary" | grep -q \
    'pid 4242 generation 2: No outbound 0x5000 was constructed'
printf '%s\n' "$trace_summary" | grep -q \
    'pid 4343 generation 1: No outbound 0x5000 was constructed'
printf '%s\n' "$trace_summary" | grep -q \
    'pid 4242 generation 3: Stock type 100 released the stock Media route while type 103 kept rendering'
printf '%s\n' "$trace_summary" | grep -q \
    'pid 4242 generation 4: The traced transport/render path and stock media-route declaration completed through QSA.'
printf '%s\n' "$trace_summary" | grep -q \
    'route_reacquire_missing: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'hid_report_posts: 1'
printf '%s\n' "$trace_summary" | grep -q \
    "now_playing_retry_actions: \['queued'\]"
printf '%s\n' "$trace_summary" | grep -q \
    "'upmix': 'scene'"
printf '%s\n' "$trace_summary" | grep -q \
    "qsa_open_actions: \['cold_buffered'\]"
printf '%s\n' "$trace_summary" | grep -q \
    "audio_route_actions: \['request'\]"
printf '%s\n' "$trace_summary" | grep -q \
    'qsa_cold_bootstrap_failures: 0'
printf '%s\n' "$trace_summary" | grep -q \
    'buffered_packet_anomalies:'
printf '%s\n' "$trace_summary" | grep -q \
    "'zero-pcm': 1"
printf '%s\n' "$trace_summary" | grep -q \
    'buffered_sequence_gaps: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'apap_max_gap_ms: 141'
printf '%s\n' "$trace_summary" | grep -q \
    'qsa_ring_wait_max_ms: 143'
printf '%s\n' "$trace_summary" | grep -q \
    'qsa_cadence_max_gap_ms: 146'
printf '%s\n' "$trace_summary" | grep -q \
    'qsa_device_underruns: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'audio_health_decision: QSA device underrun counter advanced'
printf '%s\n' "$trace_summary" | grep -q \
    'flush_restart_sequence_gaps: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'flush_restart_anchors: 2'
printf '%s\n' "$trace_summary" | grep -q \
    'flush_boundaries_crossed: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'flush_discarded_packets_total: 497'
printf '%s\n' "$trace_summary" | grep -q \
    'flush_restart_apap_gaps: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'flush_restart_qsa_gaps: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'flush_restart_qsa_write_anomalies: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'qsa_queue_prefill_clock_resets: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'qsa_buffered_silence_periods: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'flush_restart_buffered_silences: 1'
printf '%s\n' "$trace_summary" | grep -q \
    'steady_buffered_silences: 0'
printf '%s\n' "$trace_summary" | grep -q \
    'steady_qsa_gaps: 0'
printf '%s\n' "$trace_summary" | grep -q \
    'audio_health_decision: All recorded buffered/QSA anomalies followed FLUSHBUFFERED within 2000 ms'
lifecycle_summary=$(python3 tools/analyze_carplay_state_trace.py \
    tests/fixtures/state_trace_lifecycle_sample.log)
printf '%s\n' "$lifecycle_summary" | grep -q \
    'buffered_initial_rates: \[0\]'
printf '%s\n' "$lifecycle_summary" | grep -q \
    'buffered_pre_rate_qsa_writes: 1'
printf '%s\n' "$lifecycle_summary" | grep -q \
    'compressed_queue_max_packets: 10'
printf '%s\n' "$lifecycle_summary" | grep -q \
    'compressed_queue_max_bytes: 400'
printf '%s\n' "$lifecycle_summary" | grep -q \
    'compressed_queue_backpressure_events: 1'
printf '%s\n' "$lifecycle_summary" | grep -q \
    'compressed_queue_failures: 1'
printf '%s\n' "$lifecycle_summary" | grep -q \
    'flush_applied_events: 1'
printf '%s\n' "$lifecycle_summary" | grep -q \
    'flush_removed_compressed_packets: 2'
printf '%s\n' "$lifecycle_summary" | grep -q \
    'flush_removed_pcm_frames: 1024'
echo OK

printf '%-32s ' apac_integration_test
c++ -std=c++11 -O2 -w -fno-exceptions -fno-rtti \
    -Iapac_decoder/include -Ihook -Ihook/audio -x c++ \
    tests/apac_integration_test.c hook/audio/apap_crypto.c \
    hook/audio/spatial_downmix.c hook/audio/apac_bridge.cpp \
    apac_decoder/src/*.cpp -o "$OUT/integration"
"$OUT/integration"


printf '%-32s ' upmix_lang_test
out=$(python3 -I tests/upmix_lang_test.py) || { echo "$out"; exit 1; }
printf '%s\n' "$out" | tail -1

printf '%-32s ' install_dio_test
sh scripts/test_install_dio.sh

printf '%-32s ' install_listing_test
sh scripts/test_install_listing.sh

printf '%-32s ' install_ncm_test
sh scripts/test_install_ncm.sh

printf '%-32s ' install_java_gate_test
sh scripts/test_install_java_gate.sh

printf '%-32s ' java_variants_link_test
bash scripts/build_java.sh > /dev/null && bash scripts/audit_java_variants.sh

printf '%-32s ' binpatch_test
bash scripts/test_binpatch.sh
printf '%-32s ' install_e2e_test
bash scripts/test_install_e2e.sh

printf '%-32s ' logging_mib_test
sh scripts/test_logging_mib.sh

printf '%-32s ' supervisor_lifecycle_test
sh scripts/test_supervisor_lifecycle.sh
