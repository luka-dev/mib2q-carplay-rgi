#!/bin/bash
set -e

QNX_VM="192.168.64.16"
SSH_OPTS="-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOOK_DIR="${SCRIPT_DIR}/c_hook"
BUILD_DIR="${SCRIPT_DIR}/build"
OUT="${HOOK_OUT:-${BUILD_DIR}/libcarplay_hook.so}"
mkdir -p "$BUILD_DIR"

# Source files
FRAMEWORK_SRCS="
    framework/logging.c
    framework/bus.c
    framework/iap2_protocol.c
    framework/hook_framework.c
"

RGD_SRCS="
    routeguidance/rgd_tlv.c
    routeguidance/rgd_hook.c
"

CVR_SRCS="
    coverart/coverart_hook.c
"

MAIN_SRCS="main.c"

echo "=== CarPlay Hook Build ==="
echo "Hook dir: $HOOK_DIR"

EXTRA_CFLAGS="-D__QNX__"
EXTRA_LIBS="-lz -lsocket"  # QNX: dlsym is in libc, socket for renderer control

# Build options (supported):
#   LOG=1|0                 (default: 1)
#   LOG_RGD_PACKET_RAW=1|0  (default: 0, requires LOG=1)
LOG="${LOG:-1}"
LOG_RGD_PACKET_RAW="${LOG_RGD_PACKET_RAW:-0}"

if [[ "$LOG" != "0" && "$LOG" != "1" ]]; then
    echo "Invalid LOG value: $LOG (expected 0 or 1)"
    exit 1
fi

if [[ "$LOG_RGD_PACKET_RAW" != "0" && "$LOG_RGD_PACKET_RAW" != "1" ]]; then
    echo "Invalid LOG_RGD_PACKET_RAW value: $LOG_RGD_PACKET_RAW (expected 0 or 1)"
    exit 1
fi

if [[ "$LOG" == "0" ]]; then
    EXTRA_CFLAGS="${EXTRA_CFLAGS} -DENABLE_LOGGING=0"
    echo "Logging DISABLED"
fi

if [[ "$LOG_RGD_PACKET_RAW" == "1" ]]; then
    if [[ "$LOG" != "1" ]]; then
        echo "LOG_RGD_PACKET_RAW=1 requires LOG=1"
        exit 1
    fi
    EXTRA_CFLAGS="${EXTRA_CFLAGS} -DRGD_TRACE_RAW_FULL=1"
    echo "RGD raw packet full logging ENABLED"
fi

# Modular framework compile
REMOTE_DIR="/tmp/carplay_hook"

echo "Uploading sources to QNX VM..."
tar --disable-copyfile --format=ustar -C "$HOOK_DIR" -cf - framework routeguidance coverart main.c \
    | sshpass -p "root" ssh $SSH_OPTS root@$QNX_VM "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR && tar -xf - -C $REMOTE_DIR"

echo "Compiling on QNX VM..."

# Single-shot compile + link (no intermediate .o files)
ALL_SRCS=""
for f in $FRAMEWORK_SRCS $RGD_SRCS $CVR_SRCS $MAIN_SRCS; do
    ALL_SRCS="$ALL_SRCS $REMOTE_DIR/$f"
done

BUILD_CMD="/usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-gcc -shared -fPIC -O2 -std=gnu99 $EXTRA_CFLAGS -I$REMOTE_DIR $ALL_SRCS -o $REMOTE_DIR/libcarplay_hook.so $EXTRA_LIBS"

sshpass -p "root" ssh $SSH_OPTS root@$QNX_VM \
    "export QNX_HOST=/usr/qnx650/host/qnx6/x86; \
     export QNX_TARGET=/usr/qnx650/target/qnx6; \
     cd $REMOTE_DIR && \
     $BUILD_CMD && \
     ls -lh $REMOTE_DIR/libcarplay_hook.so"

echo "Downloading compiled library..."
sshpass -p "root" scp $SSH_OPTS root@$QNX_VM:$REMOTE_DIR/libcarplay_hook.so "$OUT"

echo ""
echo "Compiled: $OUT"
ls -lh "$OUT"
