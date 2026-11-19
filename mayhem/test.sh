#!/usr/bin/env bash
#
# mayhem/test.sh — RUN the CANopenNode known-answer self-test built by mayhem/build.sh.
#
# Upstream ships no test suite, so mayhem/harnesses/co_selftest.c drives the stack through its
# public interfaces (CAN frames into its receive callbacks, CiA 309-3 gateway text) and asserts the
# exact answers: boot-up frame, NMT state changes, SDO expedited upload/download/abort frames, gateway
# read/write/error output, OD restore. Each check prints PASS/FAIL; the count of checks is fixed, so a
# self-test that prints nothing (or fewer checks) fails here.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

EXPECTED=14

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

RUNNER=mayhem/out/test/co_selftest
if [ ! -x "$RUNNER" ]; then
  echo "test.sh: $RUNNER missing — mayhem/build.sh must build it" >&2
  emit_ctrf co_selftest 0 1
  exit 1
fi

out=$("$RUNNER" 2>&1); rc=$?
printf '%s\n' "$out"
passed=$(printf '%s\n' "$out" | grep -c '^PASS ' || true)
failed=$(printf '%s\n' "$out" | grep -c '^FAIL ' || true)
summary=$(printf '%s\n' "$out" | grep -c "^SUMMARY $passed $failed\$" || true)
# Checks that never reported (the runner died, exited early, or printed nothing) count as failed.
missing=$(( EXPECTED - passed - failed ))
[ "$missing" -gt 0 ] && failed=$(( failed + missing ))
if [ "$rc" -ne 0 ] || [ "$summary" -ne 1 ]; then
  echo "test.sh: co_selftest exit=$rc, summary line present=$summary" >&2
  [ "$failed" -eq 0 ] && failed=1
fi
emit_ctrf co_selftest "$passed" "$failed"
