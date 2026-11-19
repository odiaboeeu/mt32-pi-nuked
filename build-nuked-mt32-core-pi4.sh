#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$ROOT/external/Nuked-MT32"
OUT="$ROOT/build-nuked-mt32"

CXX="${CXX:-aarch64-none-elf-g++}"
AR="${AR:-aarch64-none-elf-ar}"
RANLIB="${RANLIB:-aarch64-none-elf-ranlib}"
NM="${NM:-aarch64-none-elf-nm}"
SIZE="${SIZE:-aarch64-none-elf-size}"

COMMON_FLAGS=(
    -std=gnu++17
    -O2
    -mcpu=cortex-a72
    -ffreestanding
    -DNUKED_MT32_BAREMETAL
    -fno-exceptions
    -fno-rtti
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-unused-parameter
    -Wno-missing-braces
    -Wno-unused-function
    -Wno-sign-compare
    -I"$SRC"
    -I"$SRC/munt"
)

SOURCES=(
    mt32.cpp
    la32.cpp
    rv2.cpp
    lcd.cpp
    mame/mcs96.cpp
    mame/i8x9x.cpp
)

rm -rf "$OUT"
mkdir -p "$OUT"

objects=()

for source in "${SOURCES[@]}"
do
    object_name="${source//\//_}"
    object_name="${object_name%.cpp}.o"
    object="$OUT/$object_name"

    echo "CXX $source"

    "$CXX" "${COMMON_FLAGS[@]}" \
        -c "$SRC/$source" \
        -o "$object"

    objects+=("$object")
done

archive="$OUT/libnukedmt32_core.a"

echo "AR  $archive"
"$AR" rcs "$archive" "${objects[@]}"
"$RANLIB" "$archive"

echo
echo "Archive:"
ls -l "$archive"

echo
echo "Objects:"
"$AR" t "$archive"

echo
echo "Sizes:"
"$SIZE" "${objects[@]}"

echo
echo "Undefined symbols:"
"$NM" -u "$archive" | sort -u

echo
echo "OK: Nuked-MT32 bare-metal library built successfully."
