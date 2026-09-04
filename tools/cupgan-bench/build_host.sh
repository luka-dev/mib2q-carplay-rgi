#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
OUT="$ROOT/build/cupgan"

mkdir -p "$OUT"
${CC:-cc} -std=c99 -O3 -DNDEBUG -Wall -Wextra -Werror -pedantic \
    "$HERE/cupgan_bench.c" -o "$OUT/cupgan_bench_host" -lm
printf 'built %s\n' "$OUT/cupgan_bench_host"
