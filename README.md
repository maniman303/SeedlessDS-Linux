# SeedlessDS-Linux

SeedlessDS-Linux builds a minimal Linux ARM64 runner around the pinned SeedlessDS-Core. It runs emulation without opening a window, audio device or microphone. The core keeps software video and SPU emulation active; host presentation and audio output are discarded.

The current verified build host is Ubuntu 26.04 amd64. On that host, install the project toolchain with:

```bash
bash scripts/setup-ubuntu.sh --cross
bash scripts/setup-ubuntu.sh --cross --check
```

On Ubuntu ARM64, use `--native` for both commands. The setup script installs Ubuntu packages system-wide through apt and sudo; it does not change apt sources or enable multiarch. Supported development releases are Ubuntu 24.04 and 26.04. WSL2 Ubuntu is supported; building in the Linux filesystem is recommended for performance.

Configure and build the cross artifact with:

```bash
cmake --preset linux-arm64-cross
cmake --build --preset linux-arm64-cross
file build/linux-arm64-cross/bin/seedlessds-headless
aarch64-linux-gnu-readelf -d build/linux-arm64-cross/bin/seedlessds-headless
```

On ARM64 use the `linux-arm64-native` preset and run `ctest --preset linux-arm64-native`. Builds stage the pinned core in `build/<preset>/core-src`, verify the ordered compatibility patch hashes, and build Lua, libarchive, unarr, the LZMA SDK, zlib and libm into the application. The upstream checkout remains untouched. The build generates `runtime-inventory.json` and fails if Android, OpenSL or OpenGL dependencies remain. The reference cross artifact currently needs only the target glibc loader and libc dynamically; the current reference artifact imports symbols through GLIBC 2.34 (GLIBC 2.34 or newer is required unless rebuilt against another sysroot). Set `SEEDLESS_TARGET_SYSROOT` before configuring the cross preset to target a device with a different Linux ABI.

The runner accepts one readable ROM and an existing writable data directory:

```bash
mkdir -p ./run-data
./build/linux-arm64-cross/bin/seedlessds-headless --help
./build/linux-arm64-cross/bin/seedlessds-headless --data-dir "$(realpath ./run-data)" --frames 120 /path/to/test.nds
```

`--frames N` stops at exactly N scheduler boundaries. Without it, the runner continues until SIGINT or SIGTERM. Successful and interrupted runs print one final `result=...` record. ROMs and any BIOS/firmware images are not distributed or downloaded. Core assets, when needed, belong under the selected data root at `SeedlessDS/`; user data is stored under `User/`. The core recognizes `nds_bios_arm9.bin`, `nds_bios_arm7.bin`, replacement BIOS files and `nds_firmware_modified.bin`; actual asset requirements depend on the ROM and still need confirmation on ARM64 hardware.

Cross builds cannot run on amd64. Transfer the artifact using your existing device access and test it on ARM64 Linux with a legally usable ROM. `specs/001-headless-core-runner/quickstart.md` contains the hardware acceptance workflow. Do not treat a successful cross-link as a real-device run.

See [core update procedure](docs/core-updates.md) before changing the core revision or compatibility patches.
