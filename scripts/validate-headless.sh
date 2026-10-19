#!/usr/bin/env bash
set -Eeuo pipefail

fail() { printf 'validate-headless: ERROR: %s\n' "$*" >&2; exit 1; }
[[ $# == 3 ]] || fail 'usage: bash scripts/validate-headless.sh BINARY ROM CONFIG'
binary="$(realpath -- "$1")"
rom="$(realpath -- "$2")"
config="$(realpath -- "$3")"
[[ -x "$binary" ]] || fail "not executable: $binary"
[[ -f "$rom" && -r "$rom" ]] || fail "ROM must be a readable regular file: $rom"
[[ -f "$config" && -r "$config" ]] || fail "config must be a readable regular file: $config"
[[ "$(uname -m)" == aarch64 ]] || fail 'run this acceptance script on ARM64 Linux; cross binaries are not executed on amd64'

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
validation="$root/build/validation/config"
mkdir -p "$validation"
log="$validation/run-$(date -u +%Y%m%dT%H%M%SZ).log"
runner() { "$binary" --config "$config" "$@"; }
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
expect_code 2 --frames 1 "$config"
expect_code 2 --bios-arm7 "$rom"

# Missing BIOS files used to unwind through an uninitialized jump buffer.
missing_config="$validation/missing-bios.json"
python3 - "$config" "$missing_config" <<'PYCFG'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f: data=json.load(f)
data["firmware"]["bios_arm7"]="/seedlessds-missing/arm7.bin"
data["firmware"]["bios_arm9"]="/seedlessds-missing/arm9.bin"
with open(sys.argv[2], "w", encoding="utf-8") as f: json.dump(data, f)
PYCFG
missing_output="$validation/missing-bios.out"
missing_code=0
timeout --signal=TERM --kill-after=1s 30s "$binary" --config "$missing_config" --frames 1 "$rom" >"$missing_output" 2>&1 || missing_code=$?
[[ "$missing_code" == 1 ]] || fail "missing BIOS run returned $missing_code, expected 1"
rg -q '^result=failed frames=0 operation=firmware$' "$missing_output" || fail 'missing BIOS failure record is missing'
rg -q 'firmware.bios_arm9: BIOS must be readable' "$missing_output" || fail 'missing ARM9 BIOS diagnostic is missing'
if rg -q 'core initialization completed|ROM load started' "$missing_output"; then fail 'core started despite missing BIOS'; fi

bounded() {
    local count="$1"; local output="$validation/frames-$count.out"; local code=0
    timeout --signal=TERM --kill-after=1s 30s "$binary" --config "$config" --frames "$count" "$rom" >"$output" 2>>"$log" || code=$?
    [[ "$code" == 0 ]] || fail "bounded run $count failed with exit $code (see $output and $log)"
    rg -q "^result=completed frames=$count requested=$count$" "$output" || \
        fail "bounded run $count did not report the exact scheduler target"
}
bounded 1
bounded 120
for _ in 1 2 3; do bounded 120; done

interrupt_run() {
    local signal_name="$1"; local expected="$2"; local output="$validation/$signal_name.out"
    runner "$rom" >"$output" 2>>"$log" &
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
