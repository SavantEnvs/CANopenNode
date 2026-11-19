#!/usr/bin/env bash
#
# mayhem/build.sh — build the CANopenNode fuzz harnesses and the stack self-test.
#
# CANopenNode has no build system of its own beyond example/Makefile (a plain compile of a source
# list for a blank CAN driver), so this compiles the stack sources directly, as that Makefile does:
#   - the whole stack (301/303/305/309, CANopen.c, the example OD) with $SANITIZER_FLAGS $DEBUG_FLAGS,
#     plus the harness runtime mayhem/harnesses/co_fuzz.c (host CAN driver + stack lifecycle), and
#     one libFuzzer binary and one -standalone reproducer per harness, at /mayhem/<harness>;
#   - the same sources again with normal flags into mayhem/out/test/co_selftest (run by test.sh).
# mayhem/harnesses/CO_driver_target.h replaces example/CO_driver_target.h (harness dir first on the
# include path); it carries the stack configuration that enables the gateway, SDO client/server
# block transfers, LSS master/slave and the other bus-facing modules.
# All objects go to mayhem/out/ (gitignored). Re-runnable, no network.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
H=mayhem/harnesses
OUT=mayhem/out
INC=(-I"$H" -I. -Iexample)
STACK=(301/*.c 303/*.c 305/*.c 309/*.c CANopen.c example/OD.c "$H/co_fuzz.c")
HARNESSES=(fuzz_gateway_ascii fuzz_can_node fuzz_sdo_client fuzz_lss_master)

# compile <objdir> <flags...> -- <sources...>: one object per source, in parallel.
compile() {
  local dir="$1"; shift
  local flags=()
  while [ "$1" != "--" ]; do flags+=("$1"); shift; done
  shift
  mkdir -p "$dir"
  printf '%s\n' "$@" | xargs -P "$MAYHEM_JOBS" -I{} sh -c \
    'src="$1"; shift; obj="$0/s-$(echo "$src" | tr / _).o"; exec "$CC" "$@" -c "$src" -o "$obj"' \
    "$dir" {} "${flags[@]}"
}

# 1) The stack + harness runtime, sanitized, DWARF 3. The fuzz objects also carry SanCov
#    (-fsanitize=fuzzer-no-link, always — the fuzzed code must be coverage-instrumented even when
#    SANITIZER_FLAGS is empty); the -standalone reproducers link a second object set without it, so
#    they need no libFuzzer/sanitizer runtime when the sanitizers are switched off.
rm -rf "$OUT/fuzz" "$OUT/standalone" "$OUT/test"
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link"
# shellcheck disable=SC2086
compile "$OUT/fuzz" $FUZZ_FLAGS "${INC[@]}" -- "${STACK[@]}"
# shellcheck disable=SC2086
compile "$OUT/standalone" $SANITIZER_FLAGS $DEBUG_FLAGS "${INC[@]}" -- "${STACK[@]}"
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OUT/lsan_off.o"
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -x c -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

# 2) Each harness twice: libFuzzer binary and run-once reproducer.
for h in "${HARNESSES[@]}"; do
  # shellcheck disable=SC2086
  $CC $FUZZ_FLAGS "${INC[@]}" -c "$H/$h.c" -o "$OUT/fuzz/$h.o"
  # shellcheck disable=SC2086
  $CXX $FUZZ_FLAGS $LIB_FUZZING_ENGINE "$OUT/fuzz/$h.o" "$OUT"/fuzz/s-*.o "$OUT/lsan_off.o" -o "/mayhem/$h"
  # shellcheck disable=SC2086
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS "${INC[@]}" -c "$H/$h.c" -o "$OUT/standalone/$h.o"
  # shellcheck disable=SC2086
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/standalone_main.o" "$OUT/standalone/$h.o" "$OUT"/standalone/s-*.o \
    "$OUT/lsan_off.o" -o "/mayhem/$h-standalone"
done

# 3) The self-test with normal flags (no sanitizers), for mayhem/test.sh.
# shellcheck disable=SC2086
compile "$OUT/test" -O1 $COVERAGE_FLAGS "${INC[@]}" -- "${STACK[@]}" "$H/co_selftest.c"
# shellcheck disable=SC2086
$CC $COVERAGE_FLAGS "$OUT"/test/*.o -o "$OUT/test/co_selftest"

echo "built: ${HARNESSES[*]} (+ -standalone) and $OUT/test/co_selftest"
