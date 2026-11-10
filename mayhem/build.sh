#!/usr/bin/env bash
#
# mayhem/build.sh — builds the OpenCat firmware as a host program (the serial-command fuzz target, its standalone
# reproducer, and the known-answer oracle that mayhem/test.sh runs). Runs in the commit image as `mayhem` in /mayhem,
# offline: nothing is fetched. See mayhem/shim/ for the Arduino/AVR peripherals the firmware is linked against.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE STANDALONE_FUZZ_MAIN MAYHEM_JOBS COVERAGE_FLAGS

SRC="${SRC:-/mayhem}"
cd "$SRC"
B="$SRC/mayhem/_build"
rm -rf "$B"
mkdir -p "$B/fuzz" "$B/standalone" "$B/oracle"

# The firmware is Arduino C++ for an 8-bit AVR. On this host it is compiled as-is (upstream is never edited) with the
# Arduino core replaced by mayhem/shim:
#   -include Arduino.h                  upstream headers rely on the Arduino IDE auto-including it
#   -fms-extensions / -Wno-pointer-to-int-cast
#                                       skill.h stores PROGMEM addresses in int tables: a 16-bit pointer on AVR
INC="-I$SRC/mayhem/shim -I$SRC/mayhem/harness -I$B -I$SRC/src -I$SRC"
CXXFLAGS_COMMON="-std=gnu++17 -include Arduino.h -fms-extensions -Wno-pointer-to-int-cast -Wno-everything -DARDUINO=10819 $INC"
LIBSRC="$SRC/src/Adafruit-PWM-Servo-Driver-Library/Adafruit_PWMServoDriver.cpp"
SHIMSRC="$SRC/mayhem/shim/mock.cpp"

# 1) The factory EEPROM images. The firmware boots from the contents the NyBoard is flashed with (calibration, the
#    skill table, the instinct skills on the I2C EEPROM); they are produced by upstream's own configuration code,
#    running here, and baked into factory_image.inc.
$CXX $CXXFLAGS_COMMON -O1 $DEBUG_FLAGS -no-pie \
  "$SRC/mayhem/harness/gen_eeprom.cpp" "$SHIMSRC" "$LIBSRC" "$SRC/src/mpu6050/MPU6050.cpp" "$SRC/src/mpu6050/I2Cdev.cpp" \
  -o "$B/gen_eeprom"
"$B/gen_eeprom" "$B/factory_image.inc"

# 2) Fuzz target + standalone reproducer: the firmware and the shim are instrumented with $SANITIZER_FLAGS.
#    The libFuzzer objects carry coverage instrumentation ($LIB_FUZZING_ENGINE at -c); the standalone ones do not,
#    so they link without the fuzzer runtime (and with an empty SANITIZER_FLAGS).
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone/main.o"
pids=()
compile() {  # <dir> <extra flags> <src> <obj>
  $CXX $CXXFLAGS_COMMON -O1 $SANITIZER_FLAGS $DEBUG_FLAGS $2 -c "$3" -o "$B/$1/$4.o" &
  pids+=($!)
}
for kind in fuzz standalone; do
  engine=""; [ "$kind" = fuzz ] && engine="$LIB_FUZZING_ENGINE"
  compile $kind "$engine" "$SRC/mayhem/harness/serial_fuzz.cpp" serial_fuzz
  compile $kind "$engine" "$SHIMSRC" mock
  compile $kind "$engine" "$LIBSRC" pwm
  compile $kind "" "$SRC/mayhem/lsan_off.cc" lsan_off
done
for p in "${pids[@]}"; do wait "$p"; done

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B"/fuzz/*.o -o "$B/serial_fuzz"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$B"/standalone/*.o -o "$B/serial_fuzz-standalone"

# 3) The oracle test.sh runs, with the project's normal flags (no sanitizers).
$CXX $CXXFLAGS_COMMON -O2 $COVERAGE_FLAGS \
  "$SRC/mayhem/harness/oracle.cpp" "$SHIMSRC" "$LIBSRC" -o "$B/oracle/oracle"
