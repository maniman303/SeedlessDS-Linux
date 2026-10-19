# Tasks: Frontend JSON Configuration

**Input**: Design documents in `specs/003-frontend-json-config/`.
**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), and [contracts/](contracts/).
**Organization**: Four P1 user stories, preceded by shared foundations. Configuration must live in new C/header modules; `runner.c` only coordinates lifecycle.
**Validation**: Specification SC-001–SC-009 and acceptance scenarios require verification of persistence, boundaries, failure behavior, precedence, and actual core application. Tasks below include the focused automated and device checks needed for those outcomes; no blanket TDD requirement is imposed.

## Format: `[ID] [P?] [Story] Description`

All paths are repository-relative. `[P]` identifies work on independent files after the prerequisites stated below; it does not authorize concurrent edits to shared files. Story tasks carry `[US1]` through `[US4]`. Check a task only after its stated behavior is implemented and verified.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Confirm current local inputs and create reusable module boundaries without overwriting unrelated work.
- [X] T001 Inspect current `CMakeLists.txt`, `scripts/prepare-core.sh`, `cmake/Dependencies.cmake`, and `patches/seedlessds-core/manifest.json` against the plan; record prerequisite/cache availability and any concurrent source changes in `specs/003-frontend-json-config/quickstart.md`, preserve existing edits, and fix the missing tempfile import only if still present. Never invoke remote Git/GitHub, create commits, or run sudo; require user completion for privileged prerequisites.
- [X] T002 Declare configuration ownership/error/runtime types and module APIs in `src/config.h`, `src/config_json.h`, `src/launch_options.h`, and `src/core_config.h`; encode “Owned strings have explicit init/free lifetimes”, “Do not retain JSON node pointers after parsing”, and save/frame override presence separately from values, as required by `specs/003-frontend-json-config/data-model.md`.
- [X] T003 Create a core-independent host test project in `tests/config/CMakeLists.txt` plus `tests/config/config_probe.c` for configuration/launch subprocess checks; keep production ARM64 architecture gates intact and introduce no external downloads.

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared strict schema, defaults, file lifecycle, and supported core bridge. Complete T001–T010 before story integration.
- [X] T004 Implement bounded strict JSON decoding/encoding in `src/config_json.c` against `src/config_json.h`: valid UTF-8/escapes/surrogate pairs, exact booleans/integers, escaped output, duplicate-key detection, no trailing data/comments/commas/NUL/nonfinite numbers, 64 KiB maximum input and depth 8; report syntax/type/key errors without integer overflow.
- [X] T005 Implement all four typed groups and schema/default validation in `src/config.c`: “Every object/field is required” and “Validate integer type and bounds before narrowing”; reject unknown/fixed-policy/year/combined-birthday keys. Use visual booleans false/thread_count 0 with integer range 0–15; exact home-based ARM7/ARM9 defaults; empty firmware/save strings; nickname SEEDLESS, birthday_month 3, birthday_day 2, color 0. Enforce nonempty valid nickname ≤10 UTF-16 units, month 1–12/day per month including February 29, integer color 0–15, and required absolute BIOS/nonempty firmware paths without shell expansion. Depends on T004.
- [X] T006 Implement selected-file read/create primitives and structured stage/key/path/errno errors in `src/config.c`: only missing files receive defaults, nonblocking regular-file reads, parent creation, same-directory complete temporary writes with EINTR/short-IO handling, sync and atomic no-replace publication, concurrent-winner reload, and cleanup of owned temporary files only; never truncate or rewrite existing files. Require HOME only for default-file selection/default generation, ignore XDG_CONFIG_HOME, and reject dangling symlinks or existing invalid files.
- [X] T007 [P] Implement `src/launch_options.c` against `src/launch_options.h` for `--config PATH`, `--save-dir DIR`, `--frames N`, `--help`, `--`, and one ROM; preserve explicit empty save override, require nonempty config value and frame integer 1–UINT64_MAX, reject duplicates/missing values/unknown and BIOS arguments/legacy `--data-dir`, and keep parsing/help free of filesystem writes.
- [ ] T008 Create `patches/seedlessds-core/0006-frontend-config-paths.patch` with a versioned supported `seedlessds/linux_config.h` bridge for copied exact BIOS/optional firmware/direct cartridge-save/private-data paths, setup/status/reset APIs, bounded path copies and no reconfiguration while running; preserve old callers and nds_t/existing ABI layouts. Rebase the patch filename/order locally if another numbered patch already exists; do not edit the pinned checkout or create commits.
- [ ] T009 Register the bridge patch and SHA-256 in `patches/seedlessds-core/manifest.json`, update `cmake/SeedlessCore.cmake` if new staged sources require registration, and verify local patch application plus `tests/abi/core_layout.c` layout assertions and new bridge-version/size checks. Depends on T008; use only local staged source preparation.
- [ ] T010 Add strict codec/schema/CLI boundary cases to `tests/test_config.c` and `tests/test_launch_options.c` and register them in `tests/config/CMakeLists.txt`; include Unicode unit counts, required/unknown/duplicate keys, integer overflow/types, 64 KiB/depth limits, all profile limits, path length limits, absent-versus-empty save override, frame count 0/overflow and removed arguments. Verify the host suite after T004–T007 and bridge ABI checks after T009.

**Checkpoint**: Schema, default generation, CLI, and supported core-path interface exist and can be exercised without embedding policy in runner.

## Phase 3: User Story 1 — Launch with persistent BIOS settings (Priority: P1) — MVP

**Goal**: Launch repeatedly from a valid default configuration using the exact ARM7/ARM9 files without rewriting user edits.
**Independent test**: Provide a complete default JSON, valid distinct BIOS images and a ROM; run twice, verify selected images reach the corresponding core processor and the config hash stays unchanged.
- [X] T011 [US1] Implement default configuration selection and effective asset resolution in `src/config.c` with owned immutable runtime paths; use `$HOME/.config/seedlessds/config.json`, validate ARM7 16384-byte and ARM9 4096-byte readable regular files, preserve user paths exactly, and reject generated path/suffix lengths exceeding the core 1024-byte buffer including terminator. Do not search replacement filenames or copy BIOS images.
- [X] T012 [US1] Implement exact BIOS loading in `patches/seedlessds-core/0006-frontend-config-paths.patch` through the new supported bridge before core asset reads; remove fallback only when explicit Linux overrides are active, retain old API behavior otherwise, and refresh the checksum in `patches/seedlessds-core/manifest.json`.
- [X] T013 [US1] Implement `src/core_config.c` startup translation and ownership cleanup through `src/core_config.h`; install path overrides before `nds_create`, retain JIT initialization, set mandatory RTC bit 39/raw-save bit 50 for every ROM open, retain unrelated existing bits and non-fixed control clock, and report unsupported policy/setup failures without private core layout access.
- [ ] T014 [US1] Integrate module calls into `src/runner.c` and `src/runner.h`: argv/ROM validation → selected JSON → runtime resolution → bridge/core initialization → worker run → flush/cleanup. Remove superseded inline argv/default BIOS helpers, leave JSON/schema/packed-bit logic outside runner, and use single cleanup paths for allocated config/runtime/bridge state.
- [X] T015 [US1] Wire new `.c` modules into `CMakeLists.txt` with existing C11/warning/link policy; keep native and cross compilation and runtime inventory integration working without new shared JSON dependencies.
- [ ] T016 [US1] Add default-config persistence and exact asset selection cases in `tests/test_config_cli.py` and register native integration checks in `CMakeLists.txt`; verify distinct paths are not swapped, unavailable/invalid BIOS exits before emulation, no legacy fallback occurs, config hashes remain unchanged across two launches, and repeated sessions do not retain prior bridge paths.

**Checkpoint**: US1 can be demonstrated with an existing complete default file; automatic missing-file recovery is exercised in US3. This is the recommended MVP after shared foundations.

## Phase 4: User Story 2 — Select an alternative configuration (Priority: P1)

**Goal**: A supplied custom file is the only source and creation destination, regardless of default-file contents or failure.
**Independent test**: Use conflicting default/custom configs; verify only custom assets apply, a missing custom file is created there, and invalid custom content fails without fallback or rewriting either existing file.
- [X] T017 [US2] Connect `--config` selection to selected-file load-or-create in `src/config.c`, preserving relative-to-launch-CWD and space-containing paths; never open or write the default path when an override is present, and allow a complete custom config without HOME while failing clearly if generating defaults requires unavailable HOME.
- [X] T018 [US2] Propagate custom config path and dotted-key/filesystem diagnostics through `src/runner.c` without fallback, preserving byte-identical existing files for schema/access failures and failed startup; use exit 1/result operation=config for configuration errors.
- [ ] T019 [US2] Add custom-file authority cases in `tests/test_config_cli.py`: conflicting or malformed default, missing custom parents/file, relative and spaced paths, existing unreadable/directory/dangling-link custom target, unavailable HOME, schema failure and failed custom creation; assert no default reads/writes and unchanged existing config hashes.

**Checkpoint**: US2 is independently testable using conflicting files and observable file-access tracing or equivalent injected filesystem hooks.

## Phase 5: User Story 3 — Recover missing configuration and understand launch errors (Priority: P1)

**Goal**: First launch generates all correct defaults; bad invocation/configuration produces useful diagnostics and clean exits while preserving bounded run behavior.
**Independent test**: Isolated HOME plus a ROM creates a valid default JSON and missing-file log; unwritable destinations exit 1, no ROM exits 2 without config creation, help exits 0 without config access, and valid bounded runs stop at N boundaries.
- [X] T020 [US3] Finish missing-file logging and generated-default startup behavior in `src/config.c`; preserve all four groups, resolved home-based BIOS paths, empty optional paths, numeric separate birthday 3/2 and other defaults; a config must remain fully valid after publication even if subsequent BIOS checks fail.
- [X] T021 [US3] Implement invocation/help/config failure ordering and status/result records in `src/runner.c` using `src/launch_options.c`: standalone help before config IO, missing/unreadable ROM or invalid args exit 2 before creation, config/asset failures exit 1 before core, supported-input help and selected-path diagnostics; retain `--frames` exact control behavior and SIGINT/SIGTERM shutdown semantics.
- [ ] T022 [US3] Add first-launch, required-default, help/no-ROM/invalid-argument and frame/signal regression cases in `tests/test_config_cli.py`; assert no config IO on help, no config creation for invalid invocation, missing-file diagnostic paths, exact bounded frames and zero emulation for preflight failures.
- [ ] T023 [P] [US3] Add deterministic fault-injection and unprivileged filesystem cases in `tests/test_config.c` for config short writes/EINTR/write-close-sync-publish errors/ENOSPC, competing creators, malformed or partially written existing content, size/depth limits and preservation of existing files; verify only process-owned temporary files are removed.

**Checkpoint**: Existing and missing default/custom configurations have consistent safe startup behavior; US3 provides the launch/error contract used by all stories.

## Phase 6: User Story 4 — Customize visuals, firmware, profile, and saves (Priority: P1)

**Goal**: Apply grouped settings, validate profile data, and use direct raw saves with CLI precedence while JIT/RTC/format stay fixed.
**Independent test**: Edit visuals/profile/firmware, observe actual core state, write/reload a cartridge save, and repeat with a conflicting and then empty save-directory override; verify no config rewrite or lower-priority save writes.
- [ ] T024 [US4] Add core-application test cases in `tests/test_core_config.c` for visual mappings, profile packing, fixed JIT/RTC/raw policies, override-presence semantics and session reset; include observed runtime state checks so matching serialized values alone cannot pass, and register core-free stubs/native bridge checks in `tests/config/CMakeLists.txt` and `CMakeLists.txt`.
- [X] T025 [US4] Implement visuals/profile application in `src/core_config.c`: threaded bit28, count bits16–19, auto_trim bit36, fix_main_screen bit35, disable_edge_marking bit40; pass losslessly validated ≤10 UTF-16 nickname units with English=1, color<<8, (birthday_month-1)<<16, birthday_day<<24 before firmware startup. Preserve 0–15 requested worker counts while allowing documented core worker availability/scheduling limits.
- [ ] T026 [US4] Implement optional firmware handling and profile precedence in `patches/seedlessds-core/0006-frontend-config-paths.patch`: empty path bypasses all legacy/modified-image lookup, nonempty exact path receives core structural validation with propagated errors, and explicit user fields win over image data; keep modified firmware/cache/user files rooted separately under selected-config-parent compatibility data, not cartridge saves; refresh `patches/seedlessds-core/manifest.json`.
- [X] T027 [US4] Resolve effective save paths in `src/config.c`: “Resolve save choice by presence first (CLI if supplied, otherwise JSON), then convert empty to ROM parent”; retain persisted string separately, canonicalize relative paths from launch CWD, support spaces, create only effective missing directories, require usable destinations and derive direct ROM-basename `.sav` without `backup/` or `.dsv` fallback. Do not probe overridden config directories or mutate JSON.
- [X] T028 [US4] Add exclusive advisory session locking and cleanup for effective save destinations in `src/core_config.c` and `src/core_config.h`, handling absent save files without premature truncation; “Concurrent sessions writing the same save are unsupported”; reject contending writers and retain locks through final flush and error collection.
- [ ] T029 [US4] Implement exact raw save load/write and thread-safe error latching through `patches/seedlessds-core/0006-frontend-config-paths.patch`, adapting existing cartridge/SPI save routines rather than duplicating them: missing save normal, unreadable/corrupt existing save preserved and rejected, failures on reads/writes/flush/close latched through teardown, independent private-data roots retained; refresh `patches/seedlessds-core/manifest.json` and ABI assertions in `tests/abi/core_layout.c`.
- [X] T030 [US4] Integrate firmware/save preflight and final status collection in `src/runner.c` via `src/core_config.c`: join worker, complete core save flush, inspect latched error before completed/interrupted result, make save error exit 1 take precedence over signal status, release locks/resources only after status is captured, and retain operation=firmware/save diagnostics without adding settings logic to runner.
- [ ] T031 [US4] Add profile/visual/firmware integration matrix in `tests/test_config_cli.py`: all defaults, 10/11 UTF-16 and non-BMP boundaries, month/day integer/calendar cases, colors -1/0/15/16, boolean render fields/count 0/1/15 versus invalid types/ranges, forbidden policies/year/combined birthday, empty/nonempty/invalid firmware and no stale-image fallback. Observe effective core settings and configured profile winning over firmware.
- [ ] T032 [US4] Add raw-save path/precedence/reload/failure cases in `tests/test_config_cli.py`: configured versus CLI versus empty CLI destination, relative/spaced paths, absent/unusable directories, existing .dsv ignored, real game-save reload, corrupt/unreadable save preserved, concurrent writer rejection, mid-run/final-flush faults, no lower-priority writes, byte-identical JSON and no backup subdirectory; provide deterministic core fault hooks where necessary without exposing production launch options.

**Checkpoint**: All four stories satisfy the five-input CLI, four-group config, exact asset paths, fixed execution policies and portable raw-save contract.

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Update existing callers, run required build/device checks and record actual results.
- [X] T033 Update `scripts/debug-device.py` frontend invocation construction and associated `tests/test_debug_device_diagnostics.py` plus other discovered caller tests for `--config`/`--save-dir`, retain `--frames` and independent device transport/inventory options, remove frontend `--data-dir`, and preserve result parsing including config/firmware/save failures without leaking credentials.
- [X] T034 Update `scripts/validate-headless.sh` for isolated complete JSON fixtures and explicit config/save launch options while retaining bounded-run, invalid-ROM/BIOS and shutdown checks; inspect all called workflows for prohibited actions before execution.
- [X] T035 [P] Update `README.md`, `docs/core-options-inventory.md`, and `specs/003-frontend-json-config/quickstart.md` with actual supported JSON/defaults, separate birthday fields, fixed policies, config authority, save override precedence and validation commands; preserve unrelated edits and make commands directly runnable in an existing Linux terminal.
- [ ] T036 Run the host/native tests via `tests/config/CMakeLists.txt` and production native/cross builds via `CMakePresets.json` using existing local caches only; run `tests/abi/core_layout.c` and `scripts/audit-runtime.py` checks, resolve regressions and record commands/results or exact missing prerequisites in `specs/003-frontend-json-config/quickstart.md`. Do not claim ARM64 execution from a cross-build alone.
- [ ] T037 Validate `specs/003-frontend-json-config/quickstart.md` on real ARM64 Linux hardware using inspected `scripts/debug-device.py`: confirm actual BIOS/firmware/profile/visual/JIT/RTC behavior, exact frame limits, cartridge write/reload and failure handling, and record model/OS/runtime inventory/evidence without credentials. If hardware/assets are unavailable, keep this task incomplete and report the specific dependency.
- [ ] T038 Perform final FR-001–FR-019 and SC-001–SC-009 coverage/source-separation review across `src/config.c`, `src/config_json.c`, `src/launch_options.c`, `src/core_config.c`, `src/runner.c`, the compatibility patch and all tests; update `specs/003-frontend-json-config/quickstart.md` with evidence/limitations and check local diffs for unintended changes. Leave commits and every remote repository action to the user.

## Dependencies & Execution Order

### Phase and task dependencies

```text
Setup T001 → T002 → T003
Foundations: T004 → T005 → T006
             T007 can run alongside T004–T006 after setup
             T008 → T009 (separate core patch files)
             T010 verifies foundations after their implementations
All foundations → US1 T011–T016 → US2 T017–T019 → US3 T020–T023
All preceding integration → US4 T024–T032 → Polish T033–T038
```

All stories are P1. The conservative delivery order reflects shared config/runner files, not different product priorities. US1 is runnable with an existing complete file after foundations. US2 adds file selection without requiring visual/save features to be applied. US3 exercises the already-shared file lifecycle and CLI with isolated fixtures. US4 adds runtime application/save semantics; US1 already enforces fixed policies so the MVP does not launch under unintended modes. Within a phase, tasks run in listed order unless marked `[P]` and prerequisites are satisfied. Shared patch edits and checksum updates always serialize.

### Requirements coverage

| Requirements | Tasks |
|---|---|
| FR-001, FR-003, FR-006, FR-011 | T005, T011–T016 |
| FR-002, FR-010, FR-012 | T006, T017–T019 |
| FR-004, FR-005 | T006, T020, T022–T023 |
| FR-007–FR-009 | T007, T010, T021–T022 |
| FR-013–FR-016 | T005, T013, T024–T026, T031 |
| FR-017–FR-019 | T027–T030, T032 |
| SC-001–SC-006 | T016, T019, T022–T023, T036–T037 |
| SC-007–SC-009 | T010, T024, T031–T032, T036–T037 |

## Parallel Examples

- **Foundations**: after T003, T007 (`launch_options.c`) can run alongside T004–T006 (`config_json.c`/`config.c`) and T008 (`0006` patch); integration follows sequentially.
- **US1**: T011 and T012 touch separate files and can be split after foundations if API ownership is fixed; listed sequentially conservatively. T013–T016 wait for the production paths they exercise.
- **US2**: author T019 custom-file cases in a separate working copy while T017–T018 are implemented; do not concurrently edit the same `tests/test_config_cli.py` as another story. No default claim of independent story integration.
- **US3**: T023 C fault tests can run alongside T022 Python launch cases after T020–T021; both exercise completed file-lifecycle primitives and edit different files.
- **US4**: after T024 establishes the harness, visuals/profile translation (T025), firmware patch work (T026) and save-path resolution (T027) can be split across distinct files, then serialize T028–T030 integration. T031/T032 share a test file and must be serialized.
- **Polish**: T035 documentation can run alongside T033–T034 caller updates, after all story behavior is settled.

These are scheduling opportunities, not instructions to spawn agents. Preserve shared-file ownership and serialize dependency-sensitive work.

## Implementation Strategy

1. Complete setup and foundations, then demonstrate US1 from a complete existing JSON as the smallest MVP. Verify actual exact BIOS loading and unchanged configuration across repeat launches.
2. Add US2 and US3; demonstrate authoritative custom selection, correct first-run generation, no-ROM/help behavior, preserved `--frames`, and deterministic failures.
3. Add US4 runtime application and direct save semantics. Validate both successful reloads and failure reporting before calling the feature complete.
4. Finish caller/docs integration, native/cross/ABI checks and real-device acceptance. Optional external resources do not justify checking unverified tasks complete.

## Notes

- No commits, remote Git operations or GitHub actions are permitted. The template's commit instruction is intentionally omitted under Constitution VI.
- Never execute sudo; supply the exact necessary command to the user and resume dependent work only after explicit confirmation of success.
- Do not modify the pinned core checkout: implement core adaptation through the tracked ordered patch and manifest checksum.
- Preserve existing non-feature working-tree changes, including parallel fullscreen-video work. Coordinate build/source edits locally rather than overwrite them.
- Tests verify required observable behavior and failure guarantees; a bounded run alone does not prove a cartridge save was written.
