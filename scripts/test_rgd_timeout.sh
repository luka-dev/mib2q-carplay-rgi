#!/bin/bash
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
CFLAGS=(-std=gnu99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
    -DENABLE_LOGGING=0 -I"$PROJECT_DIR/hook")
cc "${CFLAGS[@]}" -Dbus_send_text=bus_send_text_unused -c "$PROJECT_DIR/hook/framework/bus.c" -o "$TEST_DIR/bus.o"
cc "${CFLAGS[@]}" "$PROJECT_DIR/tests/rgd_timeout_race_test.c" "$TEST_DIR/bus.o" \
    "$PROJECT_DIR/hook/routeguidance/rgd_tlv.c" "$PROJECT_DIR/hook/framework/iap2_protocol.c" \
    "$PROJECT_DIR/hook/framework/signal_guard.c" -lpthread -o "$TEST_DIR/timeout"
"$TEST_DIR/timeout"
