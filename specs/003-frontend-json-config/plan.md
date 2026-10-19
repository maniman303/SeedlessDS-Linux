# Implementation Plan: Frontend JSON Configuration

**Branch**: `main` (unchanged; no feature branch created) | **Date**: 2026-10-19 | **Spec**: [spec.md](spec.md)

**Input**: `specs/003-frontend-json-config/spec.md` and explicit requirement to separate configuration into new C/header files.

## Summary

Add a strict typed JSON configuration with authoritative custom-file selection, validated firmware/profile/visual settings, and command-line save-directory precedence. Keep JIT, real-time RTC, and raw cartridge saves fixed. Put configuration lifecycle and validation in `src/config.c`/`src/config.h`; extract launch parsing into `src/launch_options.c`/`.h` and isolate core translation in `src/core_config.c`/`.h`. `runner.c` retains lifecycle orchestration, frame control, signals, and teardown, not JSON parsing, settings validation, or packed configuration arithmetic.

## Technical Context

**Language/Version**: C11 with existing GNU extensions; Python 3 for integration tests and tooling.

**Primary Dependencies**: Existing pinned SeedlessDS-Core, POSIX filesystem APIs, pthreads, existing CMake 3.28+ build. A small repository-owned strict JSON codec avoids another runtime dependency or remote source retrieval; its syntax/Unicode behavior receives dedicated tests.

**Storage**: One JSON file, user-supplied BIOS/optional firmware, and raw `.sav` files. Configuration defaults under `$HOME/.config/seedlessds`; empty save path resolves to the canonical ROM directory.

**Testing**: Core-independent C configuration/CLI tests, Python subprocess/filesystem contract tests, ARM64 native CTest, cross-build and runtime ABI audit, real ARM64 device validation over SSH.

**Target Platform**: ARM64 Linux; native ARM64 and cross-compilation from amd64 Linux/Ubuntu in WSL2.

**Project Type**: Existing headless Linux frontend CLI; no new graphical settings UI.

**Performance Goals**: Parse configuration once before startup; no JSON or configuration-file IO in the emulation frame loop. Preserve bounded scheduler-frame accounting.

**Constraints**: No remote Git, GitHub access, commits, pushes, or agent-executed privileged commands. Maintain pinned-core ABI/layout. No BIOS copies, fallback asset discovery, or implicit backup directory. No unvalidated numeric narrowing.

**Scale/Scope**: Four JSON groups, five launch input categories, one ROM/session, no per-game config layer. Limit configuration input to 64 KiB and nesting to 8 levels with explicit diagnostics; paths must fit downstream core buffers without truncation.

## Constitution Check

Initial and post-design gates pass:

- I: ARM64 Linux remains primary; configuration and parser modules are portable C/POSIX.
- II: No graphics/input replacement; SDL preference and single/dual-screen obligations remain intact.
- III: Frontend owns configuration; core adaptation uses a documented Linux compatibility API applied through a tracked patch. No frontend writes to private core offsets or reimplements firmware/save engines.
- IV: Wire new files into existing native/cross targets; require both builds and runtime inventory checks during implementation. Cache-only preparation; inspect invoked scripts for prohibited remote operations first.
- V: Final firmware, JIT, RTC, visual-setting application, and save/reload checks run on real ARM64 hardware using existing device workflow with local, ignored credentials.
- VI: All work local. No remote actions or commits. Existing build/deploy scripts must be checked before invocation. Missing dependencies requiring sudo are handed to the user with the exact command, then await explicit completion.

Planning changes documentation only; implementation build/device gates are planned, not claimed completed.

## Project Structure

### Documentation (this feature)

```text
specs/003-frontend-json-config/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
└── contracts/
    ├── configuration.md
    ├── cli.md
    └── core-adapter.md
```

`tasks.md` will be produced by speckit-tasks.

### Source Code (repository root)

```text
src/
├── config.c / config.h                 # typed settings, defaults, validation, file lifecycle
├── config_json.c / config_json.h       # bounded strict JSON codec; no core dependency
├── launch_options.c / launch_options.h # CLI parsing/help; explicit override presence
├── core_config.c / core_config.h       # validated options -> supported core API
├── runner.c / runner.h                 # startup/run/stop orchestration
└── platform/linux/                    # existing adapter, including save error propagation
patches/seedlessds-core/
├── 0006-frontend-config-paths.patch    # proposed explicit Linux configuration bridge
└── manifest.json                      # ordered patch and checksum registration
tests/
├── config/CMakeLists.txt              # independent host-only test project
├── test_config.c
├── test_launch_options.c
├── test_config_cli.py
└── abi/core_layout.c
```

**Structure Decision**: Module headers expose owned typed settings and explicit result/error types. JSON codec remains private to configuration. Launch options do not mutate persisted settings. The core bridge owns copied strings until ROM teardown. Runner calls these boundaries and handles one cleanup path. Configuration code is independently testable without the ARM64 core; use a separate host test build when production CMake correctly rejects non-ARM64 targets.

## Delivery and Validation Sequence

1. Implement strict JSON/configuration and launch modules with host contract tests, including Unicode, duplicate keys, creation races, and absent versus empty overrides.
2. Add the explicit Linux core bridge patch and ABI checks for settings application, exact image/save paths, fixed policies, and persistent save error reporting.
3. Integrate the new modules into runner startup/cleanup and CMake without expanding runner's parsing responsibilities.
4. Update README, `scripts/validate-video.sh`, `scripts/debug-device.py`, and its tests for `--config` and `--save-dir`; remove `--data-dir` from frontend invocations while retaining `--frames`. Keep device inventory/transport options independent from frontend settings.
5. Validate native/cross builds, filesystem/CLI integration, and real-device BIOS/firmware/profile/visual/JIT/RTC/save scenarios. Record observed device and runtime details without credentials.

No constitutional exceptions or complexity waivers are needed.
