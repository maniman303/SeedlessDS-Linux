# Build and Ubuntu Setup Contract

## Tooling installer

```text
bash scripts/setup-ubuntu.sh --native [--check|--dry-run]
bash scripts/setup-ubuntu.sh --cross [--check|--dry-run]
```

Require exactly one mode. Ubuntu 24.04/26.04 only initially; native requires ARM64, cross requires amd64 and emits ARM64. Detect release/architecture before changes. `--check` performs read-only version, target-triple and temporary compile/link probes without apt or sudo; temporary outputs are removed. `--dry-run` prints resolved packages/commands without installation. Normal setup uses apt with sudo only where needed, reports failures, and can be rerun. Never rewrites apt sources or enables multiarch silently.

Common package set: `build-essential`, `cmake`, `ninja-build`, `git`, `pkg-config`, `ca-certificates`, `curl`, `xz-utils`, `bzip2`, `unzip`, `python3`, `shellcheck`, `file`, `binutils`, `openssh-client`. Native adds `gdb`; cross adds `gcc-aarch64-linux-gnu`, `g++-aarch64-linux-gnu`, `binutils-aarch64-linux-gnu`, `libc6-dev-arm64-cross`, `gdb-multiarch`. Verify apt availability and actionable repository instructions if a package is unavailable; do not install Android SDK/NDK, SDL, GLES, or OpenSL libraries. Third-party core libraries, including zlib, are built from pinned sources rather than installed as host development packages.

Verify CMake >=3.28, Ninja, compiler target, assembler/linker, required commands, and a linked AArch64 executable inspected by readelf. Report host OS, versions and target triple. A cross probe is inspected, not executed on amd64. Setup is complete only when verification succeeds. Document that package installation is system-wide on the developer's local Ubuntu installation; project dependency sources/artifacts stay in ignored build/cache directories.

## Build interface

```text
cmake --preset linux-arm64-native
cmake --build --preset linux-arm64-native
ctest --preset linux-arm64-native
cmake --preset linux-arm64-cross
cmake --build --preset linux-arm64-cross
```

Both presets emit `build/<preset>/bin/seedlessds`. Host-only checks may run on amd64; cross target tests must be transferred and run on ARM64. Never make cross CTest attempt to execute target binaries implicitly.

Use a real CMake toolchain with Linux/aarch64, explicit compilers, and target-only library/include/package searches; host programs remain host-selected. Provide documented `SEEDLESS_TARGET_SYSROOT` override for device ABI builds; reject missing or incompatible sysroots. Pin/download/checksum dependencies and initialize nested Git submodules explicitly; offline reuse is allowed only for verified cached inputs. Preserve upstream license notices.

Source preparation stages the pinned core under the selected build directory and checks patches against the manifest. It never modifies, resets or cleans `external/SeedlessDS-Core`. Unexpected revision/context/local changes fail clearly. Cross/native caches and generated files remain separate. Audit the final ELF for AArch64, intended interpreter/libc versions, and absence of Android/JNI/OpenSL/GLES dependencies or unresolved required symbols.


## Linkage and deployment acceptance

Default outputs statically embed `recon_fn`, Lua, libarchive, unarr, LZMA SDK and zlib, including application-level transitive dependencies. Select static targets explicitly; do not rely on a global BUILD_SHARED_LIBS flag to override every upstream target. Prefer static compiler support where compatible. Retain glibc and required OS runtime components dynamically; the target ABI determines whether pthread/dl/m appear separately. Full static linking is optional, not an acceptance requirement.

A missing compatible static application library fails configure/link with an actionable diagnosis. A shared-library exception requires explicit configuration and recorded reason; no automatic fallback. Generate a per-artifact runtime inventory with interpreter, DT_NEEDED, required symbol versions, runtime-loaded libraries and exception rationale. Audit transitive dependencies and dlopen paths, not only direct library names. Confirm the target's libc baseline with the selected sysroot.

Acceptance requires the same executable to complete the bounded smoke run without separately installed core/application libraries, except declared exceptions. Do not copy a host glibc into the deployment bundle as a substitute for choosing a compatible baseline. Include necessary source/license/redistribution materials with distribution artifacts.
