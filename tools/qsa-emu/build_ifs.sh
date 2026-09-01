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
cp "$FW/app/armle/lib/dll/deva-ctrl-qc.so" "$STAGE/"
cp "$FW/ifs2/ifs_coreservices3/lib/libasound.so.2" "$STAGE/"
cp "$OUT/libcsdIpcClient.so" "$OUT/devstub" "$OUT/qsa_probe" "$STAGE/"
# The firmware was unpacked without the execute bit and mkifs copies the host
# file mode straight into the image, so procnto refuses to exec them.
chmod 755 "$STAGE"/*

sed -e "s#BSP/#/stage/#g" -e "s#OUT/#/stage/#g" \
    -e "s#FW/app/armle/sbin/#/stage/#g" \
    -e "s#FW/app/armle/lib/dll/#/stage/#g" \
    -e "s#FW/ifs2/ifs_coreservices3/lib/#/stage/#g" \
    -e "s#SDP/#/opt/qnx650/target/qnx6/armle-v7/#g" \
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
