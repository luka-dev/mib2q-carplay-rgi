#!/usr/bin/env bash
set -euo pipefail

IMAGE=${QNX_IMAGE:-qnx65-armv7-toolchain:8.5}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
# QNX 6.5's 32-bit ar can fail with EOVERFLOW on Windows/NTFS bind mounts. Keep the
# disposable FFmpeg object tree on the Linux filesystem; final artifacts still land in build/.
CACHE=${AA_BUILD_CACHE:-/tmp/mib2q-aa-build-cache}

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "ERROR: Docker image '$IMAGE' is not available." >&2
    echo "Build it from https://github.com/luka-dev/qnx65-armv7-toolchain first." >&2
    exit 1
fi

mkdir -p "$CACHE" "$ROOT/build/android_auto"
FFMPEG_ARCHIVE=$CACHE/ffmpeg-6.1.5.tar.gz
FFMPEG_SHA=b8c8e926b948c14df1264cd0beac1c773df9170ac9cac97bdf1275cd3d385902
if [ ! -f "$FFMPEG_ARCHIVE" ]; then
    echo "Downloading pinned FFmpeg 6.1.5 source..."
    curl --fail --location --retry 3 --output "$FFMPEG_ARCHIVE.new" \
        https://ffmpeg.org/releases/ffmpeg-6.1.5.tar.gz
    echo "$FFMPEG_SHA  $FFMPEG_ARCHIVE.new" | sha256sum -c -
    mv "$FFMPEG_ARCHIVE.new" "$FFMPEG_ARCHIVE"
else
    echo "$FFMPEG_SHA  $FFMPEG_ARCHIVE" | sha256sum -c -
fi
docker run --rm --platform=linux/amd64 \
    -v "$ROOT":/src \
    -v "$CACHE":/cache \
    "$IMAGE" bash -lc '
        set -e
        export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
        export QNX_HOST=/opt/qnx650/host/linux/x86
        export QNX_TARGET=/opt/qnx650/target/qnx6
        sh /src/android_auto/hook/build_ffmpeg_neon.sh
        sh /src/android_auto/hook/build_in_container.sh
    '

echo "Android Auto native outputs: $ROOT/build/android_auto"
