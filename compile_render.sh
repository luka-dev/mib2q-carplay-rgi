#!/bin/bash
set -e

QNX_VM="192.168.64.16"
SSH_OPTS="-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RENDERER_DIR="${SCRIPT_DIR}/c_render"
OUT="${RENDER_OUT:-${SCRIPT_DIR}/c_render_bin}"
REMOTE_DIR="/tmp/c_render"

RENDERER_SRCS="main.c render.c maneuver.c route_path.c server.c platform_qnx.c"

echo "=== Cluster Renderer Build ==="
echo "Renderer dir: $RENDERER_DIR"

echo "Uploading renderer sources..."
tar --disable-copyfile --format=ustar -C "$RENDERER_DIR" -cf - \
    main.c render.c render.h maneuver.c maneuver.h route_path.c route_path.h \
    server.c server.h protocol.h platform.h platform_qnx.c gl_compat.h \
    | sshpass -p "root" ssh $SSH_OPTS root@$QNX_VM "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR && tar -xf - -C $REMOTE_DIR"

echo "Compiling on QNX VM..."

COMPILE_CMD=""
OBJ_FILES=""

for f in $RENDERER_SRCS; do
    obj="${f%.c}.o"
    OBJ_FILES="$OBJ_FILES $REMOTE_DIR/$obj"
    COMPILE_CMD="$COMPILE_CMD \
        /usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-gcc -c -O2 -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX -I$REMOTE_DIR -o $REMOTE_DIR/$obj $REMOTE_DIR/$f && "
done

LINK_CMD="/usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-gcc $OBJ_FILES -o $REMOTE_DIR/c_render -lEGL -lGLESv2 -lsocket -lm"

sshpass -p "root" ssh $SSH_OPTS root@$QNX_VM \
    "export QNX_HOST=/usr/qnx650/host/qnx6/x86; \
     export QNX_TARGET=/usr/qnx650/target/qnx6; \
     cd $REMOTE_DIR && \
     $COMPILE_CMD \
     $LINK_CMD && \
     ls -lh $REMOTE_DIR/c_render"

echo "Downloading compiled binary..."
sshpass -p "root" scp $SSH_OPTS root@$QNX_VM:$REMOTE_DIR/c_render "$OUT"

echo ""
echo "Compiled: $OUT"
ls -lh "$OUT"
