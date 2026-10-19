# SeedlessDS-Linux

SeedlessDS-Linux builds a minimal Linux ARM64 runner around the pinned SeedlessDS-Core. It runs emulation without opening a window, audio device or microphone. The core keeps software video and SPU emulation active; host presentation and audio output are discarded.

## Build

Run commands from the repository root on Ubuntu 24.04 or 26.04. Supported hosts are Ubuntu ARM64 for a native build and Ubuntu amd64 for an ARM64 cross build. Windows x86-64 development uses WSL2 Ubuntu; keeping the checkout in the WSL Linux filesystem is recommended. A cold build needs network access to fetch pinned dependencies. Initialize the pinned core checkout first:

```sh
git submodule update --init --recursive
```

The setup script installs prerequisites with apt and sudo (or as root). `--native` requires ARM64 Ubuntu; `--cross` requires amd64 Ubuntu. Install first, then verify the packages and compiler with `--check`:

```sh
scripts/setup-ubuntu.sh --native
scripts/setup-ubuntu.sh --native --check
```

For Ubuntu amd64 or WSL2 Ubuntu, use:

```sh
scripts/setup-ubuntu.sh --cross
scripts/setup-ubuntu.sh --cross --check
```

The prerequisites include Python 3.12+, OpenSSH client, sshpass, CMake 3.28 or newer, Ninja, Git, pkg-config, readelf, and the selected GCC toolchain. The cross setup installs the AArch64 compiler and binutils. Setup uses system package installation and does not change apt sources or enable multiarch. `--dry-run` prints the package commands without installing them.

Configure and compile the cross artifact from the repository root:

```sh
cmake --preset linux-arm64-cross
cmake --build --preset linux-arm64-cross
file build/linux-arm64-cross/bin/seedlessds-headless
aarch64-linux-gnu-readelf -d build/linux-arm64-cross/bin/seedlessds-headless
aarch64-linux-gnu-readelf --version-info build/linux-arm64-cross/bin/seedlessds-headless
```

On ARM64 Ubuntu, select the native preset and use native tools; CTest can run on that host:

```sh
cmake --preset linux-arm64-native
cmake --build --preset linux-arm64-native
file build/linux-arm64-native/bin/seedlessds-headless
readelf -d build/linux-arm64-native/bin/seedlessds-headless
ctest --preset linux-arm64-native
```

CMake automatically runs `scripts/prepare-core.sh` to stage the pinned core and verify the ordered compatibility patch hashes. The build compiles Lua, libarchive, unarr, the LZMA SDK, zlib and libm into the application, then audits the executable. Each preset writes `build/<preset>/bin/seedlessds-headless` and `build/<preset>/runtime-inventory.json`. The inventory includes the artifact hash and ELF runtime requirements. Cross artifacts cannot run on amd64; both presets produce Linux AArch64 executables for a compatible ARM64 Linux host or device. Loader, libc and required GLIBC versions depend on the artifact and target. Check the generated inventory and ELF on every build instead of assuming a fixed GLIBC baseline. Set `SEEDLESS_TARGET_SYSROOT=/absolute/path/to/sysroot` before a clean cross configuration to target a different ABI; clear that preset's build directory before reconfiguring for another sysroot.

The runner takes a readable ROM and an existing writable data directory:

```sh
mkdir -p ./run-data/system
# Supply BIOS files in run-data/system before launching (see below).
./build/linux-arm64-native/bin/seedlessds-headless --help
./build/linux-arm64-native/bin/seedlessds-headless --data-dir "$(realpath ./run-data)" --frames 120 /path/to/test.nds
```

`--frames N` stops at exactly N scheduler boundaries. Without it, the runner continues until SIGINT or SIGTERM. Final output contains a `result=...` record. ROMs and BIOS/firmware images are not distributed. The core requires `system/nds_bios_arm9.bin` (4096 bytes) and `system/nds_bios_arm7.bin` (16384 bytes) under `--data-dir`; supported replacement images may use `nds_bios_arm9_replacement.bin` and `nds_bios_arm7_replacement.bin` instead. Before creating the core, the frontend checks that both BIOS images are readable regular files of the required sizes. If either is missing or invalid, it logs each affected BIOS to stderr and exits immediately with exit 1 and `result=failed frames=0 operation=bios`. Optional firmware belongs in the same `system/` directory; user data is stored under `User/`.

## Debug

`scripts/debug-device.py` deploys the ARM64 runner to a configured Linux console over SSH, verifies the transfer hash, and performs a bounded run. It requires the host prerequisites installed above, a user-controlled ARM64 Linux device with root password SSH authentication, OpenSSH host verification, compatible loader/libc, and target `sh`, `mkdir`, `cat`, `chmod`, `mv`, `rm`, `sha256sum`, `timeout` and `ldd`. Supply an existing readable ROM and writable data directory on the device. Supply the required BIOS files under `--data-dir/system/` as described above.

The tracked `.device.example.json` documents `ip_address`, `root_password` and `install_dir`. Ask the device owner for these values before creating the ignored `.device.local`; do not put actual values in tracked files, shell commands or shared logs. The local file must be regular, owned by the current user and mode 0600. To create it, copy the template and edit locally with a private editor, then restrict its permissions:

```sh
cp .device.example.json .device.local
chmod 600 .device.local
$EDITOR .device.local
git check-ignore .device.local
git check-ignore .device.example.json
python3 scripts/debug-device.py check
```

The template remains tracked, while `.device.local` is ignored. `check` is local-only and never connects. It rejects placeholders, malformed fields, unsafe permissions and unsupported path values without displaying the password. Existing valid settings are reused by later runs. No command-line password option exists.

Before the first automated connection, verify the SSH host fingerprint against the console or another trusted source and enroll it interactively. For example, run `ssh root@<CONSOLE_IP>` from the repository host, compare the presented fingerprint to the trusted value, accept it only after it matches, then exit. Automation keeps strict host checking enabled and stops on an unknown or changed key. The transport passes the password through an anonymous file descriptor; remote paths are quoted so spaces and shell metacharacters remain literal.

Run from the repository root, replacing the artifact preset and target asset paths as appropriate:

```sh
python3 scripts/debug-device.py run \
  --binary build/linux-arm64-cross/bin/seedlessds-headless \
  --inventory build/linux-arm64-cross/runtime-inventory.json \
  --rom /absolute/device/path/test.nds \
  --data-dir /absolute/device/path/run-data \
  --frames 120 --timeout 30
```

The tool checks the ELF and fresh inventory before connecting, then checks target architecture, runtime and assets. It creates the configured install directory when safe, stages the binary under a unique temporary name, verifies SHA-256 and atomically promotes it as `seedlessds-headless`. Existing executables remain in place if staging or verification fails. A destination lock prevents overlapping sessions; after a crash, inspect the lock directory and its `session` marker in the configured destination, then remove it only after confirming no workflow remains active. The workflow removes a lock only when its marker still matches the current session. Defaults are 120 frames, 10 seconds for SSH connection, 120 seconds for transfer and 30 seconds for execution; execution sends TERM and then KILL after five seconds.

Diagnostics identify the failing stage (`configuration`, `artifact`, `connection`, `compatibility`, `deployment` or `execution`). Exit 0 means a verified deployment and runner exit 0 with the exact requested frame completion record. Exit 2 reports local input/artifact issues, 3 connection/authentication/host verification, 4 target compatibility/assets, 5 transfer or promotion, 6 runner failure or timeout, and 7 interrupted or unconfirmed execution. SSH exit 255 is never treated as a runner result. Sanitized per-session records are written under ignored `build/validation/device/`; inspect them before summarizing a device run. Optional deeper target checks are available through `scripts/validate-headless.sh BINARY ROM DATA_DIR` and require Bash, ripgrep and timeout on the device.

### Hardware validation record

**Fixed and hardware validated on 2026-10-19.** Missing BIOS files caused machine initialization to call `longjmp` before its jump buffer had been initialized. The Linux compatibility patch now establishes the startup failure target before initialization and calls `setjmp` directly so the compiler observes its returns-twice semantics. Missing or incorrectly sized BIOS files report the required filenames and sizes and exit 1 instead of crashing. BIOS files belong under `--data-dir/system/`; the previous `SeedlessDS/` guidance was incorrect for the POSIX adapter.

The final cross build (`/tmp/seedlessds-device-cross/bin/seedlessds-headless`, SHA-256 `2445062962015cd08214e50735e5aeb3aa155555245ca19d3f97adc81a7f5ff7`) passed verified deployment and exact 1-frame and 120-frame runs with two ROMs on the RG DS Plus (Linux 7.2.7, AArch64, GNU libc 2.44). A separate empty-data-root regression returned `result=failed frames=0 operation=load`, exit 1, with the BIOS diagnostic. Existing device BIOS images were copied into the isolated validation data root; no ROM or BIOS content is distributed. Sanitized session records are in ignored `build/validation/device/`. Bounded hardware acceptance FR-009/SC-002 now passes. No commits or remote Git operations were performed.

See [core update procedure](docs/core-updates.md) before changing the core revision or compatibility patches.
