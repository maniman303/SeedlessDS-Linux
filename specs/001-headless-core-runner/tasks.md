# Tasks: Headless Core Runner and Ubuntu Build Setup

**Input**: `specs/001-headless-core-runner/` — spec.md, plan.md, research.md, data-model.md, contracts/, quickstart.md

**Organization**: Setup, shared foundation, US1 (P1), US2 (P1), US3 (P2), and final verification. The Ubuntu installer is implemented and actually run in Setup because all build work needs it; US3 retains clean-environment and repeatability acceptance.

**Validation**: Include the build, smoke-run, dependency, failure, and installation checks explicitly required by the specification. No broad unit-test suite or TDD requirement is added.

## Format and Paths

Each task uses `- [ ] Tnnn [P?] [USn?] description with paths`. `[P]` denotes independent files within the explicitly stated dependency wave; it does not authorize concurrent edits to shared build/manifest files. Paths are repository-relative. Execution evidence goes under ignored `build/validation/`; create it before recording results. Never check a task solely because its script exists when the task requires execution.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare the repository and install the actual local build toolchain before building.

- [X] T001 Configure `.gitignore` for `build/`, local device configuration and runtime assets; create planned source/build/script directories and record baseline Git/submodule status in `build/validation/baseline.txt` without altering existing user changes.
- [X] T002 Implement `scripts/setup-ubuntu.sh` with exactly one `--native` or `--cross` mode plus `--check` or `--dry-run`, Ubuntu 24.04/26.04 and architecture guards, complete package sets from `specs/001-headless-core-runner/contracts/build-and-setup.md`, apt/sudo error handling, no silent repository/multiarch edits, and version/triple/AArch64 compile-link verification; read-only checks remove temporary outputs and cross checks never execute ARM64 binaries on amd64.
- [X] T003 Run `bash -n scripts/setup-ubuntu.sh` and `bash scripts/setup-ubuntu.sh --cross --dry-run` on this amd64 Ubuntu environment; inspect the resolved package/install commands and record output in `build/validation/tooling-dry-run.txt` before installation. Use native mode instead only if the detected host is ARM64.
- [X] T004 **Run the installer locally** using `bash scripts/setup-ubuntu.sh --cross` on this amd64 Ubuntu host (or `--native` on ARM64), then run the same mode with `--check` and run ShellCheck on `scripts/setup-ubuntu.sh`; record installed tools, versions, compiler triple and linked AArch64 probe inspection in `build/validation/tooling-installed.txt`. Use the environment's required escalation for apt installation; if privilege or network access prevents installation, leave this task incomplete and report the exact failed command and recovery action. A dry-run or instructions for the user do not satisfy this task.

**Checkpoint**: Tooling is installed and verified on the working Ubuntu host. T004 blocks compilation-dependent work.

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish target isolation, staged core integration and ABI gates shared by all stories.

- [X] T005 Create `patches/seedlessds-core/manifest.json` recording base commit `1cae6f9d8ad7dc651a48452a5ebe23fd8c347d12`, expected module/source membership, explicit Android/presentation replacement inventory, patch ordering/hashes and reasons; inspect current source against `specs/001-headless-core-runner/research.md` and fail on unexpected drift rather than assuming the revision still matches.
- [X] T006 Implement `scripts/prepare-core.sh` to initialize/verify required nested submodules, diagnose local source changes without discarding them, stage sources under `build/<preset>/core-src`, validate manifest/context/hash checks and apply patches without fuzz; ensure unchanged verified inputs are repeatable and changed inputs invalidate staging, leaving `external/SeedlessDS-Core/` untouched.
- [X] T007 [P] Implement `cmake/Dependencies.cmake` with verified pinned zlib URL/SHA-256 and the core's pinned Lua/archive/LZMA source dependencies; preserve `LUA_32BITS`, `LUA_COMPAT_5_2` and file redirection, correct third-party subdirectory context, target-specific download/build caches and explicit static targets with complete transitive linkage; fail clearly on missing/unverified inputs or unavailable static archives.
- [X] T008 [P] Implement `cmake/toolchains/aarch64-linux-gnu.cmake` with Linux/aarch64 compilers, target-only include/library/package lookup, host program lookup, and validated `SEEDLESS_TARGET_SYSROOT` override; keep reference Ubuntu and device sysroot modes explicit and reject incompatible target configuration.
- [X] T009 Add root `CMakeLists.txt`, `CMakePresets.json` and `cmake/SeedlessCore.cmake` wiring T006–T008 into separate `linux-arm64-native` and `linux-arm64-cross` build directories; create the expected static `recon_fn` target, consume reviewed module lists with exact replacements, enable GNU C11/AArch64 ASM, threads/math/dl as needed, static compiler support where compatible, output `build/<preset>/bin/seedlessds`, and fail on non-ARM64 runtime targets. Do not mask unresolved symbols.
- [X] T010 Add target compile/layout probes in `tests/abi/core_layout.c` and `cmake/SeedlessCore.cmake` for raw jump storage, pthread object size/alignment, fixed stat consumers, 16 KiB region/page assumptions and core offsets; document every mismatch in `build/validation/abi-report.txt` and make unsafe unhandled mismatches fail configuration/build. Cross probes must be compile-time or target-run, never implicitly host-executed.
- [X] T011 Define adapter ownership and result interfaces in `src/platform/linux/compat.h` and `src/runner.h`: one global core/session, one dispatch worker, explicit acquired-resource cleanup ownership, synchronized worker outcome and 64-bit frame accounting; model lifecycle as `validated -> initialized -> loading/running -> stopping -> stopped`, with failure through `stopping` to `failed`, as specified in `specs/001-headless-core-runner/data-model.md`.

**Checkpoint**: Toolchain, source/dependency staging and ABI checks are wired. Probes may expose work for US1; unsafe mismatches must never be treated as passing. Complete their fixes before executing a ROM.

## Phase 3: User Story 1 — Run the Core Headlessly (Priority: P1)

**Goal**: Run a real ROM through the core with no audiovisual services and exact bounded completion.

**Independent validation**: Build the working artifact through the foundation; on real ARM64 Linux, run a known-good legally usable ROM for 1 and 120 frames, then run unlimited and interrupt. Require genuine scheduler-boundary counts, no devices/windows/sound, correct status and cleanup. Build-host output alone cannot satisfy this checkpoint.

- [X] T012 [US1] Implement `src/platform/linux/platform.c` with POSIX time/thread services and bounded variadic-safe stderr translation for `__android_log_print`; add `patches/seedlessds-core/0001-platform-registration.patch` so `nds_create` honors its supplied platform via the existing registration seam and defines null/default and teardown lifetime behavior; update patch metadata in `patches/seedlessds-core/manifest.json`.
- [X] T013 [P] [US1] Implement `src/platform/linux/files.c` using the existing POSIX descriptor bridge contract with correct ownership, real open/read/write/stat failures, parent creation and truncation detection; preserve absolute ROM paths and selected `SEEDLESS_ROOT`, and replace JNI file source dependencies without requiring a JVM.
- [X] T014 [P] [US1] Implement `src/platform/linux/memory.c` with real memfd/ftruncate backing and mapping/protection/unmapping/error cleanup; prepare `patches/seedlessds-core/0002-linux-shared-memory.patch` replacing Android ashmem creation only for the Linux integration, preserving aliases, page-size handling and JIT instruction-cache synchronization.
- [X] T015 [P] [US1] Implement `src/platform/linux/headless_audio.c` providing required platform audio state and retained audio/mic symbols, discard host output and neutralize microphone input while preserving emulated SPU/state semantics; exclude OpenSL vtable implementations and retain any required control/capture bookkeeping without audio device access.
- [X] T016 [P] [US1] Implement `src/platform/linux/headless_video.c` retaining required software framebuffers, locks and signal behavior while discarding presentation; trace the glViewport-derived boot word producer/consumer and implement a consistent headless seam, exclude or explicitly reject unused FX/presentation operations, and prevent waits for nonexistent display consumers.
- [X] T017 [US1] Resolve T010's confirmed glibc mismatches with narrowly scoped patches under `patches/seedlessds-core/` and translations in `src/platform/linux/`: use correctly sized/aligned jump state in a still-live dispatch frame, explicit legacy stat conversion or audited typed consumers, and safe pthread storage or adapter-owned indirection; audit direct native calls and fixed offsets, update `patches/seedlessds-core/manifest.json` with T014/T017 patches and run the ABI checks again.
- [X] T018 [US1] Integrate adapter source selections in `cmake/SeedlessCore.cmake`, preserving rasterizer/JIT/scheduler functionality; compile/link the core and audit required/undefined symbols, fortify aliases, boot invariants and transitive Android/GL/OpenSL paths, recording findings in `build/validation/core-link.txt` and fixing genuine missing platform semantics rather than suppressing diagnostics.
- [X] T019 [US1] Implement parsing and RunRequest validation in `src/main.c`: `rom_path` is a “required canonical readable regular file”; `frame_limit` is an “optional unsigned 64-bit decimal integer, range 1 through UINT64_MAX; omitted means unlimited”; `data_dir` is a “writable absolute directory, default current working directory”; reject duplicate/unknown options, missing values, extra operands, overflow and truncation, support `--`, and resolve ROM paths before data-root translation.
- [X] T020 [US1] Implement `src/runner.c` to initialize the core/platform, neutral input, disabled unintended autosave/load/capture defaults and control tracking; prearm `nds_control_run_to(N)` before worker start, run blocking `nds_load_rom` on the worker, observe `frames == N && paused`, request quit, join, capture final status and end tracking. Use real scheduler control counts, never a synthetic `nds_run_frame` loop, and distinguish load return from successful completion.
- [X] T021 [US1] Complete shutdown/error handling in `src/runner.c` and signal wiring in `src/main.c`: handlers only set `sig_atomic_t`, main handles quit/wakeup/join including loading and target pause, avoid duplicate destruction after core unwind, propagate worker and adapter failures, and emit exactly one final result for started runs with CLI exit codes 0/1/2/130/143 from `specs/001-headless-core-runner/contracts/cli.md`.
- [ ] T022 [US1] Implement `scripts/validate-video.sh` for contract-required help/input/path/failure cases, 1/120-frame genuine execution checks, three unlimited interruption runs, SIGTERM, startup/target-pause shutdown and no audiovisual access; use finite external startup/run watchdogs that report failure, retain logs/status/frame counts under `build/validation/us1/`, and require normal workload shutdown within five seconds.
- [ ] T023 [US1] Run `scripts/validate-video.sh` on real ARM64 Linux with an identified legally usable ROM and required user-supplied assets; record device/kernel/libc/page size, ROM SHA-256, artifact/core revisions, actual boundaries and no-device evidence in `build/validation/us1/acceptance.md`; diagnose/fix failures and leave hardware-dependent checks incomplete if hardware/assets are unavailable.

**Checkpoint**: US1 delivers the headless execution MVP after its real-device validation. The native/cross release matrix and clean Ubuntu onboarding remain below.

## Phase 4: User Story 2 — Build the Complete Application (Priority: P1)

**Goal**: Reproduce native/cross builds, embed application libraries and demonstrate deployable artifacts with a declared OS runtime baseline.

**Independent validation**: From clean build directories, build both presets and run each artifact three times for 120 frames on real ARM64 hardware with no separately installed application libraries beyond explicit exceptions. Verify final ELF and dependency inventory, source checkout cleanliness and failure diagnostics.

- [X] T024 [US2] Finalize configure/build/native-test presets in `CMakePresets.json` and integration in `CMakeLists.txt` for documented one-entry-point builds, clear missing-tool/source/sysroot errors and isolated caches; cross configurations must not register target executables as implicitly runnable host tests.
- [X] T025 [P] [US2] Implement `scripts/audit-runtime.py` generating `build/<preset>/runtime-inventory.json` with ELF architecture/interpreter, direct/transitive shared dependencies, symbol versions, explicit retained runtime-loaded libraries, statically embedded library versions and exception reasons; enforce a documented OS-runtime allowlist and fail on undeclared shared core/Lua/archive/LZMA/zlib dependencies, Android/OpenSL/GLES dependencies or unresolved required symbols.
- [X] T026 [US2] Wire `scripts/audit-runtime.py` into `CMakeLists.txt` artifact verification and installation; explicitly link application static targets and compatible compiler support, require opt-in configuration plus purpose/compatibility reason/runtime baseline for any shared application-library exception, and verify no silent fallback or unintended C++ runtime dependency.
- [ ] T027 [US2] Exercise missing archives, checksum mismatch, incompatible architecture/sysroot, source/patch drift and interrupted preparation using disposable inputs for `scripts/prepare-core.sh` and `cmake/Dependencies.cmake`; record actionable failures, verified cache reuse and original core checkout cleanliness in `build/validation/us2/preparation.md` without destructive changes to the real checkout.
- [ ] T028 [US2] Run `cmake --preset linux-arm64-cross` and `cmake --build --preset linux-arm64-cross` from a clean build directory on this Ubuntu host, run the ELF audit, and record tool/compiler versions, command results, manifest/dependency hashes and runtime baseline in `build/validation/us2/cross-build.md`; use a device-compatible sysroot where the target libc requires it.
- [ ] T029 [US2] Run the clean `linux-arm64-native` configure/build and `ctest --preset linux-arm64-native` on ARM64 Ubuntu, audit the artifact and record results in `build/validation/us2/native-build.md`; verify the documented build/dependency setup with distro CMake 3.28 and 4.x across the supported Ubuntu baselines, patching dependency policy compatibility only in staged sources when necessary.
- [ ] T030 [US2] Run both audited artifacts on real ARM64 hardware without separately installed application libraries except declared compatibility exceptions, three 120-frame runs per configuration; record exact counts, statuses, runtime inventory, target baseline and absence of added application libraries in `build/validation/us2/deployment.md`, and verify the one-frame boundary check and interruption behavior remain correct.
- [X] T031 [US2] Add source/dependency attribution and static-distribution materials through `CMakeLists.txt` install rules and `docs/licensing.md`, including source availability/redistribution instructions, core revision and required notices; ensure the install output includes the runner and runtime inventory without packaging proprietary ROM/BIOS assets or substituting bundled host glibc for a compatible sysroot.

**Checkpoint**: US2 passes only when both complete build workflows and minimal-dependency deployments have evidence. Full static glibc or musl support is not required.

## Phase 5: User Story 3 — Prepare Ubuntu Tooling (Priority: P2)

**Goal**: Prove the already implemented and locally executed installer is repeatable and sufficient on clean supported environments.

**Independent validation**: On clean supported Ubuntu environments, run the selected mode twice and verify all prerequisite checks after each run. Exercise insufficient-privilege/unavailable-package failures without modifying package repositories. This complements T004's mandatory installation on the active host.

- [ ] T032 [US3] Run `scripts/setup-ubuntu.sh --cross` a second time on the working Ubuntu host and then `--cross --check`; record version/triple/probe success and idempotence in `build/validation/us3/local-repeat.md`, preserving working installed prerequisites. Use native mode if the working host is ARM64.
- [ ] T033 [US3] Validate `scripts/setup-ubuntu.sh` installation and check modes on clean Ubuntu 24.04 and 26.04 environments covering amd64 cross and ARM64 native workflows, recording each environment/mode and two successful consecutive setup/check executions in `build/validation/us3/clean-environments.md`; checks must include all packages/tools in the setup contract, not just compiler presence.
- [ ] T034 [P] [US3] Exercise unsupported release/architecture, conflicting options, insufficient privileges, unavailable packages and download failures for `scripts/setup-ubuntu.sh` in disposable or safely isolated environments; confirm no false success, no unexpected apt-source/multiarch edits and useful recovery diagnostics, recording commands and outcomes in `build/validation/us3/failures.md`.
- [X] T035 [US3] Update `README.md` with exact user-run installer commands for native/cross, actual execution/check expectations, prerequisites/privileges, Ubuntu/WSL2 baseline, source/dependency preparation, build/smoke commands and failure recovery; explain that locally running the script installs Ubuntu packages system-wide and that application libraries are statically embedded while documented OS runtime requirements remain.

**Checkpoint**: Installer creation (T002), actual local installation (T004), repetition (T032) and clean-environment acceptance (T033) are separate completion conditions.

## Phase 6: Polish and Cross-Cutting Verification

- [X] T036 [P] Document the safe core-bump procedure in `docs/core-updates.md`: inspect pinned revision/source drift, review adapter inventory, regenerate/check ordered patch metadata, rebuild native/cross, rerun ABI/runtime inventory and hardware acceptance; never edit/reset the source submodule automatically or apply fuzzy patches.
- [X] T037 Reconcile `specs/001-headless-core-runner/quickstart.md` and `README.md` with implemented commands, observed runtime asset names/requirements, sysroot support, static-link exceptions and actual output/status behavior; do not label planned artifacts or unsupported device baselines as validated.
- [ ] T038 Execute the final `specs/001-headless-core-runner/quickstart.md` workflow and consolidate FR-001–FR-016 / SC-001–SC-008 coverage in `build/validation/acceptance.md`, linking installation, clean native/cross builds, ABI, runtime inventory and device-run evidence; list any unavailable hardware/ROM/environment acceptance explicitly as outstanding and keep corresponding tasks unchecked.
- [X] T039 Run final Bash syntax/ShellCheck for `scripts/`, relevant CMake/CTest checks after final changes, `git diff --check`, and verify `external/SeedlessDS-Core/` remains unmodified; record results in `build/validation/final-checks.md` and ensure tracked docs/artifacts/logs contain no credentials or proprietary runtime assets.

## Dependencies and Execution Order

```text
T001 -> T002 -> T003 -> T004 (ACTUAL LOCAL INSTALLATION)
  -> T005 -> T006 -> {T007, T008} -> T009 -> T010 -> T011
  -> US1: T012 -> {T013, T014, T015, T016} -> T017 -> T018
          -> T019 -> T020 -> T021 -> T022 -> T023
  -> US2: T024 -> T025 -> T026 -> T027 -> T028 -> T029 -> T030 -> T031
  -> US3: T032 -> {T033, T034} -> T035
  -> T036 -> T037 -> T038 -> T039
```

This graph gives a safe sequential implementation path, not a requirement to postpone every independent task. US1 depends on the shared build foundation; US2 completes release reproducibility and the native/cross deployment matrix after US1. US3 acceptance can run after T004 independently of US1/US2; prioritize T032 early to confirm the local installation is repeatable. Clean-environment setup validation does not need a ROM. Hardware unavailability must not block unrelated local implementation work, but it prevents claiming hardware acceptance.

T007/T008 may run together after T006. T013–T016 may run together after T012/T011; only T017 updates their common manifest and T018 updates their common build wiring. T025 can be developed alongside T024 after the foundation with integration in T026. T034 can run alongside T033 using separate environments. Parallel work is optional; this checklist does not require spawning agents.

## Parallel Examples by Story

- **US1**: After interfaces and platform registration are fixed, implement `src/platform/linux/files.c` (T013), `memory.c` and its separate patch (T014), `headless_audio.c` (T015), and `headless_video.c` (T016) independently; consolidate manifest/build edits in T017–T018.
- **US2**: After the foundation, finalize presets (T024) while independently developing `scripts/audit-runtime.py` (T025); integrate and validate the audit in T026. Run native/cross acceptance on separate hosts only once each build's prerequisites and ABI checks pass.
- **US3**: After T004/T032, validate clean installs (T033) while separately exercising installer failures (T034); consolidate documented onboarding in T035.

## Requirement Coverage

| Requirements / outcomes | Primary tasks |
| --- | --- |
| FR-001–FR-007; SC-003–SC-005 | T010–T023 |
| FR-008–FR-010; SC-001–SC-002 | T005–T010, T024–T030 |
| FR-011–FR-012; SC-006 | T002–T004, T032–T035, T037 |
| FR-013; real-device evidence | T023, T029–T030, T038 |
| FR-014–FR-016; SC-007–SC-008 | T007, T009, T025–T026, T028–T031 |
| Core update boundary, licenses and final integrity | T005–T006, T017–T018, T031, T036–T039 |

## Implementation Strategy

1. Install tooling first: implement, inspect and **run** the script locally; T004 is mandatory and cannot be replaced by a user-facing suggestion.
2. Establish source staging, dependencies, cross toolchain and ABI gates, then implement US1 for the first genuine headless execution MVP. Use existing core control APIs, preserve emulation behavior and retain exact source/patch traceability.
3. Complete US2's reproducible native/cross builds, static application linkage, documented OS-runtime compatibility and real-device matrix.
4. Finish US3 clean-environment evidence and onboarding, with local repeat setup performed early when practical.
5. Complete final requirement/evidence review. Do not mark unavailable target tests as passed, introduce a full libc migration, or expand into graphical/audio frontend work.
