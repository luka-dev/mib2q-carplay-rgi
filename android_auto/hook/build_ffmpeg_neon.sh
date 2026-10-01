#!/bin/sh
# NEON FFmpeg (H.264 decode only) for the Qualcomm APQ8064 (Krait, ARMv7 + NEON), next to the plain-C
# build in /cache/ffmpeg. Car log 2026-09-29 (live run 5): the plain-C decoder managed 12-18 fps on a
# moving 1280x720 cluster map (30 fps in), backlog up to 15.7 MB. Same pinned source as build_ffmpeg.sh.
set -eu
ROOT=/cache
cd "$ROOT"
SHA=b8c8e926b948c14df1264cd0beac1c773df9170ac9cac97bdf1275cd3d385902
echo "$SHA  ffmpeg-6.1.5.tar.gz" | sha256sum -c -
if [ -f ffmpeg-neon/.sq5-built ]; then exit 0; fi
rm -rf ffmpeg-neon; mkdir -p ffmpeg-neon
tar xf ffmpeg-6.1.5.tar.gz -C ffmpeg-neon --strip-components=1
# The QNX 6.5 headers need '-include stddef.h' for C, but the same flag breaks every .S file (the
# assembler sees C typedefs), which is why the first build used --disable-asm. Add it for C only.
mkdir -p "$ROOT/bin"
cat > "$ROOT/bin/qnx-cc" <<'EOF'
#!/bin/sh
for a in "$@"; do case "$a" in *.S|*.s) exec arm-unknown-nto-qnx6.5.0eabi-gcc "$@";; esac; done
exec arm-unknown-nto-qnx6.5.0eabi-gcc -include stddef.h "$@"
EOF
chmod 755 "$ROOT/bin/qnx-cc"
cd ffmpeg-neon
./configure --enable-cross-compile --arch=arm --cpu=armv7-a --target-os=qnx \
    --cc="$ROOT/bin/qnx-cc" --ld=arm-unknown-nto-qnx6.5.0eabi-gcc \
    --as=arm-unknown-nto-qnx6.5.0eabi-gcc \
    --ar=arm-unknown-nto-qnx6.5.0eabi-ar --ranlib=arm-unknown-nto-qnx6.5.0eabi-ranlib \
    --enable-neon --disable-debug --disable-doc --disable-programs \
    --disable-autodetect --disable-network --disable-avdevice --disable-avformat \
    --disable-avfilter --disable-swscale --disable-swresample --disable-postproc \
    --disable-everything --enable-decoder=h264 --enable-parser=h264 \
    --extra-cflags='-D_QNX_SOURCE -march=armv7-a -mfloat-abi=softfp -mfpu=neon' \
    --extra-libs=-lsocket
grep -E '^#define HAVE_NEON |^#define HAVE_ARMV7 ' config.h
make -j4
touch .sq5-built
