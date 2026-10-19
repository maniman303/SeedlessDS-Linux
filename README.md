# SeedlessDS-Linux

SeedlessDS-Linux builds a Linux ARM64 frontend around the pinned SeedlessDS-Core. Video is presented fullscreen through SDL2; audio output and microphone input are discarded.

## Build

Run commands from the repository root on Ubuntu 24.04 or 26.04. Supported hosts are Ubuntu ARM64 for a native build and Ubuntu amd64 for an ARM64 cross build. Windows x86-64 development uses WSL2 Ubuntu; keeping the checkout in the WSL Linux filesystem is recommended. Ensure the pinned core and required dependencies are already available locally. Configure never initializes a missing Git submodule or downloads SDL; cached pinned archives may be reused.

`--native` requires ARM64 Ubuntu; `--cross` requires amd64 Ubuntu. The setup script checks prerequisites and prints manual package guidance; it never installs packages or invokes sudo. Install packages yourself if needed, then run `--check`:

```sh
scripts/setup-ubuntu.sh --native --dry-run
scripts/setup-ubuntu.sh --native --check
```

For Ubuntu amd64 or WSL2 Ubuntu, use:

```sh
scripts/setup-ubuntu.sh --cross --dry-run
scripts/setup-ubuntu.sh --cross --check
```

The prerequisites include Python 3.12+, OpenSSH client, sshpass, CMake 3.28 or newer, Ninja, Git, pkg-config, readelf, the selected GCC toolchain, and SDL2 2.0.12 or newer. Setup does not install packages, change apt sources, or enable multiarch. `--dry-run` prints manual package guidance; run any privileged package command yourself, confirm it succeeded, then run `--check`.

For a native Ubuntu ARM64 build, install the distribution `libsdl2-dev` package when needed. For an amd64 cross build, the simplest path is Ubuntu/Debian's AArch64 multiarch development package (`libsdl2-dev:arm64`). If apt does not know the `arm64` architecture yet, enable it and install the target package manually:

```sh
sudo dpkg --add-architecture arm64
sudo apt-get update
sudo apt-get install libsdl2-dev:arm64
```

CMake reads only `/usr/lib/aarch64-linux-gnu/pkgconfig` and links the target SDL2 library, never the amd64 one. Alternatively, set `SEEDLESS_TARGET_SYSROOT` to a complete ARM64 sysroot. The ROCKNIX device supplies SDL2 at runtime; the executable uses the stable SDL2 shared SONAME and this project does not bundle or replace the device's SDL2. The repository does not enable multiarch, provision sysroots, or install target packages automatically.

Configure and compile the cross artifact from the repository root:

```sh
scripts/build.sh --cross
file build/linux-arm64-cross/bin/seedlessds
aarch64-linux-gnu-readelf -d build/linux-arm64-cross/bin/seedlessds
aarch64-linux-gnu-readelf --version-info build/linux-arm64-cross/bin/seedlessds
```

On an ARM64 Ubuntu host, run `scripts/build.sh --native`; without a mode, the script selects the
native preset on ARM64 and the cross preset on amd64. Add `--clean` to remove only that preset's
build directory before CMake reconfigures and reacquires any missing pinned build archives. The
shared pinned core-submodule cache under `build/downloads` is preserved.

On ARM64 Ubuntu, select the native preset and use native tools; CTest can run on that host:

```sh
cmake --preset linux-arm64-native
cmake --build --preset linux-arm64-native
file build/linux-arm64-native/bin/seedlessds
readelf -d build/linux-arm64-native/bin/seedlessds
ctest --preset linux-arm64-native
```

CMake automatically runs `scripts/prepare-core.sh` to stage the pinned core and verify the ordered compatibility patch hashes. Use only the preset's canonical build directory (`build/linux-arm64-cross` or `build/linux-arm64-native`); do not create suffixed alternatives. The build compiles Lua, libarchive, unarr, the LZMA SDK, zlib and libm into the application, then audits the executable. Each preset writes `build/<preset>/bin/seedlessds` and `build/<preset>/runtime-inventory.json`. The inventory includes the artifact hash and ELF runtime requirements. Cross artifacts cannot run on amd64; both presets produce Linux AArch64 executables for a compatible ARM64 Linux host or device. Loader, libc and required GLIBC versions depend on the artifact and target. Check the generated inventory and ELF on every build instead of assuming a fixed GLIBC baseline. Set `SEEDLESS_TARGET_SYSROOT=/absolute/path/to/sysroot` before a clean cross configuration; clear only the canonical preset directory before reconfiguring for another sysroot.

The frontend reads `$HOME/.config/seedlessds/config.json` by default. A complete example and validation rules are documented in [the config contract](specs/003-frontend-json-config/contracts/configuration.md). It groups rendering options under `visuals`, BIOS and optional firmware paths under `firmware`, the DS nickname/month/day/color under `user`, the battery-save directory under `emulation`, and display preferences under `video`. The first ROM launch creates this file with defaults and logs the missing path. A supplied `--config PATH` selects the only config file used; it is also where missing defaults are created. The JSON file is never rewritten during launch.

The default BIOS paths are `$HOME/.config/seedlessds/bios/drastic_bios_arm7.bin` (16384 bytes) and `$HOME/.config/seedlessds/bios/drastic_bios_arm9.bin` (4096 bytes). `firmware_path` defaults to empty. User defaults are nickname `SEEDLESS`, birthday month 3/day 2, and color 0. Save paths default to the ROM directory; `--save-dir DIR` overrides the JSON setting, and an explicitly empty argument selects the ROM directory. The CPU backend is always JIT, the clock always uses real time, and saves always use raw `.sav`; these policies are not configurable.

## Fullscreen video

Gameplay always opens fullscreen SDL2 output. The required installed SDL2 version is at least 2.0.12. On dual-output systems, `video.display_mode` accepts `auto` (default), `dual_screen`, or `single_screen`: `auto` and `dual_screen` use output indices 0 and 1 when available and fall back to one output; `single_screen` always uses index 0. Additional outputs are unused. The mode requested in JSON is retained even when the current device uses the one-output fallback.

`video.swap_screens` defaults to `false` and exchanges top/bottom image destinations. In single-output mode, `video.single_screen_layout` defaults to `vertical` (top above bottom) and can be `horizontal` (top left of bottom). It is ignored for drawing while dual output is active. The complete config example is:

```json
"video": {
  "display_mode": "auto",
  "swap_screens": false,
  "single_screen_layout": "vertical"
}
```

Each screen is scaled by the largest whole-number factor that fits its output; single-output layouts use one factor for both screens. SDL nearest sampling and disabled blending preserve sharp source pixels. The presentation is centered with black borders, without crop or fractional stretching. If the selected layout cannot fit at 1x, startup fails instead of changing the layout. All three preferences live only under the top-level `video` object; fullscreen and integer scaling are fixed behavior.

SDL2 is a shared runtime dependency. Native ARM64 builds use the host distribution package; cross builds can use the target-architecture multiarch development package or a complete ARM64 sysroot. Runtime loading uses the target's SDL2 and its compiled-in video backends, including ROCKNIX's SDL2 2.32.10 installation. Setup checks target pkg-config metadata and prints guidance but leaves package installation to the user. The installed SDL license notice is [licenses/SDL2.txt](licenses/SDL2.txt).

The frontend accepts a ROM plus `--config`, `--save-dir`, and `--frames`; it has no BIOS-path or general data-root command-line options. `--help` exits before config access, and launching without a ROM logs the missing argument and exits cleanly. `--frames N` stops at exactly N scheduler boundaries. Without it, the runner continues until SIGINT or SIGTERM. Final output contains a `result=...` record.

```sh
./build/linux-arm64-native/bin/seedlessds --help
./build/linux-arm64-native/bin/seedlessds --frames 120 /path/to/test.nds
./build/linux-arm64-native/bin/seedlessds --config "$HOME/test config.json" --save-dir "$HOME/game saves" /path/to/test.nds
```

## Debug

`scripts/debug-device.py` deploys the ARM64 runner to a configured Linux console over SSH, verifies the transfer hash, and performs a bounded run. It requires the host prerequisites installed above, a user-controlled ARM64 Linux device with root password SSH authentication, OpenSSH host verification, compatible loader/libc, and target `sh`, `mkdir`, `cat`, `chmod`, `mv`, `rm`, `sha256sum`, `timeout` and `ldd`. `rom_dir` must contain a readable ROM unless `--rom` names a different one; configure BIOS and optional firmware paths in the device JSON config as described above.

The tracked `.device.example.json` documents `ip_address`, `root_password`, `install_dir` and `rom_dir`. Ask the device owner for these values before creating the ignored `.device.local`; do not put actual values in tracked files, shell commands or shared logs. The local file must be regular, owned by the current user and mode 0600. To create it, copy the template and edit locally with a private editor, then restrict its permissions:

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
  --binary build/linux-arm64-cross/bin/seedlessds \
  --inventory build/linux-arm64-cross/runtime-inventory.json \
  --rom /absolute/device/path/test.nds \
  --frontend-config /absolute/device/path/config.json \
  --save-dir /absolute/device/path/saves \
  --frames 120 --timeout 30
```

The tool checks the ELF and fresh inventory before connecting, then checks target architecture, runtime and ROM readability. `--rom` is optional; if omitted, the first readable `.nds` file in configured `rom_dir` is selected. Configure BIOS/firmware on the device through the frontend JSON file; `--frontend-config` selects it and `--save-dir` can override its save path. It creates the configured install directory when safe, stages the binary under a unique temporary name, verifies SHA-256 and atomically promotes it as `seedlessds`. Existing executables remain in place if staging or verification fails. A destination lock prevents overlapping sessions; after a crash, inspect the lock directory and its `session` marker in the configured destination, then remove it only after confirming no workflow remains active. The workflow removes a lock only when its marker still matches the current session. Defaults are 120 frames, 10 seconds for SSH connection, 120 seconds for transfer and 30 seconds for execution; execution sends TERM and then KILL after five seconds.

Diagnostics identify the failing stage (`configuration`, `artifact`, `connection`, `compatibility`, `deployment` or `execution`). Exit 0 means a verified deployment and runner exit 0 with the exact requested frame completion record. Exit 2 reports local input/artifact issues, 3 connection/authentication/host verification, 4 target compatibility/assets, 5 transfer or promotion, 6 runner failure or timeout, and 7 interrupted or unconfirmed execution. SSH exit 255 is never treated as a runner result. Sanitized per-session records are written under ignored `build/validation/device/`; inspect them before summarizing a device run. Optional deeper target checks are available through `scripts/validate-video.sh BINARY ROM CONFIG` and require Bash, ripgrep and timeout on the device.

### Hardware validation record

**Historical hardware validation for the pre-JSON runner (2026-10-19).** Missing BIOS files caused machine initialization to call `longjmp` before its jump buffer had been initialized. The Linux compatibility patch now establishes the startup failure target before initialization and calls `setjmp` directly so the compiler observes its returns-twice semantics. Missing or incorrectly sized BIOS files report the required filenames and sizes and exit 1 instead of crashing. The JSON-config frontend now uses explicit BIOS paths in `firmware`; this historical record does not validate the JSON-config feature.

**JSON config hardware validation (2026-10-19).** The current local AArch64 build (`34c18077e24fb3ab8a5c5ee4b56f87e3acf5b6606381a123f788969492fde8f9`) was deployed over SSH to the RG DS Plus (Linux 7.2.7, AArch64, GNU libc 2.44). With `rom_dir` set to `/storage/roms/nds`, automatic ROM selection loaded a ROM and emulation completed exactly one frame (`result=completed frames=1 requested=1`). A preceding startup probe caused the frontend to create the missing default config. The generated config selected the default ARM7 and ARM9 BIOS paths; both target files were readable and had the required 16,384-byte and 4,096-byte sizes. This verifies a one-frame JSON-config launch with those BIOS paths; it does not claim longer-run or shutdown-boundary acceptance. Sanitized evidence: `build/validation/device/20261019T202131Z-42535906.json`.

The final cross build (`/tmp/seedlessds-device-cross/bin/seedlessds-headless`, SHA-256 `2445062962015cd08214e50735e5aeb3aa155555245ca19d3f97adc81a7f5ff7`) passed verified deployment and exact 1-frame and 120-frame runs with two ROMs on the RG DS Plus (Linux 7.2.7, AArch64, GNU libc 2.44). A separate empty-data-root regression returned `result=failed frames=0 operation=load`, exit 1, with the BIOS diagnostic. Existing device BIOS images were copied into the isolated validation data root; no ROM or BIOS content is distributed. Sanitized session records are in ignored `build/validation/device/`. Bounded hardware acceptance FR-009/SC-002 now passes. No commits or remote Git operations were performed.

See [core update procedure](docs/core-updates.md) before changing the core revision or compatibility patches.
