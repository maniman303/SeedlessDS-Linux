#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
    cat <<'USAGE'
Usage: bash scripts/setup-ubuntu.sh (--native|--cross) [--check|--dry-run]

  --native   Prepare an ARM64 Ubuntu host to build and run natively.
  --cross    Prepare an amd64 Ubuntu host to cross-build for ARM64 Linux.
  --check    Verify prerequisites and compile/link a temporary ARM64 probe.
  --dry-run  Print the package installation commands without changing the host.

With neither --check nor --dry-run, install the packages using apt. Setup is
limited to Ubuntu 24.04/26.04 and never edits apt sources or enables multiarch.
USAGE
}

fail() {
    printf 'setup-ubuntu: ERROR: %s\n' "$*" >&2
    exit 1
}

mode=''
action='install'
while (($#)); do
    case "$1" in
        --native|--cross)
            [[ -z "$mode" ]] || fail 'choose exactly one of --native or --cross'
            mode="${1#--}"
            ;;
        --check|--dry-run)
            [[ "$action" == install ]] || fail 'choose at most one of --check or --dry-run'
            action="${1#--}"
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *) fail "unknown option: $1" ;;
    esac
    shift
done
[[ -n "$mode" ]] || { usage >&2; fail 'select --native or --cross'; }

[[ -r /etc/os-release ]] || fail 'cannot identify operating system from /etc/os-release'
# shellcheck disable=SC1091
source /etc/os-release
[[ "${ID:-}" == ubuntu ]] || fail "Ubuntu is required (detected ${ID:-unknown})"
case "${VERSION_ID:-}" in
    24.04|26.04) ;;
    *) fail "Ubuntu 24.04 or 26.04 is supported (detected ${VERSION_ID:-unknown})" ;;
esac

host_arch="$(dpkg --print-architecture 2>/dev/null)" || fail 'dpkg is required to detect host architecture'
case "$mode:$host_arch" in
    native:arm64|cross:amd64) ;;
    native:*) fail "--native requires ARM64 Ubuntu (detected $host_arch)" ;;
    cross:*) fail "--cross requires amd64 Ubuntu (detected $host_arch)" ;;
esac

common_packages=(build-essential cmake ninja-build git pkg-config ca-certificates curl sshpass
    xz-utils bzip2 unzip python3 shellcheck file binutils openssh-client)
if [[ "$mode" == native ]]; then
    packages=("${common_packages[@]}" gdb)
    cc=gcc
    readelf=readelf
else
    packages=("${common_packages[@]}" gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
        binutils-aarch64-linux-gnu libc6-dev-arm64-cross gdb-multiarch)
    cc=aarch64-linux-gnu-gcc
    readelf=aarch64-linux-gnu-readelf
fi

printf 'Ubuntu: %s (%s), host architecture: %s, mode: %s, target: aarch64-linux-gnu\n' \
    "${VERSION_ID}" "${VERSION_CODENAME:-unknown}" "$host_arch" "$mode"
printf 'Packages: %s\n' "${packages[*]}"

if [[ "$action" == dry-run ]]; then
    printf 'Would run: sudo apt-get update\n'
    printf 'Would run: sudo apt-get install --no-install-recommends %s\n' "${packages[*]}"
    exit 0
fi

if [[ "$action" == install ]]; then
    if ((EUID == 0)); then
        apt=(apt-get)
    elif command -v sudo >/dev/null 2>&1; then
        apt=(sudo apt-get)
    else
        fail 'install mode needs root or sudo; rerun as root or configure a narrowly scoped package-install privilege wrapper'
    fi
    "${apt[@]}" update || fail 'apt-get update failed; check network access and configured Ubuntu repositories'
    "${apt[@]}" install --no-install-recommends -y "${packages[@]}" || \
        fail "package installation failed; check that Ubuntu ${VERSION_ID} repositories provide: ${packages[*]}"
fi

missing=()
for command_name in cmake ninja git pkg-config curl python3 shellcheck file ssh sshpass "$readelf"; do
    command -v "$command_name" >/dev/null 2>&1 || missing+=("$command_name")
done
command -v "$cc" >/dev/null 2>&1 || missing+=("$cc")
((${#missing[@]} == 0)) || fail "required tools are missing: ${missing[*]}; run the installer or check Ubuntu repositories"

cmake_version="$(cmake --version | awk 'NR == 1 { print $3 }')"
python3 - "$cmake_version" <<'PY' || exit 1
import sys
version = tuple(int(part) for part in sys.argv[1].split('.')[:2])
if version < (3, 28):
    print(f"setup-ubuntu: ERROR: CMake >=3.28 is required (found {sys.argv[1]})", file=sys.stderr)
    raise SystemExit(1)
PY
python3 - <<'PY_VERSION' || exit 1
import sys
if sys.version_info < (3, 12):
    print(f"setup-ubuntu: ERROR: Python >=3.12 is required (found {sys.version.split()[0]})", file=sys.stderr)
    raise SystemExit(1)
PY_VERSION

triple="$($cc -dumpmachine 2>/dev/null)" || fail "could not query target triple from $cc"
[[ "$triple" == aarch64*-linux-gnu* ]] || fail "$cc targets '$triple', expected aarch64 Linux GNU"

probe_dir="$(mktemp -d "${TMPDIR:-/tmp}/seedless-toolchain.XXXXXX")" || fail 'could not create temporary probe directory'
trap 'rm -rf "$probe_dir"' EXIT
cat > "$probe_dir/probe.c" <<'C'
#include <pthread.h>
int main(void) { pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; return pthread_mutex_destroy(&mutex); }
C
"$cc" -Wall -Wextra -Werror "$probe_dir/probe.c" -pthread -o "$probe_dir/probe" || \
    fail "compiler could not compile and link ARM64 probe with pthreads: $cc"
elf_header="$("$readelf" -h "$probe_dir/probe" 2>&1)" || fail "$readelf could not inspect the probe"
grep -Eq 'Machine:.*AArch64' <<<"$elf_header" || fail 'linked probe is not an AArch64 executable'

printf 'CMake: %s\n' "$cmake_version"
printf 'Ninja: %s\n' "$(ninja --version)"
printf 'Compiler: %s (%s)\n' "$cc" "$triple"
printf 'Readelf: %s; linked probe machine: AArch64\n' "$readelf"
printf 'Package setup and toolchain check passed (%s mode).\n' "$mode"
