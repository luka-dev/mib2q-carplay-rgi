#!/bin/sh
# Isolated build cache inside the QNX toolchain container. No existing project outputs.
set -eu
ROOT=/cache
mkdir -p "$ROOT"
cd "$ROOT"
SHA=b8c8e926b948c14df1264cd0beac1c773df9170ac9cac97bdf1275cd3d385902
if [ ! -f ffmpeg-6.1.5.tar.gz ]; then
    curl -fL --retry 3 https://ffmpeg.org/releases/ffmpeg-6.1.5.tar.gz -o ffmpeg-6.1.5.tar.gz.part
    mv ffmpeg-6.1.5.tar.gz.part ffmpeg-6.1.5.tar.gz
fi
echo "$SHA  ffmpeg-6.1.5.tar.gz" | sha256sum -c -
if [ -f ffmpeg/.sq5-built ]; then exit 0; fi
mkdir -p ffmpeg
tar xf ffmpeg-6.1.5.tar.gz -C ffmpeg --strip-components=1
cd ffmpeg
./configure --enable-cross-compile --arch=arm --target-os=qnx \
    --cc=arm-unknown-nto-qnx6.5.0eabi-gcc --ld=arm-unknown-nto-qnx6.5.0eabi-gcc \
    --ar=arm-unknown-nto-qnx6.5.0eabi-ar --ranlib=arm-unknown-nto-qnx6.5.0eabi-ranlib \
    --disable-asm --disable-debug --disable-doc --disable-programs \
    --disable-autodetect --disable-network --disable-avdevice --disable-avformat \
    --disable-avfilter --disable-swscale --disable-swresample --disable-postproc \
    --disable-everything --enable-decoder=h264 --enable-parser=h264 \
    --extra-cflags='-D_QNX_SOURCE -include stddef.h -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16' \
    --extra-libs=-lsocket
make -j4
touch .sq5-built
