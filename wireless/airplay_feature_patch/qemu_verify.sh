#!/bin/sh
# Boot a minimal QNX 6.5 IFS twice under QEMU and compare stock/patched features.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
SDK="$ROOT/Tools/qnx-65-sdp-docker"
APP="$ROOT/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg"
IFS="$ROOT/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/ifs2/ifs_display/proc/boot"
FIRMWARE_LIBC="$ROOT/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app/armle/lib/libc.so.3"
STOCK="$APP/eso/lib/libairplay.so"
PATCHED="$HERE/out/libairplay.feature37.offline.so"
IMG=qnx65-armv7-toolchain:latest
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

python3 "$HERE/build_offline.py" >/dev/null
cp "$SDK/tests/qemu/startup-virt" "$SDK/tests/qemu/libstartup.a" \
   "$SDK/tests/qemu/devc-serdebug" "$WORK/"
cp "$APP/armle/lib/libOmxCore.so" "$APP/armle/lib/libomxctx.so.1" \
   "$APP/armle/lib/libOmxBase.so" "$APP/armle/usr/lib/libdns_sd.so.1" \
   "$APP/armle/usr/lib/libaoi.so.1" \
   "$IFS/libOSAbstraction.so" "$IFS/libscreen.so.1" \
   "$IFS/liblibstd.so" "$IFS/libcommonUtils.so" "$WORK/"
cp "$HERE/qemu_probe.c" "$WORK/"

cat > "$WORK/test.build" <<'EOF'
[linker="(40;1;)qcc -bootstrap -nostdlib -Wl,--no-keep-memory -Vgcc_ntoarmv7%(m!=3,%(m!=6,%(e==0, -EL%)%(e==1, -EB%)%)%)%(h!=0, -Wl,-Ttext -Wl,0x%t%)%(d!=0, -Wl,-Tdata -Wl,0x%d%) -o%o %i %[M -L%^i -Wl,-uinit_%n -lmod_%n%] /w/libstartup.a -L/opt/qnx650/target/qnx6/armle-v7/lib -L/opt/qnx650/target/qnx6/armle-v7/usr/lib -llzo -lucl -ldrvr"]
[linker="(40;2;)cp %i %o"]
[image=0x40200000]
[virtual=armle-v7,raw] .bootstrap = {
    startup-virt -v -S
    PATH=:/proc/boot LD_LIBRARY_PATH=:/proc/boot procnto-smp -vvv
}
[+script] .script = {
    procmgr_symlink ../../proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    devc-serdebug -e -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    display_msg "@@FEATURE-START@@"
    qemu_probe
    display_msg "@@FEATURE-END@@"
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[perms=+r,+x]
libc.so.3
libm.so.2
libsocket.so.3
libbacktrace.so.1
libasound.so.2
libecpp-ne.so.4
procnto-smp
devc-serdebug
qemu_probe
libairplay.so
libomxctx.so.1
libOmxBase.so
libOmxCore.so
libOSAbstraction.so
libscreen.so.1
libdns_sd.so.1
libaoi.so.1
liblibstd.so
libcommonUtils.so
EOF

docker run --rm --platform=linux/amd64 -v "$WORK":/w -w /w "$IMG" sh -c '
set -e
T=/opt/qnx650/target/qnx6/armle-v7
cp $T/lib/libc.so.3 $T/lib/libm.so.2 $T/lib/libsocket.so.3 \
   $T/lib/libasound.so.2 $T/lib/libecpp-ne.so.4 \
   $T/usr/lib/libbacktrace.so.1 $T/boot/sys/procnto-smp /w/
qcc -Vgcc_ntoarmv7 -Wl,-E -o qemu_probe qemu_probe.c
' > "$WORK/compile.log" 2>&1 || { cat "$WORK/compile.log" >&2; exit 1; }
cp "$FIRMWARE_LIBC" "$WORK/libc.so.3"

for variant in stock patched; do
    if [ "$variant" = stock ]; then cp "$STOCK" "$WORK/libairplay.so";
    else cp "$PATCHED" "$WORK/libairplay.so"; fi
    # No AirPlay server exists in this probe. Bypass only its property query,
    # identically for both variants; the deliverable binary is untouched.
    python3 - "$WORK/libairplay.so" <<'PY'
import pathlib
import sys
path = pathlib.Path(sys.argv[1])
data = bytearray(path.read_bytes())
offset = 0x20D6C
old = bytes.fromhex("d3e6ffeb")  # BL CFObjectGetPropertyInt64Sync
new = bytes.fromhex("0000a0e3")  # MOV R0,#0; R1 is already zero
if data[offset:offset + 4] != old:
    raise SystemExit("unexpected platform getter call bytes")
data[offset:offset + 4] = new
path.write_bytes(data)
PY
    docker run --rm --platform=linux/amd64 -v "$WORK":/w -w /w "$IMG" sh -c '
export MKIFS_PATH=/w:/opt/qnx650/target/qnx6/armle-v7/boot/sys:/opt/qnx650/target/qnx6/armle-v7/bin:/opt/qnx650/target/qnx6/armle-v7/lib:/opt/qnx650/target/qnx6/armle-v7/usr/lib
mkifs test.build ifs.bin' > "$WORK/mkifs.log" 2>&1 || { tail -20 "$WORK/mkifs.log" >&2; exit 1; }
    mkdir -p "$WORK/fat"
    mv "$WORK/ifs.bin" "$WORK/fat/"
    ( printf '\n'; sleep 4; printf 'virtio scan\n'; sleep 2;
      printf 'fatload virtio 0:1 0x40200000 ifs.bin\n'; sleep 3;
      printf 'go 0x40200000\n'; sleep 18 ) |
      timeout 37 qemu-system-arm -M virt -m 256 -cpu cortex-a15 \
        -bios "$SDK/tests/qemu/u-boot.bin" \
        -drive file=fat:rw:"$WORK/fat",format=raw,media=disk \
        -display none -serial stdio > "$WORK/$variant.log" 2>&1 || true
    echo "== $variant =="
    sed -n '/@@FEATURE-START@@/,/@@FEATURE-END@@/p' "$WORK/$variant.log" | head -20
    if ! grep -q '@@FEATURE-END@@' "$WORK/$variant.log"; then
        echo "$variant probe did not return; QEMU log follows" >&2
        tail -30 "$WORK/$variant.log" >&2
        exit 1
    fi
    if ! grep -q 'AirPlayGetFeatures=0x' "$WORK/$variant.log"; then
        echo "$variant library did not return features" >&2
        exit 1
    fi
    if ! grep -q 'Process .* (qemu_probe) exited status=0' "$WORK/$variant.log"; then
        echo "$variant probe did not exit successfully" >&2
        exit 1
    fi
done
grep -q 'AirPlayGetFeatures=0x0000000144440b80 bit37=0' "$WORK/stock.log"
grep -q 'AirPlayGetFeatures=0x0000002144440b80 bit37=1' "$WORK/patched.log"
echo 'QEMU comparison passed: only feature bit 37 changed.'
