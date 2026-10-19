# Validation Quickstart

This guide describes validation with the implemented `scripts/debug-device.py` CLI; the interface and security contract are defined in [contracts/device-debug.md](contracts/device-debug.md).

## Prerequisites

Use Ubuntu 24.04/26.04, ARM64 native or amd64 cross; Windows x86-64 uses WSL2 Ubuntu and the Linux filesystem. Run all host commands from repository root. Have network access for initial dependencies and a clean pinned core checkout (initialize with `git submodule update --init --recursive`). Have a user-controlled ARM64 Linux console with root SSH password authentication, verified host fingerprint, required POSIX utilities, compatible glibc, and legally supplied device-local ROM/data assets.

## Prepare and build

On amd64 Linux or WSL2 Ubuntu:

```sh
scripts/setup-ubuntu.sh --cross
scripts/setup-ubuntu.sh --cross --check
cmake --preset linux-arm64-cross
cmake --build --preset linux-arm64-cross
file build/linux-arm64-cross/bin/seedlessds-headless
aarch64-linux-gnu-readelf -l build/linux-arm64-cross/bin/seedlessds-headless
aarch64-linux-gnu-readelf --version-info build/linux-arm64-cross/bin/seedlessds-headless
```

Installer needs root/sudo for apt; verification/build do not. Updated setup must install/check sshpass and SSH tooling. CMake runs `prepare-core.sh linux-arm64-cross` automatically, fetches pinned dependencies and audits the executable; expect `build/linux-arm64-cross/runtime-inventory.json`. If targeting another sysroot, export `SEEDLESS_TARGET_SYSROOT=/absolute/path/to/device-sysroot` before a clean configure and confirm the resulting ABI against hardware.

On ARM64 Ubuntu substitute `--native` and `linux-arm64-native` for setup/preset/output paths and use `readelf`; then run `ctest --preset linux-arm64-native`. Never execute the cross artifact on an amd64 host. Both presets produce Linux ARM64 executables and require compatible target runtime libraries.

## Configure and check locally

The implementer explicitly asks the user for console IP, root SSH password and absolute installation directory, then creates ignored `.device.local` from `.device.example.json` without printing credentials, with mode 0600. Keep the template placeholder-only. Confirm exclusion without reading its content:

```sh
git check-ignore .device.local
git check-ignore .device.example.json
python3 scripts/debug-device.py check
python3 -m unittest discover -s tests/device-debug -p 'test_*.py'
```

First ignore check must succeed; template check must return 1 (not ignored). Test temporary configurations cover absent/invalid/placeholder values, duplicate keys, permissions, malicious-looking literal values, IPv6, split-chunk secret redaction, auth/host failures, corrupt/interrupted transfer, preservation of old binary, concurrency locking, runner failure, timeout and disconnect. Verify no remote action occurs after invalid inputs or failed stages. Tests use fake transport and never count as hardware acceptance.

Before automated access, manually run `ssh root@<CONSOLE_IP>` with the real address privately, verify the displayed fingerprint through a trusted source, enroll the host, and exit. Do not place the password on the command line or disable host verification. Device-local ROM and writable data directory must already exist, including any required BIOS/firmware in the runner's documented layout.

## Real-device run

Replace runtime-path placeholders with the user's device-local paths; use native artifact paths if applicable:

```sh
python3 scripts/debug-device.py run \
  --binary build/linux-arm64-cross/bin/seedlessds-headless \
  --inventory build/linux-arm64-cross/runtime-inventory.json \
  --rom /absolute/device/path/test.nds \
  --data-dir /absolute/device/path/run-data \
  --frames 120 --timeout 30
```

Expected: validated settings, root connection, Linux/AArch64 and ABI checks, verified atomic deployment, successful help launch and bounded emulation; CLI exit 0, runner exit 0 and `result=completed frames=120 requested=120`. Repeat without editing tracked files to demonstrate configuration reuse. Inspect sanitized evidence under `build/validation/device/` and verify deployed SHA-256 equals the local binary. Help alone does not prove successful emulation.

Perform controlled failure checks through fake transport for authentication/transfer failures and on hardware where practical without changing device policies. Missing assets or incompatible libraries must identify the blocked stage and prevent dependent execution. Do not alter existing assets merely to induce failure. Existing `scripts/validate-headless.sh BINARY ROM DATA_DIR` can provide deeper on-device regression coverage if its Bash/ripgrep/timeout prerequisites are available; it is optional for this feature's acceptance.

## Documentation acceptance

README Build must show working directories, supported hosts, setup and verification order, privileges, environment/sysroot settings, both presets, artifact paths, runtime constraints and which machines can execute each artifact. README Debug must show private config setup, host verification, prerequisites, runtime inputs, deployment/run commands, stage/runner interpretation and actual hardware findings.

Record attempt date, sanitized device model/OS/kernel/architecture/libc, tested revision/artifact hash, actual commands with private values replaced, result and exit status, issues and workarounds. Missing hardware, credentials or ROMs remain explicitly blocked; record what prevented the attempt and leave FR-009/SC-002 outstanding. Review all changed tracked files and evidence for private values before sharing. Do not publish raw logs. A real successful bounded session is mandatory for completed hardware acceptance.

## Implementation status (2026-10-19)

Local configuration schema, owner-only mode, Git exclusion, placeholder rejection, special-character preservation, CLI help and artifact/inventory hash validation were checked. The cross setup check passed after the host prerequisites were installed. A fresh preset-based cross configure/build succeeded in `/tmp/seedlessds-device-cross` because the repository's prior preset cache references a different checkout; its sanitized inventory and build observations are recorded under ignored `build/validation/device/`. The output is AArch64, imports only the target loader and libc, and requires GLIBC 2.34. It was inspected with `file` and `readelf`, not run on amd64. After the user confirmed the trusted host fingerprint, the actual console passed connection, compatibility and deployment checks. The console reports Anbernic RG DS Plus, Linux, kernel 7.2.7, AArch64 and GNU libc 2.44. The 120-frame runner exited 139; a second 120-frame diagnostic and one-frame probe exited 135, all without valid completion records. Kernel logs report failed systemd-coredump pipe attempts, and no core dump was available. The sanitized per-session record and separate hardware diagnostic are under ignored `build/validation/device/`. User-provided ROM and data directories contained ROM files; one was selected without including its filename in evidence. Controlled fake-transport checks cover workflow exit mapping and transfer safety, but do not establish successful hardware acceptance. FR-009/SC-002 remains pending until the crash is diagnosed and an exact bounded completion succeeds. See [README Debug](../../README.md#debug) for the findings.
