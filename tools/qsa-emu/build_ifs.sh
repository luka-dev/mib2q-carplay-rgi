#!/bin/bash
# Build the audio-stack IFS locally in the qnx65 Docker toolchain (no SDP VM).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
A=/Users/luka/Desktop/AUDI_2
BSP=$A/Tools/bsp-qemu-arm/extract
FW=$A/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted
OUT=$ROOT/build/qsa-emu
mkdir -p "$OUT"

# Rebuild the three task-owned binaries so this script is the complete build, not
# merely an IFS packer around whatever happened to be left in build/qsa-emu.
docker run --rm --platform=linux/amd64 -v "$ROOT":/src -w /src \
  qnx65-armv7-toolchain:latest bash -c '
    export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
    export QNX_HOST=/opt/qnx650/host/linux/x86
    export QNX_TARGET=/opt/qnx650/target/qnx6
    qcc -O2 -Wall -Wextra -o build/qsa-emu/qsa_probe \
      tools/qsa-emu/qsa_probe.c -lasound
    qcc -shared -fPIC -O2 -Wall -Wextra \
      -Wl,-soname,libcsdIpcClient.so \
      -o build/qsa-emu/libcsdIpcClient.so tools/qsa-emu/csd_stub.c
    qcc -shared -fPIC -O2 -Wall -Wextra -Wl,-soname,libtap.so \
      -o build/qsa-emu/libtap.so tools/qsa-emu/tap_preload.c
  '

# mkifs wants plain paths; stage everything into one directory so the recipe has
# no host-specific layout baked into it.
STAGE=$OUT/stage; rm -rf "$STAGE"; mkdir -p "$STAGE"
cp "$BSP/libc.so.3" "$BSP/devc-serdebug" "$BSP/waitfor" "$BSP/slogger" \
   "$BSP/procnto-smp" "$STAGE/"
# The BSP's extracted startup-virt lost its ELF header to dumpifs, so mkifs cannot
# identify it.  The emu harness keeps a properly linked one; that is the bootable
# image its own QNX guests already run.
cp "$A/Tools/qnx-carplay-emu/build/re/startup-virt" "$STAGE/"
cp "$FW/app/armle/sbin/io-audio" "$STAGE/"
# Use the PATCHED driver by default -- that is what the car runs, and the stock
# one silently reports max_voices=2 while QSA hides a channel conversion behind a
# setup readback that claims six.  QSA_STOCK_DEVA=1 selects the firmware original
# for an A/B.
if [ "${QSA_STOCK_DEVA:-0}" = "1" ]; then
  cp "$FW/app/armle/lib/dll/deva-ctrl-qc.so" "$STAGE/"
  echo ">> using STOCK deva"
else
  cp "$ROOT/deploy/deva-ctrl-qc/deva-ctrl-qc.so" "$STAGE/"
  echo ">> using PATCHED deva"
fi
cp "$FW/ifs2/ifs_coreservices3/lib/libasound.so.2" "$STAGE/"
# Exactly one second of the known-good six-channel reference. qsa_probe consumes
# 62 x 768 frames (0.992 s); the small tail guarantees the final fread is full.
ffmpeg -nostdin -v error -y \
  -i "$ROOT/captures/broken_5_1_20260901_drive/session6_5_1_declared_order.wav" \
  -t 1 -map 0:a:0 -acodec pcm_s16le -f s16le "$STAGE/music6.s16"
cp "$OUT/libcsdIpcClient.so" "$OUT/devstub" "$OUT/qsa_probe" "$OUT/libtap.so" "$STAGE/"
# The firmware was unpacked without the execute bit and mkifs copies the host
# file mode straight into the image, so procnto refuses to exec them.
chmod 755 "$STAGE"/*

sed -e "s#BSP/#/stage/#g" -e "s#OUT/#/stage/#g" \
    -e "s#FW/app/armle/sbin/#/stage/#g" \
    -e "s#FW/app/armle/lib/dll/#/stage/#g" \
    -e "s#FW/ifs2/ifs_coreservices3/lib/#/stage/#g" \
    -e "s#SDP/#/opt/qnx650/target/qnx6/armle-v7/#g" \
    -e "s#PROBE_VOICES#${QSA_PROBE_VOICES:-6}#g" \
    -e "s#^    startup-virt #    /stage/startup-virt #" \
    -e "s#^    procnto-smp#    /stage/procnto-smp#" \
    "$HERE/qsa.build" > "$STAGE/qsa.build"

docker run --rm --platform=linux/amd64 -v "$OUT":/out -v "$STAGE":/stage \
  qnx65-armv7-toolchain:latest bash -c '
    export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
    export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
    mkifs -v /stage/qsa.build /out/qsa.bin 2>&1 | tail -20
    ls -l /out/qsa.bin'
echo ">> $OUT/qsa.bin"
