#!/bin/bash
set -e

QNX_VM="192.168.64.16"
SSH_OPTS="-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RENDERER_DIR="${SCRIPT_DIR}/c_render"
BUILD_DIR="${SCRIPT_DIR}/build"
OUT="${RENDER_OUT:-${BUILD_DIR}/maneuver_render}"
REMOTE_DIR="/tmp/c_render"
mkdir -p "$BUILD_DIR"

RENDERER_SRCS="main.c render.c maneuver.c route_path.c server.c platform_qnx.c"

echo "=== Cluster Renderer Build ==="
echo "Renderer dir: $RENDERER_DIR"

echo "Uploading renderer sources..."
tar --disable-copyfile --format=ustar -C "$RENDERER_DIR" -cf - \
    main.c render.c render.h maneuver.c maneuver.h route_path.c route_path.h \
    server.c server.h protocol.h platform.h platform_qnx.c gl_compat.h \
    | sshpass -p "root" ssh $SSH_OPTS root@$QNX_VM "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR && tar -xf - -C $REMOTE_DIR"

echo "Compiling on QNX VM..."

EXTRA_CFLAGS=""
if [ "$1" = "grid" ]; then
    EXTRA_CFLAGS="-DCR_DEBUG_GRID"
    echo "Building with DEBUG GRID"
fi

SRC_PATHS=""
for f in $RENDERER_SRCS; do
    SRC_PATHS="$SRC_PATHS $REMOTE_DIR/$f"
done

BUILD_CMD="/usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-gcc -O2 -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX $EXTRA_CFLAGS -I$REMOTE_DIR $SRC_PATHS -o $REMOTE_DIR/maneuver_render -lEGL -lGLESv2 -lsocket -lm"

sshpass -p "root" ssh $SSH_OPTS root@$QNX_VM \
    "export QNX_HOST=/usr/qnx650/host/qnx6/x86; \
     export QNX_TARGET=/usr/qnx650/target/qnx6; \
     cd $REMOTE_DIR && \
     $BUILD_CMD && \
     ls -lh $REMOTE_DIR/maneuver_render"

echo "Downloading compiled binary..."
sshpass -p "root" scp $SSH_OPTS root@$QNX_VM:$REMOTE_DIR/maneuver_render "$OUT"

echo ""
echo "Compiled: $OUT"
ls -lh "$OUT"
