#!/bin/sh
# Builds build/mod.elf: big-endian MIPS II (O32), linked at 0x81000000, the format the program's mod runtime loads.
# CLANG and LLD name clang and ld.lld with the MIPS target (LLVM 18.1.8 was used for 1.0.0; default: the ones on PATH).
set -e
cd "$(dirname "$0")"
CC=${CLANG:-clang}
LD=${LLD:-ld.lld}
OUT=${OUT:-build}
mkdir -p "$OUT"
$CC -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division -fomit-frame-pointer \
    -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset -nostdinc -Iinclude -Wall -Wextra \
    -Wno-unused-parameter -Wno-unsupported-floating-point-opt -Werror=section $EXTRA -c src/first_person.c -o "$OUT/first_person.o"
$LD "$OUT/first_person.o" -nostdlib -T mod.ld -Map "$OUT/mod.map" --unresolved-symbols=ignore-all --emit-relocs -e 0 \
    --no-nmagic -o "$OUT/mod.elf"
ls -l "$OUT/mod.elf"
