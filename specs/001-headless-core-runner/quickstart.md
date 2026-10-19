# Quickstart Validation Guide

These commands exercise the implementation. Run from the repository root; the current verified host is Ubuntu 26.04 amd64 for the cross build.

## Prepare Ubuntu locally

Supported development hosts: Ubuntu 24.04/26.04 LTS. Windows contributors run these commands inside WSL2 Ubuntu. Use a Linux filesystem checkout when practical for build performance.

On this amd64 Ubuntu host:

```bash
bash scripts/setup-ubuntu.sh --cross --dry-run
bash scripts/setup-ubuntu.sh --cross
bash scripts/setup-ubuntu.sh --cross --check
```

On ARM64 Ubuntu substitute `--native`. Repeat normal setup and check once to validate idempotence. Installation uses local apt/sudo; prerequisite checks report versions and target triple. See [setup contract](contracts/build-and-setup.md) for exact scope.

## Prepare and build

```bash
git submodule update --init --recursive
cmake --preset linux-arm64-cross
cmake --build --preset linux-arm64-cross
file build/linux-arm64-cross/bin/seedlessds
aarch64-linux-gnu-readelf -h build/linux-arm64-cross/bin/seedlessds
aarch64-linux-gnu-readelf -d build/linux-arm64-cross/bin/seedlessds
```

Expect an AArch64 Linux executable with no Android/OpenSL/GLES dependency. For native builds use `linux-arm64-native` for configure/build and run `ctest --preset linux-arm64-native`. Supply the documented sysroot override when targeting devices whose libc differs from the Ubuntu reference runtime. Cross binaries must be validated on ARM64, not launched on amd64.

Dependency preparation must leave the core submodule untouched, stage patches deterministically, and verify pinned downloads. Inspect the source checkout after preparation. Build failures must identify the missing dependency, compiler, source drift or ABI constraint.

## Validate on actual ARM64 Linux

Transfer the executable through existing user-managed SSH configuration. Application libraries should already be embedded; compare the generated runtime inventory with the target OS runtime and any explicit compatibility exceptions. Validate on a target without separately installed core/Lua/archive/zlib libraries; do not add shared application libraries just to make the smoke run pass. Supply a legally usable test ROM and documented runtime assets separately. On the target, using absolute local paths:

```bash
mkdir -p ./run-data
env -u DISPLAY -u WAYLAND_DISPLAY ./seedlessds --data-dir ./run-data --frames 120 /path/to/test.nds
```

Expect `result=completed frames=120 requested=120`, exit 0, no windows, sound or microphone access. Repeat three times for each artifact from native and cross builds. Also run `--frames 1` to catch startup/target ordering errors. Missing display and sound services must not affect execution. The runner arms `nds_control_run_to(N)` before the worker starts and reports the control API frame count. Retain adapter device-access audit evidence.

Start without `--frames`, interrupt with Ctrl-C, and expect an interruption record and exit 130 within five seconds. Repeat three times and verify no process remains. Test SIGTERM similarly, expecting exit 143. Exercise termination during loading and at the target pause; report any workload that cannot meet the bound.

## Failure and evidence checks

Run `--help`, missing ROM, nonexistent/directory ROM, corrupt ROM, ROM path containing spaces, invalid limits `0`, `-1`, nonnumeric and overflow, and unwritable data root. Expect the [CLI contract](contracts/cli.md) outcomes. Run setup checks with missing tools and build preparation with a mismatched patch base in disposable test directories; expect actionable failures.

Record core/source revision, tool versions, host and target libc, kernel, page size, device model, ROM checksum, runtime asset requirements, remaining shared-library inventory with exception reasons and symbol-version baseline, commands, exit statuses, actual frame counts and shutdown timing. Reference [data model](data-model.md). Do not record SSH credentials. A successful cross-link or QEMU test does not complete hardware acceptance.
