#!/usr/bin/env bash
set -Eeuo pipefail

fail() { printf 'validate-headless: ERROR: %s\n' "$*" >&2; exit 1; }
[[ $# == 3 ]] || fail 'usage: bash scripts/validate-headless.sh BINARY ROM DATA_DIR'
binary="$(realpath -- "$1")"
rom="$(realpath -- "$2")"
data_dir="$(realpath -- "$3")"
[[ -x "$binary" ]] || fail "not executable: $binary"
[[ -f "$rom" && -r "$rom" ]] || fail "ROM must be a readable regular file: $rom"
[[ -d "$data_dir" && -w "$data_dir" && -x "$data_dir" ]] || fail "data root must be writable: $data_dir"
[[ "$(uname -m)" == aarch64 ]] || fail 'run this acceptance script on ARM64 Linux; cross binaries are not executed on amd64'

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
validation="$root/build/validation/us1"
mkdir -p "$validation"
log="$validation/run-$(date -u +%Y%m%dT%H%M%SZ).log"
runner() { "$binary" "$@"; }
expect_code() {
    local expected="$1"; shift
    local actual=0
    runner "$@" >>"$log" 2>&1 || actual=$?
    [[ "$actual" == "$expected" ]] || fail "expected exit $expected, got $actual for: $* (see $log)"
}
expect_code 0 --help
expect_code 2
expect_code 2 --frames 0 "$rom"
expect_code 2 --frames -1 "$rom"
expect_code 2 --frames nope "$rom"
expect_code 2 --frames 18446744073709551616 "$rom"
expect_code 2 --frames 1 --frames 2 "$rom"
expect_code 2 --unknown "$rom"
expect_code 2 --frames 1 "$data_dir"
expect_code 2 --data-dir "$data_dir/missing" "$rom"

bounded() {
    local count="$1"; local output="$validation/frames-$count.out"; local code=0
    timeout --signal=TERM --kill-after=1s 30s "$binary" --data-dir "$data_dir" --frames "$count" "$rom" >"$output" 2>>"$log" || code=$?
    [[ "$code" == 0 ]] || fail "bounded run $count failed with exit $code (see $output and $log)"
    rg -q "^result=completed frames=$count requested=$count$" "$output" || \
        fail "bounded run $count did not report the exact scheduler target"
}
bounded 1
bounded 120
for _ in 1 2 3; do bounded 120; done

interrupt_run() {
    local signal_name="$1"; local expected="$2"; local output="$validation/$signal_name.out"
    runner --data-dir "$data_dir" "$rom" >"$output" 2>>"$log" &
    local pid=$!; local deadline=$((SECONDS + 5))
    sleep 1
    kill -s "$signal_name" "$pid" 2>/dev/null || true
    while kill -0 "$pid" 2>/dev/null && ((SECONDS < deadline)); do sleep 0.1; done
    if kill -0 "$pid" 2>/dev/null; then kill -KILL "$pid"; wait "$pid" || true; fail "$signal_name shutdown exceeded five seconds"; fi
    local code=0; wait "$pid" || code=$?
    [[ "$code" == "$expected" ]] || fail "$signal_name run returned $code, expected $expected (see $output)"
    rg -q "^result=interrupted frames=[0-9]+ signal=$signal_name$" "$output" || fail "$signal_name output record is missing"
}
interrupt_run SIGINT 130
interrupt_run SIGTERM 143
printf 'Headless acceptance checks passed. Log: %s\n' "$log"
