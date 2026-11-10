#!/usr/bin/env bash
#
# mayhem/test.sh — runs the known-answer oracle (built by mayhem/build.sh with normal flags). It boots the emulated
# Bittle, sends serial commands (?, d, kup, j, m8 45, kwkF, c, ksit, kvtF) and compares the UART replies and the
# PCA9685 servo pulses to recorded values, then checks that the robot is reset between inputs. The result comes from
# the oracle's own `ORACLE passed=N failed=M` line, so a program that exits 0 without checking anything fails.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

ORACLE="$SRC/mayhem/_build/oracle/oracle"
if [ ! -x "$ORACLE" ]; then
  echo "missing $ORACLE — mayhem/build.sh must build it" >&2
  emit_ctrf "opencat-oracle" 0 1
  exit 1
fi

out=$("$ORACLE" 2>&1); rc=$?
printf '%s\n' "$out"
summary=$(printf '%s\n' "$out" | grep -E '^ORACLE passed=[0-9]+ failed=[0-9]+$' | tail -n 1)
if [ -z "$summary" ]; then
  echo "oracle printed no summary line (rc=$rc)" >&2
  emit_ctrf "opencat-oracle" 0 1
  exit 1
fi
passed=${summary#ORACLE passed=}; passed=${passed%% *}
failed=${summary##*failed=}
[ "$rc" -eq 0 ] || failed=$(( failed > 0 ? failed : 1 ))
[ "$passed" -gt 0 ] || failed=$(( failed > 0 ? failed : 1 ))
emit_ctrf "opencat-oracle" "$passed" "$failed"
