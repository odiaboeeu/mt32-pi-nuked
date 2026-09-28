#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SC88HOME="${SC88HOME:-$ROOT/external/gearmulator-sc88}"
OUT="$ROOT/build-gearmulator-sc88"

CXX="${CXX:-aarch64-none-elf-g++}"
AR="${AR:-aarch64-none-elf-ar}"
RANLIB="${RANLIB:-aarch64-none-elf-ranlib}"
NM="${NM:-aarch64-none-elf-nm}"
SIZE="${SIZE:-aarch64-none-elf-size}"

if [ ! -d "$SC88HOME" ]; then
    echo "ERROR: Gearmulator checkout not found: $SC88HOME" >&2
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT/obj"

COMMON_FLAGS=(
    -std=gnu++20
    -Ofast
    -DNDEBUG
    -DRELEASE
    -DCHIPS_FORCE_NO_JIT
    -DSC88_HEADLESS_NO_EXCEPTIONS
    -mcpu=cortex-a72
    -mlittle-endian
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-stack-protector
    -ffunction-sections
    -fdata-sections
    -fomit-frame-pointer
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-sign-compare
)

INCLUDES=(
    -I"$SC88HOME/sc88-headless-core"
    -I"$SC88HOME/source"
    -I"$SC88HOME/source/cpu"
    -I"$SC88HOME/source/ronaldo"
    -I"$SC88HOME/source/ronaldo/88emu"
    -I"$SC88HOME/source/ronaldo/custom_chips"
    -I"$SC88HOME/source/ronaldo/custom_chips/xp"
    -I"$SC88HOME/source/framework"
    -I"$ROOT/external/circle-stdlib/install/aarch64-none-circle/include"
)

SOURCES=(
    sc88-headless-core/sc88_headless_core.cpp
    sc88-headless-core/sc88_wave_rom.cpp

    source/ronaldo/88emu/88lib/boards/sc88.cpp
    source/ronaldo/88emu/88lib/mcu/sc88_submcu.cpp

    source/framework/hardwareLib/hd44780.cpp
    source/framework/synthLib/midiBufferParser.cpp
    source/framework/synthLib/midiRateLimiter.cpp

    source/cpu/h8500/adc.cpp
    source/cpu/h8500/chip.cpp
    source/cpu/h8500/cpu.cpp
    source/cpu/h8500/dtc.cpp
    source/cpu/h8500/exec.cpp
    source/cpu/h8500/frt.cpp
    source/cpu/h8500/h8532_regs.cpp
    source/cpu/h8500/h8570_regs.cpp
    source/cpu/h8500/intc.cpp
    source/cpu/h8500/isp_regs.cpp
    source/cpu/h8500/ports.cpp
    source/cpu/h8500/pwm.cpp
    source/cpu/h8500/sci.cpp
    source/cpu/h8500/timing.cpp
    source/cpu/h8500/tmr.cpp
    source/cpu/h8500/wdt.cpp

    source/ronaldo/custom_chips/xp/xp.cpp
    source/ronaldo/custom_chips/xp/xp_dsp.cpp
    source/ronaldo/custom_chips/xp/xp_dsp_naive.cpp
    source/ronaldo/custom_chips/xp/xp_dsp_program.cpp
    source/ronaldo/custom_chips/xp/xp_host.cpp
    source/ronaldo/custom_chips/xp/xp_voice.cpp
)

OBJECTS=()

for source in "${SOURCES[@]}"; do
    object_name="${source//\//_}.o"
    object="$OUT/obj/$object_name"

    echo "SC88 CXX $source"

    "$CXX" \
        "${COMMON_FLAGS[@]}" \
        "${INCLUDES[@]}" \
        -c "$SC88HOME/$source" \
        -o "$object"

    OBJECTS+=("$object")
done

ARCHIVE="$OUT/libgearmulator_sc88_core.a"

echo "SC88 AR  $ARCHIVE"

"$AR" rcs "$ARCHIVE" "${OBJECTS[@]}"
"$RANLIB" "$ARCHIVE"

"$AR" t "$ARCHIVE" > "$OUT/archive-members.txt"
"$NM" -u "$ARCHIVE" | sort -u > "$OUT/undefined-symbols.txt"
"$NM" -C "$ARCHIVE" > "$OUT/all-symbols.txt"
"$SIZE" "${OBJECTS[@]}" > "$OUT/object-sizes.txt"

echo
echo "Archive:"
ls -lh "$ARCHIVE"

echo
echo "Archive members:"
cat "$OUT/archive-members.txt"

echo
echo "Public SC-88 API:"
"$NM" -g --defined-only "$ARCHIVE" |
    grep -E 'sc88_headless_' |
    sort

echo
echo "Checks:"

if "$NM" -u "$ARCHIVE" |
    grep -qE '__cxa_throw|__cxa_begin_catch|__gxx_personality_v0'
then
    echo "SC88_NO_EXCEPTION_RUNTIME=FAIL"
    exit 1
else
    echo "SC88_NO_EXCEPTION_RUNTIME=PASS"
fi

if "$NM" -u "$ARCHIVE" | grep -q '__dynamic_cast'
then
    echo "SC88_NO_RTTI=FAIL"
    exit 1
else
    echo "SC88_NO_RTTI=PASS"
fi

if "$NM" -C "$ARCHIVE" | grep -q 'DspJitDispatcher'
then
    echo "SC88_NO_JIT_DISPATCHER=FAIL"
    exit 1
else
    echo "SC88_NO_JIT_DISPATCHER=PASS"
fi

if "$NM" -C "$ARCHIVE" |
    grep -qE 'std::thread|std::mutex|std::condition_variable'
then
    echo "SC88_NO_CPP_THREADS=FAIL"
    exit 1
else
    echo "SC88_NO_CPP_THREADS=PASS"
fi

if "$NM" -C "$ARCHIVE" |
    grep -qE 'HardwareDevice|RomLoader|RomScanner|RomInventory|Sc55Board|Sc8820|Sc8850|Sc88Pro'
then
    echo "SC88_NO_UNRELATED_88EMU=FAIL"
    exit 1
else
    echo "SC88_NO_UNRELATED_88EMU=PASS"
fi

EXPECTED_OBJECTS="${#SOURCES[@]}"
ACTUAL_OBJECTS="$("$AR" t "$ARCHIVE" | wc -l)"

echo "expected_objects=$EXPECTED_OBJECTS"
echo "actual_objects=$ACTUAL_OBJECTS"

if [ "$EXPECTED_OBJECTS" -ne "$ACTUAL_OBJECTS" ]; then
    echo "SC88_ARCHIVE_OBJECT_COUNT=FAIL"
    exit 1
else
    echo "SC88_ARCHIVE_OBJECT_COUNT=PASS"
fi

echo
echo "OK: Gearmulator SC-88 bare-metal library built successfully."
