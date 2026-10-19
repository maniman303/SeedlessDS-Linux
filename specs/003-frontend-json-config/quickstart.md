# Quickstart Validation: Frontend JSON Configuration

This guide describes the implemented Linux JSON config and launch arguments, plus validation that still requires ARM64 hardware.

## Prerequisites

Use an existing Linux terminal, locally available pinned core/dependency sources, configured ARM64 native or cross toolchain, and a legally supplied readable `.nds` ROM plus valid ARM7/ARM9 BIOS images. Optional firmware must be supported by the pinned core. Inspect build/deploy scripts for prohibited actions before running. No remote Git, GitHub, commits, or sudo execution by agents. If packages are missing, have the user install them manually and confirm completion before continuing.

## Build and automated checks

Use cached sources only; do not let missing dependencies trigger fetching. The host CTest suite runs from a standalone core-free project. `bash scripts/prepare-core.sh linux-arm64-cross` succeeds locally and applies the six checksum-verified compatibility patches. The pre-existing cross CMake cache points to `/mnt/d/Programowanie/SeedlessDS-Linux`, so it cannot be reused. A fresh configure using the staged core and cached zlib stopped because the pinned libarchive source archive is not cached and the environment could not resolve its download host. The workspace also has no target SDL2 sysroot, and no native ARM64 host is available.

```sh
cmake -S tests/config -B build/config-tests
cmake --build build/config-tests
ctest --test-dir build/config-tests --output-on-failure
# Production cross configure/build requires the pinned third-party source archives
# (libarchive, unarr, LZMA) and a configured target SDL2 sysroot; do not run until cached.
```

The production build remains blocked by the uncached libarchive archive and absent target SDL2 sysroot. A native ARM64 build and real-device validation were not available in this session. Do not execute an ARM64 binary directly on an amd64 host.

## First-launch and argument checks (ARM64)

Set paths to your existing ROM and binary; create an isolated home. Supply valid ROM even for first-launch tests. Shell variables below are task-specific and never replace HOME in the shell itself.

```sh
seedless_test_home=$(mktemp -d)
seedless_bin="$(pwd)/build/linux-arm64-native/bin/seedlessds-headless"
seedless_rom=/absolute/path/to/game.nds
"$seedless_bin" --help
env HOME="$seedless_test_home" "$seedless_bin"
env HOME="$seedless_test_home" "$seedless_bin" --frames 1 "$seedless_rom"
cat "$seedless_test_home/.config/seedlessds/config.json"
```

Help exits 0 without IO. No ROM logs the missing argument and exits 2 without creating config. The first valid-ROM launch logs missing config and creates all defaults, then exits 1 for absent BIOS images. Inspect all fields against [configuration.md](contracts/configuration.md), especially birthday integers 3/2, empty firmware/save values, and no CPU/RTC/save-format setting.

Place valid BIOS images at the generated paths (or edit the two path fields). Repeat with `--frames 120`; expect exactly 120 boundaries, fixed JIT/real-time/raw policies, and no configuration rewrite. Repeat without `--frames` and use SIGTERM; expect clean shutdown and existing interruption status.

## Custom configuration and overrides

```sh
seedless_custom="$seedless_test_home/custom settings/config.json"
env HOME="$seedless_test_home" "$seedless_bin" --config "$seedless_custom" --frames 1 "$seedless_rom"
```

Verify missing parents and defaults are created only at the custom path. Edit it with valid assets and a nonempty `emulation.save_dir`. Give the default file conflicting paths or invalid contents; it must not be accessed. Test relative custom paths from a known working directory and filenames containing spaces. Record config hashes before/after valid launches and failures; existing files stay byte-identical.

```sh
"$seedless_bin" --config "$seedless_custom" --save-dir "$seedless_test_home/override saves" --frames 120 "$seedless_rom"
"$seedless_bin" --config "$seedless_custom" --save-dir "" --frames 120 "$seedless_rom"
```

Use a save-capable game and allow enough execution/user interaction to save; 120 frames alone does not prove a cartridge write. The first command loads/writes only the override directory; the second selects the ROM directory. Restart to verify game-level reload. No implicit `backup/` or `.dsv`, and configured directory remains unchanged. Without override, use configured directory. Verify a CLI override avoids probing an overridden unwritable directory while malformed JSON still fails.

## Negative and boundary matrix

Automate missing/unreadable custom files, invalid JSON/duplicate keys/unknown keys/trailing bytes/Unicode, absent required fields, all invalid value types, oversized file/path/depth, creation races, partial writes and ENOSPC through controlled fault injection. Run permission tests unprivileged so root permissions cannot mask them. Assert no default fallback, no overwritten existing config, and no emulation on startup failure.

Profile cases: 10 vs 11 UTF-16 units, multibyte UTF-8 and surrogate-pair boundaries, months 0/1/12/13, February 29 allowed/30 rejected, April 31 rejected, day 0 rejected, colors -1/0/15/16, and fractional/boolean/string birthday values rejected. Reject birthday year/combined date and forbidden policy keys.

Visual cases: boolean toggles, thread counts 0/1/15 accepted, -1/16/noninteger rejected; observe actual core config and thread scheduling rather than infer correctness only from JSON. Optional firmware empty must not load legacy firmware; supported supplied image loads with configured user profile, invalid/unreadable image fails cleanly.

Save cases: missing destination created, path-is-file/unwritable destination failure, existing unreadable/corrupt save preserved, simultaneous writer rejected, mid-run and final-flush write failure reported as exit 1. Verify old `.dsv` is never silently selected.

CLI cases: old `--data-dir`, BIOS args, unknown/duplicate options, missing values, multiple/no ROM, frame count 0/overflow, empty save override, and `--` filename handling. Preserve result records for existing device tooling.

## Real-device acceptance

After local builds/tests, use the updated device workflow over SSH with ignored local device settings. Validate both BIOS paths, optional firmware/profile application, actual JIT/RTC/visual mapping, bounded frames, real save-write/reload and shutdown failures on ARM64 hardware. Record device model, OS, build/runtime inventory, scenarios and results; omit credentials. No display layout change is introduced, so separate new single/dual-screen presentation testing is outside this change.
