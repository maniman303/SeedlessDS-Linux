#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/build.sh [--cross|--native] [--clean]

Build the ARM64 frontend with the matching canonical CMake preset. The preset
is selected from the host architecture when omitted: amd64 uses --cross and
ARM64 uses --native. CMake downloads and verifies pinned build dependencies.

  --cross  Build into build/linux-arm64-cross/ for an ARM64 Linux target.
  --native Build into build/linux-arm64-native/ on an ARM64 Linux host.
  --clean  Remove only the selected preset directory before configuring.
USAGE
}

fail() {
    printf 'build: ERROR: %s\n' "$*" >&2
    exit 1
}

preset=''
clean=0
while (($#)); do
    case "$1" in
        --cross|--native)
            [[ -z "$preset" ]] || fail 'choose only one of --cross or --native'
            preset="linux-arm64-${1#--}"
            ;;
        --clean)
            clean=1
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            usage >&2
            fail "unknown option: $1"
            ;;
    esac
    shift
done

if [[ -z "$preset" ]]; then
    host_arch="$(uname -m)"
    case "$host_arch" in
        x86_64|amd64) preset='linux-arm64-cross' ;;
        aarch64|arm64) preset='linux-arm64-native' ;;
        *) fail "unsupported build host architecture: $host_arch" ;;
    esac
fi

host_arch="${host_arch:-$(uname -m)}"
case "$preset:$host_arch" in
    linux-arm64-cross:x86_64|linux-arm64-cross:amd64|linux-arm64-native:aarch64|linux-arm64-native:arm64) ;;
    linux-arm64-cross:*) fail "--cross requires an amd64 host (detected $host_arch)" ;;
    linux-arm64-native:*) fail "--native requires an ARM64 host (detected $host_arch)" ;;
esac

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
build_dir="$repo_root/build/$preset"
case "$preset" in
    linux-arm64-cross|linux-arm64-native) ;;
    *) fail "invalid canonical preset: $preset" ;;
esac

if ((clean)); then
    printf 'Removing canonical build directory: %s\n' "$build_dir"
    rm -rf -- "$build_dir"
fi

cd -- "$repo_root"
printf 'Configuring %s\n' "$preset"
cmake --preset "$preset"

jobs="${BUILD_JOBS:-}"
if [[ -z "$jobs" ]]; then
    jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')"
fi
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || fail "BUILD_JOBS must be a positive integer (got '$jobs')"

printf 'Building with %s parallel job(s)\n' "$jobs"
cmake --build --preset "$preset" --parallel "$jobs"

printf 'Running tests in build/%s\n' "$preset"
ctest --test-dir "$build_dir" --output-on-failure

artifact="$build_dir/bin/seedlessds"
[[ -x "$artifact" ]] || fail "build completed without the expected executable: $artifact"
printf 'Build and tests passed. Executable: %s\n' "$artifact"
