#!/usr/bin/env bash
# Executes this standalone graph in an isolated QNX guest. Never uses the unit.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
[[ $# -ge 3 && $# -le 4 ]] || { echo "usage: $0 COPIED_RENDERER_DIR ASSETS_DIRECTORY NEW_OUTPUT_DIRECTORY [DOCKER_IMAGE]" >&2; exit 2; }
COPIES="$(cd "$1" && pwd)";ASSETS="$(cd "$2" && pwd)";OUT="$3";IMAGE="${4:-qnx65-armv7-toolchain:latest}"
[[ ! -e "$OUT" ]] || { echo "preserve previous guest: $OUT" >&2; exit 1; }
for f in arm_fixture.bin unified_float.bin unified_int8.bin manifest.json;do [[ -f "$ASSETS/$f" ]];done
SEARCH="$ROOT";QEMU_ASSETS=""
while [[ "$SEARCH" != / ]];do
    if [[ -d "$SEARCH/Tools/qnx-65-sdp-docker/tests/qemu" ]];then QEMU_ASSETS="$SEARCH/Tools/qnx-65-sdp-docker/tests/qemu";break;fi
    SEARCH="$(dirname "$SEARCH")"
done
[[ -n "$QEMU_ASSETS" ]] || { echo "QNX guest boot assets absent" >&2; exit 1; }
command -v qemu-system-arm >/dev/null
docker image inspect "$IMAGE" >/dev/null
mkdir -p "$OUT";OUT="$(cd "$OUT" && pwd)"
cp "$QEMU_ASSETS/startup-virt" "$QEMU_ASSETS/libstartup.a" "$QEMU_ASSETS/devc-serdebug" "$OUT/"
cp "$ASSETS/arm_fixture.bin" "$ASSETS/unified_float.bin" "$ASSETS/unified_int8.bin" "$ASSETS/manifest.json" "$OUT/"
. "$ROOT/scripts/apac_flags.sh"
docker image inspect "$IMAGE" --format '{{.Id}}' > "$OUT/docker-image-id.txt"
shasum -a 256 "$HERE/spectral_chain_arm_check.c" "$HERE/spectral_chain_cli.c" "$ROOT/tools/stereo-upmix-render/unified_spectral_runtime_v1/"{unified_runtime,unified_audio_stream}.{c,h} "$COPIES/"{stereo_upmix,hssr_dsp,hssr_neon,hssr_mask}.c > "$OUT/source-sha256.txt"
docker run --rm --platform=linux/amd64 -v "$ROOT":/src:ro -v "$COPIES":/copies:ro -v "$OUT":/w "$IMAGE" bash -c '
set -euo pipefail
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86
export QNX_TARGET=/opt/qnx650/target/qnx6
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
$CC --version > /w/compiler-version.txt
FLAGS="'"$APAC_CFLAGS_NOINC $UPMIX_DSP_CFLAGS"' -Werror -mfpu=neon -mfloat-abi=softfp -DSPA_HSSR_TEST_MASK -DSPA_HSSR_TEST_DIRECT_NO_BED -I/src/hook/audio -I/src/tools/stereo-upmix-render/unified_spectral_runtime_v1"
echo "$FLAGS" > /w/flags.txt
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_spectral_chain_v1/spectral_chain_arm_check.c -o /w/checker.o
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_spectral_runtime_v1/unified_runtime.c -o /w/runtime.o
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_spectral_runtime_v1/unified_audio_stream.c -o /w/stream.o
for f in stereo_upmix hssr_dsp hssr_neon hssr_mask;do
    $CC $FLAGS -Wno-unused-function -Wno-unused-parameter -c /copies/$f.c -o /w/$f.o
done
$CC $FLAGS -c /src/hook/audio/soundcheck.c -o /w/soundcheck.o
$CC /w/checker.o /w/runtime.o /w/stream.o /w/stereo_upmix.o /w/hssr_dsp.o /w/hssr_neon.o /w/hssr_mask.o /w/soundcheck.o -pthread -lm -o /w/umx_arm_check
arm-unknown-nto-qnx6.5.0eabi-objdump -d /w/umx_arm_check > /w/disassembly.txt
TARGET=/opt/qnx650/target/qnx6/armle-v7
cp "$TARGET/lib/libc.so.3" "$TARGET/lib/libm.so.2" "$TARGET/boot/sys/procnto-smp" /w/
'
cat > "$OUT/check.build" <<'IFS'
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
    display_msg "@@SPECTRAL-CHAIN-ARM-CHECK-START@@"
    /proc/boot/umx_arm_check /proc/boot/unified_float.bin /proc/boot/unified_int8.bin /proc/boot/arm_fixture.bin
    display_msg "@@SPECTRAL-CHAIN-ARM-CHECK-END@@"
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[perms=+r,+x]
libc.so.3
libm.so.2
procnto-smp
devc-serdebug
umx_arm_check
unified_float.bin
unified_int8.bin
arm_fixture.bin
IFS
docker run --rm --platform=linux/amd64 -v "$OUT":/w -w /w "$IMAGE" bash -c '
set -e
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86
export QNX_TARGET=/opt/qnx650/target/qnx6
TARGET=/opt/qnx650/target/qnx6/armle-v7
export MKIFS_PATH=/w:$TARGET/boot/sys:$TARGET/bin:$TARGET/lib:$TARGET/usr/lib
mkifs check.build check.ifs
' > "$OUT/mkifs.log" 2>&1
shasum -a 256 "$OUT/umx_arm_check" "$OUT/check.ifs" > "$OUT/binary-sha256.txt"
QEMU_PID=""
cleanup(){ if [[ -n "$QEMU_PID" ]];then kill "$QEMU_PID" 2>/dev/null || true;wait "$QEMU_PID" 2>/dev/null || true;fi;}
trap cleanup EXIT INT TERM
qemu-system-arm -M virt -m 512 -cpu cortex-a15 -accel tcg,thread=single -icount shift=0 \
    -device loader,file="$OUT/check.ifs",addr=0x40200000,force-raw=on \
    -device loader,addr=0x40200000,cpu-num=0 -display none -serial stdio -monitor none \
    </dev/null > "$OUT/qemu.log" 2>&1 &
QEMU_PID=$!;START=$SECONDS
while kill -0 "$QEMU_PID" 2>/dev/null;do
    if rg -q '@@SPECTRAL-CHAIN-ARM-CHECK-END@@' "$OUT/qemu.log";then break;fi
    if ((SECONDS-START>900));then echo "guest numerical check timed out; log retained" >&2;exit 1;fi
    sleep 1
done
cleanup;QEMU_PID=""
rg '@@SPECTRAL-CHAIN-ARM-CHECK|SPECTRAL_CHAIN_ARM' "$OUT/qemu.log"
python3 - "$OUT" <<'PY'
import hashlib,json,sys
from pathlib import Path
out=Path(sys.argv[1]);log=(out/'qemu.log').read_text(errors='replace')
rows=[json.loads(line.split('SPECTRAL_CHAIN_ARM_CHECK ',1)[1]) for line in log.splitlines() if 'SPECTRAL_CHAIN_ARM_CHECK ' in line]
passed='@@SPECTRAL-CHAIN-ARM-CHECK-END@@' in log and len(rows)==2 and all(row['passed'] and row['execution']=='qnx-arm' for row in rows)
result=dict(complete=True,passed=passed,scope='QEMU Cortex-A15/QNX full causal FFT/one spectral int8 or float model/WOLA/NN-free C arithmetic; no Audi/Krait timing, quantized broad acoustic or loaded deadline proof',
    arms=rows,fixture=json.loads((out/'manifest.json').read_bytes()),compiler=(out/'compiler-version.txt').read_text(),flags=(out/'flags.txt').read_text(),
    docker_image_id=(out/'docker-image-id.txt').read_text().strip(),source_identity=(out/'source-sha256.txt').read_text(),
    executable_sha256=hashlib.sha256((out/'umx_arm_check').read_bytes()).hexdigest(),boot_image_sha256=hashlib.sha256((out/'check.ifs').read_bytes()).hexdigest(),
    log_sha256=hashlib.sha256((out/'qemu.log').read_bytes()).hexdigest())
(out/'complete.json').write_text(json.dumps(result,indent=2)+'\n')
if not passed:raise SystemExit('ARM execution parity failed; artifacts preserved')
PY
